/* dvg_pages.c - text pages rendered into the REAL display list.
 *
 * Transcribes the ROM's own page machinery:
 *
 *   DRAW_PAGE         0x2AF3  page setup + immediate draw (no reveal bit)
 *   PAGE_SCRIPT_STEP  0x2B6D  one glyph/escape per call, rate-limited on
 *                             fire_cooldown (reload 10; 25 at line end,
 *                             the end-of-line pause)
 *   CHAR_GLYPH_EMIT   0x2C36  ASCII -> glyph JSRL word at the cursor
 *   BCD_TO_DIGITS     0x240E  BCD bytes -> digit words, leading-zero
 *                             blanking, MSB-first (source walked down)
 *   ATTRACT_PAGE_LOAD 0x16F1  DLIST_RESET + fill 0x8000-0x8301 with the
 *                             word at 0x33D9 (0xE180 = JMPL 0x8300) +
 *                             copy the 26-byte border box 0x33DB->0x8300.
 *                             The fill is the termination trick: any word
 *                             a page has not (yet) overwritten jumps to
 *                             the border + HALT, so half-revealed pages
 *                             end cleanly and the border always draws.
 *
 * Page data is the RAW ROM bytes (omega_pagerom.c, generated): chained
 * records [next(2)][dest(2)][hdr 8][glyph bytes...], chain ends at a
 * 0000 next-word. Page byte 0 bit7 = reveal gradually. Glyph bytes:
 * plain = one ASCII char; bit7 set = escape: bit6 clear = (count, ptr)
 * BCD field via BCD_TO_DIGITS, bit6 set = (count, ptr) run of chars
 * read forward from ptr. Escape pointers are Z80 addresses: ROM
 * addresses inside the page region read from the blob, everything else
 * resolves through pages.c's resolve_ref() (credits, scores, hiscore
 * tables, initials...).
 *
 * ROM state cells used (same names in g):
 *   0x4085 page_ptr        0x4087 page_rec_next
 *   0x4089 page_line_cur   0x408A page_line_glyph
 *   0x408B page_line_total 0x408C page_line_len
 *   0x408D page_script_ptr 0x408F vlist_cursor
 *
 * Coverage (see DVG_PAGES_PLAN.md): every page in the machine's table,
 * 0x01-0x1C, is built here - attract, game HUD, mode-4 entry and the
 * wave-clear announcements.
 */
#include <stdint.h>
#include "omega_state.h"

extern uint8_t omega_vecram[0x2000];
extern uint8_t omega_dlist_shadow[0x1B0];      /* RAM 0x4481-0x4630 */
extern const uint16_t omega_dlist_tail[24];    /* ROM 0x30D9 template */
extern void    dvg_dlist_reset(void);

extern const uint8_t  omega_page_rom[0xDB6];   /* ROM 0x3217-0x3FCC
                                                  (incl. 0x3EE5 HUD
                                                  patch rows and the
                                                  0x3F69 P2 icon
                                                  template)          */
extern const uint16_t omega_page_table[0x1C];  /* ROM 0x3F15        */
extern const uint16_t omega_glyph_rom[21];     /* ROM 0x3FCD-0x3FF6:
                                                  incl. the ?/@ droid-
                                                  icon words 3FF3/3FF5 */

extern const uint8_t  omega_post_test_hdr[0x2C];    /* ROM 0x01A0 */
extern const uint8_t  omega_post_test_text[0xA2];   /* ROM 0x01CC */
extern const uint8_t  omega_post_input_hdr[0x2C];   /* ROM 0x026E */
extern const uint8_t  omega_post_input_text[0x195]; /* ROM 0x029A */
extern const uint8_t  omega_post_xhatch[0x18];      /* ROM 0x042F */
extern const uint8_t  omega_post_grid[0xA0];        /* ROM 0x0447 */

extern int  resolve_ref(uint16_t rom_addr, uint8_t** ptr, int* is_bcd);
void dvg_hud_patch_row(int row);                  /* below */

#define PAGE_ROM_LO 0x3217
#define PAGE_ROM_HI 0x3FCD

