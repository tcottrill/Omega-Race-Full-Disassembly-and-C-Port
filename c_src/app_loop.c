/* app_loop.c - Omega Race C port: the platform-agnostic application loop.
 *
 *  - owns omega_state g and boots via post_boot() (ROM boot order)
 *  - runs the 244 Hz service on machine time (omega_app_advance)
 *  - maps abstract platform inputs to the hardware's port bytes: the
 *    active-low bit encodings, the 64-entry spinner Gray-code table and
 *    the accumulated spinner angle live HERE, not in a backend - they
 *    are hardware truth, key bindings are backend policy
 *  - serializes NVRAM (the omega_c.nv byte order) over the backend's
 *    blob store
 *  - paces displayed frames on the authentic cost model (absolute
 *    schedule + EMA, see FRAME_PACING_NOTES.md) in omega_app_step; a
 *    backend that paces itself (Teensy: the beam) calls omega_app_frame
 *    directly instead
 *
 * Harness builds (probes/test_drive/wbtest) do NOT link this file: they
 * link platform/headless/plat_headless.c, which implements the
 * omega_hw_* seam below as raw injectable port bytes - the right level
 * for differential tests against the real ROMs.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "omega_state.h"
#include "platform/omega_platform.h"

omega_state g;

/* module entry points */
extern void omega_irq_244hz(void);
extern void omega_mainloop_tick(void);   /* free-running arm, per tick  */
extern void omega_mainloop_frame(void);  /* object arm, per frame       */
extern void post_boot(void);   /* post.c: POST_START / POST_TESTSW_GATE */
extern void post_enter(void);  /* post.c: F2 - diagnostics from anywhere */

/* sound_board.c: the synthesized sound board (backend opt-in) */
extern int  omega_sound_ay;
extern void sndboard_init(int sample_rate);
extern void sndboard_tick(void);

#define OMEGA_AUDIO_RATE 48000

/* ------------------------------------------------------------------ */
/* input mapping: plat_inputs -> the hardware's port bytes             */
/* ------------------------------------------------------------------ */

static unsigned char spinnerTable[64] = {
    0x00, 0x04, 0x14, 0x10, 0x18, 0x1c, 0x5c, 0x58,
    0x50, 0x54, 0x44, 0x40, 0x48, 0x4c, 0x6c, 0x68,
    0x60, 0x64, 0x74, 0x70, 0x78, 0x7c, 0xfc, 0xf8,
    0xf0, 0xf4, 0xe4, 0xe0, 0xe8, 0xec, 0xcc, 0xc8,
    0xc0, 0xc4, 0xd4, 0xd0, 0xd8, 0xdc, 0x9c, 0x98,
    0x90, 0x94, 0x84, 0x80, 0x88, 0x8c, 0xac, 0xa8,
    0xa0, 0xa4, 0xb4, 0xb0, 0xb8, 0xbc, 0x3c, 0x38,
    0x30, 0x34, 0x24, 0x20, 0x28, 0x2c, 0x0c, 0x08
};

static plat_inputs cur_in;
static int spin_angle = 1;

uint8_t omega_hw_in_p1(void)
{
    uint8_t v = 0xff;                             /* active low */
    if (cur_in.fire)   v &= (uint8_t)~0x40;
    if (cur_in.thrust) v &= (uint8_t)~0x20;
    if (cur_in.coin1)  v &= (uint8_t)~0x01;
    if (cur_in.coin2)  v &= (uint8_t)~0x02;
    if (cur_in.test)   v &= (uint8_t)~0x80;
    return v;
}

uint8_t omega_hw_in_p2(void)
{
    uint8_t v = 0xff;
    if (cur_in.start1) v &= (uint8_t)~0x40;
    if (cur_in.start2) v &= (uint8_t)~0x80;
    return v;
}

uint8_t omega_hw_spinner(void)  { return spinnerTable[spin_angle & 63]; }
uint8_t omega_hw_spinner2(void) { return (uint8_t)(spin_angle & 63); }

uint8_t omega_hw_dsw_c4(void) { return plat_dsw_c4(); }
uint8_t omega_hw_dsw_c6(void) { return plat_dsw_c6(); }

void omega_hw_leds_out(uint8_t led_shadow) { plat_leds_out(led_shadow); }

/* ------------------------------------------------------------------ */
/* NVRAM: the blob layout is core-owned and byte-identical to the      */
/* omega_c.nv file (fwrite order: hiscore_bank, nvram_template,        */
/* bookkeep, nvram_credits), so existing saves load.                   */
/* ------------------------------------------------------------------ */

#define NV_BLOB_LEN (sizeof g.hiscore_bank + sizeof g.nvram_template + \
                     sizeof g.bookkeep + sizeof g.nvram_credits)

void omega_hw_nvram_save(void)
{
    uint8_t blob[NV_BLOB_LEN];
    uint8_t* p = blob;
    memcpy(p, g.hiscore_bank,   sizeof g.hiscore_bank);   p += sizeof g.hiscore_bank;
    memcpy(p, g.nvram_template, sizeof g.nvram_template); p += sizeof g.nvram_template;
    memcpy(p, g.bookkeep,       sizeof g.bookkeep);       p += sizeof g.bookkeep;
    memcpy(p, g.nvram_credits,  sizeof g.nvram_credits);
    plat_nvram_write(blob, (unsigned)sizeof blob);
}

