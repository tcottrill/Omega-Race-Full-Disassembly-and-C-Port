#!/usr/bin/env python3
"""Tracing Z80 disassembler for the Omega Race ROMs.

Follows control flow from a set of entry points, so code and data are
separated properly.  Anything in the code address range that is never
reached is rendered as byte data, and a coverage report is printed so
jump tables / computed-goto targets can be added to EXTRA_ENTRIES and
WORD_TABLES iteratively.

Usage: python z80trace.py <config>   where <config> is 'main' or 'sound'
"""

import sys

# ---------------------------------------------------------------------------
# configurations
# ---------------------------------------------------------------------------

CFG_MAIN = {
    "binfile": "omega_dump.bin",
    "outfile": "omega_main.asm",
    "code_lo": 0x0000,
    "code_hi": 0x4000,          # exclusive
    "entries": [0x0000, 0x0038],
    # manually confirmed extra code entry points (jump-table targets etc.)
    # 0x0066: NMI vector (just retn, NMI unused on main board)
    # 0x1AE8: dead stanza, skipped by "jr L1AF9" at 0x1AE6 (patched out)
    # 0x2C14: dead code island, no remaining references
    "extra_entries": [0x0066, 0x1AE8, 0x2C14],
    # (start, end_exclusive, entries_are_code) word tables rendered as dw
    "word_tables": [],
    # typed data regions: (start, end_exclusive, type, comment)
    # types: dw / ascii / objtable / rec23 / bytepairs / bcdwords /
    #        hiscore7 / script / db
    "data_defs": [
        (0x01A0, 0x01CB, "dw",      "POST screen display-list header "
                                    "(defines 4-glyph subroutines at JSRL "
                                    "005='  OK' 00A=' NG ' 00F=blank; each "
                                    "ends in a shared newline vector)"),
        (0x01CB, 0x026E, "ascii",   "POST test-line labels (trailing '.' "
                                    "= JSRL 00F blank+newline; result "
                                    "posting overwrites it with 005/00A)"),
        (0x026E, 0x02C6, "dw",      "input-test page display list"),
        (0x02C6, 0x042F, "ascii",   "input/DSW test labels"),
        (0x042F, 0x0447, "dw",      "cross-hatch page display list"),
        (0x0447, 0x04E8, "dw",      "grid page display list"),
        (0x2E4B, 0x2E90, "db",      "scratch/padding + dummy entry"),
        (0x2E90, 0x2F50, "objtable","object table: record/template/"
                                    "dlist entry (12-byte, in the "
                                    "shadow 0x4481+, mirrored to the "
                                    "0x8000 object region per frame)"),
        (0x2F50, 0x2FC3, "rec23",   "object templates: ship, droid, "
                                    "shot/mine, command + inert(2FAC)"),
        (0x2FC3, 0x2FD3, "bytepairs","coinage: coins-per-credit/credits"),
        (0x2FD3, 0x2FDB, "bytepairs","lives per DIP (normal/alt)"),
        (0x2FDB, 0x2FE5, "bcdwords","kill point values (BCD)"),
        (0x2FE5, 0x2FED, "dw",      "bonus-ship DIP thresholds"),
        (0x2FED, 0x2FF7, "dw",      "extra-life thresholds 2/3 (sole "
                                    "reader: ATTRACT_INIT 0x0B29)"),
        (0x2FF7, 0x3021, "hiscore7","default high scores + initials"),
        (0x3021, 0x3041, "bytepairs","quarter-wave sin/cos (dx,dy)"),
        (0x3041, 0x3061, "dw",      "aim slope->angle buckets"),
        (0x3061, 0x30A1, "db",      "spinner gray-code decode"),
        (0x30A1, 0x30A9, "dw",      "SHIP_BASE: hull frame-set base per "
                                    "heading bits 4-5 (sets 2-3 live in "
                                    "vector RAM, built by 0x18BC)"),
        (0x30A9, 0x30C9, "dw",      "hull frame offsets[16]: shape = "
                                    "0xC000|(base+ofs), flame = shape-7"),
        (0x30C9, 0x30D9, "db",      "NVRAM validation template"),
        (0x30D9, 0x3109, "dw",      "status-line dlist template"),
        (0x3109, 0x3217, "dw",      "playfield walls display list -> "
                                    "0x81B0 (318D-319C: 8x FFFF "
                                    "placeholders, runtime-patched at "
                                    "0x8234 with the JSRLs from 0x3EE5)"),
        (0x3217, 0x33D9, "script",  "attract/message page scripts"),
        (0x33D9, 0x33F6, "dw",      "attract page dlist fragment "
                                    "(loaded by ATTRACT_PAGE_LOAD)"),
        (0x33F6, 0x3A0E, "script",  "message page scripts (cont.)"),
        (0x3A0E, 0x3A88, "dw",      "letter-wheel alphabet display list "
                                    "(0x7A bytes, SHIP_RESPAWN -> dlist "
                                    "in mode 4; 0x3A2E inside it doubles "
                                    "as DRAW_TEXT's A-Z JSRL table)"),
        (0x3A88, 0x3EE5, "script",  "message page scripts (cont.)"),
        (0x3EE5, 0x3F15, "dw",      "3x8 JSRL words -> dlist 0x8234: "
                                    "calls to score/credit/lives "
                                    "sub-lists (rows 2/3 = player-swap "
                                    "orders, loaded at 0x0D71/0x0DA3)"),
        (0x3F15, 0x3F4D, "dw",      "page pointer table (page 1..)"),
        (0x3F4D, 0x3F5D, "dw",      "anim shape words: droid 3F4D[0-3] / "
                                    "death ship 3F55[0-3] (move_mode "
                                    "0x1E03 arm; VG_KICK_TITLE reuses "
                                    "both rows as the title shimmer)"),
        (0x3F5D, 0x3F65, "dw",      "explosion anim shape words "
                                    "(0x1DB4 arm, indexed angle&3)"),
        (0x3F65, 0x3FCD, "dw",      "hiscore-page dlist fragments (word "
                                    "0 = 0xC0C5, the ship entry's JSRL "
                                    "to its 0x818A hull/flame sub-list)"
                                    "\n0x3F69 + 0x64 bytes doubles as "
                                    "the player-2 lives/bonus icon-slot "
                                    "template: PLAYER_TURN_SWAP 0x0DAE "
                                    "copies it to dlist 0x8258 on the "
                                    "swap to player 2, mirroring the "
                                    "0x31B1 -> 0x8258 copy (walls blob "
                                    "offset 0xA8) done for player 1"),
        (0x3FCD, 0x3FF7, "dw",      "digit/char glyph JSRL words "
                                    "(CHAR_GLYPH_EMIT 0x2C36; index "
                                    "base 0x3FD3 for chars 0x2F-0x40)"
                                    "\n3FCD = D000 guard; "
                                    "3FCF = '.' CDAA; 3FD1 = ',' CDB3"
                                    "\n'/'=CDD3  '0'-'9'=CD2F..CDA0  "
                                    "':'=CD92 (space glyph)"
                                    "\n';'=CBA7 (0x974E points-caption "
                                    "enemy icon)  '>'=CBAF (0x975E icon)"
                                    "\n'<'=D000 - an RTSL, the 'return' "
                                    "char game-mode pages end with, so "
                                    "a JSRL'd page sub returns to its "
                                    "caller"
                                    "\n'='=CDCA  '?'=CA04 / '@'=CA2E "
                                    "(droid anim frames 0/1 - page 2 "
                                    "draws its shimmer enemy icons as "
                                    "text)"),
        (0x3FF7, 0x4000, "db",      "constants (0x3FF7 = BCD +1)"),
    ],
    # Naming convention: UPPERCASE = function entry points (verified),
    # lowercase = internal blocks / steps / best-guess identifications.
    "symbols": {
        0x0000: "RESET",
        0x0038: "IRQ_244HZ",
        0x0068: "irq_slam_credit_penalty",
        0x007C: "irq_coin_edge_detect",
        0x0097: "irq_coin1_accept",
        0x00EA: "irq_coin2_accept",
        0x013D: "irq_meter_pulse",
        0x0159: "irq_meter1_start",
        0x016A: "irq_meter2_start",
        0x018F: "irq_meter_release",
        0x019B: "irq_exit",
        0x04F5: "post_draw_header",
        0x0548: "post_rom1_result",
        0x0553: "post_csum_rom2",
        0x0560: "post_rom2_result",
        0x056B: "post_csum_rom3",
        0x0578: "post_rom3_result",
        0x0583: "post_csum_rom4",
        0x0590: "post_rom4_result",
        0x059B: "post_ramtest_4000",
        0x05AD: "post_ram1_result",
        0x05B8: "post_ramtest_4400",
        0x05C5: "post_ram2_result",
        0x05D0: "post_ramtest_4800",
        0x05DD: "post_ram3_result",
        0x05E8: "post_nvram_test",
        0x05FA: "post_nvram_result",
        0x0605: "post_vramtest_835A",
        0x0617: "post_vram1_result",
        0x0622: "post_vramtest_8400",
        0x062F: "post_vram2_result",
        0x063A: "post_vramtest_8800",
        0x0647: "post_vram3_result",
        0x0652: "post_vramtest_8C00",
        0x065F: "post_vram4_result",
        0x066A: "post_csum_vrom1",
        0x067C: "post_vrom1_result",
        0x0687: "post_csum_vrom2",
        0x0694: "post_vrom2_result",
        0x069F: "post_wait_fire",
        0x06A6: "post_input_page",
        0x06B0: "post_input_draw",
        0x06CE: "post_input_loop",
        0x06E6: "post_show_p2_bits",
        0x06FB: "post_show_spinner1",
        0x070A: "post_show_spinner2",
        0x0717: "post_show_dsw_c4",
        0x0722: "post_show_dsw_c6",
        0x0734: "post_xhatch_page",
        0x073D: "post_xhatch_draw",
        0x0748: "post_xhatch_loop",
        0x075F: "post_grid_draw",
        0x076A: "post_grid_loop",
        0x0781: "post_sound_test",
        0x0786: "post_sound_loop",
        0x07BD: "vg_wipe_body",
        0x07EF: "csum_page_loop",
        0x0809: "csum_result_mark",
        0x0828: "ramtest_body",
        0x086A: "nvramtest_body",
        0x08A8: "showbits_loop",
        0x08E2: "dtxt_period",
        0x08EB: "dtxt_letter",
        0x08FC: "dtxt_digit",
        0x090E: "dtxt_push_glyph",
        0x091C: "blink_loop",
        0x1750: "VG_RESTART_FRAME",   # copy shadow 0x4481 -> 0x8000
                                      # (0x1B0 = 36 entries); 0x1763-78
                                      # thrust flame strobe (0x4071 odd
                                      # -> flame JSRL into 0x818C, else
                                      # RTSL); 0x17CB WALL_FLASH_DECAY
                                      # on wall_fx[0] b7; kick DVG
        0x1854: "VG_WAIT_DONE",
        0x185D: "WAIT_NEXT_TICK",     # spin until 244Hz tick changes
        0x1868: "WALL_FLASH_DECAY",   # decay wall_fx; writes intensity
                                      # nibbles into wall list 0x81BB
                                      # stride 6 (C0/70/&0F); called from
                                      # VG_RESTART_FRAME on wall_fx[0] b7
        0x18BC: "WALL_BUFFERS_BUILD", # ship frame set 0x9000 (0x408 B)
                                      # -> vec RAM 0x8BF8 (180 deg) and
                                      # 0x87F0 (Y-flip) via VWALL_XFORMs;
                                      # builds hull frames for headings
                                      # 0x20-0x3F (X-flip set is in ROM;
                                      # its 0x9BF8 ldir is a no-op)
        0x04E8: "POST_START",
        0x04EC: "POST_CYCLE",
        0x050A: "POST_TESTSW_GATE",
        0x07A0: "VG_WAIT_IY",         # wait VG idle (timeout), resume via IY
        0x07A2: "VG_WAIT_TIMEOUT",
        0x07B6: "VG_WAIT_WIPE",       # wait idle then fill list with HALTs
        0x07CF: "VG_KICK_IY",         # kick VG if idle, resume via IY
        0x07DD: "EDGE_BIT6_IX",       # port11 edge detect, Z=bit6, via IX
        0x07E9: "POST_CSUM_BLOCK",    # checksum HL..HL+BC, Z=ok, via IX
        0x081D: "POST_RAMTEST",       # 55/AA/00 pattern test, via IX
        0x0863: "POST_NVRAMTEST",     # 4-bit RAM test (0xA/0x5), via IX
        0x08A0: "POST_SHOW_BITS",     # draw B bits of C as pass/fail marks
        0x08C8: "DRAW_TEXT",          # BC chars from HL -> JSRL words at DE
        0x0917: "POST_FAIL_BLINK",    # blink LEDs/sound B times, via IX
        0x0955: "GAME_INIT",
        0x0A18: "NVRAM_RD_BYTE",      # read+validate 2 BCD nibbles via RRD
        0x0A2E: "NVRAM_WR_BYTES",     # write B bytes as nibble pairs
        0x0A3B: "NVRAM_WIPE",         # zero credits + bookkeeping
        0x0A4C: "SCORES_RESET",       # default high scores, save NVRAM
        0x0A68: "NVRAM_SAVE_ALL",
        0x0A9D: "ATTRACT_INIT",       # game_mode = 1
        0x0B83: "GAME_START",         # game_mode = 2 (also attract demo)
        0x0BD1: "PLAY_BEGIN",         # game_mode = 3, deduct credits
        0x0C98: "PLAY_FRAME",         # in-play per-frame body
        0x0CD4: "WAVE_CLEAR_ANIM",    # blocking bonus count-up loop
                                      # waits 5 laps of the free-running
                                      # 244 Hz tick (sub/jr nc to wrap)
        0x0D1B: "TURN_SWITCH_CHECK",  # 2P alternation / game over
        0x0D51: "PLAYER_TURN_SWAP",
        0x0DDF: "FRAME_TAIL",         # per-frame bookkeeping end
        0x0E1E: "WAVE_ADVANCE",       # wave_num += 2, clamp 12
        0x0E3D: "WAVE_ROSTER",        # kill 1..21 / spawn 1..wave+1
                                      # 0x0E56-89: on wave 1 only
                                      # (wave_num==6) res 6,flags0 parks
                                      # all six droids; designation
                                      # (set 6) releases them one by one
        0x0E8D: "WAVE_COMPLETE_DET",  # via ENEMY_ALIVE_SCAN
        0x0EA4: "DEMO_AUTODESTRUCT",
        0x0EE6: "SHIP_RESPAWN",       # also mode-4 entry: 0x0EF7 copies
                                      # the 0x7A-byte letter-wheel
                                      # alphabet dlist from 0x3A0E
        0x0BEB: "play_score_setup",
        0x0C1E: "play_lives_setup",
        0x0C88: "play_roster_init",
        0x1063: "att_step1_idle",
        0x1066: "att_step2_title",
        0x1077: "att_step3_instr",
        0x1088: "att_step4_instr2",
        0x1099: "att_step5_scores",
        0x10B5: "att_step6_scores2",
        0x10D1: "att_step7_demo",
        0x115D: "opmenu_loop",
        0x1185: "opmenu_zero_credits",
        0x118B: "opmenu_wipe_check",
        0x11D7: "start_led_blink",
        0x1204: "start_2p_setup",
        0x1247: "start_none",
        0x12F9: "fe_thrust_input",
        0x1309: "fe_fire_request",
        0x1323: "fe_death_check",
        0x134A: "fe_death_start",
        0x1396: "fe_state_gate",
        0x13AB: "fe_gameover_check",
        0x13D9: "fe_join_check",
        0x1417: "fe_spawn_gate",
        0x142A: "fe_respawn_tick",
        0x147B: "fe_next_player",
        0x1494: "fe_tail_sounds",
        0x14B2: "fe_halfplane_toggle",
        0x0F30: "MAINLOOP_SYNC",
        0x0F39: "MAINLOOP",           # 0x0F51-0FCB runs only on tick
                                      # wrap (~1 Hz): on-time BCD, slam
                                      # alarm, wave_pace_timer (expiry
                                      # -> 0x406A b7), wave_start_timer
                                      # (expiry -> 0x406A b6 + wave_
                                      # flags b7); 0x0FDA gates VG/objs
                                      # on game_mode bit 1
        0x1030: "ATTRACT_STEP_DISP",  # dec-chain on attract_step
        0x10DC: "ATTRACT_PRICING",    # coin pricing page from coinage tbl
        0x1137: "OPERATOR_MENU",      # test switch during attract
        0x1194: "ATTRACT_FRAME_TAIL",
        0x11AA: "START_BTN_CHECK",    # nz = start game; blinks start LEDs
        0x1248: "START_LEDS_OFF",
        0x127B: "HISCORE_ENTRY_FRAME",# game_mode = 4 per-frame
        0x12D2: "FRAME_ENGINE",       # per-frame objects+list (play/demo)
        0x14F4: "INPUTS_SAMPLE",      # ports 11/12 -> 0x402F-0x4033 edges
        0x1518: "BOOKKEEP_COIN",      # A=slot: add BCD incr to 0x4427 array
        0x1521: "BOOKKEEP_ADD",       # add BCD (HL) to counter A at 0x4427
        0x1540: "AUDIT_SNAPSHOT",     # last-life bookkeeping stats fold
        0x158C: "BOOKKEEP_DECAY_AVG", # leaky running-average ledger upd
        0x1614: "BOOKKEEP_HIGHWATER", # ledger[a] = max(ledger[a],0x403A)
        0x1639: "BOOKKEEP_PTR",       # A -> bookkeep_ctrs + A*4
        0x1646: "COINAGE_DISP_PREP",  # operator-menu pricing buffers
        0x1674: "BCD_DIV4",           # 4B BCD divide by unit (rpt sub)
        0x16D5: "BCD_ADD4",           # 4-byte BCD add into (DE)
        0x16E3: "CREDITS_NVRAM_SYNC", # credits -> 0x5C56 plain + 0x5C57
                                      # nibble-swapped; read back by
                                      # GAME_INIT via NVRAM_RD_BYTE
        0x16F1: "ATTRACT_PAGE_LOAD",  # big attract dlist from 0x33D9
        0x1711: "P1_INPUT_READ",      # port 0x11, cocktail seat-aware
        0x1737: "SPINNER_READ",
        0x1902: "GAME_DLIST_BUILD",   # walls/score line/status into list
        0x1985: "OBJ_UPDATE_LOOP",    # per-object, SP pops 0x2E90 entries
        0x19CD: "OBJ_STAGE_RUN",      # stage record -> 0x4000 wkspc, run
        0x199B: "obj_expire_slot",
        0x19C1: "obj_build_check",
        0x19FC: "obj_proximity_upd",  # beam-settle delay from distance
                                      # to prev obj (0x4075/76 tracker);
                                      # tail pops to THIS obj's dlist
                                      # entry, writes word3 high (+7)
        0x1A41: "obj_track_pos",
        0x1A4D: "obj_skip_entry",
        0x1A51: "obj_next",
        0x1A59: "FRAME_SYNC_DISP",    # VG kick + per-object-# dispatch
        0x1A88: "fsd_shots_collide",
        0x1AA4: "fsd_ship_object",
        0x1ABE: "fsd_reset_pairmin",
        0x1ACA: "fsd_droids_collide",
        0x1AD8: "fsd_mines_collide",
        0x1AF4: "fsd_do_collide",
        0x1AF9: "fsd_slot_init_once",
        0x1B16: "OBJ_PHYSICS",        # thrust/velocity/position integr.
                                      # thrust gated on wave_flags b7;
                                      # 0x1BB5: no integration unless
                                      # flags0 b6 (parked objs hold still)
        0x1BEB: "enemy_fire_gate",    # (verified, diff-tested 2000/2000)
                                      # shooter designation + release
                                      # (set 6 = grant wall-collide),
                                      # fire cycle off 0x4022, special
                                      # arming 0x407F/0x407C
        0x1D54: "quadrant_engage_chk",# staged obj vs ship same-half test
                                      # (XOR posx/posy high sign bits):
                                      # diff half -> WALL_TRACK_STEER,
                                      # same -> engage (aim -> heading)
        0x1DB4: "move_mode_dispatch", # (verified) per-stage-run tail:
                                      # dying -> explosion anim 0x3F5D;
                                      # shape select (flags0 b4/flags1 b6
                                      # -> record word / heading path
                                      # 0x1E18 / anim table 0x3F4D);
                                      # b12 lifetime + b14 fire timer;
                                      # 0x1E9B fire gate -> SPAWN_MINE_
                                      # SHOT; beam-settle tail 0x1EE5
        0x1F2A: "WALL_BOUNCE",        # reflect velocity, halve speed
        0x2017: "bounce_reflect_y",
        0x2030: "bounce_reflect_x",
        0x2074: "bounce_halve_retest",
        0x20CE: "WALL_HIT_FX",        # segment glow array 0x409A, snd 8
                                      # outcome set {1..8,0A,0D,10,13}
                                      # = wall_fx index of struck seg
        0x2148: "APPLY_FRICTION",     # v -= v/8 per frame
        0x2164: "CLAMP_SPEED",        # clamp velocity to +/-0x02BC
        0x2186: "DIFFICULTY_CALC",    # wave-based timing param -> 0x407C
        0x2199: "COLLIDE_SCAN",       # B objs at IX vs ship, bbox radii
        0x21C0: "cs_delta_calc",
        0x2201: "cs_next_entry",
        0x2210: "cs_hit_confirm",     # 3 outcomes: kill / mutate cand.
                                      # into death ship (b13=2, rady=6,
                                      # shape 0xCBAF) / no-op; the last
                                      # two END the scan (jp 0x2334)
        0x2253: "cs_kill_object",
        0x2335: "SCORE_ADD",          # BCD pts tbl 0x2FDB + extra lives
                                      # kill tier passed in A (1-based),
                                      # NOT read from 0x406F
        0x2467: "ENEMY_ALIVE_SCAN",   # + designates shooter candidates
                                      # walks obj 2..wave_num+1 (b =
                                      # wave_num records from OBJ 2);
                                      # wave 12: 0x407C/0x407F both = 2
        0x24EC: "SHOOTER_PROMOTE",    # closest pair -> active shooter
        0x252F: "AIM_AT_SHIP",        # turret step toward ship angle
        0x25AA: "MUL8X8",             # HL = D * E
        0x25C1: "DIV_SLOPE",          # HL ~= E*256/D (aim ratio)
                                      # D == 0 returns 0 (DE ends 0, no
                                      # carry ever, result = BC = 0)
        0x25EB: "SPAWN_MINE_SHOT",    # mine/shot pools 0x2EF1/0x2F09
                                      # tail 0x2737-0x274E builds shape
                                      # word 0xC000|(0AE7+3*(aim&3F))
        0x2758: "SPAWN_SPECIAL",      # command/death ship pool 22-32;
                                      # spawns AT the parent droid's pos
                                      # (0x27B0 copies posx/posy highs);
                                      # 0x2758-0x277E gate: only when
                                      # the droid is outside the centre
        0x27BF: "OBJ_DESTROY",        # wipe rec w/ 0x2FAC, class sound
        0x283F: "WALL_TRACK_STEER",   # octagon wall-follow steering
        0x2751: "spawn_fail_clear",
        0x2819: "kill_snd_droid",
        0x282F: "kill_snd_cmdship",
        0x2906: "RANDOM_NEXT",        # PRNG: R reg + tick tap -> 0x4073
        0x2A8E: "hs_finalize_filter",
        0x2AB8: "hs_letter_commit",
        0x2ACB: "hs_redraw_initials",
        0x2E3B: "snd_do_send",
        0x29F1: "HISCORE_INSERT",     # shift table, -> mode 4
        0x2A35: "HISCORE_INITIALS",   # spinner letter wheel + filter
        0x2C14: "HISCORE_ROW_FMT",    # dead code, unreferenced
        0x2C36: "CHAR_GLYPH_EMIT",    # ASCII -> JSRL glyph at (HL)
        0x2CF9: "LIVES_ICON_DRAW",    # ships-remaining HUD row
        0x2D56: "VWALL_XFORM_A",      # sign-flip variant builders: XOR
                                      # 0x04 on the byte holding a
                                      # component's sign (SVEC lo=X,
                                      # hi=Y; VCTR w0 hi=Y, w1 hi=X).
                                      # A = X-flip (target is ROM
                                      # SHIP_M*, write no-ops)
        0x2D8A: "VWALL_XFORM_B",      # X+Y flip (180 deg) -> 0x8BF8
        0x2DC8: "VWALL_XFORM_C",      # Y flip -> 0x87F0
        0x2DF6: "HEARTBEAT_SOUND",    # tempo cmds 0x0C-0x0F by
                                      # wave_pace_timer thresholds
        0x240E: "BCD_TO_DIGITS",      # render B BCD bytes as digit JSRLs
        0x291D: "OBJ_SPAWN",          # B=obj#: copy 23-byte template to rec
        0x29B2: "OBJ_KILL",           # B=obj#: clear active bit
        0x29CD: "HISCORE_CHECK",      # score vs 6x7 table 0x43A9, insert
        0x2B6D: "PAGE_SCRIPT_STEP",   # text/BCD page-script interpreter
        0x2C8E: "PLAYER_STATE_SAVE",  # HL=0x4046 P1 / 0x4053 P2
        0x2CBD: "PLAYER_STATE_LOAD",
        0x17D5: "VG_KICK_TITLE",      # kick VG + animate title shimmer:
                                      # rewrites glyph words 0x8080 /
                                      # 0x80A8 (page 2's 4th/5th icon)
                                      # from 0x3F4D/0x3F55 on tick b2-3
        0x1823: "DLIST_RESET",        # wipe live+shadow lists to HALT
        0x1965: "OBJECTS_UPDATE",     # SP=0x2E90 pops obj ptrs, tick-scaled
        0x2AF3: "DRAW_PAGE",          # draw text page (iy+4) via 0x3F15 tbl
        0x2E04: "SOUND_CMD_SEND",
    },
    "ram_symbols": {
        0x4017: "tick_244hz",       # the board's IRQ is 244.140625 Hz
                                    # (12 MHz/49152, schematic omegaM2 -
                                    # see FRAME_PACING_NOTES.md); the
                                    # literature's "250 Hz" is wrong
        0x4018: "tick_prev",
        0x4019: "tick_delta",
        0x401A: "last_kick_tick",
        0x4020: "seconds_ctr",
        0x4021: "timer_seconds",
        0x4022: "wave_pace_timer",  # 1 Hz (starts 0x19 = 25 s); dec in
                                    # MAINLOOP once-per-second block and
                                    # by the 0x1D7C fire cycle; expiry
                                    # sets 0x406A bit7; a shooter_a kill
                                    # reloads 7, or 2 when 0x406A bit0
                                    # is set (0x2310/0x231E)
        0x4023: "wave_start_timer", # 1 Hz; expiry sets 0x406A bit6 +
                                    # wave_flags bit7 ("wave started",
                                    # gates thrust in OBJ_PHYSICS)
        0x4024: "game_mode",
        0x402E: "svc_flags",        # bit6 credit display dirty (coin
                                    # path sets, 0x1788 clears), bit7
                                    # gates SOUND_CMD_SEND
        0x402F: "in1_state", 0x4030: "in1_pressed", 0x4031: "in1_released",
        0x4032: "in2_state", 0x4033: "in2_pressed",
        # 0x4000-0x4016: staged-object workspace. OBJ_STAGE_RUN copies
        # the current 23-byte record here and back, so these are
        # PER-OBJECT fields, not globals (obj_aim especially).
        0x4000: "obj_flags0",   0x4001: "obj_flags1",
        0x4002: "obj_velx",     0x4004: "obj_vely",
        0x4006: "obj_posx",     0x4008: "obj_posy",
        0x400A: "obj_angle",
        0x400C: "obj_lifetime", # b12: countdown -> clear flags0 b7
        0x400E: "obj_fire_timer",# b14: expiry reloads 0x2C-3*wave_num
                                # and raises the fire-request bit
        0x400F: "obj_aim",      # +15 per-object aim byte, not a shared
                                # global: the ship arm 0x1E1B stores its
                                # own heading here, enemies get
                                # AIM_AT_SHIP's result
        0x4010: "obj_radx",     0x4011: "obj_rady",
        0x4013: "obj_timer",
        0x4015: "obj_shape_word",# +21/22 JSRL shape word (0x1E63 arm)
        0x4036: "score_cur",
        0x403E: "cur_player_num",   # active player, 1 or 2: every write
                                    # pairs with the matching p1/p2 save
                                    # block; reads select cocktail input/
                                    # spinner port, screen flip, pages
        0x403F: "player_sel",   # players still owed a mode-4 initials
                                # entry (1/2); dec'd by 0x12AA
        0x4040: "start_credits",# credit price of the pending start
                                # (1/2/4) from START_BTN_CHECK; BCD-
                                # debited once at PLAY_BEGIN 0x0BE3.
                                # NOT the player count
        0x4041: "game_flags",   # b2 cocktail seat2, b3 swap pending,
                                # b4 hiscore-entry gate, b5 deluxe start
                                # (2 credits/player = bonus ship + own
                                # hiscore bank), b6 2-player, b7 game
                                # in progress
        0x4046: "p1_save",      0x4053: "p2_save",  # 13-byte areas
        0x4060: "xlife_thr1",   0x4062: "xlife_thr2", 0x4064: "xlife_thr3",
        0x4066: "lives",
        0x406A: "xlife_flags",  # bits3-5 extra-life latches; bit7 =
                                # pace-timer expired, bit6 = start-timer
                                # expired (see 0x4022/0x4023); bit0 =
                                # late-game latch (0x24DB, on-time >= 3)
                                # selecting the 2/2 shooter-kill re-pace
        0x406B: "wave_flags",   # bit6 = wave complete, bit7 = started
        0x4071: "flame_flicker",# VG_RESTART_FRAME counter; odd -> flame
        0x4073: "random_val",
        0x4075: "settle_track_x",   # prev obj posx/posy highs, beam-
        0x4076: "settle_track_y",   # settle distance tracker (0x1A41)
        0x4079: "prng_cache",   # NOT a cocktail flag
        0x407A: "shooter_a",    0x407B: "shooter_b",  # designated
                                # shooter object numbers (0x1BFE/0x1DAD)
        0x407C: "special_countdown",# reload 0x14-0x23 by DIFFICULTY_
                                # CALC; dec per shooter stage-run; a
                                # shooter_a kill sets 5, or 2 when
                                # 0x406A bit0 is set (0x2314/0x2322)
        0x407D: "obj_index",    0x407E: "obj_class",
        0x407F: "special_armed",# 0x1D18 sets 0xFA when fire cycle runs
        0x409A: "wall_fx",      # 20-entry bounce glow array
        0x4067: "wave_num",     # really the droid count: attract 4,
                                # play starts 6, +2/wave, cap 12
        0x406C: "wave_ctr",
        0x4070: "fire_cooldown",# also the page-scrawl rate limiter
                                # (PAGE_SCRIPT_STEP reloads 10, 25 at
                                # line end -> 25 glyphs/s)
        0x4083: "attract_step",
        0x4084: "page_id",
        0x4085: "page_ptr",
        0x4087: "page_rec_next",# next page-record pointer (DRAW_PAGE
                                # 0x2AF3 / PAGE_SCRIPT_STEP 0x2B6D)
        0x4089: "page_line_cur",
        0x408A: "page_line_glyph",
        0x408B: "page_line_total",
        0x408C: "page_line_len", # glyph bytes in the current line,
                                # set from next-record - 12 (DRAW_PAGE
                                # 0x2AF3 / PAGE_SCRIPT_STEP 0x2B6D)
        0x403A: "time_played_bcd",
        0x4068: "last_sound_cmd",
        0x4069: "sound_ready",
        0x4077: "shooter_state",
        0x408D: "page_script_ptr",
        0x408F: "vlist_cursor",
        0x4094: "initials_ptr",
        0x4096: "initials_cur",
        0x4098: "initials_cnt",
        0x4099: "letter_sel",
        0x406F: "kill_tier",
        # ship record = object 1 at 0x40B2 (+6 posx, +8 posy, +10 angle).
        # posx carries screen-X: the entry builder 0x1EA2 emits
        # LABS X <- posx, LABS Y <- posy.
        0x40B9: "ship_x",       # ship posx high byte (0x40B2 +7)
        0x40BB: "ship_y",       # ship posy high byte (0x40B2 +9)
        0x40BC: "ship_angle",   # = ship record +10, NOT a separate
                                # cell: spinner writes it (0x12F6),
                                # physics + letter wheel read it back
        0x43A9: "hiscore_work",
        0x401B: "coin1_debounce",
        0x401C: "coin2_debounce",
        0x401D: "coin1_pending",
        0x401E: "coin2_pending",
        0x401F: "meter_pulse_t",
        0x4025: "coin1_batch",
        0x4026: "coin2_batch",
        0x4027: "coinage1_ptr",
        0x4029: "coinage2_ptr",
        0x402B: "coin_mode1",
        0x402C: "coin_mode2",
        0x402D: "credits_bcd",
        0x4035: "coin_lines",
        0x4072: "led_shadow",
        0x4082: "slam_latch",
        0x4427: "bookkeep_ctrs",
        0x4481: "dlist_shadow",
        0x8000: "vector_ram",
        0x5C00: "nvram",
        0x5C56: "nvram_credits",    # +0x57 = same byte nibble-swapped
    },
    "ports_in": {
        0x08: "VG_GO (read starts vector generator)",
        0x09: "WATCHDOG",
        0x0B: "VG_STATUS (d7=busy)",
        0x10: "DSW C4",
        0x17: "DSW C6",
        0x11: "IN P1/coin/tilt/test",
        0x12: "IN P2/start",
        0x15: "SPINNER 1",
        0x16: "SPINNER 2",
    },
    "ports_out": {
        0x13: "LEDS/coin counters/flip",
        0x14: "SOUND CMD latch",
        0x0A: "VG RESET",
    },
}

