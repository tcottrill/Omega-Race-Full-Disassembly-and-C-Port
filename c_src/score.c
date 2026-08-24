/* score.c - Omega Race C conversion: scoring / high-score / bookkeeping /
 * NVRAM module.
 *
 * Translates disasm/omega_main.asm:
 *   SCORE_ADD             0x2335
 *   HISCORE_CHECK         0x29CD
 *   HISCORE_INSERT        0x29F1
 *   HISCORE_INITIALS      0x2A35 (+ hs_finalize_filter 0x2A8E,
 *                          hs_letter_commit 0x2AB8, hs_redraw_initials 0x2ACB)
 *   AUDIT_SNAPSHOT         0x1540
 *   BOOKKEEP_COIN/ADD      0x1518 / 0x1521
 *   BOOKKEEP_DECAY_AVG     0x158C
 *   BOOKKEEP_HIGHWATER     0x1614
 *   BOOKKEEP_PTR           0x1639
 *   NVRAM_RD_BYTE/WR_BYTES/WIPE/SCORES_RESET/SAVE_ALL   0x0A18-0x0A9C
 *   (plus the NVRAM validate-or-wipe block inside GAME_INIT, 0x09B4-0x0A15,
 *   folded into nvram_boot_load() below - see its header comment)
 * See disasm/FUNCTIONS.md ("Scoring / progression / players", "Interrupt /
 * coins / bookkeeping") and disasm/MAINLINE_NOTES.md ("GAME_INIT").
 *
 * ============================================================================
 * NVRAM MODEL / CONTRACT (CONVENTIONS.md: "model NVRAM as a byte array
 * persisted via omega_hw_nvram_save/load, with the same validation rules")
 * ============================================================================
 * The real hardware NVRAM is a 4-bit chip at 0x5C00, everything stored as
 * nibbles with RRD-based packing. That bit-level mechanism is not
 * reproduced; instead the RAM cells that the ROM actually round-trips
 * through NVRAM are modelled as plain bytes living in (would-be) `g`
 * fields, and omega_hw_nvram_save()/omega_hw_nvram_load() are the host's
 * hook to persist/restore exactly those fields verbatim. The host
 * implementation of those two functions must round-trip:
 *   - g.nvram_template[32]        (0x5C1A signature mirror)
 *   - g.hiscore_bank[0][0][7]      (0x5C3A hiscore record, context A)
 *   - g.hiscore_bank[1][0][7]      (0x5C3A hiscore record, context B)
 *   - g.bookkeep[0x40]            (0x5C58 ledger)
 *   - g.credits_bcd               (0x5C56/57)
 * nvram_boot_load() calls omega_hw_nvram_load() once, expecting the host
 * to have already filled those fields in-place; nvram_save_all() calls
 * omega_hw_nvram_save() the same way. BCD-digit-range and initials
 * A-Z/space validation are applied exactly as GAME_INIT does, so a
 * corrupt/blank save wipes+resets exactly like the ROM.
 *
 * QUIRK, carefully verified against the disassembly (CONVENTIONS rule 6 -
 * preserved, not "fixed"): GAME_INIT's NVRAM restore loop is `ld b,2`
 * around a 7-byte (4 BCD + 3 initials) inner read, i.e. it only ever
 * round-trips the *first* hiscore record of each of the two 42-byte
 * "context" mirror tables at ROM RAM 0x43D3/0x43FD - not the full 6-entry
 * table. Entries #2-#6 of both contexts are always reset to the ROM
 * default table on every power-up and never persisted. Only the #1 (top)
 * score+initials survives a power cycle; the rest of the attract-mode
 * top-6 list resets to factory defaults every boot. This is why
 * hiscore_check()/hiscore_insert() work purely in g.hiscore (the "live"
 * table, ROM 0x43A9) while the *_ctx mirrors are a separate persistence
 * layer, bridged only at PLAY_BEGIN (hiscore_context_activate, ROM
 * 0x0C0A-0x0C1C) and during initials entry (hiscore_context_sync, ROM
 * 0x1292-0x12A4) - both call sites belong to mainline.c, not this file.
 * ==========================================================================*/
#include <string.h>
#include "omega_state.h"
#include "omega_shapes.h"   /* wheel glyph indices + their DVG advances */

/* ---- cross-module externs ------------------------------------------------ */