/* ---- helpers ---------------------------------------------------------- */

static uint8_t rom8(uint16_t a)
{
    if (a >= PAGE_ROM_LO && a < PAGE_ROM_HI)
        return omega_page_rom[a - PAGE_ROM_LO];
    return 0;
}

static uint16_t rom16(uint16_t a)
{
    return (uint16_t)(rom8(a) | (rom8((uint16_t)(a + 1)) << 8));
}

/* glyph word by its ROM table address (0x3FCD-0x3FF2) */
static uint16_t glyph_at(uint16_t rom_addr)
{
    return omega_glyph_rom[(rom_addr - 0x3FCD) >> 1];
}

/* display-list write through the Z80 address: vector RAM, or the
 * display-list shadow (0x4481-0x4630) whose 0x1B0 bytes VG_RESTART
 * copies over 0x8000 every game frame - digit runs written there
 * survive the per-frame copy, exactly like the hardware. */
static void dl_wr8(uint16_t addr, uint8_t v)
{
    if (addr >= 0x8000 && addr < 0xA000)
        omega_vecram[(addr - 0x8000) & 0x1FFF] = v;
    else if (addr >= 0x4481 && addr < 0x4631)
        omega_dlist_shadow[addr - 0x4481] = v;
}

static void dl_wr16(uint16_t addr, uint16_t w)
{
    dl_wr8(addr, (uint8_t)w);
    dl_wr8((uint16_t)(addr + 1), (uint8_t)(w >> 8));
}

/* CHAR_GLYPH_EMIT 0x2C36: one ASCII char -> JSRL word at the cursor.
 * Letters via the 0x3A2E table (inside the page blob), everything else
 * via 0x3FCF/0x3FD3; <'/' falls back to '0', >'Z' clamps to 'Z'. */
static void char_emit(uint8_t ch, uint16_t* dst)
{
    uint16_t w;
    if (ch >= 0x41) {                            /* 'A'.. */
        if (ch >= 0x5B) ch = 0x5A;               /* clamp to 'Z' */
        w = rom16((uint16_t)(0x3A2E + (ch - 0x41) * 2));
    } else if (ch == 0x20) {                     /* space: idx 0x0B */
        w = glyph_at(0x3FD3 + 0x0B * 2);
    } else if (ch == 0x2E) {                     /* '.' */
        w = glyph_at(0x3FCF);
    } else if (ch == 0x2C) {                     /* ',' */
        w = glyph_at(0x3FD1);
    } else {
        if (ch < 0x2F) ch = 0x30;                /* fall back to '0' */
        w = glyph_at((uint16_t)(0x3FD3 + (ch - 0x2F) * 2));
    }
    dl_wr16(*dst, w);
    *dst = (uint16_t)(*dst + 2);
}

/* BCD_TO_DIGITS 0x240E: n BCD bytes MSB-first. The ROM walks the source
 * DOWN from the addressed (most significant) byte; resolve_ref returns
 * the C base (least significant) pointer, so iterate n-1..0. Leading
 * zeros emit the blank glyph (digit index 0x0A) until the first nonzero
 * digit; a value of zero still prints its final '0' (ROM 0x2437). */
static void bcd_digits(const uint8_t* base, int n, uint16_t* dst)
{
    int blank = 1, i;
    for (i = n - 1; i >= 0; i--) {
        uint8_t byte = base[i];
        int d = (byte >> 4) & 0x0F;
        if (d == 0 && blank) d = 0x0A; else blank = 0;
        dl_wr16(*dst, glyph_at((uint16_t)(0x3FD5 + d * 2)));
        *dst = (uint16_t)(*dst + 2);
        d = byte & 0x0F;
        if (d == 0 && blank) { if (i != 0) d = 0x0A; }
        else blank = 0;
        dl_wr16(*dst, glyph_at((uint16_t)(0x3FD5 + d * 2)));
        *dst = (uint16_t)(*dst + 2);
    }
}

