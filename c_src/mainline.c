/* mainline.c - Omega Race C conversion: boot, mode machine, main-loop
 * skeleton, attract cycle, operator menu.
 *
 * Translates disasm/omega_main.asm:
 *   GAME_INIT            0x0955   ATTRACT_INIT        0x0A9D
 *   GAME_START            0x0B83   MAINLOOP_SYNC/MAINLOOP 0x0F30/0x0F39
 *   ATTRACT_STEP_DISP     0x1030   ATTRACT_PRICING     0x10DC
 *   OPERATOR_MENU         0x1137   COINAGE_DISP_PREP/BCD_DIV4 0x1646/0x1674
 *   START_BTN_CHECK       0x11AA   START_LEDS_OFF(+126C/1255) 0x1248/126C/1255
 *   HISCORE_ENTRY_FRAME   0x127B (thin wrapper only)
 *   VG_KICK_TITLE         0x17D5 (title-shimmer selection logic only)
 *
 * See disasm/FUNCTIONS.md and disasm/MAINLINE_NOTES.md. IY=0x4080 is
 * fixed throughout the ROM for this code; every (iy+d) below is
 * annotated with the RAM cell it resolves to.
 *
 * MODULE BOUNDARIES (CONVENTIONS.md module map):
 *   - PLAY_BEGIN and everything it falls through into
 *     (play_score_setup, play_lives_setup, PLAY_FRAME, WAVE_CLEAR_ANIM,
 *     TURN_SWITCH_CHECK, PLAYER_TURN_SWAP, FRAME_TAIL, WAVE_ADVANCE,
 *     WAVE_ROSTER, WAVE_COMPLETE_DET, DEMO_AUTODESTRUCT, SHIP_RESPAWN,
 *     FRAME_ENGINE, INPUTS_SAMPLE) belongs to frame.c. The ROM dives
 *     from GAME_START straight into that chain and never returns to
 *     GAME_START - it only comes back out via a jump to MAINLOOP_SYNC.
 *     Represented here by the extern play_begin(); see its declaration.
 *   - NVRAM byte-level validate/wipe/save and hiscore-table content
 *     belong to score.c.
 *   - DRAW_PAGE/PAGE_SCRIPT_STEP/ATTRACT_PAGE_LOAD belong to pages.c.
 *
 * STRUCTURAL TRANSFORMATION (rule 4): the ROM's MAINLOOP is an
 * unbounded loop that only ever "returns" by falling back into itself;
 * ATTRACT_INIT/GAME_START/OPERATOR_MENU/etc. all end with a `jp` back
 * into that loop (directly or via MAINLOOP_SYNC). Here every such tail
 * jump is translated as "call the target function, then return to the
 * host" rather than as a literal recursive dive back into
 * omega_mainloop_pass() - the host is expected to call
 * omega_mainloop_pass() again on its own next frame. This is what
 * makes one call to any entry point here do exactly one frame's worth
 * of work, matching rule 4 ("frame pacing itself is the host's job").
 */
#include <string.h>
#include <stdint.h>
#include "omega_state.h"
#include "omega_shapes.h"   /* SHAPE_IDX_* */

/* ---- cross-module externs ---------------------------------------------- */

/* objects.c */
extern void obj_spawn(uint8_t obj_num);        /* OBJ_SPAWN 0x291D */
extern void objects_update(void);              /* OBJECTS_UPDATE 0x1965 */
extern void wall_flash_decay(void);            /* WALL_FLASH_DECAY 0x1868 */

/* frame.c - see module-boundary note above. is_demo selects whether the
 * PLAY_BEGIN prologue (mode=3, deduct credits, clear/snapshot scores,
 * pick hiscore bank) runs before the shared play_lives_setup/roster/
 * DEMO_AUTODESTRUCT tail; GAME_START's own demo branch skips that
 * prologue and dives straight into the shared tail (ROM 0x0BCF
 * `jr play_lives_setup`), which is why game_mode stays 2 (never
 * upgraded to 3) for the whole self-play demo. Ends by calling
 * omega_mainloop_sync(), then returns to the host (see structural-
 * transformation note above) - mirrors the ROM's `jr MAINLOOP_SYNC`. */
extern void play_begin(uint8_t is_demo);
extern void frame_engine(void);                 /* FRAME_ENGINE 0x12D2 */
extern void inputs_sample(void);                /* INPUTS_SAMPLE 0x14F4 */

/* enemies.c */
extern void heartbeat_sound(uint8_t code);      /* HEARTBEAT_SOUND 0x2DF6 */

/* pages.c */
extern void draw_page(uint8_t page_id);         /* DRAW_PAGE 0x2AF3 */
extern void page_script_step(void);             /* PAGE_SCRIPT_STEP 0x2B6D */
extern void dvg_title_shimmer(void);            /* dvg_pages.c: VG_KICK_TITLE words */
extern void dvg_pricing_digit(int slot, uint8_t value, int blank);
extern void dvg_lives_preview(uint8_t big_blank, uint8_t small_blank_count);
extern void dvg_banner_switch(int show);        /* the 0x8242 hub word */
extern void dvg_credit_digits(void);            /* VG_RESTART 0x1791   */
extern void dvg_mode4_kick_copy(void);          /* 0x180D shadow copy  */
extern void attract_page_load(void);            /* ATTRACT_PAGE_LOAD 0x16F1:
                                                  * loads the big attract
                                                  * dlist fragment (ROM
                                                  * 0x33D9) for pages 1/3;
                                                  * pure DVG list build, no
                                                  * gameplay state - dropped
                                                  * per rule 5, kept as a
                                                  * pages.c hook */

/* score.c: NVRAM validate/restore + hiscore/bookkeeping persistence.
 * DESIGN DEVIATION (documented): on hardware, NVRAM_WIPE (0x0A3B) falls
 * straight through into SCORES_RESET (0x0A4C) which falls through into
 * NVRAM_SAVE_ALL (0x0A68) and then `jp ATTRACT_INIT`. The mode-machine
 * transition (jp ATTRACT_INIT) is deliberately NOT reproduced inside
 * these score.c externs here - mainline.c (the mode-machine owner)
 * calls attract_init() itself right after invoking them, at every call
 * site. nvram_wipe()'s internal cascade into "reset scores + save all"
 * IS preserved (that part is pure NVRAM/score data, score.c's remit). */
