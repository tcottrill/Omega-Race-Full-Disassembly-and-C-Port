/* omega_state.h - Omega Race C conversion: shared game state.
 * Derived from the disassembly RAM map (disasm/omega_main.asm,
 * disasm/FUNCTIONS.md). Names match the disassembly symbols so the C
 * code can be diffed against the annotated listing.
 *
 * Conventions (see CONVENTIONS.md):
 *  - BCD values stay BCD (uint8_t per byte pair of digits) and use the
 *    bcd_* helpers, so scoring/credit math matches the ROM exactly.
 *  - All timing is expressed in CPU-board ticks; the host supplies
 *    tick_244hz and tick_delta exactly like the interrupt did.
 *    (Named for the real 244.140625 Hz rate, OMEGA_TICK_HZ below;
 *    the literature calls the same tick "250 Hz".)
 *  - No direct hardware access: modules call the omega_hw_* interface.
 */
#ifndef OMEGA_STATE_H
#define OMEGA_STATE_H

#include <stdint.h>

/* ---- the CPU board's interrupt rate ------------------------------------
 * Derived from the schematic (manual sheet omegaM2), NOT from the
 * traditional "250 Hz" figure, which this hardware cannot produce:
 *
 *   XTAL101  12 MHz
 *     -> N8  74161  QD          = /16   ->  750 kHz
 *     -> S5  74393  2QD (pin 8) = /256  ->  2929.6875 Hz
 *     -> R6  74161  preset 4, RCO fed back to LOAD (counts 4..15)
 *                               = /12   ->  244.140625 Hz
 *     -> S6 inverter -> H9 7474 (D tied low) -> Z80 INT,
 *        cleared by the M1 interrupt acknowledge.
 *
 * 12 MHz / 49152 exactly; 12288 Z80 cycles at 3 MHz; 4.096 ms per tick.
 * The traditional 250 Hz figure (shared with MAME) is 2.4% fast. To get
 * exactly 250 Hz you would need 750 kHz / 3000, and no combination of
 * a binary 74393 tap with a <=/16 74161 yields 3000.
 */
#define OMEGA_TICK_HZ   244.140625
#define OMEGA_TICK_MS   (1000.0 / OMEGA_TICK_HZ)      /* 4.096 ms */

/* ---- object system ----------------------------------------------------- */

#define OBJ_COUNT       32
#define OBJ_SHIP        1        /* object index bands:               */
#define OBJ_DROID_FIRST 2        /*  1      = player ship             */
#define OBJ_DROID_LAST  13       /*  2-13   = droids                  */
#define OBJ_SHOT_FIRST  14       /*  14-21  = shots / mines           */
#define OBJ_SHOT_LAST   21       /*  22+    = command / death ships   */
#define OBJ_SPECIAL_FIRST 22

typedef struct {
    /* 23-byte record, offsets match the ROM layout */
    uint8_t  flags0;             /* +0  bit7 active, bit4 shot-capable,
                                        bit2 armed, bit1 track dir     */
    uint8_t  flags1;             /* +1  bit1/2 dlist built, bit3 slot
                                        assigned, bit5 dying, bit6 pool,
                                        bit7 wall-follow               */
    int16_t  velx;               /* +2  16-bit velocity               */
    int16_t  vely;               /* +4                                */
    int16_t  posx;               /* +6  high byte = screen X (dvg.c
                                        builds LABS X from this)      */
    int16_t  posy;               /* +8  screen Y (LABS Y source)      */
    uint8_t  angle;              /* +10 6-bit heading                 */
    uint8_t  b11, b12, b13, b14; /* +11..14 per-class fields          */
    uint8_t  aim;                /* +15 aim/state byte                */
    uint8_t  radx, rady;         /* +16/17 collision half-extents     */
    uint8_t  b18;                /* +18                               */
    uint8_t  timer;              /* +19 decremented by tick_delta     */
    uint8_t  b20;                /* +20                               */
    uint16_t shape_word;         /* +21/22: the object's DVG shape word,
                                        built by the ROM as
                                        0xC000|(base+index) and stored
                                        low byte in +21, high in +22
                                        (see ROM 0x2742 in SPAWN_MINE_
                                        SHOT). Some classes reuse the
                                        pair as a table pointer instead
                                        (the ship template holds 0x30A1).*/
} obj_rec;

/* ---- whole-game state -------------------------------------------------- */

