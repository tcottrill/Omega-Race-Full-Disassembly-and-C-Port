# Omega Race — C source port

A faithful, playable C11 translation of **Omega Race** (Midway, 1981).
This is not an emulator: there is no Z80 core and no interpretation at
runtime. The entire main-CPU program was decompiled (see `../disasm/`)
and rewritten as C, one function per ROM routine, against a single state
struct whose fields are named after the original RAM symbols. The vector
display pipeline is the real one — the game builds actual DVG display-list
words in vector RAM and a transcription of the DVG PROM state machine
walks them — so what reaches the screen is what the hardware's beam drew.

Builds on Windows with VS2022, no external SDK: `build_win.bat` produces
`omega_win.exe` (x64, OpenGL beam renderer, XAudio2 sound, raw input).

## Fidelity rules

The port follows two hard rules, learned the expensive way:

1. **No invented data.** Every constant, table, timer reload, and branch
   condition traces to a ROM address; each C function carries its
   original address. Where behavior was uncertain it was settled by
   *differential testing* against the real ROMs, not by guessing.
2. **Machine time is 244 Hz ticks** (the board's true 244.140625 Hz IRQ
   rate, traced off the schematic). Game logic never sees seconds or
   floats; `tick_delta` scales motion exactly as the ROM used it.

Verified equivalences against hardware (the rig running the real ROMs):

- Attract and credit-screen **vector RAM byte-identical**, all 64 rows.
- Play-mode vector RAM identical except live-object state.
- Pacing traces **cell-for-cell identical** for the wave-pace scheduler,
  shooter designation, fire cycle, and special-ship timing.
- Frame pacing reproduces the hardware's **floating frame rate**: the
  original has no frame limiter (MAINLOOP free-runs against the DVG busy
  flag), so each displayed frame here is scheduled by an exact cost model
  of the DVG's beam time for the list just drawn, plus the measured
  serialized-CPU gap. Play runs ~46 fps against hardware's ~47–49,
  floating with display-list length exactly as the machine does.

Deliberate departures, each documented in code: the sound board CPU is
not emulated (commands map to samples recorded from a real board), F2
enters the diagnostics from anywhere (hardware POST is power-on only),
and an optional phosphor-persistence composite (`phosphor_ms`) smooths
the hard-switched-LCD look of the authentic tick quantization.

## Layout

| module | contents (ROM subsystem) |
|--------|--------------------------|
| `mainline.c` | boot, mode machine, MAINLOOP, attract sequence, operator menu |
| `frame.c` | per-frame engine, input sampling, wave progression, player swap |
| `objects.c` | object lifecycle, physics, wall bounce, collision, destruction |
| `enemies.c` | AI, shooter designation, aiming, spawners, PRNG, difficulty |
| `score.c` | scoring, extra lives, high-score table + initials entry, NVRAM |
| `irq_coins.c` | the 244 Hz service: coins, credits, meters, slam, start buttons |
| `post.c` | power-on self test: all five diagnostic screens, operator/audit page |
| `dvg.c`, `dvg_pages.c`, `pages.c` | vector RAM, the DVG state machine + frame cost model, the object-list builder, and every text page/HUD built from the ROM's own page bytes |
| `omega_vecrom.c`, `omega_shapes.c`, `omega_pagerom.c`, `omega_postrom.c`, `omega_dvgprom.c` | generated ROM data (shapes, pages, POST screens, DVG PROM) — no ROM files needed to build or run; regenerate from your own ROM set with `../disasm/gen_from_roms.py` |
| `sound_samples.c`, `glue.c` | sound-command → sample map; cross-module bridges |
| `app_loop.c` | the platform-agnostic application loop: machine-time tick service, input → port-byte mapping, NVRAM serialization, authentic frame pacing |
| `omega_state.h` | `omega_state g` — all game state, fields named by RAM address |

### Platform seam

`platform/omega_platform.h` is a link-time C contract (no vtables, one
backend per binary):

- `platform/windows/` — the shipping backend: window/GL beam renderer,
  raw input, XAudio2 mixer, DirectInput joystick, ini + logging
  (self-contained vendored framework files, no external SDK).
- `platform/headless/` — the test backend: injectable raw port bytes and
  capture hooks; every harness links this instead of `app_loop.c`.
- `platform/linux/`, `platform/teensy/` — compile-checked stubs. The
  Teensy target (DAC replay to a real vector CRT) is why pacing lives in
  the core: a backend where the beam *is* the frame period calls
  advance+frame directly and skips the PC pacing.

## Building and testing

```bat
build_win.bat    — the game, omega_win.exe
build_all.bat    — everything: five probes, probe_objects, test_drive,
                   wbtest (all into tests\), stub syntax checks, and the
                   game; /W4 clean is the pre-commit bar
```

The harnesses are the fast iteration loop — no window, simulated time:

- `tests\test_drive.exe` — 90 simulated seconds, logs mode/step/page
  transitions; `--vram` / `--playvram` dump vector RAM for hardware
  diffs; `--draw` prints page layouts; `--paceattract` measures pacing.
- `tests\probe_wave / probe_nvram / probe_hiscore / probe_sound /
  probe_post` — regression probes for once-broken behavior, each must
  print `PROBE PASSED`.
- `tests\probe.exe` (probe_objects) — 90 s object/display-list invariant
  sweep with `--play` for a real coined game.

Run them from `c_src\` so relative paths resolve.

## Running

Keys: **Left/Right** spin (or mouse spinner / joystick), **Space** or
**Left Ctrl** fire, **Up** or **Alt** thrust, **5** coin, **1** start,
**ALT+ENTER** fullscreen, **Esc** quit. **F2** opens the diagnostics from
anywhere; **9** is the authentic test-switch line (hold at startup for
POST, hold in attract for the operator/audit page).

`omega_win.ini` (written beside the exe): `[main] vsync` 0/1/2 — vsync
off lets the panel free-run at the authentic floating rate; with a VRR
display, vsync 1 + G-Sync is ideal. `[vector]` holds beam-rendering
tuning (`linewidth` and `line_smoothing`, the beam width in pixels at the
default 1024x768 window, scaling in proportion with the picture in bigger
windows and fullscreen, and the anti-alias feather in physical pixels on
any screen, 2026-09-03;
`phosphor_ms` — 0 disables the phosphor composite). High scores, credits, and operator bookkeeping
persist in `omega_c.nv`, byte-compatible across builds. Sound loads
from `samples\omegrace.zip`, a MAME-style sample set whose members are
hex-named by the sound command byte (`1.wav` … `16.wav`).

## Provenance

Sources of truth are in `../disasm/`: the annotated listings,
`FUNCTIONS.md`, the subsystem notes, and `frametime.cpp` (the harness
that derives hardware timing from the real ROMs). Translation rules are
in `CONVENTIONS.md`; the frame-pacing model is documented in
`../FRAME_PACING_NOTES.md`.