extern void sound_cmd_send(uint8_t cmd);   /* SOUND_CMD_SEND 0x2E04 (enemies.c/elsewhere) */
extern int  credits_nvram_sync(void);      /* CREDITS_NVRAM_SYNC 0x16E3 (irq_coins.c) */
extern int  start_btn_check(void);         /* START_BTN_CHECK 0x11AA: nonzero = start edge (mainline.c) */
extern void dvg_score_digits(void);        /* dvg_pages.c: shadow 0x4617 digit run */
extern void dvg_bonus_icon_award(int xlife_bit); /* dvg_pages.c: 0xCE03 -> bonus slot */
extern void dvg_wheel_cursor(uint8_t letter_sel);       /* shadow 0x4491 LABS X */
extern void dvg_initials_redraw(const uint8_t initials[3]); /* shadow 0x4489+ */

/* ---- forward declarations (definitions below, see each for ROM addr/role) */

static uint8_t* bookkeep_ptr(uint8_t index);
static void     bookkeep_decay_avg(uint8_t index, const uint8_t* source,
                                    uint8_t* games_ctr, int mode_time);
static void     hiscore_insert(int slot);
static void     hs_finalize_filter(void);
static void     hs_redraw_initials(void);
static void     score_display_update(void);
static uint8_t  bcd_sub8(uint8_t a, uint8_t b, int* borrow);
void            nvram_save_all(void);   /* forward ref: scores_reset() calls this before its definition below */

/* ---- ROM data tables ------------------------------------------------------ */

/* ROM 0x2FDB: kill-tier point values, 2-byte BCD little-endian, indexed by
 * kill_tier-1 (kill_tier is 1-based; 1=350, 2=500, 3=1000, 4=1500, 5=2500). */
static const uint8_t POINTS_TABLE[5][2] = {
    {0x50, 0x03},   /* 350  */
    {0x00, 0x05},   /* 500  */
    {0x00, 0x10},   /* 1000 */
    {0x00, 0x15},   /* 1500 */
    {0x00, 0x25},   /* 2500 */
};

/* ROM 0x2FF7: default high-score table, 6 records of {4-byte BCD score LE,
 * 3 ASCII initials}. */
const uint8_t DEFAULT_HISCORES[6][7] = {   /* extern'd by mainline.c */
    {0x50, 0x82, 0x03, 0x00, 'A', 'E', ' '},   /* 00038250 "AE " */
    {0x50, 0x13, 0x03, 0x00, 'D', 'L', 'M'},   /* 00031350 "DLM" */
    {0x00, 0x05, 0x03, 0x00, 'M', 'I', 'C'},   /* 00030500 "MIC" */
    {0x50, 0x33, 0x02, 0x00, 'L', 'R', 'S'},   /* 00023350 "LRS" */
    {0x00, 0x24, 0x02, 0x00, 'J', 'A', 'C'},   /* 00022400 "JAC" */
    {0x50, 0x95, 0x01, 0x00, 'R', 'D', 'H'},   /* 00019550 "RDH" */
};

/* ROM 0x30C9: 16-byte NVRAM signature/canary pattern, compared low-nibble
 * only against two consecutive copies in NVRAM at boot. */
static const uint8_t NVRAM_TEMPLATE_REF[16] = {
    0x0F, 0x0D, 0x0B, 0x09, 0x07, 0x05, 0x03, 0x01,
    0x00, 0x02, 0x04, 0x06, 0x08, 0x0A, 0x0C, 0x0E,
};

/* Score-line redraw: the ROM writes glyph JSRLs into the fixed digit run
 * GAME_DLIST_BUILD lays out once (0x4617 in the shadow, 0x8196 live);
 * the *contents* refresh (BCD_TO_DIGITS, 0x240E) belongs here. The
 * score position is omega_state.h's HUD_SCORE_*, one definition shared
 * with frame.c (score_display_update() refreshes the same display slot
 * frame.c lays out). The high-score initials have no derived position
 * of their own; they belong to the hiscore-entry page layout (pages
 * 0x11-0x13). */
