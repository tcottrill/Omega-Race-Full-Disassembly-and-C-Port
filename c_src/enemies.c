/* enemies.c - Omega Race C conversion: enemy AI / weapons module.
 *
 * Translates disasm/omega_main.asm 0x2186-0x2DF6 (DIFFICULTY_CALC,
 * ENEMY_ALIVE_SCAN, SHOOTER_PROMOTE, AIM_AT_SHIP + its MUL8X8/DIV_SLOPE
 * helpers, SPAWN_MINE_SHOT, SPAWN_SPECIAL, WALL_TRACK_STEER, RANDOM_NEXT,
 * HEARTBEAT_SOUND) plus the enemy fire-gate scheduling block at
 * 0x1BEB-0x1DB3 (enemy_fire_gate / quadrant_engage_chk). See
 * disasm/FUNCTIONS.md ("Enemy AI / weapons") and disasm/GAMEPLAY_NOTES.md
 * for the surrounding object-system context.
 *
 * Staging model note (see omega_state.h): the ROM runs all of this
 * against a *single* staged-object workspace at 0x4000-0x4016 (one
 * object processed at a time), so several RAM cells that read like
 * "per object" state are really shared scratch. 0x400F (aim) is NOT
 * shared scratch: it is the workspace copy of the currently staged
 * object's own aim byte - see the aim_at_ship comment. Where the ROM
 * instead reaches into a *specific other* object's persisted record
 * (e.g. ENEMY_ALIVE_SCAN seeding a bystander droid's aim byte), this
 * file writes obj_rec.aim on that object directly, matching the ROM's
 * absolute-address access.
 *
 * IY is fixed at 0x4080 for all of this code; every (iy+d) below is
 * annotated with the RAM cell it resolves to and the g./obj_rec field
 * that stands in for it.
 */
#include <stdlib.h>
#include "omega_state.h"

/* ---- cross-module externs (objects.c) ----------------------------------- */

extern void obj_spawn(uint8_t obj_num);   /* OBJ_SPAWN 0x291D: template -> g.obj[obj_num] */
extern void obj_kill(uint8_t obj_num);    /* OBJ_KILL  0x29B2 */
extern void sound_cmd_send(uint8_t cmd);  /* SOUND_CMD_SEND 0x2E04 */

/* ---- forward declarations (see definitions below for ROM addr/role) ----- */

uint8_t random_next(uint8_t entropy_arg);
void    difficulty_calc(void);
void    heartbeat_sound(uint8_t code);
void    aim_at_ship(obj_rec* o);
void    enemy_alive_scan(void);
void    shooter_promote(uint8_t dy_abs, uint8_t dx_abs, uint8_t loop_b, obj_rec* candidate);
void    spawn_mine_shot(obj_rec* o);
void    spawn_special(obj_rec* o);
void    wall_track_steer(obj_rec* o);
void    enemy_fire_gate(obj_rec* o);

/* ---- helpers -------------------------------------------------------------
 * "posx" is screen X and "posy" is screen Y (rule 7 / CONVENTIONS.md;
 * dvg.c's entry builder writes LABS X from +6 and LABS Y from +8,
 * verified byte-exact against a hardware dump). The ROM always works
 * with just the high (integer) byte of the position when doing
 * screen-space geometry.
 */
#define SCREEN_X(o) ((uint8_t)((o).posx >> 8))
#define SCREEN_Y(o) ((uint8_t)((o).posy >> 8))

/* ---- ROM data tables ------------------------------------------------------ */

/* ROM 0x3041-0x305D: aim slope->angle bucket thresholds. The table
 * actually holds 16 words (0x3041-0x3060); AIM_AT_SHIP's compare loop
 * is hard-bounded to 15 entries (a=$0F), so the 16th word (0x000C) is
 * never read - dead ROM data, dropped here. */
static const uint16_t AIM_ANGLE_BUCKETS[15] = {
    0x1400, 0x0699, 0x040A, 0x02C3, 0x0224, 0x01AF, 0x0155, 0x011A,
    0x00E7, 0x00C0, 0x0098, 0x0077, 0x005D, 0x003F, 0x0026
};

/* ROM 0x3021-0x3040: quarter-wave sin/cos pairs, indexed by a 4-bit
 * (angle&0xF, octant-folded) index; column 0 = "dy"-ish (feeds
 * posy/vely), column 1 = "dx"-ish (feeds posx/velx) component used by
 * SPAWN_MINE_SHOT. */
static const int8_t QUARTER_WAVE[16][2] = {
    {0, 32}, {3, 31}, {6, 31}, {9, 30}, {12, 29}, {15, 28}, {18, 26}, {20, 25},
    {22, 22}, {25, 20}, {26, 18}, {28, 15}, {29, 12}, {30, 9}, {31, 6}, {32, 0}
};

