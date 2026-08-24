#ifdef OMEGA_TRACE
#include <stdio.h>
#define OTRACE(s) fprintf(stderr, "[T] %s\n", s)
#else
#define OTRACE(s)
#endif
/* frame.c - Omega Race C conversion: per-frame game sequencer + wave
 * progression.
 *
 * Translates disasm/omega_main.asm:
 *   FRAME_ENGINE      0x12D2  per-frame ship input / death / game-over /
 *                             player-swap sequencer (fe_* labels)
 *   INPUTS_SAMPLE     0x14F4  port 0x11/0x12 edge detection
 *   P1_INPUT_READ     0x1711  cocktail seat-aware port 0x11 read
 *   SPINNER_READ      0x1737  cocktail seat-aware spinner read
 *   PLAY_BEGIN        0x0BD1  mode 3 setup
 *   PLAY_FRAME        0x0C98  in-play frame body, wave-clear bonus anim,
 *                             turn switch, wave advance/roster, demo
 *                             auto-destruct (through 0x0EE4)
 *   SHIP_RESPAWN      0x0EE6
 *   GAME_DLIST_BUILD  0x1902  game-screen setup -> draw calls
 *   PLAYER_STATE_SAVE 0x2C8E, PLAYER_STATE_LOAD 0x2CBD (13-byte areas)
 *   LIVES_ICON_DRAW   0x2CF9  (translated inline, called from
 *                             demo_autodestruct like the ROM does)
 *
 * See disasm/FUNCTIONS.md and disasm/GAMEPLAY_NOTES.md ("FRAME_ENGINE -
 * the per-frame game sequencer", "PLAY_BEGIN", "PLAY_FRAME (in-play frame
 * body)") for the walkthroughs this file follows.
 *
 * Control-flow style: FRAME_ENGINE and PLAY_FRAME are each one big ROM
 * routine stitched together with many small internal jr/jp labels (fe_*,
 * WAVE_*, etc). Per enemies.c's precedent for enemy_fire_gate, this file
 * keeps that as goto-label-per-ROM-address inside one C function each,
 * rather than restructuring into many tiny call/return functions whose
 * semantics don't match the ROM's fall-through/branch shape.
 *
 * Blocking-loop transformation: WAVE_CLEAR_ANIM (0x0CD4) busy-waits for
 * ~5 real seconds inside a single PLAY_FRAME call. That's turned into a
 * per-frame state machine below (wave_clear_anim_step()); play_frame()
 * runs one step per real frame and returns early while it's active.
 * See the comment on wave_clear_anim_step() for how the ROM's timing
 * idiom (reusing tick_prev as a scratch "elapsed ticks" register while
 * MAINLOOP wasn't running) is replaced with a dedicated countdown.
 *
 * RAM cells this file needs live directly in the canonical `g`
 * (omega_state.h): game_flags, start_credits, cur_player_num, svc_flags
 * (bit7 = "game started"), timer4023, counter_406D, cocktail_seat_ref,
 * in2_seat_raw, fire_cooldown, score_final_bcd, score_disp_shadow,
 * frame_tail_flags, page_script_limit, page_script_pos, wave_anim_active,
 * wave_anim_ticks.
 */
#include "omega_state.h"
#include "omega_shapes.h"

/* ---- cross-module externs ------------------------------------------------ */

/* objects.c */
extern void obj_spawn(uint8_t obj_num);        /* OBJ_SPAWN 0x291D */
extern void obj_kill(uint8_t obj_num);         /* OBJ_KILL  0x29B2 */
extern void sound_cmd_send(uint8_t cmd);       /* SOUND_CMD_SEND 0x2E04 */

/* enemies.c */
extern void    enemy_alive_scan(void);         /* 0x2467 */
extern void    difficulty_calc(void);          /* 0x2186 */
extern uint8_t random_next(uint8_t entropy_arg); /* 0x2906 */

/* score.c */
extern void score_add(uint8_t kill_tier);      /* SCORE_ADD 0x2335 */
extern void audit_snapshot(void);              /* AUDIT_SNAPSHOT 0x1540 (irq_coins.c really; ROM calls it from here) */
extern void hiscore_check(void);               /* HISCORE_CHECK 0x29CD */
extern void hiscore_wheel_draw(void);          /* SHIP_RESPAWN's 0x3A0E blob (score.c) */
extern void hiscore_select_working_table(int use_alternate); /* PLAY_BEGIN's 42-byte
        0x43D3/0x43FD -> hiscore_work(0x43A9) copy; delegated to score.c since it
        owns the hiscore machinery/NVRAM tables (rule: don't invent parallel state
        for the two battery-backed tables this needs) */

/* pages.c - draw_page(page_id) is declared in omega_state.h */
extern void page_script_step(void);            /* PAGE_SCRIPT_STEP 0x2B6D */

/* mainline.c */
extern void    attract_init(void);             /* ATTRACT_INIT 0x0A9D */
extern void    game_start(void);                /* GAME_START 0x0B83 */
extern int     start_btn_check(void);           /* START_BTN_CHECK 0x11AA; nonzero == start accepted (Z80 nz).
                                                   Returns int, matching mainline.c's definition - the linker
                                                   matches cross-TU declarations on name alone, so the types
                                                   must agree. */
