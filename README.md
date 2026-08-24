# Omega Race — decompilation and C port

An **AI-assisted decompilation of Omega Race** (Midway, 1981) and a
complete, playable **1:1 C port** of it — no emulation, no Z80 core at
runtime; the original program itself, translated. The reverse
engineering and the port were done together with an AI assistant
(Anthropic's Claude) over the course of a bit more than a week, with a
hard rule enforced throughout: **no invented data**. Every constant,
timer, and branch traces to a ROM address, and every behavioral question
was settled by differential testing against the real ROMs running on an
emulator rig — not by guessing. As far as measurement can tell, the port
runs at the original machine's speed, frame for frame and tick for tick.

## What's in the repository

| where | what |
|-------|------|
| [`disasm/`](disasm/README.md) | the decompilation: traced, fully annotated disassemblies of the main CPU, sound board, and vector ROM; the tools that generated them; subsystem notes; and `frametime.cpp`, the headless harness (with a vendored Z80 core) that runs the **real ROMs** with an authentic PROM-driven DVG timing model — the ground truth the port is verified against |
| [`c_src/`](c_src/README.md) | the C port: the whole main-CPU program as C11, the real DVG display-list pipeline, a platform abstraction (Windows backend shipping, headless backend for tests), and a regression suite of differential probes. Builds `omega_win.exe` with VS2022, no external SDK |
| `c_src/samples/omegrace.zip` | a MAME-style sample set recorded from a real board (the port plays samples instead of emulating the sound CPU) |

Each of the two main components has its own README with full detail.

## No ROMs included — bring your own

Midway's ROM images are not distributed here. The C port **builds and
runs without them** (the ROM-derived data it needs is checked in as
generated C files), but the disassembly tools, the test rig, and
regenerating those C files all read real ROM images.

Given a standard MAME `omegrace` ROM set, one script rebuilds everything
ROM-derived — the working dumps, all generated C data files, and
(optionally) the annotated listings:

```bash
cd disasm
python gen_from_roms.py <path-to-your-rom-directory> --listings
```

The output is verified reproducible: regenerating from a raw MAME set
produces byte-identical files to the ones checked in.

## The interesting part: this game has no frame rate

Omega Race's display is a vector monitor driven by a Digital Vector
Generator. There is no vblank and **no frame limiter anywhere in the
program** — this was verified down to the last `in` instruction. The
main loop free-runs: it polls the DVG's busy flag, kicks a new frame the
moment the beam finishes the old one, and measures elapsed 244 Hz
interrupt ticks to scale motion. The frame period is therefore
*emergent*: the beam time of the current display list plus the CPU work
between kicks. Measured on the real ROMs, it floats between roughly
43 and 52 fps as the scene changes.

**MAME does not model this.** MAME's `omegrace` driver runs the display
at a fixed refresh rate, redrawing the vector list on a fixed beat
regardless of how long the real beam would have taken. The game still
plays fine that way, but the pacing texture is subtly wrong: on hardware
the number of 244 Hz ticks consumed per frame varies from frame to frame
(4–6), and the game's own motion code is written around that variation.
Pinning the frame rate changes it — during this project, an early build
that ran one pass per 60 Hz vsync made the ship visibly skip every sixth
physics integration, because a fixed ~4.17 ticks/frame interacts badly
with the ship's 5-tick update period.

The port reproduces the hardware behavior instead: each frame is
scheduled by an exact cost model of the DVG — a transcription of the
DVG's state PROM walks the actual display list just drawn and prices it
in master-clock cycles — plus the measured serialized-CPU gap. The
result floats at the machine's own rate (~46 fps in play) and was
verified against the rig: pacing traces cell-for-cell identical, vector
RAM byte-identical, and the interrupt rate is the schematic-traced
**244.140625 Hz** (12 MHz / 49152), not the commonly quoted 250 Hz.

## Quick start

```bat
cd c_src
build_win.bat
omega_win.exe
```

Keys: Left/Right (or mouse) spin, Space/Ctrl fire, Up/Alt thrust,
5 coin, 1 start, F2 diagnostics, ALT+ENTER fullscreen. See
[`c_src/README.md`](c_src/README.md) for the rest, and
`build_all.bat` for the full test suite (five regression probes, a
90-second headless simulation, all `/W4` clean).

## Provenance and method

The workflow that produced this, in short: trace the ROM with a
control-flow-following disassembler, name every routine and RAM cell
from observed behavior, translate one routine at a time under the rules
in [`c_src/CONVENTIONS.md`](c_src/CONVENTIONS.md), and settle every
uncertainty by running the same scenario on the real ROMs and on the
port and comparing memory — RAM cells and vector RAM byte-for-byte.
The regression probes in `c_src/tests/` pin down everything that
verification caught.
