;=====================================================================
; omegarace_defines.asm - Omega Race (Midway 1981) main CPU memory map
;=====================================================================
; Every named RAM cell, hardware port and notable ROM table, with its
; address and role. The names are EXACTLY the symbols the annotated
; listing (omega_main.asm) prints inline - both come from the same
; table in z80trace.py, so this file is the glossary for the listing.
;
; Everything here was traced from the ROM itself (and the schematic,
; for the clocks); the disassembly method and per-routine reference
; are in README.md / FUNCTIONS.md, subsystem detail in the *_NOTES.md
; files, frame pacing in ../FRAME_PACING_NOTES.md.
;
; THE ONE CONVENTION THAT MATTERS: the mainline loads IY = $4080 at
; 0x0958 / 0x0A9D / 0x0F39 and keeps it there, so every (IY+d) /
; (IY-d) operand in the listing is the RAM cell $4080+d. Resolve the
; arithmetic before trusting a name: (IY-105) = $4017 = tick_244hz,
; (IY-2) = $407E = obj_class, (IY+26) = $409A = wall_fx, etc.
;
; Standard Z80 EQU syntax (sjasmplus/zasm/pasmo compatible).

;--------------------------[ Memory boundaries ]----------------------

ProgramRom:     EQU $0000   ; through $3FFF. 16K: omega.m7/l7/k7/j7
MainRam:        EQU $4000   ; through $4BFF. 3K work RAM
StackTop:       EQU $4BFC   ; SP loaded here at reset
Nvram:          EQU $5C00   ; through $5CFF. 4-bit battery-backed RAM
VectorRam:      EQU $8000   ; through $8FFF. 4K, shared with the DVG
VectorRom:      EQU $9000   ; through $9FFF. 2K: omega.e1 + omega.f1
                            ; (DVG word address $800-$FFF)

;-----------------[ Staged-object workspace  $4000-$4016 ]------------
; OBJ_STAGE_RUN ($19CD) copies the current object's 23-byte record
; here, runs the shared engine (physics/collide/dispatch), and copies
; it back. These cells are therefore PER-OBJECT state, not globals -
; obj_aim especially. Offsets shown are also the record layout.

obj_flags0:     EQU $4000   ; +0  b2 fire request (ship; consumed by
                            ;     the $1E9B spawn gate)
                            ;     b3 friction applies this pass
                            ;     b4 shape from dispatch (heading/anim)
                            ;        instead of the record word
                            ;     b5 thrust gate (template constant)
                            ;     b6 integrate position / wall-collide
                            ;     b7 active
obj_flags1:     EQU $4001   ; +1  b2 dlist entry live (set at first
                            ;     stage-run, $1B03)
                            ;     b5 dying - explosion anim ($1DB4)
                            ;     b6 heading-shape path (ship); also
                            ;        stamped on pool shots by
                            ;        SPAWN_MINE_SHOT
                            ;     b7 anim set: droid vs death-ship
obj_velx:       EQU $4002   ; +2  int16, high byte = screen units
obj_vely:       EQU $4004   ; +4  int16
obj_posx:       EQU $4006   ; +6  int16, high byte = screen X
obj_posy:       EQU $4008   ; +8  int16, high byte = screen Y
obj_angle:      EQU $400A   ; +10 heading 0-63
obj_lifetime:   EQU $400C   ; +12 countdown; expiry clears flags0 b7
obj_fire_timer: EQU $400E   ; +14 enemy fire timer; expiry reloads
                            ;     $2C - 3*wave_num, raises the request
obj_aim:        EQU $400F   ; +15 aim heading: ship stores its own
                            ;     ($1E1B), enemies get AIM_AT_SHIP's
obj_radx:       EQU $4010   ; +16 collision half-box X
obj_rady:       EQU $4011   ; +17 collision half-box Y
obj_timer:      EQU $4013   ; +19 elapsed-tick accumulator (motion)
obj_shape_word: EQU $4015   ; +21 JSRL display-list word ($1E63 arm)

;--------------------------[ Timebase ]-------------------------------

tick_244hz:     EQU $4017   ; free-running IRQ tick. The interrupt is
                            ; 244.140625 Hz (12 MHz / 49152, traced on
                            ; schematic omegaM2) - NOT the 250 Hz the
                            ; literature quotes
tick_prev:      EQU $4018   ; tick at previous MAINLOOP pass
tick_delta:     EQU $4019   ; ticks elapsed since - the per-frame
                            ; motion scale (the loop free-runs)
last_kick_tick: EQU $401A   ; tick at last DVG kick ($17C8)
seconds_ctr:    EQU $4020   ; ~1 Hz, incremented on tick wraparound
timer_seconds:  EQU $4021   ; general 1 Hz countdown (death pause,
                            ; attract page holds, slam 3 s hold)

