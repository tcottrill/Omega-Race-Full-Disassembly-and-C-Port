/* wbtest.c - THROWAWAY differential test for WALL_BOUNCE.
 *
 * Reads the vector file produced by
 *   disasm\frametime.exe --wallbounce-vectors N > wbvec.txt
 * which drives the real ROM's WALL_BOUNCE (0x1F2A) on the Z80 with
 * synthetic staged-object states, and replays every case through the
 * port's own wall_bounce(). Any divergence names the exact input.
 *
 * Build (x64, run from c_src\):
 *   cl /W4 /std:c11 /DOMEGA_WBTEST tests/wbtest.c mainline.c frame.c objects.c
 *      enemies.c score.c irq_coins.c pages.c dvg_pages.c omega_pagerom.c
 *      sound_samples.c omega_shapes.c glue.c dvg.c omega_vecrom.c
 *      omega_dvgprom.c platform/headless/plat_headless.c
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../omega_state.h"
#include "../omega_shapes.h"

extern uint8_t omega_test_wall_bounce(obj_rec *o);   /* objects.c hook */
extern void    omega_test_obj_physics(obj_rec *o);   /* objects.c hook */
extern void    enemy_fire_gate(obj_rec *o);          /* enemies.c        */

/* host stubs: platform/headless/plat_headless.c - nothing here draws
 * or makes noise */

/* ---- the comparison ------------------------------------------------- */

typedef struct {
    unsigned f0, f1, velx, vely, posx, posy, ang, b11, wf;   /* in  */
    unsigned of0, of1, ovelx, ovely, oposx, oposy, ob18, o78; /* out */
} vec_t;

static const char *FIELD[] = { "flags0", "flags1", "velx", "vely",
                               "posx", "posy", "bounce_flags/angle" };

