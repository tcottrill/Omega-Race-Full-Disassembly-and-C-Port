/* objects.c - Omega Race C conversion: the object system.
 *
 * Translates (disasm/omega_main.asm, all addresses ROM/CPU0):
 *   OBJ_SPAWN            0x291D
 *   OBJ_KILL              0x29B2
 *   OBJ_DESTROY            0x27BF  (incl. kill_snd_droid/kill_snd_cmdship)
 *   OBJECTS_UPDATE          0x1965
 *   OBJ_UPDATE_LOOP          0x1985
 *   obj_expire_slot/obj_build_check/obj_proximity_upd  0x199B-0x1A58
 *   OBJ_STAGE_RUN             0x19CD
 *   OBJ_PHYSICS                0x1B16 (thrust..position integration only;
 *                                      see "module boundary" note below)
 *   WALL_BOUNCE                 0x1F2A
 *   WALL_HIT_FX                   0x20CE
 *   APPLY_FRICTION                  0x2148
 *   CLAMP_SPEED                       0x2164
 *   COLLIDE_SCAN                        0x2199
 *
 * ---- staging transformation ---------------------------------------------
 * The ROM keeps exactly one 23-byte object record "staged" at a time in a
 * fixed workspace, RAM 0x4000-0x4016 (see FUNCTIONS.md header and the
 * obj_rec comment in omega_state.h): OBJ_STAGE_RUN copies the record being
 * processed into that workspace, runs the shared per-frame logic against
 * it (thrust, wall bounce, collision...), then copies the workspace back
 * over the real record. All the "(iy+N)"/"(ix+N)" accesses to that
 * workspace in the disassembly are therefore just field accesses on
 * *whichever* obj_rec is currently being processed. In this file we skip
 * the copy-in/copy-out entirely and just pass an `obj_rec*` around - the
 * struct layout in omega_state.h already matches the ROM's field offsets,
 * so nothing is lost, and there is no separate "workspace" object at all.
 * `g.obj_index` mirrors the ROM's 0x407D global (which object, 1-32, is
 * currently staged) since several routines (OBJ_PHYSICS, COLLIDE_SCAN,
 * OBJ_DESTROY) read it directly as a global rather than taking it as a
 * parameter - kept as-is rather than threading an index through every
 * call, to stay diffable against the listing.
 *
 * ---- axes ------------------------------------------------------------
 * obj_rec.posx carries the screen-X coordinate and obj_rec.posy carries
 * screen-Y (see omega_state.h); both are 16-bit with the HIGH byte the
 * integer screen coordinate and the low byte a sub-pixel accumulator fed
 * by velocity integration. The OBJ_HI/OBJ_SET_HI helpers below read/write
 * just that high byte, matching the ROM's frequent 8-bit-only compares.
 *
 * ---- module boundary with enemies.c ----------------------------------
 * The ROM's OBJ_PHYSICS routine does not `ret` after position integration
 * (0x1BD5-0x1BEA) - it falls straight through into "enemy_fire_gate"
 * (0x1BEB) and from there into shooter designation / AI-aim / special-ship
 * spawn / movement-mode dispatch, all the way out past 0x1DAD to the one
 * `ret` at 0x1F28. Per CONVENTIONS.md's module map, SHOOTER_PROMOTE,
 * AIM_AT_SHIP, SPAWN_MINE_SHOT, SPAWN_SPECIAL, WALL_TRACK_STEER,
 * RANDOM_NEXT, DIFFICULTY_CALC and HEARTBEAT_SOUND all belong to
 * enemies.c, so that stretch is split: obj_physics() stops at position
 * integration, enemies.c's enemy_fire_gate() covers 0x1BEB-0x1DB3, and
 * move_mode_dispatch() below covers 0x1DB4-0x1F28. obj_collide_dispatch()
 * calls the three in that order, which is the fall-through chain.
 *
 * Similarly, FRAME_SYNC_DISP (0x1A59-0x1B15) sits between OBJECTS_UPDATE's
 * explicit task range (...0x1A58) and OBJ_PHYSICS's (0x1B16+). It is the
 * glue that decides which slice of the object table COLLIDE_SCAN should
 * scan against the currently-staged object, so it is translated here
 * (obj_collide_dispatch) since COLLIDE_SCAN is explicitly in scope and
 * this is its only caller; the watchdog kick and VG_RESTART_FRAME/VG
 * status poll it also did are hardware/display-list artifacts dropped
 * per CONVENTIONS rule 5/6 (display restart is the renderer's job now).
 *
 * ---- dropped ROM/hardware artifacts (CONVENTIONS rule 6) -------------
 *  - watchdog `in a,($09)` kicks throughout: dropped, host's job.
 *  - FRAME_SYNC_DISP's VG_RESTART_FRAME call and VG-busy poll: display
 *    list restart, not modeled (rule 5).
 *  - obj_expire_slot's slot-address-to-word-offset computation
 *    (0x19AE-0x19BD): this wrote a display-list byte offset back into
 *    the object table's own slot-pointer cell for the DVG renderer;
 *    dropped (rule 5). Only the one-shot "slot assigned" flag
 *    (flags1 bit3) semantics are preserved, since nothing else in this
 *    file's scope reads that bit, this is kept for gameplay fidelity in
 *    case a future module needs it.
 *  - WALL_FLASH_DECAY's per-segment vector-list nibble writes (the
 *    `(de)` reads/writes into dlist 0x81BB+, stride 6) are NOT dropped:
 *    the wall list lives in vector RAM where the hardware puts it (see
 *    frame.c game_dlist_build) and this routine rewrites those bytes
 *    exactly as the ROM does, so the DVG walker draws the result.
 *
 * ---- rendering (CONVENTIONS rule 5) -----------------------------------
 * Per-object omega_draw_shape() calls are NOT emitted from this file.
 * The ROM built the display list as a side effect of OBJ_STAGE_RUN
 * (each object owned a fixed dlist slot rewritten with its current
 * LABS position + a JSRL to its shape subroutine). Shape selection
 * lives in dvg.c (dvg_obj_shape_select, called from move_mode_dispatch),
 * and dvg.c's dvg_build_object_list rebuilds the object region of the
 * display list from the records at draw time, using the positions this
 * file maintains in obj_rec.posx/posy.
 *
 * ---- uncertain / not independently verified --------------------------
 *  - obj_proximity_upd/obj_track_pos (0x19FC-0x1A4F) computes the
 *    beam-settling delay for this object's display-list entry (word3's
 *    high byte) from how far the beam has to travel since the last
 *    object - see obj_settle_update below. The same computation appears
 *    again at the tail of move_mode_dispatch (0x1EE5-0x1F1A) for
 *    objects that took the full stage-run path. Still unverified: the
 *    exact tier spacing's intent beyond "further to slew, longer to
 *    settle".
 *  - WALL_BOUNCE/WALL_HIT_FX's wall-coordinate constants (0x21, 0x30,
 *    0x50, 0x78, 0x7E, 0x80 etc.) are transcribed literally from the
 *    branch structure rather than independently re-derived into a
 *    clean "outer rect + inner island" geometric model; see the
 *    per-function comments for the literal transliteration. Given how
 *    tangled the branch structure is, this file mirrors it register-
 *    for-register (goto-based) rather than risk misrepresenting the
 *    logic with a "cleaned up" rewrite.
 *  - obj_class's post-scan value (g.player_cur, incremented once per
 *    COLLIDE_SCAN miss starting from the caller's seed) feeding
 *    OBJ_DESTROY's shot/mine sound-band dispatch: mechanically
 *    preserved, but *why* a miss-count offset from a class seed should
 *    select a destroy sound is not understood - flagged as-is.
 *  - flags0/flags1 bit meanings beyond what omega_state.h documents are
 *    inferred from which ROM object templates set them (see the
 *    OBJ_F0_ and OBJ_F1_ comments below) and from control-flow context,
 *    not from an independent authoritative source.
 */

