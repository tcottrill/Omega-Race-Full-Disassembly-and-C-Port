# Omega Race — boot flow, POST, and the complete interrupt cycle

Companion to `omega_main.asm` (regenerate with `python z80trace.py main`).
All `(iy±d)` operands assume IY = 0x4080, so `(iy-105)` = 0x4017 etc.

## How to read this code: the continuation machine

Outside the interrupt handler, early boot code barely uses CALL/RET.
Instead it runs as a chain of continuations:

    ld ix,<resume addr>     ; link register
    jp  <service routine>   ; routine ends with jp (ix)

Some services need a second link (IY) because they nest one level:
`VG_WAIT_IY` / `VG_KICK_IY` resume via `jp (iy)`, and are themselves used
by IX-linked routines. This is why the boot code has no stack discipline —
it repeatedly repoints SP (e.g. DRAW_TEXT uses SP as a *display-list write
pointer*). Only after GAME_INIT (0x0955) does SP = 0x4BFC become a real
stack and CALL/RET take over.

## Reset → boot flow

    RESET (0x0000)
      di, im 1, sp=0x4BFC, kick watchdog (in 0x09), leds 0xFF
      fill vector RAM 0x8000+ with 0xB000 words  = HALT (blank screen)
      copy 0x180 bytes 0x8000 -> 0x4481          (init the shadow list)
      copy 0x30 bytes ROM 0x30D9 -> 0x4601
      jp POST_START (0x04E8)

    POST_START
      sound cmd 0
    POST_CYCLE (0x04EC)
      watchdog, wait VG idle + wipe list (VG_WAIT_WIPE)
      copy 0x2C bytes ROM 0x01A0 -> 0x8000       (the POST screen header:
        LABS, scale, JSRLs for fixed text)
      DRAW_TEXT: 0xA2 chars from ROM 0x01CB      ("P ROM 1 ." ... etc, the
        test-result line labels)
    POST_TESTSW_GATE (0x050A)
      in 0x11, bit 7 (test switch, active low)
        released -> jp GAME_INIT (0x0955): normal boot skips the long POST
        held     -> run the full self test below, looping forever

## The self test (test switch held)

Each stage runs with the VG kept alive (`VG_KICK_IY` is called every 256
bytes inside the checksum/RAM-test loops so the POST screen keeps
redrawing), then writes a result word into the on-screen list at DE
(slot stride 0x12 bytes). The "characters" 0x05 and 0x0A are JSRL
operands: the POST screen header (ROM 0x01A0) defines three 4-glyph
subroutines in the list itself — `"  OK"` at word address 0x005,
`" NG "` at 0x00A, four blanks at 0x00F — each ending in a shared
newline vector. Every label's trailing `'.'` in the 0x01CB text is
DRAW_TEXT's encoding for JSRL 0x00F (blank + line feed), so posting a
result is a one-word overwrite: 0x00F -> 0x005 (pass) or 0x00A (fail).
The input-display screen reuses the same two offsets: its header
defines 0x005 = `" LOW"` and 0x00A = `" HI "`.
On failure it also runs POST_FAIL_BLINK: LEDs 0xC3/0xFF flash and sound
command 7/0, repeated <test#> times — the classic "count the blinks"
error code.

| # | test | routine | range |
|---|------|---------|-------|
| 1 | P ROM 1 checksum (sum of bytes == 0xFF) | POST_CSUM_BLOCK | 0x0000-0x0FFF |
| 2 | P ROM 2 | " | 0x1000-0x1FFF |
| 3 | P ROM 3 | " | 0x2000-0x2FFF |
| 4 | P ROM 4 | " | 0x3000-0x3FFF |
| 5 | main RAM | POST_RAMTEST (FF?/55/AA/00) | 0x4000-0x43FF |
| 6 | main RAM | " | 0x4400-0x47FF |
| 7 | main RAM | " | 0x4800-0x4BFF |
| 8 | NVRAM (4-bit, 0xA/0x5 patterns) | POST_NVRAMTEST | 0x5C00-0x5C19 |
| 9 | vector RAM | POST_RAMTEST | 0x835A-0x83FF |
| A | vector RAM | " | 0x8400-0x87FF |
| B | vector RAM | " | 0x8800-0x8BFF |
| C | vector RAM | " | 0x8C00-0x8FFF |
| D | vector ROM 1 checksum | POST_CSUM_BLOCK | 0x9000-0x97FF |
| E | vector ROM 2 checksum | " | 0x9800-0x9FFF |

After the memory tests (advance with fire button = port 0x11 bit 6, edge
detected by EDGE_BIT6_IX):

- input display screen (list from ROM 0x026E): POST_SHOW_BITS renders the
  live bits of port 0x11, 0x12, spinner 0x15, spinner 0x16, DSW 0x10 and
  DSW 0x17 as rows of pass/fail marks
- screen from ROM 0x042F (cross-hatch/alignment pattern), fire to advance
- screen from ROM 0x0447 (0xA0-byte list — grid/pattern page)
- sound test: command 0x16, then loop 256 VG frames, repeat until fire
- jp POST_CYCLE — the whole POST loops while the switch is held

DRAW_TEXT (0x08C8) is the text renderer used everywhere: for each ASCII
char it fetches a JSRL word and *pushes* it into the display list with
`ld sp,<slot>; push de` — letters via pointer table 0x3A2E ('A'-'Z'),
digits/punctuation via 0x3FD3, space = JSRL $D92, '.' = JSRL $00F.
Character stroke subroutines live in the vector ROM (`omega_vecrom.asm`).