/* escape-pointer source: ROM page region directly, else resolve_ref
 * (returns a pointer into g; NULL if unmapped - emits spaces/zeros) */
static const uint8_t* src_ptr(uint16_t addr, int bcd_field, int n)
{
    static const uint8_t zero[8] = { 0 };
    uint8_t* p; int is_bcd;
    if (addr >= PAGE_ROM_LO && addr < PAGE_ROM_HI) {
        /* char runs read forward from addr; BCD reads take the base
         * (addr - n + 1), matching resolve_ref's convention */
        uint16_t base = bcd_field ? (uint16_t)(addr - n + 1) : addr;
        return &omega_page_rom[base - PAGE_ROM_LO];
    }
    if (resolve_ref(addr, &p, &is_bcd)) return p;
    return zero;
}

/* record setup shared by DRAW_PAGE (first record) and the line advance
 * in PAGE_SCRIPT_STEP: copy the 8 header bytes to dest, position the
 * source and cursor, compute the line's glyph-byte count. */
static void record_open(uint16_t rec)
{
    uint16_t next = rom16(rec);
    uint16_t dest = rom16((uint16_t)(rec + 2));
    int i;
    g.page_rec_next = next;
    g.page_line_len = (uint8_t)(next - rec - 12);
    for (i = 0; i < 8; i++)
        dl_wr8((uint16_t)(dest + i), rom8((uint16_t)(rec + 4 + i)));
    g.page_script_ptr = (uint16_t)(rec + 12);
    g.vlist_cursor    = (uint16_t)(dest + 8);
    g.page_line_glyph = 0;
}

/* Whether a page is up. The reveal call sites (mainline attract tail,
 * frame.c game-over) gate on the page_script_pos/limit pair, which a
 * DVG page keeps as a shadow ("0/1 while revealing, equal when done").
 *
 * This is 0 only before the first draw_page - at boot, when page_id is
 * still 0 and the attract tail is already stepping. page_script_step_
 * no_page() covers exactly that window. It cannot advance anything
 * (pos and limit are both 0) but it DOES reload fire_cooldown, which
 * is the shared fire-rate gate, so it is kept verbatim rather than
 * short-circuited. */
static int s_dvg_active = 0;

static void page_script_step_no_page(void)
{
    if (g.fire_cooldown) return;                 /* ROM 0x2B73 */
    g.fire_cooldown = 0x0A;                      /* ROM 0x2B7A */
    if (g.page_script_pos < g.page_script_limit)
        g.page_script_pos++;
}

/* PAGE_SCRIPT_STEP 0x2B6D */
void page_script_step(void)
{
    uint16_t de, dst;
    uint8_t  a;

    if (!s_dvg_active) { page_script_step_no_page(); return; }
    if (g.fire_cooldown) return;                 /* 0x2B73 rate gate   */
    g.fire_cooldown = 0x0A;                      /* 10 ticks per glyph */
    g.page_line_glyph++;

    de  = g.page_script_ptr;
    dst = g.vlist_cursor;
    a   = rom8(de);
    if (a & 0x80) {                              /* escape             */
        int n = a & 0x3F;
        uint16_t ptr;
        g.page_line_glyph = (uint8_t)(g.page_line_glyph + 2);
        de++;
        ptr = (uint16_t)(rom8(de) | (rom8((uint16_t)(de + 1)) << 8));
        de = (uint16_t)(de + 2);
        if (a & 0x40) {                          /* run of n chars     */
            const uint8_t* s = src_ptr(ptr, 0, n);
            int i;
            for (i = 0; i < n; i++) char_emit(s[i], &dst);
        } else {                                 /* n-byte BCD field   */
            bcd_digits(src_ptr(ptr, 1, n), n, &dst);
        }
    } else {                                     /* plain char         */
        char_emit(a, &dst);
        de++;
    }
    g.page_script_ptr = de;
    g.vlist_cursor    = dst;

    if (g.page_line_glyph < g.page_line_len) return;

    /* line complete: the 25-tick end-of-line pause, then next record */
    g.fire_cooldown = 0x19;                      /* 0x2BD3             */
    g.page_line_cur++;
    if (g.page_line_total < g.page_line_cur) {   /* page done          */
        g.page_script_pos = g.page_script_limit; /* release the gate   */
        return;
    }
    record_open(g.page_rec_next);
}