/* SPAWN_MINE_SHOT slot pools, transcribed from the pointer arithmetic at
 * ROM 0x2EF1 (photon/4-slot) and 0x2F09 (vapor/8-slot): both pool
 * pointers land 1 byte into a 0x2E8A object-table entry (the record
 * high-byte field), and the scan walks *backward* through the table by
 * one entry (6 bytes) per step. That resolves to concrete object
 * numbers:
 *   photon (flags1 bit6 clear): objects 17,16,15,14
 *   vapor  (flags1 bit6 set):   objects 21,20,19,18 ...
 * QUIRK (verified against the bytes at 0x25ED/0x25F5): the scan-count
 * register C is hard-coded to 4 regardless of pool, *including* for the
 * nominally-8-slot vapor pool. So the vapor pool's other four object
 * slots (14-17) are never actually tried by this routine - only
 * objects 18-21 are ever candidates for a vapor mine drop. Preserved
 * exactly (CONVENTIONS rule 6).
 */
static const uint8_t MINE_POOL_PHOTON[4] = {17, 16, 15, 14};
static const uint8_t MINE_POOL_VAPOR[4]  = {21, 20, 19, 18};

/* SPAWN_SPECIAL 11-slot pool, ROM 0x2F4B -> object-table entry for
 * object 32, walked backward: objects 32,31,...,22. */
static const uint8_t SPECIAL_POOL[11] = {32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22};

/* WALL_TRACK_STEER per-quadrant steering constants, ROM 0x285A-0x2899
 * (track_dir=0) and 0x2884-0x2899 (track_dir=1). Indexed by
 * [track_dir][quadrant], quadrant = (Y>=0x80)*2 | (X>=0x80). h/l are
 * the two candidate heading angles (6-bit) for that wall octant; c
 * selects which of the two nested geometric tests below picks between
 * them. */
typedef struct { uint8_t h, l, c; } wall_entry_t;
static const wall_entry_t WALL_TABLE[2][4] = {
    { {0x1E, 0x10, 0}, {0x0E, 0x00, 1}, {0x2E, 0x20, 1}, {0x3E, 0x30, 0} },
    { {0x32, 0x00, 1}, {0x22, 0x30, 0}, {0x02, 0x10, 0}, {0x12, 0x20, 1} }
};

/* MUL8X8 (ROM 0x25AA): HL = D*E. The ROM's 8-cycle shift-add multiply
 * reduces to a plain C multiplication with the same result. */
static uint16_t mul8x8(uint8_t d, uint8_t e) {
    return (uint16_t)((uint16_t)d * (uint16_t)e);
}

/* DIV_SLOPE (ROM 0x25C1): HL ~= E*256/D, a 17-iteration restoring
 * binary divider on hardware.
 *
 * D == 0 is NOT undefined, and it is not rare: it happens every time an
 * enemy is exactly level with the ship on one axis. Tracing the ROM:
 * `ld a,d / cpl / ld e,a / ld d,$FF / inc de` forms -D, so D == 0 makes
 * DE == 0; `add hl,de` then never carries, every iteration restores HL,
 * and the 17 shifts walk BC's bits out of the top. The routine ends
 * `ld h,b / ld l,c`, so the result is BC - which after 17 left shifts is
 * zero. The ROM returns 0.
 *
 * Returning 0 (rather than saturating) matters: the result drives
 * AIM_AT_SHIP's bucket search (0x3041), and qec_1D6B copies aim_angle
 * straight into the object's heading, so a level enemy's bearing
 * depends on this exact value. */
static uint16_t div_slope(uint8_t d, uint8_t e) {
    if (d == 0) return 0;
    return (uint16_t)(((uint32_t)e << 8) / d);
}

/* Shared "shortest direction, +/-1 step" angle stepper used by both
 * AIM_AT_SHIP (0x258E-0x25A6) and WALL_TRACK_STEER (0x28EF-0x2902):
 * identical bit pattern in both ROM routines. */
static uint8_t step_angle_toward(uint8_t cur6, uint8_t target6) {
    int diff = (int)cur6 - (int)target6;
    if (diff == 0) return cur6;
    if (diff > 0) return (uint8_t)(cur6 + ((diff >= 0x20) ? 1 : -1));
    return (uint8_t)(cur6 + ((diff >= -0x20) ? 1 : -1));
}