extern int  nvram_validate_all(void);   /* GAME_INIT 0x09B2-0x0A15 bundle:
                                          * template match at 0x5C1A vs ROM
                                          * 0x30C9, hiscore nibble read at
                                          * 0x5C3A (initials range check),
                                          * bookkeeping nibbles at 0x5C58,
                                          * credits at 0x5C56; nonzero on
                                          * success */
extern void nvram_wipe(void);           /* NVRAM_WIPE 0x0A3B; cascades into
                                          * scores_reset()+nvram_save_all()
                                          * internally, see deviation note */
extern void scores_reset(void);         /* SCORES_RESET 0x0A4C; cascades
                                          * into nvram_save_all() internally */
extern void nvram_save_all(void);       /* NVRAM_SAVE_ALL 0x0A68 */
extern int  credits_nvram_sync(void);   /* CREDITS_NVRAM_SYNC 0x16E3;
                                          * nonzero if the shadow changed */
extern void player_state_save(int slot); /* PLAYER_STATE_SAVE 0x2C8E:
                                          * slot 0 = P1 (ROM 0x4046),
                                          * slot 1 = P2 (0x4053). Signature
                                          * must match frame.c's int-slot
                                          * definition: separate translation
                                          * units, so C diagnoses nothing and
                                          * the linker matches on name alone. */
extern void hiscore_initials(void);     /* HISCORE_INITIALS 0x2A35 */
extern void hiscore_context_sync(int ctx); /* score.c: working table -> bank */
extern void fe_respawn_tick_1478(void); /* frame.c: PLAYER_STATE_LOAD +
                                          * fe_next_player, the target of
                                          * HISCORE_ENTRY_FRAME_12C5 */
extern void sound_cmd_send(uint8_t cmd); /* SOUND_CMD_SEND 0x2E04 */
extern const uint8_t DEFAULT_HISCORES[6][7]; /* ROM 0x2FF7 default table,
                                          * transcribed by score.c */

/* host hooks:
 *   omega_frame_present()  replaces VG_RESTART_FRAME's DVG-kick body.
 *     The shadow-list copy and hardware VG kick are pure hardware,
 *     dropped (rule 5). VG_RESTART_FRAME's other two jobs are done:
 *     the credits digit refresh is dvg_credit_digits() and the wall
 *     glow fade is WALL_FLASH_DECAY over g.wall_fx.
 *   omega_hw_leds_out()      out($13): LED/coin-counter/flip register.
 *
 * The attract draws are real display-list writes in dvg_pages.c, called
 * from this file: dvg_lives_preview (0x80F0, 0x8130/32/34),
 * dvg_pricing_digit (0x8024/3A/54/6A), dvg_title_shimmer
 * (0x8080/0x80A8).
 */
/* host hooks: declared in omega_state.h */

/* This file's omega_state.h fields (led_shadow, coin_lines,
 * hiscore_bank, svc_flags, game_flags, player_sel, cur_player_num,
 * start_credits, page_script_*, slam_latch, fire_cooldown, timer4023,
 * cocktail_seat_ref, opmenu_disp*) are documented at their
 * declarations. player_sel (0x403F) is the count of players still owed
 * a mode-4 initials entry: 1 from ATTRACT_INIT, 2 from a 2P start,
 * decremented per commit by HISCORE_ENTRY_FRAME 0x12AA and routing the
 * letter-wheel handoff. start_credits (0x4040) is the credit price of
 * the pending start (1/2/4), BCD-debited once at PLAY_BEGIN 0x0BE3. */

/* ---- ROM data tables -----------------------------------------------------
 * All four DIP tables transcribed verbatim from the typed data section
 * at disasm/omega_main.asm (lines ~6282-6296).
 */

/* ROM 0x2FC3: coinage, 8 (coins_per_credit, credits_awarded) pairs,
 * indexed by 3-bit DSW C6 coin-mode (bits0-2, rotated left 1 -> *2, so
 * table index = mode value; g.coin_mode1/2 already carry that *2). */
static const uint8_t COINAGE_TABLE[8][2] = {
    {1,2}, {1,3}, {1,5}, {4,5}, {3,4}, {2,3}, {2,1}, {1,1}
};

/* ROM 0x2FD3: lives per DIP, 4 (lives, icon_blank_base) pairs, indexed
 * by DSW C4 bits4-5 (>>4). */
static const uint8_t LIVES_TABLE[4][2] = {
    {2,4}, {2,5}, {3,6}, {3,7}
};

/* ROM 0x2FE5: bonus-ship (extra-life) DIP thresholds, indexed by DSW C4
 * bits0-1 (word index). */
static const uint16_t BONUS_DIP_TABLE[4] = { 0x0004, 0x0005, 0x0007, 0x0010 };

/* ROM 0x2FED: difficulty DIP params, indexed by DSW C4 bits2-3 (word
 * index); ATTRACT_INIT reads TWO consecutive words starting at this
 * index for xlife_thr[1]/xlife_thr[2]. */
static const uint16_t DIFFICULTY_DIP_TABLE[5] = {
    0x0015, 0x0025, 0x0050, 0x0075, 0x0150
};

/* ROM 0x3F4D/0x3F55: title-shimmer 4-entry JSRL word tables, decoded
 * (rom_addr = ((word & 0x3FFF) << 1) | 0x8000) and mapped to vector-ROM
 * shape names via disasm/vec_names.py. Table B decodes to all four
 * DEATH_SHIP_* variants (00 included). dvg_title_shimmer() in
 * dvg_pages.c writes the ROM's own 0x3F4D/0x3F55 words into the live
 * display list at 0x8080/0x80A8, so no shape-index copy is needed
 * here. */

/* ---- forward declarations ------------------------------------------------
 * Needed because several of these call each other out of definition
 * order (e.g. omega_mainloop_pass() calls start_btn_check(),
 * operator_menu(), hiscore_entry_frame(), attract_step_disp(), all
 * defined later in the file); C99 has no implicit function
 * declaration.
 */
void game_init(void);
void attract_init(void);
void game_start(void);
void omega_mainloop_sync(void);
void omega_mainloop_pass(void);
void omega_mainloop_tick(void);   /* host: after each 244 Hz tick     */
void omega_mainloop_frame(void);  /* host: once per displayed frame   */
int  start_btn_check(void);
void start_leds_off(void);
void start_leds_off_126c(void);
void start_leds_off_1255(void);
void attract_step_disp(void);
void attract_pricing(void);
void operator_menu(void);
void hiscore_entry_frame(void);
extern int  post_frame(void);        /* post.c: one frame of the self test */
void vg_kick_title(void);