/* DRAW_PAGE 0x2AF3. Every page the machine has - the whole 0x01-0x1C
 * table - is built as display-list words. Anything outside that range
 * was never a page: omega_page_table has exactly 0x1C entries, and the
 * ROM indexes it 1-based, so page 0 and page 0x1D+ are out of bounds
 * and are dropped here rather than read past the end of the table. */
void draw_page(uint8_t page_id)
{
    uint16_t pp, rec, p;
    int guard;

    if (page_id < 1 || page_id > 0x1C) {
        s_dvg_active = 0;
        return;
    }
    s_dvg_active = 1;

    g.page_line_cur   = 1;                       /* (iy+9)  0x4089 */
    g.page_line_glyph = 0;                       /* (iy+10) 0x408A */
    g.page_line_total = 0;                       /* (iy+11) 0x408B */

    pp = omega_page_table[page_id - 1];
    g.page_ptr = pp;
    rec = (uint16_t)(pp + 1);
    record_open(rec);

    /* count the records (0x2B45): walk the chain until a 0000 next */
    for (p = g.page_rec_next, guard = 64; guard--; ) {
        uint16_t q;
        g.page_line_total++;
        q = rom16(p);
        if (q == 0) break;
        p = q;
    }

    if (rom8(pp) & 0x80) {                       /* reveal page: per-  */
        g.page_script_pos   = 0;                 /* frame stepping;    */
        g.page_script_limit = 1;                 /* hold the gate      */
        return;
    }
    /* draw the whole page now (0x2B57 loop, cooldown forced to 0) */
    for (guard = 4096; guard-- && g.page_line_total >= g.page_line_cur; ) {
        g.fire_cooldown = 0;
        page_script_step();
    }
    g.fire_cooldown = 0;
    g.page_script_pos = g.page_script_limit = 0; /* gate released      */
}

/* ATTRACT_PAGE_LOAD 0x16F1 */
void attract_page_load(void)
{
    uint16_t fill = rom16(0x33D9);               /* 0xE180: JMPL 0x8300 */
    uint16_t a;
    int i;
    dvg_dlist_reset();
    for (a = 0x8000; a < 0x8302; a = (uint16_t)(a + 2))
        dl_wr16(a, fill);
    for (i = 0; i < 0x1A; i++)
        dl_wr8((uint16_t)(0x8300 + i), rom8((uint16_t)(0x33DB + i)));
}

/* ---- attract display-list patches (page words already positioned) ----- */

/* ATTRACT_INIT 0x0B57-0x0B7B: lives preview on page 4. Word 0xF000 (a
 * zero-length, zero-intensity SVEC - invisible) over the icon glyph
 * words: 0x80F0 (the "2" row icon) and the last N of 0x8130/32/34. */
void dvg_lives_preview(uint8_t big_blank, uint8_t small_blank_count)
{
    if (big_blank)              dl_wr16(0x80F0, 0xF000);
    if (small_blank_count >= 3) dl_wr16(0x8130, 0xF000);
    if (small_blank_count >= 2) dl_wr16(0x8132, 0xF000);
    if (small_blank_count >= 1) dl_wr16(0x8134, 0xF000);
}

/* ATTRACT_PRICING 0x10FE-0x1122: live digit words into page 0x14's
 * glyph slots. The "blank" case writes 0xB000 (HALT) at 0x8054, which
 * truncates the list there - the x2 pricing line simply is not shown
 * when credits-per-coin >= 2, exactly as on hardware. */
void dvg_pricing_digit(int slot, uint8_t value, int blank)
{
    static const uint16_t ADDR[4] = { 0x8024, 0x803A, 0x8054, 0x806A };
    if (slot < 0 || slot > 3) return;
    dl_wr16(ADDR[slot], blank ? (uint16_t)0xB000
                              : glyph_at((uint16_t)(0x3FD5
                                                    + (value & 0x0F) * 2)));
}