/* ---- mode-4 letter wheel geometry ------------------------------------
 * Transcribed from the 0x7A-byte display list SHIP_RESPAWN copies out of
 * ROM 0x3A0E (0x0EF7-0x0F00) into the shadow list at 0x4481, which
 * VG_KICK_TITLE_180D re-uploads to vector RAM every mode-4 frame. Decoded
 * with the same LABS convention pages.c uses ({y, x, hdr_op, scale}, scale
 * = the second LABS word's top nibble):
 *
 *   3A0E  A100 21A0 6000 0000   LABS y=256 x=416 scale=2
 *   3A16  CD92 CD92 CD92        the three typed initials (space, space, space)
 *   3A1C  A1F4 0200 7000 0000   LABS y=500 x=512 scale=0   <- x is PATCHED
 *   3A24  F0FA                  SVEC: 16-unit bright bar = the cursor
 *   3A26  A200 1020 8000 0000   LABS y=512 x=32  scale=1
 *   3A2E  CC4D..CD27            26 JSRLs, 'A'..'Z' (this is also the table
 *                               CHAR_GLYPH_EMIT indexes at $3A2E)
 *   3A62  CD92                  space
 *   3A64  CC75 CDBF CCDC CDBF   'E' 'R' 'A' 'S' 'E' with CHAR_SPACE2 between
 *         CC4D CDBF CCE6 CDBF CC75
 *   3A76  A200 13C0 8000 0000   LABS y=512 x=960 scale=1
 *   3A7E  CC75 CDBF CCBA CDBF CC6C   'E' 'N' 'D'
 *
 * CHAR_SPACE2 (vecrom 0xDBF) advances (-15.969, -19.969), so ERASE and END
 * are drawn as vertical stacks, each occupying a single 32-unit selection
 * column. At scale 1 a letter advances 16*2 = 32, exactly one column, so
 * the wheel index reads straight off the x position:
 *   index 0-25 = 'A'-'Z' (x 32..832), 26 = the space,
 *   index 27 (0x1B) = ERASE at x=896, index 29 (0x1D) = END at x=960
 * which is precisely the set HISCORE_INITIALS acts on at 0x2A71-0x2A7B. */
#define WHEEL_Y         512.0f
#define WHEEL_X0         32.0f
#define WHEEL_PITCH      32.0f
#define WHEEL_END_X     960.0f
#define WHEEL_SCALE         1
#define CURSOR_Y        500.0f
#define CURSOR_LEN       16.0f   /* SVEC F0FA at LABS scale 0 */
#define CURSOR_Z         0x0F
#define INITIALS_X      416.0f   /* LABS 3A0E: y=0x100, x=0x1A0, scale 2 */
#define INITIALS_Y      256.0f
#define INITIALS_SCALE      2

/* ---- BCD helpers (declared in omega_state.h) ----------------------------- */

/* Single-byte BCD add with carry in/out, done digit-wise rather than by
 * emulating the Z80 DAA flag logic byte-for-byte - equivalent for valid
 * BCD operands (guaranteed here: NVRAM validation and gameplay never
 * produce out-of-range nibbles). */
uint8_t bcd_add8(uint8_t a, uint8_t b, int* carry) {
    int lo = (a & 0x0F) + (b & 0x0F) + (*carry ? 1 : 0);
    int hi = (a >> 4) + (b >> 4);
    if (lo > 9) { lo -= 10; hi += 1; }
    if (hi > 9) { hi -= 10; *carry = 1; } else { *carry = 0; }
    return (uint8_t)((hi << 4) | lo);
}

static uint8_t bcd_sub8(uint8_t a, uint8_t b, int* borrow) {
    int lo = (a & 0x0F) - (b & 0x0F) - (*borrow ? 1 : 0);
    int hi = (a >> 4) - (b >> 4);
    if (lo < 0) { lo += 10; hi -= 1; }
    if (hi < 0) { hi += 10; *borrow = 1; } else { *borrow = 0; }
    return (uint8_t)((hi << 4) | lo);
}

/* dst += src, n-byte BCD, little-endian (byte 0 = least-significant digit
 * pair), matching every chained-BCD-add loop in the ROM (SCORE_ADD,
 * BOOKKEEP_ADD, BCD_ADD4, ...). */
void bcd_add(uint8_t* dst, const uint8_t* src, int n) {
    int carry = 0;
    int i;
    for (i = 0; i < n; i++) dst[i] = bcd_add8(dst[i], src[i], &carry);
}

/* Magnitude compare of two n-byte little-endian BCD numbers, -1/0/1.
 * Equivalent to the ROM's chained sbc-a,(hl)/daa borrow-compare loops
 * (HISCORE_CHECK, BOOKKEEP_HIGHWATER): since BCD byte values are
 * monotonic as plain 8-bit magnitudes (0x00..0x99, no A-F nibbles), a
 * most-significant-byte-first compare gives the identical ordering
 * without needing to replicate the borrow chain. */
int bcd_cmp(const uint8_t* a, const uint8_t* b, int n) {
    int i;
    for (i = n - 1; i >= 0; i--) {
        if (a[i] != b[i]) return (a[i] > b[i]) ? 1 : -1;
    }
    return 0;
}