extern void    start_leds_off(void);            /* START_LEDS_OFF_1255 */
extern void    hiscore_entry_frame_resume(void); /* HISCORE_ENTRY_FRAME_12C8 continuation - mode-4 owner */

/* "posx" is screen X, "posy" is screen Y (rule 7 / see enemies.c's
 * identical macros and dvg.c's LABS builder). */
#define SCREEN_X(o) ((uint8_t)((o).posx >> 8))
#define SCREEN_Y(o) ((uint8_t)((o).posy >> 8))

/* HUD placement, taken from the machine (frametime --dump-vram during
 * play) instead of invented.
 *
 * The layout is a JSRL chain at the end of the game list, 0x8234:
 *   C15F C0CB   "SCORE"      sub-list, then the digit run at 0x8196
 *   C171 C123   "LAST SCORE" sub-list, then the digit run at 0x8246
 *   C12C        the ten lives/bonus icon slots at 0x8258
 *   C18D C0D5   "CREDIT"     sub-list, then the digit run at 0x81AA
 * Each label sub-list is a LABS, its glyphs, and - for the two score
 * runs - a second LABS that parks the beam where the digits start. The
 * digits themselves carry no position of their own; they draw from
 * wherever the label left the beam. Read off the live list:
 *
 *   SCORE       label LABS (y=540, x=528) scale 0
 *               digits  LABS (y=500, x=352) scale 1   <- 0x82D8
 *   LAST SCORE  label LABS (y=440, x=448) scale 0
 *               digits  LABS (y=400, x=352) scale 1   <- 0x830A
 *   CREDIT      label LABS (y=400, x=220) scale 0, no trailing LABS,
 *               so the digits follow the label's own advance: six
 *               glyphs and two spaces at 16 units each = x 348, y 400.
 *
 * All of it sits inside the centre box (x 193-832, y 384-640), which is
 * where this game keeps its status display. The scale nibbles are the
 * ROM's: both score runs really are double-size, the credit count
 * really is not.
 *
 * The constants themselves are in omega_state.h: score.c redraws the
 * same slots on a score change and must not keep a second copy.
 * The lives/bonus icons are separate - see lives_icon_draw, which has
 * the ROM's ten fixed slot positions. */

/* ---- forward declarations ------------------------------------------------ */

void frame_engine(void);
void inputs_sample(void);
void play_begin(uint8_t is_demo);
void play_frame(void);
void ship_respawn(void);
void game_dlist_build(void);
void player_state_save(int slot);
void player_state_load(int slot);

extern void nvram_save_all(void);              /* NVRAM_SAVE_ALL 0x0A68 (score.c) */

/* dvg_pages.c: the game HUD as real display-list words */
extern void dvg_game_hud_build(void);
extern void dvg_lives_icon_draw(void);
extern void dvg_score_digits(void);
extern void dvg_credit_digits(void);
extern void dvg_score2_digits(void);
extern void dvg_hud_player_swap(int player);
extern void dvg_banner_switch(int show);
extern void dvg_wave_announce_seal(void);
extern void dvg_letterwheel_install(void);

static uint8_t p1_input_read(void);
static uint8_t spinner_read(void);
static void    lives_icon_draw(void);
static int     wave_clear_anim_step(void);
static uint8_t bcd_sub1(uint8_t a, uint8_t b);

/* ============================================================================
 * ROM 0x3061-0x30A0: spinner gray-code decode, 64 entries.
 * ==========================================================================*/
static const uint8_t SPINNER_GRAY_DECODE[64] = {
    0x00,0x01,0x03,0x02,0x07,0x06,0x04,0x05, 0x0F,0x0E,0x0C,0x0D,0x08,0x09,0x0B,0x0A,
    0x1F,0x1E,0x1C,0x1D,0x18,0x19,0x1B,0x1A, 0x10,0x11,0x13,0x12,0x17,0x16,0x14,0x15,
    0x3F,0x3E,0x3C,0x3D,0x38,0x39,0x3B,0x3A, 0x30,0x31,0x33,0x32,0x37,0x36,0x34,0x35,
    0x20,0x21,0x23,0x22,0x27,0x26,0x24,0x25, 0x2F,0x2E,0x2C,0x2D,0x28,0x29,0x2B,0x2A
};

/* ============================================================================
 * bcd_sub1: single-byte BCD subtract (Z80 SUB + DAA equivalent). Not
 * provided by omega_state.h's bcd_* helper set (add/cmp only); needed
 * here for play_begin()'s credit deduction. Operand ranges in this file
 * are small (credits 0x00-0x20 BCD capped per CONVENTIONS rule 6, minus
 * 1 or 2 players), so a straightforward per-digit borrow is exact.
 * ==========================================================================*/
static uint8_t bcd_sub1(uint8_t a, uint8_t b) {
    int lo = (a & 0x0F) - (b & 0x0F);
    int hi = (a >> 4) - (b >> 4);
    if (lo < 0) { lo += 10; hi--; }
    if (hi < 0) hi += 10;
    return (uint8_t)(((hi & 0x0F) << 4) | (lo & 0x0F));
}