#include <string.h>
#include "omega_state.h"

/* ---- cross-module externs (implemented elsewhere) ----------------------- */

extern void score_add(uint8_t kill_tier);              /* score.c   0x2335 */
extern void sound_cmd_send(uint8_t cmd);               /* sound module 0x2E04 */
extern uint8_t random_next(uint8_t entropy_arg);       /* enemies.c 0x2906,
                                                           returns/sets g.rng */
extern void enemy_alive_scan(void);                    /* enemies.c 0x2467 */
extern void shooter_promote(uint8_t dy_abs, uint8_t dx_abs,
                             uint8_t loop_b, obj_rec *candidate); /* enemies.c 0x24EC */
extern void wall_track_steer(obj_rec *o);               /* enemies.c 0x283F */
extern void heartbeat_sound(uint8_t code);              /* enemies.c 0x2DF6 */
extern uint8_t omega_vecram[0x2000];   /* dvg.c - WALL_FLASH_DECAY
                                          writes the wall list's
                                          intensity nibbles */
extern void spawn_mine_shot(obj_rec *o);                /* enemies.c 0x25EB */
extern void enemy_fire_gate(obj_rec *o);                /* enemies.c 0x1BEB */

/* ---- object flags0/flags1 bits ------------------------------------------
 * Bits documented in omega_state.h are not repeated here. The following
 * are inferred from which of the 5 ROM templates (0x2F50/67/7E/95/AC) set
 * them and from how OBJ_PHYSICS/WALL_BOUNCE/COLLIDE_SCAN test them; not
 * independently confirmed beyond that.
 */
#define OBJ_F0_THRUST   0x01 /* flags0 bit0: thrust active this frame (ship:
                                 set by FRAME_ENGINE on player thrust input,
                                 same RAM cell as obj_rec.flags0 for obj 1) */
#define OBJ_F0_FRICTION 0x08 /* flags0 bit3: apply friction (set on the
                                 ship/droid templates only)                */
#define OBJ_F0_WALLCOLL 0x40 /* flags0 bit6: participates in WALL_BOUNCE
                                 (ship/droid/shot templates; NOT set on the
                                 command-ship or inert templates)          */
#define OBJ_F0_ACTIVE   0x80 /* flags0 bit7: object alive/active          */

#define OBJ_F1_SHOT       0x01 /* flags1 bit0: ballistic/shot-mine class -
                                   destroyed rather than bounced on a wall
                                   hit (set only on the shot/mine template) */
#define OBJ_F1_SLOTASSIGN 0x08 /* flags1 bit3: one-shot "slot assigned" -
                                   see obj_expire_slot()                   */
#define OBJ_F1_SHOOTFLAG  0x10 /* flags1 bit4: tested alongside a scanned
                                   candidate's flags0 bit4 to gate
                                   SHOOTER_PROMOTE; exact semantics belong
                                   to enemies.c                            */
#define OBJ_F1_DYING      0x20 /* flags1 bit5: "dying" - set on the inert
                                   template; gates FRAME_SYNC_DISP's
                                   collision dispatch (dying objects skip
                                   straight to physics, no collide_scan)   */

/* ---- 8.8-style position/velocity high-byte helpers ----------------------
 * posx/posy/velx/vely are int16_t with the high byte the integer 8-bit
 * screen coordinate/velocity and the low byte a sub-pixel accumulator,
 * exactly like the ROM (CONVENTIONS rule 7).
 */
#define OBJ_HI(v)        ((int8_t)((uint16_t)(v) >> 8))
#define OBJ_LO(v)         ((uint8_t)((uint16_t)(v) & 0x00FFu))
#define OBJ_SET_HI(v, b) ((v) = (int16_t)(((uint16_t)(v) & 0x00FFu) | \
                                           ((uint16_t)(uint8_t)(b) << 8)))

/* ---- object templates: ROM 0x2F50-0x2FC2 (23 bytes each) ----------------
 * Field order matches obj_rec: flags0,flags1,velx(lo,hi),vely(lo,hi),
 * posx(lo,hi),posy(lo,hi),angle,b11,b12,b13,b14,aim,radx,rady,b18,timer,
 * b20,slot(lo,hi).
 */
static const uint8_t OBJ_TMPL_SHIP[23] = {    /* ROM 0x2F50 */
    0xF8, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF5, 0x80, 0xED,
    0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x04, 0x05, 0x00,
    0x00, 0xA1, 0x30
};
static const uint8_t OBJ_TMPL_DROID[23] = {   /* ROM 0x2F67 */
    0xF9, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x40,
    0x20, 0x00, 0x00, 0x03, 0x00, 0x00, 0x04, 0x04, 0x0C, 0x00,
    0x00, 0x04, 0xCA
};
static const uint8_t OBJ_TMPL_SHOT[23] = {    /* ROM 0x2F7E */
    0xC0, 0x01, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x19, 0x00, 0x00, 0x00, 0x01, 0x01, 0x0C, 0x00,
    0x00, 0xF8, 0xF8
};
static const uint8_t OBJ_TMPL_COMMAND[23] = { /* ROM 0x2F95 */
    0x80, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x03, 0x03, 0xFF, 0x00,
    0x00, 0xA7, 0xCB
};
static const uint8_t OBJ_TMPL_INERT[23] = {   /* ROM 0x2FAC */
    0x80, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x03, 0x00, 0x04, 0x00, 0x00, 0x00, 0x01, 0x01, 0x19, 0x00,
    0x00, 0xBA, 0xCB
};

/* quarter-wave sin/cos table: ROM 0x3021-0x3040, 16 (dx,dy) pairs indexed
 * by angle bits 0-3 (bits 4-5 select the quadrant fold in obj_physics). */
static const int8_t OBJ_QWAVE[16][2] = {
    {  0, 32 }, {  3, 31 }, {  6, 31 }, {  9, 30 },
    { 12, 29 }, { 15, 28 }, { 18, 26 }, { 20, 25 },
    { 22, 22 }, { 25, 20 }, { 26, 18 }, { 28, 15 },
    { 29, 12 }, { 30,  9 }, { 31,  6 }, { 32,  0 }
};

/* ROM 0x2E8A object table: obj# -> which template. Matches the 32-entry
 * record/template/dlist_slot table (0x2E90-0x2F49) exactly. */
static const uint8_t *obj_template_for_index(int idx)
{
    if (idx == OBJ_SHIP)                          return OBJ_TMPL_SHIP;
    if (idx >= OBJ_DROID_FIRST && idx <= OBJ_DROID_LAST) return OBJ_TMPL_DROID;
    if (idx >= OBJ_SHOT_FIRST  && idx <= OBJ_SHOT_LAST)  return OBJ_TMPL_SHOT;
    return OBJ_TMPL_COMMAND;
}

/* Field-by-field template unpack (avoids relying on obj_rec having no
 * compiler padding for a raw 23-byte memcpy). */
static void obj_apply_template(obj_rec *o, const uint8_t *t)
{
    o->flags0 = t[0];
    o->flags1 = t[1];
    o->velx   = (int16_t)((uint16_t)t[2]  | ((uint16_t)t[3]  << 8));
    o->vely   = (int16_t)((uint16_t)t[4]  | ((uint16_t)t[5]  << 8));
    o->posx   = (int16_t)((uint16_t)t[6]  | ((uint16_t)t[7]  << 8));
    o->posy   = (int16_t)((uint16_t)t[8]  | ((uint16_t)t[9]  << 8));
    o->angle  = t[10];
    o->b11    = t[11];
    o->b12    = t[12];
    o->b13    = t[13];
    o->b14    = t[14];
    o->aim    = t[15];
    o->radx   = t[16];
    o->rady   = t[17];
    o->b18    = t[18];
    o->timer  = t[19];
    o->b20    = t[20];
    o->shape_word = (uint16_t)((uint16_t)t[21] | ((uint16_t)t[22] << 8));
}