/* VG_KICK_TITLE 0x17EB-0x1806: title shimmer. Rewrites the 4th/5th
 * enemy-icon glyph words page 2 emitted at 0x8080/0x80A8 from the
 * anim tables 0x3F4D/0x3F55 keyed on tick bits 2-3 (16 Hz). */
/* ROM 0x3F4D (the same words as dvg.c's SHAPE_ANIM) */
static const uint16_t TITLE_ANIM[8] = {
    0xCA04, 0xCA2E, 0xCA51, 0xCA74,
    0xCA97, 0xCAAA, 0xCABE, 0xCAD2,
};

void dvg_title_shimmer(void)
{
    int idx = (g.tick_244hz & 0x0C) >> 2;
    dl_wr16(0x8080, TITLE_ANIM[idx]);
    dl_wr16(0x80A8, TITLE_ANIM[4 + idx]);
}

/* ---- game-screen HUD --------------------------------------------------
 * The walls blob's eight patched JSRLs at 0x8234 are the HUD hub - they
 * CALL every piece as a display-list subroutine (each ends in RTSL,
 * pages via their '<' = D000 glyph):
 *   C15F -> 0x82BE page 5 "SCORE"      C0CB -> 0x8196 score digits
 *   C171 -> 0x82E2 page 6 "LAST SCORE" C123 -> 0x8246 last-score digits
 *   C12C -> 0x8258 lives/bonus icons   C18D -> 0x831A page 9 "CREDIT"
 *   C0D5 -> 0x81AA credit digit        C19A -> 0x8334 pages 0B-0E
 * The main walk ends right after the patch (HALT at 0x8244). The word
 * at 0x8242 doubles as a switch: MAINLOOP/TURN_SWITCH overwrite it with
 * 0xF000 (blank) or restore a JSRL from the 0x3EE5 rows. */

/* GAME_DLIST_BUILD 0x1905-0x194E, the data moves (WALL_BUFFERS_BUILD
 * and the walls copy stay with the caller, matching the port's split;
 * the caller draws pages 5/6/9 afterwards like the ROM does). */
void dvg_game_hud_build(void)
{
    extern void dvg_walls_install(void);
    uint16_t cur;
    int i;
    /* 0x1905: status-line template ROM 0x30D9 -> shadow 0x4601 */
    for (i = 0; i < 24; i++)
        dl_wr16((uint16_t)(0x4601 + i * 2), omega_dlist_tail[i]);
    /* 0x1910: score digits -> shadow 0x4617; 0x191B: credit -> 0x462B */
    cur = 0x4617; bcd_digits(g.score_cur, 4, &cur);
    cur = 0x462B; bcd_digits(&g.credits_bcd, 1, &cur);
    /* 0x1926: shadow -> vector RAM (the per-frame copy repeats this) */
    for (i = 0; i < 0x1B0; i++) omega_vecram[i] = omega_dlist_shadow[i];
    /* 0x1931: the full 0x10E-byte wall blob -> 0x81B0 */
    dvg_walls_install();
    /* 0x1939: JSRL patch row 0 over the FFFF placeholders at 0x8234 */
    dvg_hud_patch_row(0);
    /* 0x1944: last-score digits (0x4042-45 snapshot) -> live 0x8246 */
    cur = 0x8246; bcd_digits(g.score_disp_shadow, 4, &cur);
}

/* PLAYER_TURN_SWAP 0x0D6B-0x0DB1: the incoming player's patch row and
 * a fresh icon-slot region (blanks undone). Player 1 restores the
 * walls blob's own icon template (ROM 0x31B1 = blob offset 0xA8);
 * player 2 gets the alternate template at ROM 0x3F69. */
