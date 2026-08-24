/* probe_sound.c - regression test for the port-0x14 sound path.
 *
 * Build (x64, run from c_src\):
 *   cl /nologo /W4 /std:c11 /wd4102 /D_CRT_SECURE_NO_WARNINGS
 *      tests/probe_sound.c mainline.c frame.c objects.c enemies.c score.c
 *      irq_coins.c pages.c dvg_pages.c omega_pagerom.c post.c sound_samples.c omega_shapes.c glue.c
 *      dvg.c omega_vecrom.c omega_dvgprom.c
 *      platform/headless/plat_headless.c /Fe:tests/probe_sound.exe
 *
 * Covers the ship-death timer path and the channel map, plus the
 * command-table facts they depend on. Ground truth is
 * disasm/SOUND_NOTES.md, which is read off omega_main.asm and
 * sound_k5.bin - not off this port.
 *
 * 1. OBJ_DESTROY's ship-death branch (ROM 0x27F6-0x2817). The ROM's
 *    (iy-95)/(iy-94) are timer_seconds (0x4021) and wave_pace_timer
 *    (0x4022), since iy == 0x4080 (fixed by last_sound_cmd == (iy-24) ==
 *    0x4068). Writing anything else loses the death pause AND lets
 *    SOUND_CMD_SEND's gate swallow the explosion when the player dies
 *    while a countdown is running.
 * 2. omega_hw_sound's channel map, which must reproduce the sound ROM's
 *    mutual `F5` (kill) relationships: within a family a new command
 *    silences the others, across families it does not.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../omega_state.h"
#include "../omega_shapes.h"
#include "../platform/headless/plat_headless.h"

extern void obj_destroy(obj_rec *staged);

/* host stubs: platform/headless/plat_headless.c */

/* ---- instrumentation ------------------------------------------------ */

/* what each mixer channel is playing, -1 = silent. Must be >= the channel
 * count sound_samples.c uses. */
#define PROBE_CH 16
static int  chan[PROBE_CH];
/* commands seen by omega_hw_sound this scenario */
static int  cmds[16], ncmds;

static int started[0x20];   /* how many times each sample was triggered */

static void capture_sample_start(int c, int s, int loop)
{
    (void)loop;
    if (s >= 0 && s < 0x20) started[s]++;
    if (c >= 0 && c < PROBE_CH) chan[c] = s;
}
static void capture_sample_stop(int c) { if (c >= 0 && c < PROBE_CH) chan[c] = -1; }

/* sound_samples.c owns omega_hw_sound; wrap the call sites' view of it by
 * recording here and forwarding. */
extern void omega_hw_sound(uint8_t cmd);
static void snd(uint8_t cmd)
{
    if (ncmds < 16) cmds[ncmds++] = cmd;
    omega_hw_sound(cmd);
}

static int fails;

static void chk(const char *what, int got, int want)
{
    printf("  %-26s got %3d  want %3d  %s\n", what, got, want,
           got == want ? "ok" : "FAIL");
    if (got != want) fails++;
}

static void reset_mixer(void)
{
    int i;
    for (i = 0; i < PROBE_CH; i++) chan[i] = -1;
    ncmds = 0;
}

static int playing(int sample)   /* which channel holds `sample`, else -1 */
{
    int i;
    for (i = 0; i < PROBE_CH; i++) if (chan[i] == sample) return i;
    return -1;
}

static int voices(void)          /* how many channels are sounding */
{
    int i, n = 0;
    for (i = 0; i < PROBE_CH; i++) if (chan[i] >= 0) n++;
    return n;
}

/* ---- 1. OBJ_DESTROY ship-death timers ------------------------------- */

