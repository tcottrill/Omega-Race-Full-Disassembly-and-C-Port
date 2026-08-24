/* glue.c - Omega Race C port: cross-module bridges.
 *
 * Small owners/adapters for functions that fall between the module
 * boundaries. Each is marked with where it should eventually live.
 */
#include <string.h>
#include "omega_state.h"

extern void attract_init(void);                 /* mainline.c */
extern int  nvram_boot_load(void);              /* score.c    */
extern void hiscore_context_activate(int ctx);  /* score.c    */
extern void hiscore_context_sync(int ctx);      /* score.c    */
extern void nvram_save_all(void);               /* score.c    */

/* ---- SOUND_CMD_SEND 0x2E04 (no module owned it) ---------------------
 * Gate logic per disasm/FUNCTIONS.md:
 *  - cmd 0 is always sent (silence/reset), ROM also idled 4 ticks.
 *  - otherwise requires the sound-enable gate (svc_flags bit7).
 *  - with timer_seconds==0, only sent during play (mode 3).
 *  - with timer_seconds!=0, requires sound_ready and dedups against
 *    last_sound_cmd (which only callers like HEARTBEAT_SOUND update).
 * Belongs in: sound_samples.c eventually. */
void sound_cmd_send(uint8_t cmd)
{
    if (cmd == 0) { omega_hw_sound(0); return; }
    if (!(g.svc_flags & 0x80)) return;
    if (g.timer_seconds == 0) {
        if (g.game_mode != 3) return;
    } else {
        if (!g.sound_ready) return;
        if (cmd == g.last_sound_cmd) return;
    }
    omega_hw_sound(cmd);
}

/* ---- display hooks that are core behavior, not host behavior --------
 * DLIST_RESET is a vector-RAM operation the DVG module owns; the
 * present is handled by the app loop's plat_video_present
 * (VG_RESTART_FRAME's kick body has no equivalent work in the port). */
void omega_display_reset(void) { dvg_dlist_reset(); }
void omega_frame_present(void) { }

/* ---- GAME_INIT's NVRAM validate bundle ------------------------------
 * mainline.c expects nvram_validate_all(); score.c implements the
 * whole validate-or-wipe block as nvram_boot_load(). */
int nvram_validate_all(void) { return nvram_boot_load(); }

/* ---- PLAY_BEGIN hiscore bank selection ------------------------------
 * frame.c expects hiscore_select_working_table(use_alternate);
 * score.c's hiscore_context_activate(ctx) is that operation. */
void hiscore_select_working_table(int use_alternate)
{
    hiscore_context_activate(use_alternate ? 1 : 0);
}

/* (no mode-4 bridge here: HISCORE_ENTRY_FRAME 0x128E-0x12CF is
 * mainline.c's hiscore_entry_finish / hiscore_entry_frame_resume,
 * including the 2P player-switch through frame.c's fe_respawn_tick_1478.) */