void dvg_hud_player_swap(int player)
{
    extern const unsigned short omega_walls_words[135];
    int i;
    if (player == 1) {
        dvg_hud_patch_row(1);                     /* 0x0D71: 0x3EF5    */
        for (i = 0; i < 0x32; i++)                /* 0x0D7C: 0x31B1    */
            dl_wr16((uint16_t)(0x8258 + i * 2),
                    omega_walls_words[0x54 + i]);
    } else {
        dvg_hud_patch_row(2);                     /* 0x0DA3: 0x3F05    */
        for (i = 0; i < 0x64; i++)                /* 0x0DAE: 0x3F69    */
            dl_wr8((uint16_t)(0x8258 + i),
                   rom8((uint16_t)(0x3F69 + i)));
    }
}

/* VG_RESTART_FRAME 0x179A-0x17B4: in a 2-player game the word at
 * 0x81A6 blinks on tick bit 5 between 0xF000 and 0xCDC2 - the JSRL to
 * the marker art at 0x9B84 that flags whose score is live. Written to
 * the LIVE list after the per-frame shadow copy, like the ROM. */
void dvg_player_blink(void)
{
    if (!(g.game_flags & 0x40)) return;           /* bit6 = 2P game    */
    dl_wr16(0x81A6, (g.tick_244hz & 0x20) ? (uint16_t)0xCDC2
                                          : (uint16_t)0xF000);
}

/* the 0x3EE5 rows: 0 = game build, 1/2 = the player-swap orders loaded
 * at 0x0D71/0x0DA3 */
void dvg_hud_patch_row(int row)
{
    uint16_t src = (uint16_t)(0x3EE5 + (row & 3) * 0x10);
    int i;
    for (i = 0; i < 0x10; i++)
        dl_wr8((uint16_t)(0x8234 + i), rom8((uint16_t)(src + i)));
}

/* per-site digit updates, at the ROM's own call sites:
 *   score  -> shadow 0x4617: SCORE_ADD 0x2367, player swap 0x0DC3
 *   credit -> shadow 0x462B: VG_RESTART 0x1791 (credit-dirty flag),
 *             player swap 0x0DCE
 *   last   -> live 0x8246:   player swap 0x0DD9 */
void dvg_score_digits(void)
{
    uint16_t cur = 0x4617;
    bcd_digits(g.score_cur, 4, &cur);
}

void dvg_credit_digits(void)
{
    uint16_t cur = 0x462B;
    bcd_digits(&g.credits_bcd, 1, &cur);
}

void dvg_score2_digits(void)
{
    uint16_t cur = 0x8246;
    bcd_digits(g.score_disp_shadow, 4, &cur);
}

/* LIVES_ICON_DRAW 0x2CF9: blank word 0xF000 over the icon JSRLs.
 * Bonus slots first (0x82BA/0x82B0/0x82A6 for xlife bits 3/4/5): a
 * remaining life whose bit is set keeps its icon and consumes a count;
 * then the seven tally slots blank as a cascade - entry at "kept = n"
 * blanks slot n+1 onward (0x829C down to 0x8260, stride 10). */
void dvg_lives_icon_draw(void)
{
    static const uint16_t TALLY[7] = {
        0x829C, 0x8292, 0x8288, 0x827E, 0x8274, 0x826A, 0x8260
    };
    static const uint16_t BONUS[3] = { 0x82BA, 0x82B0, 0x82A6 };
    int a = (int)g.lives - 1, i;      /* 0x2CFF dec a: the ship in play */

    if (a < 0) a = 0;
    for (i = 0; i < 3; i++) {
        if (a != 0 && (g.xlife_flags & (uint8_t)(0x08 << i)))
            a--;                      /* icon kept, one life consumed  */
        else
            dl_wr16(BONUS[i], 0xF000);
    }
    /* tally cascade (0x2D2C dec chain): keep a, blank slots a+1..7;
     * with a >= 6 the ROM still lands on the 0x2D52 store, so at most
     * six tally icons ever show */
    if (a > 6) a = 6;
    for (i = a; i < 7; i++)
        dl_wr16(TALLY[i], 0xF000);
}

/* SCORE_ADD's extra-life award (0x2384-0x2394 and the two sibling
 * arms): write the mirrored-hull icon word 0xC000|((0xDFC+7)&0xFFF)
 * = 0xCE03 (JSRL SHIP_M00) into the bonus slot for the latch bit. */
