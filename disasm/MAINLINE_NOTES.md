# Omega Race — GAME_INIT, the main loop, and the attract cycle

Continues BOOT_AND_IRQ_NOTES.md. All `(iy±d)` assume IY = 0x4080.

## GAME_INIT (0x0955) — cold start into the mainline

    sp=0x4BFC, iy=0x4080, clear 0x4000-0x4481, led_shadow=0xFF
    call 0x18BC          ; WALL_BUFFERS_BUILD: ship heading frame sets
                         ; into vector RAM (see GAMEPLAY_NOTES)
    decode DSW C6 coinage -> coinage1_ptr/coinage2_ptr (table 0x2FC3)
    copy default high-score table 0x2FF7 -> 0x43A9, dup -> 0x43D3

then NVRAM (0x5C00, 4-bit chips, everything stored as nibbles) is
validated in full:

  - 2x16 nibbles at 0x5C1A must match the template at ROM 0x30C9
  - high-score records at 0x5C3A: NVRAM_RD_BYTE (0x0A18) re-packs two
    nibbles via RRD and rejects any BCD digit > 9; initials must be
    'A'-'Z' or space
  - 0x40 bookkeeping nibbles at 0x5C58 -> bookkeep_ctrs (0x4427)
  - credits themselves are persisted (0x5C56 -> credits_bcd)

Any failure -> NVRAM_WIPE (0x0A3B): credits=0, bookkeeping zeroed, then
SCORES_RESET (0x0A4C): default scores restored and NVRAM_SAVE_ALL
(0x0A68) rewrites every record + template. Either way -> ATTRACT_INIT.

## The mode machine

`game_mode` (0x4024, aka (iy-92)) is the master state:

| mode | meaning | set at |
|------|---------|--------|
| 1 | attract | ATTRACT_INIT 0x0A9D (also re-entered from steps 5/6, hiscore exit 0x29EB/0x2A8E) |
| 2 | game starting / ship spawn (also the attract self-play demo) | GAME_START 0x0B83 |
| 3 | playing | 0x0BD1 |
| 4 | high-score entry | 0x2A2C |

`MAINLOOP` (0x0F39) runs every pass regardless of mode:

    iy=0x4080, watchdog
    tick_delta = tick_244hz - tick_prev      ; motion scale for this pass
    on 8-bit wrap of the subtraction (~ once per second):
        seconds_ctr++ ; machine-on-time bookkeeping (0x16D5)
        slam alarm timeout   (timer_seconds, then SOUND_CMD_SEND(0/9))
        wave_pace_timer (0x4022, 25 s) countdown; expiry sets bit7 of
          0x406A — the flag the enemy fire cycle waits for
        wave_start_timer (0x4023) countdown; expiry sets 0x406A bit6 +
          wave_flags (0x406B) bit7 "wave started" (gates thrust in
          OBJ_PHYSICS), and blanks list slot 0x8242
      NOTE these two run at 1 Hz because they are INSIDE the tick-wrap
      block (0x0F4F jr nc skips 0x0F51-0x0FCB between wraps) — running
      them per frame makes specials appear ~60x early
    0x4070 -= tick_delta (floor 0)           ; fire cooldown, also the
                                             ; page-scrawl rate limiter
    INPUTS_SAMPLE                            ; ports 11/12 + edges
    if mode has bit1 (2,3):
        VG_RESTART_FRAME                     ; the DVG heartbeat
        OBJECTS_UPDATE
        mode 3 -> FRAME_ENGINE               ; gameplay
        mode 2 -> start check, coin screens, timeout back to attract
    else (1,4):
        START_BTN_CHECK: credits and start edge -> GAME_START
        mode 4 -> HISCORE_ENTRY_FRAME (0x127B: kick VG via 0x180D,
                  entry logic 0x2A35, then FRAME_ENGINE)
        mode 1 -> test switch held? -> OPERATOR_MENU
                  else ATTRACT_STEP_DISP

Two details worth keeping:

- OBJECTS_UPDATE (0x1965) points **SP at ROM 0x2E90** and `pop`s object
  record pointers (32 records); each object's movement is scaled by
  tick_delta. This is the mechanism that makes game speed independent of
  frame rate (see FRAME_PACING_NOTES.md), and it's why `ld ix,$2E90`
  looks like a data load — 0x2E90 is a pointer table, not code.
- FRAME_ENGINE (0x12D2) starts by reading the spinner (SPINNER_READ),
  decoding it through the 64-entry table at 0x3061 into ship_angle
  (0x40BC), gated on mode and the ship-alive bit ((iy+51) bit 5).

## The attract cycle

ATTRACT_INIT resets per-game state, honors free play (DSW C6 bit 6 ->
credits forced to 4), and if credits exist immediately decodes DSW C4
(extra-life threshold 1 via 0x2FE5, thresholds 2/3 via 0x2FED into
0x4060/62/64, lives via 0x2FD3 — lives also drawn by blanking ship
icons at list slots 0x80F0/0x8130-0x8134).

(Correction to an earlier draft of these notes: the routine at 0x2199,
once described here as a display-data copier, is actually COLLIDE_SCAN —
the ship-vs-object bounding-box collision engine. See GAMEPLAY_NOTES.)

With no credits it runs the attract sequence. `attract_step` (0x4083)
advances on per-step second timers (`timer_seconds`); each step picks a
text page (`page_id` -> DRAW_PAGE, pointer table at ROM 0x3F15):