/* Nested nearest-wall-octant geometric test, ROM 0x28AE-0x28E9.
 * dx/dy are |position-0x80| on each axis; c selects one of two
 * disjoint threshold trees. Returns 1 to pick the "l" constant of the
 * WALL_TABLE entry, 0 to keep "h". The exact geometric rationale for
 * these particular thresholds is not annotated in the ROM; transcribed
 * byte-for-byte. */
static int wall_region_use_l(int c_flag, uint8_t dx, uint8_t dy) {
    if (c_flag) {
        if (dx >= 0x58) {
            return (((uint8_t)(dx - 0x40)) < dy) ? 1 : 0;
        }
        return (dy < 0x21) ? 0 : 1;
    }
    if (dx < 0x21) return 0;
    if (dx >= 0x68) return 1;
    if (dy < 0x30) {
        if (dx >= 0x53) return 1;
        {
            uint8_t t = (uint8_t)(0x70 - dx);
            return (t >= dy) ? 0 : 1;
        }
    }
    return (((uint8_t)(dx - 0x10)) < dy) ? 0 : 1;
}

/* ============================================================================
 * RANDOM_NEXT (0x2906)
 *
 * Original Z80:
 *   add a,(iy-13)      ; A(entropy_arg) += rng            (iy-13 = 0x4073 = rng)
 *   ld (random_val),a  ; rng = entropy_arg + rng
 *   in a,($09)         ; watchdog kick (dropped, hardware artifact)
 *   ld a,r              ; A = Z80 refresh (R) register - free-running,
 *                        ; increments every instruction fetch
 *   add a,(iy-13)       ; A += rng
 *   rlca *3              ; A = rotl3(A)
 *   sub (iy-105)         ; A -= tick_244hz               (iy-105 = 0x4017)
 *   ld (random_val),a    ; rng = A
 *
 * There is no host equivalent of the Z80 R register. A monotonically
 * incrementing call counter is substituted as the "free-running"
 * ingredient (documented quirk: this makes random_next depend on call
 * order/count as well as the entropy byte, g.rng and g.tick_244hz -
 * still fully deterministic for a given call sequence, matching the
 * "deterministic given a host-supplied entropy byte" requirement).
 * ==========================================================================*/
uint8_t random_next(uint8_t entropy_arg) {
#ifdef OMEGA_WBTEST
    /* Differential-test build only. The ROM's RANDOM_NEXT reads the Z80
     * R register, which has no host equivalent, so a comparison would
     * diverge for reasons that are not bugs. The harness stubs the ROM
     * copy to RET - which leaves A holding the entropy argument - and
     * this matches it exactly. */
    return entropy_arg;
#else
    static uint8_t refresh_counter_substitute = 0;

    g.rng = (uint8_t)(g.rng + entropy_arg);

    refresh_counter_substitute++;
    {
        uint8_t mixed = (uint8_t)(refresh_counter_substitute + g.rng);
        mixed = (uint8_t)((mixed << 3) | (mixed >> 5)); /* rotl3 */
        g.rng = (uint8_t)(mixed - g.tick_244hz);
    }
    return g.rng;
#endif
}

/* ============================================================================
 * DIFFICULTY_CALC (0x2186): wave-scaled timing parameter -> 0x407C.
 * ==========================================================================*/
void difficulty_calc(void) {
    uint8_t wave_component = (uint8_t)((uint8_t)(g.wave_num << 1) & 0x1E); /* rlca; and $1E (safe for wave_num < 0x80) */
    uint8_t r = random_next(wave_component);
    r = (uint8_t)((r & 0x2F) | 0x20);
    g.difficulty_param = (uint8_t)(r - wave_component);
}

/* ============================================================================
 * HEARTBEAT_SOUND (0x2DF6): codes 2-5 -> sound cmds 0x0C-0x0F (12-15),
 * deduplicated against last_sound_cmd.
 * ==========================================================================*/
void heartbeat_sound(uint8_t code) {
    uint8_t cmd = (uint8_t)(code + 0x0A);
    if (cmd == g.last_sound_cmd) return;
    g.last_sound_cmd = cmd;
    sound_cmd_send(cmd);
}

/* ============================================================================
 * AIM_AT_SHIP (0x252F): dy/dx via MUL8X8/DIV_SLOPE -> 15-entry angle
 * bucket lookup, octant-folded to a 6-bit angle, then one +/-1 step of
 * the staged object's aim cell toward it.
 *
 * 0x400F is the STAGED OBJECT'S OWN aim byte (obj_rec +15), not a
 * global: OBJ_STAGE_RUN (0x19DE/0x19E9) copies the whole 23-byte record
 * into the workspace before FRAME_SYNC_DISP and copies it back after,
 * so every 0x400F access in this chain reads and writes per-object
 * state that persists in the record between stage-runs. It cannot be
 * modeled as one shared global: move_mode_dispatch's ship arm (0x1E1B)
 * stores the ship's heading into its own aim cell every ship stage-run,
 * which would overwrite the designated shooter's aim between ITS
 * stage-runs. Every user in this chain takes the staged object's
 * o->aim.
 * ==========================================================================*/