/* ===========================================================================
 * OBJ_SPAWN - ROM 0x291D
 * B=obj# (1-32). Copies that object's ROM template over its record, sets
 * record+19 (timer) = obj#, then for obj# < 14 (ship + droids) runs
 * per-class initial placement.
 * =========================================================================*/
void obj_spawn(uint8_t obj_num)
{
    int idx = obj_num;
    obj_rec *o = &g.obj[idx];

    obj_apply_template(o, obj_template_for_index(idx));
    o->timer = (uint8_t)idx;

    if (idx >= OBJ_SHOT_FIRST)
        return; /* shots/mines/command ships: template placement only */

    if (idx == OBJ_SHIP) {
        /* 0x2952-0x296B: posy(screen-Y) hi = random 0xB0-0xBF; if the
         * cocktail-flip gate is set, mirror posx(screen-X) hi in place.
         * Entropy input mirrors the ROM's stale accumulator, which at
         * this call site still holds obj# (from the "cp $0E" compare
         * just before). */
        uint8_t r = random_next((uint8_t)idx);
        uint8_t py = (uint8_t)((r & 0x0F) | 0xB0);
        OBJ_SET_HI(o->posy, py);
        if (g.rng_cache & 0x80) {
            int8_t h = OBJ_HI(o->posx);
            OBJ_SET_HI(o->posx, (uint8_t)(-h));
        }
        return;
    }

    /* 2..13: droid. b18 = 0x1E + (idx-1); posx(screen-X) placed along a
     * fixed spiral (start + delta*(idx-1)), mirrored/offset for cocktail;
     * posy(screen-Y) hi = random in [0x0C,0x59]. */
    int b = idx - 1;
    o->b18 = (uint8_t)(0x1E + b);

    int16_t start = 0x1900;
    int16_t delta = 0x0DC0;
    if (g.rng_cache & 0x80) {
        o->flags0 |= 0x02;              /* set 1,(ix+0): cocktail droid marker */
        start = (int16_t)-0x1900;       /* ROM: hl=0xE700 */
        delta = (int16_t)-0x0DC0;       /* ROM: de=0xF240 */
        o->angle = (uint8_t)(o->angle + 0x20);
    }
    o->posx = (int16_t)(start + delta * b); /* ROM copies both bytes of the
                                                spiral hl into (ix+6)/(ix+7) */

    /* entropy input on the first call mirrors the ROM's stale accumulator
     * (still holding b18's value from just above); on a rejected sample
     * the next call reuses the masked candidate, matching 'a' retaining
     * it across the loop (0x299B: res 7,a leaves the masked byte in a,
     * which the next "call RANDOM_NEXT" then adds as entropy). */
    uint8_t entropy = o->b18;
    uint8_t py;
    do {
        uint8_t r = random_next(entropy);
        py = (uint8_t)(r & 0x7F);
        entropy = py;
    } while (py < 0x0C || py >= 0x5A);
    OBJ_SET_HI(o->posy, py);
}

/* ===========================================================================
 * OBJ_KILL - ROM 0x29B2: clear the active bit only.
 * =========================================================================*/
void obj_kill(uint8_t obj_num)
{
    g.obj[obj_num].flags0 &= (uint8_t)~OBJ_F0_ACTIVE;
}

/* ===========================================================================
 * OBJ_DESTROY - ROM 0x27BF
 * Always operates on the *currently staged* object (g.obj_index / the
 * pointer passed in, which must be &g.obj[g.obj_index]) - this is how the
 * ROM implements "ramming/shooting something also destroys the attacker":
 * COLLIDE_SCAN deactivates the hit *candidate* directly via its own
 * record (res 7,(ix+0)), then calls OBJ_DESTROY, which wipes the
 * workspace/staged object (the attacker) with the inert template. Once
 * OBJ_STAGE_RUN copies the workspace back, the attacker (ship or shot)
 * is destroyed too.
 * =========================================================================*/
void obj_destroy(obj_rec *staged)
{
    int idx = g.obj_index;

    /* wipe with the inert template, preserving position (posx/posy hi
     * bytes only, matching the ROM's (iy-121)/(iy-119) save/restore) */
    int8_t saved_y = OBJ_HI(staged->posx);
    int8_t saved_x = OBJ_HI(staged->posy);
    obj_apply_template(staged, OBJ_TMPL_INERT);
    OBJ_SET_HI(staged->posx, (uint8_t)saved_y);
    OBJ_SET_HI(staged->posy, (uint8_t)saved_x);

    /* class dispatch: obj_index normally, except for the shot/mine band
     * (14-21) where the ROM re-reads obj_class (g.player_cur, see the
     * comment on that field in omega_state.h) instead. */
    uint8_t a = (uint8_t)idx;
    if (idx >= OBJ_SHOT_FIRST && idx < OBJ_SPECIAL_FIRST)
        a = g.player_cur;

    if (a >= OBJ_SPECIAL_FIRST) {
        /* kill_snd_cmdship */
        uint8_t snd = (g.kill_tier == 1) ? 0x03 : 0x11;
        sound_cmd_send(snd);
    } else if (a >= OBJ_SHOT_FIRST) {
        /* shot/mine band: no destroy sound (ROM jumps straight to ret) */
    } else if (a >= OBJ_DROID_FIRST) {
        /* kill_snd_droid */
        uint8_t snd;
        if (g.kill_tier == 3)      snd = 0x02;
        else if (g.kill_tier == 4) snd = 0x10;
        else                        snd = 0x15;
        sound_cmd_send(snd);
    } else {
        /* ship death (idx==1, or obj_class reload resolved to <2).
         * iy is 0x4080 (fixed by last_sound_cmd == (iy-24) == 0x4068), so
         * the ROM's (iy-94)/(iy-95) here are wave_pace_timer (0x4022) and
         * timer_seconds (0x4021) - the same cells frame.c and score.c use.
         * The timer_seconds=0 before the send is the ROM's standard idiom
         * (also at 0CFB, 137D, 23F2) for forcing a command past
         * SOUND_CMD_SEND's gate via the game_mode==3 path; the =5 after it
         * starts the death pause. */
        if (g.lives == 1)
            g.wave_pace_timer = 0x0A;    /* ROM 27FD: (iy-94) */
        g.timer_seconds = 0x00;          /* ROM 2801: (iy-95) */
        sound_cmd_send(0x00);
        staged->b12 = 0x0F; /* ROM: (iy-116) = workspace+18? no - workspace
                                offset 0x400C = +12 = b12 of the (now-inert)
                                staged record itself */
        sound_cmd_send(0x01);
        g.timer_seconds = 0x05;          /* ROM 2813: (iy-95) */
    }
}

/* ===========================================================================
 * APPLY_FRICTION - ROM 0x2148: v -= v/8 (arithmetic shift by 3, rounding
 * a zero shifted-magnitude up to 1 so nonzero v always decays).
 * =========================================================================*/
static int16_t apply_friction(int16_t v)
{
    if (v == 0)
        return 0;
    int16_t eighth = (int16_t)(v >> 3); /* arithmetic shift x3, matches sra/rr */
    if (eighth == 0)
        eighth = 1;
    return (int16_t)(v - eighth);
}

