# Omega Race — decompilation

A complete, traced, annotated disassembly of **Omega Race** (Midway, 1981),
covering the main CPU program, the sound board program, and the vector
("DVG") ROM — plus the tooling that generated it and the notes that record
what was learned. This work is the ground truth for the C port in `c_src/`:
every routine, RAM cell, and timing claim over there traces back to an
address in these listings.

## The hardware, briefly

- **Main CPU:** Z80 at 3 MHz (12 MHz crystal / 4).
- **Interrupt:** one periodic IRQ at **244.140625 Hz** (12 MHz / 49152,
  traced gate-by-gate off the schematic's divider chain — *not* the 250 Hz
  usually quoted). It increments a tick counter and services coins/slam;
  it never paces the display.
- **Display:** a Midway implementation of the Atari Digital Vector
  Generator, driven by a PROM state machine. The CPU builds a display list
  in shared vector RAM at 0x8000, kicks the DVG with an `in` on port 0x08,
  and polls busy on port 0x0B bit 7. **There is no frame limiter and no
  vblank: the frame rate is emergent** — the beam time of the current list
  plus the Z80 work between kicks, floating around 43–52 fps.
- **Sound:** a Z80 daughter board with two AY-8910s, commanded over a
  latched port (0x14). Its NMI is the *same* 244 Hz node as the main
  board's IRQ (net GBNMI), so sound tempo shares the main clock.
- **Persistence:** 4-bit battery-backed RAM (credits, high scores,
  operator bookkeeping), plus two coin doors, slam tilt, and two spinners.

## What is in here

### Listings (generated — do not hand-edit)

| file | contents |
|------|----------|
| `omega_main.asm` | main CPU program, 0x0000–0x3FFF, every reachable routine labeled and annotated; data regions typed (object templates, coinage tables, pages, high-score defaults) |
| `omega_sound.asm` | sound board program (`sound_k5.bin`), the command dispatch table at $024A and all sound scripts |
| `omega_vecrom.asm` | the vector ROM as DVG opcodes: every glyph and shape subroutine, named |

### Tools

| file | role |
|------|------|
| `z80trace.py` | tracing Z80 disassembler. Follows control flow from the reset/IRQ vectors so code and data separate cleanly; jump-table targets and typed data regions are declared in its config and refined iteratively against a coverage report. `python z80trace.py main` / `sound` regenerates the listings. |
| `dvgdasm.py`, `vecxref.py`, `vec_names.py` | DVG disassembler and shape cross-reference: names glyphs from the game's own glyph tables, counts JSRL/JMPL references, groups pre-rotated frame sets, and exports every shape as C data (`omega_shapes.c/h`). |
| `gen_pagerom.py`, `gen_postrom.py`, `gen_dvgprom.py` | export the attract/message page bytes, the POST screen display lists, and the DVG state PROM as C arrays for the port. |
| `gen_from_roms.py` | the one-command entry point: assembles the working dumps from a user-supplied MAME `omegrace` ROM set, then runs every generator above (and, with `--listings`, the disassemblers too). |
| `frametime.cpp` | headless harness that runs the **real ROMs** on a Z80 core with a cycle-accurate DVG timing model (PROM state machine, drawing stripped). No throttling — everything runs on the emulated timebase, so the measured kick-to-kick period *is* the hardware frame period. Grew the differential modes the port is verified against: `--pace-trace`, `--special-trace`, `--attract-vram` / `--credit-vram` dumps, and the measured CPU-gap figures the port's pacing model uses. The Z80 core is vendored here (`cpu_z80.cpp/h` + support headers); build with `cl /O2 /EHsc frametime.cpp cpu_z80.cpp`, inputs are the `gen_from_roms.py` dumps beside it. |
| `shapes_preview.html` | visual index of every extracted vector shape. |

### Notes (the findings)

| file | contents |
|------|----------|
| `FUNCTIONS.md` | every call-target routine in the main ROM, by subsystem, with the staged-workspace object model and the `iy = 0x4080` convention |
| `BOOT_AND_IRQ_NOTES.md` | reset, POST self-test, the IRQ body, coin/credit/bookkeeping logic |
| `MAINLINE_NOTES.md` | MAINLOOP, the mode machine, attract sequence, operator menu |
| `GAMEPLAY_NOTES.md` | object engine, physics, collision, enemy AI, shooter designation, spawn scheduling |
| `SOUND_NOTES.md` | the sound command set decoded from the dispatch table and every `out ($14)` site |
| `../FRAME_PACING_NOTES.md` | the frame-rate derivation, including the 244 Hz divider-chain trace |

## Method

The listing is not a linear dump. `z80trace.py` walks control flow from
the entry points, so unreached bytes render as data; computed-goto and
jump-table targets were confirmed by hand and added to the config until
coverage closed. Every routine and RAM symbol was then named from observed
behavior, and claims were tested rather than inferred:

- **Timing came from the schematic and from measurement**, not from
  folklore — the 244.140625 Hz rate was traced through the actual divider
  chain, and `frametime.exe` reproduces the hardware frame period from the
  ROMs themselves.
- **Behavioral questions were settled by differential testing**: the same
  scenario run on the real ROMs (via `frametime.exe`) and on the C port,
  comparing RAM cells and vector RAM byte-for-byte.
- **RAM cells were resolved against `iy = 0x4080`** (fixed by the
  `last_sound_cmd == (iy-24) == 0x4068` anchor); several early
  field-naming bugs came from trusting `(iy±n)` comments before doing
  that arithmetic.

A few results worth knowing before reading the code:

- The game **free-runs**. MAINLOOP measures elapsed ticks and *consumes*
  them (`fire_cooldown -= tick_delta`, tick-scaled flash decay); every
  frame-kick site checks DVG busy and skips rather than waits. The one
  limiter-shaped fragment — a ticks-since-last-kick compare against 10 in
  VG_RESTART_FRAME (0x17BE) — has its conditional body NOP'd out in the
  shipped ROM.
- The attract/message pages are a small interpreted format (chained
  segment records with BCD-field and run-length escapes), decoded in the
  listing at DRAW_PAGE (0x2AF3).
- Object records are 23 bytes, staged into workspace 0x4000–0x4016 for
  the shared engine and copied back — workspace cells are per-object
  state, not globals.

## ROMs — bring your own

The tools read `omega_dump.bin` (a 64 KB image of the main CPU address
space, program + vector ROM), `sound_k5.bin`, and `dvgprom.bin`. These
are dumps of copyrighted Midway ROMs and are **not in the repository** —
supply a standard MAME `omegrace` set and build them:

```bash
python gen_from_roms.py <path-to-rom-directory> --listings
```

This assembles the dumps, regenerates every ROM-derived C file in
`../c_src/`, and with `--listings` re-traces the disassemblies. The
output is reproducible: a raw MAME set regenerates files byte-identical
to the checked-in ones. The listings, notes, and tools are this
project's own work.