CFG_SOUND = {
    "binfile": "sound_k5.bin",
    "outfile": "omega_sound.asm",
    "code_lo": 0x0000,
    "code_hi": 0x0800,
    "entries": [0x0000, 0x0038, 0x0066],
    "extra_entries": [],
    "word_tables": [],
    "data_defs": [
        (0x024A, 0x02A6, "sndtable",  "command dispatch table: "
                                      "{script ptr, var ptr} per cmd "
                                      "0x00-0x16 (copied to the slot at "
                                      "0x11C0+4*cmd by SND_IRQ_LATCH / "
                                      "op F4)"),
        (0x02A6, 0x0800, "sndscript", "sound scripts, F0-FF bytecode "
                                      "run by SCRIPT_RUN once per slot "
                                      "per 244 Hz tick (semantics in "
                                      "SOUND_NOTES.md)"),
    ],
    # Naming convention matches the main listing: UPPERCASE = verified
    # entry points, lowercase = internal blocks.
    "symbols": {
        0x0000: "SND_RESET",          # clear 0x1000-0x13FF, mirrors to
                                      # 0xFF, all 0x17 slots dead (FFFF)
        0x0038: "SND_IRQ_LATCH",      # cmd latch: 0 -> reset; >=0x17 or
                                      # == running script -> ignored;
                                      # else load slot + zero its vars
        0x0043: "irq_cmd_check",
        0x007C: "irq_var_clear",      # zero var[0..1] of loaded slot
        0x0082: "irq_exit",
        0x0066: "SND_NMI_244HZ",      # SP page must be 0x13 or it spins
                                      # (006F); bumps snd_tick, retn
        0x0087: "SND_MAIN",           # post-reset: both mixers off, ei
        0x0090: "snd_main_loop",      # wait for a new tick, run every
                                      # active slot, flush AY shadows
        0x00A8: "snd_slot_scan",      # bit7 of slot pc high = dead
        0x00BE: "snd_ay_flush",       # shadow 0x1010 vs mirror 0x1030;
                                      # changed regs: 0-15 -> AY1 ports
                                      # 0/1, 16-31 -> AY2 ports 2/3
        0x00D8: "snd_ay2_write",
        0x00E7: "SCRIPT_RUN",         # run one slot for this tick:
                                      # var[0] delay gate, then execute
                                      # ops until delay/end/yield
        0x0108: "script_next_op",
        0x0109: "script_op",          # < 0xF0: shadow-reg op, reg =
                                      # b&0x1F, mode = b&0xE0
        0x0139: "op_reg_load",        # mode 00: reg = nn; ENV_SHAPE
                                      # (reg&0xF==0xD) forces a rewrite
                                      # (mirror = 0xFF) so the envelope
                                      # retriggers on equal value
        0x014A: "op_reg_load16",      # mode 20: reg,reg+1 = nn,nn
        0x0155: "op_reg_or",          # mode 40: reg |= nn
        0x015E: "op_reg_and",         # mode 60: reg &= nn
        0x0167: "op_reg_add",         # mode 80: reg += nn
        0x0170: "op_reg_add16",       # mode A0: reg,reg+1 += nnnn
        0x0181: "script_ctl_op",      # F0-FF dispatch
        0x019F: "op_f1_delay",        # var[0] = nn ticks (4.096 ms each)
        0x01A4: "op_f2_loop_push",    # push {count,pc} frame at
                                      # var[2+4*depth], var[1] = depth
        0x01C3: "op_f3_loop_end",     # dec count: nz -> back to frame
                                      # pc, z -> pop frame
        0x01F3: "op_f4_start",        # start script nn (reload slot
                                      # from table, zero its vars)
        0x0219: "op_f5_kill",         # set bit7 of slot nn's pc high
        0x022A: "op_f6_jump",         # pc = opaddr + 3 + s16 operand
        0x0231: "op_end_script",      # FF (and F0/F7-FE): bit7 -> pc
                                      # high, slot dies on writeback
        0x0236: "script_save_pc",     # skip writeback if the script
                                      # F4-restarted itself (bit7 of
                                      # slot pc high already clear)
    },
    "ram_symbols": {
        0x1000: "snd_tick",           # NMI 244 Hz counter (GBNMI, J4-14)
        0x1001: "snd_tick_prev",      # main loop's last-seen tick
        0x1002: "snd_slot_idx",       # slot being run this pass
        0x1003: "snd_cur_cmd",        # dedup cell for SND_IRQ_LATCH;
                                      # 0x18 = none
        0x1010: "ay_shadow",          # 32 regs: AY1 0-15, AY2 16-31
        0x1030: "ay_mirror",          # last value flushed to the chips
        0x11C0: "script_slots",       # 0x17 x 4: pc lo/hi, var lo/hi;
                                      # pc bit15 set = slot dead.
                                      # var areas 0x1050-0x11B0 (16 B
                                      # per cmd, see dispatch table):
                                      # +0 delay, +1 loop depth,
                                      # +2.. 4-byte loop frames
    },
    "ports_in": {
        0x00: "SOUND LATCH / AY1 read",
    },
    "ports_out": {
        0x00: "AY1 addr", 0x01: "AY1 data",
        0x02: "AY2 addr", 0x03: "AY2 data",
    },
}

