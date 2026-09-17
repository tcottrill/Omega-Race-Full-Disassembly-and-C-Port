/* ---------------------------------------------------------------------------
 * ay8910.h - AY-3-8910 / AY-3-8912 PSG chip core, plain C.
 *
 * C conversion of the chip-core portion of AAE's
 * aae/aae/sndhrdwr/ay8910.cpp / .h (class AY8910Chip). See the license /
 * attribution block at the top of ay8910.c for full details: the synthesis
 * core is a port of the AY-3-8910 emulator from the M.A.M.E.(TM) project.
 * -------------------------------------------------------------------------*/
#ifndef AY8910_H
#define AY8910_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ay8910 {
    uint8_t regs[16];

    uint32_t UpdateStep;
    int32_t  PeriodA, PeriodB, PeriodC, PeriodN, PeriodE;
    int32_t  CountA,  CountB,  CountC,  CountN,  CountE;
    uint32_t VolA, VolB, VolC, VolE;
    uint8_t  EnvelopeA, EnvelopeB, EnvelopeC;
    uint8_t  OutputA, OutputB, OutputC, OutputN;
    int8_t   CountEnv;
    uint8_t  Hold, Alternate, Attack, Holding;
    uint32_t RNG;

    uint32_t VolTable[32];

    int32_t dc_prev_in, dc_prev_out;
} ay8910;

/* master_clock_hz = chip clock input (Omega Race: 1000000), sample_rate =
 * output PCM rate. Builds the volume table, computes UpdateStep, then calls
 * ay8910_reset. */
void    ay8910_init(ay8910* c, int master_clock_hz, int sample_rate);
void    ay8910_reset(ay8910* c);

/* reg is masked & 0x0F; stores the value then applies the side effects
 * (periods, volumes, envelope restart on reg 13). */
void    ay8910_write_reg(ay8910* c, uint8_t reg, uint8_t data);
uint8_t ay8910_read_reg(const ay8910* c, uint8_t reg);

/* Renders n mono signed 16-bit samples. */
void    ay8910_render(ay8910* c, int16_t* dst, int n);

#ifdef __cplusplus
}
#endif

#endif /* AY8910_H */
