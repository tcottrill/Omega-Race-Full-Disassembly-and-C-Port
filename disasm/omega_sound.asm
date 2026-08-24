; sound_k5.bin  traced disassembly (z80trace.py)
;

SND_RESET:
0000:  F3           di                          
0001:  ED 56        im 1                        
0003:  31 FC 13     ld sp,$13FC                 
0006:  FD 21 00 10  ld iy,snd_tick               ; snd_tick
000A:  01 FF 03     ld bc,$03FF                 
000D:  11 01 10     ld de,snd_tick_prev          ; snd_tick_prev
0010:  21 00 10     ld hl,snd_tick               ; snd_tick
0013:  36 00        ld (hl),$00                 
0015:  ED B0        ldir                        
0017:  01 1F 00     ld bc,$001F                 
001A:  11 31 10     ld de,$1031                 
001D:  21 30 10     ld hl,ay_mirror              ; ay_mirror
0020:  36 FF        ld (hl),$FF                 
0022:  ED B0        ldir                        
0024:  21 FF FF     ld hl,$FFFF                 
0027:  22 C0 11     ld (script_slots),hl         ; script_slots
002A:  01 5A 00     ld bc,$005A                 
002D:  11 C2 11     ld de,$11C2                 
0030:  21 C0 11     ld hl,script_slots           ; script_slots
0033:  ED B0        ldir                        
0035:  18 50        jr SND_MAIN                  ; -> SND_MAIN
0037:  db  4E                                              ; N

SND_IRQ_LATCH:
0038:  08           ex af,af'                   
0039:  D9           exx                         
003A:  DB 00        in a,($00)                   ; SOUND LATCH / AY1 read
003C:  E6 7F        and $7F                     
003E:  20 03        jr nz,irq_cmd_check          ; -> irq_cmd_check
0040:  C3 00 00     jp SND_RESET                 ; -> SND_RESET

irq_cmd_check:
0043:  FE 17        cp $17                      
0045:  30 3B        jr nc,irq_exit               ; -> irq_exit
0047:  FD BE 03     cp (iy+3)                   
004A:  28 36        jr z,irq_exit                ; -> irq_exit
004C:  4F           ld c,a                      
004D:  CB 21        sla c                       
004F:  06 00        ld b,$00                    
0051:  21 C0 11     ld hl,script_slots           ; script_slots
0054:  09           add hl,bc                   
0055:  09           add hl,bc                   
0056:  EB           ex de,hl                    
0057:  21 4A 02     ld hl,$024A                 
005A:  09           add hl,bc                   
005B:  09           add hl,bc                   
005C:  01 04 00     ld bc,$0004                 
005F:  ED B0        ldir                        
0061:  2B           dec hl                      
0062:  56           ld d,(hl)                   
0063:  2B           dec hl                      
0064:  18 16        jr irq_var_clear             ; -> irq_var_clear

SND_NMI_244HZ:
0066:  F5           push af                     
0067:  E5           push hl                     
0068:  21 00 00     ld hl,$0000                  ; SND_RESET
006B:  39           add hl,sp                   
006C:  3E 13        ld a,$13                    
006E:  BC           cp h                        

SND_NMI_244HZ_006F:
006F:  20 FE        jr nz,SND_NMI_244HZ_006F    
0071:  E1           pop hl                      
0072:  3A 00 10     ld a,(snd_tick)              ; snd_tick
0075:  3C           inc a                       
0076:  32 00 10     ld (snd_tick),a              ; snd_tick
0079:  F1           pop af                      
007A:  ED 45        retn                        

irq_var_clear:
007C:  5E           ld e,(hl)                   
007D:  3E 00        ld a,$00                    
007F:  12           ld (de),a                   
0080:  13           inc de                      
0081:  12           ld (de),a                   

irq_exit:
0082:  D9           exx                         
0083:  08           ex af,af'                   
0084:  FB           ei                          
0085:  ED 4D        reti                        

SND_MAIN:
0087:  FD 36 17 3F  ld (iy+23),$3F              
008B:  FD 36 27 3F  ld (iy+39),$3F              
008F:  FB           ei                          

snd_main_loop:
0090:  3A 00 10     ld a,(snd_tick)              ; snd_tick
0093:  FD BE 01     cp (iy+1)                   
0096:  28 F8        jr z,snd_main_loop           ; -> snd_main_loop
0098:  32 01 10     ld (snd_tick_prev),a         ; snd_tick_prev
009B:  06 17        ld b,$17                    
009D:  21 C1 11     ld hl,$11C1                 
00A0:  FD 36 03 18  ld (iy+3),$18               
00A4:  FD 36 02 00  ld (iy+2),$00               

snd_slot_scan:
00A8:  CB 7E        bit 7,(hl)                  
00AA:  CC E7 00     call z,SCRIPT_RUN            ; -> SCRIPT_RUN
00AD:  23           inc hl                      
00AE:  23           inc hl                      
00AF:  23           inc hl                      
00B0:  23           inc hl                      
00B1:  FD 34 02     inc (iy+2)                  
00B4:  10 F2        djnz snd_slot_scan           ; -> snd_slot_scan
00B6:  06 20        ld b,$20                    
00B8:  0E 00        ld c,$00                    
00BA:  DD 21 10 10  ld ix,ay_shadow              ; ay_shadow

snd_ay_flush:
00BE:  DD 7E 00     ld a,(ix+0)                 
00C1:  DD BE 20     cp (ix+32)                  
00C4:  28 19        jr z,snd_ay2_write_00DF     
00C6:  DD 77 20     ld (ix+32),a                
00C9:  79           ld a,c                      
00CA:  E6 0F        and $0F                     
00CC:  B9           cp c                        
00CD:  20 09        jr nz,snd_ay2_write          ; -> snd_ay2_write
00CF:  D3 00        out ($00),a                  ; AY1 addr
00D1:  DD 7E 20     ld a,(ix+32)                
00D4:  D3 01        out ($01),a                  ; AY1 data
00D6:  18 07        jr snd_ay2_write_00DF       

snd_ay2_write:
00D8:  D3 02        out ($02),a                  ; AY2 addr
00DA:  DD 7E 20     ld a,(ix+32)                
00DD:  D3 03        out ($03),a                  ; AY2 data

snd_ay2_write_00DF:
00DF:  0C           inc c                       
00E0:  DD 23        inc ix                      
00E2:  10 DA        djnz snd_ay_flush            ; -> snd_ay_flush
00E4:  C3 90 00     jp snd_main_loop             ; -> snd_main_loop

SCRIPT_RUN:
00E7:  C5           push bc                     
00E8:  E5           push hl                     
00E9:  F3           di                          
00EA:  3A 02 10     ld a,(snd_slot_idx)          ; snd_slot_idx
00ED:  32 03 10     ld (snd_cur_cmd),a           ; snd_cur_cmd
00F0:  2B           dec hl                      
00F1:  5E           ld e,(hl)                   
00F2:  23           inc hl                      
00F3:  56           ld d,(hl)                   
00F4:  CB FE        set 7,(hl)                  
00F6:  E5           push hl                     
00F7:  23           inc hl                      
00F8:  4E           ld c,(hl)                   
00F9:  23           inc hl                      
00FA:  46           ld b,(hl)                   
00FB:  FB           ei                          
00FC:  0A           ld a,(bc)                   
00FD:  B7           or a                        
00FE:  EB           ex de,hl                    
00FF:  28 08        jr z,script_op               ; -> script_op
0101:  3D           dec a                       
0102:  02           ld (bc),a                   
0103:  C2 36 02     jp nz,script_save_pc         ; -> script_save_pc
0106:  18 01        jr script_op                 ; -> script_op