/* ===========================================================================
 * CLAMP_SPEED - ROM 0x2164: clamp to [-0x02BC, +0x02BC].
 * =========================================================================*/
static int16_t clamp_speed(int16_t v)
{
    if (v >= 0) {
        if (v > 0x02BC) return 0x02BC;
    } else {
        if (v < -0x02BC) return -0x02BC;
    }
    return v;
}

/* ===========================================================================
 * WALL_HIT_FX - ROM 0x20CE
 * Classifies which of the 19 wall segments (g.wall_fx[1..19]) was hit
 * from the staged object's position (h=posx hi/screen-X, l=posy hi/
 * screen-Y) and the axis flags WALL_BOUNCE has already set in
 * `bounce_flags` (bit6/bit5, mirroring ROM 0x4078), then sets that
 * segment to full brightness (0x96) and forces the "still glowing" meta
 * flag (wall_fx[0]) on.
 *
 * Invariant (confirmed by a live wall_fx dump): the ROM's set of
 * outcomes is exactly {1..8, 0x0A, 0x0D, 0x10, 0x13}, and every one of
 * those lands WALL_FLASH_DECAY's stride-6 write on a byte whose high
 * nibble is already an intensity, making the write idempotent. Any
 * other slot (e.g. 0x0B) would land on an SVEC's high byte instead,
 * turning 0xF0F0 into 0x70F0 - a long vector that swallows the
 * following word and derails the rest of the display list.
 * =========================================================================*/
static void wall_hit_fx(const obj_rec *o, uint8_t bounce_flags)
{
    int8_t  h = OBJ_HI(o->posx); /* (iy-121) 0x4007, screen-X */
    int8_t  l = OBJ_HI(o->posy); /* (iy-119) 0x4009, screen-Y */
    uint8_t a = bounce_flags;    /* 0x4078 */
    uint8_t e = 0;               /* 0x20D2 ld de,$0000 */

    g.wall_fx[0] = 0x80;         /* 0x20CE ld (iy+26),$80 */

    if (!(a & 0x40)) {                    /* 0x20DE bit 6,a */
        if (h < 0) {                      /* 0x20E2 bit 7,h */
            if (l < 0) {                  /* 0x20E6 bit 7,l */
                e = 0x01;                 /* 0x20EA */
                if (a & 0x20) e = 0x08;   /* 0x20F0 */
            } else {                      /* 0x20F4 */
                e = 0x02;
                if (a & 0x20) e++;        /* 0x20FA inc e */
            }
        } else {                          /* 0x20FD */
            if (l < 0) {                  /* 0x2101 */
                e = 0x06;
                if (a & 0x20) e++;        /* 0x2107 inc e */
            } else {                      /* 0x210A */
                e = 0x04;
                if (!(a & 0x20)) e++;     /* 0x2110: inc only when bit 5 clear */
            }
        }
    } else {                              /* 0x2113 */
        if (h < 0) {                      /* 0x2117 */
            e = 0x0A;
            if (l < 0) {                  /* 0x211D */
                if (a & 0x20) e = 0x13;   /* 0x2121 */
            } else {                      /* 0x2125 */
                if (a & 0x20) e = 0x0D;   /* 0x2129 */
            }
        } else {                          /* 0x212D */
            e = 0x10;
            if (l < 0) {                  /* 0x2133 */
                if (a & 0x20) e = 0x13;   /* 0x2137 */
            } else {                      /* 0x213B */
                if (a & 0x20) e = 0x0D;   /* 0x213F */
            }
        }
    }

    g.wall_fx[e] = 0x96;                  /* 0x2141-0x2145 */
}

/* ===========================================================================
 * WALL_BOUNCE - ROM 0x1F2A
 * Tentative-position test against the outer arena rectangle and inner
 * island, component reflection (velocity negation), velocity halving,
 * corner re-test. This is transliterated register-for-register from the
 * disassembly (goto-based) rather than re-derived into a cleaner
 * geometric model, per the file header's "uncertain" note - the branch
 * structure is dense enough that a literal mirror is the safest way to
 * guarantee fidelity to the original wall/island shape.
 *
 * Returns the ROM's 0x4078 "bounce flags" byte (bit5/6 axis selectors for
 * wall_hit_fx, bit7 = "actually hit something", bit0 borrowed from the
 * caller's flags1 test) so obj_physics can decide whether to self-destruct
 * a shot-class object.
 * =========================================================================*/
/* arithmetic >>1, matching the ROM's `sra high / rr low` pair */
static int16_t sra16(int16_t v)
{
    uint16_t u = (uint16_t)v;
    return (int16_t)((u >> 1) | (u & 0x8000));
}

static uint8_t wall_bounce(obj_rec *o)
{
    uint8_t bf;
    uint8_t d2, e2;

restart:                                        /* 0x1F2A */
    bf = 0;                                     /* 0x1F2C (iy-8) = 0 */
    {
        /* ---- Y axis: tentative posy + vely -------------------------- */
        uint8_t d = (uint8_t)OBJ_HI(o->posy);
        uint8_t h = (uint8_t)OBJ_HI((int16_t)(o->posy + o->vely));

        if (d & 0x80) {                         /* 0x1F4B */
            if ((d & 0x40) && !(h & 0x80) && !(h & 0x40))
                goto bounce_reflect_y;
        } else {                                /* 0x1F3C */
            if (!(d & 0x40) && (h & 0x80) && (h & 0x40))
                goto bounce_reflect_y;
        }

        /* 0x1F58: distance of the tentative position from screen centre */
        e2 = (uint8_t)(h - 0x80);
        if (h < 0x80) e2 = (uint8_t)(-(int)e2);

        /* ---- X axis: tentative posx + velx -------------------------- */
        {
            uint8_t b = (uint8_t)OBJ_HI(o->posx);
            uint8_t hx = (uint8_t)OBJ_HI((int16_t)(o->posx + o->velx));
            int crossed;

            d2 = (uint8_t)(hx - 0x80);
            if (hx < 0x80) d2 = (uint8_t)(-(int)d2);

            if (b & 0x80)                       /* 0x1F82 */
                crossed = (b & 0x40) && !(hx & 0x80) && !(hx & 0x40);
            else                                /* 0x1F74 */
                crossed = !(b & 0x40) && (hx & 0x80) && (hx & 0x40);

            if (crossed) {                      /* 0x1F8E */
                if (e2 >= 0x78) goto wb_1FDE;
                goto bounce_reflect_x;
            }
        }
    }

    /* 0x1F96 */
    if (e2 >= 0x78) {
        if (d2 < 0x7F) goto bounce_reflect_y;
        goto wb_1FDE;
    }

    /* 0x1FA2: a wall-following object gets the tighter 0x78 limit; every
     * other object uses 0x7F. Either way, falling past this test means
     * the object is inside the outer rectangle and the inner island has
     * to be considered. */
    if (o->flags1 & 0x80) {
        if (d2 >= 0x78) goto bounce_reflect_x;
    } else {
        if (d2 >= 0x7F) goto bounce_reflect_x;
    }

    /* 0x1FB4: inner island */
    bf |= 0x40;
    if (d2 == 0x50) {                           /* 0x1FD3 */
        if (e2 == 0x21) goto wb_1FDE;
        if (e2 > 0x21)  return bf;
        goto bounce_reflect_x;
    }
    if (d2 > 0x50)      return bf;              /* 0x1FBC */
    /* 0x1FBF: d2 < 0x50 - e2 equal to or below 0x21 both fall to 0x1FC7 */
    if (e2 > 0x21)      return bf;
    {
        uint8_t a = (uint8_t)(e2 + 0x30);       /* 0x1FC7 */
        if (a == d2) goto wb_1FDE;
        if (a >  d2) goto bounce_reflect_y;
        goto bounce_reflect_x;
    }

wb_1FDE:
    /* 0x1FDE: position and velocity disagreeing in sign on one axis
     * means that axis is the one that ran out; if neither does, the
     * object took a corner and both components are swapped-and-negated
     * (a 90-degree deflection). */
    if (((uint8_t)OBJ_HI(o->posx) ^ (uint8_t)OBJ_HI(o->velx)) & 0x80)
        goto bounce_reflect_y;
    if (((uint8_t)OBJ_HI(o->posy) ^ (uint8_t)OBJ_HI(o->vely)) & 0x80)
        goto bounce_reflect_x;
    if (o->flags1 & OBJ_F1_SHOT)
        goto bounce_reflect_x_204E;
    {
        int16_t nx = (int16_t)(-o->velx);       /* 0x1FF8-0x2011 */
        int16_t ny = (int16_t)(-o->vely);
        o->vely = nx;
        o->velx = ny;
    }
    goto halve_retest;

bounce_reflect_y:                               /* 0x2017 */
    bf |= 0x20;
    if (o->flags1 & OBJ_F1_SHOT)
        goto bounce_reflect_x_204E;
    o->vely = (int16_t)(-o->vely);
    goto halve_retest;

bounce_reflect_x:                               /* 0x2030 */
    if (o->flags1 & OBJ_F1_SHOT) {
        wall_hit_fx(o, bf);                     /* 0x2045 */
        bf |= 0x80;
        return bf;                              /* ROM: jr 20CD (ret) */
    }
    o->velx = (int16_t)(-o->velx);
    goto halve_retest;

bounce_reflect_x_204E:                          /* 0x204E */
    wall_hit_fx(o, bf);
    bf |= 0x80;
    if ((uint8_t)OBJ_HI(o->posx) >= 0x7E)
        return bf;
    o->posx = (int16_t)(o->posx + o->velx);
    /* ROM 0x206F is `ld (obj_vely),hl` after computing posy+vely - it
     * writes the sum into the VELOCITY cell, not obj_posy. Preserved as
     * found; it only ever runs for shot-class objects, which are about
     * to be destroyed by obj_physics anyway. */
    o->vely = (int16_t)(o->posy + o->vely);
    return bf;

halve_retest:                                   /* 0x2074 */
    wall_hit_fx(o, bf);
    o->velx = sra16(o->velx);
    o->vely = sra16(o->vely);
    if (o->velx == -1) o->velx = 0;             /* 0x208D / 0x209A */
    if (o->vely == -1) o->vely = 0;
    if (o->velx == 0 && o->vely == 0)           /* 0x20AD */
        return bf;
    bf |= 0x80;
    if (o->flags0 & 0x10)                       /* 0x20BF bit 4 gates it */
        sound_cmd_send(0x08);
    goto restart;                               /* 0x20CA jp WALL_BOUNCE */
}

