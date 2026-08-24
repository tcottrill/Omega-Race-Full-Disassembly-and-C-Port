# Omega Race — CPU 0 function reference

Every call-target routine in the main program ROM, by subsystem.
Regenerate the annotated listing with `python z80trace.py main`; details
per area in BOOT_AND_IRQ_NOTES.md, MAINLINE_NOTES.md, GAMEPLAY_NOTES.md.
IY = 0x4080 everywhere. Object records are 23 bytes; the engine stages
the current record into workspace 0x4000-0x4016 (flags0/1, velx +2,
vely +4, posx +6, posy +8, angle +10, b12 lifetime +12, b14 fire timer
+14, aim +15, radii +16/17, timer +19, shape word +21/22), runs the
shared logic, and copies it back. Because of that staging, workspace
cells like 0x400F (aim) are PER-OBJECT state, not globals. Axes:
posx (+6) is screen X, posy (+8) is screen Y — verified byte-exact
against the hardware's own display-list entries (0x1EA2 writes
LABS X from posx, LABS Y from posy).

## Boot / self test
| addr | name | role |
|------|------|------|
| 0000 | RESET | di/im1, clear vector RAM to HALTs, jp POST |
| 04E8 | POST_START / 04EC POST_CYCLE | POST screen, watchdog |
| 050A | POST_TESTSW_GATE | test switch held? full POST : GAME_INIT |
| 07A0/07A2 | VG_WAIT_IY / VG_WAIT_TIMEOUT | wait VG idle (0x3000 tries) |
| 07B6 | VG_WAIT_WIPE | wait idle, fill list with HALT words |
| 07CF | VG_KICK_IY | kick VG if idle, resume via IY |
| 07DD | EDGE_BIT6_IX | port-0x11 edge detect, Z=fire, via IX |
| 07E9 | POST_CSUM_BLOCK | checksum HL..+BC must sum 0xFF |
| 081D | POST_RAMTEST | 55/AA/00 pattern test |
| 0863 | POST_NVRAMTEST | 4-bit NVRAM 0xA/0x5 test |
| 08A0 | POST_SHOW_BITS | draw B bits of C as pass/fail marks |
| 08C8 | DRAW_TEXT | ASCII -> JSRL words pushed with SP |
| 0917 | POST_FAIL_BLINK | blink LEDs + sound, B times |

## Interrupt / coins / bookkeeping
| addr | name | role |
|------|------|------|
| 0038 | IRQ_244HZ | tick++, slam (credit penalty), coin debounce/credits/BCD, meter pulses |
| 1518 | BOOKKEEP_COIN | ledger[A] += 1 (BCD, via 0x3FF7 constant) |
| 1521 | BOOKKEEP_ADD | ledger[A] += *HL, 4-byte BCD |
| 1540 | AUDIT_SNAPSHOT | on last life: fold score/time/events into 6 ledger slots (bank by 0x4041 bit5) |
| 158C | BOOKKEEP_DECAY_AVG | leaky running-average ledger update |
| 1614 | BOOKKEEP_HIGHWATER | ledger[A] = max(ledger[A], time_played) |
| 1639 | BOOKKEEP_PTR | A -> 0x4427 + A*4 |
| 16D5 | BCD_ADD4 | (DE) += (HL), 4-byte BCD |
| 16E3 | CREDITS_NVRAM_SYNC | credits -> 0x5C56/57 nibble shadow |