script_next_op:
0108:  23           inc hl                      

script_op:
0109:  7E           ld a,(hl)                   
010A:  FE F0        cp $F0                      
010C:  D2 81 01     jp nc,script_ctl_op          ; -> script_ctl_op
010F:  DD 21 10 10  ld ix,ay_shadow              ; ay_shadow
0113:  E6 1F        and $1F                     
0115:  5F           ld e,a                      
0116:  16 00        ld d,$00                    
0118:  DD 19        add ix,de                   
011A:  7E           ld a,(hl)                   
011B:  23           inc hl                      
011C:  E6 E0        and $E0                     
011E:  FE 00        cp $00                      
0120:  28 17        jr z,op_reg_load             ; -> op_reg_load
0122:  FE 20        cp $20                      
0124:  28 24        jr z,op_reg_load16           ; -> op_reg_load16
0126:  FE 40        cp $40                      
0128:  28 2B        jr z,op_reg_or               ; -> op_reg_or
012A:  FE 60        cp $60                      
012C:  28 30        jr z,op_reg_and              ; -> op_reg_and
012E:  FE 80        cp $80                      
0130:  28 35        jr z,op_reg_add              ; -> op_reg_add
0132:  FE A0        cp $A0                      
0134:  28 3A        jr z,op_reg_add16            ; -> op_reg_add16
0136:  C3 35 02     jp op_end_script_0235       

op_reg_load:
0139:  7E           ld a,(hl)                   
013A:  DD 77 00     ld (ix+0),a                 
013D:  7B           ld a,e                      
013E:  E6 0F        and $0F                     
0140:  FE 0D        cp $0D                      
0142:  20 C4        jr nz,script_next_op         ; -> script_next_op
0144:  DD 36 20 FF  ld (ix+32),$FF              
0148:  18 BE        jr script_next_op            ; -> script_next_op

op_reg_load16:
014A:  7E           ld a,(hl)                   
014B:  DD 77 00     ld (ix+0),a                 
014E:  23           inc hl                      
014F:  7E           ld a,(hl)                   
0150:  DD 77 01     ld (ix+1),a                 
0153:  18 B3        jr script_next_op            ; -> script_next_op

op_reg_or:
0155:  7E           ld a,(hl)                   
0156:  DD B6 00     or (ix+0)                   
0159:  DD 77 00     ld (ix+0),a                 
015C:  18 AA        jr script_next_op            ; -> script_next_op

op_reg_and:
015E:  DD 7E 00     ld a,(ix+0)                 
0161:  A6           and (hl)                    
0162:  DD 77 00     ld (ix+0),a                 
0165:  18 A1        jr script_next_op            ; -> script_next_op

op_reg_add:
0167:  DD 7E 00     ld a,(ix+0)                 
016A:  86           add a,(hl)                  
016B:  DD 77 00     ld (ix+0),a                 
016E:  18 98        jr script_next_op            ; -> script_next_op

op_reg_add16:
0170:  DD 7E 00     ld a,(ix+0)                 
0173:  86           add a,(hl)                  
0174:  DD 77 00     ld (ix+0),a                 
0177:  23           inc hl                      
0178:  DD 7E 01     ld a,(ix+1)                 
017B:  8E           adc a,(hl)                  
017C:  DD 77 01     ld (ix+1),a                 
017F:  18 87        jr script_next_op            ; -> script_next_op

script_ctl_op:
0181:  23           inc hl                      
0182:  FE F1        cp $F1                      
0184:  28 19        jr z,op_f1_delay             ; -> op_f1_delay
0186:  FE F2        cp $F2                      
0188:  28 1A        jr z,op_f2_loop_push         ; -> op_f2_loop_push
018A:  FE F3        cp $F3                      
018C:  28 35        jr z,op_f3_loop_end          ; -> op_f3_loop_end
018E:  FE F4        cp $F4                      
0190:  28 61        jr z,op_f4_start             ; -> op_f4_start
0192:  FE F5        cp $F5                      
0194:  CA 19 02     jp z,op_f5_kill              ; -> op_f5_kill
0197:  FE F6        cp $F6                      
0199:  CA 2A 02     jp z,op_f6_jump              ; -> op_f6_jump
019C:  C3 31 02     jp op_end_script             ; -> op_end_script

op_f1_delay:
019F:  7E           ld a,(hl)                   
01A0:  02           ld (bc),a                   
01A1:  C3 35 02     jp op_end_script_0235       

op_f2_loop_push:
01A4:  C5           push bc                     
01A5:  03           inc bc                      
01A6:  0A           ld a,(bc)                   
01A7:  3C           inc a                       
01A8:  02           ld (bc),a                   
01A9:  03           inc bc                      

op_f2_loop_push_01AA:
01AA:  3D           dec a                       
01AB:  28 06        jr z,op_f2_loop_push_01B3   
01AD:  03           inc bc                      
01AE:  03           inc bc                      
01AF:  03           inc bc                      
01B0:  03           inc bc                      
01B1:  18 F7        jr op_f2_loop_push_01AA     

op_f2_loop_push_01B3:
01B3:  7E           ld a,(hl)                   
01B4:  02           ld (bc),a                   
01B5:  23           inc hl                      
01B6:  03           inc bc                      
01B7:  7E           ld a,(hl)                   
01B8:  02           ld (bc),a                   
01B9:  03           inc bc                      
01BA:  7D           ld a,l                      
01BB:  02           ld (bc),a                   
01BC:  03           inc bc                      
01BD:  7C           ld a,h                      
01BE:  02           ld (bc),a                   
01BF:  C1           pop bc                      
01C0:  C3 08 01     jp script_next_op            ; -> script_next_op

op_f3_loop_end:
01C3:  C5           push bc                     
01C4:  03           inc bc                      
01C5:  0A           ld a,(bc)                   
01C6:  03           inc bc                      

op_f3_loop_end_01C7:
01C7:  3D           dec a                       
01C8:  28 06        jr z,op_f3_loop_end_01D0    
01CA:  03           inc bc                      
01CB:  03           inc bc                      
01CC:  03           inc bc                      
01CD:  03           inc bc                      
01CE:  18 F7        jr op_f3_loop_end_01C7      

op_f3_loop_end_01D0:
01D0:  0A           ld a,(bc)                   
01D1:  03           inc bc                      
01D2:  5F           ld e,a                      
01D3:  0A           ld a,(bc)                   
01D4:  57           ld d,a                      
01D5:  1B           dec de                      
01D6:  7A           ld a,d                      
01D7:  02           ld (bc),a                   
01D8:  0B           dec bc                      
01D9:  7B           ld a,e                      
01DA:  02           ld (bc),a                   
01DB:  B2           or d                        
01DC:  28 0B        jr z,op_f3_loop_end_01E9    
01DE:  03           inc bc                      
01DF:  03           inc bc                      
01E0:  0A           ld a,(bc)                   
01E1:  6F           ld l,a                      
01E2:  03           inc bc                      
01E3:  0A           ld a,(bc)                   
01E4:  67           ld h,a                      
01E5:  C1           pop bc                      
01E6:  C3 08 01     jp script_next_op            ; -> script_next_op

op_f3_loop_end_01E9:
01E9:  2B           dec hl                      
01EA:  C1           pop bc                      
01EB:  03           inc bc                      
01EC:  0A           ld a,(bc)                   
01ED:  3D           dec a                       
01EE:  02           ld (bc),a                   
01EF:  0B           dec bc                      
01F0:  C3 08 01     jp script_next_op            ; -> script_next_op