static void death_case(const char *title, uint8_t lives, uint8_t timer_in)
{
    memset(&g, 0, sizeof g);
    g.svc_flags     = 0x80;   /* sound+scoring enable gate (0x402E bit7) */
    g.game_mode     = 3;      /* play */
    g.lives         = lives;
    g.timer_seconds = timer_in;
    g.sound_ready   = 0;      /* worst case: the gated path would drop 0x01 */
    g.obj_index     = 1;      /* the ship */
    reset_mixer();

    printf("%s (lives=%u, timer_seconds in=%u)\n", title, lives, timer_in);
    obj_destroy(&g.obj[1]);

    /* ROM 2813: (iy-95) = 5 starts the death pause */
    chk("timer_seconds after", g.timer_seconds, 5);
    /* ROM 27FD: (iy-94) = 0x0A only on the last life */
    chk("wave_pace_timer", g.wave_pace_timer, lives == 1 ? 0x0A : 0);
    /* obj_destroy calls sound_cmd_send directly, so watch the mixer: the
     * sample only lands if the command got past SOUND_CMD_SEND's gate. */
    chk("explosion 0x01 sounding", playing(0x01) >= 0, 1);
    printf("\n");
}

static void death_tests(void)
{
    puts("=== OBJ_DESTROY ship-death timers (ROM 0x27F6-0x2817) ===");
    death_case("last life",                1, 0);
    death_case("spare lives",              3, 0);
    death_case("death during a countdown", 3, 4);
}

/* ---- 2. channel map vs the sound ROM's kill sets --------------------- */

static void family_case(const char *title, int first, int second, int exclusive)
{
    reset_mixer();
    snd((uint8_t)first);
    snd((uint8_t)second);
    printf("%s\n", title);
    chk("second is sounding", playing(second) >= 0, 1);
    chk(exclusive ? "first was cut" : "first still sounding",
        playing(first) >= 0, exclusive ? 0 : 1);
    printf("\n");
}

static void mixer_tests(void)
{
    int i;

    puts("=== channel map vs sound_k5.bin kill sets ===");

    /* the five heartbeat tempos each kill the other four */
    for (i = 0x0b; i < 0x0f; i++) {
        char t[64];
        sprintf(t, "heartbeat %02X then %02X (exclusive)", i, i + 1);
        family_case(t, i, i + 1, 1);
    }
    /* 02/10/15 droid explosions; 03/11 command-ship explosions */
    family_case("droid expl 02 then 10 (exclusive)",     0x02, 0x10, 1);
    family_case("droid expl 10 then 15 (exclusive)",     0x10, 0x15, 1);
    family_case("cmd-ship expl 03 then 11 (exclusive)",  0x03, 0x11, 1);
    /* script 07 kills 12 and back */
    family_case("fire 07 then coin 12 (exclusive)",      0x07, 0x12, 1);
    /* script 04 kills 14 */
    family_case("tune 04 then beeps 14 (exclusive)",     0x04, 0x14, 1);
    /* across families nothing is cut: a wall hit must not stop the heartbeat */
    family_case("heartbeat 0B then wall hit 08 (independent)", 0x0b, 0x08, 0);
    family_case("thrust 09 then fire 07 (independent)",        0x09, 0x07, 0);

    /* cmd 0 = script 0: 19 kills then every AY register zeroed */
    reset_mixer();
    snd(0x0b); snd(0x09); snd(0x08); snd(0x04);
    puts("cmd 00 silences everything");
    chk("voices before", voices(), 5);   /* 0x09 occupies two: attack + sustain */
    snd(0x00);
    chk("voices after", voices(), 0);
    printf("\n");

    /* scripts 13 and 14 open with F4 00 */
    reset_mixer();
    snd(0x0b); snd(0x09);
    snd(0x14);
    puts("cmd 14 (bonus beeps) opens with F4 00");
    chk("heartbeat cut", playing(0x0b) >= 0, 0);
    chk("thrust cut",    playing(0x09) >= 0, 0);
    chk("beeps sounding", playing(0x14) >= 0, 1);
    printf("\n");

    reset_mixer();
    snd(0x0b); snd(0x09);
    snd(0x13);
    puts("cmd 13 (tilt) opens with F4 00");
    chk("heartbeat cut", playing(0x0b) >= 0, 0);
    chk("thrust cut",    playing(0x09) >= 0, 0);
    chk("alarm sounding", playing(0x13) >= 0, 1);
    printf("\n");

    /* Thrust: 0x09 starts BOTH the a.wav attack and the looped 9.wav
     * sustain (the sample set is cut on script 0x09's two-phase seam, and
     * the real PCB has an audible attack); 0x0A stops both. 9.wav alone
     * peaks at 2% FS, so dropping the attack reads as "thrust is silent". */
    reset_mixer();
    snd(0x09);
    puts("thrust 09 on / 0A off");
    chk("sustain 9 sounding",   playing(0x09) >= 0, 1);
    chk("attack a sounding",    playing(0x0a) >= 0, 1);
    chk("on separate channels", playing(0x09) != playing(0x0a), 1);
    snd(0x0a);
    chk("both stopped", voices(), 0);
    printf("\n");

    /* cmd 0 must reach the thrust channels too */
    reset_mixer();
    snd(0x09); snd(0x00);
    puts("cmd 00 also silences thrust");
    chk("nothing sounding", voices(), 0);
    printf("\n");

    /* 5 and 6 are script 04's own harmony voices, never latched; >= 0x17 is
     * dropped by the sound board's `cp $17 / jr nc` */
    reset_mixer();
    snd(0x05); snd(0x06); snd(0x17); snd(0xff);
    puts("unlatched (05/06) and out-of-range (>= 17) commands");
    chk("nothing sounding", voices(), 0);
    printf("\n");
}

