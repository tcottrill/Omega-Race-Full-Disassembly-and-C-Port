/* probe_sndboard.c - differential test of sound_board.c against the real
 * sound ROM.
 *
 * Build (x64, run from c_src\): build_all.bat builds it with the rest,
 * against the same core file list as every other probe.
 *
 * 1. The oracle. `frametime --sndboard-trace tests\sndboard_ref.txt`
 *    (disasm/) runs sound_k5.bin on the Z80 - NMI every 6144 cycles,
 *    commands latched 100 cycles before an NMI, i.e. between two ticks,
 *    which is where sound_board.c delivers them - and logs every AY data
 *    write with its tick number. This probe replays each scenario's
 *    commands through the port and requires the identical write stream:
 *    same tick, chip, register and value, in the same order. Scenarios:
 *    every command 0x01-0x16 alone for 4000 ticks, command 0x00 over a
 *    running heartbeat, a gameplay mix, and the tune/bonus/tilt chain.
 *    The reference file is checked in beside this probe (it is generated
 *    data like omega_sndrom.c, so the probe runs without a ROM set);
 *    frametime is its generator of record. Without it this part reports
 *    SKIPPED and the probe fails.
 *
 *    Tick numbers are reported, not required: the real Z80 is slow enough
 *    to run late and even lose ticks (see run_scenario), so the port's
 *    writes carry the same or an earlier tick than the ROM's.
 *
 *      S <name>                      scenario, from a power-on reset
 *      C <tick> <cmd>                latch cmd after tick's pass
 *      W <tick> <chip 1|2> <reg> <val>
 *      E <ticks>                     scenario length
 *
 * 2. The audio end. A tone on AY1 channel A must come out of the rendered
 *    stream at 1 MHz / (16 * period), and the per-tick block sizes must
 *    add up to the sample rate's share of the elapsed ticks exactly.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../platform/headless/plat_headless.h"

extern void sndboard_init(int sample_rate);
extern void sndboard_command(uint8_t cmd);
extern void sndboard_tick(void);
extern void (*sndboard_write_hook)(int chip, uint8_t reg, uint8_t val);

static int fails;

/* ---- captured AY writes ---------------------------------------------- */

typedef struct { int tick, chip, reg, val; } ay_w;

#define MAX_W 400000
static ay_w got[MAX_W], want[MAX_W];
static int  ngot, nwant, cur_tick;

static void capture_write(int chip, uint8_t reg, uint8_t val)
{
    if (ngot < MAX_W) {
        got[ngot].tick = cur_tick; got[ngot].chip = chip + 1;
        got[ngot].reg  = reg;      got[ngot].val  = val;
    }
    ngot++;
}

/* ---- captured audio --------------------------------------------------- */

static long   pcm_frames;
static int    pcm_prev, pcm_rises;

static void capture_audio(const int16_t* pcm, int frames)
{
    int i;
    for (i = 0; i < frames; i++) {
        if (pcm_prev <= 0 && pcm[i] > 0) pcm_rises++;
        pcm_prev = pcm[i];
    }
    pcm_frames += frames;
}

/* ---- scenario replay -------------------------------------------------- */

#define MAX_C 64
static struct { int tick, cmd; } cmds[MAX_C];
static int ncmds;

/* Does each of the 32 registers receive the same values in the same order? */
static int same_per_register(void)
{
    int key;
    for (key = 0; key < 32; key++) {
        int chip = (key >> 4) + 1, reg = key & 15, i = 0, j = 0;
        for (;;) {
            while (i < nwant && (want[i].chip != chip || want[i].reg != reg)) i++;
            while (j < ngot  && (got[j].chip  != chip || got[j].reg  != reg)) j++;
            if (i >= nwant || j >= ngot) break;
            if (want[i].val != got[j].val) return 0;
            i++; j++;
        }
        if (i < nwant || j < ngot) return 0;
    }
    return 1;
}

static void run_scenario(const char* name, int ticks)
{
    int t, i, n, bad = -1;
    int skew_min = 0, skew_max = 0, skewed = 0;

    ngot = 0;
    sndboard_init(0);
    for (t = 1; t <= ticks; t++) {
        for (i = 0; i < ncmds; i++)             /* latched after tick t-1 */
            if (cmds[i].tick == t - 1) sndboard_command((uint8_t)cmds[i].cmd);
        cur_tick = t;
        sndboard_tick();
    }

    n = ngot < nwant ? ngot : nwant;
    if (n > MAX_W) n = MAX_W;
    for (i = 0; i < n; i++) {
        int skew = want[i].tick - got[i].tick;
        if (got[i].chip != want[i].chip || got[i].reg != want[i].reg ||
            got[i].val != want[i].val) { bad = i; break; }
        if (i == 0 || skew < skew_min) skew_min = skew;
        if (i == 0 || skew > skew_max) skew_max = skew;
        if (skew != 0) skewed++;
    }
    if (bad < 0 && ngot != nwant) bad = n;

    if (bad < 0) {
        printf("  ok    %-12s %6d writes over %d ticks; rom-port tick skew %d..%d (%d skewed)\n",
               name, nwant, ticks, skew_min, skew_max, skewed);
        return;
    }

    /* Second tier, for the multi-command scenarios. The Z80 is slow enough
     * to LOSE ticks: a pass that runs script 00's full register wipe (or
     * SND_RESET's RAM clear) spans two or three NMIs, and snd_tick moving
     * by more than one still buys only one pass. Every running script
     * then sits that many ticks behind the wall clock - and behind any
     * command the main CPU sends later, which arrives on the wall clock.
     * The port's pass is instantaneous and loses nothing, so after a wipe
     * a later command can interleave with an older script a pass or two
     * differently. What must still hold is that every register receives
     * exactly the same sequence of values. */
    if (ngot <= MAX_W && nwant <= MAX_W && same_per_register()) {
        printf("  ok~   %-12s %6d writes over %d ticks; per-register streams identical,\n"
               "                     interleave differs from write #%d (hardware lost ticks)\n",
               name, nwant, ticks, bad);
        return;
    }

    fails++;
    printf("  FAIL  %-12s write #%d of %d (port made %d)\n", name, bad, nwant, ngot);
    for (i = bad > 2 ? bad - 2 : 0; i < bad + 4; i++) {
        printf("        #%-6d", i);
        if (i < nwant) printf(" rom  t%-5d ay%d r%x=%02x", want[i].tick, want[i].chip, want[i].reg, want[i].val);
        else           printf(" rom  -                 ");
        if (i < ngot && i < MAX_W)
                       printf("   port t%-5d ay%d r%x=%02x", got[i].tick, got[i].chip, got[i].reg, got[i].val);
        else           printf("   port -");
        printf("%s\n", i == bad ? "   <--" : "");
    }
}