| step | action | page | duration |
|------|--------|------|----------|
| 1 | (display only) | - | - |
| 2 | title page | 1 | 15 s |
| 3 | DLIST_RESET + instructions | 2 | 6 s |
| 4 | instructions 2 | 3 | 8 s |
| 5 | high-score table | 0x0F | 10 s |
| 6 | high-score table p2 | 0x10 | 10 s |
| 7 | **self-play demo**: 30 s timer, jp GAME_START | - | 30 s |
| 8/9 | score tables again | 0x0F/0x10 | 10 s |
| 10 | pricing page: coin/credit digits computed live from coinage1_ptr into slots 0x8024/0x803A/0x8054/0x806A (page 0x14), unless credits arrived | 0x14 | 5 s |
| 11/12 | score tables | 0x0F/0x10 | 10 s |
| else | wrap -> ATTRACT_INIT | | |

The demo (step 7) enters GAME_START with no credits; GAME_START notices
`attract_step == 7 || credits == 0` and sets bit 5 of (iy-63) — the
"demo, no player control" flag. High-score pages 0x11/0x12/0x13 are used
by the mode-4 entry screens (0x0F02-0x0F2D chooses by player count).

While attract runs, VG_KICK_TITLE (0x17D5) restarts the DVG each frame
and, on the title page only (mode 1, page 2), cycles two display-list
words at 0x8080/0x80A8 through 4-entry ROM tables 0x3F4D/0x3F55 on tick
bits 2-3. Those two words are the glyph JSRLs of the 4th and 5th enemy
icons page 2 itself draws down the left edge (LABS x=200,
y=700/600/500/400/300) — the "shimmer" is those icons cycling their
animation frames in place. The same tables are move_mode_dispatch's
droid (0x3F4D) and death-ship (0x3F55) animation frame sets.

Coin lockouts and cocktail flip live in led_shadow bit games: 0x1248
turns start LEDs off (|0x3C), START_BTN_CHECK blinks them on tick bit 6
(1 credit = P1 only, 2+ = both), 0x1255/0x126C drive the flip-screen
bit (bit 6) from the current player for cocktail cabinets (DSW C6 bit 7
= upright, forces no flip; bit 2 of (iy-63) = cocktail game in
progress).

## OPERATOR_MENU (0x1137, test switch during attract)

Draws page 0x15 with live BCD bookkeeping (0x1646 formats counter pairs
from 0x4433/0x4435 into display slots), then loops kicking the VG.
Exits: (iy-80) bit5 -> credits=0 + redraw; bit6 -> SCORES_RESET (erase
high scores); (iy-77) bit7 -> NVRAM_WIPE (erase everything); (iy-79)
bit7 -> back to ATTRACT_INIT.

## New RAM assignments established

| addr | name | meaning |
|------|------|---------|
| 0x4018/19 | tick_prev / tick_delta | per-pass tick bookkeeping |
| 0x4020 | seconds_ctr | ~1 Hz counter (tick wrap) |
| 0x4021 | timer_seconds | attract step / slam timer, 1 Hz |
| 0x4024 | game_mode | 1 attract / 2 spawn+demo / 3 play / 4 hiscore |
| 0x402F-31 | in1_state/pressed/released | port 0x11 edges |
| 0x4032-33 | in2_state/pressed | port 0x12 edges |
| 0x4041,0x403E,0x4040 | (iy-63/-66/-64) | game_flags: bit2 cocktail, bit5 deluxe start (2 cr/player = bonus ship), bit6 2-player; active player 1/2; start_credits = credit price of the pending start (1/2/4, BCD-debited at PLAY_BEGIN 0x0BE3) |
| 0x4060-65 | extra-life thresholds 1-3 | from DSW C4 tables 0x2FE5/0x2FED |
| 0x4070 | fire_cooldown | decremented by tick_delta; fire repeat gate AND the page-scrawl limiter (PAGE_SCRIPT_STEP reloads 10 per glyph, 25 at line end -> 25 glyphs/s) |
| 0x4083 | attract_step | sequence position |
| 0x4084/85 | page_id / page_ptr | current text page (table 0x3F15) |
| 0x4089/8A/8B | page_line_cur / page_line_glyph / page_line_total | page reveal progress: current line, glyph within line, total lines (0x1027 holds the attract step until cur == total) |
| 0x4022 | wave_pace_timer | 1 Hz, starts 0x19; expiry -> 0x406A bit7 (fire cycle) |
| 0x4023 | wave_start_timer | 1 Hz; expiry -> 0x406A bit6 + wave_flags bit7 |
| 0x40BC | ship_angle | from spinner table 0x3061; = ship record 0x40B2 +10, the same cell OBJ_PHYSICS and the letter wheel read |

ROM tables: 0x2FC3 coinage, 0x2FD3 lives, 0x2FE5 extra-life thr 1,
0x2FED extra-life thr 2/3 (sole reader ATTRACT_INIT 0x0B29 -
DIFFICULTY_CALC reads no table), 0x2E90 object-record pointers
(popped!), 0x3061 spinner
decode, 0x3F15 text pages, 0x3F4D/0x3F55 title shimmer words (= the
droid/death-ship anim frame sets), 0x3FD5 digit JSRLs (pricing).
