/* probe_objects.c - THROWAWAY diagnostic. Headless mirror of test_drive
 * that dumps object-table activity once per simulated second, so the
 * enemy_fire_gate / move_mode_dispatch chain can be watched doing
 * something. Not part of the build; delete when done. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../omega_state.h"
#include "../omega_shapes.h"
#include "../platform/headless/plat_headless.h"

extern void game_init(void);
extern void omega_irq_244hz(void);
extern void omega_mainloop_pass(void);
extern void omega_mainloop_tick(void);
extern void omega_mainloop_frame(void);

static int play_mode;          /* --play: coin + start into real mode 3 */
static int pace_mode;          /* --pace: log every change of the wave-
                                  pacing cells (0x4022/0x407C/shooters/
                                  0x4077/0x406A) through attract + demo,
                                  mirroring frametime --pace-trace */
static int droid_mode;         /* --droid: trace droid 2's patrol path,
                                  mirroring frametime --droid-trace */
static int death_mode;         /* --death: force droid 2 into the
                                  converted-shooter "death ship" state
                                  and trace it, mirroring frametime's
                                  --deathship-trace hardware run */
/* host stubs: platform/headless/plat_headless.c */

/* The whole frame is the display list, so the frame cost IS
 * dvg_cost_frame_ms() - there is no retained layer to add in. */
static double probe_frame_cost_ms(void) { return dvg_cost_frame_ms(); }


/* Called from enemies.c (compiled with /DOMEGA_PROBE) on every
 * spawn_special() call. site = the ROM call-site address, target = the
 * slot spawned, or -1 when the player-exclusion boxes rejected it. */
static int probe_frame;
void probe_spawn_special(int site, obj_rec* o, int target)
{
    int i, droids = 0;
    for (i = OBJ_DROID_FIRST; i <= OBJ_DROID_LAST; i++)
        if (g.obj[i].flags0 & 0x80) droids++;
    printf("  [ss] f=%5d site=%04X caller_obj=%2d -> %s%d | mode=%d "
           "wave_num=%d droids=%d 4022=%02X 407C=%02X 407F=%02X xlife=%02X\n",
           probe_frame, site, g.obj_index,
           target < 0 ? "REJ " : "obj ", target < 0 ? 0 : target,
           g.game_mode, g.wave_num, droids,
           g.wave_pace_timer, g.difficulty_param, g.special_spawn_armed,
           g.xlife_flags);
    (void)o;
}

/* ---- probe bookkeeping ---------------------------------------------- */
static int spawn_seen[OBJ_COUNT + 1];   /* times each slot went active   */
static int max_life[OBJ_COUNT + 1];     /* longest continuous life, frames */
static int cur_life[OBJ_COUNT + 1];
static int noshape_frames;              /* active object, empty shape word */
static int prev_active[OBJ_COUNT + 1];
static int bad_word5, bad_word0, checked_frames;
static double cost_sum[4];              /* per game_mode 1..3 */
static int    cost_n[4];
static double cost_min[4], cost_max[4];

extern uint8_t omega_vecram[0x2000];
extern int dvg_dbg_iters, dvg_dbg_halted;

/* Walk the 32 object entries the way the DVG would and complain about
 * anything that is not a legal entry: word0 must be a JMPL (0xE000) and
 * word5, on a live entry, must be a JSRL (0xC000). A stray opcode there
 * eats the following word and derails the whole list. */
static void check_dlist(void)
{
    int n;
    checked_frames++;
    for (n = 1; n <= OBJ_COUNT; n++) {
        int b = (n - 1) * 12;
        uint16_t w0 = (uint16_t)(omega_vecram[b] | (omega_vecram[b+1] << 8));
        uint16_t w5 = (uint16_t)(omega_vecram[b+10] | (omega_vecram[b+11] << 8));
        if ((w0 & 0xF000) != 0xE000) bad_word0++;
        if (!(g.obj[n].flags0 & 0x80) || !(g.obj[n].flags1 & 0x04)) continue;
        if ((w5 & 0xF000) != 0xC000) {
            if (bad_word5 < 8)
                printf("  !! obj %2d word5=%04X (flags0=%02X flags1=%02X)\n",
                       n, w5, g.obj[n].flags0, g.obj[n].flags1);
            bad_word5++;
        }
    }
}

static void band_counts(int *ship, int *droid, int *shot, int *spec)
{
    int i;
    *ship = *droid = *shot = *spec = 0;
    for (i = 1; i <= OBJ_COUNT; i++) {
        if (!(g.obj[i].flags0 & 0x80)) continue;
        if (i == OBJ_SHIP) (*ship)++;
        else if (i <= OBJ_DROID_LAST) (*droid)++;
        else if (i <= OBJ_SHOT_LAST) (*shot)++;
        else (*spec)++;
    }
}

