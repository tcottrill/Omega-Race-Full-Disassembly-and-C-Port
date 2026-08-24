/* plat_headless.h - injection/capture hooks for the shared harness backend.
 *
 * Harness builds (probes, test_drive, wbtest) link plat_headless.c
 * INSTEAD of app_loop.c + a real backend: it owns omega_state g and
 * implements the omega_hw_* seam as raw injectable port bytes - the
 * right level for differential tests against the real ROMs - plus the
 * two plat_* sinks the core references directly (plat_video_line from
 * dvg.c, plat_sample_* from sound_samples.c), dispatched to optional
 * capture hooks.
 */
#ifndef PLAT_HEADLESS_H
#define PLAT_HEADLESS_H

#include <stdint.h>

/* raw port bytes, injected by the harness (defaults in parentheses) */
extern uint8_t hl_p1;         /* port 0x11, active low          (0xff) */
extern uint8_t hl_p2;         /* port 0x12, active low          (0xff) */
extern uint8_t hl_spinner;    /* port 0x15                      (0x04) */
extern uint8_t hl_spinner2;   /* port 0x16, cocktail            (0x00) */
extern uint8_t hl_dsw_c4;     /* DIP bank C4                    (0x5f) */
extern uint8_t hl_dsw_c6;     /* DIP bank C6                    (0xbf) */

/* capture hooks, NULL = discard */
extern void (*hl_vec_line_hook)(float x0, float y0, float x1, float y1, int z);
extern void (*hl_sample_start_hook)(int channel, int sample, int loop);
extern void (*hl_sample_stop_hook)(int channel);

/* NVRAM hooks, NULL = no chip (save discards, load leaves g untouched) */
extern void (*hl_nvram_save_hook)(void);
extern void (*hl_nvram_load_hook)(void);

#endif