# Sound command names (decoded in SOUND_NOTES.md; 05/06 are internal
# harmony voices launched by 04, never sent by the main CPU).
SND_CMD_NAMES = {
    0x00: "all sound off",
    0x01: "player ship explosion",
    0x02: "droid explosion (kill_tier 3)",
    0x03: "command-ship explosion (kill_tier 1)",
    0x04: "wave-complete tune",
    0x05: "tune harmony voice A (internal)",
    0x06: "tune harmony voice B (internal)",
    0x07: "ship fire",
    0x08: "wall hit",
    0x09: "thrust on",
    0x0A: "thrust off",
    0x0B: "heartbeat tempo 1 (slowest)",
    0x0C: "heartbeat tempo 2",
    0x0D: "heartbeat tempo 3",
    0x0E: "heartbeat tempo 4",
    0x0F: "heartbeat tempo 5 (fastest)",
    0x10: "droid explosion (kill_tier 4)",
    0x11: "command-ship explosion (kill_tier != 1)",
    0x12: "coin chime",
    0x13: "tilt/slam alarm",
    0x14: "bonus/extra-life beeping",
    0x15: "droid explosion (default tier)",
    0x16: "self-test sweep",
}

# AY-3-8912 register names for the script decoder (index = reg & 0x0F).
AY_REG_NAMES = [
    "A_PER_LO", "A_PER_HI", "B_PER_LO", "B_PER_HI",
    "C_PER_LO", "C_PER_HI", "NOISE_PER", "MIXER",
    "A_VOL", "B_VOL", "C_VOL", "ENV_PER_LO",
    "ENV_PER_HI", "ENV_SHAPE", "IO_A", "IO_B",
]
# 16-bit names used by the 0x20/0xA0 (reg, reg+1) ops on even bases.
AY_PAIR_NAMES = {0: "A_PERIOD", 2: "B_PERIOD", 4: "C_PERIOD",
                 11: "ENV_PERIOD"}

