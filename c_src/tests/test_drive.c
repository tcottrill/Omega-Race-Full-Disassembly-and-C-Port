/* test_drive.c - headless test driver for the Omega Race C port.
 *
 * Runs the game logic with simulated time (no window, no GL, no sound)
 * and logs mode/step/page transitions, so boot/attract behavior can be
 * verified without launching the GUI.
 *
 * Build (x64, run from c_src\):
 *   cl /nologo /W4 /std:c11 /wd4102 /D_CRT_SECURE_NO_WARNINGS
 *      tests/test_drive.c mainline.c frame.c objects.c enemies.c score.c
 *      irq_coins.c pages.c dvg_pages.c omega_pagerom.c sound_samples.c
 *      omega_shapes.c glue.c dvg.c omega_vecrom.c omega_dvgprom.c
 *      platform/headless/plat_headless.c /Fe:tests/test_drive.exe
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../omega_state.h"
#include "../omega_shapes.h"
#include "../platform/headless/plat_headless.h"

extern void game_init(void);
extern void omega_irq_244hz(void);
extern void omega_mainloop_pass(void);
extern void omega_mainloop_tick(void);
extern void omega_mainloop_frame(void);
extern void dvg_build_object_list(void);
extern void dvg_init(void);

/* ---- host hooks (backend: platform/headless/plat_headless.c) -------- */

static void log_sample_start(int c, int s, int l)
{ printf("        sample start ch%d #%02X loop=%d\n", c, s, l); }

static int draw_log = 0;   /* --draw: dump each page's list words */
extern void page_script_step(void);        /* dvg_pages.c: reveal step */

/* ---- simulation ----------------------------------------------------- */