op_f4_start:
01F3:  7E           ld a,(hl)                   
01F4:  C5           push bc                     
01F5:  E5           push hl                     
01F6:  4F           ld c,a                      
01F7:  CB 21        sla c                       
01F9:  06 00        ld b,$00                    
01FB:  21 C0 11     ld hl,script_slots           ; script_slots
01FE:  09           add hl,bc                   
01FF:  09           add hl,bc                   
0200:  EB           ex de,hl                    
0201:  21 4A 02     ld hl,$024A                 
0204:  09           add hl,bc                   
0205:  09           add hl,bc                   
0206:  01 04 00     ld bc,$0004                 
0209:  ED B0        ldir                        
020B:  2B           dec hl                      
020C:  56           ld d,(hl)                   
020D:  2B           dec hl                      
020E:  5E           ld e,(hl)                   
020F:  3E 00        ld a,$00                    
0211:  12           ld (de),a                   
0212:  13           inc de                      
0213:  12           ld (de),a                   
0214:  E1           pop hl                      
0215:  C1           pop bc                      
0216:  C3 08 01     jp script_next_op            ; -> script_next_op

op_f5_kill:
0219:  5E           ld e,(hl)                   
021A:  E5           push hl                     
021B:  CB 23        sla e                       
021D:  16 00        ld d,$00                    
021F:  21 C1 11     ld hl,$11C1                 
0222:  19           add hl,de                   
0223:  19           add hl,de                   
0224:  CB FE        set 7,(hl)                  
0226:  E1           pop hl                      
0227:  C3 08 01     jp script_next_op            ; -> script_next_op

op_f6_jump:
022A:  5E           ld e,(hl)                   
022B:  23           inc hl                      
022C:  56           ld d,(hl)                   
022D:  19           add hl,de                   
022E:  C3 08 01     jp script_next_op            ; -> script_next_op

op_end_script:
0231:  CB FC        set 7,h                     
0233:  18 00        jr op_end_script_0235       

op_end_script_0235:
0235:  23           inc hl                      

script_save_pc:
0236:  D1           pop de                      
0237:  1A           ld a,(de)                   
0238:  CB 7F        bit 7,a                     
023A:  28 07        jr z,script_save_pc_0243    
023C:  7C           ld a,h                      
023D:  F3           di                          
023E:  12           ld (de),a                   
023F:  1B           dec de                      
0240:  7D           ld a,l                      
0241:  12           ld (de),a                   
0242:  FB           ei                          

script_save_pc_0243:
0243:  FD 36 03 18  ld (iy+3),$18               
0247:  E1           pop hl                      
0248:  C1           pop bc                      
0249:  C9           ret                         

; ---- 024A-02A5: command dispatch table: {script ptr, var ptr} per cmd 0x00-0x16 (copied to the slot at 0x11C0+4*cmd by SND_IRQ_LATCH / op F4) ----
024A:  cmd 00: script $07B4  vars $1070   ; all sound off
024E:  cmd 01: script $0759  vars $1080   ; player ship explosion
0252:  cmd 02: script $0467  vars $1090   ; droid explosion (kill_tier 3)
0256:  cmd 03: script $0446  vars $10A0   ; command-ship explosion (kill_tier 1)
025A:  cmd 04: script $0549  vars $10B0   ; wave-complete tune
025E:  cmd 05: script $0659  vars $10C0   ; tune harmony voice A (internal)
0262:  cmd 06: script $06A7  vars $10D0   ; tune harmony voice B (internal)
0266:  cmd 07: script $072E  vars $10E0   ; ship fire
026A:  cmd 08: script $04F7  vars $10F0   ; wall hit
026E:  cmd 09: script $0428  vars $1100   ; thrust on
0272:  cmd 0A: script $043F  vars $1110   ; thrust off
0276:  cmd 0B: script $02E6  vars $1120   ; heartbeat tempo 1 (slowest)
027A:  cmd 0C: script $0328  vars $1130   ; heartbeat tempo 2
027E:  cmd 0D: script $036A  vars $1140   ; heartbeat tempo 3
0282:  cmd 0E: script $03AC  vars $1150   ; heartbeat tempo 4
0286:  cmd 0F: script $03EE  vars $1160   ; heartbeat tempo 5 (fastest)
028A:  cmd 10: script $048F  vars $1170   ; droid explosion (kill_tier 4)
028E:  cmd 11: script $04B5  vars $1180   ; command-ship explosion (kill_tier != 1)
0292:  cmd 12: script $04CE  vars $1190   ; coin chime
0296:  cmd 13: script $052A  vars $11A0   ; tilt/slam alarm
029A:  cmd 14: script $0512  vars $11B0   ; bonus/extra-life beeping
029E:  cmd 15: script $02D1  vars $1060   ; droid explosion (default tier)
02A2:  cmd 16: script $02A6  vars $1050   ; self-test sweep

; ---- 02A6-07FF: sound scripts, F0-FF bytecode run by SCRIPT_RUN once per slot per 244 Hz tick (semantics in SOUND_NOTES.md) ----

; ==== script 16: self-test sweep  (vars $1050) ====
02A6:  67 FE        AY1_MIXER &= $FE
02A8:  20 33 00     AY1_A_PERIOD = $0033
02AB:  08 0F        AY1_A_VOL = $0F
02AD:  F2 F4 01     repeat 500 times {
02B0:  A0 01 00     AY1_A_PERIOD += $0001
02B3:  F1 00        delay 0 ticks (0 ms)
02B5:  F3           } loop back while count remains
02B6:  47 01        AY1_MIXER |= $01
02B8:  F1 63        delay 99 ticks (396 ms)
02BA:  77 FE        AY2_MIXER &= $FE
02BC:  30 33 00     AY2_A_PERIOD = $0033
02BF:  18 0F        AY2_A_VOL = $0F
02C1:  F2 F4 01     repeat 500 times {
02C4:  B0 01 00     AY2_A_PERIOD += $0001
02C7:  F1 00        delay 0 ticks (0 ms)
02C9:  F3           } loop back while count remains
02CA:  57 01        AY2_MIXER |= $01
02CC:  F1 63        delay 99 ticks (396 ms)
02CE:  F6 D5 FF     jump -> $02A6 (loop)

; ==== script 15: droid explosion (default tier)  (vars $1060) ====
02D1:  F5 10        kill script 10 (droid explosion (kill_tier 4))
02D3:  F5 02        kill script 02 (droid explosion (kill_tier 3))
02D5:  F1 09        delay 9 ticks (36 ms)
02D7:  F2 04 00     repeat 4 times {
02DA:  F4 10        start script 10 (droid explosion (kill_tier 4))
02DC:  F1 1E        delay 30 ticks (120 ms)
02DE:  F5 10        kill script 10 (droid explosion (kill_tier 4))
02E0:  F1 08        delay 8 ticks (32 ms)
02E2:  F3           } loop back while count remains
02E3:  F4 10        start script 10 (droid explosion (kill_tier 4))
02E5:  FF           end of script

