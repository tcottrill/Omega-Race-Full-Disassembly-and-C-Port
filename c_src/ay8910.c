/* -----------------------------------------------------------------------------
 * AY-3-8910 / YM2149 PSG emulator for AAE (Another Arcade Emulator)
 *
 * Attribution / Licensing:
 *   The synthesis core in this file -- the volume (DAC) table construction, the
 *   per-register write side-effects (apply_register_side_effects), and the
 *   sample generation loop (ay8910_render): tone/noise/envelope counters, the
 *   17-bit Galois LFSR, the per-sample area integration of the (tone | tone_
 *   disable) & (noise | noise_disable) channel mix, and the envelope-shape
 *   progression -- is a port of the AY-3-8910 emulator from the M.A.M.E.(TM)
 *   project (sound/ay8910.c).
 *
 *   That MAME code is copyright the M.A.M.E. Team and was based on various code
 *   snippets by Ville Hallik, Michael Cuddy, Tatsuyuki Satoh, Fabrice Frances,
 *   and Nicola Salmoria. Because this file is a derivative of that work, it is
 *   distributed under the terms of the GNU General Public License (version 2 or,
 *   at your option, any later version), the license under which the original
 *   MAME source was made available. Redistribution must preserve this notice
 *   and the original MAME copyright acknowledgement.
 *
 *   This program is distributed in the hope that it will be useful, but WITHOUT
 *   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *   FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 *   more details: <https://www.gnu.org/licenses/>.
 *
 *   The surrounding AAE bank/mixer integration -- AY8910Config/AY8910Bank, the
 *   ay8910_sh_* lifecycle, the mid-frame catch-up scheduler, the MEM/PORT
 *   trampolines, and the port A/B read/write callbacks -- was removed for the
 *   Omega Race C port. This file is the plain-C conversion of just the chip
 *   core (configure/reset/register side effects/render and the DC blocker)
 *   used directly by the Omega Race platform code.
 * -----------------------------------------------------------------------------*/

#include "ay8910.h"

#include <string.h>     /* memset */

#define AY8910_ENABLE_DC_BLOCK 1
#define AY8910_DC_COEF         4076   /* ~0.995 * 4096 */
#define AY8910_DC_SHIFT        12     /* matches 4096 = 1<<12 */
#define AY8910_STEP            0x8000  /* 32768 */