void aim_at_ship(obj_rec* o) {
    uint8_t ship_x = SCREEN_X(g.obj[OBJ_SHIP]);
    uint8_t ship_y = SCREEN_Y(g.obj[OBJ_SHIP]);
    uint8_t obj_x  = SCREEN_X(*o);
    uint8_t obj_y  = SCREEN_Y(*o);

    int dx_s = (int)ship_x - (int)obj_x;
    uint8_t d = (uint8_t)(dx_s < 0 ? -dx_s : dx_s);
    int dx_neg = (dx_s < 0);

    int dy_s = (int)ship_y - (int)obj_y;
    uint8_t e = (uint8_t)(dy_s < 0 ? -dy_s : dy_s);
    int dy_neg = (dy_s < 0);

    uint16_t slope = div_slope(d, e); /* ~ |dy|*256/|dx| */

    /* 15-entry descending threshold walk (ROM 0x2559-0x2569): a starts
     * at 15, decrements each non-matching entry; a=0 if none matched. */
    int bucket = 0;
    {
        int i;
        for (i = 0; i < 15; i++) {
            if (slope > AIM_ANGLE_BUCKETS[i]) { bucket = 15 - i; break; }
        }
    }

    /* octant fold (ROM 0x256D-0x2587) */
    uint8_t target;
    if (!dx_neg) {
        target = dy_neg ? (uint8_t)(((uint8_t)bucket ^ 0x0F) + 0x30) : (uint8_t)bucket;
    } else {
        target = dy_neg ? (uint8_t)((uint8_t)bucket + 0x20) : (uint8_t)(((uint8_t)bucket ^ 0x0F) + 0x10);
    }

    o->aim = step_angle_toward((uint8_t)(o->aim & 0x3F), target);
}

/* ============================================================================
 * ENEMY_ALIVE_SCAN (0x2467): census of live droids - g.wave_num objects
 * starting at object #2 (ROM table walk from 0x2E96, the object-table
 * entry for object 2), i.e. objects 2..(wave_num+1), the whole droid
 * roster. Nominates a shooter candidate and sets the wave-complete flag.
 * ==========================================================================*/
void enemy_alive_scan(void) {
    int c = 2; /* counts down: 2->live-found-once, hits 0 -> "2+ found" */
    int n;

    /* ROM 0x2472: hl = $2E96, the object table's OBJ **2** entry, stepped by
     * the 6-byte record stride for b = wave_num iterations - so the scan
     * examines objects 2 .. wave_num+1 inclusive, the whole droid roster
     * (verified against the ROM; object 2 is always a live droid at wave
     * start - see the roster dump in any wave). */
    for (n = 2; n <= 1 + (int)g.wave_num; n++) {
        obj_rec* r = &g.obj[n];

        if (!(r->flags0 & 0x80)) continue;  /* not active */
        if (r->flags1 & 0x20) continue;      /* dying */

        c--;
        if (c == 0) {
            g.shooter_state = 2;
            return; /* 2+ live droids: done immediately, no wave/xlife checks */
        }

        /* exactly one found so far. ROM 0x2494: a = -b + wave_num + 2,
         * where b still holds its entry value for this pass (djnz
         * decrements at 0x24B1, after the body). On pass k that is
         * -(wave_num-(k-1)) + wave_num + 2 = k+1, i.e. the object index
         * being examined - so candidate_num is simply n. */
        {
            int candidate_num = n;
            if (g.shooter_b != 0) continue;
            if (g.shooter_state == 1) continue;
            if (candidate_num == g.shooter_a) continue;
            g.shooter_b = (uint8_t)candidate_num;
            r->aim = r->angle; /* seed this droid's persisted aim byte from its facing */
        }
    }

    if (c & 1) { /* exactly one live droid found */
        g.shooter_state = 1;
        if (g.wave_num >= 0x0C) {
            g.wave_pace_timer = 2;    /* 0x24CC ld (iy-94),$02 */
            g.difficulty_param = 2;   /* 0x24D0 ld (iy-4),$02  */
        }
        if (g.time_played[1] >= 3) g.xlife_flags |= 0x01;
    } else { /* none found */
        g.wave_flags |= 0x40; /* wave complete */
        g.shooter_state = 0;
    }
}

