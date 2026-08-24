# Omega Race — interrupt and frame pacing

How the machine keeps time, traced from the ROM (`omega_main.asm`, all
RAM offsets assume `IY = 0x4080`) and the service-manual schematics.
The measured figures come from `disasm/frametime.exe`, which runs the
real ROMs against a cycle-accurate model of the vector generator.

## The interrupt is 244.140625 Hz

Read off the schematic (manual sheet `omegaM2`), tracing the Z80's INT
pin backwards:

    XTAL101 12 MHz
      -> N8 74161  QD          = /16   ->  750 kHz
      -> S5 74393  2QD (pin 8) = /256  ->  2929.6875 Hz
      -> R6 74161  preset 4 (A/B/D grounded, C pulled up through R156),
                   RCO fed back to LOAD, so it counts 4..15
                               = /12   ->  244.140625 Hz
      -> S6 inverter -> H9 7474 (D tied low) -> INT,
         cleared by the M1 interrupt acknowledge.

That is 12 MHz / 49152 exactly = **12288 Z80 cycles** at 3 MHz =
4.096 ms per tick. The 250 Hz figure often quoted for this game (MAME
uses it too) is 2.4% fast, and this divider chain cannot produce it at
all: 750 kHz / 250 Hz = 3000, and no 74393 tap combined with a /16
74161 gives 3000.

The sound board's NMI is the **same signal**, not a separate divider:
sheet `omegaD` shows /NMI arriving on J4-14, and omegaM2's J4 legend
names pin 14 GBNMI — the inverted R6 RCO, the very node that clocks H9
for the main INT. So the sound tempo shares the 244 Hz timebase and
each sound-script tick is 4.096 ms.

## The IRQ handler (0x0038, IM 1)

```
0038  PUSH AF / PUSH BC
003a  INC (IY-0x69)          ; tick counter at 0x4017, +1 per interrupt
0040  IN A,(0x11)            ; coin/tilt/test inputs
...   coin bookkeeping, sound command latch (out 0x14), DAA credit math
```

The handler never touches the vector generator. It only increments the
tick counter at **0x4017** and services coins/tilt/sound. When the
mainline wants a wall-clock delay it spins on the counter (0x185d,
used for the sound-command latch timing):

```
185d  PUSH AF
185e  LD A,(0x4017)
1861  CP (IY-0x69)           ; spin until the interrupt bumps the counter
1864  JR z,0x1861
```

## The vector generator kick — the actual frame heartbeat

Ports: `0x08` read = VG start, `0x0B` read = status (d7=1 while
drawing), `0x09` read = watchdog reset.

The frame-restart routine at **0x1750** is called from the main loop
whenever the DVG is seen idle (e.g. 0x1a59: `IN A,(0B); BIT 7,A;
CALL z,0x1750`):

```
1750  IN A,(0x09)            ; watchdog
1752  IN A,(0x0b)            ; VG status
1754  BIT 7,A
1756  JR nz,0x17d4           ; still drawing -> return, try again next pass
1758  LD BC,0x01b0           ; copy new display list:
175b  LD DE,0x8000           ;   shadow buffer 0x4481 -> vector RAM 0x8000
175e  LD HL,0x4481           ;   (game double-buffers the display list)
1761  LDIR
1763  ...                    ; patch ship/thrust entries (0x818c), flicker via
                             ;   INC (IY-0x0f) once per VG frame
17b7  LD A,(0x4017)          ; tick delta since last kick
17ba  SUB (IY-0x66)          ;   (consumed by the wall-flash decay)
17c5  LD A,(0x4017)
17c8  LD (0x401a),A          ; remember kick tick
17d2  IN A,(0x08)            ; *** START THE DVG ***
17d4  RET
```

Busy-wait variants exist for display-list maintenance: 0x1854 polls
0x0B until done (no timeout), 0x07a2 polls with a 0x3000-iteration
timeout, and the kicks at 0x17d5/0x180d/0x1820 poll-then-kick. All of
them wait for the beam to *finish* before touching vector RAM — none
of them enforces a minimum frame time.

## The frame rate is emergent

- The 244 Hz interrupt is only a timebase (ticks, coins, sound). It
  does not pace the display.
- The game polls DVG done in its main loop and **restarts the DVG
  immediately each time it finishes**, after copying in the freshly
  built list. There is no frame limiter anywhere in the program.
- The visible frame rate is therefore
  `1 / (DVG draw time + serialized Z80 work per frame)`, and it floats
  with scene complexity. The game's own motion code scales by the
  measured tick delta per frame (0x0f3f / 0x1a62), because the
  designers knew the frame time varies.

Measured on the real ROMs (`frametime.exe`, kick-to-kick on the
emulated timebase):

    gameplay (mode 3 + demo): avg 46-49 fps  (~21 ms: ~15-16 ms DVG draw
                              + ~5.3 ms serialized Z80 work per frame)
    attract text pages:       avg 64-69 fps  (~14-16 ms, draw-bound,
                              CPU gap only ~0.3 ms)
    wave transitions:         dips to ~30 fps (blocking animations)

Both cost components matter. The DVG state machine charges 8 master
cycles per *state* on top of the per-vector beam time
(`8 * ((2 << scale) & 0x7ff)` master cycles), and a ~800-word list
burns thousands of states on fetches/JSRLs/RTSLs. On the CPU side,
VG_RESTART_FRAME's 0x1B0-byte LDIR (~3 ms) and the object-slot writes
gated on VG_WAIT_DONE all happen while the DVG sits idle.

### Clock confirmation

The DVG here is Midway's clone of the Atari Digital Vector Generator.
Sheet omegaM2: N8 taps XTAL101 down to 1.5 MHz, gate PR4 emits it as
**VCK, the vector clock**, driving the cascaded 7497 rate multipliers
on omegaM1 — the Atari BRM design. Atari's own vector clock is
12.096 MHz / 8 = 1.512 MHz, the same rate to within 0.8%, so MAME's
one-constant DVG state-machine model (8 master cycles per BRM pulse at
12.096 MHz) prices this board's lists correctly to within that 0.8%
(the board's real crystal is 12.000 MHz).

### The MAME comparison

MAME's omegrace driver presents the vector screen at a fixed
`set_refresh_hz(40)`. That is close to the gameplay *average*, but it
is a fixed sampling rate: the hardware's rate floats (30-69 fps by
scene), and sampling a floating frame stream at a fixed cadence
drops/doubles frames. Omega Race is unusual among DVG games in this
respect — the Atari vector games kick their generator from a fixed
interrupt, so a fixed refresh models them well; Omega Race's rate IS
the draw time.

### What the C port does

`omega_app_step` (c_src/app_loop.c) reproduces the hardware behavior:
each displayed frame is scheduled by `dvg_cost_frame_ms()` — the DVG
state machine walked over the exact list just drawn, plus the measured
serialized-CPU gap (5.30 ms in play, 0.35 ms in attract) — EMA-smoothed
on an absolute schedule. The 244 Hz service runs on machine time while
the host waits, exactly as the interrupt kept firing while MAINLOOP
polled the busy flag. Vsync defaults off so the panel follows the
floating rate; with a VRR display, vsync + adaptive sync works too.