static void apply_register_side_effects(ay8910* c, uint8_t addr, uint8_t data)
{
    (void)data;
    int32_t old_period;
    switch (addr) {
    case 0: case 1:
        c->regs[1] &= 0x0f;
        old_period = c->PeriodA;
        c->PeriodA = (c->regs[0] + 256 * c->regs[1]) * (int32_t)c->UpdateStep;
        if (c->PeriodA == 0) c->PeriodA = (int32_t)c->UpdateStep;
        c->CountA += c->PeriodA - old_period;
        if (c->CountA <= 0) c->CountA = 1;
        break;

    case 2: case 3:
        c->regs[3] &= 0x0f;
        old_period = c->PeriodB;
        c->PeriodB = (c->regs[2] + 256 * c->regs[3]) * (int32_t)c->UpdateStep;
        if (c->PeriodB == 0) c->PeriodB = (int32_t)c->UpdateStep;
        c->CountB += c->PeriodB - old_period;
        if (c->CountB <= 0) c->CountB = 1;
        break;

    case 4: case 5:
        c->regs[5] &= 0x0f;
        old_period = c->PeriodC;
        c->PeriodC = (c->regs[4] + 256 * c->regs[5]) * (int32_t)c->UpdateStep;
        if (c->PeriodC == 0) c->PeriodC = (int32_t)c->UpdateStep;
        c->CountC += c->PeriodC - old_period;
        if (c->CountC <= 0) c->CountC = 1;
        break;

    case 6:
        c->regs[6] &= 0x1f;
        old_period = c->PeriodN;
        c->PeriodN = c->regs[6] * (int32_t)c->UpdateStep;
        if (c->PeriodN == 0) c->PeriodN = (int32_t)c->UpdateStep;
        c->CountN += c->PeriodN - old_period;
        if (c->CountN <= 0) c->CountN = 1;
        break;

    case 7:
        /* Mixer/enable register: regs[7] is stored generically by
         * ay8910_write_reg. The I/O ports are unused on this board (Omega
         * Race's chips are AY-3-8912s with nothing wired to the ports), so
         * there is no port-direction side effect left to apply here. */
        break;

    case 8:
        c->regs[8] &= 0x1f;
        c->EnvelopeA = c->regs[8] & 0x10;
        c->VolA = c->EnvelopeA ? c->VolE
                               : c->VolTable[c->regs[8] ? c->regs[8] * 2 + 1 : 0];
        break;

    case 9:
        c->regs[9] &= 0x1f;
        c->EnvelopeB = c->regs[9] & 0x10;
        c->VolB = c->EnvelopeB ? c->VolE
                               : c->VolTable[c->regs[9] ? c->regs[9] * 2 + 1 : 0];
        break;

    case 10:
        c->regs[10] &= 0x1f;
        c->EnvelopeC = c->regs[10] & 0x10;
        c->VolC = c->EnvelopeC ? c->VolE
                               : c->VolTable[c->regs[10] ? c->regs[10] * 2 + 1 : 0];
        break;

    case 11: case 12:
        old_period = c->PeriodE;
        c->PeriodE = (c->regs[11] + 256 * c->regs[12]) * (int32_t)c->UpdateStep;
        if (c->PeriodE == 0) c->PeriodE = (int32_t)c->UpdateStep / 2;
        c->CountE += c->PeriodE - old_period;
        if (c->CountE <= 0) c->CountE = 1;
        break;

    case 13:
        c->regs[13] &= 0x0f;
        c->Attack = (c->regs[13] & 0x04) ? 0x1f : 0x00;
        if ((c->regs[13] & 0x08) == 0) {
            /* Continue = 0: collapse to a one-shot shape (hold at end). */
            c->Hold = 1;
            c->Alternate = c->Attack;
        } else {
            c->Hold = c->regs[13] & 0x01;
            c->Alternate = c->regs[13] & 0x02;
        }
        c->CountE = c->PeriodE;
        c->CountEnv = 0x1f;
        c->Holding = 0;
        c->VolE = c->VolTable[c->CountEnv ^ c->Attack];
        if (c->EnvelopeA) c->VolA = c->VolE;
        if (c->EnvelopeB) c->VolB = c->VolE;
        if (c->EnvelopeC) c->VolC = c->VolE;
        break;

    case 14:
        /* Port A data register: I/O ports are unused on this board, so there
         * is nothing to push out; the value is stored generically. */
        break;

    case 15:
        /* Port B data register: same as case 14 -- unused on this board. */
        break;

    default:
        break;
    }
}

void ay8910_init(ay8910* c, int master_clock_hz, int sample_rate)
{
    memset(c, 0, sizeof(*c));

    if (sample_rate <= 0) sample_rate = 1;
    if (master_clock_hz <= 0) master_clock_hz = 1;

    /* UpdateStep scales period_reg so that per output sample, CountX advances
     * by AY8910_STEP. PeriodX = period_reg * UpdateStep. Toggle period in
     * samples = period_reg * UpdateStep / AY8910_STEP = period_reg * sys_freq *
     * 8 / master_clock. */
    c->UpdateStep = (uint32_t)(
        ((double)AY8910_STEP * (double)sample_rate * 8.0) / (double)master_clock_hz + 0.5);
    if (c->UpdateStep == 0) c->UpdateStep = 1;

    /* Build the volume table: 1.5 dB per envelope step, log scale.
     * Tone-level v reads VolTable[v*2+1] (odd index); envelope reads any index.
     *
     * DELIBERATE DEVIATION from the AAE/MAME source: the original starts
     * out_v at 32767.0, so VolTable[31] == MAX_OUTPUT == 32767. Here out_v
     * starts at 10922.0 (32767/3), so VolTable[31] == 10922, meaning three
     * channels all at full volume sum to <= 32766 and cannot clip inside a
     * single chip's own mix. The final int16 clamp below is kept regardless. */
    {
        double out_v = 10922.0;
        int i;
        for (i = 31; i > 0; --i) {
            c->VolTable[i] = (uint32_t)(out_v + 0.5);
            out_v /= 1.188502227;  /* 10^(1.5/20) */
        }
        c->VolTable[0] = 0;
    }

    ay8910_reset(c);
}

