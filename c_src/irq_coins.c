/* irq_coins.c - Omega Race C conversion: 244 Hz service module.
 *
 * Translates disasm/omega_main.asm IRQ_244HZ (0x0038-0x019E): tick,
 * slam/tilt handling, coin-line debounce/accept, BCD credit math against
 * the DIP coinage table, and the coin-meter pulse generator. See
 * disasm/BOOT_AND_IRQ_NOTES.md ("The complete interrupt cycle")
 * for the annotated walkthrough this file follows line-by-line.
 *
 * Also covers (deviating from the CONVENTIONS.md module map default of
 * mainline.c) BOOKKEEP_COIN/BOOKKEEP_ADD (0x1518/0x1521),
 * CREDITS_NVRAM_SYNC (0x16E3), START_BTN_CHECK (0x11AA) + its
 * START_LEDS_OFF (0x1248) helper, and the DIP decode arithmetic used by
 * GAME_INIT (coinage, 0x0977-0x0999) and ATTRACT_INIT (lives/bonus/
 * difficulty, 0x0B0E-0x0B57, and the free-play force at 0x0AC2-0x0AC8).
 * ATTRACT_INIT/GAME_INIT themselves live in mainline.c; the functions
 * here are the pure decode helpers those routines call into.
 *
 * IY is fixed at 0x4080 for all of this code; every (iy+d)/(iy-d) below
 * is annotated with the absolute RAM cell it resolves to.
 *
 * ---- state ------------------------------------------------------------
 * All ROM RAM cells this module needs live directly in the canonical `g`
 * (omega_state.h): coin_lines, slam_latch, coin_debounce[0]/[1] (coin1/
 * coin2 debounce), coin_pending[0]/[1], coin_batch[0]/[1], meter_pulse_t,
 * led_shadow, nvram_credits[0]/[1] (lo/hi nibble-swapped NVRAM shadow),
 * svc_flags (0x402E: bit6 credit-display-dirty owned by this module,
 * bit7 tested by FRAME_ENGINE - frame.c, out of scope here), game_flags
 * (0x4041: bit2 cocktail seat2, bit3 player-swap pending, bit4
 * hiscore-entry gate, bit5 deluxe start (2 credits/player = bonus ship),
 * bit6 2-player - all consumed by frame.c), start_credits (0x4040:
 * credits the pending start consumes, 1/2/4; debited by PLAY_BEGIN),
 * cur_player_num (0x403E: set 1 by ATTRACT_INIT default / 2 on confirmed
 * 2P start), player_sel (0x403F: NOT a duplicate of cur_player_num -
 * it is the count of players still owed a mode-4 initials entry,
 * decremented by HISCORE_ENTRY_FRAME 0x12AA; the two only happen to be
 * written together at the start sites here).
 */
#include <stdint.h>
#include "omega_state.h"

/* ---- forward declarations ------------------------------------------------ */

void    omega_irq_244hz(void);
void    bookkeep_coin(uint8_t index);
void    bookkeep_add(uint8_t index, uint8_t value);
int     credits_nvram_sync(void);   /* nonzero if the shadow changed */
int     start_btn_check(void);
void    start_leds_off(void);
void    dip_decode_coinage(void);
void    dip_apply_free_play(void);

static void    coin_accept_tick(int mech, uint8_t new_lines);
static void    meter_pulse_tick(void);
static uint8_t bcd_dec1(uint8_t v);

/* ---- ROM data tables ------------------------------------------------------ */

/* ROM 0x2FC3-0x2FD2: (coins-per-credit, credits-per-batch) x8, one
 * shared table selected by DSW C6 bits 0-2 (mech1, g.coin_mode1) or
 * bits 3-5 (mech2, g.coin_mode2) - both fields already hold a *byte
 * offset* into this table (mirrors the ROM's coinageN_ptr = 0x2FC3 +
 * mode), not two separate table regions. */