void dvg_bonus_icon_award(int xlife_bit)     /* 3, 4 or 5 */
{
    static const uint16_t SLOT[3] = { 0x82BA, 0x82B0, 0x82A6 };
    if (xlife_bit < 3 || xlife_bit > 5) return;
    dl_wr16(SLOT[xlife_bit - 3], 0xCE03);
}

/* the 0x8242 switch word: TURN_SWITCH 0x0D38 restores the JSRL from
 * (0x3EF3) while the "get ready" banner shows; MAINLOOP 0x0FC6 blanks
 * it with 0xF000 when the wave-start timer expires. */
void dvg_banner_switch(int show)
{
    dl_wr16(0x8242, show ? rom16(0x3EF3) : (uint16_t)0xF000);
}

/* ---- wave clear ------------------------------------------------------- */

/* PLAY_FRAME 0x0CC7-0x0CCD: after the ordinal + bonus pages are drawn
 * into 0x8000-0x8091, two HALT words seal the list at 0x8092/0x8094 -
 * nothing beyond the bonus text is walked during the animation. The
 * shadow is untouched, so the first object-list build after the
 * animation restores the region (the hardware's VG_RESTART copy). */
void dvg_wave_announce_seal(void)
{
    dl_wr16(0x8092, 0xB000);
    dl_wr16(0x8094, 0xB000);
}

/* ---- mode 4 / hiscore entry ------------------------------------------- */

/* SHIP_RESPAWN 0x0EF7: the 0x7A-byte letter-wheel alphabet display
 * list, ROM 0x3A0E -> dlist shadow 0x4481. The mode-4 kick then copies
 * those 0x7A bytes shadow -> 0x8000 every frame, so the cursor and
 * typed-initials updates (which write the shadow) appear one frame
 * later, like the hardware. The blob has no HALT: it falls through
 * into page 0x11's header at 0x807A. */
void dvg_letterwheel_install(void)
{
    int i;
    for (i = 0; i < 0x7A; i++)
        omega_dlist_shadow[i] = rom8((uint16_t)(0x3A0E + i));
}

/* VG_KICK_TITLE_180D 0x1815-0x181E: the mode-4 per-frame copy. */
void dvg_mode4_kick_copy(void)
{
    int i;
    for (i = 0; i < 0x7A; i++)
        omega_vecram[i] = omega_dlist_shadow[i];
}

/* HISCORE_INITIALS 0x2A42-0x2A51: the wheel cursor. The bright SVEC at
 * blob +0x16 is static; what moves is the X word of the LABS before it
 * (shadow 0x4491/0x4492 = vram 0x8010/0x8011), set to
 * (letter_sel+1)*0x20 - LABS second-word format, scale nibble 0. */
void dvg_wheel_cursor(uint8_t letter_sel)
{
    dl_wr16(0x4491, (uint16_t)(((letter_sel & 0x1F) + 1) * 0x20));
}

/* hs_redraw_initials 0x2AD3-0x2AE0: the three typed-initials glyphs,
 * CHAR_GLYPH_EMIT from the hiscore-work chars into shadow 0x4489/8B/8D
 * (vram 0x8008-0x800C, over the blob's three space words). */
void dvg_initials_redraw(const uint8_t initials[3])
{
    uint16_t cur = 0x4489;
    int i;
    for (i = 0; i < 3; i++)
        char_emit(initials[i], &cur);
}

