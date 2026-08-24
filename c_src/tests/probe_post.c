/* probe_post.c - regression test for the POST / diagnostic screens.
 *
 * Build (x64, run from c_src\):
 *   cl /nologo /W4 /std:c11 /wd4102 /D_CRT_SECURE_NO_WARNINGS
 *      tests/probe_post.c mainline.c frame.c objects.c enemies.c score.c
 *      irq_coins.c pages.c dvg_pages.c omega_pagerom.c post.c sound_samples.c omega_shapes.c glue.c
 *      dvg.c omega_vecrom.c omega_dvgprom.c
 *      platform/headless/plat_headless.c /Fe:tests/probe_post.exe
 *
 * Ground truth is post.c's header comment, which decodes the ROM's screen
 * headers at 0x01A0 / 0x026E and the label blocks at 0x01CC / 0x029A.
 *
 * The two things a self test must not do are hang and lie, so this checks
 * that every screen terminates on the fire edge, that the whole cycle
 * repeats while the switch is held and hands off to the game when it is
 * released, and that each screen actually draws its rows.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../omega_state.h"
#include "../omega_shapes.h"
#include "../platform/headless/plat_headless.h"

extern void post_boot(void);
extern void post_enter(void);
extern void omega_mainloop_pass(void);
extern void omega_irq_244hz(void);
extern int  post_frame(void);

/* ---- simulated panel: hl_p1/hl_p2 from plat_headless.c ---------------
 * (active low: bit6 fire, bit7 test) */

/* a valid persisted image, so the BBU RAM stage has something real to
 * pass; nvram_corrupt() below breaks it on purpose. */
static int nv_good = 1;
static void ref_nvram_load(void)
{
    static const uint8_t REF[16] = {
        0x0F, 0x0D, 0x0B, 0x09, 0x07, 0x05, 0x03, 0x01,
        0x00, 0x02, 0x04, 0x06, 0x08, 0x0A, 0x0C, 0x0E,
    };
    int i;
    for (i = 0; i < 16; i++) {
        g.nvram_template[i]      = nv_good ? REF[i] : 0x00;
        g.nvram_template[16 + i] = REF[i];
    }
    memset(g.bookkeep, 0, sizeof g.bookkeep);
    g.nvram_credits[0] = 0; g.nvram_credits[1] = 0;
}

/* ---- read the display list -------------------------------------------
 * POST builds real vector-RAM words (dvg_post_* in dvg_pages.c out of
 * the ROM blobs in omega_postrom.c), so this probe reads the list instead
 * of watching host draw calls. Expected words below were read off
 * disasm/omega_dump.bin, not produced by the code under test. */
extern uint8_t omega_vecram[0x2000];
extern void dvg_init(void);
extern void dvg_build_object_list(void);

#define VBASE 0x802C            /* first label word (after the header)  */
#define VMARK 0x803C            /* cell 0's '.' word = VBASE + 8*2      */
#define VCELL 0x12              /* 9 words per label cell               */

static unsigned vw(unsigned addr)      /* one word at a Z80 vecram addr */
{
    unsigned o = (addr - 0x8000) & 0x1FFF;
    return (unsigned)omega_vecram[o] | ((unsigned)omega_vecram[o + 1] << 8);
}
static unsigned mark_of(int cell) { return vw(VMARK + cell * VCELL); }
static unsigned first_of(int cell) { return vw(VBASE + cell * VCELL); }

/* dvg.c's line sink. Nothing is drawn, but counting the calls is how
 * this probe checks a screen actually WALKS: dvg_render() is the same
 * walker the GUI uses, so a list that renders here renders there. */
static int n_vec;
static void count_vec(float x0, float y0, float x1, float y1, int z)
{ (void)x0;(void)y0;(void)x1;(void)y1;(void)z; n_vec++; }

/* walk the current list and report how many vectors came out */
static int render_vectors(void)
{
    n_vec = 0;
    dvg_render();
    return n_vec;
}