/* ============================================================================
 * SHOOTER_PROMOTE (0x24EC): called (from COLLIDE_SCAN in objects.c, not
 * part of this module) once per candidate/staged-object pair with the
 * absolute screen-space deltas already reduced to bytes, and the
 * collide-scan loop's remaining record count in loop_b (mirrors the Z80
 * B register at the call site - the candidate's "designation number" is
 * derived from it exactly as the ROM does: 0x21-loop_b).
 *
 * Maintains the running |dy|*|dx| minimum in g.pair_min and promotes the
 * closest pair's candidate to g.shooter_a. NOTE: the caller is expected
 * to reset g.pair_min to 0xFFFF before a scan pass, matching the ROM's
 * "(iy+0/1)=FF" initialisation (not part of this routine).
 * ==========================================================================*/
void shooter_promote(uint8_t dy_abs, uint8_t dx_abs, uint8_t loop_b, obj_rec* candidate) {
    uint16_t product = mul8x8(dy_abs, dx_abs);
    if (product > g.pair_min) return; /* not a new minimum */

    {
        uint8_t candidate_num = (uint8_t)(0x21 - loop_b);

        if (candidate_num == g.shooter_b) {
            if (g.shooter_state >= 2) return; /* abort: don't disturb an existing 2+ designation */
            g.shooter_b = 0;
        }

        g.pair_min = product;
        g.shooter_a = candidate_num;
    }

    if (g.shooter_state == 1) {
        /* 50/50 PRNG toggle of the candidate's "track dir" bit (flags0 bit1) */
        candidate->flags0 &= (uint8_t)~0x02;
        if (random_next(g.shooter_state) & 0x10) {
            candidate->flags0 |= 0x02;
        }
    }
}

/* ============================================================================
 * SPAWN_MINE_SHOT (0x25EB): pool select by flags1 bit6 (photon/4-slot vs
 * vapor/8-slot-but-only-4-scanned, see MINE_POOL_VAPOR comment above),
 * template classes 14-21 (object numbers double as template classes,
 * verified against the ROM object table), wave-scaled velocity
 * magnitude, sound 7 (vapor only - see below), armed flag.
 * ==========================================================================*/
void spawn_mine_shot(obj_rec* o) {
    int vapor = (o->flags1 & 0x40) != 0;
    const uint8_t* pool = vapor ? MINE_POOL_VAPOR : MINE_POOL_PHOTON;
    int found = -1;
    int i;

    for (i = 0; i < 4; i++) {
        uint8_t idx = pool[i];
        if (!(g.obj[idx].flags0 & 0x80)) { found = idx; break; }
    }

    if (found < 0) {
        o->flags0 &= (uint8_t)~0x04; /* spawn_fail_clear (0x2751) */
        return;
    }

    obj_spawn((uint8_t)found);
    {
        obj_rec* m = &g.obj[found];

        m->posx = o->posx;
        m->posy = o->posy;
        m->aim = o->aim;      /* 0x400F: the SHOOTER's staged aim byte */
        if (vapor) m->flags1 |= 0x40;

        {
            uint8_t aim = m->aim;
            uint8_t idx4 = (uint8_t)(aim & 0x0F);
            if (aim & 0x10) idx4 = (uint8_t)(idx4 ^ 0x0F);

            int8_t raw_e = QUARTER_WAVE[idx4][0];
            int8_t raw_c = QUARTER_WAVE[idx4][1];
            m->rady = (uint8_t)(raw_e >> 2);
            m->radx = (uint8_t)(raw_c >> 2);

            {
                int c_negated = 0;
                int c_val = raw_c, e_val = raw_e;
                uint8_t masked30 = (uint8_t)(aim & 0x30);
                if (masked30 == 0x10 || masked30 == 0x20) { c_val = -raw_c; c_negated = 1; }
                if (aim & 0x20) { e_val = -raw_e; }

                if (vapor) {
                    /* position nudge by quarter-wave component/4 (ROM 0x2692-26AF) */
                    int8_t dx_off = (int8_t)(c_val >> 2);
                    uint8_t base_x = SCREEN_X(*m);
                    int sum_x = (int)base_x + (int)(uint8_t)dx_off;
                    int carry_x = (sum_x > 0xFF) ? 1 : 0;
                    if (c_negated == carry_x) {
                        m->posx = (uint16_t)((m->posx & 0x00FF) | (((uint16_t)(uint8_t)sum_x) << 8));
                    }

                    {
                        int8_t dy_off = (int8_t)(e_val >> 2);
                        uint8_t base_y = SCREEN_Y(*m);
                        int sum_y = (int)base_y + (int)(uint8_t)dy_off;
                        m->posy = (uint16_t)((m->posy & 0x00FF) | (((uint16_t)(uint8_t)sum_y) << 8));
                    }

                    sound_cmd_send(7);
                    m->flags0 |= 0x04;
                } else {
                    m->b12 = 0x19;
                    {
                        int diff = (int)o->aim - (int)o->angle; /* unmasked, unlike AIM_AT_SHIP */
                        uint8_t adiff = (uint8_t)(diff < 0 ? -diff : diff);
                        if (adiff < 0x10 || adiff >= 0x30) {
                            if (g.xlife_flags & 0x80) m->flags0 |= 0x04;
                        }
                    }
                }

                /* wave-scaled velocity magnitude (ROM 0x26DF-2736): the
                 * ROM accumulates c_val/e_val (sign-extended via B/D)
                 * (mult) or (mult) times via repeated 16-bit adds, where
                 * mult = 32 (or 64 if the transient "armed" bit is set)
                 * plus 4*wave_adj more. That reduces algebraically to a
                 * single multiply. */
                {
                    int mult = (m->flags0 & 0x04) ? 64 : 32;
                    int wave_adj = g.wave_num;
                    if (!(g.wave_flags & 0x08)) wave_adj -= 3; /* reused wave_flags bit3 */
                    {
                        int scale = mult + 4 * wave_adj;
                        m->velx = (int16_t)(c_val * scale);
                        m->vely = (int16_t)(e_val * scale);
                    }
                }
                m->flags0 &= (uint8_t)~0x04; /* res 2,(ix+0) - always cleared before spawn finishes */
            }
        }

        /* ROM 0x2737-0x274E: build this shot's own JSRL word and store
         * it in the record at +21/+22. Three words per shot shape:
         *   0xC000 | ((0x0AE7 + 3 * (aim_angle & 0x3F)) & 0x0FFF)
         * The display list takes a shot's shape straight from the
         * record (flags0 bit 4 is clear for the shot template, so
         * move_mode_dispatch's 0x1E63 arm applies), so this word must
         * be written here. */
        m->shape_word = (uint16_t)(0xC000 |
            ((0x0AE7 + 3 * (o->aim & 0x3F)) & 0x0FFF));
    }

    o->flags0 &= (uint8_t)~0x04; /* spawn_fail_clear (0x2751): clears the
                                   * SHOOTER's own fire-request bit on
                                   * both success and failure. */
}