/* ============================================================================
 * P1_INPUT_READ (0x1711): cocktail seat-aware raw read of port 0x11.
 * ==========================================================================*/
static uint8_t p1_input_read(void) {
    if (g.game_flags & 0x04) {                       /* cocktail cabinet */
        uint8_t sel = (uint8_t)((g.cur_player_num ^ g.cocktail_seat_ref) & 0x01);
        if (sel) {
            uint8_t p2 = omega_hw_in_p2();
            /* ROM: (p2&3) rrca x3 == rotate the low 2 bits up into bits 6-5 */
            g.in2_seat_raw = (uint8_t)((p2 & 0x03) << 5);
            return (uint8_t)((omega_hw_in_p1() & 0x9F) | g.in2_seat_raw);
        }
    }
    return omega_hw_in_p1();
}

/* ============================================================================
 * SPINNER_READ (0x1737): cocktail seat-aware spinner read.
 * ==========================================================================*/
static uint8_t spinner_read(void) {
    if (g.game_flags & 0x04) {
        uint8_t sel = (uint8_t)((g.cur_player_num ^ g.cocktail_seat_ref) & 0x01);
        if (sel) return omega_hw_spinner2();           /* port 0x16, no shift */
    }
    {
        uint8_t raw = omega_hw_spinner();               /* port 0x15 */
        return (uint8_t)((raw >> 2) | (raw << 6));       /* rrca x2 */
    }
}

/* ============================================================================
 * INPUTS_SAMPLE (0x14F4): ports 11/12 raw + edge detection.
 * ==========================================================================*/
void inputs_sample(void) {
    uint8_t prev1 = g.in1_state;
    g.in1_state = p1_input_read();
    g.in1_pressed  = (uint8_t)((prev1 & g.in1_state) ^ prev1);  /* falling edge (active-low buttons) */
    g.in1_released = (uint8_t)((g.in1_state | prev1) ^ prev1);  /* rising edge  */

    {
        uint8_t prev2 = g.in2_state;
        g.in2_state = omega_hw_in_p2();
        g.in2_pressed = (uint8_t)((prev2 & g.in2_state) ^ prev2);
    }
}

/* ============================================================================
 * PLAYER_STATE_SAVE (0x2C8E) / PLAYER_STATE_LOAD (0x2CBD): the 13-byte
 * per-player save areas. slot 0 = P1 (ROM 0x4046), slot 1 = P2 (0x4053).
 * ==========================================================================*/
void player_state_save(int slot) {
    uint8_t* p = g.p_save[slot];
    p[0] = g.wave_pace_timer;
    p[1] = g.lives;
    p[2] = g.wave_num;
    p[3] = g.xlife_flags;
    p[4] = g.wave_ctr;
    if (g.game_flags & 0x80) {                          /* real game in progress */
        int i;
        for (i = 0; i < 4; i++) p[5 + i] = g.score_cur[i];
        for (i = 0; i < 4; i++) p[9 + i] = g.time_played[i];
    }
}

void player_state_load(int slot) {
    const uint8_t* p = g.p_save[slot];
    int i;
    g.wave_pace_timer = p[0];
    g.lives           = p[1];
    g.wave_num        = p[2];
    g.xlife_flags     = p[3];
    g.wave_ctr        = p[4];
    for (i = 0; i < 4; i++) g.score_cur[i]   = p[5 + i];
    for (i = 0; i < 4; i++) g.time_played[i] = p[9 + i];

    /* second ROM ldir: refresh the "other player" score-line shadow from
     * whichever score_final_bcd slot is NOT the player about to play. */
    {
        int other = (g.cur_player_num == 1) ? 1 : 0;
        for (i = 0; i < 4; i++) g.score_disp_shadow[i] = g.score_final_bcd[other][i];
    }
}

/* ============================================================================
 * LIVES_ICON_DRAW (0x2CF9): 3 "big" bonus-life slots gated one-for-one
 * by xlife_flags award-latch bits 3/4/5, then up to 7 more as a plain
 * tally of any lives beyond that.
 *
 * The ten icon slots are fixed positions in the ROM's game-screen list
 * (0x31B1 onwards, landing at vector RAM 0x8258-0x82BA). Every one of
 * them is a `LABS scale=0` followed by a JSRL, so they draw at the same
 * size as the ship in play. The three bonus-life slots
 * (0x82A6/0x82B0/0x82BA, the ones score.c's award code writes into)
 * use SHIP_15; the seven tally slots (0x8258-0x8294) use SHIP_00.
 *
 * Slot order follows the xlife_flags bit tested: bit 3 is award icon 3
 * at 0x82BA, bit 4 icon 2 at 0x82B0, bit 5 icon 1 at 0x82A6. */