void ay8910_reset(ay8910* c)
{
    int i;

    for (i = 0; i < 16; ++i) c->regs[i] = 0;

    c->PeriodA = c->PeriodB = c->PeriodC = c->PeriodN = c->PeriodE = 0;
    c->CountA  = c->CountB  = c->CountC  = c->CountN  = c->CountE  = 0;
    c->VolA = c->VolB = c->VolC = c->VolE = 0;
    c->EnvelopeA = c->EnvelopeB = c->EnvelopeC = 0;
    c->OutputA = c->OutputB = c->OutputC = 0;
    c->OutputN = 0xff;
    c->CountEnv = 0;
    c->Hold = c->Alternate = c->Attack = c->Holding = 0;
    c->RNG = 1;

    c->dc_prev_in = 0;
    c->dc_prev_out = 0;

    /* Drive R0..R13 through the side-effect handler to initialize all derived
     * state (periods, vol levels, envelope). Matches MAME's reset. */
    for (i = 0; i < 14; ++i) {
        apply_register_side_effects(c, (uint8_t)i, 0);
    }
}

void ay8910_write_reg(ay8910* c, uint8_t reg, uint8_t data)
{
    const uint8_t a = reg & 0x0F;
    c->regs[a] = data;
    apply_register_side_effects(c, a, data);
}

uint8_t ay8910_read_reg(const ay8910* c, uint8_t reg)
{
    /* Registers 14/15 (I/O port A/B data) have no port callback on this
     * board, so they just return the stored value like every other
     * register. */
    const uint8_t a = reg & 0x0F;
    return c->regs[a];
}