int main(int argc, char **argv)
{
    FILE *f = fopen(argc > 1 ? argv[1] : "wbvec.txt", "r");
    char line[512];
    int total = 0, bad = 0, shown = 0;
    int field_bad[7] = { 0 };

    if (!f) { printf("cannot open vector file\n"); return 1; }

    while (fgets(line, sizeof line, f)) {
        if (line[0] == 'F') {                 /* enemy_fire_gate vectors */
            unsigned i_[13], o_[12];
            static const char *FF[12] = {
                "flags0","flags1","angle","b14","b18","shooter_a",
                "shooter_b","pace_timer","difficulty","armed","xlife","aim"
            };
            unsigned mine_[12];
            int k, mm = 0;
            obj_rec *ob;
            if (sscanf(line + 1,
                " %x %x %x %x %x %x %x %x %x %x %x %x %x |"
                " %x %x %x %x %x %x %x %x %x %x %x %x",
                &i_[0],&i_[1],&i_[2],&i_[3],&i_[4],&i_[5],&i_[6],&i_[7],
                &i_[8],&i_[9],&i_[10],&i_[11],&i_[12],
                &o_[0],&o_[1],&o_[2],&o_[3],&o_[4],&o_[5],&o_[6],&o_[7],
                &o_[8],&o_[9],&o_[10],&o_[11]) != 25)
                continue;
            total++;
            memset(&g, 0, sizeof g);
            /* stage into a NON-ship slot: on the ROM the staged copy is
             * the workspace at 0x4000, distinct from the ship record at
             * 0x40B2 that 0x1C33 tests. Staging into obj[1] would alias
             * the two and produce false flags1-bit4 divergences that are
             * the harness's fault, not the port's. */
            ob = &g.obj[2];
            ob->flags0 = (uint8_t)i_[0];
            ob->flags1 = (uint8_t)i_[1];
            ob->angle  = (uint8_t)i_[2];
            ob->b14    = (uint8_t)i_[3];
            g.obj_index           = (uint8_t)i_[4];
            g.shooter_a           = (uint8_t)i_[5];
            g.shooter_b           = (uint8_t)i_[6];
            g.shooter_state       = (uint8_t)i_[7];
            g.wave_pace_timer     = (uint8_t)i_[8];
            g.difficulty_param    = (uint8_t)i_[9];
            g.special_spawn_armed = (uint8_t)i_[10];
            g.xlife_flags         = (uint8_t)i_[11];
            g.wave_num            = (uint8_t)i_[12];

            enemy_fire_gate(ob);

            mine_[0]  = ob->flags0;      mine_[1]  = ob->flags1;
            mine_[2]  = ob->angle;       mine_[3]  = ob->b14;
            mine_[4]  = ob->b18;         mine_[5]  = g.shooter_a;
            mine_[6]  = g.shooter_b;     mine_[7]  = g.wave_pace_timer;
            mine_[8]  = g.difficulty_param;
            mine_[9]  = g.special_spawn_armed;
            mine_[10] = g.xlife_flags;   mine_[11] = ob->aim;

            for (k = 0; k < 12; k++)
                if (o_[k] != mine_[k]) { mm = 1; if (k < 7) field_bad[k]++; }
            if (mm) {
                bad++;
                if (shown < 10) {
                    shown++;
                    printf("MISMATCH obj=%02X sha=%02X shb=%02X state=%X "
                           "f0=%02X f1=%02X pace=%02X diff=%02X armed=%02X "
                           "xlife=%02X wave=%02X\n",
                           i_[4], i_[5], i_[6], i_[7], i_[0], i_[1],
                           i_[8], i_[9], i_[10], i_[11], i_[12]);
                    for (k = 0; k < 12; k++)
                        if (o_[k] != mine_[k])
                            printf("    %-11s rom=%02X  port=%02X\n",
                                   FF[k], o_[k], mine_[k]);
                }
            }
            continue;
        }
        vec_t v;
        obj_rec *o;
        uint8_t got;
        unsigned exp[7], mine[7];
        int i, mism = 0;

        if (line[0] != 'V' && line[0] != 'P') continue;
        if (sscanf(line + 1, " %x %x %x %x %x %x %x %x %x | %x %x %x %x %x %x %x %x",
                   &v.f0, &v.f1, &v.velx, &v.vely, &v.posx, &v.posy,
                   &v.ang, &v.b11, &v.wf,
                   &v.of0, &v.of1, &v.ovelx, &v.ovely, &v.oposx, &v.oposy,
                   &v.ob18, &v.o78) != 17)
            continue;
        total++;

        /* stage the object exactly as the trampoline did */
        memset(&g, 0, sizeof g);
        g.obj_index  = 1;
        g.wave_flags = (uint8_t)v.wf;
        o = &g.obj[1];
        o->flags0 = (uint8_t)v.f0;
        o->flags1 = (uint8_t)v.f1;
        o->velx   = (int16_t)v.velx;
        o->vely   = (int16_t)v.vely;
        o->posx   = (int16_t)v.posx;
        o->posy   = (int16_t)v.posy;
        o->angle  = (uint8_t)v.ang;
        o->b11    = (uint8_t)v.b11;

        if (line[0] == 'V') {
            got = omega_test_wall_bounce(o);
        } else {
            omega_test_obj_physics(o);
            got = o->angle;          /* the P vectors report angle here:
                                        wall_track_steer moves it, and a
                                        wrong turn is the whole question */
        }

        exp[0] = v.of0;   mine[0] = o->flags0;
        exp[1] = v.of1;   mine[1] = o->flags1;
        exp[2] = v.ovelx; mine[2] = (uint16_t)o->velx;
        exp[3] = v.ovely; mine[3] = (uint16_t)o->vely;
        exp[4] = v.oposx; mine[4] = (uint16_t)o->posx;
        exp[5] = v.oposy; mine[5] = (uint16_t)o->posy;
        /* the two vector kinds report a different last field: V gives
         * the ROM's 0x4078 bounce flags, P gives the staged angle
         * (0x400A), which is what wall_track_steer moves */
        exp[6] = (line[0] == 'V') ? v.o78 : v.ob18;
        mine[6] = got;

        for (i = 0; i < 7; i++)
            if (exp[i] != mine[i]) { mism = 1; field_bad[i]++; }

        if (mism) {
            bad++;
            if (shown < 12) {
                shown++;
                printf("MISMATCH in: f0=%02X f1=%02X vel=(%04X,%04X) "
                       "pos=(%04X,%04X) ang=%02X b11=%02X wave=%02X\n",
                       v.f0, v.f1, v.velx, v.vely, v.posx, v.posy,
                       v.ang, v.b11, v.wf);
                for (i = 0; i < 7; i++)
                    if (exp[i] != mine[i])
                        printf("    %-13s rom=%04X  port=%04X\n",
                               FIELD[i], exp[i], mine[i]);
            }
        }
    }
    fclose(f);

    printf("\n%d cases, %d match, %d differ\n", total, total - bad, bad);
    if (bad) {
        printf("divergence by field:");
        for (int i = 0; i < 7; i++)
            if (field_bad[i]) printf("  %s=%d", FIELD[i], field_bad[i]);
        printf("\n");
    }
    return bad ? 1 : 0;
}