static int snd_last = -1;
static void capture_sample(int c,int s,int l){(void)c;(void)l; snd_last = s;}

static int fails;

/* display-list words read far better in hex than in decimal */
static void chk_w(const char* what, unsigned got, unsigned want)
{
    printf("  %-38s got %04X  want %04X  %s\n", what, got, want,
           got == want ? "ok" : "FAIL");
    if (got != want) fails++;
}

static void chk(const char* what, int got, int want)
{
    printf("  %-38s got %4d  want %4d  %s\n", what, got, want,
           got == want ? "ok" : "FAIL");
    if (got != want) fails++;
}
static void chk_ge(const char* what, int got, int least)
{
    printf("  %-38s got %4d  want >=%3d  %s\n", what, got, least,
           got >= least ? "ok" : "FAIL");
    if (got < least) fails++;
}

/* One frame with the panel as given, in the HOST's order: the game pass
 * and then the object build. The host (app_loop.c) calls
 * dvg_build_object_list() unconditionally after every mainloop pass, so a
 * probe that only called post_frame() would miss object records landing
 * on top of the diagnostic screen - which is what happens when F2 enters
 * POST from a game in progress, where game_mode is still 3. */
static int post_clobber;        /* bytes the object build overwrote */

/* what the host does right after every mainloop pass, plus a check
 * that it did not touch the list POST had just built */
static void object_build_pass(void)
{
    static uint8_t before[0x400];
    int i;
    memcpy(before, omega_vecram, sizeof before);
    dvg_build_object_list();
    for (i = 0; i < (int)sizeof before; i++)
        if (omega_vecram[i] != before[i]) post_clobber++;
}

static int step(int fire)
{
    int r;
    hl_p1 = 0xFF;
    hl_p1 &= (uint8_t)~0x80;                       /* test switch held    */
    if (fire) hl_p1 &= (uint8_t)~0x40;             /* fire                */
    g.tick_delta = 4;
    r = post_frame();
    object_build_pass();
    return r;
}

/* press and release fire, returning the frames consumed */
static void tap_fire(void) { step(1); step(0); }

/* run until the stage changes, or give up */
static int run_until_stage(uint8_t want, int limit)
{
    int i;
    for (i = 0; i < limit; i++) {
        if (g.post_stage == want) return i;
        step(0);
    }
    return -1;
}

