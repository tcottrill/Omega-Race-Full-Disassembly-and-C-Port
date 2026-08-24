# Omega Race — FRAME_ENGINE, PLAY_BEGIN, and the object system

Continues MAINLINE_NOTES.md. All `(iy±d)` assume IY = 0x4080.

## FRAME_ENGINE (0x12D2) — the per-frame game sequencer

Runs every main-loop pass in modes 3 and 4 (and from attract paths),
ends with `jp MAINLOOP`. In order:

1. **Ship steering** (0x12D2): SPINNER_READ, gray-code decode through
   the 64-entry table at ROM 0x3061 -> ship_angle (0x40BC). Skipped in
   mode 3 while the ship is exploding ((iy+51) bit 5).
2. **Thrust/fire input** (0x12F9): port-0x11 samples from
   INPUTS_SAMPLE. Thrust held sets bit 0 of the ship flags (iy+50).
   Fire *edge* (in1_pressed bit 6) with fire_cooldown (0x4070) expired
   and ship alive -> set bit 2 of (iy+50) ("fire requested"),
   fire_cooldown = 2 ticks.
3. **Ship-death sequence** (0x1323/0x134A): on (iy+50) bit 7 (ship
   destroyed), one-shot guarded by (iy-21) bit 5: sound 0 then 4
   (explosion), death timers timer_seconds 0x4021 (iy-95) and
   wave_start_timer 0x4023 (iy-93) = 5, tick counters
   0x4017/0x4018 cleared. NOTE: the code at 0x1354-0x1370 tries to
   deactivate four objects with `ld ix,$000E / res 7,(ix+0)` etc. —
   those are ROM addresses 0x000E-0x0011, so the writes are no-ops on
   hardware. Looks like a genuine ROM bug (intended: player-shot
   records); kept because it's harmless.
4. **Game-over / respawn / player swap** (0x13B2-0x1491): when lives
   (0x4066) hit the last one: "GAME OVER" page 0x0E, RTSL-blank the
   ship's list rows at 0x8334+, high-score-table insert loop (32x
   OBJ_KILL of everything first), START_BTN_CHECK still live (join-in).
   Respawn path (0x142A): dec (iy-26) death pause, then for 2-player
   games compare P1/P2 4-byte BCD scores (0x404B vs 0x4058) to pick
   who plays next, PLAYER_STATE_SAVE/LOAD swap the 13-byte per-player
   areas (0x4046 P1 / 0x4053 P2), HISCORE_CHECK on final death ->
   mode 4 (entry) or ATTRACT_INIT.
5. **Tail** (0x1494): thrust sound edges (9 on press, 0x0A on release,
   from in1_pressed/in1_released bit 5). Then a positional trigger:
   when ship_x (0x40B9, posx high — axes were swapped in an earlier
   draft) crosses into 0x30-0xD0 and ship_y (0x40BB) >= 0x80, toggles
   bit 1 (track direction) in wave_num/2+1 consecutive object records
   (via the 0x2E8A table) — droid patrol direction flips keyed to the
   ship entering the upper half of the tank.

## PLAY_BEGIN (0x0BD1) — mode 3 setup

    game_mode=3, set bit7 (iy-63) "game in progress"
    deduct credits_bcd -= players (skipped on free play, DSW C6 bit6)
    snapshot + clear the two 4-byte BCD scores (0x4036/0x403A block)
    pick high-score working table (0x43D3 normal / 0x43FD alternate)
    lives = table 0x2FD3 [DSW C4 bits 4-5] (+1 entry offset in demo)
      -> 0x4066; wave state cleared; wave_pace_timer (0x4022) = 0x19
      (25 s at 1 Hz — the enemy fire cycle and special-ship schedule
      hang off its expiry; every ship death reloads it)
    PLAYER_STATE_SAVE both players, DLIST_RESET, GAME_DLIST_BUILD
    cocktail: extra flipped text pages 7/8/0x0A
    NVRAM_SAVE_ALL (persists the credit deduction immediately)
    32x { OBJ_SPAWN(n); OBJ_KILL(n) }   ; init all records, inactive

## The object system

Master table at ROM **0x2E8A**, 6 bytes per object, 32 objects
(so 0x2E8A-0x2F49 — inside the "data" gap, as expected):

    +0/1  RAM record pointer (23-byte record)
    +2/3  ROM template pointer (copied over the record by OBJ_SPAWN)
    +4/5  display-list slot pointer (vector RAM 0x8000+)

- OBJ_SPAWN (0x291D): B=obj#; copies the 23-byte ROM template into the
  record, then (record+19) = obj# — and for obj# < 0x0E runs extra
  per-class init (0x294F+, one-shot setup like 0x2906 for object 1).
