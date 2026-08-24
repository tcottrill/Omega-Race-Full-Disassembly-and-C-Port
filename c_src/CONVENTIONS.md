# Omega Race C conversion — conventions

Goal: a faithful C translation of the main-CPU game logic, driving the
OpenGL renderer via `omega_shapes.h` shape data instead of DVG words.

Sources of truth (read before translating a module):
- `disasm/omega_main.asm` — annotated disassembly (all labels named)
- `disasm/FUNCTIONS.md` — per-function reference
- `disasm/BOOT_AND_IRQ_NOTES.md`, `MAINLINE_NOTES.md`, `GAMEPLAY_NOTES.md`

Rules:
1. One C function per named routine, same name lowercased
   (`FRAME_ENGINE` -> `frame_engine()`). Keep the per-function comment:
   original address + one-line role.
2. All state lives in `omega_state g` (omega_state.h). Use the struct
   field named after the disassembly RAM symbol. If you need a RAM cell
   that has no field yet, add it to the struct with its 0x4xxx address
   in a comment - do not invent parallel state.
3. BCD stays BCD (scores, credits, bookkeeping) via the bcd_* helpers,
   so displayed digits match the original exactly.
4. Timing: everything is 244 Hz ticks. Do not convert to seconds or
   floats. `g.tick_delta` is the per-frame motion scale, exactly as the
   ROM used it. Frame pacing itself is the host's job.
5. Hardware access only through the `omega_hw_*` / `omega_draw_*`
   interface. Never model DVG words, display-list addresses, or ports.
   Where the ROM wrote a JSRL for shape N into a slot, call
   `omega_draw_shape(...)` at the equivalent point; shape indices are
   the `SHAPE_IDX_*` macros in omega_shapes.h (SHIP_nn, THRUST_nn,
   SHOT_nn, CHAR_x, DROID_F0, PHOTON_MINE, ...).
6. Preserve quirks that affect gameplay (wave_num += 2, credit cap
   0x20, slam credit penalty, shooter designation, 5/6-tick motion
   wobble). Drop pure hardware artifacts (watchdog kicks, the ROM-write
   no-op bug at 0x1354, the vestigial ROM-destination LDIR in
   WALL_BUFFERS_BUILD) but note each drop in a comment.
7. Fixed-point: object positions/velocities are int16 with the high
   byte as screen coordinate, exactly like the ROM. Keep that layout.
   The axes are straight: "posx" (+6) is screen X and "posy" (+8) is
   screen Y — dvg.c's entry builder writes LABS X from +6 and LABS Y
   from +8, verified byte-exact against a hardware dump. (An earlier
   "posx carries screen-Y" claim was wrong; corrected 2026-08-20.)
8. Plain C99. No dynamic allocation. No globals besides `g` and const
   tables. Const tables copied from the ROM keep their ROM address in
   a comment.

Module map (one .c per module):
- `irq_coins.c`    — IRQ_244HZ body: slam, coin accept, credits, meters,
                     BOOKKEEP_* (bookkeeping/audit family)
- `mainline.c`     — GAME_INIT, mode machine, MAINLOOP skeleton,
                     ATTRACT_*, START_BTN_CHECK, OPERATOR_MENU
- `frame.c`        — FRAME_ENGINE, INPUTS_SAMPLE, spinner, ship death /
                     respawn / player swap, WAVE_* progression
- `objects.c`      — OBJ_* lifecycle, OBJ_PHYSICS, WALL_BOUNCE/FX,
                     COLLIDE_SCAN, OBJ_DESTROY
- `enemies.c`      — ENEMY_ALIVE_SCAN, SHOOTER_PROMOTE, AIM_AT_SHIP,
                     SPAWN_MINE_SHOT, SPAWN_SPECIAL, WALL_TRACK_STEER,
                     RANDOM_NEXT, DIFFICULTY_CALC, HEARTBEAT_SOUND
- `score.c`        — SCORE_ADD, extra lives, HISCORE_* incl. initials
                     entry + profanity filter, NVRAM save/load logic
- `pages.c`        — DRAW_PAGE / PAGE_SCRIPT_STEP equivalents: render
                     the attract/message pages from C string tables
                     (transcribe the decoded text from the listing)