int main(void)
{
    int i;

    hl_vec_line_hook     = count_vec;
    hl_sample_start_hook = capture_sample;
    hl_nvram_load_hook   = ref_nvram_load;

    /* embed the vector ROM into vecram, as the GUI host does. Without
     * this every glyph JSRL lands in zeros and the screens render
     * nothing at all - which is what the render checks below catch. */
    dvg_init();

    puts("=== the test switch gates POST (ROM 0x050A) ===");
    memset(&g, 0, sizeof g);
    hl_p1 = 0xFF;                                  /* switch released     */
    post_boot();
    chk("released -> straight to the game", g.post_active, 0);
    chk("released -> game_mode is attract",  g.game_mode, 1);

    memset(&g, 0, sizeof g);
    hl_p1 = (uint8_t)~0x80;                        /* switch held         */
    post_boot();
    chk("held -> POST owns the screen", g.post_active, 1);
    chk("held -> starts on the test screen", g.post_stage, 0);

    puts("");
    puts("=== the result screen as display-list words (ROM 0x01A0/0x01CC) ===");
    for (i = 0; i < 200 && g.post_step < 14; i++) step(0);
    chk("all 14 stages stepped", g.post_step, 14);
    step(0);
    /* the 0x2C-byte header landed verbatim */
    chk_w("8000 LABS y=800", vw(0x8000), 0xA320);
    chk_w("8002 LABS x=416 sc1", vw(0x8002), 0x11A0);
    chk_w("8008 JMPL ->02C", vw(0x8008), 0xE016);
    chk_w("800E O of \"  OK\"",  vw(0x800E), 0xCCC2);
    chk_w("802A RTSL", vw(0x802A), 0xD000);
    /* labels: DRAW_TEXT put the right glyph at the start of each cell */
    chk_w("cell 0  = P of P ROM 1", first_of(0), 0xCCCA);
    chk_w("cell 9  = B of BBU RAM", first_of(9), 0xCC58);
    chk_w("cell 16 = V of V ROM 1", first_of(16), 0xCD02);
    /* marks: JSRL 0x005 on every tested cell, spacers left as 0x00F */
    chk_w("cell 0  marked OK", mark_of(0), 0xC005);
    chk_w("cell 9  marked OK", mark_of(9), 0xC005);
    chk_w("cell 17 marked OK", mark_of(17), 0xC005);
    chk_w("spacer cell 4 untouched", mark_of(4), 0xC00F);
    /* the list has to END: cell 17 is the last, and the word after it is
     * whatever VG_WAIT_WIPE left, i.e. a HALT. */
    chk_w("list halts after cell 17", vw(VBASE + 18 * VCELL), 0xB000);
    /* and it has to WALK: a malformed list runs to the walker's guard and
     * prices out at the 50 ms cap, so a sane frame cost proves the
     * JMPL/JSRL/RTSL structure actually returns. */
    chk_ge("test screen renders vectors", render_vectors(), 200);

    puts("");
    puts("=== fire advances through every screen (ROM EDGE_BIT6_IX) ===");
    tap_fire();
    chk("-> input display", g.post_stage, 1);
    step(0);
    chk_w("8000 LABS y=900", vw(0x8000), 0xA384);
    chk_w("cell 0  = C of COIN 1", first_of(0), 0xCC64);
    chk_w("cell 44 = D of DIPSW2", first_of(44), 0xCC6C);
    /* switch held = bit clear = the ROM writes 0x05, which reads " LOW" */
    chk_w("TEST row reads LOW", mark_of(5), 0xC005);
    chk_w("COIN 1 row reads HI", mark_of(0), 0xC00A);
    chk_w("group gap untouched", mark_of(6), 0xC00F);
    chk_w("list halts after cell 44", vw(VBASE + 45 * VCELL), 0xB000);
    chk_ge("input screen renders vectors", render_vectors(), 400);

    tap_fire();
    chk("-> cross-hatch", g.post_stage, 2);
    step(0);
    chk_w("8000 LABS 512,0", vw(0x8000), 0xA200);
    chk_w("8008 first vector", vw(0x8008), 0x83FD);
    chk_w("8016 last vector", vw(0x8016), 0xC7FF);
    chk_w("8018 wipe terminates", vw(0x8018), 0xB000);
    chk_ge("cross-hatch renders vectors", render_vectors(), 4);

    tap_fire();
    chk("-> grid", g.post_stage, 3);
    step(0);
    chk_w("8000 LABS 0,0", vw(0x8000), 0xA000);
    chk_w("809E last word", vw(0x809E), 0xF000);
    chk_w("80A0 wipe terminates", vw(0x80A0), 0xB000);
    chk_ge("grid renders vectors", render_vectors(), 12);

    snd_last = -1;
    tap_fire();
    chk("-> sound test", g.post_stage, 4);
    chk("sound command 0x16 issued", snd_last, 0x16);
    step(0);
    /* post_sound_loop 0x0786 only ever calls VG_WAIT_WIPE - it never
     * kicks the generator - so this screen really is blank. */
    chk_w("sound screen is a wiped list", vw(0x8000), 0xB000);
    chk("sound screen draws nothing", render_vectors(), 0);

    puts("");
    puts("=== the cycle repeats while held, exits when released ===");
    tap_fire();
    chk("wrapped back to the test screen", g.post_stage, 0);
    chk("marks cleared for the new pass", g.post_step <= 1, 1);
    chk("still active", g.post_active, 1);

    if (run_until_stage(0, 5) < 0) { puts("  lost the stage"); fails++; }
    for (i = 0; i < 200 && g.post_step < 14; i++) step(0);
    /* walk to the sound stage again, then release the switch and fire */
    tap_fire(); tap_fire(); tap_fire(); tap_fire();
    chk("back at the sound stage", g.post_stage, 4);
    hl_p1 = 0xFF; hl_p1 &= (uint8_t)~0x40;        /* fire, switch RELEASED */
    g.tick_delta = 4;
    post_frame();
    chk("released -> POST hands off", g.post_active, 0);
    chk("released -> game is running", g.game_mode, 1);

    puts("");
    puts("=== a failing check really does show NG ===");
    memset(&g, 0, sizeof g);
    nv_good = 0;                                    /* corrupt the image   */
    hl_p1 = (uint8_t)~0x80;
    post_boot();
    for (i = 0; i < 200 && g.post_step < 14; i++) step(0);
    step(0);
    chk_w("corrupt NVRAM -> BBU RAM NG", mark_of(9), 0xC00A);
    chk_w("other rows still OK", mark_of(0), 0xC005);
    nv_good = 1;

    puts("");
    puts("=== F2 enters the diagnostics from anywhere (host affordance) ===");
    /* from attract */
    memset(&g, 0, sizeof g);
    hl_p1 = 0xFF;
    post_boot();                                    /* boots into attract  */
    chk("attract: not in POST",  g.post_active, 0);
    chk("attract: mode",         g.game_mode, 1);
    post_enter();
    chk("attract + F2 -> in POST", g.post_active, 1);
    chk("attract + F2 -> stage 0", g.post_stage, 0);
    step(0);
    chk_w("draws the result screen", first_of(0), 0xCCCA);

    /* from a game in progress: mainloop_pass must hand the frame to POST */
    memset(&g, 0, sizeof g);
    hl_p1 = 0xFF;
    post_boot();
    g.game_mode = 3;                                /* pretend we are playing */
    g.game_flags = 0x80;
    post_enter();
    chk("in-game + F2 -> in POST", g.post_active, 1);
    hl_p1 = 0xFF;                                  /* switch NOT held        */
    g.tick_244hz = (uint8_t)(g.tick_244hz + 4);
    omega_mainloop_pass();                          /* the real routing path  */
    object_build_pass();                            /* ...and what follows it */
    chk("mainloop_pass routes to POST", g.post_active, 1);
    chk("game frame did not run",       g.game_mode, 3);
    chk_w("POST drew its screen", first_of(0), 0xCCCA);
    /* game_mode is still 3 here, so the per-frame object build would run
     * if it were not gated on post_active - and would stamp ship and
     * enemy records straight over these labels. */
    chk("mid-game POST: mode still play", g.game_mode, 3);
    chk("object build left the list alone", post_clobber, 0);
    /* fewer than the boot screen: no marks are posted yet, and each
     * "  OK" adds its two glyphs. 446 with marks, 348 without. */
    chk_ge("mid-game POST still renders", render_vectors(), 340);

    /* and it leaves through GAME_INIT, ending that game */
    for (i = 0; i < 200 && g.post_step < 14; i++) step(0);
    hl_p1 = 0xFF; hl_p1 &= (uint8_t)~0x40;        /* fire, switch released  */
    g.tick_delta = 4; post_frame();                 /* -> inputs              */
    hl_p1 = 0xFF; g.tick_delta = 4; post_frame();
    for (i = 0; i < 8; i++) {                        /* walk out of the cycle  */
        hl_p1 = 0xFF; hl_p1 &= (uint8_t)~0x40; g.tick_delta = 4; post_frame();
        hl_p1 = 0xFF;                               g.tick_delta = 4; post_frame();
        if (!g.post_active) break;
    }
    chk("exits POST",                    g.post_active, 0);
    chk("exit went through GAME_INIT",   g.game_mode, 1);

    puts("");
    puts("=== POST never blocks: every frame returns ===");
    chk("(reaching here at all proves it)", 1, 1);

    printf("\n%s\n", fails ? "PROBE FAILED" : "PROBE PASSED");
    return fails ? 1 : 0;
}