/* ==== POST / diagnostic screens ========================================
 * The five screens the ROM draws with the test switch held at boot. Each
 * is a list copied to vector RAM 0x8000 right after VG_WAIT_WIPE (0x07B6)
 * has filled the region with 0xB000 HALT words - which is why none of the
 * ROM blobs carries a terminator of its own, and why every builder here
 * calls dvg_dlist_reset() first.
 *
 * The two label screens share one trick worth stating plainly. Their
 * headers define three 4-glyph SUBROUTINES at word offsets 0x005 / 0x00A
 * / 0x00F, each ending in a shared newline vector, and DRAW_TEXT encodes
 * the '.' that ends every 9-character label cell as JSRL 0x00F - the
 * blank mark AND the line feed in one word. So posting a result is a
 * single BYTE write over the low half of that word: 0x05 turns it into
 * JSRL 0x005 ("  OK" / " LOW"), 0x0A into JSRL 0x00A (" NG " / " HI ").
 * That is exactly what POST_SHOW_BITS (0x08A0) does with `ld (de),a`,
 * stepping de by 0x12 - one 9-word cell - per row.
 */

#define POST_LABEL_BASE 0x802C           /* DRAW_TEXT dest after the hdr */
#define POST_MARK_0     0x803C           /* cell 0's '.' word (base+8*2) */
#define POST_CELL       0x12             /* 9 words per label cell       */

/* DRAW_TEXT 0x08C8: one JSRL word per character, written in sequence.
 * (The ROM does the store with `ld sp,de` + `push`, using the stack
 * pointer as a write cursor; the effect is a plain word write.)
 *   ' '      -> 0xCD92 literal (same word as the glyph table's blank)
 *   '.'      -> 0xC00F literal: JSRL to the screen's blank+newline sub
 *   'A'-'Z'  -> table 0x3A2E, clamped at 'Z'
 *   otherwise -> table 0x3FD3 indexed (c - 0x2F), clamped up to '0' */
static void post_text_emit(const uint8_t* src, int n, uint16_t dst)
{
    int i;
    for (i = 0; i < n; i++) {
        uint8_t c = src[i];
        uint16_t w;
        if (c == 0x20) {
            w = 0xCD92;
        } else if (c == 0x2E) {
            w = 0xC00F;
        } else if (c >= 0x41) {
            if (c > 0x5A) c = 0x5A;
            w = rom16((uint16_t)(0x3A2E + (c - 0x41) * 2));
        } else {
            if (c < 0x30) c = 0x30;
            w = glyph_at((uint16_t)(0x3FD3 + (c - 0x2F) * 2));
        }
        dl_wr16(dst, w);
        dst = (uint16_t)(dst + 2);
    }
}

/* copy one ROM blob to 0x8000 over a wiped list (the ROM's ldir) */
static void post_blob(const uint8_t* blob, int n)
{
    int i;
    dvg_dlist_reset();
    for (i = 0; i < n; i++)
        omega_vecram[i] = blob[i];
}

/* post_draw_header 0x04F5: header to 0x8000, then 0xA2 label chars. */
void dvg_post_tests_page(void)
{
    post_blob(omega_post_test_hdr, 0x2C);
    post_text_emit(omega_post_test_text, 0xA2, POST_LABEL_BASE);
}

/* post_input_draw 0x06B0: same, with 0x195 label chars. */
void dvg_post_inputs_page(void)
{
    post_blob(omega_post_input_hdr, 0x2C);
    post_text_emit(omega_post_input_text, 0x195, POST_LABEL_BASE);
}

/* POST_SHOW_BITS 0x08B2 `ld (de),a`: post one row's mark. op is the ROM's
 * own JSRL operand - 0x05 for the pass/LOW subroutine, 0x0A for NG/HI -
 * and only the low byte moves, so the 0xC0 JSRL opcode nibble stays put. */
void dvg_post_mark(int cell, uint8_t op)
{
    dl_wr8((uint16_t)(POST_MARK_0 + cell * POST_CELL), op);
}

/* post_xhatch_draw 0x073D and post_grid_draw 0x075F: raw list copies. */
void dvg_post_xhatch_page(void) { post_blob(omega_post_xhatch, 0x18); }
void dvg_post_grid_page(void)   { post_blob(omega_post_grid,   0xA0); }

/* post_sound_test 0x0781 draws nothing at all - it latches sound command
 * 0x16 and loops on VG_WAIT_WIPE, so the screen is a wiped list. */
void dvg_post_sound_page(void) { dvg_dlist_reset(); }