# ---------------------------------------------------------------------------
# decode tables (per z80.info/decoding.htm)
# ---------------------------------------------------------------------------

R   = ["b", "c", "d", "e", "h", "l", "(hl)", "a"]
RP  = ["bc", "de", "hl", "sp"]
RP2 = ["bc", "de", "hl", "af"]
CC  = ["nz", "z", "nc", "c", "po", "pe", "p", "m"]
ALU = ["add a,", "adc a,", "sub ", "sbc a,", "and ", "xor ", "or ", "cp "]
ROT = ["rlc", "rrc", "rl", "rr", "sla", "sra", "sll", "srl"]
IMM = ["0", "0/1", "1", "2", "0", "0/1", "1", "2"]
X07 = ["rlca", "rrca", "rla", "rra", "daa", "cpl", "scf", "ccf"]
BLI = {(4,0):"ldi",(4,1):"cpi",(4,2):"ini",(4,3):"outi",
       (5,0):"ldd",(5,1):"cpd",(5,2):"ind",(5,3):"outd",
       (6,0):"ldir",(6,1):"cpir",(6,2):"inir",(6,3):"otir",
       (7,0):"lddr",(7,1):"cpdr",(7,2):"indr",(7,3):"otdr"}


class Insn:
    def __init__(self):
        self.size = 1
        self.text = "??"
        self.targets = []      # absolute jump/call targets (code)
        self.stop = False      # unconditional flow break
        self.memrefs = []      # absolute 16-bit memory operands
        self.ports = []        # (port, 'r'|'w')
        self.ldimm = None      # (reg, value) for ld rp,nn