static const float BONUS_ICON_XY[3][2] = {
    { 320.0f, 460.0f },   /* 0x82BA, xlife_flags bit 3 */
    { 320.0f, 500.0f },   /* 0x82B0, bit 4             */
    { 320.0f, 540.0f }    /* 0x82A6, bit 5             */
};
static const float LIVES_ICON_XY[7][2] = {
    { 270.0f, 480.0f }, { 270.0f, 520.0f }, { 270.0f, 560.0f },
    { 220.0f, 440.0f }, { 220.0f, 480.0f }, { 220.0f, 520.0f },
    { 220.0f, 560.0f }
};

static void lives_icon_draw(void) {
    /* LIVES_ICON_DRAW 0x2CF9: 0xF000 blanks over the icon JSRL slots
     * inside the walls blob (dvg_pages.c). The shapes/positions come
     * from the blob itself, not from host coordinates. */
    dvg_lives_icon_draw();
}

/* ============================================================================
 * GAME_DLIST_BUILD (0x1902): game-screen setup, translated to draw calls.
 * ==========================================================================*/
void game_dlist_build(void) {
    /* The wall dlist (ROM 0x3109) is copied into the display list at
     * 0x81B0, exactly where the hardware puts it, so that
     * WALL_FLASH_DECAY's per-segment intensity writes (0x81BB stride 6)
     * land on real bytes and the DVG walker draws the result. The
     * vestigial ROM-destination LDIR (writes to 0x9000, ignored by
     * hardware) is dropped. */

    /* ROM 0x1902's first act is WALL_BUFFERS_BUILD (0x18BC) - it rebuilds
     * the ship's rotated frame sets in vector RAM, which DLIST_RESET
     * wipes (it clears through 0x8BF7, the end of the 0x87F0 buffer). */
    dvg_ship_buffers_build();

    /* 0x1905-0x194E: template + digit runs into the shadow, shadow ->
     * vector RAM, full walls blob -> 0x81B0, the eight-JSRL HUD patch
     * over 0x8234, last-score digits at 0x8246 - all real list words.
     * The ROM does NOT redraw the lives icons here: a fresh walls copy
     * shows the template icons and SHIP_RESPAWN's LIVES_ICON_DRAW call
     * blanks the right ones before the screen matters. */
    dvg_game_hud_build();

    g.page_id = 5; draw_page(g.page_id);   /* score labels */
    g.page_id = 6; draw_page(g.page_id);
    g.page_id = 9; draw_page(g.page_id);
}

/* ============================================================================
 * PLAY_BEGIN (0x0BD1): mode 3 setup.
 * ==========================================================================*/
void play_begin(uint8_t is_demo) {
    /* ROM: the attract demo enters at 0x0C1E (lives setup), skipping the
     * whole PLAY_BEGIN prologue - so game_mode stays 2 for the demo and
     * no credits/scores/hiscore-bank state is touched. */
    if (!is_demo) {
        g.game_mode = 3;
        g.game_flags |= 0x80;                          /* "game in progress" */

        if (!(omega_hw_dsw_c6() & 0x40)) {              /* DSW C6 bit6 = free play */
            g.credits_bcd = bcd_sub1(g.credits_bcd, g.start_credits);
        }

        g.svc_flags |= 0x80;
        {
            int i;
            for (i = 0; i < 4; i++) g.score_disp_shadow[i] = g.score_cur[i];
            for (i = 0; i < 4; i++) { g.score_cur[i] = 0; g.time_played[i] = 0; }
        }
        hiscore_select_working_table(g.game_flags & 0x20);
    }

    {
        /* ROM 0x2FD3, 8 bytes indexed directly by the decoded DSW value
         * (0-3, or 0-4 in demo mode); the "pairs" grouping in the data
         * dump is just cosmetic. */
        static const uint8_t LIVES_TABLE[8] = { 2,4, 2,5, 3,6, 3,7 };
        uint8_t idx = (uint8_t)((omega_hw_dsw_c4() & 0x30) >> 4);
        if (g.game_flags & 0x20) idx++;                 /* deluxe start: bonus ship */
        g.lives = LIVES_TABLE[idx];
    }

    g.wave_num = 6;
    g.counter_406D = 0;
    g.seconds_ctr = 0;
    g.timer_seconds = 0;
    g.wave_pace_timer = 0x19;
    g.xlife_flags = 0;
    g.wave_flags = 0;
    g.wave_ctr = 0;
    g.last_sound_cmd = 1;

    player_state_save(0);
    player_state_save(1);
    omega_display_reset();  /* DLIST_RESET */
    game_dlist_build();     /* installs the wall list into vector RAM */

    if (g.game_flags & 0x40) {                          /* 2-player: extra cocktail pages */
        g.page_id = 7;    draw_page(g.page_id);
        g.page_id = 8;    draw_page(g.page_id);
        g.page_id = 0x0A; draw_page(g.page_id);
    }

    /* ROM 0x0A68 NVRAM_SAVE_ALL, not a raw host flush: the full save
     * re-stages the credit shadow (CREDITS_NVRAM_SYNC at 0x0A86) before
     * writing - a raw flush would persist a stale shadow when a banked
     * credit is spent, since only the coin-in path resyncs it via the
     * svc bit6 flag. */
    nvram_save_all();

    {
        int i;
        for (i = 1; i <= 32; i++) { obj_spawn((uint8_t)i); obj_kill((uint8_t)i); }
    }
    g.cocktail_seat_ref++;

    play_frame();
}