;--------------------------[ Coins / credits / service ]--------------

coin1_debounce: EQU $401B   ; 5-tick debounce counters per chute
coin2_debounce: EQU $401C
coin1_pending:  EQU $401D   ; accepted, awaiting meter pulse
coin2_pending:  EQU $401E
meter_pulse_t:  EQU $401F   ; coin-meter pulse timer ($32 -> 0)
game_mode:      EQU $4024   ; 1 attract, 2 spawn/demo, 3 play,
                            ; 4 high-score initials entry
coin1_batch:    EQU $4025   ; coins banked toward the next credit
coin2_batch:    EQU $4026
coinage1_ptr:   EQU $4027   ; -> coinage table row ($2FC3)
coinage2_ptr:   EQU $4029
coin_mode1:     EQU $402B   ; DIP-decoded pricing mode per chute
coin_mode2:     EQU $402C
credits_bcd:    EQU $402D   ; BCD credits, capped $20
svc_flags:      EQU $402E   ; b6 credit display dirty (coin path sets,
                            ; $1788 clears), b7 gates SOUND_CMD_SEND
coin_lines:     EQU $4035   ; previous coin-line sample (edge detect)
led_shadow:     EQU $4072   ; shadow of out port $13
slam_latch:     EQU $4082   ; slam alarm active (credit penalty +
                            ; 3 s freeze + sound $13)

;--------------------------[ Input edges ]----------------------------

in1_state:      EQU $402F   ; port $11 sampled/edge bits
in1_pressed:    EQU $4030
in1_released:   EQU $4031
in2_state:      EQU $4032   ; port $12 sampled/edge bits
in2_pressed:    EQU $4033
in2_seat_raw:   EQU $4034   ; cocktail seat-2 raw sample

;--------------------------[ Score / players / lives ]----------------

score_cur:      EQU $4036   ; 4 bytes BCD, little-endian
time_played_bcd:EQU $403A   ; 4 bytes BCD seconds (audit)
cur_player_num: EQU $403E   ; active player, 1 or 2 (selects save
                            ; block, cocktail port/flip, pages)
player_sel:     EQU $403F   ; players still owed a mode-4 initials
                            ; entry (decremented at $12AA)
start_credits:  EQU $4040   ; credit price of the pending start
                            ; (1/2/4), BCD-debited once at $0BE3.
                            ; NOT the player count
game_flags:     EQU $4041   ; b2 cocktail seat 2, b3 swap pending,
                            ; b4 hiscore-entry gate, b5 deluxe start
                            ; (2 credits/player: bonus ship + own
                            ; hiscore bank), b6 2-player, b7 game
                            ; in progress
score_disp:     EQU $4042   ; 4-byte score display shadow
p1_save:        EQU $4046   ; 13-byte per-player save areas
p1_score_final: EQU $404B   ; final-score BCD inside the save area
p2_save:        EQU $4053
p2_score_final: EQU $4058
xlife_thr1:     EQU $4060   ; bonus-ship thresholds from the DIP
xlife_thr2:     EQU $4062   ; table ($2FE5/$2FED), BCD word image
xlife_thr3:     EQU $4064
lives:          EQU $4066
kill_tier:      EQU $406F   ; points-table index of the last kill
counter_406D:   EQU $406D   ; SCORE_ADD event counter (audit)
cocktail_ref:   EQU $406E   ; seat-swap reference

;--------------------------[ Wave / enemy scheduling ]----------------

wave_pace_timer:EQU $4022   ; 1 Hz, starts $19 (25 s); expiry sets
                            ; xlife_flags b7 (arms the fire cycle);
                            ; ship death reloads it; a shooter_a kill
                            ; reloads 7 (or 2 late-game, $2310/$231E)
wave_start_timer:EQU $4023  ; 1 Hz; expiry sets xlife_flags b6 +
                            ; wave_flags b7 ("wave started" - gates
                            ; thrust in OBJ_PHYSICS)
wave_num:       EQU $4067   ; really the droid count: attract 4,
                            ; play starts 6, +2 per wave, cap 12
xlife_flags:    EQU $406A   ; b3-b5 extra-life award latches;
                            ; b6 start-timer expired, b7 pace-timer
                            ; expired; b0 late-game latch ($24DB)
                            ; selecting the 2/2 shooter-kill re-pace
wave_flags:     EQU $406B   ; b6 wave complete, b7 wave started
wave_ctr:       EQU $406C   ; waves cleared (bonus page every 4th)
fire_cooldown:  EQU $4070   ; -= tick_delta, floor 0; ALSO the page
                            ; scrawl rate limiter (PAGE_SCRIPT_STEP
                            ; reloads 10, 25 at line end -> 25 gl/s)