static void att_step5_scores(void);
static void att_step6_scores2(void);
static void attract_frame_tail(void);
static void opmenu_redraw(void);
static void bcd_div4(uint8_t dest4[4], const uint8_t dividend4[4], uint8_t divisor_bcd);
static void coinage_disp_prep(const uint8_t count_flag[2], uint8_t dest8[8],
                               const uint8_t src8[8]);

/* ============================================================================
 * GAME_INIT (0x0955): cold-start entry. Clears state, decodes the DSW C6
 * coinage tables, copies the ROM default hiscore table into the working
 * copy + both battery banks, then validates NVRAM and either restores
 * from it or wipes/reinitializes. Always ends in ATTRACT_INIT.
 *
 * DROPPED (rule 6, hardware artifacts): sp=0x4BFC / iy=0x4080 stack and
 * index-register setup, WALL_BUFFERS_BUILD's vector-ROM prebake call
 * (0x18BC - the transformed variant is pre-baked in the ROM image per
 * FUNCTIONS.md's "known quirks" note), the raw 0x4000-0x4481 memory
 * clear (the C `g` struct is expected to start zeroed by the host).
 * ==========================================================================*/
void game_init(void) {
    uint8_t dsw_c6 = omega_hw_dsw_c6();

    g.led_shadow = 0xFF;                          /* (iy-14) 0x4072, new field */
    g.coin_lines = (uint8_t)(omega_hw_in_p1() & 0x03); /* 0x4035, new field */

    /* DSW C6 coinage decode -> coin_mode1/2 (existing g fields) and
     * coinage1_ptr/coinage2_ptr (new fields: pointers into
     * COINAGE_TABLE, replacing the ROM's 0x4027/0x4029 pointer cells).
     * NOTE: on hardware, 0x4029/0x402A (coinage2_ptr) physically alias
     * death_timer_a/death_timer_b (omega_state.h) because coinage2_ptr
     * is written once here and never touched again outside GAME_INIT,
     * while the death timers are only live during actual gameplay
     * (mode 3) - a temporal RAM-reuse quirk like the documented
     * player_cur/obj_class alias. Not reproduced as literal address
     * aliasing in C; modelled as its own field instead. */
    g.coin_mode1 = (uint8_t)((dsw_c6 & 0x07) << 1);
    g.coinage1_ptr = &COINAGE_TABLE[g.coin_mode1 >> 1][0];
    g.coin_mode2 = (uint8_t)(((dsw_c6 & 0x38) >> 3) << 1);
    g.coinage2_ptr = &COINAGE_TABLE[g.coin_mode2 >> 1][0];

    /* default hiscore table -> working copy + both battery banks (ROM
     * 0x2FF7 -> 0x43A9 working; 0x43A9 -> 0x43D3 dup covers BOTH
     * banks in one 0x54-byte copy since 0x43D3/0x43FD are contiguous). */
    memcpy(g.hiscore, DEFAULT_HISCORES, sizeof g.hiscore);
    memcpy(g.hiscore_bank[0], DEFAULT_HISCORES, sizeof g.hiscore_bank[0]);
    memcpy(g.hiscore_bank[1], DEFAULT_HISCORES, sizeof g.hiscore_bank[1]);

    if (!nvram_validate_all()) {
        nvram_wipe(); /* cascades into scores_reset()+nvram_save_all() */
    }
    /* both the validate-success and validate-fail ROM paths converge
     * on `jp ATTRACT_INIT` (0x0A15 / via NVRAM_WIPE's fallthrough). */
    attract_init();
}

/* ============================================================================
 * ATTRACT_INIT (0x0A9D): mode 1 entry/re-entry. Resets per-game timers,
 * honors free play (DSW C6 bit6 -> credits forced to 4), and if credits
 * already exist, decodes DSW C4 bonus/lives/difficulty and previews the
 * starting lives-icon display; otherwise plays a quiet sound. Spawns
 * the title-page ship object and resyncs the tick baseline.
 * ==========================================================================*/
void attract_init(void) {
    start_leds_off();
    start_leds_off_126c();

    g.svc_flags = 0;           /* (iy-82) 0x402E, new field */
    g.game_mode = 1;           /* (iy-92) 0x4024 */

    /* if game_flags bit7 (PLAY_BEGIN has run) and NOT bit6 (2-player),
     * i.e. a single-player game was in progress: snapshot player 1's
     * state before wiping it. game_flags is a new field (0x4041); bit
     * meanings transcribed literally from the reads/writes at this
     * address. */
    if ((g.game_flags & 0x80) && !(g.game_flags & 0x40)) {
        player_state_save(0);               /* HL=0x4046, i.e. P1 */
    }

    if (omega_hw_dsw_c6() & 0x40) {
        g.credits_bcd = 0x04; /* free play */
    }

    g.game_flags &= 0x40; /* keep only the 2-player bit across reinit */

    g.player_sel = 1;           /* (iy-65) 0x403F, new field */
    g.cur_player_num = 1;       /* (iy-66) 0x403E, ROM 0x0AD8. Distinct
                                  * cell from 0x407E obj_class - see
                                  * omega_state.h */
    g.page_id = 0;               /* (iy+4) 0x4084 */
    /* 0x0AE0/0x0AE4: (iy+11)=0x408B page_line_total <- 0 and
     * (iy+9)=0x4089 page_line_cur <- 1. total < cur is the RELEASED
     * state of the 0x1027 step gate below, so the very first attract
     * pass dispatches the title page - the boot one-shot's 5 s hold
     * (fe_death_start, armed later that same pass) then counts down
     * BEHIND the visible title, exactly as on hardware. */
    g.page_line_total = 0;       /* (iy+11) 0x408B */
    g.page_line_cur = 1;         /* (iy+9)  0x4089 */
    g.page_script_pos = 0;       /* no page armed: the reveal scheduler */
    g.page_script_limit = 0;     /* pair idles until DRAW_PAGE arms it  */
    g.attract_step = 1;          /* (iy+3) 0x4083 */
    g.seconds_ctr = 0;           /* (iy-96) 0x4020 */
    g.timer_seconds = 0;         /* (iy-95) 0x4021 */
    g.wave_num = 4;               /* (iy-25) 0x4067 */
    g.slam_latch = 0;            /* (iy+2) 0x4082, new field */
    g.fire_cooldown = 0;         /* (iy-16) 0x4070, new field */

    omega_display_reset();
    /* DLIST_RESET (0x1823): DVG display-list clear. No persistent
     * display list in the immediate-mode renderer - dropped (rule 5). */

    if (g.credits_bcd != 0) {
        g.timer_seconds = 0x3C;

        {
            uint8_t idx = (uint8_t)((omega_hw_dsw_c4() & 0x03) << 1);
            g.xlife_thr[0] = BONUS_DIP_TABLE[idx >> 1];
        }
        {
            uint8_t idx = (uint8_t)((omega_hw_dsw_c4() & 0x0C) >> 2);
            g.xlife_thr[1] = DIFFICULTY_DIP_TABLE[idx];
            g.xlife_thr[2] = DIFFICULTY_DIP_TABLE[idx + 1];
        }

        g.page_id = 4;
        draw_page(g.page_id);

        /* lives-icon preview: DSW C4 bits4-5 -> LIVES_TABLE index.
         * lives_b = starting lives, c = 8 - table[1] selects how many
         * of the 3 "small" icon slots get blanked (0..3), and the
         * "big" icon slot is blanked iff lives_b==2. Preserved as the
         * exact ROM cascade-decrement quirk: c==1 blanks 0 slots,
         * c==2 blanks 1 (last only), c==3 blanks 2 (last two), c>=4
         * blanks all 3. */
        {
            uint8_t idx = (uint8_t)((omega_hw_dsw_c4() & 0x30) >> 4);
            uint8_t lives_b = LIVES_TABLE[idx][0];
            uint8_t c = (uint8_t)(8 - LIVES_TABLE[idx][1]);
            uint8_t big_blank = (uint8_t)(lives_b == 2);
            uint8_t small_blank_count;

            if (--c == 0)      small_blank_count = 0;
            else if (--c == 0) small_blank_count = 1;
            else if (--c == 0) small_blank_count = 2;
            else                small_blank_count = 3;

            dvg_lives_preview(big_blank, small_blank_count);
        }
    } else {
        sound_cmd_send(0);
    }

    obj_spawn(1); /* spawn the title-page ship object */

    /* ROM: `jp MAINLOOP_SYNC`, which falls straight through into
     * MAINLOOP. Per the structural-transformation note at the top of
     * this file, only the one-time tick resync is reproduced here;
     * the per-frame body is left for the host's next
     * omega_mainloop_pass() call rather than dived into recursively. */
    omega_mainloop_sync();
}