def s8(v):
    return v - 256 if v >= 128 else v


class Disasm:
    def __init__(self, mem):
        self.mem = mem

    def rb(self, a):
        return self.mem[a & 0xFFFF]

    def rw(self, a):
        return self.rb(a) | (self.rb(a + 1) << 8)

    # ---- index-register renaming helpers -----------------------------------
    def rname(self, idx, ix, disp):
        # register name with DD/FD renaming; (hl) becomes (ix+d)
        if ix is None:
            return R[idx]
        if idx == 6:
            return "(%s%+d)" % (ix, disp)
        if idx == 4:
            return ix + "h"
        if idx == 5:
            return ix + "l"
        return R[idx]

    def rname_plain(self, idx, ix):
        # renaming for instructions with no memory operand
        if ix is None:
            return R[idx]
        if idx == 4:
            return ix + "h"
        if idx == 5:
            return ix + "l"
        return R[idx]

    def hlname(self, ix):
        return ix if ix else "hl"

    # ---- main decode -------------------------------------------------------
    def decode(self, a):
        ins = Insn()
        p = a
        ix = None
        op = self.rb(p); p += 1

        while op in (0xDD, 0xFD):
            ix = "ix" if op == 0xDD else "iy"
            op = self.rb(p); p += 1
            if op in (0xDD, 0xFD):
                # stacked prefixes: previous one is a no-op
                continue

        if op == 0xCB:
            self.decode_cb(ins, p, ix, a)
            return ins
        if op == 0xED:
            self.decode_ed(ins, p, a)
            return ins

        x, y, z = op >> 6, (op >> 3) & 7, op & 7
        q, pp = y & 1, y >> 1

        if x == 0:
            if z == 0:
                if y == 0:
                    ins.text = "nop"
                elif y == 1:
                    ins.text = "ex af,af'"
                elif y == 2:
                    d = s8(self.rb(p)); p += 1
                    t = (p + d) & 0xFFFF
                    ins.text = "djnz $%04X" % t
                    ins.targets.append(t)
                elif y == 3:
                    d = s8(self.rb(p)); p += 1
                    t = (p + d) & 0xFFFF
                    ins.text = "jr $%04X" % t
                    ins.targets.append(t)
                    ins.stop = True
                else:
                    d = s8(self.rb(p)); p += 1
                    t = (p + d) & 0xFFFF
                    ins.text = "jr %s,$%04X" % (CC[y - 4], t)
                    ins.targets.append(t)
            elif z == 1:
                if q == 0:
                    nn = self.rw(p); p += 2
                    rp = RP[pp] if pp != 2 else self.hlname(ix)
                    ins.text = "ld %s,$%04X" % (rp, nn)
                    ins.memrefs.append(nn)
                    ins.ldimm = (rp, nn)
                else:
                    rp = RP[pp] if pp != 2 else self.hlname(ix)
                    ins.text = "add %s,%s" % (self.hlname(ix), rp)
            elif z == 2:
                if q == 0:
                    if pp == 0:
                        ins.text = "ld (bc),a"
                    elif pp == 1:
                        ins.text = "ld (de),a"
                    elif pp == 2:
                        nn = self.rw(p); p += 2
                        ins.text = "ld ($%04X),%s" % (nn, self.hlname(ix))
                        ins.memrefs.append(nn)
                    else:
                        nn = self.rw(p); p += 2
                        ins.text = "ld ($%04X),a" % nn
                        ins.memrefs.append(nn)
                else:
                    if pp == 0:
                        ins.text = "ld a,(bc)"
                    elif pp == 1:
                        ins.text = "ld a,(de)"
                    elif pp == 2:
                        nn = self.rw(p); p += 2
                        ins.text = "ld %s,($%04X)" % (self.hlname(ix), nn)
                        ins.memrefs.append(nn)
                    else:
                        nn = self.rw(p); p += 2
                        ins.text = "ld a,($%04X)" % nn
                        ins.memrefs.append(nn)
            elif z == 3:
                rp = RP[pp] if pp != 2 else self.hlname(ix)
                ins.text = ("inc %s" if q == 0 else "dec %s") % rp
            elif z in (4, 5):
                mnem = "inc" if z == 4 else "dec"
                if ix and y == 6:
                    d = s8(self.rb(p)); p += 1
                    ins.text = "%s (%s%+d)" % (mnem, ix, d)
                else:
                    ins.text = "%s %s" % (mnem, self.rname_plain(y, ix))
            elif z == 6:
                if ix and y == 6:
                    d = s8(self.rb(p)); p += 1
                    n = self.rb(p); p += 1
                    ins.text = "ld (%s%+d),$%02X" % (ix, d, n)
                else:
                    n = self.rb(p); p += 1
                    ins.text = "ld %s,$%02X" % (self.rname_plain(y, ix), n)
            else:
                ins.text = X07[y]
        elif x == 1:
            if y == 6 and z == 6:
                ins.text = "halt"
            else:
                if ix and (y == 6 or z == 6):
                    d = s8(self.rb(p)); p += 1
                    dst = "(%s%+d)" % (ix, d) if y == 6 else R[y]
                    src = "(%s%+d)" % (ix, d) if z == 6 else R[z]
                    ins.text = "ld %s,%s" % (dst, src)
                else:
                    ins.text = "ld %s,%s" % (self.rname_plain(y, ix),
                                             self.rname_plain(z, ix))
        elif x == 2:
            if ix and z == 6:
                d = s8(self.rb(p)); p += 1
                ins.text = "%s(%s%+d)" % (ALU[y], ix, d)
            else:
                ins.text = "%s%s" % (ALU[y], self.rname_plain(z, ix))
        else:  # x == 3
            if z == 0:
                ins.text = "ret %s" % CC[y]
            elif z == 1:
                if q == 0:
                    rp = RP2[pp] if pp != 2 else self.hlname(ix)
                    ins.text = "pop %s" % rp
                else:
                    if pp == 0:
                        ins.text = "ret"
                        ins.stop = True
                    elif pp == 1:
                        ins.text = "exx"
                    elif pp == 2:
                        ins.text = "jp (%s)" % self.hlname(ix)
                        ins.stop = True
                    else:
                        ins.text = "ld sp,%s" % self.hlname(ix)
            elif z == 2:
                nn = self.rw(p); p += 2
                ins.text = "jp %s,$%04X" % (CC[y], nn)
                ins.targets.append(nn)
            elif z == 3:
                if y == 0:
                    nn = self.rw(p); p += 2
                    ins.text = "jp $%04X" % nn
                    ins.targets.append(nn)
                    ins.stop = True
                elif y == 2:
                    n = self.rb(p); p += 1
                    ins.text = "out ($%02X),a" % n
                    ins.ports.append((n, "w"))
                elif y == 3:
                    n = self.rb(p); p += 1
                    ins.text = "in a,($%02X)" % n
                    ins.ports.append((n, "r"))
                elif y == 4:
                    ins.text = "ex (sp),%s" % self.hlname(ix)
                elif y == 5:
                    ins.text = "ex de,hl"
                elif y == 6:
                    ins.text = "di"
                else:
                    ins.text = "ei"
            elif z == 4:
                nn = self.rw(p); p += 2
                ins.text = "call %s,$%04X" % (CC[y], nn)
                ins.targets.append(nn)
            elif z == 5:
                if q == 0:
                    rp = RP2[pp] if pp != 2 else self.hlname(ix)
                    ins.text = "push %s" % rp
                else:
                    nn = self.rw(p); p += 2
                    ins.text = "call $%04X" % nn
                    ins.targets.append(nn)
            elif z == 6:
                n = self.rb(p); p += 1
                ins.text = "%s$%02X" % (ALU[y], n)
            else:
                t = y * 8
                ins.text = "rst $%02X" % t
                ins.targets.append(t)

        ins.size = p - a
        return ins

    def decode_cb(self, ins, p, ix, start):
        if ix:
            d = s8(self.rb(p)); p += 1
            op = self.rb(p); p += 1
            x, y, z = op >> 6, (op >> 3) & 7, op & 7
            loc = "(%s%+d)" % (ix, d)
            extra = "" if z == 6 else ",%s" % R[z]  # undocumented copy-to-reg
            if x == 0:
                ins.text = "%s %s%s" % (ROT[y], loc, extra)
            elif x == 1:
                ins.text = "bit %d,%s" % (y, loc)
            elif x == 2:
                ins.text = "res %d,%s%s" % (y, loc, extra)
            else:
                ins.text = "set %d,%s%s" % (y, loc, extra)
        else:
            op = self.rb(p); p += 1
            x, y, z = op >> 6, (op >> 3) & 7, op & 7
            if x == 0:
                ins.text = "%s %s" % (ROT[y], R[z])
            elif x == 1:
                ins.text = "bit %d,%s" % (y, R[z])
            elif x == 2:
                ins.text = "res %d,%s" % (y, R[z])
            else:
                ins.text = "set %d,%s" % (y, R[z])
        ins.size = p - start

    def decode_ed(self, ins, p, start):
        op = self.rb(p); p += 1
        x, y, z = op >> 6, (op >> 3) & 7, op & 7
        q, pp = y & 1, y >> 1
        if x == 1:
            if z == 0:
                ins.text = "in (c)" if y == 6 else "in %s,(c)" % R[y]
                ins.ports.append((None, "r"))
            elif z == 1:
                ins.text = "out (c),0" if y == 6 else "out (c),%s" % R[y]
                ins.ports.append((None, "w"))
            elif z == 2:
                ins.text = ("sbc hl,%s" if q == 0 else "adc hl,%s") % RP[pp]
            elif z == 3:
                nn = self.rw(p); p += 2
                if q == 0:
                    ins.text = "ld ($%04X),%s" % (nn, RP[pp])
                else:
                    ins.text = "ld %s,($%04X)" % (RP[pp], nn)
                ins.memrefs.append(nn)
            elif z == 4:
                ins.text = "neg"
            elif z == 5:
                ins.text = "reti" if y == 1 else "retn"
                ins.stop = True
            elif z == 6:
                ins.text = "im %s" % IMM[y]
            else:
                ins.text = ["ld i,a", "ld r,a", "ld a,i", "ld a,r",
                            "rrd", "rld", "nop*", "nop*"][y]
        elif x == 2 and z <= 3 and y >= 4:
            ins.text = BLI[(y, z)]
        else:
            ins.text = "db $ED,$%02X ; invalid" % op
        ins.size = p - start