/* ===========================================================================
 * WALL_FLASH_DECAY - ROM 0x1868: fade the 19 wall segments by tick_delta.
 *
 * The ROM also rewrites each segment's display-list intensity nibble on
 * every pass (0x1884-0x18A2, into 0x81BB stride 6). That write is what
 * makes a struck wall flash and what leaves the outer border dark at
 * rest. The decay state lives in g.wall_fx[], and the nibble writes go
 * into omega_vecram exactly as the ROM makes them, so the DVG walker
 * draws the result (see the file-header note on WALL_FLASH_DECAY).
 * =========================================================================*/
void wall_flash_decay(void)
{
    uint8_t  any_glow = 0;
    uint8_t* de = &omega_vecram[0x1BB];   /* ROM: ld de,$81BB */
    int      b  = 0x13;                   /* ROM: ld b,$13    */
    int      i;

    for (i = 1; i <= 19; i++, de += 6, b--) {   /* 0x18A3 six incs, djnz */
        uint8_t v = g.wall_fx[i];               /* ROM: hl = 0x409B.. */
        uint8_t nv;
        int     borrow;

        if (v == 0)                     continue;   /* 0x1872 */
        if (!(g.xlife_flags & 0x40))    continue;   /* 0x1878 */

        borrow = (v < g.tick_delta);                /* 0x187A sub c */
        nv = borrow ? (uint8_t)0 : (uint8_t)(v - g.tick_delta);
        g.wall_fx[i] = nv;                          /* 0x187F ld (hl),a */

        /* 0x1880 jr c / 0x1882 jr z -> the segment stopped glowing this
         * pass and takes the rest-intensity write below. Otherwise it is
         * still counting down, and bit 4 of the countdown alternates the
         * nibble between bright and the rest value every 16 counts -
         * that alternation IS the flash. */
        if (!borrow && nv != 0) {
            any_glow = 1;                           /* 0x1884 set 6,(iy+26) */
            if (!(nv & 0x10)) {                     /* 0x1888 bit 4,a */
                *de = (uint8_t)((*de & 0x0F) | 0xC0);   /* 0x188C */
                continue;
            }
        }
        /* 0x1893: a = b; cp $0B */
        if (b >= 0x0B)
            *de = (uint8_t)(*de & 0x0F);            /* 0x1898 intensity 0 */
        else
            *de = (uint8_t)((*de & 0x0F) | 0x70);   /* 0x189D intensity 7 */
    }

    g.wall_fx[0] = (uint8_t)(any_glow ? 0x80 : 0x00);   /* 0x18AC-0x18B8 */
}

/* ===========================================================================
 * COLLIDE_SCAN - ROM 0x2199
 * Scans `count` object-table entries starting at `start_idx` against the
 * staged object `staged`, bbox test using the max of each side's radii,
 * on hit: inline-kill the candidate, run wave/shooter bookkeeping,
 * kill_tier + score_add(), then obj_destroy() the *staged* (attacker)
 * object (see the obj_destroy() comment for why).
 * `class_seed` mirrors the ROM's (iy-2)/obj_class parameter set by the
 * caller before each call; per-miss it is incremented (g.player_cur),
 * matching the ROM even though the reason a caller-supplied class seed
 * plus a miss-count should feed OBJ_DESTROY's sound dispatch is not
 * understood (see file header).
 * =========================================================================*/