typedef struct {
    /* timebase (the 244.14 Hz interrupt + MAINLOOP head) */
    uint8_t  tick_244hz;         /* 0x4017 free-running tick          */
    uint8_t  tick_prev;          /* 0x4018 */
    uint8_t  tick_delta;         /* 0x4019 ticks since last pass      */
    uint8_t  seconds_ctr;        /* 0x4020 ~1 Hz (tick wraparound)    */
    uint8_t  timer_seconds;      /* 0x4021 general 1 Hz countdown     */

    /* mode machine */
    uint8_t  game_mode;          /* 0x4024: 1 attract 2 spawn/demo
                                            3 play    4 hiscore entry */
    uint8_t  attract_step;       /* 0x4083 */
    uint8_t  page_id;            /* 0x4084 */

    /* coins / credits (BCD) */
    uint8_t  credits_bcd;        /* 0x402D, capped 0x20               */
    uint8_t  coin_mode1, coin_mode2;
    uint8_t  bookkeep[0x40];     /* 0x4427 ledger, 4-byte BCD each    */

    /* score / progression */
    uint8_t  score_cur[4];       /* 0x4036 BCD little-endian          */
    uint8_t  time_played[4];     /* 0x403A BCD seconds                */
    uint8_t  lives;              /* 0x4066 */
    uint8_t  wave_num;           /* 0x4067 (+2 per wave, clamp 12)    */
    uint8_t  wave_ctr;           /* 0x406C */
    uint8_t  wave_flags;         /* 0x406B bit6 = wave complete       */
    uint8_t  xlife_flags;        /* 0x406A bits3-5 award latches;
                                        bits0/7 reused by enemy_fire_gate
                                        as one-shot engagement latches   */
    uint16_t xlife_thr[3];       /* 0x4060/62/64 from DIPs            */
    uint8_t  kill_tier;          /* 0x406F -> points table index      */
    uint8_t  wave_pace_timer;    /* 0x4022 countdown set by PLAY_BEGIN/
                                        DEMO_AUTODESTRUCT/etc, gates
                                        HEARTBEAT_SOUND tempo + the
                                        enemy_fire_gate spawn windows    */

    /* enemy fire designation */
    uint8_t  shooter_state;      /* 0x4077 0/1/2+ eligible            */
    uint8_t  shooter_a, shooter_b; /* 0x407A/0x407B slot indices      */
    uint16_t pair_min;           /* 0x4080/81 closest-pair minimum    */
    /* (no aim_angle here: 0x400F is the staged object's own aim byte -
       OBJ_STAGE_RUN copies the record into workspace 0x4000-16 and back,
       so every 0x400F access is per-object state, held in obj_rec.aim;
       a shared cell would clobber the designated shooter's aim between
       stage-runs.)                                                      */
    uint8_t  difficulty_param;   /* 0x407C DIFFICULTY_CALC output; also
                                        reused by enemy_fire_gate as the
                                        active shooter's special-spawn
                                        countdown                        */
    uint8_t  special_spawn_armed;/* 0x407F enemy_fire_gate: nonzero once
                                        armed, counts down difficulty_param
                                        to trigger SPAWN_SPECIAL          */
    /* (no fire_urgency_timer: 0x4026 is coin_batch[1], the chute-2 coin
       counter below - its only ROM accesses are the coin IRQ 0x010B/
       0x010E/0x0118 and the slam clear 0x0058. The enemy-side pace
       writes at 0x24CC and 0x231E target (iy-94) = wave_pace_timer.)   */

    /* players */
    uint8_t  player_cur;         /* 0x407E obj_class alias ONLY: a
                                        transient collision-class scratch
                                        written by FRAME_SYNC_DISP's
                                        per-band COLLIDE_SCAN dispatch,
                                        incremented per scanned miss
                                        inside COLLIDE_SCAN, and read
                                        back by OBJ_DESTROY (shot/mine
                                        sound band) and cs_kill_object
                                        (shooter_a/b clear). The real
                                        current-player cell is
                                        cur_player_num (0x403E), below;
                                        the ROM aliases these two bytes
                                        because their live ranges never
                                        overlap (mid-frame object
                                        processing vs game-over/respawn),
                                        but they are logically distinct  */
    uint8_t  p_save[2][13];      /* 0x4046 / 0x4053 save areas        */

    /* high score */
    uint8_t  hiscore[6][7];      /* 0x43A9 4 BCD + 3 initials         */
    uint8_t  initials_cnt;       /* 0x4098 */
    uint8_t  letter_sel;         /* 0x4099 */

    /* input snapshot (was INPUTS_SAMPLE) */
    uint8_t  in1_state, in1_pressed, in1_released;
    uint8_t  in2_state, in2_pressed;
    uint8_t  spinner;            /* decoded 0-63                      */
    /* 0x40BC is NOT a field of its own: the ship record starts at 0x40B2,
     * so 0x40BC is obj[OBJ_SHIP].angle (+10). The ROM's `ld
     * (ship_angle),a` at 0x12F6 stores the spinner heading straight into
     * the ship's own record, and OBJ_PHYSICS reads it back from there.
     * Use g.obj[OBJ_SHIP].angle. */

    /* wall bounce fx: index 0 is a meta "still glowing" latch (bit6
     * accumulated by WALL_FLASH_DECAY, reset to 0x80/0x00 each pass);
     * indices 1-19 are the real wall segments, decayed each pass and
     * set to 0x96 by WALL_HIT_FX on a fresh bounce.                    */
    uint8_t  wall_fx[20];        /* 0x409A/0x409B.. segment glow      */

    /* objects */
    obj_rec  obj[OBJ_COUNT + 1]; /* 1-based like the ROM              */
    uint8_t  obj_settle[OBJ_COUNT + 1];
                                 /* byte +7 of each object's display-list
                                    entry (word3's high byte) - the ROM's
                                    beam-settling delay, written by
                                    obj_proximity_upd at 0x1A3C. Zero
                                    length and zero intensity, so it draws
                                    nothing; it exists to buy the DVG time
                                    to slew the beam between objects, and
                                    therefore matters to frame pacing.
                                    Kept out of obj_rec because obj_rec
                                    mirrors the ROM's 23-byte record and
                                    this lives in the display list.      */
    uint16_t obj_shape[OBJ_COUNT + 1];
                                 /* word 5 of each object's display-list
                                    entry: the JSRL that draws it. The ROM
                                    writes it in move_mode_dispatch
                                    (0x1E5B/0x1E63) - i.e. only on a pass
                                    where the object actually stage-runs -
                                    and the entry keeps it until the next
                                    one. Cached here for the same reason
                                    obj_settle is: the selection has side
                                    effects (the explosion/droid animation
                                    counters advance, and the ship path
                                    stores its aim byte), so it must run at
                                    object-update rate, not once per
                                    rendered frame.                       */
    uint8_t  obj_index;          /* 0x407D: index (1-32) of whichever
                                        object OBJECTS_UPDATE is
                                        currently staging/processing;
                                        read directly (as a global, not
                                        a parameter) by OBJ_PHYSICS,
                                        COLLIDE_SCAN and OBJ_DESTROY     */
    /* (no death_timer_a/b: OBJ_DESTROY's (iy-95)/(iy-94) resolve to
       timer_seconds 0x4021 and wave_pace_timer 0x4022, which already
       have fields above; the hardware cells 0x4029/0x402A are never
       read by anything - see CONVENTIONS rule 2.)                      */
    uint8_t  rng_cache;          /* 0x4079: PRNG output cache written
                                        every frame by frame_tail's
                                        random_next; OBJ_SPAWN reads
                                        bit7 as a placement coin-flip   */

    /* sound */
    uint8_t  last_sound_cmd;     /* 0x4068 */
    uint8_t  sound_ready;        /* 0x4069 */

    uint8_t  rng;                /* 0x4073 random_val                 */

    /* ---- integrated per-module field requests (canonical names) ---- */

    /* coins / IRQ service (irq_coins.c) */
    uint8_t  coin_lines;         /* 0x4035 prev coin-line sample      */
    uint8_t  slam_latch;         /* 0x4082 slam alarm active          */
    uint8_t  coin_debounce[2];   /* 0x401B/1C 5-tick debounce         */
    uint8_t  coin_pending[2];    /* 0x401D/1E awaiting meter pulse    */
    uint8_t  coin_batch[2];      /* 0x4025/26 coins toward credit     */
    uint8_t  meter_pulse_t;      /* 0x401F 0x32 -> 0 pulse timer      */
    uint8_t  led_shadow;         /* 0x4072 port-0x13 shadow           */
    uint8_t  nvram_credits[2];   /* 0x5C56/57 credit shadow bytes     */

    /* shared flag bytes (multiple modules) */
    uint8_t  svc_flags;          /* 0x402E bit6 credit-display-dirty /
                                        service return, bit7 sound+
                                        scoring enable gate            */
    uint8_t  game_flags;         /* 0x4041 bit2 cocktail-in-progress,
                                        bit5 demo (score.c notes it
                                        doubles as audit-bank/context
                                        selector), bit6 two-player,
                                        bit7 game-in-progress          */

    /* players (mainline.c; 0x403E is the real player-select cell -
     * player_cur/0x407E above is the aliased obj_class scratch)       */
    uint8_t  cur_player_num;     /* 0x403E active player, 1 or 2: every
                                        ROM write pairs it with the
                                        matching p1/p2 save block, every
                                        read selects per-player ports/
                                        pages/screen flip. Distinct cell
                                        from 0x407E obj_class.          */
    uint8_t  player_sel;         /* 0x403F players still owed a mode-4
                                        initials entry (see mainline.c) */
    uint8_t  start_credits;      /* 0x4040 credits the pending start
                                        consumes (1/2/4) - written by
                                        START_BTN_CHECK per button+credit
                                        combo, BCD-subtracted from
                                        credits_bcd once at PLAY_BEGIN
                                        0x0BE3. NOT the player count
                                        (that is 0x403E/0x403F).        */
    uint8_t  in2_seat_raw;       /* 0x4034                            */
    uint8_t  score_disp_shadow[4]; /* 0x4042-45                       */
    uint8_t  score_final_bcd[2][4]; /* 0x404B P1 / 0x4058 P2 final-
                                        score compare areas            */
    uint8_t  frame_tail_flags[4]; /* 0x40A4/A7/AA/AD, set each frame,
                                        consumer unknown                */
    uint8_t  wave_anim_active;   /* non-ROM: WAVE_CLEAR_ANIM state    */
    int16_t  wave_anim_ticks;    /* non-ROM: WAVE_CLEAR_ANIM's 256-tick
                                        countdown. SIGNED on purpose - an
                                        unsigned counter can only notice
                                        it reached zero by landing on it
                                        exactly, which tick_delta does not
                                        do; see wave_clear_anim_step()   */
    /* POST / diagnostics (post.c). Non-ROM: the ROM runs the self test as
       a chain of IX-linked continuations with its own blocking VG waits,
       before GAME_INIT and outside the mode machine; the port needs
       explicit state to drive it one frame at a time. */
    uint8_t  post_active;        /* non-ROM: POST owns the screen      */
    uint8_t  post_stage;         /* non-ROM: post_stage enum           */
    uint8_t  post_step;          /* non-ROM: tests marked so far       */
    uint8_t  post_fire_prev;     /* non-ROM: EDGE_BIT6_IX edge latch   */
    int16_t  post_ticks;         /* non-ROM: signed, see wave_anim_ticks */

    uint8_t* initials_ptr;       /* 0x4094 initials write pointer     */
    uint8_t* initials_cur;       /* 0x4096 initials write pointer     */

    /* coinage / operator menu (mainline.c) */
    const uint8_t* coinage1_ptr; /* was 0x4027 -> coinage table row   */
    const uint8_t* coinage2_ptr; /* was 0x4029 (aliases death_timer_*
                                        on hardware only)              */
    uint8_t  opmenu_active;     /* non-ROM: the operator menu is a
                                        blocking `for(;;)` on hardware;
                                        the port runs it one frame at a
                                        time and needs to know it is in  */
    uint8_t  opmenu_disp1[8];    /* 0x4471 pricing display buffer     */
    uint8_t  opmenu_disp2[8];    /* 0x4479                            */

    /* high-score battery banks (mainline.c / score.c hiscore_ctx)    */
    uint8_t  hiscore_bank[2][6][7]; /* 0x43D3 / 0x43FD                */
    uint8_t  nvram_template[32]; /* 0x5C1A validation nibbles         */
    uint8_t  counter_406D;       /* 0x406D: SCORE_ADD increments (ships
                                        earned); TURN_SWITCH uses as
                                        turn-banner countdown - same
                                        cell, both uses preserved       */

    /* misc timers / page reveal (mainline.c / frame.c)               */
    uint8_t  fire_cooldown;      /* 0x4070 -= tick_delta, floor 0     */
    uint8_t  thrust_flicker;     /* 0x4071 (iy-15): incremented once per
                                        frame by VG_RESTART_FRAME while
                                        thrust is held; bit 0 chooses
                                        flame-on or flame-off for the
                                        ship sub-list slot 0x818C, so
                                        the exhaust strobes every other
                                        frame instead of burning steady */
    uint8_t  timer4023;          /* 0x4023 1 Hz countdown             */
    uint16_t page_script_pos;    /* glyphs revealed so far. The ROM's
                                        0x408B/0x4089 pair counts LINES
                                        (total vs cursor); the host
                                        reveals a glyph prefix instead,
                                        so these count glyphs - and must
                                        be 16-bit, because page 1 is ~350
                                        glyphs, past an 8-bit range      */
    uint16_t page_script_limit;  /* glyphs in the current page, 0 = the
                                        page is painted in one go        */
    uint8_t  cocktail_seat_ref;  /* (iy-18)/0x406E seat-swap ref      */

    /* ROM page machinery (dvg_pages.c, the DVG text path) - the same
     * cells the listing names. pos/limit above are the reveal gate the
     * call sites check; dvg_pages keeps them in sync. */
    uint16_t page_ptr;           /* 0x4085 page flag-byte ROM addr    */
    uint16_t page_rec_next;      /* 0x4087 next record ROM addr       */
    uint8_t  page_line_cur;      /* 0x4089 current line, 1-based      */
    uint8_t  page_line_glyph;    /* 0x408A glyph bytes done this line */
    uint8_t  page_line_total;    /* 0x408B lines in the page          */
    uint8_t  page_line_len;      /* 0x408C glyph bytes in cur line    */
    uint16_t page_script_ptr;    /* 0x408D glyph source ROM addr      */
    uint16_t vlist_cursor;       /* 0x408F display-list write addr    */
} omega_state;