# ---------------------------------------------------------------------------
# tracer
# ---------------------------------------------------------------------------

def trace(cfg):
    mem = bytearray(open(cfg["binfile"], "rb").read())
    if len(mem) < 0x10000:
        mem += bytes(0x10000 - len(mem))
    dis = Disasm(mem)

    lo, hi = cfg["code_lo"], cfg["code_hi"]

    # word tables: mark bytes as claimed, harvest entries
    table_bytes = set()
    table_targets = []
    for (ts, te, is_code) in cfg["word_tables"]:
        for a in range(ts, te):
            table_bytes.add(a)
        if is_code:
            for a in range(ts, te - 1, 2):
                table_targets.append(dis.rw(a))

    kind = {}          # addr -> 'op' (instruction start) | 'in' (inside)
    insns = {}         # addr -> Insn
    labels = set()
    callers = {}       # target -> count, to distinguish subs from branches

    work = list(cfg["entries"]) + list(cfg["extra_entries"]) + table_targets
    for w in work:
        labels.add(w)

    while work:
        a = work.pop()
        if a < lo or a >= hi:
            continue
        while lo <= a < hi and a not in kind and a not in table_bytes:
            ins = dis.decode(a)
            kind[a] = "op"
            insns[a] = ins
            for i in range(1, ins.size):
                kind[a + i] = "in"
            for t in ins.targets:
                labels.add(t)
                callers[t] = callers.get(t, 0) + 1
                if lo <= t < hi and t not in kind:
                    work.append(t)
            # continuation linkage: ld ix/iy,<rom addr> immediately followed
            # by an unconditional jump is a code pointer (the jumped-to
            # routine resumes the caller with jp (ix) / jp (iy)).  A bare
            # ld ix,nn without the jump is a data-table pointer -- ignore.
            if ins.ldimm and ins.ldimm[0] in ("ix", "iy"):
                t = ins.ldimm[1]
                nxt = dis.decode(a + ins.size)
                is_link = nxt.stop and nxt.text.startswith(("jp ", "jr "))
                if lo <= t < hi and is_link:
                    labels.add(t)
                    if t not in kind:
                        work.append(t)
            if ins.stop:
                break
            a += ins.size

    return mem, dis, kind, insns, labels