/* ============================================================================
 * WAVE_CLEAR_ANIM (0x0CD4): ~5 real seconds of "tick time_played, count
 * down timer_seconds, blip a sound at 2s left", then award 2x
 * SCORE_ADD(5) as the wave-clear bonus. The ROM busy-waits for each
 * second inside one PLAY_FRAME call by spinning on tick_244hz wrapping
 * past a scratch copy of itself (borrowed from tick_prev, 0x4018 - safe
 * on hardware only because MAINLOOP isn't running during the spin).
 *
 * Converted to a per-frame state machine: play_frame() calls this once
 * per real frame while g.wave_anim_active is set. Each call consumes
 * this frame's g.tick_delta from a 256-tick countdown (256 ticks at
 * 244Hz standing in for the ROM's 8-bit tick_244hz wraparound, i.e. the
 * same ~1.024s granularity the ROM actually used, not a rounded-off 1.0s
 * literal). Returns 1 while still waiting (caller should return this
 * frame without running the rest of PLAY_FRAME), 0 once the whole
 * animation (all 5 "seconds" plus the final bonus) is finished.
 * ==========================================================================*/
static int wave_clear_anim_step(void) {
    /* The countdown must be SIGNED and tested "> 0": tick_delta is 4 or
     * 5 and varies, so an unsigned counter can overshoot zero without
     * ever landing on it exactly, walking the residues mod 256 forever.
     * Signed countdown: fire once per 256 accumulated ticks whatever the
     * per-frame delta, which is the ROM's own unit at 0x0CD9-0x0CE3 - it
     * spins until the free-running 244 Hz counter wraps past its previous
     * value, i.e. one full 256-tick lap, ~1.024 s. */
    g.wave_anim_ticks = (int16_t)(g.wave_anim_ticks - (int16_t)g.tick_delta);
    if (g.wave_anim_ticks > 0) return 1;
    g.wave_anim_ticks = (int16_t)(g.wave_anim_ticks + 256);

    {
        static const uint8_t ONE_BCD[4] = { 0x01, 0x00, 0x00, 0x00 }; /* ROM 0x3FF7, "+1" */
        bcd_add(g.time_played, ONE_BCD, 4);
    }
    g.timer_seconds--;
    if (g.timer_seconds == 0) {
        int i;
        for (i = 0; i < 2; i++) score_add(5);
        g.wave_anim_active = 0;
        return 0;
    }
    if (g.timer_seconds == 2) {
        g.timer_seconds = 0;
        sound_cmd_send(0x14);
        g.timer_seconds = 2;
        OTRACE("frame.c:390 wave-announce timer=2");
    }
    return 1;
}

/* ============================================================================
 * SHIP_RESPAWN (0x0EE6).
 * ==========================================================================*/
void ship_respawn(void) {
    sound_cmd_send(0);
    obj_spawn(1);
    g.shooter_state = 2;
    omega_display_reset();  /* DLIST_RESET */
    /* ROM 0x0EF7-0x0F00: the 0x7A-byte alphabet dlist ROM 0x3A0E ->
     * shadow 0x4481 - the letter wheel, cursor LABS/SVEC and initials
     * slots as real list words. */
    dvg_letterwheel_install();
    g.initials_cnt = 0;     /* (iy+24) = 0x4098: reused here as a respawn-path
                                scratch byte, distinct from its mode-4 hiscore-
                                initials-cursor role */
    g.seconds_ctr = 0;
    g.page_id = 0x11;
    draw_page(g.page_id);

    if (!(g.game_flags & 0x40)) return;
    start_leds_off();
    if (g.cur_player_num & 0x01) {          /* player_turn==1 */
        g.page_id = 0x12;
        draw_page(g.page_id);
        return;
    }
    g.page_id = 0x13;                     /* player_turn==2 */
    draw_page(g.page_id);
}

/* ============================================================================
 * PLAY_FRAME (0x0C98) through DEMO_AUTODESTRUCT (0x0EE4): in-play frame
 * body. One C function with goto labels named after the ROM's internal
 * labels (WAVE_CLEAR_ANIM excepted - see wave_clear_anim_step() above).
 * ==========================================================================*/
