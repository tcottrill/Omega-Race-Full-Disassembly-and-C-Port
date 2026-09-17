# Omega Race sound: port 0x14 command inventory

Ground truth for the C port's `omega_hw_sound()` / `sound_cmd_send()`.
Everything here is derived from `omega_main.asm` (main CPU) and
`sound_k5.bin` / `omega_sound.asm` (sound CPU). Nothing is guessed.

---

## 1. How the latch works

The main CPU writes one byte to port `0x14`. That raises the sound Z80's
IRQ; `SND_IRQ_LATCH` (0x0038) does:

```
in a,($00)      ; read the latch
and $7F
jr z  -> jp SND_RESET      ; cmd 0 hard-resets the sound CPU
cp $17
jr nc -> ignore            ; only 0x01..0x16 are valid
cp (iy+3)
jr z  -> ignore            ; dedup against the in-flight command
; else: copy 4 bytes from the table at $024A + 4*cmd
;       into the script slot at $11C0 + 4*cmd, and start it
```

So **valid commands are 0x00-0x16**; anything >= 0x17 is dropped on the
floor by the sound board.

Each table entry is `{script_ptr, var_ptr}`. Scripts are a byte-code that
writes AY-3-8910 shadow registers (`$1010`, 32 bytes = AY1 regs 0-15 then
AY2 regs 0-15), which a 244 Hz loop flushes to the two chips.

Script opcodes used below: `F1 nn` delay, `F2 lo hi` / `F3` counted loop,
`F4 nn` **start** script nn, `F5 nn` **kill** script nn, `F6 lo hi`
relative jump (backwards = loop forever), `FF` end. Bytes < `$F0` are
register writes (`and $1F` = shadow reg, top 3 bits = load/or/and/add).

---

## 2. Command table (from `$024A`, verified against `sound_k5.bin`)

| cmd | script | loops | kills | starts | what it is |
|-----|--------|-------|-------|--------|------------|
| 0x00 | 07B4 | - | 01,16,15,02,03,04,05,06,07,08,09,0B,0C,0D,0E,0F,10,11,12 | - | **all sound off** - kills 19 scripts, then zeroes every AY register on both chips |
| 0x01 | 0759 | - | - | 00 | **player ship explosion** - dual-AY noise + envelope, ~1.8 s, **ends with `F4 00` (silences everything)** |
| 0x02 | 0467 | - | 10,15 | - | droid explosion, `kill_tier == 3` |
| 0x03 | 0446 | - | 11 | - | command-ship explosion, `kill_tier == 1` |
| 0x04 | 0549 | - | 14 | 05,06 | **wave-complete tune** - 271 bytes, longest script; launches 05 and 06 as its two harmony voices |
| 0x05 | 0659 | - | - | - | harmony voice of 0x04. **Never sent by the main CPU** |
| 0x06 | 06A7 | - | - | - | harmony voice of 0x04. **Never sent by the main CPU** |
| 0x07 | 072E | - | 12 | - | **ship fire** - descending pitch zap |
| 0x08 | 04F7 | - | - | - | **wall / border hit** - short blip on AY1 channel C |
| 0x09 | 0428 | - | - | - | **thrust ON** - two phases: a 7-step volume decay burst, then a 10-step noise-period settle. Ends with `FF` but leaves the noise generator running, so it sustains until 0x0A |
| 0x0A | 043F | - | 09 | - | **thrust OFF** - `F5 09 / vol C=0 / mixer bit5=1 / FF`. Pure stop, *not* a sound of its own |
| 0x0B | 02E6 | yes | 0C,0D,0E,0F | - | **background heartbeat, tempo 1 (slowest)** - beat gap `F1 7F` |
| 0x0C | 0328 | yes | 0B,0D,0E,0F | - | background heartbeat, tempo 2 - beat gap `F1 5F` |
| 0x0D | 036A | yes | 0B,0C,0E,0F | - | background heartbeat, tempo 3 - beat gap `F1 2F` |
| 0x0E | 03AC | yes | 0B,0C,0D,0F | - | background heartbeat, tempo 4 - beat gap `F1 17` |
| 0x0F | 03EE | yes | 0B,0C,0D,0E | - | **background heartbeat, tempo 5 (fastest)** - no gap at all |
| 0x10 | 048F | - | 02 | - | droid explosion, `kill_tier == 4` |
| 0x11 | 04B5 | - | 03 | - | command-ship explosion, `kill_tier != 1` - noise sweep with rising volume |
| 0x12 | 04CE | - | 07 | - | **coin chime** |
| 0x13 | 052A | - | - | 00 | **tilt / slam alarm** - **starts with `F4 00`, so it silences everything first** |
| 0x14 | 0512 | - | - | 00 | **bonus / extra-life beeping** - 16 beeps; **also starts with `F4 00`** |
| 0x15 | 02D1 | - | 10,02 | 10,10 | droid explosion, default tier - a stuttered re-trigger of 0x10 |
| 0x16 | 02A6 | yes | - | - | **self-test sweep** - 500-step rising tone, loops forever |