/* ============================================================================
 * GAME_START (0x0B83): mode 2 entry from START_BTN_CHECK, the attract
 * step-7 demo timer, or FRAME_ENGINE's join-in check. Detects the
 * self-play demo case (attract_step==7, or - reached from any other
 * caller - credits_bcd==0) and, only for a real credited start, hands
 * off to PLAY_BEGIN (frame.c, via play_begin(0)); the demo path skips
 * PLAY_BEGIN's credit-deduct/score-reset/mode-3-upgrade prologue
 * entirely and dives into the shared play_lives_setup tail with
 * play_begin(1), leaving game_mode at 2 for the whole demo.
 * ==========================================================================*/
void game_start(void) {
    start_leds_off();
    /* VG_WAIT_DONE (0x1854): DVG idle-wait, dropped (rule 5/6). */
    start_leds_off_126c();

    g.game_mode = 2;   /* (iy-92) */
    g.svc_flags = 0;   /* (iy-82) */

    if ((g.game_flags & 0x80) && !(g.game_flags & 0x40)) {
        player_state_save(0);               /* HL=0x4046, i.e. P1 */
    }

    if (g.attract_step == 7 || g.credits_bcd == 0) {
        /* demo detection: attract_step==7 is the ROM's own demo-timer
         * entry; credits_bcd==0 covers every other caller (join-in
         * check, etc.) landing here with no credits. */
        g.game_flags |= 0x20; /* bit5: demo flag */
        g.cur_player_num = 1;

        if (g.game_flags & 0x40) { /* 2-player flag already set */
            g.cur_player_num = 2;
            if (omega_hw_dsw_c6() & 0x80) {
                /* Literal ROM polarity - bit7==1 sets the cocktail
                 * flag. MAINLINE_NOTES.md's prose says "DSW C6 bit7 =
                 * upright, forces no flip", the OPPOSITE of what
                 * 0x0BC5-0x0BCF and START_BTN_CHECK's 0x1210-0x1219
                 * actually test-and-branch on. The disassembly bytes
                 * are the source of truth here. */
                g.game_flags |= 0x04; /* bit2: cocktail-in-progress */
            }
        }
        play_begin(1);
    } else {
        play_begin(0);
    }
}

/* ============================================================================
 * MAINLOOP_SYNC (0x0F30): zero the tick free-runner and its "previous"
 * shadow so the next omega_mainloop_pass() sees tick_delta==0. Called
 * whenever a blocking sequence (ATTRACT_INIT, PLAY_BEGIN's chain) is
 * about to hand control back to the host at a fresh tick baseline.
 * ==========================================================================*/
void omega_mainloop_sync(void) {
    g.tick_244hz = 0;
    g.tick_prev = 0;
}

/* ============================================================================
 * MAINLOOP (0x0F39): runs once per host frame regardless of mode. See
 * the structural-transformation note at the top of this file - this is
 * omega_mainloop_pass(), called once per frame by the host, which owns
 * frame pacing (rule 4); tick_244hz is expected to have been advanced
 * by the 244 Hz tick source between calls, exactly as the ROM's
 * IRQ_244HZ did independently of MAINLOOP's own call rate.
 * ==========================================================================*/
/* ---- host pass scheduling: machine time --------------------------------
 * The machine's MAINLOOP is never frame-locked - it free-runs, measured
 * on the real ROMs by sampling its own tick_delta cell (0x4019) per
 * 244 Hz slice (frametime --passrate-trace): during the title reveal
 * delta is 0 for 81% / 1 for 19% of passes; in play it is 0/1/2 for
 * 21/58/19%. Passes where no tick elapsed are observationally inert in
 * the object-free arm - every consumer there (fire_cooldown reloads,
 * step timers, the seconds wrap, input edges) is tick- or edge-gated -
 * so ONE pass per elapsed tick reproduces the machine's behavior at the
 * machine's own timing resolution, with nothing invented.
 *
 * The two arms differ, so the host schedules them differently:
 *  - omega_mainloop_tick(): modes 1/4 and POST (the arm that skips
 *    VG_RESTART_FRAME/OBJECTS_UPDATE, ROM 0x0FDA bit1 gate). Called
 *    once after every 244 Hz tick.
 *  - omega_mainloop_frame(): modes 2/3. Called once per displayed
 *    frame - a deliberate transformation for the object arm, kept
 *    because it is what every play-mode hardware verification
 *    (display-list byte diffs, --special-trace cell-for-cell, pacing)
 *    was measured against. Hardware free-runs here too; reconciling the
 *    exact per-pass object topology (obj_index staging vs the port's
 *    whole-roster objects_update) against those measurements is its own
 *    piece of work. */