void play_frame(void) {
    /* drop: VG_WAIT_DONE (hardware vector-generator sync, rule 5) */

    if (g.wave_anim_active) {
        if (wave_clear_anim_step()) return;
        goto turn_switch_check;
    }

    if (g.wave_flags & 0x40) {                    /* wave complete */
        g.wave_ctr++;
        if ((g.wave_ctr & 0x03) == 0) {            /* only every 4th clear announces (verbatim ROM quirk) */
            int page = g.wave_ctr >> 2;
            if (page >= 6) page = 6;
            g.page_id = (uint8_t)(0x16 + page);
            draw_page(g.page_id);
            g.page_id = 0x16;
            draw_page(g.page_id);
            /* 0x0CC7-0x0CCD: two HALT words at 0x8092/0x8094 seal the
             * list right after the bonus text - objects, walls and HUD
             * are simply not walked during the animation. */
            dvg_wave_announce_seal();
            g.timer_seconds = 5;
            OTRACE("frame.c:446 turn/gameover timer=5");
            g.wave_anim_active = 1;
            g.wave_anim_ticks = 256;   /* one full 244 Hz lap, the ROM's
                                          wait unit at 0x0CD9-0x0CE3 */
            if (wave_clear_anim_step()) return;
            goto turn_switch_check;
        }
    }

turn_switch_check:
    if (!(g.game_flags & 0x40)) goto frame_tail;             /* single player: no turn-switch logic */
    if ((g.obj[OBJ_SHIP].flags0 & 0x80) && (g.obj[OBJ_SHIP].flags0 & 0x10)) goto frame_tail;
    if (g.counter_406D == 0) goto player_turn_swap;
    g.counter_406D--;
    /* drop: ($8242) = ($3EF3) dlist word write (rule 5) */
    dvg_banner_switch(1);   /* 0x0D38: restore the 0x8242 JSRL so the
                               banner sub-list is reached again */
    g.page_id = 0x0B;
    draw_page(g.page_id);
    g.timer_seconds = 0;
    sound_cmd_send(0x14);
    goto frame_tail;

player_turn_swap:
    if (g.cur_player_num == 1) goto player_turn_swap_0d8a;

    player_state_save(1);
    g.cur_player_num = 1;
    player_state_load(0);
    start_leds_off();
    dvg_hud_player_swap(1);  /* 0x0D6B-0x0D7F: patch row 1 + the walls
                                blob's own icon template -> 0x8258 */
    g.page_id = 0x0C;
    draw_page(g.page_id);
    goto turn_swap_tail;

player_turn_swap_0d8a:
    player_state_save(0);
    g.cur_player_num = 2;
    player_state_load(1);
    start_leds_off();
    dvg_hud_player_swap(2);  /* 0x0D9D-0x0DB1: patch row 2 + the P2
                                icon template (ROM 0x3F69) -> 0x8258 */
    g.page_id = 0x0D;
    draw_page(g.page_id);
    /* falls into turn_swap_tail */

turn_swap_tail:
    g.wave_flags = 0;                              /* whole-byte reset, PLAYER_TURN_SWAP_0DBA */
    /* 0x0DC0-0x0DDE: refresh all three digit runs for the new player */
    dvg_score_digits();
    dvg_credit_digits();
    dvg_score2_digits();

frame_tail:
    {
        int i;
        g.wall_fx[0] = 0x80;
        for (i = 1; i <= 8; i++) g.wall_fx[i] = 1;
    }
    g.frame_tail_flags[0] = 1;
    g.frame_tail_flags[1] = 1;
    g.frame_tail_flags[2] = 1;
    g.frame_tail_flags[3] = 1;
    if (g.timer4023 == 0) g.timer4023 = 2;
    g.timer_seconds = g.timer4023;
    g.last_sound_cmd = 1;
    g.sound_ready = 0;
    difficulty_calc();
    /* Refreshes g.rng_cache (0x4079) with a fresh PRNG value every frame;
     * OBJ_SPAWN (objects.c) later reads bit7 of this cache as a placement
     * coin-flip - see omega_state.h's rng_cache comment. */
    g.rng_cache = random_next(g.difficulty_param);

wave_advance:
    if (g.wave_flags & 0x40) {
        g.wave_num = (uint8_t)(g.wave_num + 2);
        if (g.wave_num >= 0x0A) g.xlife_flags |= 0x80;   /* hard-mode bit */
        if (g.wave_num >= 0x0C) g.wave_num = 0x0C;        /* clamp */
    }

wave_roster:
    {
        int killcount = (g.wave_num == 6) ? 32 : 21;
        int i;
        for (i = killcount; i >= 1; i--) obj_kill((uint8_t)i);
        for (i = g.wave_num + 1; i >= 1; i--) obj_spawn((uint8_t)i);
        if (g.wave_num == 6) {
            /* ROM 0x0E5D-0x0E89: first wave ONLY - `res 6,(ix+0)` on all
             * six droid records (0x40C9/40E0/40F7/410E/4125/413C). That
             * is flags0 bit 6, wall-collide, and OBJ_PHYSICS (0x1BB5)
             * skips position integration without it: the droids sit
             * parked at their spawn points, thrust ramping their stored
             * velocity to terminal while the position never moves, until
             * shooter designation grants bit 6 back (0x1C07/0x1C12) and
             * releases one at a time. Wave 2+ skips the clears, so the
             * whole roster patrols - which is why droids stand still on
             * stage 1 and move from stage 2 (verified on hardware:
             * parked droid flags0=BB vs patrolling FB). */
            for (i = 2; i <= 7; i++) g.obj[i].flags0 &= (uint8_t)~0x40;
        }
    }

wave_complete_det:
    g.shooter_a = 0;
    g.shooter_b = 0;
    g.wave_flags = 0;                 /* whole-byte reset */
    g.shooter_state = 2;
    enemy_alive_scan();

demo_autodestruct:
    if (g.game_mode == 2) {
        g.obj[OBJ_SHIP].flags1 |= 0x80;
        g.obj[OBJ_SHIP].flags0 |= 0x01;
        g.obj[OBJ_SHIP].b18 = 0x0E;
        g.obj[OBJ_SHIP].b14 = 0x14;
    }
    lives_icon_draw();
    /* drop: ($8334) = 0xD000 dlist blank of the P2-score slot for
     * single-player games (rule 5) */
    if (g.wave_num < 0x0C) g.wave_pace_timer = 0x19;
    g.difficulty_param = (uint8_t)((g.wave_ctr < 7) ? 0xFA : 0x4B);
    return;   /* ROM: jr MAINLOOP_SYNC; the tick_244hz/tick_prev reset there
                 belongs to mainline.c's MAINLOOP */
}