extern omega_state g;

/* ---- hardware/host interface -------------------------------------------
 * The GUI gets these from app_loop.c, which maps the abstract platform
 * inputs (platform/omega_platform.h) to these port encodings and
 * serializes NVRAM over the backend's blob store; harness builds get
 * them from platform/headless/plat_headless.c as raw injectable port
 * bytes. omega_display_reset/omega_frame_present live in glue.c. Game
 * modules call these names either way.                                 */

void omega_hw_sound(uint8_t cmd);          /* was out(0x14)           */
uint8_t omega_hw_dsw_c4(void);
uint8_t omega_hw_dsw_c6(void);
uint8_t omega_hw_in_p1(void);              /* port 0x11 raw           */
uint8_t omega_hw_in_p2(void);              /* port 0x12 raw           */
uint8_t omega_hw_spinner(void);            /* port 0x15 raw           */
uint8_t omega_hw_spinner2(void);           /* port 0x16 raw (cocktail)*/
void omega_hw_nvram_save(void);
void omega_hw_nvram_load(void);
void omega_hw_leds_out(uint8_t led_shadow); /* was out(0x13): meters,
                                        start LEDs, screen flip        */
void omega_frame_present(void);            /* host presents one frame  */
void omega_display_reset(void);            /* was DLIST_RESET 0x1823:
                                        clears vector RAM              */