void omega_mainloop_tick(void)
{
    if (g.post_active || !(g.game_mode & 0x02)) omega_mainloop_pass();
}

void omega_mainloop_frame(void)
{
    if (!g.post_active && (g.game_mode & 0x02)) omega_mainloop_pass();
}

void omega_mainloop_pass(void) {
    uint8_t tick_now;

    /* POST owns the screen while the test switch was held at boot (ROM
     * 0x04E8-0x0940, before GAME_INIT and outside this mode machine).
     * post_frame() returns 0 once it has handed off to the game. */
    if (g.post_active) {
        g.tick_delta = (uint8_t)(g.tick_244hz - g.tick_prev);
        g.tick_prev  = g.tick_244hz;
        if (post_frame()) return;
    }

    tick_now = g.tick_244hz;
    uint8_t old_prev = g.tick_prev;
    uint8_t delta = (uint8_t)(tick_now - old_prev);
    int wrapped = (tick_now < old_prev); /* SUB borrow == carry set on Z80 */

    g.tick_delta = delta;
    g.tick_prev = tick_now;

    if (wrapped) { /* once-per-~second block (0x4017/0x4018 8-bit wrap) */
        g.seconds_ctr++; /* (iy-96) 0x4020 */

        {
            /* time_played BCD += the ROM's "+1" 4-byte BCD constant
             * (ROM 0x3FF7), via the shared bcd_add() helper. */
            static const uint8_t PLUS_ONE_BCD[4] = { 0x01, 0x00, 0x00, 0x00 };
            bcd_add(g.time_played, PLUS_ONE_BCD, 4);
        }

        if (g.timer_seconds != 0) {   /* (iy-95) 0x4021 */
            g.timer_seconds--;
            if (g.timer_seconds == 0) {
                uint8_t old_slam = g.slam_latch; /* 0x4082, new field */
                g.slam_latch = 0; /* unconditional, matches ROM order */

                if (old_slam != 0) {
                    /* slam alarm timeout */
                    sound_cmd_send(0);
                    attract_init();
                    return; /* ROM: `jp ATTRACT_INIT`, never falls through */
                }

                /* MAINLOOP_0F7A: heartbeat-off tail */
                g.sound_ready = 0;   /* (iy-23) 0x4069 */
                {
                    uint8_t code = g.last_sound_cmd; /* (iy-24) 0x4068 read
                                                       * BEFORE clearing */
                    g.last_sound_cmd = 0;
                    heartbeat_sound(code);
                }
                if (!(g.in1_state & 0x20)) { /* (iy-81) 0x402F bit5 */
                    sound_cmd_send(0x09); /* thrust off */
                }
            }
        }

        /* MAINLOOP_0F93 and 0FA2. These two countdowns are INSIDE the
         * once-per-second wrap block - ROM 0x0F4F `jr nc,MAINLOOP_0FCC`
         * skips everything from 0x0F51 to 0x0FCB when the tick counter
         * has not wrapped. On the hardware trace (frametime
         * --special-trace) 0x4022 ticks once a second and 0x407F stays
         * 0 for the whole first minute. */
        if (g.wave_pace_timer != 0) { /* (iy-94) 0x4022 */
            g.wave_pace_timer--;
            if (g.wave_pace_timer == 0) {
                g.xlife_flags |= 0x80; /* (iy-22) 0x406A bit7 */
            }
        }

        if (g.timer4023 != 0) { /* 0x4023, new field */
            g.timer4023--;
            if (g.timer4023 == 0) {
                g.xlife_flags |= 0x40; /* (iy-22) bit6 */
                g.wave_flags  |= 0x80; /* (iy-21) 0x406B bit7 */
                if ((g.svc_flags & 0x80) && g.game_mode != 1) {
                    /* 0x0FC6: blank the HUD hub's banner JSRL at 0x8242
                     * - the "GET READY" line disappears when the wave
                     * starts (VG_WAIT_DONE itself stays dropped). */
                    dvg_banner_switch(0);
                }
            }
        }
    }

    /* fire_cooldown -= tick_delta, floored at 0 (0x0FCC) */
    g.fire_cooldown = (delta > g.fire_cooldown) ? 0 : (uint8_t)(g.fire_cooldown - delta);

    inputs_sample();

    if (g.game_mode & 0x02) { /* modes 2,3 (bit1 set) */
        omega_frame_present();  /* VG_RESTART_FRAME equivalent - see host-hook note */
        /* VG_RESTART_FRAME 0x17CB: `bit 7,(iy+26)` on wall_fx[0], the
         * "something is still glowing" latch, then call WALL_FLASH_DECAY
         * to decay wall_fx[] and rewrite the wall list's intensity
         * nibbles. */
        if (g.wall_fx[0] & 0x80)
            wall_flash_decay();
        /* VG_RESTART_FRAME 0x1788: coin arrived (svc bit6, set by the
         * IRQ) -> clear the flag and refresh the credit digit run in
         * the shadow. The NVRAM flush stays with the mode-1 site. */
        if (g.svc_flags & 0x40) {
            g.svc_flags &= (uint8_t)~0x40;
            dvg_credit_digits();
            if (credits_nvram_sync()) omega_hw_nvram_save();
        }
        objects_update();

        if (g.game_mode & 0x01) { /* mode 3 (bit0 also set) */
            frame_engine();
            return;
        }

        /* mode 2: start check, coin/demo screens, timeout back to attract */
        if (start_btn_check()) { game_start(); return; }
        if (g.svc_flags & 0x40) { attract_init(); return; }
        if (g.seconds_ctr < 0x2D) { frame_engine(); return; }
        attract_step_disp();
        return;
    }

    /* modes 1,4 */
    if (start_btn_check()) { game_start(); return; }
    if (g.game_mode != 1) { hiscore_entry_frame(); return; } /* mode 4 */

    /* ROM 0x1012/0x1016. On hardware the store into battery-backed RAM is
     * itself the persist; here that means flushing the image, but only
     * when the shadow moved - see credits_nvram_sync(). */
    if (g.svc_flags & 0x40) { if (credits_nvram_sync()) omega_hw_nvram_save(); }
    /* ROM 0x1019-0x101D. Also re-enter while the menu is active: it exits
     * through its own release branch (attract_init), which the switch gate
     * alone would skip past. */
    if (g.opmenu_active || !(omega_hw_in_p1() & 0x80)) { operator_menu(); return; }
    if (g.timer_seconds != 0) { attract_frame_tail(); return; }
    /* 0x1027-0x102D: `ld a,(page_line_total) / cp (iy+9) / jp nc,TAIL`
     * - hold the step while total >= cur, i.e. while the current
     * page's reveal has not walked past its last line. ATTRACT_INIT
     * leaves total=0 / cur=1 (released), so the first pass dispatches
     * the title immediately; DRAW_PAGE arms total>=cur for the page's
     * lifetime and PAGE_SCRIPT_STEP releases it when the last line
     * completes. */
    if (g.page_line_total >= g.page_line_cur) { attract_frame_tail(); return; }
    attract_step_disp();
}