flame_flicker:  EQU $4071   ; VG_RESTART_FRAME counter; odd = thrust
                            ; flame JSRL into the ship sub-list
random_val:     EQU $4073   ; PRNG state (RANDOM_NEXT $2906: R reg +
                            ; tick tap + caller's A)
settle_track_x: EQU $4075   ; previous object's pos high bytes -
settle_track_y: EQU $4076   ; beam-settle distance tracker ($1A41)
shooter_state:  EQU $4077   ; 0/1/2+ shooter-eligible droids
prng_cache:     EQU $4079   ; cached PRNG output (NOT a cocktail flag)
shooter_a:      EQU $407A   ; designated shooter object numbers
shooter_b:      EQU $407B   ; ($1BFE / $1DAD)
special_countdown:EQU $407C ; special-ship countdown; reload $14-$23
                            ; by DIFFICULTY_CALC, dec per shooter
                            ; stage-run; shooter_a kill sets 5 (2 late)
obj_index:      EQU $407D   ; object (1-32) being staged this pass
obj_class:      EQU $407E   ; staged object's class byte (COLLIDE_SCAN
                            ; also counts misses here - it is scratch,
                            ; NOT the player number)
special_armed:  EQU $407F   ; $1D18 sets $FA when the fire cycle runs;
                            ; gates SPAWN_SPECIAL
pair_min:       EQU $4080   ; closest-pair minimum (16-bit) - yes,
                            ; this is also where IY points
wall_fx:        EQU $409A   ; 20-entry wall-segment glow array, decays
                            ; by tick delta (WALL_FLASH_DECAY $1868)

;--------------------------[ Attract / pages / initials ]-------------

attract_step:   EQU $4083   ; attract sequence position
page_id:        EQU $4084   ; current text page (ROM table $3F15)
page_ptr:       EQU $4085   ; page flag-byte ROM address
page_rec_next:  EQU $4087   ; next segment-record ROM address
page_line_cur:  EQU $4089   ; current line (1-based)
page_line_glyph:EQU $408A   ; glyph bytes emitted this line
page_line_total:EQU $408B   ; lines in the page
page_line_len:  EQU $408C   ; glyph bytes in the current line
page_script_ptr:EQU $408D   ; glyph source ROM address (scrawl)
vlist_cursor:   EQU $408F   ; display-list write address
initials_ptr:   EQU $4094   ; initials write pointer (entry screen)
initials_cur:   EQU $4096
initials_cnt:   EQU $4098
letter_sel:     EQU $4099   ; letter-wheel selection

;--------------------------[ Frame-engine flags ]---------------------

frame_tail_a:   EQU $40A4   ; per-frame one-shot flags, set each
frame_tail_b:   EQU $40A7   ; frame by FRAME_ENGINE's tail
frame_tail_c:   EQU $40AA
frame_tail_d:   EQU $40AD

;--------------------------[ Object records & tables ]----------------
; The ROM table at $2E90 gives every object its trio of pointers:
;   OBJ n: record (23 bytes, RAM), template (23 bytes, ROM $2F50+),
;          dlist entry (12 bytes, shadow $4481+)
; Records are contiguous: object n's record = $40B2 + 23*(n-1).
; Object 1 is the ship. Record layout = the workspace offsets above.

ship_record:    EQU $40B2   ; object 1's record
ship_x:         EQU $40B9   ; = ship record +7 (posx high byte)
ship_y:         EQU $40BB   ; = ship record +9 (posy high byte)
ship_angle:     EQU $40BC   ; = ship record +10. NOT a separate cell:
                            ; the spinner writes it ($12F6), physics
                            ; and the letter wheel read it back
hiscore_work:   EQU $43A9   ; working high-score table, 6 x 7 bytes
                            ; (4 BCD + 3 initials)
hiscore_bank1:  EQU $43D3   ; persisted banks (normal / deluxe)
hiscore_bank2:  EQU $43FD
bookkeep_ctrs:  EQU $4427   ; operator audit ledger, 4-byte BCD each
opmenu_disp1:   EQU $4471   ; operator-menu pricing display buffers
opmenu_disp2:   EQU $4479
dlist_shadow:   EQU $4481   ; display-list shadow: 36 x 12-byte object
                            ; entries; VG_RESTART_FRAME copies $01B0
                            ; bytes to $8000 per frame
score_template: EQU $4601   ; 48-byte score-line template, copied from
                            ; ROM $30D9 at reset
flame_word:     EQU $4611   ; thrust-flame JSRL staged for $818C
credit_digits:  EQU $462B   ; credit display digit staging

;--------------------------[ NVRAM  ($5C00, 4-bit cells) ]------------