; ==== script 0B: heartbeat tempo 1 (slowest)  (vars $1120) ====
02E6:  F5 0C        kill script 0C (heartbeat tempo 2)
02E8:  F5 0D        kill script 0D (heartbeat tempo 3)
02EA:  F5 0E        kill script 0E (heartbeat tempo 4)
02EC:  F5 0F        kill script 0F (heartbeat tempo 5 (fastest))
02EE:  77 FE        AY2_MIXER &= $FE
02F0:  30 F6 02     AY2_A_PERIOD = $02F6
02F3:  18 0B        AY2_A_VOL = $0B
02F5:  F1 14        delay 20 ticks (80 ms)
02F7:  18 00        AY2_A_VOL = $00
02F9:  F1 02        delay 2 ticks (8 ms)
02FB:  F1 7F        delay 127 ticks (508 ms)
02FD:  30 7E 02     AY2_A_PERIOD = $027E
0300:  18 0C        AY2_A_VOL = $0C
0302:  F1 14        delay 20 ticks (80 ms)
0304:  18 00        AY2_A_VOL = $00
0306:  F1 02        delay 2 ticks (8 ms)
0308:  F1 7F        delay 127 ticks (508 ms)
030A:  30 5A 02     AY2_A_PERIOD = $025A
030D:  18 0D        AY2_A_VOL = $0D
030F:  F1 14        delay 20 ticks (80 ms)
0311:  18 00        AY2_A_VOL = $00
0313:  F1 02        delay 2 ticks (8 ms)
0315:  F1 7F        delay 127 ticks (508 ms)
0317:  30 7E 02     AY2_A_PERIOD = $027E
031A:  18 0C        AY2_A_VOL = $0C
031C:  F1 14        delay 20 ticks (80 ms)
031E:  18 00        AY2_A_VOL = $00
0320:  F1 02        delay 2 ticks (8 ms)
0322:  F1 7F        delay 127 ticks (508 ms)
0324:  F6 C7 FF     jump -> $02EE (loop)
0327:  FF           end of script

; ==== script 0C: heartbeat tempo 2  (vars $1130) ====
0328:  F5 0B        kill script 0B (heartbeat tempo 1 (slowest))
032A:  F5 0D        kill script 0D (heartbeat tempo 3)
032C:  F5 0E        kill script 0E (heartbeat tempo 4)
032E:  F5 0F        kill script 0F (heartbeat tempo 5 (fastest))
0330:  77 FE        AY2_MIXER &= $FE
0332:  30 F6 02     AY2_A_PERIOD = $02F6
0335:  18 0B        AY2_A_VOL = $0B
0337:  F1 14        delay 20 ticks (80 ms)
0339:  18 00        AY2_A_VOL = $00
033B:  F1 02        delay 2 ticks (8 ms)
033D:  F1 5F        delay 95 ticks (380 ms)
033F:  30 7E 02     AY2_A_PERIOD = $027E
0342:  18 0C        AY2_A_VOL = $0C
0344:  F1 14        delay 20 ticks (80 ms)
0346:  18 00        AY2_A_VOL = $00
0348:  F1 02        delay 2 ticks (8 ms)
034A:  F1 5F        delay 95 ticks (380 ms)
034C:  30 5A 02     AY2_A_PERIOD = $025A
034F:  18 0D        AY2_A_VOL = $0D
0351:  F1 14        delay 20 ticks (80 ms)
0353:  18 00        AY2_A_VOL = $00
0355:  F1 02        delay 2 ticks (8 ms)
0357:  F1 5F        delay 95 ticks (380 ms)
0359:  30 7E 02     AY2_A_PERIOD = $027E
035C:  18 0C        AY2_A_VOL = $0C
035E:  F1 14        delay 20 ticks (80 ms)
0360:  18 00        AY2_A_VOL = $00
0362:  F1 02        delay 2 ticks (8 ms)
0364:  F1 5F        delay 95 ticks (380 ms)
0366:  F6 C7 FF     jump -> $0330 (loop)
0369:  FF           end of script

; ==== script 0D: heartbeat tempo 3  (vars $1140) ====
036A:  F5 0C        kill script 0C (heartbeat tempo 2)
036C:  F5 0B        kill script 0B (heartbeat tempo 1 (slowest))
036E:  F5 0E        kill script 0E (heartbeat tempo 4)
0370:  F5 0F        kill script 0F (heartbeat tempo 5 (fastest))
0372:  77 FE        AY2_MIXER &= $FE
0374:  30 F6 02     AY2_A_PERIOD = $02F6
0377:  18 0C        AY2_A_VOL = $0C
0379:  F1 14        delay 20 ticks (80 ms)
037B:  18 00        AY2_A_VOL = $00
037D:  F1 02        delay 2 ticks (8 ms)
037F:  F1 2F        delay 47 ticks (188 ms)
0381:  30 7E 02     AY2_A_PERIOD = $027E
0384:  18 0D        AY2_A_VOL = $0D
0386:  F1 14        delay 20 ticks (80 ms)
0388:  18 00        AY2_A_VOL = $00
038A:  F1 02        delay 2 ticks (8 ms)
038C:  F1 2F        delay 47 ticks (188 ms)
038E:  30 5A 02     AY2_A_PERIOD = $025A
0391:  18 0E        AY2_A_VOL = $0E
0393:  F1 14        delay 20 ticks (80 ms)
0395:  18 00        AY2_A_VOL = $00
0397:  F1 02        delay 2 ticks (8 ms)
0399:  F1 2F        delay 47 ticks (188 ms)
039B:  30 7E 02     AY2_A_PERIOD = $027E
039E:  18 0D        AY2_A_VOL = $0D
03A0:  F1 14        delay 20 ticks (80 ms)
03A2:  18 00        AY2_A_VOL = $00
03A4:  F1 02        delay 2 ticks (8 ms)
03A6:  F1 2F        delay 47 ticks (188 ms)
03A8:  F6 C7 FF     jump -> $0372 (loop)
03AB:  FF           end of script

; ==== script 0E: heartbeat tempo 4  (vars $1150) ====
03AC:  F5 0D        kill script 0D (heartbeat tempo 3)
03AE:  F5 0B        kill script 0B (heartbeat tempo 1 (slowest))
03B0:  F5 0C        kill script 0C (heartbeat tempo 2)
03B2:  F5 0F        kill script 0F (heartbeat tempo 5 (fastest))
03B4:  77 FE        AY2_MIXER &= $FE
03B6:  30 F6 02     AY2_A_PERIOD = $02F6
03B9:  18 0C        AY2_A_VOL = $0C
03BB:  F1 14        delay 20 ticks (80 ms)
03BD:  18 00        AY2_A_VOL = $00
03BF:  F1 02        delay 2 ticks (8 ms)
03C1:  F1 17        delay 23 ticks (92 ms)
03C3:  30 7E 02     AY2_A_PERIOD = $027E
03C6:  18 0D        AY2_A_VOL = $0D
03C8:  F1 14        delay 20 ticks (80 ms)
03CA:  18 00        AY2_A_VOL = $00
03CC:  F1 02        delay 2 ticks (8 ms)
03CE:  F1 19        delay 25 ticks (100 ms)
03D0:  30 5A 02     AY2_A_PERIOD = $025A
03D3:  18 0E        AY2_A_VOL = $0E
03D5:  F1 14        delay 20 ticks (80 ms)
03D7:  18 00        AY2_A_VOL = $00
03D9:  F1 02        delay 2 ticks (8 ms)
03DB:  F1 17        delay 23 ticks (92 ms)
03DD:  30 7E 02     AY2_A_PERIOD = $027E
03E0:  18 0D        AY2_A_VOL = $0D
03E2:  F1 14        delay 20 ticks (80 ms)
03E4:  18 00        AY2_A_VOL = $00
03E6:  F1 02        delay 2 ticks (8 ms)
03E8:  F1 17        delay 23 ticks (92 ms)
03EA:  F6 C7 FF     jump -> $03B4 (loop)
03ED:  FF           end of script