/* ============================================================================
 * START_BTN_CHECK (0x11AA): credits gate + start-LED blink + player-
 * count/2-player/cocktail/demo flag setup on a start-button edge.
 * Returns nonzero ("start!") exactly where the ROM falls through to
 * start_2p_setup_123E with A=0xFF/NZ; returns 0 wherever the ROM
 * reaches start_none's bare `ret` (Z still set from the credits==0
 * check, or from a failed edge test). Control flow kept as labelled
 * gotos matching the ROM's branch tree (see enemies.c's
 * enemy_fire_gate for precedent - many long forward jumps, judged
 * safer for fidelity than restructuring).
 * ==========================================================================*/
int start_btn_check(void) {
    if (g.credits_bcd == 0) return 0;

    /* start-LED blink display (0x11B1-0x11D4) */
    if (g.tick_244hz & 0x40) {
        uint8_t b = (uint8_t)(g.led_shadow & (uint8_t)~0x04);
        if (g.credits_bcd >= 2) b &= (uint8_t)~0x18; /* res 3,4 */
        if (g.credits_bcd >= 4) b &= (uint8_t)~0x20; /* res 5 */
        g.led_shadow = b;
        omega_hw_leds_out(b);
    } else {
        start_leds_off();
    }

    if (g.credits_bcd < 2) goto start_2p_setup_1230;
    if (g.credits_bcd < 4) goto start_led_blink_11F6;
    if (g.in2_pressed & 0x08) { /* credits>=4 and in2_pressed bit3 */
        g.game_flags = 0;
        g.start_credits = 4;
        g.game_flags |= 0x20; /* deluxe: 2 credits/player buys a bonus
                                 ship (lives table row+1, frame.c) and
                                 its own hiscore bank */
        goto start_2p_setup;
    }
    /* fall through */

start_led_blink_11F6:
    if (!(g.in2_pressed & 0x04)) goto start_2p_setup_121C;
    g.game_flags = 0;
    g.start_credits = 2;
    /* fall through */

start_2p_setup:
    g.player_sel = 2;
    g.cur_player_num = 2;
    g.game_flags |= 0x40; /* 2-player */
    if (omega_hw_dsw_c6() & 0x80) g.game_flags |= 0x04; /* cocktail, see
        game_start()'s polarity note - same literal ROM condition */
    goto start_2p_setup_123E;

start_2p_setup_121C:
    if (!(g.in2_pressed & 0x80)) goto start_2p_setup_1230;
    g.game_flags = 0;
    g.game_flags |= 0x20; /* deluxe 1P: 2 credits, bonus ship */
    g.start_credits = 2;
    goto start_2p_setup_123E;

start_2p_setup_1230:
    if (!(g.in2_pressed & 0x40)) return 0; /* start_none */
    g.game_flags = 0;
    g.start_credits = 1;
    /* fall through */

start_2p_setup_123E:
    start_leds_off();
    g.attract_step = 1;
    return 1;
}

/* ============================================================================
 * START_LEDS_OFF (0x1248): force the start-button LEDs off (OR mask
 * 0x3C into the shadow register) and push it to hardware.
 * ==========================================================================*/
void start_leds_off(void) {
    g.led_shadow |= 0x3C;
    omega_hw_leds_out(g.led_shadow);
}

/* ============================================================================
 * START_LEDS_OFF_126C / _1255 (0x126C/0x1255): cocktail flip-screen +
 * coin-lockout bit control. Only 126C's unconditional "set bit6, then
 * push" path is common; 1255 additionally toggles the bit based on
 * cocktail seat parity before falling into the same push.
 * ==========================================================================*/
void start_leds_off_126c(void) {
    g.led_shadow |= 0x40; /* set 6,(iy-14) */
    omega_hw_leds_out(g.led_shadow);
}

void start_leds_off_1255(void) {
    if (!(g.game_flags & 0x04)) return; /* not cocktail: no-op */

    /* cocktail_seat_ref (0x406E, new field) XORed with cur_player_num
     * selects flip-screen state by seat parity; exact provenance of
     * 0x406E is outside this file's scope (also read/written by
     * P1_INPUT_READ/SPINNER_READ/PLAYER_TURN_SWAP in frame.c). */
    if ((g.cur_player_num ^ g.cocktail_seat_ref) & 0x01) {
        g.led_shadow &= (uint8_t)~0x40;
    } else {
        g.led_shadow |= 0x40;
    }
    omega_hw_leds_out(g.led_shadow);
}

/* ============================================================================
 * ATTRACT_STEP_DISP (0x1030): the 12-step attract sequence. attract_step
 * is incremented BEFORE dispatch (ROM order), so entering with
 * attract_step==1 dispatches step 2 - step "1" (att_step1_idle, a bare
 * jump to the tail) is consequently only reachable if attract_step is
 * ever 0 on entry, which normal play never produces; preserved exactly
 * as a quirk (matches MAINLINE_NOTES' step-1 row being a placeholder).
 * Steps 8/9/11/12 alias steps 5/6 exactly (ROM jumps straight to the
 * same handlers); step 10 is ATTRACT_PRICING.
 * ==========================================================================*/
void attract_step_disp(void) {
    g.attract_step++;

    switch (g.attract_step) {
    case 1:
        break; /* att_step1_idle: no-op */
    case 2:
        g.page_id = 1;
        g.timer_seconds = 0x0F;
        attract_page_load();
        draw_page(g.page_id);
        break;
    case 3:
        omega_display_reset();  /* DLIST_RESET */
        g.page_id = 2;
        g.timer_seconds = 6;
        draw_page(g.page_id);
        break;
    case 4:
        g.page_id = 3;
        g.timer_seconds = 8;
        attract_page_load();
        draw_page(g.page_id);
        break;
    case 5: case 8: case 11:
        att_step5_scores();
        break;
    case 6: case 9: case 12:
        att_step6_scores2();
        break;
    case 7:
        g.timer_seconds = 0x1E;
        game_start(); /* jp GAME_START - no attract_frame_tail */
        return;
    case 10:
        attract_pricing(); /* ends with its own attract_frame_tail() */
        return;
    default: /* attract_step==0 or >=13 */
        attract_init();
        return;
    }
    attract_frame_tail();
}