static const uint8_t g_coinage_table[16] = {
    1,2,  1,3,  1,5,  4,5,  3,4,  2,3,  2,1,  1,1
};

/* ROM 0x2FD3-0x2FDA: (lives, X) x4 pairs, selected by DSW C4 bits 4-5.
 * The second value's exact downstream use is a display digit-blank
 * computation in ATTRACT_INIT (8 - X) for the bonus-score readout -
 * out of scope here; transcribed as-is. */
static const uint8_t g_lives_table[8] = {
    2,4,  2,5,  3,6,  3,7
};

/* ROM 0x2FE5-0x2FEC: first extra-life score threshold, by DSW C4 bits
 * 0-1 (-> g.xlife_thr[0]). */
static const uint16_t g_bonus_thr_table[4] = {
    0x0004, 0x0005, 0x0007, 0x0010
};

/* ROM 0x2FED-0x2FF6: source for the 2nd/3rd extra-life thresholds (a
 * consecutive pair, selected by DSW C4 bits 2-3 -> g.xlife_thr[1]/[2]).
 * ATTRACT_INIT 0x0B29 is the range's ONLY reader (exhaustive operand
 * sweep of the listing); DIFFICULTY_CALC reads no ROM table at all -
 * its `and $2F` is an immediate mask, not an address. FUNCTIONS.md's
 * "difficulty DIP params" label for this table is a misnomer. */
static const uint16_t g_bonus_thr2_table[5] = {
    0x0015, 0x0025, 0x0050, 0x0075, 0x0150
};

/* ---- small helpers --------------------------------------------------------- */

/* Z80 "SUB $01 / DAA" on a valid (0-99) BCD byte == decimal decrement
 * with inter-nibble borrow. Only ever called with v != 0 (guarded by
 * the caller, matching the ROM's "or a / jr z" guard). */
static uint8_t bcd_dec1(uint8_t v) {
    uint8_t lo = (uint8_t)(v & 0x0F);
    uint8_t hi = (uint8_t)(v >> 4);
    if (lo == 0) {
        lo = 9;
        hi = (uint8_t)(hi - 1);
    } else {
        lo = (uint8_t)(lo - 1);
    }
    return (uint8_t)((hi << 4) | lo);
}

/* BOOKKEEP_ADD (0x1521) / BOOKKEEP_COIN (0x1518) live in score.c;
 * coin_accept_tick below calls them there. */

/* ============================================================================
 * CREDITS_NVRAM_SYNC (0x16E3): mirror credits_bcd into the NVRAM nibble
 * shadow bytes that NVRAM_WR_BYTES/NVRAM_RD_BYTE pack/validate against
 * the 0x30C9 template (0x5C56 plain copy, 0x5C57 nibble-swapped copy -
 * "rrca x4" on a byte == swap nibbles).
 *
 * 0x5C56/57 are battery-backed RAM, so on hardware this store IS the
 * persist - there is nothing else to call. The port's whole-image
 * equivalent is omega_hw_nvram_save(), but that belongs at the call site,
 * not here: this stays the literal 0x16E3 staging step.
 *
 * Returns 1 if the shadow actually changed, so a caller can flush only
 * then. That matters because svc_flags bit6 ("credit display dirty") is
 * set by the coin/slam paths and nothing on the normal attract path
 * clears it - the ROM does that from VG_RESTART_FRAME at 0x1788, which is
 * display-list work this port drops - so MAINLOOP reaches this every
 * frame once any coin has been inserted.
 * ==========================================================================*/
int credits_nvram_sync(void) {
    uint8_t a  = g.credits_bcd;
    uint8_t lo = a;                                      /* 0x5C56 */
    uint8_t hi = (uint8_t)((a >> 4) | (a << 4));          /* 0x5C57, rrca x4 */

    if (g.nvram_credits[0] == lo && g.nvram_credits[1] == hi) return 0;
    g.nvram_credits[0] = lo;
    g.nvram_credits[1] = hi;
    return 1;
}