nvram:          EQU $5C00   ; high scores, bookkeeping, credits;
                            ; validated nibble-wise by NVRAM_RD_BYTE
nvram_template: EQU $5C1A   ; validation nibbles
nvram_credits:  EQU $5C56   ; credits_bcd plain; $5C57 = the same
                            ; byte nibble-swapped (integrity pair)

;--------------------------[ Vector RAM landmarks ]-------------------
; The DVG walks vector RAM from $8000. DLIST_RESET halt-fills
; $8000-$8BF7; the object region $8000-$81AF is re-copied from the
; shadow every frame.

dlist_objects:  EQU $8000   ; 36 x 12-byte object entries
title_glyph_a:  EQU $8080   ; page 2's 4th/5th icon glyph words -
title_glyph_b:  EQU $80A8   ; VG_KICK_TITLE rewrites them from
                            ; $3F4D/$3F55 = the title shimmer
ship_sublist:   EQU $818A   ; ship hull JSRL; $818C = RTSL or flame
ship_flame:     EQU $818C   ; JSRL (the thrust flicker); $8190 hull
score_digits1:  EQU $8196   ; score digit runs
blink_mark:     EQU $81A6   ; 2P active-player score blink word
                            ; ($F000 or the $9B84 mark, $17AB)
credit_digits_v:EQU $81AA   ; credit digits in the game screen list
score_digits2:  EQU $8246
lives_icons:    EQU $8258   ; through $82BA: 7 tally + 3 bonus-ship
                            ; slots, LABS scale=0 + JSRL each
ship_buf_flip:  EQU $87F0   ; runtime ship frame sets: $9000-$9407
ship_buf_180:   EQU $8BF8   ; copied then sign-flipped in place
                            ; (WALL_BUFFERS_BUILD $18BC)

;--------------------------[ Hardware I/O ports ]---------------------
; Reads:
VG_GO:          EQU $08     ; read STARTS the vector generator
WATCHDOG:       EQU $09     ; read kicks the watchdog
VG_RESET:       EQU $0A     ; (out) vector generator reset
VG_STATUS:      EQU $0B     ; d7 = 1 while the DVG is drawing
DSW_C4:         EQU $10     ; DIP bank C4 (coinage, lives, bonus)
IN_P1:          EQU $11     ; d0/d1 coins, d4 slam, d5 thrust,
                            ; d6 fire, d7 test switch (active low)
IN_P2:          EQU $12     ; d6 start1, d7 start2 (active low)
SPINNER1:       EQU $15     ; 6-bit Gray-coded spinner, player 1
SPINNER2:       EQU $16     ; player 2 (cocktail)
DSW_C6:         EQU $17     ; DIP bank C6
; Writes:
OUT_LEDS:       EQU $13     ; player LEDs, coin meters, screen flip
SOUND_CMD:      EQU $14     ; command latch to the sound board (the
                            ; command set is decoded in SOUND_NOTES.md)

;--------------------------[ ROM tables of note ]---------------------

PostScreens:    EQU $01A0   ; through $04E7: POST display-list
                            ; headers, labels, cross-hatch, grid
ObjTable:       EQU $2E90   ; per-object record/template/entry ptrs
Templates:      EQU $2F50   ; 23-byte object templates: ship $2F50,
                            ; droid $2F67, shot/mine $2F7E, command
                            ; $2F95, inert $2FAC
CoinageTbl:     EQU $2FC3   ; coins-per-credit / credits pairs
LivesTbl:       EQU $2FD3   ; lives per DIP (normal / deluxe row)
KillPointsTbl:  EQU $2FDB   ; kill point values, BCD words
XlifeDipTbl:    EQU $2FE5   ; bonus-ship DIP thresholds
XlifeThrTbl:    EQU $2FED   ; extra-life thresholds 2/3
DefaultScores:  EQU $2FF7   ; default high scores + initials
ShipBaseTbl:    EQU $30A1   ; ship hull set base per heading quadrant
ShipFrameTbl:   EQU $30A9   ; per-heading frame offsets
ScoreLineTmpl:  EQU $30D9   ; 48-byte score-line dlist template
GameScreen:     EQU $3109   ; walls + (via JSRL) score/lives words
PageData:       EQU $3217   ; through $3FCC: text page records,
                            ; letter-wheel blob ($3A0E), glyph tables
                            ; ($3A2E A-Z, $3FD3 digits/punctuation)
PageTable:      EQU $3F15   ; page id -> record chain
AnimTbl:        EQU $3F4D   ; 4-frame anim shapes: 0-3 droid,
                            ; 4-7 death ship; also the title shimmer
ExplosionTbl:   EQU $3F5D   ; explosion anim frames (angle & 3)
BcdOneTbl:      EQU $3FF7   ; BCD +1 constant (BOOKKEEP_COIN)