static void att_step5_scores(void) {
    g.game_mode = 1;             /* (iy-92) - re-affirms attract mode */
    omega_display_reset();  /* DLIST_RESET */
    g.timer_seconds = 0x0A;
    g.cur_player_num = 1;
    start_leds_off_126c();
    g.page_id = 0x0F;
    draw_page(g.page_id);
}

static void att_step6_scores2(void) {
    g.game_mode = 1;
    omega_display_reset();  /* ROM 0x10B9: call DLIST_RESET */
    g.timer_seconds = 0x0A;
    g.cur_player_num = 1;
    start_leds_off_126c();
    g.page_id = 0x10;
    draw_page(g.page_id);
}

/* ============================================================================
 * ATTRACT_FRAME_TAIL (0x1194): common tail for every attract-mode pass -
 * advances the page-reveal script, kicks the title shimmer, and either
 * bounces to ATTRACT_INIT (service flag) or falls into FRAME_ENGINE
 * (which, for mode 1, only does input sampling / join-in checks).
 * ==========================================================================*/
static void attract_frame_tail(void) {
    if (g.page_script_pos < g.page_script_limit) {
        page_script_step();       /* advance the scrawl while revealing */
    }
    vg_kick_title();
    if (g.svc_flags & 0x40) { attract_init(); return; }
    frame_engine();
}

/* ============================================================================
 * ATTRACT_PRICING (0x10DC): step-10 handler. Computes the 4 pricing
 * digits (coins, credits, coins*2, credits*2) from coinage1_ptr and
 * hands them to the host as draw calls; the ROM's glyph-pointer lookup
 * (table 0x3FD5) is pure DVG addressing and is dropped (rule 5) - only
 * the digit VALUES (and the one conditional blank) are preserved. The
 * doubling is plain 8-bit addition exactly as the ROM does it (no DAA),
 * which is safe since coinage digits are always single BCD digits 1-4.
 * ==========================================================================*/
void attract_pricing(void) {
    if (g.credits_bcd != 0) { attract_init(); return; }

    omega_display_reset();  /* DLIST_RESET */
    g.page_id = 0x14;
    draw_page(g.page_id);
    g.timer_seconds = 5;

    {
        uint8_t coins   = g.coinage1_ptr[0];
        uint8_t credits = g.coinage1_ptr[1];
        uint8_t coins_x2   = (uint8_t)(coins + coins);
        uint8_t credits_x2 = (uint8_t)(credits + credits);

        dvg_pricing_digit(0, coins,      0);
        dvg_pricing_digit(1, credits,    0);
        dvg_pricing_digit(2, coins_x2,   (credits >= 2) ? 1 : 0);
        dvg_pricing_digit(3, credits_x2, 0);
    }

    attract_frame_tail();
}

/* ============================================================================
 * OPERATOR_MENU (0x1137, entered when the test switch is held during
 * attract mode 1): formats the two bookkeeping ratio displays via
 * coinage_disp_prep()/bcd_div4(), draws the bookkeeping page, then
 * waits for an operator input edge. Exits: in1_pressed bit5 -> zero
 * credits + redraw; bit6 -> scores_reset(); in2_state bit6 clear AND
 * in2_pressed bit7 -> nvram_wipe(); in1_released bit7 -> back to
 * attract. Both terminal exits transition via attract_init() per the
 * score.c-extern design deviation documented above.
 * ==========================================================================*/
/* ROM 0x1137-0x1160: the two bookkeeping ratio computations, then page
 * 0x15. Split out because the ROM re-runs it from the top of its loop on
 * every "redraw" branch, and the port has to do that without looping. */
static void opmenu_redraw(void) {
    coinage_disp_prep(&g.bookkeep[0x0C], g.opmenu_disp1, &g.bookkeep[0x1C]);
    coinage_disp_prep(&g.bookkeep[0x0E], g.opmenu_disp2, &g.bookkeep[0x34]);

    g.page_id = 0x15;
    omega_display_reset();  /* DLIST_RESET */
    draw_page(g.page_id);
    g.svc_flags &= (uint8_t)~0x40; /* res 6,(iy-82) */
}

void operator_menu(void) {
    if (!g.opmenu_active) {
        g.opmenu_active = 1;
        opmenu_redraw();
    }

    vg_kick_title();
    inputs_sample();

    if (g.svc_flags & 0x40) { opmenu_redraw(); return; }  /* jr OPERATOR_MENU */
    if (g.in1_pressed & 0x20) {                            /* zero credits */
        g.credits_bcd = 0;
        opmenu_redraw();
        return;
    }
    if (g.in1_pressed & 0x40) { g.opmenu_active = 0; scores_reset(); attract_init(); return; }
    if (!(g.in2_state & 0x40) && (g.in2_pressed & 0x80)) {
        g.opmenu_active = 0;
        nvram_wipe();
        attract_init();
        return;
    }
    if (g.in1_released & 0x80) { g.opmenu_active = 0; attract_init(); return; }
}

/* ============================================================================
 * COINAGE_DISP_PREP (0x1646): builds an 8-byte display buffer from a
 * pair of 4-byte BCD ledger counters, either as a raw copy (when the
 * "flag" byte is nonzero) or as two BCD_DIV4 quotients (counter /
 * scalar). Faithful direct port - see bcd_div4() below for the actual
 * repeated-subtraction divide.
 * ==========================================================================*/
static void coinage_disp_prep(const uint8_t count_flag[2], uint8_t dest8[8],
                               const uint8_t src8[8]) {
    if (count_flag[1] != 0) {
        memcpy(dest8, src8, 8);
        return;
    }

    {
        uint8_t divisor = count_flag[0];
        bcd_div4(dest8 + 0, src8 + 0, divisor);
        bcd_div4(dest8 + 4, src8 + 4, divisor);
    }

    /* ROM 0x1660-0x166E tail: shifts dest[4..6] right by one byte into
     * dest[5..7] (dest[4] left as-is, dest[7] overwritten by old
     * dest[6]) - a digit-alignment quirk for the second (credits)
     * quotient; preserved exactly though its display rationale isn't
     * annotated in the ROM comments. */
    dest8[7] = dest8[6];
    dest8[6] = dest8[5];
    dest8[5] = dest8[4];
}