/* ============================================================================
 * SCORE_ADD (0x2335): add kill_tier's point value to score_cur, refresh
 * the score display, then test the three extra-life thresholds in order
 * (bit5=tier1/xlife_thr[0], bit4=tier2/xlife_thr[1], bit3=tier3/
 * xlife_thr[2] of g.xlife_flags - matches the ROM's descending-bit/
 * ascending-threshold order exactly). At most one tier is newly awarded
 * per call; the sound/lives-increment tail only runs when a tier is
 * *newly* crossed this call (matches the ROM's early-outs at every
 * "score below this threshold" and "already latched" branch).
 * ==========================================================================*/
/* The tier is a parameter, not a global: the ROM passes it in A and uses
 * it directly (0x2340 `dec a` / `rlca` indexes the points table at
 * 0x2FDB); it does NOT read 0x406F here. frame.c's wave-clear bonus
 * calls score_add(5) twice for 2 x 2500 = the 5,000 points page 0x16
 * announces. */
void score_add(uint8_t kill_tier) {
    if (kill_tier == 0) return;             /* "or a; call nz,SCORE_ADD" at the call site */
    if (!(g.svc_flags & 0x80)) return;      /* 0x402E bit7: gameplay-active gate */

    /* score_cur += points_table[kill_tier-1], 4-byte BCD accumulate. The
     * ROM does this as two loops (b=2 add-from-table, then b=2
     * carry-only-propagate); folded here into one 4-byte add against a
     * zero-padded points value with a single carry chain - identical
     * result, since the table entry is only 2 bytes wide. */
    {
        uint8_t padded[4];
        int carry = 0;
        int i;
        const uint8_t* pts = POINTS_TABLE[kill_tier - 1];
        padded[0] = pts[0]; padded[1] = pts[1]; padded[2] = 0; padded[3] = 0;
        for (i = 0; i < 4; i++) g.score_cur[i] = bcd_add8(g.score_cur[i], padded[i], &carry);
    }

    score_display_update();

    {
        uint16_t score_hi = (uint16_t)(((uint16_t)g.score_cur[3] << 8) | g.score_cur[2]);
        int awarded = 0;

        if (score_hi >= g.xlife_thr[0]) {
            if (!(g.xlife_flags & 0x20)) {
                g.xlife_flags |= 0x20;
                dvg_bonus_icon_award(5);  /* 0x2394: 0xCE03 -> $82A6 */
                awarded = 1;
            } else if (score_hi >= g.xlife_thr[1]) {
                if (!(g.xlife_flags & 0x10)) {
                    g.xlife_flags |= 0x10;
                    dvg_bonus_icon_award(4);  /* 0x23BF: -> $82B0 */
                    awarded = 1;
                } else if (score_hi >= g.xlife_thr[2]) {
                    if (!(g.xlife_flags & 0x08)) {
                        g.xlife_flags |= 0x08;
                        dvg_bonus_icon_award(3);  /* 0x23EA: -> $82BA */
                        awarded = 1;
                    }
                }
            }
        }

        if (awarded) {
            sound_cmd_send(0);
            g.timer_seconds = 0;
            sound_cmd_send(0x14);
            g.timer_seconds = 3;
            g.sound_ready = 0xFF;
            g.lives++;
            g.counter_406D++;
        }
    }
}

/* Score-line redraw (SCORE_ADD 0x2367: BCD_TO_DIGITS into the shadow
 * digit run at 0x4617, which VG_RESTART's per-frame copy publishes). */
static void score_display_update(void) {
    dvg_score_digits();
}

/* ============================================================================
 * HISCORE_CHECK (0x29CD): score_cur vs the 6x7-byte hiscore table, top
 * record first. First record score_cur qualifies for (>=) gets inserted;
 * if none qualify, drop straight back to attract (game_mode=1).
 * ==========================================================================*/
void hiscore_check(void) {
    int i;
    for (i = 0; i < 6; i++) {
        if (bcd_cmp(g.score_cur, g.hiscore[i], 4) >= 0) {
            hiscore_insert(i);
            return;
        }
    }
    g.game_mode = 1;   /* attract: score didn't make the table */
}

/* ============================================================================
 * HISCORE_INSERT (0x29F1): LDDR shift semantics -> memmove. Drops the
 * table's last entry, shifts [slot..4] down to [slot+1..5], writes the
 * new score into `slot` with blank initials, and arms the initials-entry
 * cursor. Ends by entering mode 4 (high-score initials entry).
 * ==========================================================================*/