- OBJ_KILL (0x29B2): clears bit 7 of record byte 0.
- OBJECTS_UPDATE (0x1965): saves SP, points **SP at 0x2E90** (= table
  entry 1) and `pop`s entries at full speed; 32 iterations; per object:
  (record+19) -= tick_delta, active bit 7 of (record+0) decides the
  update path; (iy-3) counts the object index (published as obj_index
  0x407D at 0x1985 — enemy_fire_gate's shooter matches key off it).
  The pacing-critical detail: all motion is scaled by tick_delta,
  which is why the game tolerates any frame rate (see
  FRAME_PACING_NOTES.md).
- The engine stages each record into workspace 0x4000-0x4016 around
  the shared logic (OBJ_STAGE_RUN), so workspace cells — including the
  aim byte 0x400F — are per-object state that persists in the record,
  NOT globals.
- Records position objects by *display-list entry*: each object owns a
  fixed 12-byte entry (6 words) in the 0x8000 object region. 0x199B
  uses the constant 0x8475 to build a dead entry's "skip to next"
  JMPL (entry word+6); 0x1B03 uses 0x847F for a live entry's "jump
  into own body" JMPL (word+1). Entry layout: [JMPL][LABS Y = A000|
  (posy>>6)][LABS X = scale_nibble<<8 | (posx>>6)][settle vector,
  opcode nibble = beam-settle delay from obj_proximity_upd][0000]
  [JSRL shape]. Verified byte-exact against a live machine dump.

## GAME_DLIST_BUILD (0x1902) — the in-game screen

    call 0x18BC   WALL_BUFFERS_BUILD — despite the name it builds the
                  SHIP's heading frame sets in vector RAM (0x8BF8 =
                  180-degree set, 0x87F0 = Y-flip set); headings
                  0x20-0x3F draw from these, not from ROM
    copy 0x30 bytes ROM 0x30D9 -> 0x4601   status-line template
    BCD_TO_DIGITS: score digits (4 bytes from 0x4039) -> 0x4617 slots,
      credits digit -> 0x462B
    copy shadow list 0x4481 (0x1B0 bytes) -> 0x8000
    append 0x10E bytes ROM 0x3109 -> live list   ** the tank walls **
    copy 0x10 bytes ROM 0x3EE5 -> 0x8234   8 JSRL words over the
      walls blob's FFFF placeholders (ROM 0x318D-0x319C). These are
      the HUD hub: each JSRL CALLs one HUD piece as a display-list
      subroutine —
        C15F -> 0x82BE  page 5 "SCORE" caption
        C0CB -> 0x8196  score digits
        C171 -> 0x82E2  page 6 "LAST SCORE" caption
        C123 -> 0x8246  last-score digits
        C12C -> 0x8258  lives/bonus icons
        C18D -> 0x831A  page 9 "CREDIT" caption
        C0D5 -> 0x81AA  credit digit
        C19A -> 0x8334  pages 0B-0E (bonus/instruction banners)
      Every piece ends in RTSL — the text pages via their trailing
      '<' glyph, whose glyph-table word is D000 = RTSL — so each
      returns to the hub, and the main walk ends at a HALT at
      0x8244. The hub word at 0x8242 (the C19A slot) doubles as the
      banner on/off switch: TURN_SWITCH 0x0D38 restores the JSRL
      from (0x3EF3), MAINLOOP 0x0FC6 blanks it with 0xF000. (An
      un-patched FFFF placeholder decodes as a full-intensity SVEC —
      the source of any stray 45-degree fan off the inner box.)
    BCD_TO_DIGITS: 4 bytes 0x4045 -> 0x8246      (P2/score line)
    DRAW_PAGE 5                                  (score labels text)

So the static playfield geometry lives at ROM **0x3109** (0x10E bytes
of DVG words, copied to dlist 0x81B0) and the 0x8234 patch words at
**0x3EE5** (3 rows of 8; rows 2/3 are alternate orders loaded at
0x0D71/0x0DA3 on player swap). The ten lives/bonus icons are fixed
LABS scale=0 + JSRL slots inside the walls blob (dlist 0x8258-0x82BA),
so they draw at the same scale as the in-play ship.

## PLAY_FRAME (0x0C98) — in-play frame body (first stretch)

    VG_WAIT_DONE
    wave-complete flag (iy-21) bit6 set?
      -> (iy-20)++; every 4th count of 0x406C:
         announcement page 0x16 + min(0x406C/4, 6)   (wave name pages)
         page 0x16, blank slots 0x8092/0x8094, 5 s timer ...

(The rest of PLAY_FRAME — wave sequencing, extra lives, enemy/collision
work — continues at 0x0CD4-0x0EE6; being mapped by sub-walkers, results
to be folded in here.)

## Per-player save area (13 bytes, 0x4046 P1 / 0x4053 P2)

    +0 game-over timer (0x4022)   +1 lives (0x4066)
    +2 wave_num (0x4067)          +3 0x406A    +4 wave_ctr (0x406C)
    +5..12 both scores (0x4036-0x403D), saved only in-game

## High-score machinery

Working table: 6 entries x 7 bytes (4 BCD score + 3 initials) at
0x43A9; battery copies at 0x43D3/0x43FD (two DIP-dependent tables);
NVRAM nibble image at 0x5C3A. HISCORE_CHECK (0x29CD) compares
score_cur against all 6, on qualify shifts entries down (DE=-7) and
enters mode 4; else straight back to attract (mode 1 at 0x29EB).