void draw_page(uint8_t page_id);           /* dvg_pages.c: DRAW_PAGE   */

/* ---- HUD digit runs ------------------------------------------------------
 * Where the score, last-score and credit digits are drawn, read off a
 * live machine rather than invented; the derivation is in frame.c.
 * These live here because score.c redraws the same slots when the score
 * changes - one definition, two users. */
#define HUD_SCORE_X    352.0f
#define HUD_SCORE_Y    500.0f
#define HUD_SCORE_SC        1
#define HUD_CREDITS_X  348.0f
#define HUD_CREDITS_Y  400.0f
#define HUD_CREDITS_SC      0
#define HUD_P2SCORE_X  352.0f      /* the ROM's "LAST SCORE" run */
#define HUD_P2SCORE_Y  400.0f
#define HUD_P2SCORE_SC      1

/* ---- DVG display list (dvg.c) ------------------------------------------- */

void dvg_init(void);
void dvg_dlist_reset(void);                /* DLIST_RESET 0x1823       */
void dvg_ship_buffers_build(void);         /* WALL_BUFFERS_BUILD 0x18BC*/
void dvg_walls_install(void);              /* GAME_DLIST_BUILD's walls */
void dvg_build_object_list(void);          /* OBJECTS_UPDATE's entries */
uint16_t dvg_obj_shape_select(obj_rec* o); /* move_mode_dispatch 0x1DB4
                                              -0x1E6D: pick this object's
                                              shape word (and, for the
                                              ship, write its sub-list)  */
void dvg_render(void);                     /* walk the list, emit lines*/

/* authentic frame timing (PROM state machine, dvg.c): after drawing a
 * frame the host asks for that frame's authentic period to pace the
 * next mainloop pass. */
int32_t dvg_cost_list(void);
double  dvg_cost_frame_ms(void);

/* ---- BCD helpers -------------------------------------------------------- */

uint8_t bcd_add8(uint8_t a, uint8_t b, int* carry);
void bcd_add(uint8_t* dst, const uint8_t* src, int n);   /* dst += src */
int  bcd_cmp(const uint8_t* a, const uint8_t* b, int n); /* -1/0/1     */

#endif