static void hiscore_insert(int slot) {
    if (slot < 5) {
        memmove(&g.hiscore[slot + 1], &g.hiscore[slot], (size_t)(5 - slot) * 7);
    }
    memcpy(g.hiscore[slot], g.score_cur, 4);
    g.hiscore[slot][4] = 0x20;
    g.hiscore[slot][5] = 0x20;
    g.hiscore[slot][6] = 0x20;

    g.initials_cur = &g.hiscore[slot][4];
    g.initials_ptr = &g.hiscore[slot][4];
    g.initials_cnt = 0;
    g.game_mode = 4;
}

/* ============================================================================
 * hiscore_context_activate / hiscore_context_sync: the bridge between the
 * "live" table (g.hiscore, used by hiscore_check/hiscore_insert during
 * gameplay) and the two NVRAM-backed context mirrors (g.hiscore_bank),
 * ROM 0x0C0A-0x0C1C (PLAY_BEGIN) and 0x1292-0x12A4 (HISCORE_ENTRY_FRAME).
 * Both call sites are mainline.c's (see final report); exposed here since
 * the tables themselves are this module's data.
 * ==========================================================================*/
void hiscore_context_activate(int ctx) {
    memcpy(g.hiscore, g.hiscore_bank[ctx ? 1 : 0], sizeof(g.hiscore));
}

void hiscore_context_sync(int ctx) {
    memcpy(g.hiscore_bank[ctx ? 1 : 0], g.hiscore, sizeof(g.hiscore));
}

/* ============================================================================
 * Mode-4 letter wheel: the static half of the mode-4 screen is the ROM's
 * own 0x3A0E display list (ROM 0x0EF7-0x0F00's `ld bc,$007A /
 * ld de,dlist_shadow / ld hl,$3A0E / ldir`) - that blob IS the
 * selectable alphabet. It is installed into the shadow by
 * dvg_letterwheel_install() (dvg_pages.c, called from frame.c's
 * ship_respawn); the blob's geometry is decoded in the letter-wheel
 * comment at the top of this file.
 * ==========================================================================*/

/* ============================================================================
 * HISCORE_INITIALS (0x2A35): per-frame mode-4 handler.
 *  - spinner (via obj[OBJ_SHIP].angle, ROM 0x40BC) -> 0-31 letter-wheel
 *    index (g.letter_sel)
 *  - start-button edge or 20s idle timeout (16s once all 3 letters are
 *    typed - see hs_redraw_initials) -> finalize
 *  - fire edge (g.obj[OBJ_SHIP].flags0 bit2, "armed") commits the current
 *    wheel selection: 0-25 = letter A-Z, 27 = backspace, 29 = done;
 *    26/28/30/31 are unused wheel positions (no-op)
 * Every path that does NOT edit a letter returns without touching the
 * idle timer or redrawing (matches the ROM's short-return branches to
 * hs_redraw_initials_2AEE, which is *not* the same routine as the
 * full hs_redraw_initials called after a successful edit).
 * ==========================================================================*/
void hiscore_initials(void) {
    g.letter_sel = (uint8_t)((g.obj[OBJ_SHIP].angle & 0x3E) >> 1); /* ROM 0x2A39 reads the same cell */

    /* ROM 0x2A42-0x2A51: hl = letter_sel*32 + 0x20, stored over the LABS X
     * word of the cursor block at $4491/$4492 (shadow). That is the whole
     * animation - the wheel glyphs never move, the static SVEC dash is
     * repositioned by this one word. Runs before the button checks,
     * exactly as the ROM does. */
    dvg_wheel_cursor(g.letter_sel);

    if (start_btn_check()) { hs_finalize_filter(); return; }
    if (g.seconds_ctr >= 0x14) { hs_finalize_filter(); return; }   /* 20s idle timeout */

    if (!(g.obj[OBJ_SHIP].flags0 & 0x04)) return;   /* no fire edge this frame */
    g.obj[OBJ_SHIP].flags0 &= (uint8_t)~0x04;        /* consume it */

    if (g.letter_sel < 0x1A) {                        /* 0-25: A-Z */
        if (g.initials_cnt >= 3) return;
        g.initials_cnt++;
        *g.initials_cur = (uint8_t)(0x41 + g.letter_sel);
        g.initials_cur++;
        hs_redraw_initials();
        return;
    }
    if (g.letter_sel == 0x1D) { hs_finalize_filter(); return; }   /* 29: done */
    if (g.letter_sel != 0x1B) return;                              /* 26/28/30/31: unused */

    /* 27: backspace */
    if (g.initials_cnt == 0) return;
    g.initials_cnt--;
    g.initials_cur--;
    *g.initials_cur = 0x20;
    hs_redraw_initials();
}