int main(int argc, char** argv)
{
    /* 60 fps host frames, 244 Hz ticks, 90 simulated seconds */
    double irq_acc = 0.0;
    int frame, secs;
    uint8_t last_mode = 0xFF, last_step = 0xFF, last_page = 0xFF;

    double vram_at = 0.0;    /* --vram N: dump vector RAM at N seconds */
    int    play    = 0;      /* --playvram N: coin+start first         */
    int    pace    = 0;      /* --pace: per-mode frame-cost statistics */
    int    authtime = 0;     /* --authtime: authentic pass pacing      */
    double pc_sum[5] = {0}, pc_min[5] = {999,999,999,999,999},
           pc_max[5] = {0};
    long   pc_n[5] = {0};

    if (argc > 1 && !strcmp(argv[1], "--draw")) draw_log = 1;
    if (argc > 2 && !strcmp(argv[1], "--vram")) vram_at = atof(argv[2]);
    if (argc > 2 && !strcmp(argv[1], "--playvram")) {
        vram_at = atof(argv[2]);
        play = 1;
    }
    if (argc > 1 && !strcmp(argv[1], "--pace")) pace = play = 1;
    /* --paceattract: the same per-mode cost statistics, but WITHOUT the
     * coin/start pulses, so the whole 90 s stays in the attract sequence.
     * --pace coins in at 3 s, which leaves attract sampled only during
     * the boot hold, when no page is up yet and the list is nearly
     * empty - 5.4 ms against the machine's 15.7. */
    if (argc > 1 && !strcmp(argv[1], "--paceattract")) { pace = 1; play = 0; }
    if (argc > 1 && !strcmp(argv[1], "--authtime")) authtime = 1;

    hl_sample_start_hook = log_sample_start;

    memset(&g, 0, sizeof g);
    dvg_init();     /* embeds the vector ROM into vecram - the GUI host
                       does this; without it every timing walk marches
                       into zeroed ROM and prices at the guard cap */
    game_init();

    /* --draw: layout check. Each page is built on its own into a clean
     * list, and the words it CHANGED are printed with their addresses.
     * Change-detection rather than a raw dump, because dvg_dlist_reset
     * fills the region with 0xE180/HALT filler that would otherwise bury
     * the page. A reveal page only arms its gate in draw_page, so it is
     * stepped to completion here to show the finished layout. The pages
     * are display-list writes - the machine's own words ARE the layout. */
    if (draw_log) {
        extern uint8_t omega_vecram[0x2000];
        static uint8_t before[0x400];
        int p;
        for (p = 1; p <= 0x1C; p++) {
            int a, n = 0, guard;
            dvg_dlist_reset();
            memcpy(before, omega_vecram, sizeof before);
            draw_page((uint8_t)p);
            for (guard = 0; guard < 4096
                 && g.page_script_pos < g.page_script_limit; guard++) {
                g.fire_cooldown = 0;      /* ignore the 10-tick rate gate */
                page_script_step();
            }
            printf("== page %02X ==\n", p);
            for (a = 0; a < 0x400; a += 2) {
                unsigned w = omega_vecram[a] | (omega_vecram[a + 1] << 8);
                unsigned o = before[a]      | (before[a + 1] << 8);
                if (w == o) continue;
                printf(" %04X:%04X", 0x8000 + a, w);
                if (++n % 8 == 0) printf("\n");
            }
            if (n % 8) printf("\n");
            printf("   (%d words changed)\n", n);
        }
        return 0;
    }
    printf("after game_init: mode=%d step=%d page=%02X credits=%02X\n",
           g.game_mode, g.attract_step, g.page_id, g.credits_bcd);

    /* --authtime: pace the mainloop the way the GUI host does - each
     * pass costs dvg_cost_frame_ms() of simulated time, with the 244 Hz
     * service running on that same clock - instead of this harness's
     * fixed 60 fps. The fixed rate overstates scrawl time: the reveal
     * consumes fire_cooldown in integer tick_delta steps, so a fixed
     * 16.67 ms pass (delta ~4) burns ~12 ticks per glyph where the
     * machine's cheap early-title frames (delta 1-2) burn ~10. */
    if (authtime) {
        extern double dvg_cost_frame_ms(void);
        double t_ms = 0.0, irq_next = OMEGA_TICK_MS;
        long pass = 0;
        int lm = -1, ls = -1, lp = -1;
        /* title-scrawl pacing stats, mirroring frametime --scrawl-trace:
         * one emission = any movement of the page_line_cur/glyph pair
         * while page 1 is up; gaps measured in 244 Hz ticks. */
        int sc_calls = 0, sc_hist[64] = { 0 };
        double sc_first = -1.0, sc_last = -1.0;
        uint8_t sc_cur = 0xFF, sc_gly = 0xFF;
        while (t_ms < 90000.0) {
            while (irq_next <= t_ms) {
                omega_irq_244hz(); irq_next += OMEGA_TICK_MS;
                omega_mainloop_tick();
                /* sample at tick resolution - emissions happen inside
                 * the tick passes now, so a frame-boundary sample would
                 * only smear measurement noise over exact gaps */
                if (g.page_id == 1
                    && (g.page_line_cur != sc_cur
                        || g.page_line_glyph != sc_gly)) {
                    double tick_now = irq_next / OMEGA_TICK_MS;
                    if (sc_first < 0.0) sc_first = tick_now;
                    else {
                        int gap = (int)(tick_now - sc_last + 0.5);
                        if (gap >= 0 && gap < 64) sc_hist[gap]++;
                    }
                    sc_last = tick_now;
                    sc_calls++;
                    sc_cur = g.page_line_cur; sc_gly = g.page_line_glyph;
                }
            }
            omega_mainloop_frame();
            dvg_build_object_list();
            t_ms += dvg_cost_frame_ms();
            pass++;
            if (g.game_mode != lm || g.attract_step != ls
                || g.page_id != lp) {
                printf("t=%7.2fs pass=%6ld  mode=%d step=%2d page=%02X "
                       "timer=%d\n", t_ms / 1000.0, pass, g.game_mode,
                       g.attract_step, g.page_id, g.timer_seconds);
                lm = g.game_mode; ls = g.attract_step; lp = g.page_id;
            }
        }
        printf("title reveal: %d emissions over %.2f s "
               "(%.2f ticks/emission avg)\n", sc_calls,
               (sc_last - sc_first) * OMEGA_TICK_MS / 1000.0,
               sc_calls > 1 ? (sc_last - sc_first) / (sc_calls - 1) : 0.0);
        {
            int i;
            printf("inter-emission gap histogram (ticks: count):\n");
            for (i = 0; i < 64; i++)
                if (sc_hist[i]) printf("  %2d: %d\n", i, sc_hist[i]);
        }
        printf("done: 90 authentic-paced seconds\n");
        return 0;
    }

    for (frame = 0; frame < 90 * 60; frame++) {
        /* --playvram: coin pulse at 3.0s, start pulse at 4.0s (bits
         * match frametime.cpp's in_coin=0xFE / in_start=0xBF) */
        if (play) {
            hl_p1 = (frame >= 180 && frame < 192) ? 0xFE : 0xFF;
            hl_p2 = (frame >= 240 && frame < 252) ? 0xBF : 0xFF;
        }
        irq_acc += 1000.0 / 60.0;
        while (irq_acc >= OMEGA_TICK_MS)
            { omega_irq_244hz(); irq_acc -= OMEGA_TICK_MS;
              omega_mainloop_tick(); }   /* free-running arm, per tick */
        omega_mainloop_frame();
        dvg_build_object_list();   /* the GUI host does this per frame */

        if (pace) {                /* the host's exact pacing input */
            extern double dvg_cost_frame_ms(void);
            int m = (g.game_mode >= 1 && g.game_mode <= 4) ? g.game_mode : 0;
            double ms;
            ms = dvg_cost_frame_ms();
            pc_sum[m] += ms; pc_n[m]++;
            if (ms < pc_min[m]) pc_min[m] = ms;
            if (ms > pc_max[m]) pc_max[m] = ms;
        }

        if (vram_at > 0.0 && frame == (int)(vram_at * 60.0)) {
            extern uint8_t omega_vecram[0x2000];
            int a;
            printf("vram @%.1fs mode=%02X step=%02X page=%02X\n",
                   vram_at, g.game_mode, g.attract_step, g.page_id);
            for (a = 0; a < 0x400; a += 16) {
                int w;
                printf("%04X:", 0x8000 + a);
                for (w = 0; w < 16; w += 2)
                    printf(" %04X", omega_vecram[a + w]
                                    | (omega_vecram[a + w + 1] << 8));
                printf("\n");
            }
            return 0;
        }

        if (frame < 8 || g.game_mode != last_mode
            || g.attract_step != last_step || g.page_id != last_page) {
            secs = frame / 60;
            printf("t=%3ds f=%5d  mode=%d step=%2d page=%02X  "
                   "credits=%02X lives=%d wave=%d timer=%d shooter=%d 406B=%02X\n",
                   secs, frame, g.game_mode, g.attract_step, g.page_id,
                   g.credits_bcd, g.lives, g.wave_num, g.timer_seconds, g.shooter_state, g.wave_flags);
            last_mode = g.game_mode;
            last_step = g.attract_step;
            last_page = g.page_id;
        }
    }
    if (pace) {
        static const char* MN[5] = { "?", "attract", "spawn/demo",
                                     "play", "hiscore" };
        int m;
        puts("\nframe-cost by mode (the host paces on these):");
        puts("  reference (frametime, real ROMs): attract 15.69ms/63.7fps,");
        puts("  play 20.52ms/48.7fps, spawn/demo 21.89ms/45.7fps");
        for (m = 1; m <= 4; m++)
            if (pc_n[m])
                printf("  mode %d %-10s n=%5ld  avg=%6.2fms (%.1f fps)"
                       "  min=%6.2f max=%6.2f\n",
                       m, MN[m], pc_n[m], pc_sum[m] / pc_n[m],
                       1000.0 / (pc_sum[m] / pc_n[m]),
                       pc_min[m], pc_max[m]);
    }
    printf("done: 90 simulated seconds\n");
    return 0;
}