void collide_scan(obj_rec *staged, int start_idx, int count, uint8_t class_seed)
{
    g.player_cur = class_seed;

    for (int n = 0; n < count; n++) {
        int idx = start_idx + n;
        if (idx < 1 || idx > OBJ_COUNT)
            continue;
        obj_rec *cand = &g.obj[idx];

        if (!(cand->flags0 & OBJ_F0_ACTIVE))
            goto miss;
        if (cand->flags1 & OBJ_F1_DYING)
            goto miss;
        if (!(cand->flags1 & 0x02)) {
            /* flags1 bit1 clear: gate on XOR of staged flags1 bit6 */
            if (!(((cand->flags1 ^ staged->flags1) & 0x40)))
                goto miss;
        }

        {
            int8_t dx = (int8_t)(OBJ_HI(cand->posx) - OBJ_HI(staged->posx));
            if (dx < 0) dx = (int8_t)-dx;
            int8_t dy = (int8_t)(OBJ_HI(cand->posy) - OBJ_HI(staged->posy));
            if (dy < 0) dy = (int8_t)-dy;

            if ((staged->flags1 & 0x10) && (cand->flags0 & 0x10)) {
                uint8_t loop_b = (uint8_t)(count - n);
                shooter_promote((uint8_t)dy, (uint8_t)dx, loop_b, cand);
            }

            uint8_t rx = cand->radx;
            if (rx < staged->radx) rx = staged->radx;
            if ((uint8_t)dx >= rx)
                goto miss;

            uint8_t ry = cand->rady;
            if (ry < staged->rady) ry = staged->rady;
            if ((uint8_t)dy >= ry)
                goto miss;
        }

        /* hit confirm (cs_hit_confirm, 0x2210-0x2250). The kill below
         * only happens when the staged (attacker) object's flags1 bit6
         * is set (0x2210) or the candidate's flags1 bit1 is clear
         * (0x2216). Otherwise NOTHING dies: if the staged object is
         * shot-capable (flags0 bit4, 0x221C) with its b14 fire timer
         * expired (0x2223), the candidate is mutated in place - b13=2,
         * rady=6, shape word 0xCBAF (0x222A-0x2249; this is the command
         * ship turning into the death ship - the dlist entry word5
         * write at 0x2246 is rebuilt from the record by dvg.c per rule
         * 5) - and in every non-kill case the scan simply ENDS (0x224C
         * and 0x224F both jump to the shared ret at 0x2334). The
         * non-kill exit matters: SPAWN_SPECIAL (0x27B0) places a fresh
         * special at the spawning droid's own position, so falling
         * through to the kill here would let the droid collide-kill it
         * immediately. */
        if (!(staged->flags1 & 0x40) && (cand->flags1 & 0x02)) {
            if (!(staged->flags0 & 0x10) || staged->b14 != 0)
                return;         /* 0x224F: no kill, no score, scan over */
            cand->b13 = 0x02;                       /* 0x222A */
            cand->rady = 0x06;                      /* 0x222E */
            cand->shape_word = 0xCBAF;              /* 0x2232-0x2238 */
            return;             /* 0x224C: jp cs_kill_object_2334 (ret) */
        }

        /* kill: deactivate the candidate, then score/destroy-attacker */
        cand->flags0 &= (uint8_t)~OBJ_F0_ACTIVE;
        cand->timer = 0;

        {
            /* cs_kill_object 0x225C-0x22D0: pick a kill_tier, with a
             * "designated shooter killed" bonus path (tier 4/5, a
             * HEARTBEAT_SOUND(2) cue, and - conditionally - deactivating
             * all 4 photon-mine slots 14-17 as a chain-clear bonus). */
            int is_droid_range = (g.obj_index < OBJ_SHOT_FIRST); /* idx<14 */
            uint8_t cls = is_droid_range ? (uint8_t)g.obj_index : g.player_cur;
            uint8_t tier;

            if (cls == g.shooter_a || cls == g.shooter_b) {
                /* idx>=14 gates on the candidate's flags1 bit7; idx<14
                 * (ship/droid) gates on the staged object's flags1 bit7 */
                int gated = is_droid_range ? ((staged->flags1 & 0x80) != 0)
                                            : ((cand->flags1 & 0x80) != 0);
                if (gated) {
                    tier = 4;
                } else {
                    heartbeat_sound(2);
                    tier = 5;
                    if (g.shooter_state & 0x01) {
                        for (int m = 14; m <= 17; m++)
                            g.obj[m].flags0 &= (uint8_t)~OBJ_F0_ACTIVE;
                    } else if (g.xlife_flags & 0x01) {
                        tier = 4;
                    } else {
                        for (int m = 14; m <= 17; m++)
                            g.obj[m].flags0 &= (uint8_t)~OBJ_F0_ACTIVE;
                    }
                }
            } else {
                tier = (staged->b13 > cand->b13) ? staged->b13 : cand->b13;
            }
            g.kill_tier = tier;
            if (g.kill_tier != 0)
                score_add(g.kill_tier);
        }

        if (staged->flags1 & 0x01) {
            OBJ_SET_HI(staged->posx, (uint8_t)OBJ_HI(cand->posx));
            OBJ_SET_HI(staged->posy, (uint8_t)OBJ_HI(cand->posy));
        }

        obj_destroy(staged);
        staged->b11 = (staged->flags0 & 0x10) ? 0x10 : 0x00;

        {
            uint8_t cls = (uint8_t)g.obj_index;
            if (g.obj_index == OBJ_SHIP || g.obj_index >= OBJ_SHOT_FIRST)
                cls = g.player_cur;
            /* 0x2307-0x2322: killing the designated shooter re-paces the
             * wave - 7 s pace timer + difficulty 5, tightened to 2/2 in
             * the late-game state (xlife_flags bit0, set by
             * ENEMY_ALIVE_SCAN 0x24DB once time_played[1] >= 3). */
            if (cls == g.shooter_a) {
                g.shooter_a = 0;                  /* 0x230C */
                g.wave_pace_timer  = 7;           /* 0x2310 (iy-94) */
                g.difficulty_param = 5;           /* 0x2314 (iy-4)  */
                if (g.xlife_flags & 0x01) {       /* 0x2318 bit 0,(iy-22) */
                    g.wave_pace_timer  = 2;       /* 0x231E */
                    g.difficulty_param = 2;       /* 0x2322 */
                }
            }
            if (cls == g.shooter_b) g.shooter_b = 0;   /* 0x2326-0x232B */
            if (cls < OBJ_SHOT_FIRST)
                enemy_alive_scan();
        }
        return;

    miss:
        g.player_cur++; /* ROM: inc (iy-2) - see file header note */
        continue;
    }
}

/* obj_physics is what FRAME_SYNC_DISP falls into at ROM 0x1B16, and
 * enemy_fire_gate/move_mode_dispatch are what it falls into in turn. */
static void obj_physics(obj_rec *o);
static void move_mode_dispatch(obj_rec *o, int idx);

/* ===========================================================================
 * Beam-settling delay and the position tracker it runs off.
 *
 * 0x4075/0x4076 hold the high bytes of the position of the last object
 * OBJECTS_UPDATE looked at, and every object's display-list entry gets a
 * word3 whose opcode nibble buys the DVG time to slew the beam from
 * there to here - further to travel, longer to settle. Both the
 * stage-run path (move_mode_dispatch 0x1EE5-0x1F1A) and the two
 * cheap paths (obj_proximity_upd 0x1A02-0x1A35) compute it identically,
 * so it lives here once instead of three times.
 *
 * The arithmetic is unsigned throughout, as the ROM's sub/neg/cp are:
 * signed 8-bit arithmetic disagrees with the hardware at a magnitude of
 * exactly 0x80.
 * =========================================================================*/
static uint8_t s_track_x = 0x80;   /* 0x4075, seeded by OBJECTS_UPDATE */
static uint8_t s_track_y = 0x80;   /* 0x4076                            */

#define OBJ_HIU(v) ((uint8_t)((uint16_t)(v) >> 8))

static uint8_t abs_diff8(uint8_t a, uint8_t b)
{
    return (uint8_t)(a >= b ? a - b : b - a);   /* sub / jr nc / neg */
}

/* ROM 0x1A41 obj_track_pos: remember this object and compute nothing. */
static void obj_track_pos(const obj_rec *o)
{
    s_track_x = OBJ_HIU(o->posx);
    s_track_y = OBJ_HIU(o->posy);
}

static uint8_t obj_settle_update(const obj_rec *o)
{
    uint8_t dx = abs_diff8(s_track_x, OBJ_HIU(o->posx));
    uint8_t dy = abs_diff8(s_track_y, OBJ_HIU(o->posy));
    uint8_t m  = (dy >= dx) ? dy : dx;          /* cp / jr c -> max     */
    uint8_t bucket = 0x90;
    int i;

    /* rlc until a set bit falls out, dropping 0x10 per shift: eight
     * distance tiers from 0x90 (adjacent) down to 0x10 (across the
     * screen, or zero distance - the loop runs out). */
    for (i = 0; i < 8; i++) {
        uint8_t carry = (uint8_t)((m & 0x80) ? 1 : 0);
        m = (uint8_t)((m << 1) | carry);
        if (carry)
            break;
        bucket = (uint8_t)(bucket - 0x10);
    }

    obj_track_pos(o);
    return (uint8_t)(bucket - o->b11);          /* 0x1A28 / 0x1F0B */
}