/* ============================================================================
 * hs_finalize_filter (0x2A8E): exits initials entry (game_mode=1) and
 * applies the profanity filter - blanks all 3 initials if they spell
 * F/S + U + C/K (i.e. "FUC", "FUK", "SUC", "SUK"). No redraw/timer touch
 * (matches the ROM falling straight into the short-return path).
 * ==========================================================================*/
static void hs_finalize_filter(void) {
    g.game_mode = 1;

    {
        uint8_t* p = g.initials_ptr;
        if ((p[0] == 'F' || p[0] == 'S') && p[1] == 'U' && (p[2] == 'C' || p[2] == 'K')) {
            p[0] = p[1] = p[2] = 0x20;
        }
    }
}

/* ============================================================================
 * hs_redraw_initials (0x2ACB): reached only after a successful letter
 * commit or backspace. Resets the 20s idle clock (seconds_ctr=0), reloads
 * the display timer, redraws the 3-letter field, and - if all 3 letters
 * are now filled - pre-loads seconds_ctr to 0x10 instead, shrinking the
 * remaining idle grace period to 4s (0x14-0x10) unless another edit
 * resets it to 0 again.
 * ==========================================================================*/
static void hs_redraw_initials(void) {
    g.seconds_ctr = 0;
    g.fire_cooldown = 0x14;

    /* 0x2AD3-0x2AE0: three CHAR_GLYPH_EMITs from the hiscore-work chars
     * into shadow 0x4489/8B/8D (vram 0x8008-0x800C), over the blob's
     * space words. Position comes from the blob's own LABS. */
    dvg_initials_redraw(g.initials_ptr);

    if (g.initials_cnt == 3) g.seconds_ctr = 0x10;
}

/* ============================================================================
 * BOOKKEEP_PTR (0x1639): ledger[index], 4 bytes per entry, 16 entries in
 * g.bookkeep[0x40]. Index*4 via two rlca in the ROM (safe here as long as
 * index < 16, which every real caller respects).
 * ==========================================================================*/
static uint8_t* bookkeep_ptr(uint8_t index) {
    return &g.bookkeep[(uint8_t)(index << 2)];
}

/* ============================================================================
 * BOOKKEEP_ADD (0x1521) / BOOKKEEP_COIN (0x1518): ledger[index] += val.
 * NOTE (quirk, preserved): the ROM's BOOKKEEP_ADD only ever actually reads
 * *hl once (byte 0 of the source) despite FUNCTIONS.md's "4-byte BCD"
 * description - bytes 1-3 of the ledger entry are ripple-carried
 * (adc a,0) rather than added from further source bytes. Its only caller
 * (BOOKKEEP_COIN) always passes the ROM 0x3FF7 "+1" constant, so in
 * practice this is exactly ledger[index] += 1 with carry propagate; kept
 * as a general single-byte-add primitive for fidelity to the actual code.
 * ==========================================================================*/
void bookkeep_add(uint8_t index, uint8_t val0) {
    uint8_t* p = bookkeep_ptr(index);
    int carry = 0;
    int i;
    p[0] = bcd_add8(p[0], val0, &carry);
    for (i = 1; i < 4; i++) p[i] = bcd_add8(p[i], 0, &carry);
}

void bookkeep_coin(uint8_t index) {
    bookkeep_add(index, 1);   /* ROM 0x3FF7 BCD "+1" constant */
}

/* ============================================================================
 * BOOKKEEP_DECAY_AVG (0x158C): leaky running-average ledger update.
 *
 * For the first 100 samples (games_ctr high byte == 0): ledger is a plain
 * running BCD sum (ledger += source, all 4 bytes). The moment the games
 * counter's low byte rolls 99->00 (i.e. this is sample #100), the ledger
 * is rescaled down by /100 (byte-shift: ledger = ledger>>1byte) to seed
 * the decay-average phase - but only on the *score* call (mode_time==0);
 * the games-counter increment itself is only committed on the
 * *time_played* call (mode_time==1), matching the ROM exactly (the score
 * call computes-and-discards the increment; AUDIT_SNAPSHOT always calls
 * the score variant before the time_played variant in the same pass, so
 * the counter still advances exactly once per snapshot).
 *
 * From sample #100 onward: leaky decay, no further games-counter update
 * (verified: the ROM's >=100 branch never touches the counter again -
 * once past the switchover it's a pure fixed-rate EMA and doesn't need a
 * sample count anymore). Each update: scratch = ledger>>1byte (~ /100);
 * ledger -= scratch (1% decay); ledger[0..2] += source[appropriate 3
 * bytes] (score drops its low byte / lowest 2 digits to match the /100
 * scaling; time_played drops its high byte instead - asymmetric but
 * transcribed exactly as coded).
 * ==========================================================================*/