; ==== script 0F: heartbeat tempo 5 (fastest)  (vars $1160) ====
03EE:  F5 0E        kill script 0E (heartbeat tempo 4)
03F0:  F5 0B        kill script 0B (heartbeat tempo 1 (slowest))
03F2:  F5 0C        kill script 0C (heartbeat tempo 2)
03F4:  F5 0D        kill script 0D (heartbeat tempo 3)
03F6:  77 FE        AY2_MIXER &= $FE
03F8:  30 A6 02     AY2_A_PERIOD = $02A6
03FB:  18 0D        AY2_A_VOL = $0D
03FD:  F1 14        delay 20 ticks (80 ms)
03FF:  18 00        AY2_A_VOL = $00
0401:  F1 02        delay 2 ticks (8 ms)
0403:  30 1E 02     AY2_A_PERIOD = $021E
0406:  18 0E        AY2_A_VOL = $0E
0408:  F1 14        delay 20 ticks (80 ms)
040A:  18 00        AY2_A_VOL = $00
040C:  F1 02        delay 2 ticks (8 ms)
040E:  30 FA 01     AY2_A_PERIOD = $01FA
0411:  18 0F        AY2_A_VOL = $0F
0413:  F1 14        delay 20 ticks (80 ms)
0415:  18 00        AY2_A_VOL = $00
0417:  F1 02        delay 2 ticks (8 ms)
0419:  30 7E 02     AY2_A_PERIOD = $027E
041C:  18 0E        AY2_A_VOL = $0E
041E:  F1 14        delay 20 ticks (80 ms)
0420:  18 00        AY2_A_VOL = $00
0422:  F1 02        delay 2 ticks (8 ms)
0424:  F6 CF FF     jump -> $03F6 (loop)
0427:  FF           end of script

; ==== script 09: thrust on  (vars $1100) ====
0428:  1A 0F        AY2_C_VOL = $0F
042A:  F2 07 00     repeat 7 times {
042D:  16 1F        AY2_NOISE_PER = $1F
042F:  77 DF        AY2_MIXER &= $DF
0431:  9A FF        AY2_C_VOL += $FF
0433:  F1 02        delay 2 ticks (8 ms)
0435:  F3           } loop back while count remains
0436:  F2 0A 00     repeat 10 times {
0439:  96 FD        AY2_NOISE_PER += $FD
043B:  F1 03        delay 3 ticks (12 ms)
043D:  F3           } loop back while count remains
043E:  FF           end of script

; ==== script 0A: thrust off  (vars $1110) ====
043F:  F5 09        kill script 09 (thrust on)
0441:  1A 00        AY2_C_VOL = $00
0443:  57 20        AY2_MIXER |= $20
0445:  FF           end of script

; ==== script 03: command-ship explosion (kill_tier 1)  (vars $10A0) ====
0446:  F5 11        kill script 11 (command-ship explosion (kill_tier != 1))
0448:  32 50 00     AY2_B_PERIOD = $0050
044B:  19 0F        AY2_B_VOL = $0F
044D:  F2 06 00     repeat 6 times {
0450:  F2 02 00     repeat 2 times {
0453:  57 10        AY2_MIXER |= $10
0455:  77 FD        AY2_MIXER &= $FD
0457:  99 FF        AY2_B_VOL += $FF
0459:  F1 06        delay 6 ticks (24 ms)
045B:  F3           } loop back while count remains
045C:  B2 F0 FF     AY2_B_PERIOD += $FFF0
045F:  F3           } loop back while count remains
0460:  57 02        AY2_MIXER |= $02
0462:  12 00        AY2_B_PER_LO = $00
0464:  19 00        AY2_B_VOL = $00
0466:  FF           end of script

; ==== script 02: droid explosion (kill_tier 3)  (vars $1090) ====
0467:  F5 10        kill script 10 (droid explosion (kill_tier 4))
0469:  F5 15        kill script 15 (droid explosion (default tier))
046B:  22 38 02     AY1_B_PERIOD = $0238
046E:  09 0F        AY1_B_VOL = $0F
0470:  F2 07 00     repeat 7 times {
0473:  F2 06 00     repeat 6 times {
0476:  67 FD        AY1_MIXER &= $FD
0478:  A2 F0 FF     AY1_B_PERIOD += $FFF0
047B:  F1 02        delay 2 ticks (8 ms)
047D:  F3           } loop back while count remains
047E:  F2 06 00     repeat 6 times {
0481:  A2 10 00     AY1_B_PERIOD += $0010
0484:  F1 02        delay 2 ticks (8 ms)
0486:  F3           } loop back while count remains
0487:  89 FE        AY1_B_VOL += $FE
0489:  F3           } loop back while count remains
048A:  47 02        AY1_MIXER |= $02
048C:  09 00        AY1_B_VOL = $00
048E:  FF           end of script

; ==== script 10: droid explosion (kill_tier 4)  (vars $1170) ====
048F:  F5 02        kill script 02 (droid explosion (kill_tier 3))
0491:  22 6F 00     AY1_B_PERIOD = $006F
0494:  09 0F        AY1_B_VOL = $0F
0496:  F2 07 00     repeat 7 times {
0499:  F2 06 00     repeat 6 times {
049C:  67 FD        AY1_MIXER &= $FD
049E:  A2 F0 FF     AY1_B_PERIOD += $FFF0
04A1:  F1 02        delay 2 ticks (8 ms)
04A3:  F3           } loop back while count remains
04A4:  F2 06 00     repeat 6 times {
04A7:  A2 10 00     AY1_B_PERIOD += $0010
04AA:  F1 02        delay 2 ticks (8 ms)
04AC:  F3           } loop back while count remains
04AD:  89 FE        AY1_B_VOL += $FE
04AF:  F3           } loop back while count remains
04B0:  47 02        AY1_MIXER |= $02
04B2:  09 00        AY1_B_VOL = $00
04B4:  FF           end of script

; ==== script 11: command-ship explosion (kill_tier != 1)  (vars $1180) ====
04B5:  F5 03        kill script 03 (command-ship explosion (kill_tier 1))
04B7:  16 00        AY2_NOISE_PER = $00
04B9:  57 02        AY2_MIXER |= $02
04BB:  77 EF        AY2_MIXER &= $EF
04BD:  19 00        AY2_B_VOL = $00
04BF:  F2 0F 00     repeat 15 times {
04C2:  96 FE        AY2_NOISE_PER += $FE
04C4:  99 01        AY2_B_VOL += $01
04C6:  F1 04        delay 4 ticks (16 ms)
04C8:  F3           } loop back while count remains
04C9:  19 00        AY2_B_VOL = $00
04CB:  57 10        AY2_MIXER |= $10
04CD:  FF           end of script

; ==== script 12: coin chime  (vars $1190) ====
04CE:  F5 07        kill script 07 (ship fire)
04D0:  67 FE        AY1_MIXER &= $FE
04D2:  20 EF 00     AY1_A_PERIOD = $00EF
04D5:  08 0B        AY1_A_VOL = $0B
04D7:  F1 11        delay 17 ticks (68 ms)
04D9:  20 5F 00     AY1_A_PERIOD = $005F
04DC:  F1 11        delay 17 ticks (68 ms)
04DE:  20 7F 00     AY1_A_PERIOD = $007F
04E1:  F1 11        delay 17 ticks (68 ms)
04E3:  20 47 00     AY1_A_PERIOD = $0047
04E6:  F1 11        delay 17 ticks (68 ms)
04E8:  20 BE 00     AY1_A_PERIOD = $00BE
04EB:  F1 11        delay 17 ticks (68 ms)
04ED:  20 77 00     AY1_A_PERIOD = $0077
04F0:  F1 11        delay 17 ticks (68 ms)
04F2:  08 00        AY1_A_VOL = $00
04F4:  47 01        AY1_MIXER |= $01
04F6:  FF           end of script