/* ============================================================================
 * SPAWN_SPECIAL (0x2758): death/command ship spawner. Two player
 * exclusion boxes (ROM 0x2760-0x277E) gate whether a special ship may
 * spawn near the staged object's own position at all; an 11-slot pool
 * (objects 22-32) is searched for a free slot, spawning at object# ==
 * pool index (verified against the object table, same as
 * SPAWN_MINE_SHOT).
 *
 * QUIRK: if the whole 11-slot pool is occupied, the ROM re-rolls a
 * smaller scan-count (1-10 via PRNG) and retries; on a retry, the
 * scan-count counting down to 0 *before* a genuinely free slot is found
 * force-selects whatever slot was being examined at that moment, even
 * if occupied - i.e. an existing command/death ship can be evicted and
 * overwritten. This can't happen on the very first pass (initial count
 * is 12 > the 11 slots), only on retries. Preserved exactly.
 * ==========================================================================*/
#ifdef OMEGA_PROBE
/* probe-only hook, implemented by probe_objects.c: reports every
 * spawn_special call with its ROM call site and outcome. */
void probe_spawn_special(int site, obj_rec* o, int target);
static int probe_ss_site;   /* which caller is active, set at each site */
#define PROBE_SS_SITE(n) (probe_ss_site = (n))
#define PROBE_SS_REPORT(o, tgt) probe_spawn_special(probe_ss_site, (o), (tgt))
#else
#define PROBE_SS_SITE(n) ((void)0)
#define PROBE_SS_REPORT(o, tgt) ((void)0)
#endif