/* ===========================================================================
 * FRAME_SYNC_DISP's collision dispatch (ROM 0x1A59-0x1B15, minus the
 * dropped watchdog/VG_RESTART_FRAME hardware calls - see file header).
 * Decides which band of the object table COLLIDE_SCAN should run against
 * the staged object, then falls into obj_physics().
 *
 * The ROM passes COLLIDE_SCAN a pointer into the 6-byte-per-object
 * record/template/dlist table at 0x2E90, so ix=0x2E96 is object 2 and
 * ix=0x2F0E is object 22; those are the start_idx values below.
 * =========================================================================*/
static void obj_collide_dispatch(obj_rec *o, int idx)
{
    /* 0x1A62: reload this object's update timer,
     * timer = (tick_244hz - tick_prev) + b18. b18 is the object's own
     * update period, which is where the per-object motion stagger comes
     * from (CONVENTIONS rule 6's "5/6-tick wobble"). */
    o->timer = (uint8_t)((uint8_t)(g.tick_244hz - g.tick_prev) + o->b18);

    /* 0x1A6E: a dying object skips collision entirely */
    if (o->flags1 & OBJ_F1_DYING)
        goto fsd_slot_init_once;

    if (idx == OBJ_SHIP) {                    /* 0x1A78 */
        /* 0x1AA4 fsd_ship_object: rearm the closest-pair search, but
         * only when no shooter is designated and the wave is under way. */
        if (g.shooter_a != 0)                 /* 0x1AA4 (wave_left_a) */
            goto fsd_droids_collide;
        if (g.wave_num == 6) {                /* 0x1AAA */
            if (g.wave_pace_timer != 0)       /* 0x1AB1 ($4022) */
                goto fsd_droids_collide;
        } else {
            if (--g.difficulty_param != 0)    /* 0x1AB9 dec (iy-4) */
                goto fsd_droids_collide;
        }
        /* 0x1ABE fsd_reset_pairmin */
        o->flags1 |= 0x10;                    /* set 4,(iy-127) */
        g.pair_min = 0xFFFF;                  /* (iy+0)/(iy+1) = FF FF */

    fsd_droids_collide:                       /* 0x1ACA */
        collide_scan(o, 2, 0x1F, 0x02);
    } else if (idx < 0x0E) {                  /* 0x1A7C: objects 2-13   */
        collide_scan(o, 22, 0x0B, 0x0E);      /* 0x1AD8 fsd_mines_collide */
    } else if (idx >= 0x16) {                 /* 0x1A80: 22 and up      */
        goto fsd_slot_init_once;
    } else if (idx < 0x12) {                  /* 0x1A84: 14-17          */
        goto fsd_slot_init_once;              /* 0x1AE6                 */
    } else {                                  /* 0x1A88: shots 18-21    */
        collide_scan(o, 2, g.wave_num, 0x02);
        collide_scan(o, 22, 0x0B, 0x16);
    }

fsd_slot_init_once:
    /* 0x1AF9: one-shot per object. The ROM body here computes this
     * object's JMPL word ((entry - 0x847F) >> 1) and writes it into the
     * display-list entry; dvg.c rebuilds that from the record every
     * frame now, so only the latch bit remains. */
    if (!(o->flags1 & 0x04))
        o->flags1 |= 0x04;

    /* The ROM has no calls left from here on: OBJ_PHYSICS falls out of
     * its bottom (and out of both of its early exits, 0x1BB9 and 0x1BD3)
     * into enemy_fire_gate at 0x1BEB, which falls out of its own bottom
     * into move_mode_dispatch at 0x1DB4. Those three are one straight
     * run through per staged object, so all three are called here
     * unconditionally - obj_physics()'s early `return`s only skip the
     * rest of obj_physics, exactly like the ROM's jumps. */
    obj_physics(o);                           /* 0x1B16 */
    enemy_fire_gate(o);                       /* 0x1BEB */
    move_mode_dispatch(o, idx);               /* 0x1DB4 */
}

/* ===========================================================================
 * OBJ_PHYSICS - ROM 0x1B16..0x1BEA (thrust, clamp, friction, wall bounce,
 * position integration). Stops exactly where the ROM's single routine
 * would fall into enemy_fire_gate (0x1BEB).
 * =========================================================================*/
static void obj_physics(obj_rec *o)
{
    /* 0x1B16 `bit 0,(iy-128)` thrust flag, AND 0x1B1C `bit 7,(iy-21)`.
     * That second gate is wave_flags bit 7 - "the wave has started",
     * raised by MAINLOOP 0x0FB2 when the 0x4023 countdown expires;
     * without it every object would accelerate on every pass and run
     * straight to the CLAMP_SPEED ceiling of 0x2BC. */
    if ((o->flags0 & OBJ_F0_THRUST) && (g.wave_flags & 0x80)) {
        uint8_t ang, qidx, quad;
        int8_t  e, d;
        int16_t v;

        if (o->flags1 & 0x80)          /* 0x1B22: wall-follow steers first */
            wall_track_steer(o);

        /* 0x1B29-0x1B58: one 16-bit entry per 1/16 turn out of ROM
         * 0x3021, then the quadrant sign flips. e is the low byte, d the
         * high byte; d drives velx and e drives vely. */
        ang = o->angle;
        qidx = ang;
        if (ang & 0x10) qidx ^= 0x0F;
        qidx &= 0x0F;
        e = OBJ_QWAVE[qidx][0];
        d = OBJ_QWAVE[qidx][1];

        quad = (uint8_t)(ang & 0x30);
        if (quad != 0x00 && quad != 0x30) d = (int8_t)(-d);
        if (ang & 0x20)                   e = (int8_t)(-e);

        /* `ld c,d / sign-extend into b / sla c / add hl,bc` == += d * 2 */
        v = (int16_t)(o->velx + ((int16_t)d << 1));
        o->velx = clamp_speed(v);
        v = (int16_t)(o->vely + ((int16_t)e << 1));
        o->vely = clamp_speed(v);

        /* 0x1B83-0x1B8F: a pooled object that is NOT wall-following gets
         * its thrust flag dropped here, so the push is a one-shot. */
        if (!(o->flags1 & 0x80) && (o->flags1 & 0x40))
            o->flags0 &= (uint8_t)~OBJ_F0_THRUST;
    }

    /* 0x1B93-0x1BB2. Note the fall-through: when the object is not
     * wall-following the ROM clears the friction flag AND STILL applies
     * friction this pass. */
    if (o->flags0 & OBJ_F0_FRICTION) {
        if (!(o->flags1 & 0x80))
            o->flags0 &= (uint8_t)~OBJ_F0_FRICTION;   /* 0x1B9F */
        o->velx = apply_friction(o->velx);
        o->vely = apply_friction(o->vely);
    }

    /* 0x1BB5-0x1BB9: when flags0 bit 6 (wall-collide) is clear the ROM
     * leaves for enemy_fire_gate WITHOUT integrating position - such
     * objects hold still. */
    if (!(o->flags0 & OBJ_F0_WALLCOLL))
        return;

    {
        uint8_t bf = wall_bounce(o);
        if ((bf & 0x80) && (o->flags1 & OBJ_F1_SHOT)) {
            /* ROM 0x1BCA-0x1BCD: `ld a,(obj_index) / ld (obj_class),a`.
             * A shot dying against a wall hit nothing, so the ROM points
             * obj_class at the shot's OWN index before calling OBJ_DESTROY.
             * That lands in the 0x0E-0x15 shot/mine band, whose dispatch
             * arm (0x27EE-0x27F0) returns without a sound - a miss is
             * silent on hardware. Without this store obj_class would
             * still hold COLLIDE_SCAN's per-miss running count. */
            g.player_cur = (uint8_t)g.obj_index;
            obj_destroy(o);
            return; /* ROM: jr enemy_fire_gate */
        }
    }

    /* 0x1BD5 */
    o->posx = (int16_t)(o->posx + o->velx);
    o->posy = (int16_t)(o->posy + o->vely);

    /* ROM falls through into enemy_fire_gate (0x1BEB) here - enemies.c's
       continuation, not translated in this file (see file header). */
}