/* START_LEDS_OFF (0x1248) lives in mainline.c (start_leds_off). */

/* ============================================================================
 * DIP decode helpers (GAME_INIT 0x0977-0x0999, ATTRACT_INIT 0x0AC2-0x0AC8
 * and 0x0B0E-0x0B57). GAME_INIT/ATTRACT_INIT themselves are mainline.c's
 * job; these are the pure decode routines they call into.
 * ==========================================================================*/

/* GAME_INIT 0x0977-0x0999: DSW C6 -> g.coin_mode1/2, each left as a byte
 * offset into g_coinage_table[] (mirrors the ROM's coinageN_ptr = 0x2FC3
 * + mode, but as a table index instead of a raw pointer). Mech1 uses C6
 * bits 0-2, mech2 uses bits 3-5; both index the SAME table. */
void dip_decode_coinage(void) {
    uint8_t c6 = omega_hw_dsw_c6();
    g.coin_mode1 = (uint8_t)((c6 & 0x07) * 2);
    g.coin_mode2 = (uint8_t)(((c6 & 0x38) >> 3) * 2);
}

/* ATTRACT_INIT 0x0AC2-0x0AC8: DSW C6 bit6 set = free play -> force
 * credits to 4. */
void dip_apply_free_play(void) {
    if (omega_hw_dsw_c6() & 0x40) {
        g.credits_bcd = 0x04;
    }
}

typedef struct {
    uint8_t lives;        /* starting lives count (2 or 3) */
    uint8_t digit_blank;  /* 8 - g_lives_table[pair+1]; feeds a display
                              digit-blank marker in ATTRACT_INIT - the
                              display-list side effects at ROM
                              0x0B57-0x0B7B are NOT reproduced here
                              (belongs to pages.c/mainline.c) */
} c4_dip_result;

/* ATTRACT_INIT 0x0B0E-0x0B57: DSW C4 -> extra-life score thresholds
 * (g.xlife_thr[0..2]) and starting lives. */
c4_dip_result dip_decode_c4(void) {
    c4_dip_result r;
    uint8_t c4 = omega_hw_dsw_c4();

    uint8_t widx1 = (uint8_t)(c4 & 0x03);            /* 0x0B10-0x0B12 */
    g.xlife_thr[0] = g_bonus_thr_table[widx1];        /* 0x0B16-0x0B1D */

    uint8_t widx2 = (uint8_t)((c4 >> 2) & 0x03);      /* 0x0B23-0x0B25 */
    g.xlife_thr[1] = g_bonus_thr2_table[widx2];       /* 0x0B29-0x0B31 */
    g.xlife_thr[2] = g_bonus_thr2_table[widx2 + 1u];  /* 0x0B31-0x0B38 */

    uint8_t pidx = (uint8_t)(((c4 >> 4) & 0x03) * 2); /* 0x0B45-0x0B47 */
    r.lives = g_lives_table[pidx];                    /* 0x0B4D-0x0B51 */
    r.digit_blank = (uint8_t)(0x08 - g_lives_table[pidx + 1u]); /* 0x0B52-0x0B56 */
    return r;
}

/* START_BTN_CHECK (0x11AA) lives in mainline.c (start_btn_check). */

/* ============================================================================
 * Coin accept, one mechanism (shared body for coin1 0x0097 / coin2
 * 0x00EA - identical structure in the ROM). mech: 0 = coin1, 1 = coin2.
 * new_lines is the freshly sampled (post-edge-detect) coin_lines value.
 * ==========================================================================*/
