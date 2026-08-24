/* plat_teensy.c - Omega Race C port: Teensy backend STUB.
 *
 * Target: a Teensy 4.x driving a REAL VECTOR CRT through X/Y/Z DACs -
 * the beam draws the segments itself. Nothing here is implemented yet;
 * this file states the plan and compiles clean (cl /c /W4 checks it in
 * build_all.bat) so the contract cannot drift away from it.
 *
 * The plan:
 *  - video: plat_video_line APPENDS to a per-frame segment buffer;
 *    plat_video_present kicks a DMA/timer-driven replay that steps the
 *    X/Y DACs (SPI dual-DAC, e.g. MCP4922, or the on-chip DACs) along
 *    each segment at beam speed with Z driving intensity. The replay
 *    time IS the frame period - the machine's own pacing physics - so
 *    the sketch loop skips omega_app_step entirely:
 *        loop() { if (replay_idle()) omega_app_frame(plat_now_ms()); }
 *    with an IntervalTimer at 244.140625 Hz calling
 *    omega_app_advance(plat_now_ms()) for machine time (ISR-safe arm).
 *  - input: real panel wiring - buttons to GPIOs (active low, debounced
 *    by the core's own coin logic), a quadrature encoder for the
 *    spinner (delta per poll), physical DIP switches read by
 *    plat_dsw_c4/c6.
 *  - audio: the Teensy Audio Library playing the hex-named sample set
 *    from flash or SD on mixer channels.
 *  - time: micros()/1000.0.
 *  - NVRAM: EEPROM.put/get of the core's blob (182 bytes today).
 *  - LEDs: plat_leds_out drives the real coin-meter / start-LED pins.
 *
 * Build story: the Arduino/Teensyduino sketch wraps this file; the
 * ARDUINO sections provide setup()/loop(). The core C files compile as
 * C11 under the same toolchain (arm-none-eabi-gcc).
 */
#include "../omega_platform.h"

#ifdef ARDUINO
#include <Arduino.h>
#endif

int  plat_init(void)     { return 0; }          /* TODO: DACs, pins, audio */
void plat_shutdown(void) {}

void plat_video_begin(void)   {}                /* TODO: reset segment buffer */
void plat_video_line(float x0, float y0, float x1, float y1, int z)
{ (void)x0; (void)y0; (void)x1; (void)y1; (void)z; /* TODO: append segment */ }
void plat_video_present(void) {}                /* TODO: kick DAC replay */

void plat_input_poll(plat_inputs* in)
{
    /* TODO: GPIOs + quadrature spinner */
    in->fire = in->thrust = in->coin1 = in->coin2 = 0;
    in->start1 = in->start2 = in->test = in->diag = in->quit = 0;
    in->spinner_delta = 0;
}

uint8_t plat_dsw_c4(void) { return 0x5f; }      /* TODO: physical DIPs */
uint8_t plat_dsw_c6(void) { return 0xbf; }

void plat_sample_start(int channel, int sample, int loop)
{ (void)channel; (void)sample; (void)loop; }    /* TODO: Audio Library */
void plat_sample_stop(int channel) { (void)channel; }

double plat_now_ms(void)
{
#ifdef ARDUINO
    return (double)micros() / 1000.0;
#else
    return 0.0;
#endif
}
void plat_sleep_ms(int ms) { (void)ms; }

int plat_nvram_read(void* buf, unsigned len)
{ (void)buf; (void)len; return 1; }             /* TODO: EEPROM.get */
int plat_nvram_write(const void* buf, unsigned len)
{ (void)buf; (void)len; return 1; }             /* TODO: EEPROM.put */

void plat_leds_out(uint8_t led_shadow) { (void)led_shadow; }
void plat_status_text(const char* s)   { (void)s; }

#ifdef ARDUINO
/* the sketch shell - see the plan above */
void setup(void)
{
    plat_init();
    omega_app_init();
    /* TODO: IntervalTimer irq; irq.begin(tick_isr, 4096); with
     * tick_isr() { omega_app_advance(plat_now_ms()); } */
}

void loop(void)
{
    /* TODO: if (replay_idle()) omega_app_frame(plat_now_ms()); */
}
#endif