void omega_hw_nvram_load(void)
{
    uint8_t blob[NV_BLOB_LEN];
    const uint8_t* p = blob;
    if (plat_nvram_read(blob, (unsigned)sizeof blob) != 0) return;
    memcpy(g.hiscore_bank,   p, sizeof g.hiscore_bank);   p += sizeof g.hiscore_bank;
    memcpy(g.nvram_template, p, sizeof g.nvram_template); p += sizeof g.nvram_template;
    memcpy(g.bookkeep,       p, sizeof g.bookkeep);       p += sizeof g.bookkeep;
    memcpy(g.nvram_credits,  p, sizeof g.nvram_credits);
}

/* ------------------------------------------------------------------ */
/* the loop                                                            */
/* ------------------------------------------------------------------ */

void omega_app_init(void)
{
    dvg_init();
    memset(&g, 0, sizeof g);
    /* Sound path, before anything can issue a command: the synthesized
     * sound board when the backend asks for it (the Windows default) AND
     * can give us a stream to put it on, otherwise the samples. */
    if (plat_sound_use_ay() && plat_audio_open(OMEGA_AUDIO_RATE) == 0) {
        sndboard_init(OMEGA_AUDIO_RATE);
        omega_sound_ay = 1;
    }
    /* ROM boot order: POST_START -> POST_TESTSW_GATE, which jumps to
     * GAME_INIT when the test switch is released and runs the self test
     * while it is held. post_boot() is that gate; it calls game_init()
     * itself on the released path. Hold the test key at startup for the
     * diagnostics. */
    post_boot();
}

/* Machine time. The 244 Hz service and the free-running mainloop arm run
 * for every elapsed tick - including while the host waits for the next
 * frame to be due, exactly as the hardware interrupt kept firing while
 * MAINLOOP polled the DVG busy flag. */
void omega_app_advance(double now_ms)
{
    static double last_ms = -1.0;
    static double irq_acc = 0.0;

    if (last_ms < 0.0) last_ms = now_ms;
    irq_acc += now_ms - last_ms;
    last_ms = now_ms;
    if (irq_acc > 100.0) irq_acc = 100.0;        /* stall guard */

    while (irq_acc >= OMEGA_TICK_MS) {           /* the board's IRQ */
        /* The sound board's NMI is this same GBNMI net. Its pass goes
         * first: on the board it ran at once, in under a tick, so a
         * command the main CPU issues during a tick is picked up by the
         * sound board's NEXT pass - as it is here. */
        if (omega_sound_ay) sndboard_tick();
        omega_irq_244hz();
        irq_acc -= OMEGA_TICK_MS;
        omega_mainloop_tick();
    }
}

/* One displayed frame: input, game object arm, display list, render.
 * Also the delivery statistics - measured flip-to-flip intervals, which
 * with vsync off are a direct read on the emulation's own steadiness. */
static double period_ema = 0.0;    /* shared with omega_app_step's pacing */

void omega_app_frame(double now_ms)
{
    static int    diag_prev = 0;
    static double last_flip_ms = 0.0, fps_t0 = -1.0;
    static double jit_min = 1e9, jit_max = 0.0, jit_sum = 0.0;
    static int    jit_n = 0, fps_frames = 0;

    plat_input_poll(&cur_in);
    spin_angle = (spin_angle + cur_in.spinner_delta) & 63;

    /* F2: enter the diagnostics from wherever we are. Edge-triggered
     * host affordance - the ROM only reaches POST at power-on; see
     * post_enter()'s comment in post.c. post_active is checked first in
     * omega_mainloop_pass(), so this takes precedence over the operator
     * page the same switch line opens. */
    if (cur_in.diag && !diag_prev) post_enter();
    diag_prev = cur_in.diag;

    omega_mainloop_frame();                 /* object arm: one pass
                                               per displayed frame  */
    dvg_build_object_list();                /* game -> display list */
    plat_video_begin();
    dvg_render();                           /* DVG state machine    */
    plat_video_present();

    if (fps_t0 < 0.0) fps_t0 = now_ms;
    if (last_flip_ms > 0.0) {
        double d = now_ms - last_flip_ms;
        if (d < jit_min) jit_min = d;
        if (d > jit_max) jit_max = d;
        jit_sum += d;
        jit_n++;
    }
    last_flip_ms = now_ms;

    fps_frames++;
    if (now_ms - fps_t0 >= 1000.0) {
        char buf[128];
        snprintf(buf, sizeof buf,
                 "%d fps  frame %.1f ms (min %.1f max %.1f, target %.1f)",
                 fps_frames,
                 jit_n ? jit_sum / jit_n : 0.0,
                 jit_n ? jit_min : 0.0,
                 jit_n ? jit_max : 0.0,
                 period_ema);
        plat_status_text(buf);
        jit_min = 1e9; jit_max = 0.0; jit_sum = 0.0; jit_n = 0;
        fps_frames = 0;
        fps_t0 = now_ms;
    }
}

/* The composed PC pass: authentic frame pacing (see
 * FRAME_PACING_NOTES.md). Each frame's period is the exact
 * DVG draw time of what was just put on screen plus the serialized Z80
 * gap (dvg_cost_frame_ms), EMA-smoothed; the schedule is ABSOLUTE
 * (next_due += period) so oversleeps repay their debt instead of slowing
 * the game, resyncing only after a real stall. */
double omega_app_step(double now_ms)
{
    static double next_due = -1.0;

    omega_app_advance(now_ms);

    if (next_due < 0.0) next_due = now_ms;
    if (now_ms < next_due - 0.05)               /* not due yet */
        return next_due - now_ms;

    omega_app_frame(now_ms);

    {
        double cost = dvg_cost_frame_ms();
        if (period_ema <= 0.0) period_ema = cost;
        period_ema += 0.25 * (cost - period_ema);
        next_due += period_ema;
    }
    if (now_ms - next_due > 100.0) next_due = now_ms;   /* stall resync */
    return 0.0;
}
