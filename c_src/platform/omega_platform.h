/* omega_platform.h - the platform contract for the Omega Race C port.
 *
 * The core (game modules + dvg + app_loop.c) is platform-agnostic and
 * calls ONLY these functions for rendering, input, audio, time and
 * storage. Each backend under platform/<name>/ implements the whole
 * contract in plain C; the build links exactly one backend per binary.
 * Link-time binding, no function pointers - /W4 catches signature drift,
 * and the Teensy pays no indirection per line segment.
 *
 * Backends:
 *   windows/   the Windows core (Win32 + OpenGL 3.3);
 *              owns WinMain(). Builds omega_win.exe.
 *   headless/  shared harness backend (probes, test_drive, wbtest): raw
 *              port injection + capture hooks, no window/sound/clock.
 *              NOTE: harness builds link this INSTEAD of app_loop.c and
 *              implement the omega_hw_* seam directly (raw port bytes are
 *              the right level for hardware-differential tests).
 *   linux/     stub: clock_gettime/evdev-or-X/ALSA plan.
 *   teensy/    stub: real vector CRT via X/Y/Z DACs; the beam paces.
 */
#ifndef OMEGA_PLATFORM_H
#define OMEGA_PLATFORM_H

#include <stdint.h>

/* ---- lifecycle --------------------------------------------------------- */

int  plat_init(void);      /* window/DACs/audio up; 0 = ok, nonzero = fail */
void plat_shutdown(void);

/* ---- video: the segment sink -------------------------------------------
 * The DVG walker (core, dvg.c) emits every visible segment through
 * plat_video_line in DVG space (0..1023, y up), z = intensity 0..7 as the
 * PROM state machine produced it (the GUI maps it (z<<4)|0x0f). A
 * zero-length segment is a point (shot sparks). A raster backend draws
 * immediately between begin/present; a vector-CRT backend buffers the
 * frame's segments and replays them to the DACs at beam speed after
 * present - the replay time IS the frame pacing there. */
void plat_video_begin(void);                    /* start of frame / clear */
void plat_video_line(float x0, float y0, float x1, float y1, int z);
void plat_video_present(void);                  /* flip / kick DAC replay */

/* ---- input: abstract controls ------------------------------------------
 * Polled once per displayed frame by the core (app_loop.c), which owns
 * the hardware truth - active-low port-bit encodings, the 64-entry
 * spinner Gray-code table, the accumulated spinner angle. Key bindings
 * are backend policy. Fields are 0/1 except spinner_delta. */
typedef struct {
    uint8_t fire, thrust;
    uint8_t coin1, coin2;
    uint8_t start1, start2;
    uint8_t test;            /* cabinet test-switch line                 */
    uint8_t diag;            /* host affordance: F2 = enter diagnostics  */
    uint8_t quit;
    int16_t spinner_delta;   /* signed steps since last poll; positive
                                turns the same direction as KEY_RIGHT    */
} plat_inputs;

void plat_input_poll(plat_inputs* in);

/* DIP banks - platform-owned so the Teensy can wire the real switches.  */
uint8_t plat_dsw_c4(void);
uint8_t plat_dsw_c6(void);

/* ---- audio: sample playback (no sound CPU, by design) ------------------
 * sound_samples.c (core) owns the command -> channel/sample map and the
 * ROM's mutual-kill sets; the backend just plays sample #n (hex-named
 * wav on the PC targets) on a mixer channel. */
void plat_sample_start(int channel, int sample, int loop);
void plat_sample_stop(int channel);

/* ---- time --------------------------------------------------------------- */

double plat_now_ms(void);            /* monotonic, any epoch              */
void   plat_sleep_ms(int ms);        /* scheduling hint; may return early */

/* ---- storage: NVRAM as one opaque blob ---------------------------------
 * The core owns the layout (the omega_c.nv byte order);
 * the backend stores bytes - file on the PC targets, EEPROM on Teensy.
 * Return 0 = ok, nonzero = no stored image / write failed. */
int plat_nvram_read(void* buf, unsigned len);
int plat_nvram_write(const void* buf, unsigned len);

/* ---- misc --------------------------------------------------------------- */

void plat_leds_out(uint8_t led_shadow);  /* out($13): coin meters,
                                            start LEDs, screen flip; real
                                            pins on the Teensy            */
void plat_status_text(const char* s);    /* pacing/fps stats, ~1/s; the
                                            window title on PC, no-op
                                            elsewhere                     */

/* ---- core app loop (app_loop.c) - what a backend's main() calls -------- */

void   omega_app_init(void);            /* dvg_init + zero g + post_boot  */
void   omega_app_advance(double now_ms);/* machine time: run every elapsed
                                           244.14 Hz tick (IRQ + free-
                                           running mainloop arm); 100 ms
                                           stall guard. ISR-safe pacing
                                           source on the Teensy.          */
void   omega_app_frame(double now_ms);  /* one displayed frame: poll+map
                                           input, F2 edge, mainloop frame
                                           arm, build list, begin/render/
                                           present, stats                 */
double omega_app_step(double now_ms);   /* the composed PC pass: advance,
                                           then frame when the absolute
                                           cost-model schedule says it is
                                           due. Returns ms until next due
                                           (0 = a frame just ran). A
                                           backend that paces itself (the
                                           Teensy: DAC replay complete)
                                           skips step and calls advance +
                                           frame directly.                */

#endif