; ==== script 08: wall hit  (vars $10F0) ====
04F7:  24 00 01     AY1_C_PERIOD = $0100
04FA:  0A 0D        AY1_C_VOL = $0D
04FC:  67 FB        AY1_MIXER &= $FB
04FE:  F2 06 00     repeat 6 times {
0501:  F2 01 00     repeat 1 times {
0504:  8A FF        AY1_C_VOL += $FF
0506:  F1 07        delay 7 ticks (28 ms)
0508:  F3           } loop back while count remains
0509:  A4 F0 FF     AY1_C_PERIOD += $FFF0
050C:  F3           } loop back while count remains
050D:  47 04        AY1_MIXER |= $04
050F:  0A 00        AY1_C_VOL = $00
0511:  FF           end of script

; ==== script 14: bonus/extra-life beeping  (vars $11B0) ====
0512:  F4 00        start script 00 (all sound off)
0514:  F1 05        delay 5 ticks (20 ms)
0516:  30 3F 00     AY2_A_PERIOD = $003F
0519:  77 FE        AY2_MIXER &= $FE
051B:  F2 10 00     repeat 16 times {
051E:  18 0F        AY2_A_VOL = $0F
0520:  F1 10        delay 16 ticks (64 ms)
0522:  18 00        AY2_A_VOL = $00
0524:  F1 0A        delay 10 ticks (40 ms)
0526:  F3           } loop back while count remains
0527:  57 01        AY2_MIXER |= $01
0529:  FF           end of script

; ==== script 13: tilt/slam alarm  (vars $11A0) ====
052A:  F4 00        start script 00 (all sound off)
052C:  F1 01        delay 1 ticks (4 ms)
052E:  F2 08 00     repeat 8 times {
0531:  20 7F 00     AY1_A_PERIOD = $007F
0534:  F2 3C 00     repeat 60 times {
0537:  67 FE        AY1_MIXER &= $FE
0539:  08 0F        AY1_A_VOL = $0F
053B:  A0 FF FF     AY1_A_PERIOD += $FFFF
053E:  F1 01        delay 1 ticks (4 ms)
0540:  F3           } loop back while count remains
0541:  F1 09        delay 9 ticks (36 ms)
0543:  F3           } loop back while count remains
0544:  47 01        AY1_MIXER |= $01
0546:  08 00        AY1_A_VOL = $00
0548:  FF           end of script

; ==== script 04: wave-complete tune  (vars $10B0) ====
0549:  F5 14        kill script 14 (bonus/extra-life beeping)
054B:  67 FD        AY1_MIXER &= $FD
054D:  67 FB        AY1_MIXER &= $FB
054F:  77 FE        AY2_MIXER &= $FE
0551:  30 47 00     AY2_A_PERIOD = $0047
0554:  18 0A        AY2_A_VOL = $0A
0556:  F1 14        delay 20 ticks (80 ms)
0558:  18 00        AY2_A_VOL = $00
055A:  F1 02        delay 2 ticks (8 ms)
055C:  30 43 00     AY2_A_PERIOD = $0043
055F:  18 0A        AY2_A_VOL = $0A
0561:  F1 14        delay 20 ticks (80 ms)
0563:  18 00        AY2_A_VOL = $00
0565:  F1 02        delay 2 ticks (8 ms)
0567:  30 47 00     AY2_A_PERIOD = $0047
056A:  18 0A        AY2_A_VOL = $0A
056C:  F1 14        delay 20 ticks (80 ms)
056E:  18 00        AY2_A_VOL = $00
0570:  F1 02        delay 2 ticks (8 ms)
0572:  30 43 00     AY2_A_PERIOD = $0043
0575:  18 0A        AY2_A_VOL = $0A
0577:  F1 14        delay 20 ticks (80 ms)
0579:  18 00        AY2_A_VOL = $00
057B:  F1 02        delay 2 ticks (8 ms)
057D:  F4 05        start script 05 (tune harmony voice A (internal))
057F:  F4 06        start script 06 (tune harmony voice B (internal))
0581:  30 3C 00     AY2_A_PERIOD = $003C
0584:  18 0A        AY2_A_VOL = $0A
0586:  F1 44        delay 68 ticks (272 ms)
0588:  18 00        AY2_A_VOL = $00
058A:  F1 02        delay 2 ticks (8 ms)
058C:  30 59 00     AY2_A_PERIOD = $0059
058F:  18 0A        AY2_A_VOL = $0A
0591:  F1 8C        delay 140 ticks (560 ms)
0593:  18 00        AY2_A_VOL = $00
0595:  F1 02        delay 2 ticks (8 ms)
0597:  30 47 00     AY2_A_PERIOD = $0047
059A:  18 0A        AY2_A_VOL = $0A
059C:  F1 14        delay 20 ticks (80 ms)
059E:  18 00        AY2_A_VOL = $00
05A0:  F1 02        delay 2 ticks (8 ms)
05A2:  30 43 00     AY2_A_PERIOD = $0043
05A5:  18 0A        AY2_A_VOL = $0A
05A7:  F1 14        delay 20 ticks (80 ms)
05A9:  18 00        AY2_A_VOL = $00
05AB:  F1 02        delay 2 ticks (8 ms)
05AD:  30 47 00     AY2_A_PERIOD = $0047
05B0:  18 0A        AY2_A_VOL = $0A
05B2:  F1 14        delay 20 ticks (80 ms)
05B4:  18 00        AY2_A_VOL = $00
05B6:  F1 02        delay 2 ticks (8 ms)
05B8:  30 43 00     AY2_A_PERIOD = $0043
05BB:  18 0A        AY2_A_VOL = $0A
05BD:  F1 14        delay 20 ticks (80 ms)
05BF:  18 00        AY2_A_VOL = $00
05C1:  F1 02        delay 2 ticks (8 ms)
05C3:  30 3C 00     AY2_A_PERIOD = $003C
05C6:  18 0A        AY2_A_VOL = $0A
05C8:  F1 44        delay 68 ticks (272 ms)
05CA:  18 00        AY2_A_VOL = $00
05CC:  F1 02        delay 2 ticks (8 ms)
05CE:  30 59 00     AY2_A_PERIOD = $0059
05D1:  18 0A        AY2_A_VOL = $0A
05D3:  F1 8C        delay 140 ticks (560 ms)
05D5:  18 00        AY2_A_VOL = $00
05D7:  F1 02        delay 2 ticks (8 ms)
05D9:  30 47 00     AY2_A_PERIOD = $0047
05DC:  18 0A        AY2_A_VOL = $0A
05DE:  F1 14        delay 20 ticks (80 ms)
05E0:  18 00        AY2_A_VOL = $00
05E2:  F1 02        delay 2 ticks (8 ms)
05E4:  30 43 00     AY2_A_PERIOD = $0043
05E7:  18 0A        AY2_A_VOL = $0A
05E9:  F1 14        delay 20 ticks (80 ms)
05EB:  18 00        AY2_A_VOL = $00
05ED:  F1 02        delay 2 ticks (8 ms)
05EF:  30 47 00     AY2_A_PERIOD = $0047
05F2:  18 0A        AY2_A_VOL = $0A
05F4:  F1 14        delay 20 ticks (80 ms)
05F6:  18 00        AY2_A_VOL = $00
05F8:  F1 02        delay 2 ticks (8 ms)
05FA:  30 43 00     AY2_A_PERIOD = $0043
05FD:  18 0A        AY2_A_VOL = $0A
05FF:  F1 14        delay 20 ticks (80 ms)
0601:  18 00        AY2_A_VOL = $00
0603:  F1 02        delay 2 ticks (8 ms)
0605:  30 3C 00     AY2_A_PERIOD = $003C
0608:  18 0A        AY2_A_VOL = $0A
060A:  F1 44        delay 68 ticks (272 ms)
060C:  18 00        AY2_A_VOL = $00
060E:  F1 02        delay 2 ticks (8 ms)
0610:  30 59 00     AY2_A_PERIOD = $0059
0613:  18 0A        AY2_A_VOL = $0A
0615:  F1 8C        delay 140 ticks (560 ms)
0617:  18 00        AY2_A_VOL = $00
0619:  F1 02        delay 2 ticks (8 ms)
061B:  30 47 00     AY2_A_PERIOD = $0047
061E:  18 0A        AY2_A_VOL = $0A
0620:  F1 14        delay 20 ticks (80 ms)
0622:  18 00        AY2_A_VOL = $00
0624:  F1 02        delay 2 ticks (8 ms)
0626:  30 43 00     AY2_A_PERIOD = $0043
0629:  18 0A        AY2_A_VOL = $0A
062B:  F1 14        delay 20 ticks (80 ms)
062D:  18 00        AY2_A_VOL = $00
062F:  F1 02        delay 2 ticks (8 ms)
0631:  30 47 00     AY2_A_PERIOD = $0047
0634:  18 0A        AY2_A_VOL = $0A
0636:  F1 14        delay 20 ticks (80 ms)
0638:  18 00        AY2_A_VOL = $00
063A:  F1 02        delay 2 ticks (8 ms)
063C:  30 43 00     AY2_A_PERIOD = $0043
063F:  18 0A        AY2_A_VOL = $0A
0641:  F1 14        delay 20 ticks (80 ms)
0643:  18 00        AY2_A_VOL = $00
0645:  F1 02        delay 2 ticks (8 ms)
0647:  30 50 00     AY2_A_PERIOD = $0050
064A:  18 0A        AY2_A_VOL = $0A
064C:  F1 D4        delay 212 ticks (848 ms)
064E:  18 00        AY2_A_VOL = $00
0650:  F1 02        delay 2 ticks (8 ms)
0652:  47 02        AY1_MIXER |= $02
0654:  47 04        AY1_MIXER |= $04
0656:  57 01        AY2_MIXER |= $01
0658:  FF           end of script