/* ============================================================================
 * FRAME_ENGINE (0x12D2): the per-frame game sequencer.
 * ==========================================================================*/
/* fe_next_player (0x147B): the hiscore gate every out-of-lives path
 * funnels through - check the (already loaded) player's score against
 * the table; a qualifying score enters mode 4 and the letter wheel,
 * otherwise fall out toward attract with step 9 staged (so the score
 * page that 12C8 enters is followed by ATTRACT_PRICING, step 10). Bit 4
 * of game_flags ("an initials entry was committed this game over") is
 * what routes a non-qualifying player to the score pages instead of a
 * full ATTRACT_INIT. */
static void fe_next_player(void) {
    hiscore_check();
    if (g.game_mode == 4) { ship_respawn(); return; }
    g.attract_step = 9;                          /* 0x1486 ld (iy+3),$09 */
    if (g.game_flags & 0x10) { hiscore_entry_frame_resume(); return; }
    attract_init();
}

/* fe_respawn_tick_1478 (0x1478): load the selected player's state and
 * fall into fe_next_player. Reached two ways, exactly like the ROM:
 * by falling out of the 2P higher-score-first compare above, and by
 * HISCORE_ENTRY_FRAME_12C5's jump - mainline.c's hiscore_entry_finish
 * calls it to hand the letter wheel to a 2P game's other player. */
void fe_respawn_tick_1478(void) {
    player_state_load(g.cur_player_num - 1);     /* 0x1478 PLAYER_STATE_LOAD */
    fe_next_player();
}