# ---------------------------------------------------------------------------
# output
# ---------------------------------------------------------------------------

def symbolize(cfg, addr):
    if addr in cfg["symbols"]:
        return cfg["symbols"][addr]
    return None


def annotate(cfg, ins):
    notes = []
    for t in ins.targets:
        s = symbolize(cfg, t)
        if s:
            notes.append("-> %s" % s)
    for m in ins.memrefs:
        if m in cfg["ram_symbols"]:
            notes.append(cfg["ram_symbols"][m])
        elif m in cfg["symbols"]:
            notes.append(cfg["symbols"][m])
    for (port, rw) in ins.ports:
        if port is None:
            continue
        tab = cfg["ports_in"] if rw == "r" else cfg["ports_out"]
        if port in tab:
            notes.append(tab[port])
    return " ; " + ", ".join(notes) if notes else ""


def sym_or_hex(cfg, v):
    return cfg["ram_symbols"].get(v) or cfg["symbols"].get(v) or "$%04X" % v


def render_snd_scripts(cfg, mem, dis, out, start, end):
    """Decode the sound board's script bytecode (interpreter at
    SCRIPT_RUN 0x00E7; opcode semantics transcribed from the code):
      < F0        shadow-register op: reg = b & 0x1F (0-15 AY1,
                  16-31 AY2), mode = b & 0xE0:
                  00 load / 20 load 16-bit / 40 or / 60 and /
                  80 add / A0 add 16-bit / C0,E0 yield this tick
      F1 nn       delay nn ticks (244.14 Hz, 4.096 ms each)
      F2 lo hi    repeat block N times (frame in the slot's var area)
      F3          loop end (dec count, jump back while nonzero)
      F4 nn       start script nn (reload its slot from the table)
      F5 nn       kill script nn
      F6 lo hi    pc = opcode addr + 3 + signed16 (backwards = loop)
      FF          end of script (F0/F7-FE fall into the same arm)
    Script boundaries come from the dispatch table at 0x024A."""
    starts = {}
    for cmd in range(0x17):
        starts[dis.rw(0x024A + cmd * 4)] = (cmd,
                                            dis.rw(0x024A + cmd * 4 + 2))
    a = start
    while a < end:
        if a in starts:
            cmd, va = starts[a]
            out.append("")
            out.append("; ==== script %02X: %s  (vars $%04X) ===="
                       % (cmd, SND_CMD_NAMES.get(cmd, "?"), va))
        b = mem[a]
        n, txt = 1, None
        if b >= 0xF0:
            if b == 0xF1 and a + 1 < end:
                n, txt = 2, ("delay %d ticks (%d ms)"
                             % (mem[a + 1], mem[a + 1] * 4))
            elif b == 0xF2 and a + 2 < end:
                n, txt = 3, ("repeat %d times {"
                             % (mem[a + 1] | (mem[a + 2] << 8)))
            elif b == 0xF3:
                txt = "} loop back while count remains"
            elif b == 0xF4 and a + 1 < end:
                n, txt = 2, ("start script %02X (%s)"
                             % (mem[a + 1],
                                SND_CMD_NAMES.get(mem[a + 1], "?")))
            elif b == 0xF5 and a + 1 < end:
                n, txt = 2, ("kill script %02X (%s)"
                             % (mem[a + 1],
                                SND_CMD_NAMES.get(mem[a + 1], "?")))
            elif b == 0xF6 and a + 2 < end:
                tgt = (a + 3 + (mem[a + 1] | (mem[a + 2] << 8))) & 0xFFFF
                n, txt = 3, ("jump -> $%04X%s"
                             % (tgt, " (loop)" if tgt <= a else ""))
            elif b == 0xFF:
                txt = "end of script"
            elif b in (0xF0,) or 0xF7 <= b <= 0xFE:
                txt = "end of script (op %02X falls into the end arm)" % b
        else:
            reg, mode = b & 0x1F, b & 0xE0
            chip = "AY1_" if reg < 16 else "AY2_"
            name = chip + AY_REG_NAMES[reg & 0x0F]
            pair = AY_PAIR_NAMES.get(reg & 0x0F)
            pname = chip + pair if pair else name + "/+1"
            if mode == 0x00 and a + 1 < end:
                n, txt = 2, "%s = $%02X" % (name, mem[a + 1])
                if (reg & 0x0F) == 0x0D:
                    txt += "  (mirror forced: retriggers envelope)"
            elif mode == 0x20 and a + 2 < end:
                n, txt = 3, "%s = $%04X" % (pname,
                                            mem[a + 1] | (mem[a + 2] << 8))
            elif mode == 0x40 and a + 1 < end:
                n, txt = 2, "%s |= $%02X" % (name, mem[a + 1])
            elif mode == 0x60 and a + 1 < end:
                n, txt = 2, "%s &= $%02X" % (name, mem[a + 1])
            elif mode == 0x80 and a + 1 < end:
                n, txt = 2, "%s += $%02X" % (name, mem[a + 1])
            elif mode == 0xA0 and a + 2 < end:
                n, txt = 3, "%s += $%04X" % (pname,
                                             mem[a + 1] | (mem[a + 2] << 8))
            elif mode in (0xC0, 0xE0):
                txt = "yield this tick (mode %02X, unused)" % mode
        if txt is None:
            hexs = " ".join("%02X" % c for c in mem[a:end])
            out.append("%04X:  %-12s (truncated: operand crosses the end "
                       "of ROM; the next fetch at $0800 is unmapped)"
                       % (a, hexs))
            break
        hexs = " ".join("%02X" % c for c in mem[a:a + n])
        out.append("%04X:  %-12s %s" % (a, hexs, txt))
        a += n