void spawn_special(obj_rec* o) {
    uint8_t ox = SCREEN_X(*o);
    uint8_t oy = SCREEN_Y(*o);

    /* player-exclusion box A: left/right screen edges of the high-Y band */
    if (oy >= 0xA0 && (ox < 0x10 || ox >= 0xF0)) { PROBE_SS_REPORT(o, -1); return; }
    /* player-exclusion box B: central box */
    if (ox >= 0x26 && ox < 0xDA && oy >= 0x56 && oy < 0xAA) { PROBE_SS_REPORT(o, -1); return; }

    {
        uint8_t c = 0x0C;
        int target;
        for (;;) {
            int i;
            target = -1;
            for (i = 0; i < 11; i++) {
                uint8_t idx = SPECIAL_POOL[i];
                if (!(g.obj[idx].flags0 & 0x80)) { target = idx; break; }
                if (--c == 0) { target = idx; break; } /* forced-eviction quirk, see above */
            }
            if (target >= 0) break;

            /* pool exhausted without a hit: re-roll a 1-10 scan-count and
             * retry (ROM 0x2798-0x27A4). RANDOM_NEXT 0x2906 consumes the
             * caller's A as entropy (add a,(iy-13) first thing): on entry
             * A holds the flags0 of the last-scanned slot (object 22,
             * 0x278A ld a,(de)), and each rejected masked value re-enters
             * as the next call's A. */
            {
                uint8_t r = g.obj[SPECIAL_POOL[10]].flags0;
                do {
                    r = (uint8_t)(random_next(r) & 0x0F);
                } while (r == 0 || r >= 0x0B);     /* accept 1..0x0A */
                c = r;
            }
        }

        obj_spawn((uint8_t)target);
        {
            obj_rec* n = &g.obj[target];
            n->posx = (uint16_t)((n->posx & 0x00FF) | ((uint16_t)ox << 8));
            n->posy = (uint16_t)((n->posy & 0x00FF) | ((uint16_t)oy << 8));
        }
        PROBE_SS_REPORT(o, target);
    }
}

/* ============================================================================
 * WALL_TRACK_STEER (0x283F): per-quadrant octagon wall-follow steering.
 * One +/-1 angle step per call, applied directly to o->angle.
 * ==========================================================================*/
void wall_track_steer(obj_rec* o) {
    uint8_t ox = SCREEN_X(*o);
    uint8_t oy = SCREEN_Y(*o);
    int track_dir = (o->flags0 & 0x02) ? 1 : 0;
    int q = ((ox & 0x80) ? 2 : 0) | ((oy & 0x80) ? 1 : 0);
    const wall_entry_t* ent = &WALL_TABLE[track_dir][q];

    uint8_t dx = (uint8_t)abs((int)ox - 0x80);
    uint8_t dy = (uint8_t)abs((int)oy - 0x80);

    int use_l = wall_region_use_l(ent->c, dx, dy);
    uint8_t chosen = use_l ? ent->l : ent->h;

    o->angle = step_angle_toward((uint8_t)(o->angle & 0x3F), chosen);
}

/* ============================================================================
 * enemy_fire_gate (0x1BEB-0x1DB3): per-staged-object fire-gate
 * scheduling. Runs inside the object update for every staged object;
 * only object numbers matching g.shooter_a/g.shooter_b actually do
 * anything. Ends where the ROM falls into move_mode_dispatch (0x1DB4),
 * which is movement-mode dispatch belonging to objects.c, out of scope
 * here - represented as a plain `return`.
 *
 * Control flow is transcribed with goto labels named after the ROM
 * addresses because of the many long forward/backward jumps; this was
 * judged far safer for fidelity than restructuring into pure
 * structured control flow (CONVENTIONS rule: "translate its control
 * flow faithfully").
 * ==========================================================================*/