/* Z80 SUB+DAA / SBC+DAA on a single BCD byte (00-99), reimplemented as
 * plain decimal arithmetic (equivalent result, no register/flag
 * emulation needed). Local to this file - not one of the shared
 * bcd_* helpers in omega_state.h, which are add-only. */
static uint8_t bcd_sub1(uint8_t a, uint8_t b, int* borrow_io) {
    int av = (a >> 4) * 10 + (a & 0x0F);
    int bv = (b >> 4) * 10 + (b & 0x0F);
    int r = av - bv - (*borrow_io ? 1 : 0);
    if (r < 0) { r += 100; *borrow_io = 1; } else { *borrow_io = 0; }
    return (uint8_t)(((r / 10) << 4) | (r % 10));
}

/* ============================================================================
 * BCD_DIV4 (0x1674): dest4 = floor(dividend4 / divisor_bcd), computed
 * by repeated 4-byte BCD subtraction (ROM 0x1692-0x16C3), ported
 * faithfully. divisor_bcd==0 (ROM: 0x4093==0)
 * yields quotient 0, matching the ROM's skip-straight-to-16C5 branch.
 * ==========================================================================*/
static void bcd_div4(uint8_t dest4[4], const uint8_t dividend4[4], uint8_t divisor_bcd) {
    uint8_t remainder[4];
    uint8_t quotient[4] = { 0, 0, 0, 0 };
    static const uint8_t ONE4[4] = { 1, 0, 0, 0 };

    memcpy(remainder, dividend4, 4);

    if (divisor_bcd != 0) {
        for (;;) {
            uint8_t attempt[4];
            int borrow = 0;
            int i;

            attempt[0] = bcd_sub1(remainder[0], divisor_bcd, &borrow);
            for (i = 1; i < 4 && borrow; i++) {
                attempt[i] = bcd_sub1(remainder[i], 0, &borrow);
            }
            if (borrow) break; /* underflowed past the top byte: done,
                                 * this attempt does NOT count */
            for (; i < 4; i++) attempt[i] = remainder[i];

            memcpy(remainder, attempt, 4);
            bcd_add(quotient, ONE4, 4);
        }
    }

    memcpy(dest4, quotient, 4);
}

/* ============================================================================
 * HISCORE_ENTRY_FRAME (0x127B): thin mode-4 per-frame wrapper.
 * Kicks the title shimmer, runs the extern initials-entry logic,
 * checks for a join-in start, and either continues FRAME_ENGINE (mode
 * still 4) or falls into the extern tail covering the dual-hiscore-bank
 * copy + player-switch + respawn/attract-return branch (0x128E-0x12CF).
 * ==========================================================================*/
/* HISCORE_ENTRY_FRAME 0x128E-0x12CF: the commit tail, reached when
 * HISCORE_INITIALS has ended the entry (game_mode left 4). Writes the
 * working table back to the context bank selected by game_flags bit 5,
 * marks bit 4 ("an initials entry was committed this game over"), and
 * decrements player_sel - the count of players still owed a turn at
 * the letter wheel (1 from ATTRACT_INIT, 2 from a 2P start). While
 * players remain, the OTHER player is selected - game_flags bit 3
 * records that P2 (the higher scorer) went first - and control jumps
 * into frame.c's fe_respawn_tick_1478, whose hiscore_check either
 * re-enters mode 4 for that player or falls out to the score pages. */
void hiscore_entry_frame_resume(void);

void hiscore_entry_finish(void)
{
    g.attract_step = 0x0C;                        /* 0x128E ld (iy+3),$0C */
    hiscore_context_sync((g.game_flags & 0x20) ? 1 : 0); /* 0x1292-0x12A4 */
    nvram_save_all();  /* host persist: on hardware the bank copy lands
                          in battery RAM, so the store IS the persist */
    g.game_flags |= 0x10;                         /* 0x12A6 set 4,(iy-63) */
    if (--g.player_sel != 0) {                    /* 0x12AA dec (iy-65) */
        if (!(g.game_flags & 0x08))               /* 0x12AF bit 3,(iy-63) */
            g.cur_player_num = 2;                 /* 0x12B5: P2's turn next */
        else
            g.cur_player_num = 1;                 /* 0x12BE: P1's turn next */
        fe_respawn_tick_1478();                   /* 0x12C5 */
        return;
    }
    hiscore_entry_frame_resume();                 /* 0x12C8 fall-through */
}

/* HISCORE_ENTRY_FRAME_12C8: every owed initials entry is done - enter
 * the attract score page of the context that was just written, which
 * exits through ATTRACT_FRAME_TAIL exactly as the ROM's jp does
 * (att_step5_scores 0x10B2). Also called from frame.c's fe_next_player
 * when a non-qualifying player follows a committed entry (0x148E). */
void hiscore_entry_frame_resume(void)
{
    if (!(g.game_flags & 0x20)) att_step5_scores();  /* 0x12CC */
    else                        att_step6_scores2(); /* 0x12CF */
    attract_frame_tail();
}

void hiscore_entry_frame(void) {
    /* VG_KICK_TITLE_180D (0x1815-0x181E): the mode-4 kick copies the
     * 0x7A-byte alphabet region shadow -> 0x8000 every frame - that is
     * how the cursor and typed-initials shadow writes reach the screen
     * (one frame later, since this runs before HISCORE_INITIALS). */
    dvg_mode4_kick_copy();

    hiscore_initials();

    if (start_btn_check()) { game_start(); return; }
    if (g.game_mode == 4) { frame_engine(); return; }

    hiscore_entry_finish();
}

/* ============================================================================
 * VG_KICK_TITLE (0x17D5): title-shimmer logic only - on
 * mode 1, page 2 (the title page), cycles two shape slots through the
 * 4-entry ROM tables at 0x3F4D/0x3F55 keyed by tick bits 2-3 (a 16 Hz
 * shimmer, since bits 2-3 of a 244 Hz counter change every 4 ticks).
 * The VG-busy check, WATCHDOG kick, and VG_GO hardware kick are pure
 * hardware and dropped (rule 6).
 * ==========================================================================*/
void vg_kick_title(void) {
    if (g.game_mode != 1) return;
    if (g.page_id != 2) return;
    dvg_title_shimmer();      /* rewrites 0x8080/0x80A8 in the live list */
}