### Families that are mutually exclusive on hardware

- `0B / 0C / 0D / 0E / 0F` - the five background heartbeat tempos. Each
  one kills the other four. Only one can ever be audible.
- `02 / 10 / 15` - the three droid-explosion variants.
- `03 / 11` - the two command-ship-explosion variants.
- `07 / 12` - fire and coin share a voice.
- `04` kills `14` (the wave tune stops the bonus beeping).
- `01` **ends** by silencing everything; `13` and `14` **begin** by
  silencing everything.

### Quirk: command 0x00 runs off the end of the ROM

Script 00's register wipe ends `... 38 00 00 3A 00` at $07FE-$07FF —
the final op is a 16-bit load into AY2 C_VOL whose second operand byte
would be at **$0800, past the 2KB ROM**. The interpreter fetches it
anyway; unmapped reads on this board float high, so the script writes
$FF into the AY2 ENV_PER_LO shadow and the next fetched byte ($FF) then
terminates the script. Harmless — every audible register is already
zeroed by then — but the script has no `FF` terminator of its own.
(Surfaced by the bytecode decoder now in `omega_sound.asm`; the whole
script region is decoded there op-by-op, command table included.)

### The sound Z80 runs late, and loses ticks (measured 2026-09-17)

`frametime --sndboard-trace` runs `sound_k5.bin` alone on the Z80
(1.5 MHz, NMI every 6144 cycles) and logs every AY write by tick. An
idle pass - 23-slot scan plus the 32-register compare loop - already
costs ~3900 of the tick's 6144 cycles, so any pass with real script work
is still running when the next NMI lands (1093 such NMIs across the 25
reference scenarios). That only delays the writes. But `snd_main_loop`
runs ONE pass however far `snd_tick` has moved, so a pass spanning two or
three NMIs loses ticks outright:

- `SND_RESET` (power-on, and every command 0): the 1 KB `ldir` RAM clear
  is ~21.5k cycles; the first pass runs at tick 4-5. **This is what
  `SOUND_CMD_SEND`'s 4 x `WAIT_NEXT_TICK` after a command 0 waits out.**
- Script 00's register wipe (run by `F4 00`: the head of 0x13 and 0x14,
  the tail of 0x01): 19 kills, 32 loads and a near-full flush - 2-3 ticks.

Every running script then sits that far behind the wall clock for good
(cmd 0x13 alone: 1 tick late for its whole life). The C port's
`sound_board.c` runs a pass in zero time and loses nothing; see its header
for what that does and does not change.

### Sample-set cross-check

`Release/samples/` holds `1 2 3 4 7 8 9 a b c d e f 10 11 12 13 14 15 16`
`.wav` - hex-named by command byte, 48 kHz 16-bit stereo. **5 and 6 are
absent**, exactly the two scripts the main CPU never sends. And
`b > c > d > e > f` in file length (464K > 339K > 214K > 142K > 135K),
which is the signature of five loops of the same pattern at increasing
tempo. Independent confirmation of the heartbeat reading above.

**Thrust is a two-sample pair, not one.** The sample set is cut on script
0x09's own two-phase seam: `a.wav` is the 0.118 s attack (peaks at 18% FS,
digital silence at both head and tail) and `9.wav` is the sustain (a flat
2.2% FS hiss, identical peak head and tail, meant to loop). Play `a` once
and loop `9` - `9` alone is ~27 dB down on the attack and reads as no
thrust sound at all next to the wall-hit and explosion samples. Confirmed
against the real PCB by the user, 2026-08-19. Note this makes `a.wav` the
attack of command 0x09, *not* the sound of command 0x0A - command 0x0A is
a pure stop and has no sound.

---

## 3. Where the main CPU sends them

### Direct `out ($14),a` (bypasses `SOUND_CMD_SEND`'s gate entirely)

| ROM | cmd | context | C port |
|-----|-----|---------|--------|
| 004E | 0x13 | IRQ slam/tilt, first tick of a new slam | `irq_coins.c:385` |
| 00B6 | 0x12 | coin mech 1 accepted | `irq_coins.c:308` (shared `coin_accept_tick`) |
| 0109 | 0x12 | coin mech 2 accepted | same |
| 04EA | 0x00 | `POST_START` | not ported (test mode) |
| 0783 | 0x16 | `post_sound_test` | not ported (test mode) |
| 079B | 0x00 | end of sound test | not ported (test mode) |
| 0922 | 0x07 | POST-fail blink loop | not ported (test mode) |
| 0939 | 0x00 | end of blink loop | not ported (test mode) |

### Via `SOUND_CMD_SEND` (0x2E04)