void frame_engine(void) {
    /* ---- ship steering: spinner -> ship_angle via the gray-code table ---- */
    {
        uint8_t idx = (uint8_t)((spinner_read() ^ 0x3F) & 0x3F);
        uint8_t decoded = SPINNER_GRAY_DECODE[idx];
        uint8_t new_angle = (uint8_t)(decoded ^ 0x3F);
        if (g.game_mode >= 3) {
            if (g.game_mode == 4 || !(g.obj[OBJ_SHIP].flags1 & 0x20)) {  /* mode 4, or mode 3 + not dying */
                g.obj[OBJ_SHIP].angle = new_angle;  /* ROM 0x12F6: (ship_angle) IS the ship record's +10 */
            }
        }
    }

fe_thrust_input:
    if (!(g.in1_state & 0x20)) {                       /* thrust held (active-low) */
        if (g.obj[OBJ_SHIP].flags0 & 0x20) {            /* ROM 0x12FF. flags0 bit5 is a template constant
                                                            (ship tmpl 0x2F50 byte0=0xF8) with this gate as
                                                            its ONLY reader in the ROM; death clears only
                                                            bit7, so it blocks thrust just between reset
                                                            and the first attract spawn */
            g.obj[OBJ_SHIP].flags0 |= 0x01;              /* thrust-active */
        }
    }

fe_fire_request:
    if (g.in1_pressed & 0x40) {                         /* fire button just pressed (edge) */
        if (g.fire_cooldown == 0 && !(g.obj[OBJ_SHIP].flags1 & 0x20)) {
            g.obj[OBJ_SHIP].flags0 |= 0x04;              /* fire requested */
            g.fire_cooldown = 2;                         /* 2-tick cooldown */
        }
    }

fe_death_check:
    if (g.obj[OBJ_SHIP].flags0 & 0x80) {                /* "ship destroyed" per GAMEPLAY_NOTES
                                                            (inverted from the generic obj_rec
                                                            active-bit polarity) */
        if (g.shooter_state != 0) goto fe_tail_sounds;
        if (!(g.obj[OBJ_SHIP].flags1 & 0x20)) goto fe_death_start;
    }

fe_death_check_1336:
    if (g.wave_flags & 0x20) goto fe_state_gate;
    g.wave_flags |= 0x20;
    g.timer_seconds = 0x0A;
    g.timer4023 = 4;
    OTRACE("fe 1336 one-shot timer=10/4023=4");
    goto fe_state_gate;

fe_death_start:
    if (g.wave_flags & 0x20) goto fe_state_gate;
    g.wave_flags |= 0x20;
    /* ROM 0x1354-0x1370 tries to deactivate four shot records via literal
     * ROM addresses 0x000E-0x0011 ("ld ix,$000E; res 7,(ix+0)" etc) -
     * that's a no-op on hardware (ROM is read-only), an apparent bug.
     * Doing nothing here matches the actual, buggy hardware result of
     * NOT deactivating those shots - this comment stands in for the
     * dropped dead code. */
    g.sound_ready = 0;
    sound_cmd_send(0);
    g.timer_seconds = 0;
    sound_cmd_send(4);
    g.timer_seconds = 5;
    g.timer4023 = 5;
    OTRACE("fe death_start timer=5/4023=5");
    g.tick_244hz = 0;
    g.tick_prev = 0;
    /* falls into fe_state_gate */

fe_state_gate:
    if (g.timer4023 >= 3) goto fe_tail_sounds;
    /* Faithful ROM condition (0x139E-0x13A8). Attract containment is
     * handled upstream: fe_death_check's shooter_state short-circuit
     * (0x1329) - after the single attract PLAY_FRAME pass runs the
     * roster+census, shooter_state=2 keeps this path unreachable
     * (verified against frametime.exe --attract-trace). */
    if (!(g.obj[OBJ_SHIP].flags1 & 0x20)) {
        if (g.obj[OBJ_SHIP].flags0 & 0x80) {
            play_frame();
            return;
        }
    }
    /* falls into fe_gameover_check */

fe_gameover_check:
    if (g.obj[OBJ_SHIP].flags0 & 0x80) goto fe_tail_sounds;
    if (g.lives != 1) goto fe_respawn_tick;

    if (g.svc_flags & 0x80) {
        if (!(g.xlife_flags & 0x02)) {
            g.xlife_flags |= 0x02;
            audit_snapshot();
        }
    }

fe_gameover_check_13cc:
    if (!(g.game_flags & 0x40)) goto fe_join_check;
    if (g.cur_player_num != 2) goto fe_respawn_tick;

fe_join_check:
    if (start_btn_check()) { game_start(); return; }
    if (!(g.game_flags & 0x02)) {
        int i;
        g.game_flags |= 0x02;
        for (i = 32; i >= 1; i--) obj_kill((uint8_t)i);
        g.wave_pace_timer = 5;
        sound_cmd_send(0);
        /* drop: 0x1A-byte dlist blank at 0x8336, ($8334)=0xD000 (rule 5) */
        g.page_id = 0x0E;
        draw_page(g.page_id);
    }

fe_spawn_gate:
    if (g.page_script_pos < g.page_script_limit) {
        page_script_step();
        goto fe_tail_sounds;
    }

fe_spawn_gate_1424:
    if (g.wave_pace_timer != 0) goto fe_tail_sounds;
    /* falls into fe_respawn_tick */

fe_respawn_tick:
    sound_cmd_send(0);
    if (--g.lives != 0) {
        /* QUIRK: reuses g.lives itself as a transient respawn-pause
         * countdown (verbatim "dec (iy-26)" in the ROM, where -26 resolves
         * to the exact same RAM cell as `lives`). This looks alarming out
         * of context, but it's harmless: player_state_load() (a few lines
         * below, reached once this countdown hits 0) unconditionally
         * restores the real lives value from the p_save area, so the
         * transient decrements here never stick. */
        play_frame();
        return;
    }
    g.wave_flags &= (uint8_t)~0x40;
    if (!(g.svc_flags & 0x80)) { attract_init(); return; }
    if (!(g.game_flags & 0x40)) { fe_next_player(); return; }
    if (g.cur_player_num != 2) { play_frame(); return; }

    player_state_save(1);
    g.cur_player_num = 1;
    {
        int cmp = bcd_cmp(g.score_final_bcd[0], g.score_final_bcd[1], 4);
        if (cmp < 0) {                      /* P1 < P2: P2 plays next */
            g.cur_player_num = 2;
            g.game_flags |= 0x08;
        }
    }
    /* falls into fe_respawn_tick_1478 */
    fe_respawn_tick_1478();
    return;

fe_tail_sounds:
    if (g.obj[OBJ_SHIP].flags1 & 0x20) goto fe_halfplane_toggle;   /* dying: no thrust-sound edges */
    if (!(g.in1_pressed & 0x20)) goto fe_tail_sounds_14a7;
    sound_cmd_send(9);                     /* thrust on */
    goto fe_halfplane_toggle;

fe_tail_sounds_14a7:
    if (!(g.in1_released & 0x20)) goto fe_halfplane_toggle;
    sound_cmd_send(0x0A);                  /* thrust off */

fe_halfplane_toggle:
    if (!(g.wave_flags & 0x10)) {
        uint8_t sx = SCREEN_X(g.obj[OBJ_SHIP]);
        if (sx < 0xD0 && sx >= 0x30) {
            g.wave_flags |= 0x10;
            {
                uint8_t sy = SCREEN_Y(g.obj[OBJ_SHIP]);
                if (sy >= 0x80) {
                    int half = g.wave_num / 2;
                    int start_obj = half + 2;
                    int i;
                    for (i = 0; i < half; i++) {
                        int idx = start_obj + i;
                        if (idx >= 1 && idx <= OBJ_COUNT) g.obj[idx].flags0 ^= 0x02;
                    }
                }
            }
        }
    }
    /* ROM: jp MAINLOOP */
}