static void bookkeep_decay_avg(uint8_t index, const uint8_t* source,
                                uint8_t* games_ctr, int mode_time) {
    uint8_t* ledger = bookkeep_ptr(index);

    if (games_ctr[1] == 0) {
        {
            int carry = 0;
            int i;
            for (i = 0; i < 4; i++) ledger[i] = bcd_add8(ledger[i], source[i], &carry);
        }
        {
            int c2 = 0;
            uint8_t new_low = bcd_add8(games_ctr[0], 1, &c2);
            if (c2) {
                if (mode_time) {
                    games_ctr[0] = new_low;   /* == 0 */
                    games_ctr[1]++;            /* plain binary inc, matches ROM */
                } else {
                    ledger[0] = ledger[1];
                    ledger[1] = ledger[2];
                    ledger[2] = ledger[3];
                    ledger[3] = 0;
                }
            } else if (mode_time) {
                games_ctr[0] = new_low;
            }
        }
        return;
    }

    {
        uint8_t scaled[4];
        int borrow = 0;
        int carry = 0;
        int i;

        scaled[0] = ledger[1];
        scaled[1] = ledger[2];
        scaled[2] = ledger[3];
        scaled[3] = 0;
        for (i = 0; i < 4; i++) ledger[i] = bcd_sub8(ledger[i], scaled[i], &borrow);

        for (i = 0; i < 3; i++) {
            uint8_t s = mode_time ? source[i] : source[i + 1];
            ledger[i] = bcd_add8(ledger[i], s, &carry);
        }
        if (!mode_time) ledger[3] = bcd_add8(ledger[3], 0, &carry);
    }
}

/* ============================================================================
 * BOOKKEEP_HIGHWATER (0x1614): ledger[index] = max(ledger[index], time_played).
 * ==========================================================================*/
void bookkeep_highwater(uint8_t index) {
    uint8_t* ledger = bookkeep_ptr(index);
    if (bcd_cmp(g.time_played, ledger, 4) >= 0) {
        memcpy(ledger, g.time_played, 4);
    }
}

/* ============================================================================
 * AUDIT_SNAPSHOT (0x1540): called on the ship's last life. Banks into
 * ledger slots [4..9] or [10..15] by g.game_flags bit5; folds the three
 * xlife award events, a decaying
 * average of score and time_played, and a time_played high-water mark
 * into six consecutive ledger entries. The two games-played sub-counters
 * live inside the bookkeeping ledger itself (ROM 0x4433/0x4435, i.e.
 * g.bookkeep offsets 0x0C/0x0E - no separate field needed).
 * ==========================================================================*/
void audit_snapshot(void) {
    uint8_t base;
    uint8_t* games_ctr;

    if (g.game_flags & 0x20) {
        base = 0x0A;
        games_ctr = &g.bookkeep[0x0E];
    } else {
        base = 0x04;
        games_ctr = &g.bookkeep[0x0C];
    }

    if (g.xlife_flags & 0x20) bookkeep_coin(base);
    if (g.xlife_flags & 0x10) bookkeep_coin((uint8_t)(base + 1));
    if (g.xlife_flags & 0x08) bookkeep_coin((uint8_t)(base + 2));

    bookkeep_decay_avg((uint8_t)(base + 3), g.score_cur, games_ctr, 0);
    bookkeep_decay_avg((uint8_t)(base + 4), g.time_played, games_ctr, 1);
    bookkeep_highwater((uint8_t)(base + 5));
}

/* ============================================================================
 * NVRAM_WIPE (0x0A3B) / SCORES_RESET (0x0A4C) / NVRAM_SAVE_ALL (0x0A68).
 * In the ROM, NVRAM_WIPE always falls straight through into SCORES_RESET
 * (no ret between them, every caller reaches it via jr/jp not call) -
 * kept as two named functions for clarity, but nvram_boot_load()'s
 * failure path (and any operator-menu "erase everything" action) must
 * call both, in order, matching that fallthrough.
 * ==========================================================================*/
void nvram_wipe(void) {
    g.credits_bcd = 0;
    memset(g.bookkeep, 0, sizeof(g.bookkeep));
}

void scores_reset(void) {
    int i, ctx;
    for (i = 0; i < 6; i++) memcpy(g.hiscore[i], DEFAULT_HISCORES[i], 7);
    for (ctx = 0; ctx < 2; ctx++)
        for (i = 0; i < 6; i++) memcpy(g.hiscore_bank[ctx][i], DEFAULT_HISCORES[i], 7);
    nvram_save_all();
}