## Mainline / modes / attract
| addr | name | role |
|------|------|------|
| 0955 | GAME_INIT | RAM clear, DIP decode, NVRAM validate/restore |
| 0A18/0A2E | NVRAM_RD_BYTE / NVRAM_WR_BYTES | nibble packing + validation |
| 0A3B/0A4C/0A68 | NVRAM_WIPE / SCORES_RESET / NVRAM_SAVE_ALL | |
| 0A9D | ATTRACT_INIT | mode 1; free-play check; DIP bonus/lives |
| 0B83 | GAME_START | mode 2 (also attract demo when step 7 / no credits) |
| 0BD1 | PLAY_BEGIN | mode 3, deduct+persist credits, lives, roster init |
| 0F30/0F39 | MAINLOOP_SYNC / MAINLOOP | tick delta, once-per-second block, mode dispatch |
| 1030 | ATTRACT_STEP_DISP | 12-step attract sequence (pages/timers/demo) |
| 10DC | ATTRACT_PRICING | pricing page from coinage tables |
| 1127 | (digit JSRL lookup for pricing) | |
| 1137 | OPERATOR_MENU | bookkeeping display, erase scores/all |
| 11AA | START_BTN_CHECK | credits gate, start LEDs blink, nz = start |
| 1248 | START_LEDS_OFF | |
| 1255/126C | cocktail flip-screen / lockout bit control | |
| 127B | HISCORE_ENTRY_FRAME | mode-4 per-frame wrapper |
| 16F1 | ATTRACT_PAGE_LOAD | big attract display list from 0x33D9 |
| 2AF3 | DRAW_PAGE | build text page (page_id) via table 0x3F15 |
| 2B6D | PAGE_SCRIPT_STEP | page-script interpreter: literal char / N-char string / N-byte BCD value records (format in the hiscore walker report; NOT an enemy spawner) |
| 2C36 | CHAR_GLYPH_EMIT | one ASCII char -> JSRL glyph |
| 240E | BCD_TO_DIGITS | BCD digits -> glyphs, leading-zero blank |
| 2C14 | HISCORE_ROW_FMT | DEAD CODE (unreferenced) |

## Frame engine / objects / physics
| addr | name | role |
|------|------|------|
| 12D2 | FRAME_ENGINE | per-frame sequencer: ship input, death, game over, 2P swap |
| 14F4 | INPUTS_SAMPLE | ports 11/12 with edge detection |
| 1711 | P1_INPUT_READ | cocktail seat-aware port 0x11 read |
| 1737 | SPINNER_READ | cocktail seat-aware spinner read |
| 1750 | VG_RESTART_FRAME | copy shadow list, wall-flash update, kick DVG |
| 1854/185D | VG_WAIT_DONE / WAIT_NEXT_TICK | |
| 1868 | WALL_FLASH_DECAY | fade 19 bounced wall segments (0x409B array -> dlist 0x81BB+, stride 6, nibble writes C0/70/&0F); called from VG_RESTART_FRAME 0x17CB on wall_fx[0] bit7 |
| 18BC | WALL_BUFFERS_BUILD | builds the SHIP's heading frame sets, not walls: copies the 0x408-byte frame set 0x9000 to vector RAM 0x8BF8 (XFORM_B, 180 deg) and 0x87F0 (XFORM_C, Y-flip); the 0x9BF8 ldir targets ROM (no-op, X-flip set SHIP_M* pre-baked). Headings 0x20-0x3F draw from vector RAM |
| 1902 | GAME_DLIST_BUILD | walls + score line + lives row into list |
| 1965/1985 | OBJECTS_UPDATE / OBJ_UPDATE_LOOP | SP pops 0x2E90 table; timers -= tick_delta |
| 19CD | OBJ_STAGE_RUN | stage record -> 0x4000, run shared logic, unstage |
| 1A59 | FRAME_SYNC_DISP | watchdog, VG kick when idle, per-object-index dispatch (collision class lists) |
| 1B16 | OBJ_PHYSICS | thrust via quarter-wave table 0x3021 (gated on wave_flags bit7), friction, clamp; 0x1BB5 skips position integration unless flags0 bit6 — parked/undesignated objects hold still. Differential-tested vs ROM: 3000/3000 exact |
| 1BEB | enemy_fire_gate | (falls in from OBJ_PHYSICS) shooter designation and release (set 6,flags0 = grant wall-collide), fire cycle off wave_pace_timer 0x4022, special-ship arming 0x407F -> countdown 0x407C -> SPAWN_SPECIAL at 0x1DA5. Differential-tested: 2000/2000 exact |
| 1D54 | quadrant_engage_chk | staged object vs ship same-screen-half test (XOR of posx/posy high sign bits): different half -> WALL_TRACK_STEER (patrol), same half -> engage (copy aim into heading) |
| 1DB4 | move_mode_dispatch | (tail of the stage-run) dying -> explosion anim table 0x3F5D by angle&3 (post-increment); shape select by flags0 bit4 / flags1 bit6: record word / heading path 0x1E18 / anim table 0x3F4D; b12 lifetime countdown; b14 fire-timer reload 0x2C-3*wave_num; 0x1E9B fire gate -> SPAWN_MINE_SHOT; beam-settle tail 0x1EE5 |
| 1F2A | WALL_BOUNCE | reflect + halve velocity (sra/rr: rounds toward -inf), corner re-test; wrap ladders keyed on bit7 of the PRE-move high byte. Differential-tested: 3000/3000 exact |
| 20CE | WALL_HIT_FX | classify segment, glow marker 0x96, sound 8 |
| 2148 | APPLY_FRICTION | v -= v/8 |
| 2164 | CLAMP_SPEED | clamp to +/-0x02BC |
| 2199 | COLLIDE_SCAN | B records at IX vs staged object, bbox max-radius test; inline kill + wave counters |
| 291D | OBJ_SPAWN | template copy, class extras, random placement |
| 29B2 | OBJ_KILL | clear active bit |
| 27BF | OBJ_DESTROY | wipe with inert template 0x2FAC; class-banded death sounds (bands: 0-1 ship, 2-13 droid, 14-21 shot/mine, 22+ command/death ship) |

