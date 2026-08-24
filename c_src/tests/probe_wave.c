/* probe_wave.c - regression test for ENEMY_ALIVE_SCAN's roster coverage.
 *
 * Build (x64, run from c_src\):
 *   cl /nologo /W4 /std:c11 /wd4102 /D_CRT_SECURE_NO_WARNINGS
 *      tests/probe_wave.c mainline.c frame.c objects.c enemies.c score.c
 *      irq_coins.c pages.c dvg_pages.c omega_pagerom.c post.c sound_samples.c omega_shapes.c glue.c
 *      dvg.c omega_vecrom.c omega_dvgprom.c
 *      platform/headless/plat_headless.c /Fe:tests/probe_wave.exe
 *
 * ROM 0x2467 ENEMY_ALIVE_SCAN:
 *   ld b,(iy-25)        ; b = wave_num  (iy is 0x4080, so iy-25 = 0x4067)
 *   ld c,$02
 *   ld hl,$2E96         ; the object table's OBJ **2** entry
 *   ...                 ; 6-byte record stride, djnz b
 * so it examines objects 2 .. wave_num+1 inclusive - the whole droid
 * roster - and sets wave_flags bit 6 (0x406B bit 6, "wave complete") only
 * when it finds NONE of them alive.
 *
 * Object 2 is always a live droid mid-wave (with wave_num 6 the live
 * objects are 2,3,4,5,6,7), so the very first roster slot in particular
 * must be able to hold the wave open.
 *
 * The test states the ROM's range directly and asserts the scan's actual
 * coverage against it, so an off-by-one in either direction fails.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../omega_state.h"
#include "../omega_shapes.h"

extern void enemy_alive_scan(void);

/* host stubs: platform/headless/plat_headless.c (defaults suffice) */

static int fails;

/* Put exactly one live enemy at `idx` and report whether the scan noticed
 * it (i.e. did NOT declare the wave complete). */
static int scan_sees(int wave_num, int idx)
{
    memset(&g, 0, sizeof g);
    g.wave_num     = (uint8_t)wave_num;
    g.shooter_state = 2;
    g.obj[idx].flags0 = 0x80;    /* active   */
    g.obj[idx].flags1 = 0x00;    /* not dying */
    enemy_alive_scan();
    return (g.wave_flags & 0x40) ? 0 : 1;
}

static void coverage_case(int wave_num)
{
    int idx, bad = 0, first = -1, last = -1;

    for (idx = 1; idx <= OBJ_COUNT; idx++) {
        int want = (idx >= 2 && idx <= 1 + wave_num);   /* the ROM's range */
        int got  = scan_sees(wave_num, idx);
        if (got != want) { bad++; if (first < 0) first = idx; }
        if (got) { if (last < 0) first = first; last = idx; }
    }
    /* report the observed span for a human reading the log */
    {
        int lo = -1, hi = -1;
        for (idx = 1; idx <= OBJ_COUNT; idx++)
            if (scan_sees(wave_num, idx)) { if (lo < 0) lo = idx; hi = idx; }
        printf("  wave_num=%-3d scanned %d..%-3d  (ROM: 2..%d)  %s\n",
               wave_num, lo, hi, 1 + wave_num,
               bad ? "FAIL" : "ok");
        if (bad) {
            printf("       %d slot(s) wrong, first at index %d\n", bad, first);
            fails++;
        }
    }
}

/* An enemy sitting in the very first roster slot must hold the wave
 * open. */