; ==== script 05: tune harmony voice A (internal)  (vars $10C0) ====
0659:  22 1C 01     AY1_B_PERIOD = $011C
065C:  09 07        AY1_B_VOL = $07
065E:  F1 D4        delay 212 ticks (848 ms)
0660:  09 00        AY1_B_VOL = $00
0662:  F1 02        delay 2 ticks (8 ms)
0664:  22 EF 00     AY1_B_PERIOD = $00EF
0667:  09 07        AY1_B_VOL = $07
0669:  F1 5C        delay 92 ticks (368 ms)
066B:  09 00        AY1_B_VOL = $00
066D:  F1 02        delay 2 ticks (8 ms)
066F:  22 1C 01     AY1_B_PERIOD = $011C
0672:  09 07        AY1_B_VOL = $07
0674:  F1 D4        delay 212 ticks (848 ms)
0676:  09 00        AY1_B_VOL = $00
0678:  F1 02        delay 2 ticks (8 ms)
067A:  22 EF 00     AY1_B_PERIOD = $00EF
067D:  09 07        AY1_B_VOL = $07
067F:  F1 5C        delay 92 ticks (368 ms)
0681:  09 00        AY1_B_VOL = $00
0683:  F1 02        delay 2 ticks (8 ms)
0685:  22 1C 01     AY1_B_PERIOD = $011C
0688:  09 07        AY1_B_VOL = $07
068A:  F1 44        delay 68 ticks (272 ms)
068C:  09 00        AY1_B_VOL = $00
068E:  F1 02        delay 2 ticks (8 ms)
0690:  22 66 01     AY1_B_PERIOD = $0166
0693:  09 07        AY1_B_VOL = $07
0695:  F1 BC        delay 188 ticks (752 ms)
0697:  09 00        AY1_B_VOL = $00
0699:  F1 02        delay 2 ticks (8 ms)
069B:  22 7B 01     AY1_B_PERIOD = $017B
069E:  09 07        AY1_B_VOL = $07
06A0:  F1 F8        delay 248 ticks (992 ms)
06A2:  09 00        AY1_B_VOL = $00
06A4:  F1 02        delay 2 ticks (8 ms)
06A6:  FF           end of script

; ==== script 06: tune harmony voice B (internal)  (vars $10D0) ====
06A7:  24 66 01     AY1_C_PERIOD = $0166
06AA:  0A 09        AY1_C_VOL = $09
06AC:  F1 44        delay 68 ticks (272 ms)
06AE:  0A 00        AY1_C_VOL = $00
06B0:  F1 02        delay 2 ticks (8 ms)
06B2:  24 DE 01     AY1_C_PERIOD = $01DE
06B5:  0A 09        AY1_C_VOL = $09
06B7:  F1 8C        delay 140 ticks (560 ms)
06B9:  0A 00        AY1_C_VOL = $00
06BB:  F1 02        delay 2 ticks (8 ms)
06BD:  24 3F 01     AY1_C_PERIOD = $013F
06C0:  0A 09        AY1_C_VOL = $09
06C2:  F1 2C        delay 44 ticks (176 ms)
06C4:  0A 00        AY1_C_VOL = $00
06C6:  F1 02        delay 2 ticks (8 ms)
06C8:  24 7B 01     AY1_C_PERIOD = $017B
06CB:  0A 09        AY1_C_VOL = $09
06CD:  F1 2C        delay 44 ticks (176 ms)
06CF:  0A 00        AY1_C_VOL = $00
06D1:  F1 02        delay 2 ticks (8 ms)
06D3:  24 66 01     AY1_C_PERIOD = $0166
06D6:  0A 09        AY1_C_VOL = $09
06D8:  F1 44        delay 68 ticks (272 ms)
06DA:  0A 00        AY1_C_VOL = $00
06DC:  F1 02        delay 2 ticks (8 ms)
06DE:  24 DE 01     AY1_C_PERIOD = $01DE
06E1:  0A 09        AY1_C_VOL = $09
06E3:  F1 8C        delay 140 ticks (560 ms)
06E5:  0A 00        AY1_C_VOL = $00
06E7:  F1 02        delay 2 ticks (8 ms)
06E9:  24 3F 01     AY1_C_PERIOD = $013F
06EC:  0A 09        AY1_C_VOL = $09
06EE:  F1 2C        delay 44 ticks (176 ms)
06F0:  0A 00        AY1_C_VOL = $00
06F2:  F1 02        delay 2 ticks (8 ms)
06F4:  24 7B 01     AY1_C_PERIOD = $017B
06F7:  0A 09        AY1_C_VOL = $09
06F9:  F1 2C        delay 44 ticks (176 ms)
06FB:  0A 00        AY1_C_VOL = $00
06FD:  F1 02        delay 2 ticks (8 ms)
06FF:  24 66 01     AY1_C_PERIOD = $0166
0702:  0A 09        AY1_C_VOL = $09
0704:  F1 44        delay 68 ticks (272 ms)
0706:  0A 00        AY1_C_VOL = $00
0708:  F1 02        delay 2 ticks (8 ms)
070A:  24 38 02     AY1_C_PERIOD = $0238
070D:  0A 09        AY1_C_VOL = $09
070F:  F1 8C        delay 140 ticks (560 ms)
0711:  0A 00        AY1_C_VOL = $00
0713:  F1 02        delay 2 ticks (8 ms)
0715:  24 DE 01     AY1_C_PERIOD = $01DE
0718:  0A 09        AY1_C_VOL = $09
071A:  F1 44        delay 68 ticks (272 ms)
071C:  0A 00        AY1_C_VOL = $00
071E:  F1 02        delay 2 ticks (8 ms)
0720:  F1 13        delay 19 ticks (76 ms)
0722:  24 DE 01     AY1_C_PERIOD = $01DE
0725:  0A 09        AY1_C_VOL = $09
0727:  F1 D4        delay 212 ticks (848 ms)
0729:  0A 00        AY1_C_VOL = $00
072B:  F1 02        delay 2 ticks (8 ms)
072D:  FF           end of script