| ROM | cmd | context | C port |
|-----|-----|---------|--------|
| 0B78 | 0x00 | `ATTRACT_INIT` | `mainline.c:404` |
| 0D01 | 0x14 | `WAVE_CLEAR_ANIM`, `timer_seconds == 2` | `frame.c:451` |
| 0D4B | 0x14 | player turn-switch page | `frame.c:529` |
| 0EE8 | 0x00 | `SHIP_RESPAWN` | `frame.c:462` |
| 0F74 | 0x00 | `MAINLOOP` slam -> attract | `mainline.c:511` |
| 0F85 | -    | `HEARTBEAT_SOUND(last_sound_cmd)` re-arm | `mainline.c:519-522` |
| 0F90 | 0x09 | thrust on (`(iy-81)` bit5 clear) | `mainline.c:525` |
| 137A | 0x00 | wave complete | `frame.c:700` |
| 1383 | 0x04 | wave-complete tune | `frame.c:702` |
| 13F6 | 0x00 | `fe_join_check` | `frame.c:747` |
| 142C | 0x00 | `fe_respawn_tick` | `frame.c:764` |
| 14A2 | 0x09 | thrust on (edge on `(iy-80)` bit5) | `frame.c:808` |
| 14AF | 0x0A | thrust off (edge on `(iy-79)` bit5) | `frame.c:813` |
| 1C8F | 0x0C | `HEARTBEAT_SOUND(2)`, 10-14 enemies left | `enemies.c:660` |
| 1C9A | 0x0D | `HEARTBEAT_SOUND(3)`, 5-9 enemies left | `enemies.c:658` |
| 1CA1 | 0x0E | `HEARTBEAT_SOUND(4)`, 0-4 enemies left | `enemies.c:658` |
| 1D1E | 0x0F | `HEARTBEAT_SOUND(5)`, engagement start | `enemies.c:706` |
| 20C7 | 0x08 | bounce / wall hit | `objects.c:655` |
| 228E | 0x0C | `HEARTBEAT_SOUND(2)`, `cs_kill_object` | `objects.c:811` |
| 23EF | 0x00 | `SCORE_ADD` extra-life award | `score.c:228` |
| 23F8 | 0x14 | `SCORE_ADD` extra-life award | `score.c:230` |
| 26D8 | 0x07 | `SPAWN_MINE_SHOT` fire | `enemies.c:457` |
| 2807 | 0x00 | `OBJ_DESTROY`, ship branch | `objects.c:389` |
| 2810 | 0x01 | `OBJ_DESTROY`, ship branch | `objects.c:393` |
| 283A | 0x03/0x11 | `kill_snd_cmdship` | `objects.c:374` |
| 283A | 0x02/0x10/0x15 | `kill_snd_droid` | `objects.c:383` |

`HEARTBEAT_SOUND` (0x2DF6) is `cmd = code + 0x0A`, deduped against
`last_sound_cmd`, so codes 1-5 map to commands 0x0B-0x0F.

### `OBJ_DESTROY`'s class dispatch, and why a miss is silent

`OBJ_DESTROY` picks its sound from `a`, which is `obj_index` except in the
shot/mine band (0x0E-0x15) where 0x27E7 re-reads `obj_class` (0x407E):

```
a >= 0x16          -> kill_snd_cmdship   0x03 (kill_tier 1) else 0x11
0x0E <= a < 0x16   -> ret, NO SOUND
0x02 <= a < 0x0E   -> kill_snd_droid     0x02 / 0x10 / 0x15 by kill_tier
a  < 0x02          -> ship death         0x00 then 0x01
```

There are only two callers. `COLLIDE_SCAN` at 0x22F3 is a real hit, and
leaves `obj_class` holding what the scan resolved. `OBJ_PHYSICS` at 0x1BD0
is a shot dying against a wall - it hit nothing - and the two instructions
before it matter:

```
1BCA:  ld a,(obj_index)
1BCD:  ld (obj_class),a      ; point obj_class at the shot's OWN index
1BD0:  call OBJ_DESTROY
```

That store lands `a` in the silent 0x0E-0x15 arm, which is why a shot from
either the player or an enemy that hits nothing makes no sound on the real
PCB. Skip the store and `obj_class` still holds `COLLIDE_SCAN`'s per-miss
running count (`inc (iy-2)` at 0x2208); once that passes 0x16 every shot
that reaches a wall fires the command-ship explosion 0x11 instead. In a
3000-frame test game that was 65 spurious explosions.

### `SOUND_CMD_SEND`'s gate

```
cmd == 0                  -> always send, then idle 4 x WAIT_NEXT_TICK
svc_flags(0x402E) bit7 clear -> drop
timer_seconds(0x4021) == 0   -> send only if game_mode == 3
timer_seconds != 0           -> send only if sound_ready(0x4069) != 0
                                and cmd != last_sound_cmd(0x4068)
```

Note the idiom the ROM uses at 0CFB, 137D, 23F2 and **2801**: it stores 0
into `timer_seconds` immediately before a send, forces the command out via
the `game_mode == 3` path, then restores the timer. Any site that skips
that store can have its sound silently swallowed.