void ay8910_render(ay8910* c, int16_t* dst, int n)
{
    if (!dst || n <= 0) return;

    const int32_t length_step = n * AY8910_STEP;

    /* Fast-forward disabled / zero-volume channels so their counters don't
     * drift past the sample window. Matches MAME's pre-loop guard. */
    if (c->regs[7] & 0x01) {
        if (c->CountA <= length_step) c->CountA += length_step;
        c->OutputA = 1;
    } else if ((c->regs[8] & 0x1f) == 0) {
        if (c->CountA <= length_step) c->CountA += length_step;
    }
    if (c->regs[7] & 0x02) {
        if (c->CountB <= length_step) c->CountB += length_step;
        c->OutputB = 1;
    } else if ((c->regs[9] & 0x1f) == 0) {
        if (c->CountB <= length_step) c->CountB += length_step;
    }
    if (c->regs[7] & 0x04) {
        if (c->CountC <= length_step) c->CountC += length_step;
        c->OutputC = 1;
    } else if ((c->regs[10] & 0x1f) == 0) {
        if (c->CountC <= length_step) c->CountC += length_step;
    }
    if ((c->regs[7] & 0x38) == 0x38) {
        if (c->CountN <= length_step) c->CountN += length_step;
    }

    int32_t outn = (c->OutputN | c->regs[7]);

    int remaining = n;
    while (remaining--) {
        int32_t vola = 0, volb = 0, volc = 0;
        int32_t left = AY8910_STEP;

        do {
            const int32_t nextevent = (c->CountN < left) ? c->CountN : left;

            /* ---- Channel A ---- */
            if (outn & 0x08) {
                if (c->OutputA) vola += c->CountA;
                c->CountA -= nextevent;
                while (c->CountA <= 0) {
                    c->CountA += c->PeriodA;
                    if (c->CountA > 0) {
                        c->OutputA ^= 1;
                        if (c->OutputA) vola += c->PeriodA;
                        break;
                    }
                    c->CountA += c->PeriodA;
                    vola += c->PeriodA;
                }
                if (c->OutputA) vola -= c->CountA;
            } else {
                c->CountA -= nextevent;
                while (c->CountA <= 0) {
                    c->CountA += c->PeriodA;
                    if (c->CountA > 0) { c->OutputA ^= 1; break; }
                    c->CountA += c->PeriodA;
                }
            }

            /* ---- Channel B ---- */
            if (outn & 0x10) {
                if (c->OutputB) volb += c->CountB;
                c->CountB -= nextevent;
                while (c->CountB <= 0) {
                    c->CountB += c->PeriodB;
                    if (c->CountB > 0) {
                        c->OutputB ^= 1;
                        if (c->OutputB) volb += c->PeriodB;
                        break;
                    }
                    c->CountB += c->PeriodB;
                    volb += c->PeriodB;
                }
                if (c->OutputB) volb -= c->CountB;
            } else {
                c->CountB -= nextevent;
                while (c->CountB <= 0) {
                    c->CountB += c->PeriodB;
                    if (c->CountB > 0) { c->OutputB ^= 1; break; }
                    c->CountB += c->PeriodB;
                }
            }

            /* ---- Channel C ---- */
            if (outn & 0x20) {
                if (c->OutputC) volc += c->CountC;
                c->CountC -= nextevent;
                while (c->CountC <= 0) {
                    c->CountC += c->PeriodC;
                    if (c->CountC > 0) {
                        c->OutputC ^= 1;
                        if (c->OutputC) volc += c->PeriodC;
                        break;
                    }
                    c->CountC += c->PeriodC;
                    volc += c->PeriodC;
                }
                if (c->OutputC) volc -= c->CountC;
            } else {
                c->CountC -= nextevent;
                while (c->CountC <= 0) {
                    c->CountC += c->PeriodC;
                    if (c->CountC > 0) { c->OutputC ^= 1; break; }
                    c->CountC += c->PeriodC;
                }
            }

            /* ---- Noise ---- */
            c->CountN -= nextevent;
            if (c->CountN <= 0) {
                /* Bit 0 ^ bit 1 of RNG controls whether OutputN toggles. */
                if ((c->RNG + 1) & 2) {
                    c->OutputN = ~c->OutputN;
                    outn = (c->OutputN | c->regs[7]);
                }
                /* 17-bit Galois LFSR with feedback at bits 14 and 17. */
                if (c->RNG & 1) c->RNG ^= 0x24000;
                c->RNG >>= 1;
                c->CountN += c->PeriodN;
            }

            left -= nextevent;
        } while (left > 0);

        /* ---- Envelope progression (one tick per output sample) ---- */
        if (c->Holding == 0) {
            c->CountE -= AY8910_STEP;
            if (c->CountE <= 0) {
                do {
                    c->CountEnv--;
                    c->CountE += c->PeriodE;
                } while (c->CountE <= 0);

                if (c->CountEnv < 0) {
                    if (c->Hold) {
                        if (c->Alternate) c->Attack ^= 0x1f;
                        c->Holding = 1;
                        c->CountEnv = 0;
                    } else {
                        if (c->Alternate && (c->CountEnv & 0x20)) c->Attack ^= 0x1f;
                        c->CountEnv &= 0x1f;
                    }
                }

                c->VolE = c->VolTable[c->CountEnv ^ c->Attack];
                if (c->EnvelopeA) c->VolA = c->VolE;
                if (c->EnvelopeB) c->VolB = c->VolE;
                if (c->EnvelopeC) c->VolC = c->VolE;
            }
        }

        /* ---- Mix: (area * volume) / STEP per channel, sum ----------------- */
        int32_t sum = (int32_t)(
            ((int64_t)vola * c->VolA
           + (int64_t)volb * c->VolB
           + (int64_t)volc * c->VolC) / AY8910_STEP);

#if AY8910_ENABLE_DC_BLOCK
        const int32_t in_dc  = sum;
        const int32_t out_dc = in_dc - c->dc_prev_in
            + (int32_t)(((int64_t)c->dc_prev_out * AY8910_DC_COEF) >> AY8910_DC_SHIFT);
        c->dc_prev_in  = in_dc;
        c->dc_prev_out = out_dc;
        int32_t s = out_dc;
#else
        int32_t s = sum;
#endif

        if (s > 32767)  s = 32767;
        if (s < -32768) s = -32768;
        *dst++ = (int16_t)s;
    }
}