## Enemy AI / weapons
| addr | name | role |
|------|------|------|
| 2467 | ENEMY_ALIVE_SCAN | live-enemy census over objects 2..wave_num+1 (b = wave_num records starting at OBJ 2); nominates shooter candidates (shooter_state 0x4077 = 0/1/2+); sets wave-complete; at wave 12 sets 0x407C/0x407F both = 2 (fast specials) |
| 24EC | SHOOTER_PROMOTE | closest ship-object pair (running min |dx|*|dy| at 0x4080/81, reset to FFFF by the (iy+0/1)=FF writes) becomes designated shooter (0x407A/0x407B) |
| 252F | AIM_AT_SHIP | dy/dx -> DIV_SLOPE -> angle-bucket table 0x3041 -> step the staged object's own aim byte (workspace 0x400F) +/-1 toward ship |
| 25AA | MUL8X8 | HL = D*E |
| 25C1 | DIV_SLOPE | HL ~= E*256/D; **D == 0 returns 0** (the -D add never carries, result is BC = 0) — happens whenever an enemy is exactly level with the ship on one axis |
| 25EB | SPAWN_MINE_SHOT | free slot from pool 0x2EF1 (4) / 0x2F09 (8) by flags1 bit6 (photon vs vapor mine), templates 0x0D-0x14, wave-scaled speed, sound 7; tail 0x2737-0x274E builds the record's shape word 0xC000|(0x0AE7+3*(aim&0x3F)) |
| 2758 | SPAWN_SPECIAL | death/command ship: player exclusion boxes, PRNG re-roll, 11-slot pool (scan 32 down to 22), templates 0x15+; 0x27B0 copies the parent droid's posx/posy highs — a special spawns ON its droid, and only when that droid is outside the central field (0x2758-0x277E gate). Specials get no velocity: they sit until designated shooter |
| 283F | WALL_TRACK_STEER | per-quadrant octagon wall-follow steering (droid patrol) |
| 2906 | RANDOM_NEXT | PRNG: A-entropy + R register + tick tap -> 0x4073 |
| 2186 | DIFFICULTY_CALC | wave-based timing param -> 0x407C |
| 2DF6 | HEARTBEAT_SOUND | heartbeat tempo cmds 0x0C-0x0F by wave_pace_timer (0x4022) thresholds, deduped via last_sound_cmd (the five tempos 0x0B-0x0F are mutually exclusive on the sound board) |