; ==== script 07: ship fire  (vars $10E0) ====
072E:  F5 12        kill script 12 (coin chime)
0730:  00 25        AY1_A_PER_LO = $25
0732:  01 00        AY1_A_PER_HI = $00
0734:  F2 04 00     repeat 4 times {
0737:  67 FE        AY1_MIXER &= $FE
0739:  08 0B        AY1_A_VOL = $0B
073B:  80 10        AY1_A_PER_LO += $10
073D:  F1 01        delay 1 ticks (4 ms)
073F:  80 FE        AY1_A_PER_LO += $FE
0741:  F1 04        delay 4 ticks (16 ms)
0743:  F3           } loop back while count remains
0744:  00 25        AY1_A_PER_LO = $25
0746:  01 00        AY1_A_PER_HI = $00
0748:  F2 0C 00     repeat 12 times {
074B:  80 10        AY1_A_PER_LO += $10
074D:  F1 01        delay 1 ticks (4 ms)
074F:  80 FE        AY1_A_PER_LO += $FE
0751:  F1 04        delay 4 ticks (16 ms)
0753:  F3           } loop back while count remains
0754:  47 01        AY1_MIXER |= $01
0756:  08 00        AY1_A_VOL = $00
0758:  FF           end of script

; ==== script 01: player ship explosion  (vars $1080) ====
0759:  07 3F        AY1_MIXER = $3F
075B:  17 3F        AY2_MIXER = $3F
075D:  06 1F        AY1_NOISE_PER = $1F
075F:  16 1F        AY2_NOISE_PER = $1F
0761:  2B 00 02     AY1_ENV_PERIOD = $0200
0764:  3B 00 02     AY2_ENV_PERIOD = $0200
0767:  08 00        AY1_A_VOL = $00
0769:  18 00        AY2_A_VOL = $00
076B:  0A 00        AY1_C_VOL = $00
076D:  1A 00        AY2_C_VOL = $00
076F:  09 10        AY1_B_VOL = $10
0771:  19 10        AY2_B_VOL = $10
0773:  67 EF        AY1_MIXER &= $EF
0775:  77 EF        AY2_MIXER &= $EF
0777:  0D 00        AY1_ENV_SHAPE = $00  (mirror forced: retriggers envelope)
0779:  1D 00        AY2_ENV_SHAPE = $00  (mirror forced: retriggers envelope)
077B:  F1 13        delay 19 ticks (76 ms)
077D:  09 00        AY1_B_VOL = $00
077F:  19 00        AY2_B_VOL = $00
0781:  08 10        AY1_A_VOL = $10
0783:  18 10        AY2_A_VOL = $10
0785:  47 10        AY1_MIXER |= $10
0787:  57 10        AY2_MIXER |= $10
0789:  67 F7        AY1_MIXER &= $F7
078B:  77 F7        AY2_MIXER &= $F7
078D:  0D 00        AY1_ENV_SHAPE = $00  (mirror forced: retriggers envelope)
078F:  1D 00        AY2_ENV_SHAPE = $00  (mirror forced: retriggers envelope)
0791:  F1 13        delay 19 ticks (76 ms)
0793:  08 00        AY1_A_VOL = $00
0795:  18 00        AY2_A_VOL = $00
0797:  09 10        AY1_B_VOL = $10
0799:  19 10        AY2_B_VOL = $10
079B:  47 08        AY1_MIXER |= $08
079D:  57 08        AY2_MIXER |= $08
079F:  67 EF        AY1_MIXER &= $EF
07A1:  77 EF        AY2_MIXER &= $EF
07A3:  2B 00 20     AY1_ENV_PERIOD = $2000
07A6:  3B 00 20     AY2_ENV_PERIOD = $2000
07A9:  0D 00        AY1_ENV_SHAPE = $00  (mirror forced: retriggers envelope)
07AB:  1D 00        AY2_ENV_SHAPE = $00  (mirror forced: retriggers envelope)
07AD:  F1 C7        delay 199 ticks (796 ms)
07AF:  F1 C7        delay 199 ticks (796 ms)
07B1:  F4 00        start script 00 (all sound off)
07B3:  FF           end of script

; ==== script 00: all sound off  (vars $1070) ====
07B4:  F5 01        kill script 01 (player ship explosion)
07B6:  F5 16        kill script 16 (self-test sweep)
07B8:  F5 15        kill script 15 (droid explosion (default tier))
07BA:  F5 02        kill script 02 (droid explosion (kill_tier 3))
07BC:  F5 03        kill script 03 (command-ship explosion (kill_tier 1))
07BE:  F5 04        kill script 04 (wave-complete tune)
07C0:  F5 05        kill script 05 (tune harmony voice A (internal))
07C2:  F5 06        kill script 06 (tune harmony voice B (internal))
07C4:  F5 07        kill script 07 (ship fire)
07C6:  F5 08        kill script 08 (wall hit)
07C8:  F5 09        kill script 09 (thrust on)
07CA:  F5 0B        kill script 0B (heartbeat tempo 1 (slowest))
07CC:  F5 0C        kill script 0C (heartbeat tempo 2)
07CE:  F5 0D        kill script 0D (heartbeat tempo 3)
07D0:  F5 0E        kill script 0E (heartbeat tempo 4)
07D2:  F5 0F        kill script 0F (heartbeat tempo 5 (fastest))
07D4:  F5 10        kill script 10 (droid explosion (kill_tier 4))
07D6:  F5 11        kill script 11 (command-ship explosion (kill_tier != 1))
07D8:  F5 12        kill script 12 (coin chime)
07DA:  20 00 00     AY1_A_PERIOD = $0000
07DD:  22 00 00     AY1_B_PERIOD = $0000
07E0:  24 00 00     AY1_C_PERIOD = $0000
07E3:  26 00 3F     AY1_NOISE_PER/+1 = $3F00
07E6:  28 00 00     AY1_A_VOL/+1 = $0000
07E9:  2A 00 00     AY1_C_VOL/+1 = $0000
07EC:  2C 00 00     AY1_ENV_PER_HI/+1 = $0000
07EF:  30 00 00     AY2_A_PERIOD = $0000
07F2:  32 00 00     AY2_B_PERIOD = $0000
07F5:  34 00 00     AY2_C_PERIOD = $0000
07F8:  36 00 3F     AY2_NOISE_PER/+1 = $3F00
07FB:  38 00 00     AY2_A_VOL/+1 = $0000
07FE:  3A 00        (truncated: operand crosses the end of ROM; the next fetch at $0800 is unmapped)
