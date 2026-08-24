/* sound_samples.c - sample-based sound for the Omega Race C port.
 *
 * Design decision: do NOT emulate the sound Z80/AYs.
 * The game logic issues sound-command bytes exactly like the ROM wrote
 * port 0x14; this module maps each command to a pre-recorded sample.
 * Because samples trigger at the instant the command is issued, sound
 * stays in sync at any frame rate - the floating frame rate is a
 * non-issue for audio.
 *
 * Sample numbers equal the command byte; channels group interruptible
 * sounds. The command identities below are read off the sound-board ROM
 * itself - the dispatch table at sound_k5.bin $024A and each script's
 * F4/F5 start/kill opcodes - and are tabulated in disasm/SOUND_NOTES.md.
 * (The AAE sample driver's labels disagree in places - it calls 0x0e/
 * 0x0f "ship spawn" where the ROM makes them the two fastest background-
 * heartbeat tempos; the ROM table is authoritative.)
 *
 * What a sample model still cannot reproduce: the real board mixes six
 * tone/noise channels across two AY-3-8910s, and scripts contend for
 * individual registers rather than for whole voices. Commands within one
 * family kill each other exactly (that is modelled below, by channel), but
 * two commands from different families that happen to want the same AY
 * register on hardware will simply both be heard here.
 *
 * Host must provide:
 *   void plat_sample_start(int channel, int sample, int loop);
 *   void plat_sample_stop(int channel);
 */

#include "omega_state.h"

void plat_sample_start(int channel, int sample, int loop);
void plat_sample_stop(int channel);

/* Channel map. Each channel is one voice, so starting a sample on a channel
 * cuts whatever was on it - which is exactly how the ROM's scripts behave
 * toward each other. The groupings below are the sound ROM's own mutual
 * `F5` (kill) relationships, so a command silences precisely the commands
 * hardware would have silenced:
 *
 *   CH_ALARM  0x08 wall hit, 0x13 tilt      (tilt silences everything anyway)
 *   CH_MUSIC  0x04 wave tune, 0x14 beeps    (script 04 kills 14)
 *   CH_SHIP   0x01 player explosion
 *   CH_DROID  0x02, 0x10, 0x15              (each kills the other two)
 *   CH_CMDR   0x03, 0x11                    (each kills the other)
 *   CH_THRUST 0x09 sustain loop, stopped by 0x0A  (script 0A kills 09)
 *   CH_THRSTA 0x09's attack transient, same lifetime as CH_THRUST
 *   CH_SHOT   0x07 fire, 0x12 coin          (script 07 kills 12, and back)
 *   CH_BEAT   0x0B-0x0F heartbeat, 0x16 sweep (the five tempos each kill
 *                                              the other four)
 */
#define CH_ALARM   0
#define CH_MUSIC   1
#define CH_SHIP    2
#define CH_DROID   3
#define CH_CMDR    4
#define CH_THRUST  5
#define CH_SHOT    6
#define CH_BEAT    7
#define CH_THRSTA  8
#define CH_COUNT   9

/* Script 0 (`F5` x19 then every AY register zeroed) and the `F4 00` that
 * opens scripts 0x13 and 0x14: everything currently sounding stops. */
static void stop_all(void)
{
    int ch;
    for (ch = 0; ch < CH_COUNT; ch++)
        plat_sample_stop(ch);
}

void omega_hw_sound(uint8_t cmd)
{
    switch (cmd)
    {
    case 0x00: stop_all(); break;                          /* all sound off         */
    case 0x01: plat_sample_start(CH_SHIP,  0x01, 0); break; /* player ship explosion.
                                                              Its script ends with F4 00,
                                                              but OBJ_DESTROY already
                                                              sends cmd 0 immediately
                                                              before it, so the audible
                                                              result is the same       */
    case 0x02: plat_sample_start(CH_DROID, 0x02, 0); break; /* droid expl, tier 3    */
    case 0x03: plat_sample_start(CH_CMDR,  0x03, 0); break; /* cmd-ship expl, tier 1 */
    case 0x04: plat_sample_start(CH_MUSIC, 0x04, 0); break; /* wave-complete tune    */
    case 0x05: /* harmony voice of 0x04 - never latched by the main CPU */ break;
    case 0x06: /* harmony voice of 0x04 - never latched by the main CPU */ break;
    case 0x07: plat_sample_start(CH_SHOT,  0x07, 0); break; /* ship fire             */
    case 0x08: plat_sample_start(CH_ALARM, 0x08, 0); break; /* wall hit              */
    /* Thrust on. Script 0x09 has two phases: a 7-step volume decay burst,
     * then a 10-step noise-period settle that ends with FF and leaves the
     * noise generator running until 0x0A shuts it off. The sample set is
     * cut on exactly that seam - a.wav is the 0.118 s attack (peaks at 18%
     * FS, silent at both ends) and 9.wav is the sustain (a flat 2% FS hiss,
     * identical head to tail, meant to loop). Both start together on their
     * own channels, so the attack is heard over the top of the loop instead
     * of cutting it; the host has no scheduler to play them in sequence.
     * Confirmed against the real PCB. */
    case 0x09: plat_sample_start(CH_THRUST, 0x09, 1);
               plat_sample_start(CH_THRSTA, 0x0a, 0); break;
    case 0x0a: plat_sample_stop(CH_THRUST);          /* thrust off - script
                                                              0x0A is a pure stop
                                                              (F5 09 / vol=0 / FF),
                                                              it adds no new sound */
               plat_sample_stop(CH_THRSTA); break;
    case 0x0b: plat_sample_start(CH_BEAT,  0x0b, 1); break; /* heartbeat tempo 1 (slowest) */
    case 0x0c: plat_sample_start(CH_BEAT,  0x0c, 1); break; /* heartbeat tempo 2     */
    case 0x0d: plat_sample_start(CH_BEAT,  0x0d, 1); break; /* heartbeat tempo 3     */
    case 0x0e: plat_sample_start(CH_BEAT,  0x0e, 1); break; /* heartbeat tempo 4     */
    case 0x0f: plat_sample_start(CH_BEAT,  0x0f, 1); break; /* heartbeat tempo 5 (fastest) */
    case 0x10: plat_sample_start(CH_DROID, 0x10, 0); break; /* droid expl, tier 4    */
    case 0x11: plat_sample_start(CH_CMDR,  0x11, 0); break; /* cmd-ship expl,
                                                              kill_tier != 1        */
    case 0x12: plat_sample_start(CH_SHOT,  0x12, 0); break; /* coin                  */
    case 0x13: stop_all();                                 /* tilt: script opens F4 00 */
               plat_sample_start(CH_ALARM, 0x13, 0); break;
    case 0x14: stop_all();                                 /* bonus beeps: same F4 00.
                                                              This is what stops the
                                                              heartbeat under the
                                                              wave-clear award     */
               plat_sample_start(CH_MUSIC, 0x14, 0); break;
    case 0x15: plat_sample_start(CH_DROID, 0x15, 0); break; /* droid expl, default   */
    case 0x16: plat_sample_start(CH_BEAT,  0x16, 1); break; /* self-test sweep (loops) */
    default:   /* >= 0x17: the sound board's `cp $17 / jr nc` drops it */ break;
    }
}
