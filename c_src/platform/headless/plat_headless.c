/* plat_headless.c - Omega Race C port: shared harness backend.
 *
 * One home for the host stubs every harness build shares (see
 * plat_headless.h). No window, no sound, no
 * clock: harnesses drive machine time themselves by calling
 * omega_irq_244hz / omega_mainloop_tick / omega_mainloop_frame directly.
 */
#include <stddef.h>
#include "../../omega_state.h"
#include "plat_headless.h"

omega_state g;

uint8_t hl_p1 = 0xff, hl_p2 = 0xff;
uint8_t hl_spinner = 0x04, hl_spinner2 = 0x00;
uint8_t hl_dsw_c4 = 0x5f, hl_dsw_c6 = 0xbf;

void (*hl_vec_line_hook)(float, float, float, float, int) = NULL;
void (*hl_sample_start_hook)(int, int, int) = NULL;
void (*hl_sample_stop_hook)(int) = NULL;
void (*hl_audio_push_hook)(const int16_t* pcm, int frames) = NULL;
void (*hl_nvram_save_hook)(void) = NULL;
void (*hl_nvram_load_hook)(void) = NULL;

/* ---- the omega_hw_* seam, raw ---------------------------------------- */

uint8_t omega_hw_in_p1(void)    { return hl_p1; }
uint8_t omega_hw_in_p2(void)    { return hl_p2; }
uint8_t omega_hw_spinner(void)  { return hl_spinner; }
uint8_t omega_hw_spinner2(void) { return hl_spinner2; }
uint8_t omega_hw_dsw_c4(void)   { return hl_dsw_c4; }
uint8_t omega_hw_dsw_c6(void)   { return hl_dsw_c6; }
void    omega_hw_leds_out(uint8_t v) { (void)v; }

void omega_hw_nvram_save(void) { if (hl_nvram_save_hook) hl_nvram_save_hook(); }
void omega_hw_nvram_load(void) { if (hl_nvram_load_hook) hl_nvram_load_hook(); }

/* ---- the plat_* sinks the core references directly ------------------- */

void plat_video_line(float x0, float y0, float x1, float y1, int z)
{
    if (hl_vec_line_hook) hl_vec_line_hook(x0, y0, x1, y1, z);
}

void plat_sample_start(int channel, int sample, int loop)
{
    if (hl_sample_start_hook) hl_sample_start_hook(channel, sample, loop);
}

void plat_sample_stop(int channel)
{
    if (hl_sample_stop_hook) hl_sample_stop_hook(channel);
}

int  plat_sound_use_ay(void) { return 0; }
int  plat_audio_open(int sample_rate) { (void)sample_rate; return 0; }
void plat_audio_push(const int16_t* pcm, int frames)
{
    if (hl_audio_push_hook) hl_audio_push_hook(pcm, frames);
}
void plat_audio_close(void) {}