/* ---- 3. a shot that hits nothing is silent -------------------------- */

/* ROM 0x1BCA-0x1BCD: a shot destroyed by a wall gets obj_class = its own
 * obj_index, which lands in the silent 0x0E-0x15 arm of OBJ_DESTROY's
 * dispatch. Drop that store and obj_class keeps COLLIDE_SCAN's per-miss
 * running count instead, which above 0x16 selects kill_snd_cmdship and
 * fires sound 0x11 on shots that hit a wall. On the real PCB a shot from
 * either the player or an enemy that hits nothing makes no sound.
 *
 * Drive a real game and count command-ship explosions. Wave 1 in this
 * window has no command ship, so the only way 0x11 or 0x03 can sound is
 * a missing 0x1BCA store. */
extern void game_init(void);
extern void omega_irq_244hz(void);
extern void omega_mainloop_pass(void);

static void shot_miss_tests(void)
{
    int frame, i;

    puts("=== live play: a shot hitting a wall is silent (ROM 0x1BCA) ===");
    memset(&g, 0, sizeof g);
    memset(started, 0, sizeof started);
    reset_mixer();
    game_init();

    for (frame = 0; frame < 3000; frame++) {
        hl_p1 = 0xFF;
        hl_p2 = 0xFF;
        if (frame >= 180 && frame < 190) hl_p1 &= (uint8_t)~0x01;  /* coin  */
        if (frame >= 300 && frame < 310) hl_p2 &= (uint8_t)~0x40;  /* start */
        if (frame >= 600 && frame < 900) hl_p1 &= (uint8_t)~0x20;  /* thrust */
        /* tap fire every 40 frames once in play, so player shots - not just
         * enemy ones - reach the wall and take the 0x1BCA path */
        if (frame > 420 && (frame % 40) < 4) hl_p1 &= (uint8_t)~0x40;
        for (i = 0; i < 5; i++) omega_irq_244hz();
        omega_mainloop_pass();
    }

    chk("reached play mode", g.game_mode >= 3, 1);
    chk("cmd-ship expl 11 count", started[0x11], 0);
    chk("cmd-ship expl 03 count", started[0x03], 0);
    printf("  (for reference: fire=%d wall=%d thrust=%d heartbeat=%d)\n\n",
           started[0x07], started[0x08], started[0x09],
           started[0x0b] + started[0x0c] + started[0x0d] +
           started[0x0e] + started[0x0f]);
}

int main(void)
{
    hl_sample_start_hook = capture_sample_start;
    hl_sample_stop_hook  = capture_sample_stop;
    death_tests();
    mixer_tests();
    shot_miss_tests();
    printf("%s\n", fails ? "PROBE FAILED" : "PROBE PASSED");
    return fails ? 1 : 0;
}