void enemy_fire_gate(obj_rec* o) {
    uint8_t obj_index = g.obj_index; /* (iy-3) 0x407D: current staged object's own number,
                                       * maintained by OBJECTS_UPDATE (objects.c) */

    if (g.shooter_b == obj_index) {
        if (g.shooter_b == g.shooter_a || (o->flags1 & 0x20)) {
            g.shooter_b = 0;
            enemy_alive_scan();
        } else {
            o->b18 = (uint8_t)(0x1E - 2 * g.wave_num);
            o->flags0 |= 0x40;
            if (o->b14 == 0) o->b14 = 0x0C;
            aim_at_ship(o);
        }
    }

    if (obj_index != g.shooter_a) return; /* jp nz,move_mode_dispatch */

    if (o->flags1 & 0x20) goto qec_1DAD; /* dying */

    if (g.obj[OBJ_SHIP].flags1 & 0x10) {
        g.obj[OBJ_SHIP].flags1 &= (uint8_t)~0x10;
        o->aim = o->angle;               /* 0x1C3D ld (aim_angle),a */
        aim_at_ship(o);
        if (g.wave_num == 6) {
            g.difficulty_param = 0xFA;
        } else {
            uint8_t r = random_next(g.wave_num);
            o->b14 = (uint8_t)((r & 0x0F) | 0x20);
            g.special_spawn_armed = (uint8_t)((r & 0x07) | 0x04);
            PROBE_SS_SITE(0x1C5E);
            spawn_special(o);
        }
    }

    /* enemy_fire_gate_1C67 */
    o->flags0 |= 0x40; /* undocumented flags0 bit6 */
    if (o->b14 == 1) {
        if (--g.special_spawn_armed == 0) o->b14 = 0;
    }

    /* enemy_fire_gate_1C7B */
    if (g.last_sound_cmd != 0x0F) {
        uint8_t t = g.wave_pace_timer;
        if (t < 0x0A) {
            heartbeat_sound((t < 0x05) ? 4 : 3);
        } else if (t < 0x0F) {
            heartbeat_sound(2);
        }
    }

    /* enemy_fire_gate_1CA4 */
    if (g.shooter_state == 1) {
        /* enemy_fire_gate_1CB5 */
        if (!(g.xlife_flags & 0x80)) goto efg_1D3C;
        o->b18 = 0x0C;
        {
            uint8_t t2 = g.wave_pace_timer;
            if (t2 >= 0x0C) goto efg_1D3C;
            o->b18 = 0x0A;
            if (t2 >= 0x06) goto efg_1D3C;
            o->b18 = 0x08;
            if (t2 != 0) goto efg_1D3C;
        }
        /* fall through to efg_1CD6 */
    } else {
        if (g.wave_pace_timer != 0) goto efg_1D3C;
        /* fall through to efg_1CD6 */
    }

efg_1CD6:
    g.xlife_flags |= 0x80;
    o->b18 = 0x04;
    {
        uint8_t seed = (uint8_t)(0x11 - g.wave_num);
        uint8_t r;
        g.wave_pace_timer = seed;
        r = random_next(seed);
        o->b14 = (uint8_t)((r & 0x0F) | 0x10);
        if (g.xlife_flags & 0x01) o->b14 = 0x02;
    }

    /* enemy_fire_gate_1CFA */
    o->flags1 &= (uint8_t)~0x80;
    o->flags0 |= 0x08;
    o->b18 = 0x04;
    if (!(g.wave_flags & 0x08)) {
        g.wave_flags |= 0x08;
        o->b18 = (uint8_t)(0x10 - g.wave_num);
    }

    /* enemy_fire_gate_1D18 */
    g.special_spawn_armed = 0xFA;
    heartbeat_sound(5);
    if (g.time_played[1] == 0) {
        if (g.time_played[0] < 0x35) {
            o->b14 = 0;
            g.special_spawn_armed = 0xFA;
            o->b18 = 0x10;
        }
    }
    goto qec_1D7F;

efg_1D3C:
    if (o->flags1 & 0x80) goto qec_1D91; /* wall-follow */
    if (g.shooter_state != 1) goto qec_1D6B;
    if (g.difficulty_param < 0x14) goto qec_1D54;
    g.difficulty_param = 0x14;
    /* fall through to qec_1D54 */

qec_1D54:
    {
        uint8_t sx = SCREEN_X(g.obj[OBJ_SHIP]);
        uint8_t sy = SCREEN_Y(g.obj[OBJ_SHIP]);
        uint8_t ox = SCREEN_X(*o);
        uint8_t oy = SCREEN_Y(*o);
        uint8_t xorc = (uint8_t)(sx ^ ox);
        uint8_t xora = (uint8_t)(sy ^ oy);
        if ((uint8_t)((xora | xorc) & 0x80) != 0) {
            wall_track_steer(o);
            goto qec_1D91;
        }
    }
    /* fall through to qec_1D6B */

qec_1D6B:
    o->angle = o->aim;                   /* 0x1D6B: heading follows aim */
    if (g.wave_pace_timer == 0) goto efg_1CD6;
    if ((g.wave_pace_timer & 0x07) != 0) goto qec_1D91;
    /* ROM 0x1D7C `dec (iy-94)`. iy is the 0x4080 globals base, so
     * iy-94 is 0x4022 - wave_pace_timer (not the 0x4026 urgency
     * timer). */
    g.wave_pace_timer--;
    goto qec_1D7F;

qec_1D7F:
    o->aim = (uint8_t)(o->aim + 0x20);   /* 0x1D7F wobble */
    if (random_next(o->aim) & 0x01) o->aim--;
    /* falls through to qec_1D91 */

qec_1D91:
    aim_at_ship(o);
    if (g.special_spawn_armed == 0) goto qec_1DAD;
    if (g.shooter_a == 0) goto qec_1DAD;
    if (--g.difficulty_param != 0) return;
    PROBE_SS_SITE(0x1DA5);
    spawn_special(o);
    difficulty_calc();
    return;

qec_1DAD:
    /* ROM 0x1DAD `ld (iy-6),$00` = 0x407A = shooter_a. (iy-5) is
     * shooter_b - that is the cell 0x1BFE clears; this one retires the
     * primary shooter so SHOOTER_PROMOTE can designate a new one. */
    g.shooter_a = 0;
    difficulty_calc();
}