void nvram_save_all(void) {
    int i;
    for (i = 0; i < 16; i++) {
        g.nvram_template[i]      = NVRAM_TEMPLATE_REF[i];
        g.nvram_template[16 + i] = NVRAM_TEMPLATE_REF[i];
    }
    /* ROM 0x0A86: NVRAM_SAVE_ALL calls CREDITS_NVRAM_SYNC as part of the
     * save. The host persists the g.nvram_credits[] shadow, not
     * g.credits_bcd, so the sync must run before omega_hw_nvram_save().
     * The hiscore banks and bookkeeping ledger are live state the host
     * reads directly (see the NVRAM CONTRACT comment at the top of this
     * file); credits are the one field with a shadow. */
    credits_nvram_sync();
    omega_hw_nvram_save();
}

/* ============================================================================
 * nvram_boot_load(): the GAME_INIT NVRAM validate-or-wipe block
 * (0x09B4-0x0A15). Seeds ROM defaults unconditionally (matching the
 * ROM's unconditional LDIRs before validation even starts), asks the
 * host to overlay persisted bytes on top, then validates: NVRAM
 * signature template (2x16, low-nibble compare), the #1 hiscore record
 * of both context mirrors (4 BCD-range-checked score bytes + 3
 * space/A-Z-checked initials), the full 64-byte bookkeeping ledger
 * (BCD-range checked), and credits (BCD-range checked). Any failure ->
 * wipe + reset, matching the ROM exactly.
 *
 * Returns 1 on a valid restore, 0 if it had to wipe/reset. Caller
 * (mainline.c's game_init()) should call this once at cold boot.
 * ==========================================================================*/
int nvram_boot_load(void) {
    int i, ctx;

    for (i = 0; i < 6; i++) memcpy(g.hiscore[i], DEFAULT_HISCORES[i], 7);
    for (ctx = 0; ctx < 2; ctx++)
        for (i = 0; i < 6; i++) memcpy(g.hiscore_bank[ctx][i], DEFAULT_HISCORES[i], 7);

    omega_hw_nvram_load();   /* host overlays persisted bytes per the NVRAM CONTRACT */

    for (i = 0; i < 16; i++) {
        if ((g.nvram_template[i] & 0x0F) != NVRAM_TEMPLATE_REF[i]) goto fail;
        if ((g.nvram_template[16 + i] & 0x0F) != NVRAM_TEMPLATE_REF[i]) goto fail;
    }

    for (ctx = 0; ctx < 2; ctx++) {
        uint8_t* rec = g.hiscore_bank[ctx][0];
        for (i = 0; i < 4; i++) {
            if ((rec[i] & 0x0F) > 9 || ((rec[i] >> 4) & 0x0F) > 9) goto fail;
        }
        for (i = 0; i < 3; i++) {
            uint8_t c = rec[4 + i];
            if (c != 0x20 && (c < 'A' || c > 'Z')) goto fail;
        }
    }

    for (i = 0; i < 0x40; i++) {
        uint8_t v = g.bookkeep[i];
        if ((v & 0x0F) > 9 || ((v >> 4) & 0x0F) > 9) goto fail;
    }

    /* Credits, ROM 0x0A0A-0x0A13: `ld de,$5C56 / ld hl,credits_bcd /
     * call NVRAM_RD_BYTE`. NVRAM_RD_BYTE takes the LOW nibble of each of
     * the two shadow bytes, rejects either being >= 0x0A, and `rrd`s them
     * into credits_bcd - which reassembles (hi<<4)|lo, the inverse of
     * CREDITS_NVRAM_SYNC's plain + nibble-swapped pair. The load must
     * come from the g.nvram_credits[] shadow (the bytes the host
     * persists), not from g.credits_bcd itself. */
    {
        uint8_t lo = (uint8_t)(g.nvram_credits[0] & 0x0F);
        uint8_t hi = (uint8_t)(g.nvram_credits[1] & 0x0F);
        if (lo > 9 || hi > 9) goto fail;
        g.credits_bcd = (uint8_t)((hi << 4) | lo);
    }

    /* Quirk preserved on purpose (see NVRAM MODEL comment): g.hiscore
     * itself is NOT refreshed from the just-restored context here - the
     * ROM doesn't either. The restored #1 score only becomes visible in
     * the live table (and thus attract-mode hiscore pages) once
     * hiscore_context_activate() runs at the first PLAY_BEGIN of the
     * session. */
    return 1;

fail:
    nvram_wipe();
    scores_reset();
    return 0;
}