## GAME_INIT (0x0955)

    sp=0x4BFC, iy=0x4080 (RAM base for the whole game)
    clear 0x4000-0x4481
    (iy-14)=0xFF                      ; led_shadow 0x4072 all off
    call 0x18BC                       ; WALL_BUFFERS_BUILD: ship heading
                                      ; frame sets into vector RAM
    coin_lines     = port 0x11 & 3
    coin_mode1     = (DSW C6 & 0x07) * 2 -> coinage1_ptr = 0x2FC3 + mode
    coin_mode2     = (DSW C6 & 0x38) / 4 -> coinage2_ptr = 0x2FC3 + mode
    copy 0x2A bytes 0x2FF7 -> 0x43A9  ; default high-score table
    copy 0x54 bytes 0x43A9 -> 0x43D3
    validate NVRAM 0x5C1A against nibble template 0x30C9 (2x16 nibbles),
      then checksum-verify records via 0x0A18; bad -> reinitialize
    ... falls into the attract/game mainline

## The complete interrupt cycle (0x0038-0x019E)

**Rate: 244.140625 Hz, not the traditional 250** (schematic sheet
`omegaM2`: 12 MHz /16 through N8 74161, /256 through S5 74393 to its
2QD pin, /12 through R6 74161 which is preset to 4 with its ripple
carry fed back to LOAD; that carry clocks the H9 7474 whose Q is INT,
and the M1 acknowledge clears it). 12 MHz / 49152 = exactly 12288 Z80
cycles at 3 MHz. Derivation and consequences: FRAME_PACING_NOTES.md.
The cell was renamed `tick_244hz` to match (2026-08-21).

The handler is *only* a timebase + coin/cabinet service. It never touches
the vector generator or game objects. Preserves AF/BC (+HL where used).

```
0038  push af, bc
003A  inc tick_244hz            ; 0x4017 — the only timebase in the game
003D  c = coin_lines            ; previous state of the two coin lines

; --- slam / tilt ---------------------------------------------------
0040  in 0x11, bit 4            ; slam switch, active low
      hit and slam_latch empty:
        sound cmd 0x13          ; alarm
        slam_latch = 0x13
        coin1_batch = coin2_batch = 0     ; void partial coins
        credits_bcd -= 1 (DAA, if any)    ; slam penalty: lose a credit
        set bit6 (0x402E)                 ; credit display dirty
      (iy-95) = 3               ; slam debounce

; --- coin line edge detection --------------------------------------
007C  a = port 0x11 & 3 -> coin_lines; b = a
      changed bits vs c:
        bit0 changed -> coin1_debounce = 5    ; 5 ticks = 20 ms
        bit1 changed -> coin2_debounce = 5

; --- coin 1 accept (0097), coin 2 accept (00EA) — identical ---------
      if coinN_debounce: if --coinN_debounce == 0 and line still low:
        if port 0x11 bit7 set:            ; bookkeeping enable line
          BOOKKEEP_COIN(slot)             ; BCD totals array 0x4427,
                                          ;   increments from 0x3FF7
          coinN_pending++                  ; 0x401D / 0x401E
        sound cmd 0x12                     ; coin chime
        coinN_batch++
        if coinN_batch >= coins-per-credit (from coinageN_ptr):
          coinN_batch = 0
          if bit7 clear: BOOKKEEP_ADD(2)   ; service-credit ledger
          credits_bcd += credits-per-batch (next table byte, DAA)
          set bit6 (0x402E)                ; display dirty
          if credits > 0x21 BCD: credits = 0x20   ; cap at 20

; --- coin meter pulse generator ------------------------------------
013D  if meter_pulse_t:                    ; 0x401F, set to 0x32 below
        --meter_pulse_t
        at 0x19: out 0x13, led_shadow|3    ; meters ON  (100 ms later)
        (at 0)  : pulse done
0159  if coin1_pending: --coin1_pending
        res bit0 of led_shadow             ; meter 1 drive low
        meter_pulse_t = 0x32
016A  if coin2_pending:
        mode-dependent bit0/bit1 of led_shadow
        --coin2_pending, meter_pulse_t = 0x32
018F  if meter_pulse_t == 0x32:
        out 0x13, led_shadow               ; meters OFF (pulse start)

019B  pop bc, af, ei, reti
```

So each accepted coin produces: chime, BCD credit math against the DIP
coinage table, a bookkeeping total in NVRAM-backed counters, and one
100 ms pulse on the mechanical coin meter via port 0x13 — all inside the
interrupt. Everything else in the game runs off `tick_244hz` deltas read
by the mainline (see FRAME_PACING_NOTES.md).

## RAM map established so far

| addr | name | meaning |
|------|------|---------|
| 0x4017 | tick_244hz | interrupt tick counter |
| 0x401A | last_kick_tick | tick at last DVG restart |
| 0x401B/1C | coinN_debounce | 5-tick coin debounce |
| 0x401D/1E | coinN_pending | accepted coins awaiting meter pulse |
| 0x401F | meter_pulse_t | coin meter pulse timer (0x32 -> 0) |
| 0x4025/26 | coinN_batch | coins toward next credit |
| 0x4027/29 | coinageN_ptr | -> DIP coinage table 0x2FC3 |
| 0x402B/2C | coin_modeN | DSW C6 coinage mode |
| 0x402D | credits_bcd | credits, BCD, cap 0x20 |
| 0x402E | (iy-82) | bit6 = credit display dirty |
| 0x4035 | coin_lines | last coin input bits |
| 0x4072 | led_shadow | port 0x13 shadow (LEDs + meters) |
| 0x4082 | slam_latch | nonzero while slam alarm active |
| 0x4427 | bookkeep_ctrs | 3-byte BCD ledger entries, stride 4 |
| 0x4481 | dlist_shadow | game-built display list (copied to 0x8000) |
| 0x5C00 | nvram | 4-bit NVRAM, records at 0x5C1A |