static void coin_accept_tick(int mech, uint8_t new_lines) {
    uint8_t *debounce  = (mech == 0) ? &g.coin_debounce[0] : &g.coin_debounce[1];
    uint8_t *pending   = (mech == 0) ? &g.coin_pending[0]  : &g.coin_pending[1];
    uint8_t *batch     = (mech == 0) ? &g.coin_batch[0]    : &g.coin_batch[1];
    uint8_t  coin_mode = (mech == 0) ? g.coin_mode1       : g.coin_mode2;
    uint8_t  line_bit  = (mech == 0) ? 0x01u : 0x02u;
    uint8_t  bk_index  = (uint8_t)mech;              /* BOOKKEEP_COIN ledger slot 0/1 */

    if (*debounce == 0) return;                       /* 0x009A/0x00ED: nothing pending */
    if (--(*debounce) != 0) return;                    /* 0x009D-0x00A0 / 0x00F0-0x00F3 */
    if (new_lines & line_bit) return;                  /* 0x00A2-0x00A4 / 0x00F5-0x00F7:
                                                            line no longer active (active-low) */

    if (omega_hw_in_p1() & 0x80) {                     /* 0x00A6-0x00AA / 0x00F9-0x00FD:
                                                            bookkeeping-enable line, active high */
        bookkeep_coin(bk_index);                        /* 0x00AE/0x0101 */
        (*pending)++;                                    /* 0x00B1/0x0104 */
    }
    omega_hw_sound(0x12);                              /* 0x00B4-0x00B6 / 0x0107-0x0109: coin chime */
    (*batch)++;                                        /* 0x00B8/0x010B */

    uint8_t coins_per_credit = g_coinage_table[coin_mode];       /* 0x00BB-0x00C2 */
    if (*batch < coins_per_credit) return;                        /* 0x00C3: batch not full yet */
    *batch = 0;                                                    /* 0x00C5 */

    uint8_t credits_per_batch = g_coinage_table[coin_mode + 1u];   /* 0x00C9 */
    if (!(omega_hw_in_p1() & 0x80)) {                              /* 0x00CA-0x00CE (inverted test) */
        bookkeep_add(2, credits_per_batch);                          /* 0x00D0-0x00D2: service-credit ledger */
    }

    int carry = 0;   /* bcd_add8 takes carry-IN; must start clear or a
                        phantom extra credit is banked per coin */
    g.credits_bcd = bcd_add8(g.credits_bcd, credits_per_batch, &carry); /* 0x00D5-0x00DA */
    g.svc_flags |= 0x40;                                          /* 0x00DD: credit display dirty */
    if (g.credits_bcd >= 0x21) {                                     /* 0x00E1-0x00E3 */
        g.credits_bcd = 0x20;                                          /* 0x00E5: cap at 20 */
    }
}

/* ============================================================================
 * Coin meter pulse generator (0x013D-0x019B): ~100 ms drive pulse per
 * accepted coin on the mechanical coin meter(s), via the led_shadow
 * shadow of port 0x13.
 * ==========================================================================*/
static void meter_pulse_tick(void) {
    if (g.meter_pulse_t != 0) {                        /* 0x013D-0x0141 */
        g.meter_pulse_t--;                               /* 0x0143-0x0144 */
        if (g.meter_pulse_t != 0) {                       /* 0x0147 */
            if (g.meter_pulse_t == 0x19) {                  /* 0x0149-0x014B: ~100ms elapsed */
                g.led_shadow |= 0x03;                         /* 0x014D-0x0150: assert meter drive */
                /* HW: out($13, led_shadow) dropped */
            }
            return;                                          /* 0x0157/0x014B: irq_exit */
        }
    }

    /* meter_pulse_t just reached (or already was) 0: arm the next pulse
       from any queued coin (0x0159-0x018E). */
    if (g.coin_pending[0] != 0) {                         /* 0x0159-0x015D */
        g.coin_pending[0]--;                                 /* 0x015F */
        g.led_shadow &= (uint8_t)~0x01;                     /* 0x0162: meter1 drive line */
        g.meter_pulse_t = 0x32;                            /* 0x0166 */
    }
    if (g.coin_pending[1] != 0) {                         /* 0x016A-0x016E */
        if (g.coin_mode1 == g.coin_mode2) {                 /* 0x0170-0x0176: shared meter wiring */
            if (g.led_shadow & 0x01) {                       /* 0x0178: not already cleared this tick */
                g.led_shadow &= (uint8_t)~0x01;                /* 0x017E */
                g.coin_pending[1]--;                             /* 0x0188 */
                g.meter_pulse_t = 0x32;                        /* 0x018B */
            }
            /* else: 0x017C jr z irq_meter_release - meter1 already
               drove the shared line this tick, skip */
        } else {
            g.led_shadow &= (uint8_t)~0x02;                   /* 0x0184: independent meter2 line */
            g.coin_pending[1]--;                                /* 0x0188 */
            g.meter_pulse_t = 0x32;                           /* 0x018B */
        }
    }
    if (g.meter_pulse_t == 0x32) {                       /* 0x018F-0x0194: a pulse was just armed */
        /* HW: out($13, led_shadow) dropped */
    }
}