int main(int argc, char** argv)
{
    double irq_acc = 0.0;
    int frame, i;

    play_mode = (argc > 1 && (!strcmp(argv[1], "--play")
                              || !strcmp(argv[1], "--death")
                              || !strcmp(argv[1], "--droid")));
    death_mode = (argc > 1 && !strcmp(argv[1], "--death"));
    droid_mode = (argc > 1 && !strcmp(argv[1], "--droid"));
    pace_mode  = (argc > 1 && !strcmp(argv[1], "--pace"));

    memset(&g, 0, sizeof g);
    dvg_init();          /* vector ROM + ship frame buffers - the host
                          * does this; without it every JSRL lands in
                          * zeroed vector RAM and the timing walk never
                          * finds a HALT. */
    game_init();

    for (frame = 0; frame < 90 * 60; frame++) {
        probe_frame = frame;
        if (play_mode) {
            /* one coin held over frames 180-190, start 1 over 300-310 */
            hl_p1 = 0xFF;
            if (frame >= 180 && frame < 190) hl_p1 &= (uint8_t)~0x01;
            hl_p2 = 0xFF;
            if (frame >= 300 && frame < 310) hl_p2 &= (uint8_t)~0x40;
        }
        irq_acc += 1000.0 / 60.0;
        while (irq_acc >= OMEGA_TICK_MS)
            { omega_irq_244hz(); irq_acc -= OMEGA_TICK_MS;
              omega_mainloop_tick(); }   /* mirror the host: per tick  */
        omega_mainloop_frame();

        if (droid_mode && g.game_mode == 3) {
            static int dstart = -1;
            static uint8_t ymin = 0xFF, ymax = 0, xmin = 0xFF, xmax = 0;
            obj_rec* d2 = &g.obj[2];
            int k;
            if (dstart < 0) dstart = frame;
            for (k = 2; k <= 7; k++) {
                obj_rec* q = &g.obj[k];
                uint8_t qy, qx;
                if (!(q->flags0 & 0x80)) continue;
                qy = (uint8_t)((uint16_t)q->posx >> 8);
                qx = (uint8_t)((uint16_t)q->posy >> 8);
                if (qy < ymin) ymin = qy;  if (qy > ymax) ymax = qy;
                if (qx < xmin) xmin = qx;  if (qx > xmax) xmax = qx;
            }
            if (((frame - dstart) % 30) == 0)
                printf("%5.1f  %02X   %02X   %02X  %02X %02X | y[%02X..%02X] x[%02X..%02X]\n",
                       (frame - dstart) / 60.0,
                       (uint8_t)((uint16_t)d2->posx >> 8),
                       (uint8_t)((uint16_t)d2->posy >> 8),
                       d2->angle, d2->flags0, d2->flags1,
                       ymin, ymax, xmin, xmax);
            if (frame - dstart >= 25 * 60) {
                printf("port droid envelope over 25 s: y[%02X..%02X] x[%02X..%02X]\n",
                       ymin, ymax, xmin, xmax);
                return 0;
            }
        }

        if (death_mode && g.game_mode == 3 && frame >= 400) {
            obj_rec* d = &g.obj[2];
            if (frame == 400) {
                /* same forcing as the hardware run: droid 2 becomes the
                 * designated shooter with the scheduler armed and the
                 * pace timer expired, so the fire cycle converts it at
                 * its next stage-run */
                g.shooter_a = 2;
                g.shooter_state = 2;
                g.special_spawn_armed = 0xFA;
                g.wave_pace_timer = 0;
            }
            if (g.shooter_a == 0) {          /* re-arm, as the trace did */
                g.shooter_a = 2;
                g.special_spawn_armed = 0xFA;
            }
            {
                static uint8_t pf0, pf1; static uint16_t pvx, pvy;
                uint16_t vx = (uint16_t)d->velx, vy = (uint16_t)d->vely;
                if (d->flags0 != pf0 || d->flags1 != pf1
                    || vx != pvx || vy != pvy || (frame % 60) == 0) {
                    printf("%6.2f %02X %02X  %04X %04X  %04X %04X"
                           "  %02X  %02X  %02X | %02X\n",
                           (frame - 400) / 60.0, d->flags0, d->flags1,
                           vx, vy, (uint16_t)d->posx, (uint16_t)d->posy,
                           d->angle, d->b14, d->b18, d->aim);
                    pf0 = d->flags0; pf1 = d->flags1; pvx = vx; pvy = vy;
                }
            }
        }
        if (pace_mode) {
            static uint8_t pv[6];
            uint8_t cv[6] = { g.wave_pace_timer, g.difficulty_param,
                              g.shooter_a, g.shooter_b,
                              g.shooter_state, g.xlife_flags };
            if (memcmp(pv, cv, sizeof cv) != 0) {
                printf("%7.2f mode=%02X wave=%02X | 4022=%02X 407C=%02X"
                       " shootA=%02X shootB=%02X 4077=%02X 406A=%02X\n",
                       frame / 60.0, g.game_mode, g.wave_num,
                       cv[0], cv[1], cv[2], cv[3], cv[4], cv[5]);
                memcpy(pv, cv, sizeof cv);
            }
        }

        dvg_build_object_list();
        if (g.game_mode & 0x02) check_dlist();

        {
            double ms = probe_frame_cost_ms();
            int m = g.game_mode & 3;
            if (cost_n[m] == 0) { cost_min[m] = cost_max[m] = ms; }
            if (ms < cost_min[m]) cost_min[m] = ms;
            if (ms > cost_max[m]) cost_max[m] = ms;
            cost_sum[m] += ms;
            cost_n[m]++;
        }

        for (i = 1; i <= OBJ_COUNT; i++) {
            int act = (g.obj[i].flags0 & 0x80) != 0;
            if (act && !prev_active[i]) { spawn_seen[i]++; cur_life[i] = 0; }
            if (act) {
                cur_life[i]++;
                if (cur_life[i] > max_life[i]) max_life[i] = cur_life[i];
                if (g.obj_shape[i] == 0 && g.obj[i].shape_word == 0)
                    noshape_frames++;
            }
            if (i >= OBJ_SPECIAL_FIRST && act != prev_active[i])
                printf("  f=%5d obj %2d %s flags0=%02X flags1=%02X b12=%02X "
                       "timer=%02X b18=%02X shape=%04X\n",
                       frame, i, act ? "SPAWN" : "gone ",
                       g.obj[i].flags0, g.obj[i].flags1, g.obj[i].b12,
                       g.obj[i].timer, g.obj[i].b18, g.obj_shape[i]);
            prev_active[i] = act;
        }

        if (play_mode && frame % 120 == 0 && (g.game_mode & 2)) {
            int k;
            printf("  walls f=%5d fx0=%02X fx[1..8]=", frame, g.wall_fx[0]);
            for (k = 1; k <= 8; k++) printf("%02X ", g.wall_fx[k]);
            printf("| z=");
            for (k = 0; k < 8; k++) {   /* stride 6 from 0x1BB */
                printf("%X", omega_vecram[0x1BB + k * 6] >> 4);
            }
            printf(" | inner=");
            for (k = 9; k < 12; k++) printf("%X", omega_vecram[0x1BB + k * 6] >> 4);
            printf("\n");
        }
        if (frame % 60 == 0) {
            double lc = (double)dvg_cost_list() / 12000.0;
            double fc = probe_frame_cost_ms();
            printf("  cost: mode=%d list=%.2f frame=%.2fms"
                   " iters=%d halted=%d\n",
                   g.game_mode, lc, fc, dvg_dbg_iters, dvg_dbg_halted);
            if (lc > 50.0) {
                int w;
                printf("  !! list blowup, object region words:\n");
                for (w = 0; w < 0xD9; w++) {
                    if (w % 6 == 0) printf("    %04X:", 0x8000 + w * 2);
                    printf(" %04X", (uint16_t)(omega_vecram[w*2] | (omega_vecram[w*2+1] << 8)));
                    if (w % 6 == 5) printf("\n");
                }
                printf("\n");
            }
        }
        if (frame % 60 == 0) {
            int ship, droid, shot, spec;
            band_counts(&ship, &droid, &shot, &spec);
            printf("t=%2ds mode=%d step=%2d | ship=%d droid=%2d shot=%d spec=%d"
                   " | shooter a=%2d b=%2d state=%d | idx=%2d 4022=%02X 407C=%02X 407F=%02X\n",
                   frame / 60, g.game_mode, g.attract_step,
                   ship, droid, shot, spec,
                   g.shooter_a, g.shooter_b, g.shooter_state,
                   g.obj_index, g.wave_pace_timer, g.difficulty_param,
                   g.special_spawn_armed);
        }
    }

    printf("\nper-slot: spawns / longest continuous life (frames)\n");
    for (i = 1; i <= OBJ_COUNT; i++)
        if (spawn_seen[i] || max_life[i])
            printf("  obj %2d: spawns=%3d maxlife=%5d shape=%04X\n",
                   i, spawn_seen[i], max_life[i], g.obj_shape[i]);
    printf("frames with an active object and no shape word at all: %d\n",
           noshape_frames);
    printf("dlist checked on %d frames: bad word0=%d, bad word5=%d\n",
           checked_frames, bad_word0, bad_word5);
    {
        static const char* mn[4] = { "?", "attract", "demo", "play" };
        int m;
        printf("\nauthentic frame periods (timing model; hardware ref:"
               " attract 15.7ms/63.7fps, demo 21.9/45.7, play 20.5/48.7):\n");
        for (m = 1; m <= 3; m++)
            if (cost_n[m])
                printf("  mode %d %-7s  avg %5.2f ms (%4.1f fps)"
                       "  min %5.2f  max %5.2f  over %d frames\n",
                       m, mn[m], cost_sum[m] / cost_n[m],
                       1000.0 * cost_n[m] / cost_sum[m],
                       cost_min[m], cost_max[m], cost_n[m]);
    }
    return 0;
}