def render_data(cfg, mem, dis, out, start, end, dtype, comment):
    out.append("")
    clines = comment.split("\n")
    out.append("; ---- %04X-%04X: %s ----" % (start, end - 1, clines[0]))
    for extra in clines[1:]:
        out.append("; %s" % extra)
    a = start
    if dtype == "dw":
        while a < end:
            n = min(8, (end - a) // 2)
            ws = " ".join("%04X" % dis.rw(a + i * 2) for i in range(n))
            out.append("%04X:  dw  %s" % (a, ws))
            a += n * 2 if n else 1
            if n == 0:
                out.append("%04X:  db  %02X" % (a, mem[a])); a += 1
    elif dtype == "ascii":
        while a < end:
            n = min(32, end - a)
            chunk = mem[a:a + n]
            txt = "".join(chr(c) if 32 <= c < 127 else "\\x%02X" % c
                          for c in chunk)
            out.append('%04X:  text "%s"' % (a, txt))
            a += n
    elif dtype == "objtable":
        idx = 1
        while a + 6 <= end:
            rec, tpl, slot = dis.rw(a), dis.rw(a + 2), dis.rw(a + 4)
            out.append("%04X:  OBJ %2d: record=%s  template=$%04X  "
                       "dlist_entry=$%04X" % (a, idx, sym_or_hex(cfg, rec),
                                              tpl, slot))
            a += 6; idx += 1
    elif dtype == "rec23":
        while a < end:
            n = min(23, end - a)
            hx = " ".join("%02X" % c for c in mem[a:a + n])
            out.append("%04X:  tmpl  %s" % (a, hx))
            a += n
    elif dtype == "bytepairs":
        while a < end:
            n = min(16, end - a)
            ps = " ".join("(%d,%d)" % (mem[a + i], mem[a + i + 1])
                          for i in range(0, n, 2))
            out.append("%04X:  pairs %s" % (a, ps))
            a += n
    elif dtype == "bcdwords":
        vals = []
        while a + 2 <= end:
            vals.append("%02X%02X" % (mem[a + 1], mem[a]))
            a += 2
        out.append("%04X:  bcd  %s" % (start, " ".join(vals)))
    elif dtype == "hiscore7":
        while a + 7 <= end:
            score = "".join("%02X" % mem[a + 3 - i] for i in range(4))
            ini = "".join(chr(c) if 32 <= c < 127 else "." for c in
                          mem[a + 4:a + 7])
            out.append('%04X:  hiscore %s "%s"' % (a, score, ini))
            a += 7
        while a < end:
            out.append("%04X:  db  %02X" % (a, mem[a])); a += 1
    elif dtype == "script":
        render_script(cfg, mem, dis, out, start, end)
        a = end
    elif dtype == "sndtable":
        cmd = 0
        while a + 4 <= end:
            script, var = dis.rw(a), dis.rw(a + 2)
            out.append("%04X:  cmd %02X: script $%04X  vars $%04X   ; %s"
                       % (a, cmd, script, var,
                          SND_CMD_NAMES.get(cmd, "?")))
            a += 4; cmd += 1
    elif dtype == "sndscript":
        render_snd_scripts(cfg, mem, dis, out, start, end)
        a = end
    else:  # db
        while a < end:
            n = min(16, end - a)
            chunk = mem[a:a + n]
            hexs = " ".join("%02X" % c for c in chunk)
            asc = "".join(chr(c) if 32 <= c < 127 else "." for c in chunk)
            out.append("%04X:  db  %-47s ; %s" % (a, hexs, asc))
            a += n
    return end


def script_hdr_ok(dis, pos, lo, hi):
    nxt, dest = dis.rw(pos), dis.rw(pos + 2)
    return ((pos < nxt < hi or nxt == 0)
            and (0x4400 <= dest < 0xA000))


def render_script(cfg, mem, dis, out, lo, hi):
    """Attract/message page scripts: chained segments of
    [next(2)][dlist dest(2)][8-byte DVG header][payload], where payload
    is ASCII text plus escapes: 0x80|N ptr16 = draw N BCD bytes from
    ptr, 0xC0|N ptr16 = draw N chars from ptr ('<' in text = line end).
    Page-table entries (0x3F15) point at a 1-byte flag preceding the
    first segment."""
    a = lo
    while a < hi:
        if a + 12 <= hi and script_hdr_ok(dis, a, lo, hi):
            nxt, dest = dis.rw(a), dis.rw(a + 2)
            hdr = " ".join("%04X" % dis.rw(a + 4 + i * 2) for i in range(4))
            out.append("")
            out.append("%04X:  SEG next=%s dest=$%04X hdr[%s]"
                       % (a, "$%04X" % nxt if nxt else "END", dest, hdr))
            pay_end = nxt if (a < nxt <= hi) else hi
            p = a + 12
            # for END segments, stop at the next parsable header
            if pay_end == hi and nxt == 0:
                q = a + 12
                while q < hi - 12:
                    if script_hdr_ok(dis, q, lo, hi):
                        break
                    q += 1
                pay_end = q if q < hi - 12 else hi
            while p < pay_end:
                b = mem[p]
                if 0x20 <= b < 0x7F:
                    q = p
                    while q < pay_end and 0x20 <= mem[q] < 0x7F:
                        q += 1
                    txt = "".join(chr(c) for c in mem[p:q])
                    out.append('%04X:    text "%s"' % (p, txt))
                    p = q
                elif (b & 0xC0) == 0x80 and p + 3 <= pay_end:
                    out.append("%04X:    num  %d BCD bytes from %s"
                               % (p, b & 0x3F,
                                  sym_or_hex(cfg, dis.rw(p + 1))))
                    p += 3
                elif (b & 0xC0) == 0xC0 and p + 3 <= pay_end:
                    out.append("%04X:    str  %d chars from %s"
                               % (p, b & 0x3F,
                                  sym_or_hex(cfg, dis.rw(p + 1))))
                    p += 3
                else:
                    q = p
                    while q < pay_end and not (0x20 <= mem[q] < 0x7F):
                        if (mem[q] & 0xC0) in (0x80, 0xC0) and q + 3 <= pay_end:
                            break
                        q += 1
                    hexs = " ".join("%02X" % c for c in mem[p:q])
                    out.append("%04X:    db   %s" % (p, hexs))
                    p = q if q > p else p + 1
            a = pay_end
        else:
            out.append("%04X:  db  %02X ; page-entry flag/pad" % (a, mem[a]))
            a += 1


def emit(cfg, mem, dis, kind, insns, labels):
    lo, hi = cfg["code_lo"], cfg["code_hi"]
    out = []

    # Resolve every label to a name: explicit symbol, or auto-derived
    # "<owning function>_<addr>" so locals identify their parent.
    sym_addrs = sorted(a for a in cfg["symbols"] if lo <= a < hi)
    names = {}
    for a in set(labels) | set(cfg["symbols"]):
        if a in cfg["symbols"]:
            names[a] = cfg["symbols"][a]
        else:
            import bisect
            i = bisect.bisect_right(sym_addrs, a) - 1
            if i >= 0:
                names[a] = "%s_%04X" % (cfg["symbols"][sym_addrs[i]], a)
            else:
                names[a] = "L%04X" % a
    out.append("; %s  traced disassembly (z80trace.py)" % cfg["binfile"])
    out.append(";")

    table_ranges = {ts: (te, is_code) for (ts, te, is_code) in cfg["word_tables"]}
    data_ranges = {ds: (de, dt, dc) for (ds, de, dt, dc) in cfg["data_defs"]}

    a = lo
    gap_start = None
    gaps = []

    def flush_gap(gs, ge):
        gaps.append((gs, ge))
        for b in range(gs, ge, 16):
            chunk = mem[b:min(b + 16, ge)]
            hexs = " ".join("%02X" % c for c in chunk)
            asc = "".join(chr(c) if 32 <= c < 127 else "." for c in chunk)
            out.append("%04X:  db  %-47s ; %s" % (b, hexs, asc))

    while a < hi:
        if a in data_ranges and kind.get(a) is None:
            if gap_start is not None:
                flush_gap(gap_start, a)
                gap_start = None
            de_, dt_, dc_ = data_ranges[a]
            a = render_data(cfg, mem, dis, out, a, de_, dt_, dc_)
            continue
        if a in table_ranges:
            if gap_start is not None:
                flush_gap(gap_start, a)
                gap_start = None
            te, is_code = table_ranges[a]
            if a in labels or a in cfg["symbols"]:
                out.append("%s:" % names.get(a, "L%04X" % a))
            for b in range(a, te - 1, 2):
                t = dis.rw(b)
                name = names.get(t, "$%04X" % t)
                out.append("%04X:  dw  %s" % (b, name))
            a = te
            continue
        if kind.get(a) == "op":
            if gap_start is not None:
                flush_gap(gap_start, a)
                gap_start = None
            ins = insns[a]
            if a in labels or a in cfg["symbols"]:
                out.append("")
                out.append("%s:" % names.get(a, "L%04X" % a))
            raw = " ".join("%02X" % mem[a + i] for i in range(ins.size))
            text = ins.text
            # rewrite $XXXX targets with symbols where known
            for t in ins.targets:
                s = names.get(t)
                if s:
                    text = text.replace("$%04X" % t, s)
            for m in ins.memrefs:
                if m in cfg["ram_symbols"]:
                    text = text.replace("$%04X" % m, cfg["ram_symbols"][m])
            out.append("%04X:  %-12s %-28s%s" % (a, raw, text, annotate(cfg, ins)))
            a += ins.size
        elif kind.get(a) == "in":
            a += 1  # shouldn't happen at top level
        else:
            if gap_start is None:
                gap_start = a
            a += 1
    if gap_start is not None:
        flush_gap(gap_start, hi)

    with open(cfg["outfile"], "w") as f:
        f.write("\n".join(out) + "\n")

    # coverage report
    ncode = sum(1 for v in kind.values())
    total = hi - lo
    print("code bytes: %d / %d (%.1f%%)" % (ncode, total, 100.0 * ncode / total))
    big = [(gs, ge) for (gs, ge) in gaps if ge - gs >= 4]
    print("gaps >= 4 bytes: %d" % len(big))
    for gs, ge in big:
        print("  %04X-%04X (%d bytes)" % (gs, ge - 1, ge - gs))


def main():
    which = sys.argv[1] if len(sys.argv) > 1 else "main"
    cfg = CFG_MAIN if which == "main" else CFG_SOUND
    mem, dis, kind, insns, labels = trace(cfg)
    emit(cfg, mem, dis, kind, insns, labels)


if __name__ == "__main__":
    main()
