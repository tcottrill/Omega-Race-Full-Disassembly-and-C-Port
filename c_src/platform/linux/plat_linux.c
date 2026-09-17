/* plat_linux.c - Omega Race C port: Linux backend STUB.
 *
 * Nothing here is implemented yet; this file states the plan and
 * compiles clean (cl /c /W4 checks it in build_all.bat; the POSIX parts
 * are guarded by __linux__) so the contract cannot drift away from it.
 *
 * The plan:
 *  - video: an OpenGL context (GLFW or EGL+X11/Wayland) with the same
 *    DVG-space ortho and line/point sink as the Windows backend.
 *  - input: keyboard via the windowing layer, mouse-x spinner deltas,
 *    same bindings as the Windows core.
 *  - audio: ALSA or a small SDL_mixer-free wav player over the hex-named
 *    sample set. (No SDL2 framework dependency; a bare audio lib
 *    is still open.)
 *  - time: clock_gettime(CLOCK_MONOTONIC).
 *  - NVRAM: omega_c.nv in the working directory, same blob.
 *  - main(): identical shape to plat_win.c's -
 *        plat_init(); omega_app_init();
 *        while (!quit) { wait = omega_app_step(plat_now_ms());
 *                        if (wait > 3.0) plat_sleep_ms(1); }
 *        omega_hw_nvram_save(); plat_shutdown();
 */
#include <stdio.h>
#include "../omega_platform.h"

#ifdef __linux__
#include <time.h>
#include <unistd.h>
#endif

int  plat_init(void)     { return 0; }          /* TODO: window/GL/audio */
void plat_shutdown(void) {}

void plat_video_begin(void)   {}                /* TODO: glClear */
void plat_video_line(float x0, float y0, float x1, float y1, int z)
{ (void)x0; (void)y0; (void)x1; (void)y1; (void)z; }
void plat_video_present(void) {}                /* TODO: swap buffers */

void plat_input_poll(plat_inputs* in)
{
    /* TODO: keyboard + mouse spinner */
    in->fire = in->thrust = in->coin1 = in->coin2 = 0;
    in->start1 = in->start2 = in->test = in->diag = in->quit = 0;
    in->spinner_delta = 0;
}

uint8_t plat_dsw_c4(void) { return 0x5f; }
uint8_t plat_dsw_c6(void) { return 0xbf; }

void plat_sample_start(int channel, int sample, int loop)
{ (void)channel; (void)sample; (void)loop; }
void plat_sample_stop(int channel) { (void)channel; }

int  plat_sound_use_ay(void) { return 0; }               /* TODO: ALSA stream */
int  plat_audio_open(int sample_rate) { (void)sample_rate; return -1; }
void plat_audio_push(const int16_t* pcm, int frames) { (void)pcm; (void)frames; }
void plat_audio_close(void) {}

double plat_now_ms(void)
{
#ifdef __linux__
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
#else
    return 0.0;
#endif
}

void plat_sleep_ms(int ms)
{
#ifdef __linux__
    usleep((useconds_t)ms * 1000);
#else
    (void)ms;
#endif
}

int plat_nvram_read(void* buf, unsigned len)
{
    size_t got;
    FILE* f = fopen("omega_c.nv", "rb");
    if (!f) return 1;
    got = fread(buf, 1, len, f);
    fclose(f);
    return got == len ? 0 : 1;
}

int plat_nvram_write(const void* buf, unsigned len)
{
    FILE* f = fopen("omega_c.nv", "wb");
    if (!f) return 1;
    fwrite(buf, 1, len, f);
    fclose(f);
    return 0;
}

void plat_leds_out(uint8_t led_shadow) { (void)led_shadow; }
void plat_status_text(const char* s)   { (void)s; }   /* TODO: window title */