/* ===========================================================================
 * move_mode_dispatch - ROM 0x1DB4-0x1F28, the tail of every staged
 * object's update: pick the shape it draws with, run its two countdowns,
 * turn a raised fire-request into a real shot, and fill in its
 * display-list entry.
 *
 * Two stretches of it are executed elsewhere and are deliberately not
 * repeated here:
 *   0x1DB4-0x1E6D  shape-word selection -> dvg.c dvg_obj_shape_select().
 *                  It has to write into vector RAM (the ship arm patches
 *                  the hull/flame sub-list at 0x818A), which is dvg.c's
 *                  business; it is called from here so that its side
 *                  effects - the explosion and droid animation counters,
 *                  and the ship's aim_angle - happen once per stage-run
 *                  at the object's own update rate, and ahead of the
 *                  0x1E9B spawn gate that reads aim_angle.
 *   0x1EA2-0x1EE4  the entry's JMPL/LABS/scale words -> dvg.c
 *                  dvg_build_object_list(), which rebuilds the whole
 *                  object region from the records at draw time instead
 *                  of writing it a field at a time here.
 * =========================================================================*/
static void move_mode_dispatch(obj_rec *o, int idx)
{
    /* 0x1DB4-0x1E6D, via dvg.c. The word lands in word 5 of the entry;
     * the ROM writes it to (ix+10)/(ix+11) at 0x1E5B or 0x1E63. */
    g.obj_shape[idx] = dvg_obj_shape_select(o);

    /* 0x1E6F: b12 is a lifetime countdown; this is its only decrement.
     * On expiry the object deactivates (a photon shot is stamped with
     * 0x19 by SPAWN_MINE_SHOT). */
    if (o->b12 != 0) {
        if (--o->b12 == 0)
            o->flags0 &= (uint8_t)~OBJ_F0_ACTIVE;   /* 0x1E7A */
    }

    /* 0x1E7E: b14 is the fire timer. On expiry it reloads with a
     * wave-scaled interval (shorter as wave_num climbs) and raises the
     * fire-request bit, which is how an enemy shoots at all: the gate
     * below is the same one the player's fire button feeds. */
    if (o->b14 != 0) {
        if (--o->b14 == 0) {
            o->b14 = (uint8_t)(0x2C - 3 * g.wave_num);   /* 0x1E89 */
            o->flags0 |= 0x04;                           /* 0x1E97 */
        }
    }

    /* 0x1E9B */
    if (o->flags0 & 0x04)
        spawn_mine_shot(o);

    /* 0x1EE5-0x1F1A: this object's beam-settling delay, then it becomes
     * the reference position for whichever object comes next. */
    g.obj_settle[idx] = obj_settle_update(o);

    /* 0x1F1D-0x1F21 kicks the DVG if it is idle - the host's frame loop
     * owns that (CONVENTIONS rule 5). */

    o->flags1 |= 0x04;                                   /* 0x1F24 */
}

/* ===========================================================================
 * obj_expire_slot - ROM 0x199B (inactive-object one-shot init). The real
 * ROM work here (converting the display-list slot address to a word
 * offset and writing it back) is a DVG rendering artifact, dropped per
 * rule 5/6; only the one-shot "slot assigned" flag is preserved.
 * =========================================================================*/
static void obj_expire_slot(obj_rec *o, int timer_borrowed)
{
    if (!timer_borrowed)
        return;
    if (o->flags1 & OBJ_F1_SLOTASSIGN)
        return;
    o->flags1 |= OBJ_F1_SLOTASSIGN;
    /* dlist slot-offset write dropped - see file header */
}

/* ===========================================================================
 * OBJECTS_UPDATE / OBJ_UPDATE_LOOP - ROM 0x1965-0x1A58
 * Iterates all 32 objects once per pass. Per object: timer -= tick_delta
 * (staggering which objects get a full physics/collision pass this call -
 * CONVENTIONS rule 6's "5/6-tick motion wobble" quirk lives here, so this
 * is preserved exactly rather than simplified to "always update").
 * =========================================================================*/
void objects_update(void)
{
    /* ROM 0x1969/0x196D seed the 0x4075/0x4076 position tracker with
       0x80 at the top of every pass; the cells themselves are file
       statics (see obj_settle_update) because move_mode_dispatch reads
       and writes them too. ROM 0x4074 bit7: "has at least one full
       stage-run happened yet this pass" - stays local. */
    int have_run = 0;

    s_track_x = 0x80;
    s_track_y = 0x80;

    for (int idx = 1; idx <= OBJ_COUNT; idx++) {
        obj_rec *o = &g.obj[idx];

        uint8_t before;

        /* ROM 0x1985 `inc (iy-3)`: publish which object is being staged.
           enemy_fire_gate matches it against shooter_a/shooter_b and
           OBJ_DESTROY turns it into a collision class. */
        g.obj_index = (uint8_t)idx;

        before = o->timer;
        o->timer = (uint8_t)(before - g.tick_delta);
        int borrowed = (o->timer > before); /* wrapped -> "carry" (borrow) */

        if (!(o->flags0 & OBJ_F0_ACTIVE)) {
            obj_expire_slot(o, borrowed);
            continue;
        }

        if (!borrowed) {
            /* obj_proximity_upd (0x19FC), gated on have_run. ROM
             * 0x1A38-0x1A3C: the two `pop ix` step from the object
             * record to THIS object's display-list entry pointer, and
             * `ld (ix+7),h` writes the entry's byte 7 - word3's high
             * byte, the beam-settling delay (not a position). */
            if (!have_run) {
                obj_track_pos(o);              /* 0x1A41 obj_track_pos */
                continue;
            }
            g.obj_settle[idx] = obj_settle_update(o);
            continue;
        }

        /* borrowed: build_check -> either a full stage-run or, if the
           dlist-built bits say otherwise, still a proximity update */
        int full_run;
        if (!(o->flags1 & 0x02)) {
            full_run = 1;
        } else if (o->flags1 & 0x04) {
            full_run = 0;
        } else {
            full_run = 1;
        }

        if (!full_run) {
            if (!have_run) {
                obj_track_pos(o);              /* 0x1A41 obj_track_pos */
                continue;
            }
            g.obj_settle[idx] = obj_settle_update(o);   /* 0x1A3C */
            continue;
        }

        /* OBJ_STAGE_RUN: no literal stage/unstage needed (rule: pass the
           obj_rec* around) - just run the dispatch + physics directly. */
        obj_collide_dispatch(o, idx);
        have_run = 1;                          /* 0x19F6 set 7,(iy-12) */
    }
}

#ifdef OMEGA_WBTEST
/* Test hook for wbtest.c's differential comparison against the ROM's
 * own WALL_BOUNCE (0x1F2A), driven on the Z80 by
 * frametime --wallbounce-vectors. Not compiled into the game. */
uint8_t omega_test_wall_bounce(obj_rec *o) { return wall_bounce(o); }
void    omega_test_obj_physics(obj_rec *o) { obj_physics(o); }
#endif