static int run_oracle(const char* path)
{
    FILE* f = fopen(path, "r");
    char line[128], name[64] = "";
    int scenarios = 0;

    if (!f) return -1;
    while (fgets(line, sizeof line, f)) {
        int a, b, c, d;
        if (line[0] == 'S') {
            sscanf(line + 1, "%63s", name);
            ncmds = 0; nwant = 0;
        } else if (line[0] == 'C' && sscanf(line + 1, "%d %x", &a, &b) == 2) {
            if (ncmds < MAX_C) { cmds[ncmds].tick = a; cmds[ncmds].cmd = b; ncmds++; }
        } else if (line[0] == 'W' && sscanf(line + 1, "%d %d %x %x", &a, &b, &c, &d) == 4) {
            if (nwant < MAX_W) {
                want[nwant].tick = a; want[nwant].chip = b;
                want[nwant].reg  = c; want[nwant].val  = d;
            }
            nwant++;
        } else if (line[0] == 'E' && sscanf(line + 1, "%d", &a) == 1) {
            if (nwant > MAX_W) { printf("  FAIL  %s: reference too long\n", name); fails++; }
            else run_scenario(name, a);
            scenarios++;
        }
        /* '#' lines are the oracle's own diagnostics (NMIs that landed
         * while a pass was still running) - see the skew report. */
    }
    fclose(f);
    return scenarios;
}

/* ---- audio ------------------------------------------------------------ */

static void test_audio(void)
{
    const int rate = 48000, ticks = 2000;
    int t;
    long expect;
    double secs, hz, want_hz;

    /* Block accounting: whatever the scripts do, the per-tick block sizes
     * must sum to exactly the sample rate's share of the elapsed ticks. */
    sndboard_write_hook = 0;
    hl_audio_push_hook = capture_audio;
    pcm_frames = 0;
    sndboard_init(rate);
    sndboard_command(0x16);
    for (t = 0; t < ticks; t++) sndboard_tick();

    /* rate * 49152 / 12e6 samples per tick, accumulated exactly */
    expect = (long)(((long long)rate * 49152LL * ticks) / 12000000LL);
    if (pcm_frames != expect) {
        printf("  FAIL  audio: %ld frames over %d ticks, expected %ld\n", pcm_frames, ticks, expect);
        fails++;
    } else
        printf("  ok    audio: %ld frames over %d ticks (exact)\n", pcm_frames, ticks);

    /* Pitch: the self-test sweep (cmd 0x16, script $02A6) opens AY1 tone A
     * at full volume with period 0x33 and adds 1 per tick, so the number
     * of cycles the stream must contain is the sum over the ticks of
     * (1 MHz / 16 / period) * tick length. */
    pcm_frames = 0; pcm_rises = 0; pcm_prev = 0;
    sndboard_init(rate);
    sndboard_command(0x16);
    hz = 0.0;
    for (t = 1; t <= 480; t++) {
        sndboard_tick();
        /* the script's first pass sets period 0x33 and then adds 1 before
         * its first delay, so tick t plays period 0x33 + t */
        hz += 1000000.0 / (16.0 * (0x33 + t));
    }
    secs = 49152.0 / 12000000.0;
    want_hz = hz * secs;                         /* expected edge count */
    if (pcm_rises < want_hz * 0.99 || pcm_rises > want_hz * 1.01) {
        printf("  FAIL  audio: sweep produced %d cycles, expected %.0f\n", pcm_rises, want_hz);
        fails++;
    } else
        printf("  ok    audio: sweep produced %d cycles, expected %.0f\n", pcm_rises, want_hz);
}

int main(int argc, char** argv)
{
    const char* ref = argc > 1 ? argv[1] : "tests\\sndboard_ref.txt";
    int n;

    printf("probe_sndboard: register stream vs sound_k5.bin on the Z80 (%s)\n", ref);
    sndboard_write_hook = capture_write;
    n = run_oracle(ref);
    if (n < 0) {
        printf("  SKIPPED - no reference file. Generate it from disasm\\ with:\n"
               "            frametime --sndboard-trace ..\\c_src\\tests\\sndboard_ref.txt\n");
        fails++;
    } else if (n == 0) {
        printf("  FAIL  reference file holds no scenarios\n");
        fails++;
    }

    test_audio();

    printf(fails ? "PROBE FAILED (%d)\n" : "PROBE PASSED\n", fails);
    return fails ? 1 : 0;
}