/* ============================================================================
 * IRQ_244HZ (0x0038): the 244 Hz timebase + coin/cabinet service. The
 * host must call this exactly 250 times per emulated second. Preserves
 * AF/BC in the ROM (register plumbing, no C equivalent needed).
 * ==========================================================================*/
void omega_irq_244hz(void) {
    g.tick_244hz++;                                    /* 0x003A: 0x4017 */
    uint8_t old_lines = g.coin_lines;                  /* 0x003D: c = coin_lines (previous) */

    /* ---- slam / tilt (0x0040-0x0078) ----------------------------------- */
    uint8_t p1 = omega_hw_in_p1();                      /* 0x0040 */
    if ((p1 & 0x10) == 0) {                              /* bit4 low = slam switch pressed */
        if (g.slam_latch == 0) {                          /* 0x0046-0x004A: first tick of a new slam */
            omega_hw_sound(0x13);                            /* 0x004C-0x004E: alarm */
            g.slam_latch = 0x13;                            /* 0x0050 */
            g.coin_batch[0] = 0;                              /* 0x0053-0x0055: void partial coins */
            g.coin_batch[1] = 0;                              /* 0x0058 */

            if (g.credits_bcd != 0) {                         /* 0x0068-0x006C */
                g.credits_bcd = bcd_dec1(g.credits_bcd);        /* 0x006E-0x0071: slam credit penalty */
                g.svc_flags |= 0x40;                         /* 0x0074: credit display dirty */
            }
        }
        /* 0x0078: reuses the general 1 Hz countdown cell (g.timer_seconds,
           RAM 0x4021, also written by ATTRACT_INIT with $3C) as a 3-second
           "slam active" hold - genuine ROM aliasing of that scratch cell,
           not a new field. Consumer: MAINLOOP's 1 Hz block 0x0F5D decs it
           and on expiry (0x0F68), slam_latch set -> sound cmd 0 (mute the
           alarm) + jump to ATTRACT_INIT (mainline.c implements this).
           While nonzero it also freezes attract stepping (0x1020). */
        g.timer_seconds = 3;
    }

    /* ---- coin line edge detection (0x007C-0x0095) ----------------------- */
    uint8_t new_lines = (uint8_t)(omega_hw_in_p1() & 0x03);  /* 0x007C-0x007E */
    g.coin_lines = new_lines;                                /* 0x0080 */
    uint8_t changed = (uint8_t)(new_lines ^ old_lines);       /* 0x0084 */
    if (changed & 0x01) g.coin_debounce[0] = 5;                /* 0x0087-0x008B */
    if (changed & 0x02) g.coin_debounce[1] = 5;                /* 0x008F-0x0093 */

    /* ---- coin accept, both mechanisms (0x0097 / 0x00EA) ------------------ */
    coin_accept_tick(0, new_lines);
    coin_accept_tick(1, new_lines);

    /* ---- coin meter pulse generator (0x013D-0x019B) ---------------------- */
    meter_pulse_tick();
}