static void first_slot_case(void)
{
    int wave_num = 6;
    memset(&g, 0, sizeof g);
    g.wave_num = (uint8_t)wave_num;
    g.shooter_state = 2;
    g.obj[2].flags0 = 0x80;
    enemy_alive_scan();
    printf("  one droid alive in slot 2 -> wave_complete=%d want 0  %s\n",
           (g.wave_flags & 0x40) ? 1 : 0,
           (g.wave_flags & 0x40) ? "FAIL" : "ok");
    if (g.wave_flags & 0x40) fails++;

    /* and with the roster genuinely empty it must still complete */
    memset(&g, 0, sizeof g);
    g.wave_num = (uint8_t)wave_num;
    g.shooter_state = 2;
    enemy_alive_scan();
    printf("  empty roster        -> wave_complete=%d want 1  %s\n",
           (g.wave_flags & 0x40) ? 1 : 0,
           (g.wave_flags & 0x40) ? "ok" : "FAIL");
    if (!(g.wave_flags & 0x40)) fails++;
}

/* ---- the every-4th-wave bonus screen must end ------------------------
 * PLAY_FRAME 0x0CA4 only announces when wave_ctr % 4 == 0, so this path
 * first runs after the fourth wave. WAVE_CLEAR_ANIM (0x0CD4) then waits
 * five laps of the free-running 244 Hz counter (~5.1 s) before awarding
 * the bonus.
 *
 * tick_delta is 4 or 5 and varies frame to frame, so the counter walks
 * the residues mod 256; the cases below mix parities so a countdown
 * that can only notice zero by landing on it exactly is caught. */
extern void game_init(void);
extern void play_frame(void);

static void bonus_case(const char *title, int first, int rest)
{
    int frame;
    const int LIMIT = 4000;          /* ~66 s at 60 fps; the ROM takes ~5 s */

    memset(&g, 0, sizeof g);
    game_init();
    g.game_mode = 3; g.game_flags = 0x80; g.svc_flags = 0x80;
    g.lives = 3; g.wave_num = 6;
    g.wave_ctr = 3;                  /* the next clear makes it 4 */
    g.wave_flags = 0x40;             /* wave complete */

    for (frame = 0; frame < LIMIT; frame++) {
        g.tick_delta = (uint8_t)(frame == 0 ? first : rest);
        play_frame();
        if (!g.wave_anim_active && frame > 0) break;
    }
    if (frame >= LIMIT) {
        printf("  %-34s NEVER ENDS (>%d frames)  FAIL\n", title, LIMIT);
        fails++;
        return;
    }
    /* one lap is 256 ticks; five of them at `rest` ticks/frame, plus slack */
    {
        int expect = 5 * 256 / (rest ? rest : 1);
        int ok = frame + 1 <= expect * 2;
        printf("  %-34s ended after %4d frames (expect ~%d)  %s\n",
               title, frame + 1, expect, ok ? "ok" : "FAIL");
        if (!ok) fails++;
    }
}

static void bonus_tests(void)
{
    bonus_case("steady delta 4",        4, 4);
    bonus_case("steady delta 5",        5, 5);
    bonus_case("odd once, then even",   5, 4);
    bonus_case("even once, then odd",   4, 5);
    bonus_case("steady delta 3",        3, 3);
}

/* ---- shooter-kill re-pace (ROM 0x2307-0x2322) ------------------------
 * Killing the designated shooter reloads wave_pace_timer=7 and
 * difficulty_param=5 (0x2310/0x2314), tightened to 2/2 when xlife_flags
 * bit0 - the 0x24DB late-game latch - is set (0x231E/0x2322). A
 * shooter_b kill only clears that cell (0x2326-0x232B), no re-pace.
 *
 * Staged through the real dispatch, not by calling internals: a player
 * shot lives in the vapor pool (slots 18-21, flags1 bit6 set - see
 * spawn_mine_shot), and FRAME_SYNC_DISP's 18-21 band scans the droid
 * roster with class seed 2 (0x1A88), so a hit on droid 2 reaches
 * cs_kill_object with obj_class == 2 == shooter_a. */
extern void objects_update(void);
extern void obj_spawn(uint8_t obj_num);
extern void dvg_init(void);