## Scoring / progression / players
| addr | name | role |
|------|------|------|
| 2335 | SCORE_ADD | BCD points from table 0x2FDB, kill tier passed **in A** (1-based; 0x2340 dec a / rlca indexes the table — it does NOT read 0x406F); extra-life thresholds 0x4060/62/64 (latched in 0x406A bits 3-5), bonus-ship glyphs 0x82A6/B0/BA, sound 0x14 |
| 2467 | (see above) wave-complete detection | |
| 0CD4 | WAVE_CLEAR_ANIM | blocking bonus count-up (BCD_ADD4 into time/bonus), pages 0x16-0x1C |
| 0E1E | WAVE_ADVANCE | wave_num += 2, hard-mode bit at >=10, clamp 12 |
| 0E3D | WAVE_ROSTER | kill 1..21 (1..32 on wave 6) then spawn 1..wave+1; 0x0E56-0x0E89: on wave 1 only (wave_num==6) res 6,flags0 parks all six droids — shooter designation (set 6) releases them one at a time. Wave 2+ patrols the whole roster |
| 0EE6 | SHIP_RESPAWN | spawn ship, DLIST_RESET, pages 0x11-0x13; in mode 4 the 0x0EF7 copy of 0x7A bytes from ROM 0x3A0E installs the letter-wheel alphabet display list (A-Z, ERASE/END stacks, cursor) |
| 29CD | HISCORE_CHECK | vs 6x7 table 0x43A9 |
| 29F1 | HISCORE_INSERT | LDDR shift, mode 4, initials slots blanked |
| 2A35 | HISCORE_INITIALS | spinner letter wheel (0-25=A-Z, 27=bksp, 29=end), fire commits, 20s idle timeout, F/S-U-C/K profanity filter |
| 2C8E/2CBD | PLAYER_STATE_SAVE/LOAD | 13-byte per-player areas 0x4046/0x4053 |
| 2CF9 | LIVES_ICON_DRAW | 3 big + 7 small icon slots |
| 1646/1674 | COINAGE_DISP_PREP / BCD_DIV4 | operator pricing math |

## Sound interface
| addr | name | role |
|------|------|------|
| 2E04 | SOUND_CMD_SEND | A=0 always sent + 4 tick delay; else gated on 0x402E bit7, mode/timer rules, dedup vs last_sound_cmd; latch hold delay after out (0x14) |

Sound commands: **the full decoded table is SOUND_NOTES.md** (from the
sound board's own dispatch table at $024A — authoritative). Summary:
0 all off, 1 player-ship explosion, 2/0x10/0x15 droid explosions by
kill tier, 3/0x11 command-ship explosions, 4 wave-complete tune (5/6
are its harmony voices, never sent by the main CPU), 7 ship fire,
8 wall hit, 9 thrust on / 0x0A thrust off, **0x0B-0x0F the five
background heartbeat tempos, slowest to fastest, each killing the
other four**, 0x12 coin, 0x13 tilt/slam, 0x14 bonus beeping, 0x16
self-test sweep.

## Known quirks / dead code
- 0x1354-0x1370: ship-death path "deactivates" shot records by writing
  to ROM 0x000E-0x0011 - no-op, apparent ROM bug.
- 0x1AE8: dead spawn stanza (skipped by jr at 0x1AE6).
- 0x2C14: HISCORE_ROW_FMT, no callers.
- 0x18BC's first copy targets vector ROM (writes ignored); the
  transformed variant is pre-baked in the ROM image at 0x9BF8.

## ROM data tables
0x01A0 POST dlist + text | 0x0AE7 shot display templates by angle |
0x2E8A object table (6B: record/template/dlist slot, 32 objs) |
0x2EF1/0x2F09/0x2F4B spawn pools | 0x2FAC inert object template |
0x2FC3 coinage | 0x2FD3 lives | 0x2FDB points | 0x2FE5 xlife thr 1 |
0x2FED xlife thr 2/3 | 0x2FF7 default hiscores | 0x3021 quarter-wave
sin/cos | 0x3041 aim angle buckets | 0x3061 spinner gray decode |
0x30A1 SHIP_BASE frame-set bases + 0x30A9 per-heading offsets |
0x30C9 NVRAM template | 0x30D9 status line | 0x3109 playfield walls
(0x318D: 8x FFFF placeholders, runtime-patched at dlist 0x8234) |
0x33D9+ attract dlist | 0x3A0E letter-wheel alphabet dlist (0x3A2E
inside it = DRAW_TEXT's A-Z JSRL table) | 0x3EE5 3x8 JSRL words for
the 0x8234 patch (score/credit/lives sub-list calls; rows 2/3 =
player-swap orders) | 0x3F15 page table | 0x3F4D droid + 0x3F55
death-ship anim shape words (doubles as the title shimmer) | 0x3F5D
explosion anim words | 0x3FD3/0x3FD5 digit glyph ptrs | 0x3FF7 BCD
"+1" constant | 0x9000-0x9FFF vector ROM (183 shape subroutines, see
omega_vecrom.asm)