static void shooter_kill_case(const char *title, uint8_t xlife,
                              int as_shooter_b,
                              uint8_t want_pace, uint8_t want_diff)
{
    int pass, dead = 0;

    memset(&g, 0, sizeof g);
    g.game_mode  = 3;
    g.wave_num   = 6;                 /* < 12: no 0x24CC overwrite      */
    g.tick_delta = 4;
    g.xlife_flags = xlife;
    g.wave_pace_timer  = 0x19;        /* recognizable pre-kill values   */
    g.difficulty_param = 0xFA;
    if (as_shooter_b) g.shooter_b = 2; else g.shooter_a = 2;
    g.shooter_state = 2;
    /* keep the designation alive across the shooter's own stage-runs:
     * with 0x407F unarmed, qec_1DAD (ROM 0x1DAD) retires shooter_a and
     * reruns DIFFICULTY_CALC before the shot ever lands */
    g.special_spawn_armed = 0xFA;

    obj_spawn(2);                     /* the designated shooter          */
    obj_spawn(3);                     /* second droid keeps the wave open */
    obj_spawn(18);                    /* the killing shot (vapor pool)   */
    g.obj[18].flags1 |= 0x40;         /* player side, as spawn_mine_shot
                                         stamps it (friend-or-foe bit)   */
    /* open field, away from the walls and the center island; the shot
     * sits on the shooter, the other droid far away */
    g.obj[2].posx  = 0x3000; g.obj[2].posy  = 0x3000;
    g.obj[18].posx = 0x3000; g.obj[18].posy = 0x3000;
    g.obj[3].posx  = (int16_t)0xC000; g.obj[3].posy = 0x3000;
    /* keep both droids' fire timers far from expiry so nothing spawns
     * into the shot pool mid-test */
    g.obj[2].b14 = 0x60; g.obj[3].b14 = 0x60;

    for (pass = 0; pass < 8 && !dead; pass++) {
        objects_update();
        dead = !(g.obj[2].flags0 & 0x80);
    }

    {
        /* a shooter_a kill re-paces both cells; a shooter_b kill leaves
         * wave_pace_timer alone (0x407C is fair game for the shooter
         * machinery - DIFFICULTY_CALC and the per-stage-run decrement -
         * so the control only pins 0x4022). enemy_alive_scan runs from
         * the kill tail and re-designates shooter_b, so "cleared" can
         * only be asserted on the shooter_a side. */
        int ok = dead && g.wave_pace_timer == want_pace
                 && (as_shooter_b
                     || (g.shooter_a == 0 && g.difficulty_param == want_diff));
        printf("  %-28s dead=%d shootA=%02X 4022=%02X (want %02X)"
               " 407C=%02X  %s\n",
               title, dead, g.shooter_a, g.wave_pace_timer, want_pace,
               g.difficulty_param, ok ? "ok" : "FAIL");
        if (!ok) fails++;
    }
}

static void shooter_kill_tests(void)
{
    dvg_init();
    shooter_kill_case("shooter_a kill",           0x00, 0, 0x07, 0x05);
    shooter_kill_case("shooter_a kill, late-game",0x01, 0, 0x02, 0x02);
    /* killing shooter_b must NOT re-pace the wave */
    shooter_kill_case("shooter_b kill (control)", 0x00, 1, 0x19, 0x00);
}

int main(void)
{
    puts("=== ENEMY_ALIVE_SCAN roster coverage (ROM 0x2467) ===");
    coverage_case(4);
    coverage_case(6);
    coverage_case(8);
    coverage_case(12);
    puts("");
    puts("=== the wave must not end with an enemy alive ===");
    first_slot_case();
    puts("");
    puts("=== every-4th-wave bonus screen must end (ROM 0x0CD4) ===");
    bonus_tests();
    puts("");
    puts("=== shooter-kill re-pace (ROM 0x2307-0x2322) ===");
    shooter_kill_tests();
    printf("\n%s\n", fails ? "PROBE FAILED" : "PROBE PASSED");
    return fails ? 1 : 0;
}
