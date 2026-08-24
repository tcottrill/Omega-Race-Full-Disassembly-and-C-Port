; omega_dump.bin  traced disassembly (z80trace.py)
;

RESET:
0000:  F3           di                          
0001:  ED 56        im 1                        
0003:  31 FC 4B     ld sp,$4BFC                 
0006:  DB 09        in a,($09)                   ; WATCHDOG
0008:  3E FF        ld a,$FF                    
000A:  D3 13        out ($13),a                  ; LEDS/coin counters/flip
000C:  01 FE 07     ld bc,$07FE                 
000F:  11 02 80     ld de,$8002                 
0012:  21 01 80     ld hl,$8001                 
0015:  36 B0        ld (hl),$B0                 
0017:  2B           dec hl                      
0018:  36 00        ld (hl),$00                 
001A:  ED B0        ldir                        
001C:  01 80 01     ld bc,$0180                 
001F:  11 81 44     ld de,dlist_shadow           ; dlist_shadow
0022:  21 00 80     ld hl,vector_ram             ; vector_ram
0025:  ED B0        ldir                        
0027:  01 30 00     ld bc,$0030                 
002A:  11 01 46     ld de,$4601                 
002D:  21 D9 30     ld hl,$30D9                 
0030:  ED B0        ldir                        
0032:  C3 E8 04     jp POST_START                ; -> POST_START
0035:  db  FF FF FF                                        ; ...

IRQ_244HZ:
0038:  F5           push af                     
0039:  C5           push bc                     
003A:  FD 34 97     inc (iy-105)                
003D:  FD 4E B5     ld c,(iy-75)                
0040:  DB 11        in a,($11)                   ; IN P1/coin/tilt/test
0042:  CB 67        bit 4,a                     
0044:  20 36        jr nz,irq_coin_edge_detect   ; -> irq_coin_edge_detect
0046:  3A 82 40     ld a,(slam_latch)            ; slam_latch
0049:  B7           or a                        
004A:  20 30        jr nz,irq_coin_edge_detect   ; -> irq_coin_edge_detect
004C:  3E 13        ld a,$13                    
004E:  D3 14        out ($14),a                  ; SOUND CMD latch
0050:  32 82 40     ld (slam_latch),a            ; slam_latch
0053:  3E 00        ld a,$00                    
0055:  32 25 40     ld (coin1_batch),a           ; coin1_batch
0058:  32 26 40     ld (coin2_batch),a           ; coin2_batch
005B:  18 0B        jr irq_slam_credit_penalty   ; -> irq_slam_credit_penalty
005D:  db  FF FF FF FF FF FF FF FF FF                      ; .........

IRQ_244HZ_0066:
0066:  ED 45        retn                        

irq_slam_credit_penalty:
0068:  3A 2D 40     ld a,(credits_bcd)           ; credits_bcd
006B:  B7           or a                        
006C:  28 0A        jr z,irq_slam_credit_penalty_0078
006E:  D6 01        sub $01                     
0070:  27           daa                         
0071:  32 2D 40     ld (credits_bcd),a           ; credits_bcd
0074:  FD CB AE F6  set 6,(iy-82)               

irq_slam_credit_penalty_0078:
0078:  FD 36 A1 03  ld (iy-95),$03              

irq_coin_edge_detect:
007C:  DB 11        in a,($11)                   ; IN P1/coin/tilt/test
007E:  E6 03        and $03                     
0080:  32 35 40     ld (coin_lines),a            ; coin_lines
0083:  47           ld b,a                      
0084:  A9           xor c                       
0085:  28 10        jr z,irq_coin1_accept        ; -> irq_coin1_accept
0087:  CB 47        bit 0,a                     
0089:  28 04        jr z,irq_coin_edge_detect_008F
008B:  FD 36 9B 05  ld (iy-101),$05             

irq_coin_edge_detect_008F:
008F:  CB 4F        bit 1,a                     
0091:  28 04        jr z,irq_coin1_accept        ; -> irq_coin1_accept
0093:  FD 36 9C 05  ld (iy-100),$05             

irq_coin1_accept:
0097:  3A 1B 40     ld a,(coin1_debounce)        ; coin1_debounce
009A:  B7           or a                        
009B:  28 4D        jr z,irq_coin2_accept        ; -> irq_coin2_accept
009D:  FD 35 9B     dec (iy-101)                
00A0:  20 48        jr nz,irq_coin2_accept       ; -> irq_coin2_accept
00A2:  CB 40        bit 0,b                     
00A4:  20 44        jr nz,irq_coin2_accept       ; -> irq_coin2_accept
00A6:  DB 11        in a,($11)                   ; IN P1/coin/tilt/test
00A8:  CB 7F        bit 7,a                     
00AA:  28 08        jr z,irq_coin1_accept_00B4  
00AC:  3E 00        ld a,$00                    
00AE:  CD 18 15     call BOOKKEEP_COIN           ; -> BOOKKEEP_COIN
00B1:  FD 34 9D     inc (iy-99)                 

irq_coin1_accept_00B4:
00B4:  3E 12        ld a,$12                    
00B6:  D3 14        out ($14),a                  ; SOUND CMD latch
00B8:  FD 34 A5     inc (iy-91)                 
00BB:  3A 25 40     ld a,(coin1_batch)           ; coin1_batch
00BE:  E5           push hl                     
00BF:  2A 27 40     ld hl,(coinage1_ptr)         ; coinage1_ptr
00C2:  BE           cp (hl)                     
00C3:  38 24        jr c,irq_coin1_accept_00E9  
00C5:  FD 36 A5 00  ld (iy-91),$00              
00C9:  23           inc hl                      
00CA:  DB 11        in a,($11)                   ; IN P1/coin/tilt/test
00CC:  CB 7F        bit 7,a                     
00CE:  20 05        jr nz,irq_coin1_accept_00D5 
00D0:  3E 02        ld a,$02                    
00D2:  CD 21 15     call BOOKKEEP_ADD            ; -> BOOKKEEP_ADD

irq_coin1_accept_00D5:
00D5:  3A 2D 40     ld a,(credits_bcd)           ; credits_bcd
00D8:  86           add a,(hl)                  
00D9:  27           daa                         
00DA:  32 2D 40     ld (credits_bcd),a           ; credits_bcd
00DD:  FD CB AE F6  set 6,(iy-82)               
00E1:  FE 21        cp $21                      
00E3:  38 04        jr c,irq_coin1_accept_00E9  
00E5:  FD 36 AD 20  ld (iy-83),$20              

irq_coin1_accept_00E9:
00E9:  E1           pop hl                      

irq_coin2_accept:
00EA:  3A 1C 40     ld a,(coin2_debounce)        ; coin2_debounce
00ED:  B7           or a                        
00EE:  28 4D        jr z,irq_meter_pulse         ; -> irq_meter_pulse
00F0:  FD 35 9C     dec (iy-100)                
00F3:  20 48        jr nz,irq_meter_pulse        ; -> irq_meter_pulse
00F5:  CB 48        bit 1,b                     
00F7:  20 44        jr nz,irq_meter_pulse        ; -> irq_meter_pulse
00F9:  DB 11        in a,($11)                   ; IN P1/coin/tilt/test
00FB:  CB 7F        bit 7,a                     
00FD:  28 08        jr z,irq_coin2_accept_0107  
00FF:  3E 01        ld a,$01                    
0101:  CD 18 15     call BOOKKEEP_COIN           ; -> BOOKKEEP_COIN
0104:  FD 34 9E     inc (iy-98)                 

irq_coin2_accept_0107:
0107:  3E 12        ld a,$12                    
0109:  D3 14        out ($14),a                  ; SOUND CMD latch
010B:  FD 34 A6     inc (iy-90)                 
010E:  3A 26 40     ld a,(coin2_batch)           ; coin2_batch
0111:  E5           push hl                     
0112:  2A 29 40     ld hl,(coinage2_ptr)         ; coinage2_ptr
0115:  BE           cp (hl)                     
0116:  38 24        jr c,irq_coin2_accept_013C  
0118:  FD 36 A6 00  ld (iy-90),$00              
011C:  23           inc hl                      
011D:  DB 11        in a,($11)                   ; IN P1/coin/tilt/test
011F:  CB 7F        bit 7,a                     
0121:  20 05        jr nz,irq_coin2_accept_0128 
0123:  3E 02        ld a,$02                    
0125:  CD 21 15     call BOOKKEEP_ADD            ; -> BOOKKEEP_ADD

irq_coin2_accept_0128:
0128:  3A 2D 40     ld a,(credits_bcd)           ; credits_bcd
012B:  86           add a,(hl)                  
012C:  27           daa                         
012D:  32 2D 40     ld (credits_bcd),a           ; credits_bcd
0130:  FD CB AE F6  set 6,(iy-82)               
0134:  FE 21        cp $21                      
0136:  38 04        jr c,irq_coin2_accept_013C  
0138:  FD 36 AD 20  ld (iy-83),$20              

irq_coin2_accept_013C:
013C:  E1           pop hl                      

irq_meter_pulse:
013D:  3A 1F 40     ld a,(meter_pulse_t)         ; meter_pulse_t
0140:  B7           or a                        
0141:  28 16        jr z,irq_meter1_start        ; -> irq_meter1_start
0143:  3D           dec a                       
0144:  32 1F 40     ld (meter_pulse_t),a         ; meter_pulse_t
0147:  28 10        jr z,irq_meter1_start        ; -> irq_meter1_start
0149:  FE 19        cp $19                      
014B:  20 4E        jr nz,irq_exit               ; -> irq_exit
014D:  3A 72 40     ld a,(led_shadow)            ; led_shadow
0150:  F6 03        or $03                      
0152:  D3 13        out ($13),a                  ; LEDS/coin counters/flip
0154:  32 72 40     ld (led_shadow),a            ; led_shadow
0157:  18 42        jr irq_exit                  ; -> irq_exit

irq_meter1_start:
0159:  3A 1D 40     ld a,(coin1_pending)         ; coin1_pending
015C:  B7           or a                        
015D:  28 0B        jr z,irq_meter2_start        ; -> irq_meter2_start
015F:  FD 35 9D     dec (iy-99)                 
0162:  FD CB F2 86  res 0,(iy-14)               
0166:  FD 36 9F 32  ld (iy-97),$32              

irq_meter2_start:
016A:  3A 1E 40     ld a,(coin2_pending)         ; coin2_pending
016D:  B7           or a                        
016E:  28 1F        jr z,irq_meter_release       ; -> irq_meter_release
0170:  3A 2B 40     ld a,(coin_mode1)            ; coin_mode1
0173:  FD BE AC     cp (iy-84)                  
0176:  20 0C        jr nz,irq_meter2_start_0184 
0178:  FD CB F2 46  bit 0,(iy-14)               
017C:  28 11        jr z,irq_meter_release       ; -> irq_meter_release
017E:  FD CB F2 86  res 0,(iy-14)               
0182:  18 04        jr irq_meter2_start_0188    

irq_meter2_start_0184:
0184:  FD CB F2 8E  res 1,(iy-14)               

irq_meter2_start_0188:
0188:  FD 35 9E     dec (iy-98)                 
018B:  FD 36 9F 32  ld (iy-97),$32              

irq_meter_release:
018F:  3A 1F 40     ld a,(meter_pulse_t)         ; meter_pulse_t
0192:  FE 32        cp $32                      
0194:  20 05        jr nz,irq_exit               ; -> irq_exit
0196:  3A 72 40     ld a,(led_shadow)            ; led_shadow
0199:  D3 13        out ($13),a                  ; LEDS/coin counters/flip

irq_exit:
019B:  C1           pop bc                      
019C:  F1           pop af                      
019D:  FB           ei                          
019E:  ED 4D        reti                        

; ---- 01A0-01CA: POST screen display-list header (defines 4-glyph subroutines at JSRL 005='  OK' 00A=' NG ' 00F=blank; each ends in a shared newline vector) ----
01A0:  dw  A320 11A0 8000 0000 E016 CD92 CD92 CCC2
01B0:  dw  CCA3 E013 CD92 CCBA CC7F CD92 E013 CD92
01C0:  dw  CD92 CD92 CD92 744F 06FF
01CA:  dw  
01CB:  db  D0

; ---- 01CB-026D: POST test-line labels (trailing '.' = JSRL 00F blank+newline; result posting overwrites it with 005/00A) ----
01CB:  text "\xD0P ROM 1 .P ROM 2 .P ROM 3 .P RO"
01EB:  text "M 4 .        .P RAM 1 .P RAM 2 ."
020B:  text "P RAM 3 .        .BBU RAM .     "
022B:  text "   .V RAM 1 .V RAM 2 .V RAM 3 .V"
024B:  text " RAM 4 .        .V ROM 1 .V ROM "
026B:  text "2 ."

; ---- 026E-02C5: input-test page display list ----
026E:  dw  A384 01A0 9000 0000 E016 CD92 CCAB CCC2
027E:  dw  CD0B E013 CD92 CC88 CC91 CD92 E013 CD92
028E:  dw  CD92 CD92 CD92 743F 06FF D000 4F43 4E49
029E:  dw  3120 2020 432E 494F 204E 2032 2E20 4954
02AE:  dw  544C 2020 2020 502E 2031 4854 5552 2E53
02BE:  dw  3150 4620 5249 2045

; ---- 02C6-042E: input/DSW test labels ----
02C6:  text ".TEST    .        .P2 THRUS.P2 F"
02E6:  text "IRE .2 P 1 CR.2 P 2 CR.1 P 1 CR."
0306:  text "1 P 2 CR.        .ANGLE1 2.ANGLE"
0326:  text "1 3.ANGLE1 4.ANGLE1 5.ANGLE1 6.A"
0346:  text "NGLE1 7.        .ANGLE2 0.ANGLE2"
0366:  text " 1.ANGLE2 2.ANGLE2 3.ANGLE2 4.AN"
0386:  text "GLE2 5.        .DIPSW1 0.DIPSW1 "
03A6:  text "1.DIPSW1 2.DIPSW1 3.DIPSW1 4.DIP"
03C6:  text "SW1 5.DIPSW1 6.DIPSW1 7.        "
03E6:  text ".DIPSW2 0.DIPSW2 1.DIPSW2 2.DIPS"
0406:  text "W2 3.DIPSW2 4.DIPSW2 5.DIPSW2 6."
0426:  text "DIPSW2 7."

; ---- 042F-0446: cross-hatch page display list ----
042F:  dw  A200 0000 8000 0000 83FD C3FF 87FD C3FD
043F:  dw  87FF C7FD 83FF C7FF

; ---- 0447-04E7: grid page display list ----
0447:  dw  A000 0000 9000 0000 93FE C000 9000 C3FE
0457:  dw  97FE C000 9000 C7FE 93FE C3FE 97FE 0000
0467:  dw  93FE C7FE A2BC 01BA 8000 0000 571F 0000
0477:  dw  531F 009F 571F 1000 531F 009F 571F 2000
0487:  dw  531F 009F 571F 3000 531F 009F 571F 4000
0497:  dw  531F 009F 571F 5000 531F 009F 571F 6000
04A7:  dw  531F 009F 571F 7000 531F 009F 571F 8000
04B7:  dw  531F 009F 571F 9000 531F 009F 571F A000
04C7:  dw  531F 009F 571F B000 531F 009F 571F C000
04D7:  dw  531F 009F 571F E000 531F 009F 571F F000
04E7:  dw  
04E8:  db  3E

POST_START:
04E8:  3E 00        ld a,$00                    
04EA:  D3 14        out ($14),a                  ; SOUND CMD latch

POST_CYCLE:
04EC:  DB 09        in a,($09)                   ; WATCHDOG
04EE:  DD 21 F5 04  ld ix,$04F5                  ; post_draw_header
04F2:  C3 B6 07     jp VG_WAIT_WIPE              ; -> VG_WAIT_WIPE

post_draw_header:
04F5:  01 2C 00     ld bc,$002C                 
04F8:  11 00 80     ld de,vector_ram             ; vector_ram
04FB:  21 A0 01     ld hl,$01A0                 
04FE:  ED B0        ldir                        
0500:  01 A2 00     ld bc,$00A2                 
0503:  DD 21 0A 05  ld ix,$050A                  ; POST_TESTSW_GATE
0507:  C3 C8 08     jp DRAW_TEXT                 ; -> DRAW_TEXT

POST_TESTSW_GATE:
050A:  DB 11        in a,($11)                   ; IN P1/coin/tilt/test
050C:  CB 7F        bit 7,a                     
050E:  C2 55 09     jp nz,GAME_INIT              ; -> GAME_INIT
0511:  01 00 00     ld bc,$0000                  ; RESET
0514:  DB 11        in a,($11)                   ; IN P1/coin/tilt/test
0516:  57           ld d,a                      
0517:  1E 00        ld e,$00                    
0519:  D9           exx                         
051A:  01 FF 0B     ld bc,$0BFF                 
051D:  11 01 40     ld de,obj_flags1             ; obj_flags1
0520:  21 00 40     ld hl,obj_flags0             ; obj_flags0
0523:  36 FF        ld (hl),$FF                 
0525:  ED B0        ldir                        
0527:  DB 09        in a,($09)                   ; WATCHDOG
0529:  01 8B 0E     ld bc,$0E8B                 
052C:  11 75 81     ld de,$8175                 
052F:  21 74 81     ld hl,$8174                 
0532:  36 FF        ld (hl),$FF                 
0534:  ED B0        ldir                        
0536:  DB 09        in a,($09)                   ; WATCHDOG
0538:  01 00 10     ld bc,$1000                 
053B:  11 3C 80     ld de,$803C                 
053E:  21 00 00     ld hl,$0000                  ; RESET
0541:  DD 21 48 05  ld ix,$0548                  ; post_rom1_result
0545:  C3 E9 07     jp POST_CSUM_BLOCK           ; -> POST_CSUM_BLOCK

post_rom1_result:
0548:  28 09        jr z,post_csum_rom2          ; -> post_csum_rom2
054A:  06 01        ld b,$01                    
054C:  DD 21 53 05  ld ix,$0553                  ; post_csum_rom2
0550:  C3 17 09     jp POST_FAIL_BLINK           ; -> POST_FAIL_BLINK

post_csum_rom2:
0553:  01 00 10     ld bc,$1000                 
0556:  21 00 10     ld hl,$1000                 
0559:  DD 21 60 05  ld ix,$0560                  ; post_rom2_result
055D:  C3 E9 07     jp POST_CSUM_BLOCK           ; -> POST_CSUM_BLOCK

post_rom2_result:
0560:  28 09        jr z,post_csum_rom3          ; -> post_csum_rom3
0562:  06 02        ld b,$02                    
0564:  DD 21 6B 05  ld ix,$056B                  ; post_csum_rom3
0568:  C3 17 09     jp POST_FAIL_BLINK           ; -> POST_FAIL_BLINK

post_csum_rom3:
056B:  01 00 10     ld bc,$1000                 
056E:  21 00 20     ld hl,$2000                 
0571:  DD 21 78 05  ld ix,$0578                  ; post_rom3_result
0575:  C3 E9 07     jp POST_CSUM_BLOCK           ; -> POST_CSUM_BLOCK

post_rom3_result:
0578:  28 09        jr z,post_csum_rom4          ; -> post_csum_rom4
057A:  06 03        ld b,$03                    
057C:  DD 21 83 05  ld ix,$0583                  ; post_csum_rom4
0580:  C3 17 09     jp POST_FAIL_BLINK           ; -> POST_FAIL_BLINK

post_csum_rom4:
0583:  01 00 10     ld bc,$1000                 
0586:  21 00 30     ld hl,$3000                 
0589:  DD 21 90 05  ld ix,$0590                  ; post_rom4_result
058D:  C3 E9 07     jp POST_CSUM_BLOCK           ; -> POST_CSUM_BLOCK

post_rom4_result:
0590:  28 09        jr z,post_ramtest_4000       ; -> post_ramtest_4000
0592:  06 04        ld b,$04                    
0594:  DD 21 9B 05  ld ix,$059B                  ; post_ramtest_4000
0598:  C3 17 09     jp POST_FAIL_BLINK           ; -> POST_FAIL_BLINK

post_ramtest_4000:
059B:  21 12 00     ld hl,$0012                 
059E:  19           add hl,de                   
059F:  EB           ex de,hl                    
05A0:  01 00 04     ld bc,$0400                 
05A3:  21 00 40     ld hl,obj_flags0             ; obj_flags0
05A6:  DD 21 AD 05  ld ix,$05AD                  ; post_ram1_result
05AA:  C3 1D 08     jp POST_RAMTEST              ; -> POST_RAMTEST

post_ram1_result:
05AD:  28 09        jr z,post_ramtest_4400       ; -> post_ramtest_4400
05AF:  06 05        ld b,$05                    
05B1:  DD 21 B8 05  ld ix,$05B8                  ; post_ramtest_4400
05B5:  C3 17 09     jp POST_FAIL_BLINK           ; -> POST_FAIL_BLINK

post_ramtest_4400:
05B8:  01 00 04     ld bc,$0400                 
05BB:  21 00 44     ld hl,$4400                 
05BE:  DD 21 C5 05  ld ix,$05C5                  ; post_ram2_result
05C2:  C3 1D 08     jp POST_RAMTEST              ; -> POST_RAMTEST

post_ram2_result:
05C5:  28 09        jr z,post_ramtest_4800       ; -> post_ramtest_4800
05C7:  06 06        ld b,$06                    
05C9:  DD 21 D0 05  ld ix,$05D0                  ; post_ramtest_4800
05CD:  C3 17 09     jp POST_FAIL_BLINK           ; -> POST_FAIL_BLINK

post_ramtest_4800:
05D0:  01 00 04     ld bc,$0400                 
05D3:  21 00 48     ld hl,$4800                 
05D6:  DD 21 DD 05  ld ix,$05DD                  ; post_ram3_result
05DA:  C3 1D 08     jp POST_RAMTEST              ; -> POST_RAMTEST

post_ram3_result:
05DD:  28 09        jr z,post_nvram_test         ; -> post_nvram_test
05DF:  06 07        ld b,$07                    
05E1:  DD 21 E8 05  ld ix,$05E8                  ; post_nvram_test
05E5:  C3 17 09     jp POST_FAIL_BLINK           ; -> POST_FAIL_BLINK

post_nvram_test:
05E8:  21 12 00     ld hl,$0012                 
05EB:  19           add hl,de                   
05EC:  EB           ex de,hl                    
05ED:  01 1A 00     ld bc,$001A                 
05F0:  21 00 5C     ld hl,nvram                  ; nvram
05F3:  DD 21 FA 05  ld ix,$05FA                  ; post_nvram_result
05F7:  C3 63 08     jp POST_NVRAMTEST            ; -> POST_NVRAMTEST

post_nvram_result:
05FA:  28 09        jr z,post_vramtest_835A      ; -> post_vramtest_835A
05FC:  06 08        ld b,$08                    
05FE:  DD 21 05 06  ld ix,$0605                  ; post_vramtest_835A
0602:  C3 17 09     jp POST_FAIL_BLINK           ; -> POST_FAIL_BLINK

post_vramtest_835A:
0605:  21 12 00     ld hl,$0012                 
0608:  19           add hl,de                   
0609:  EB           ex de,hl                    
060A:  01 A6 00     ld bc,$00A6                 
060D:  21 5A 83     ld hl,$835A                 
0610:  DD 21 17 06  ld ix,$0617                  ; post_vram1_result
0614:  C3 1D 08     jp POST_RAMTEST              ; -> POST_RAMTEST

post_vram1_result:
0617:  28 09        jr z,post_vramtest_8400      ; -> post_vramtest_8400
0619:  06 09        ld b,$09                    
061B:  DD 21 22 06  ld ix,$0622                  ; post_vramtest_8400
061F:  C3 17 09     jp POST_FAIL_BLINK           ; -> POST_FAIL_BLINK

post_vramtest_8400:
0622:  01 00 04     ld bc,$0400                 
0625:  21 00 84     ld hl,$8400                 
0628:  DD 21 2F 06  ld ix,$062F                  ; post_vram2_result
062C:  C3 1D 08     jp POST_RAMTEST              ; -> POST_RAMTEST

post_vram2_result:
062F:  28 09        jr z,post_vramtest_8800      ; -> post_vramtest_8800
0631:  06 0A        ld b,$0A                    
0633:  DD 21 3A 06  ld ix,$063A                  ; post_vramtest_8800
0637:  C3 17 09     jp POST_FAIL_BLINK           ; -> POST_FAIL_BLINK

post_vramtest_8800:
063A:  01 00 04     ld bc,$0400                 
063D:  21 00 88     ld hl,$8800                 
0640:  DD 21 47 06  ld ix,$0647                  ; post_vram3_result
0644:  C3 1D 08     jp POST_RAMTEST              ; -> POST_RAMTEST

post_vram3_result:
0647:  28 09        jr z,post_vramtest_8C00      ; -> post_vramtest_8C00
0649:  06 0B        ld b,$0B                    
064B:  DD 21 52 06  ld ix,$0652                  ; post_vramtest_8C00
064F:  C3 17 09     jp POST_FAIL_BLINK           ; -> POST_FAIL_BLINK

post_vramtest_8C00:
0652:  01 00 04     ld bc,$0400                 
0655:  21 00 8C     ld hl,$8C00                 
0658:  DD 21 5F 06  ld ix,$065F                  ; post_vram4_result
065C:  C3 1D 08     jp POST_RAMTEST              ; -> POST_RAMTEST

post_vram4_result:
065F:  28 09        jr z,post_csum_vrom1         ; -> post_csum_vrom1
0661:  06 0C        ld b,$0C                    
0663:  DD 21 6A 06  ld ix,$066A                  ; post_csum_vrom1
0667:  C3 17 09     jp POST_FAIL_BLINK           ; -> POST_FAIL_BLINK

post_csum_vrom1:
066A:  21 12 00     ld hl,$0012                 
066D:  19           add hl,de                   
066E:  EB           ex de,hl                    
066F:  01 00 08     ld bc,$0800                 
0672:  21 00 90     ld hl,$9000                 
0675:  DD 21 7C 06  ld ix,$067C                  ; post_vrom1_result
0679:  C3 E9 07     jp POST_CSUM_BLOCK           ; -> POST_CSUM_BLOCK

post_vrom1_result:
067C:  28 09        jr z,post_csum_vrom2         ; -> post_csum_vrom2
067E:  06 0D        ld b,$0D                    
0680:  DD 21 87 06  ld ix,$0687                  ; post_csum_vrom2
0684:  C3 17 09     jp POST_FAIL_BLINK           ; -> POST_FAIL_BLINK

post_csum_vrom2:
0687:  01 00 08     ld bc,$0800                 
068A:  21 00 98     ld hl,$9800                 
068D:  DD 21 94 06  ld ix,$0694                  ; post_vrom2_result
0691:  C3 E9 07     jp POST_CSUM_BLOCK           ; -> POST_CSUM_BLOCK

post_vrom2_result:
0694:  28 09        jr z,post_wait_fire          ; -> post_wait_fire
0696:  06 0E        ld b,$0E                    
0698:  DD 21 9F 06  ld ix,$069F                  ; post_wait_fire
069C:  C3 17 09     jp POST_FAIL_BLINK           ; -> POST_FAIL_BLINK

post_wait_fire:
069F:  DD 21 A6 06  ld ix,$06A6                  ; post_input_page
06A3:  C3 DD 07     jp EDGE_BIT6_IX              ; -> EDGE_BIT6_IX

post_input_page:
06A6:  CA 0A 05     jp z,POST_TESTSW_GATE        ; -> POST_TESTSW_GATE
06A9:  DD 21 B0 06  ld ix,$06B0                  ; post_input_draw
06AD:  C3 B6 07     jp VG_WAIT_WIPE              ; -> VG_WAIT_WIPE

post_input_draw:
06B0:  01 2C 00     ld bc,$002C                 
06B3:  11 00 80     ld de,vector_ram             ; vector_ram
06B6:  21 6E 02     ld hl,$026E                 
06B9:  ED B0        ldir                        
06BB:  DB 09        in a,($09)                   ; WATCHDOG
06BD:  01 95 01     ld bc,$0195                 
06C0:  DD 21 C7 06  ld ix,$06C7                 
06C4:  C3 C8 08     jp DRAW_TEXT                 ; -> DRAW_TEXT

post_input_draw_06C7:
06C7:  DD 21 CE 06  ld ix,$06CE                  ; post_input_loop
06CB:  C3 DD 07     jp EDGE_BIT6_IX              ; -> EDGE_BIT6_IX

post_input_loop:
06CE:  11 3C 80     ld de,$803C                 
06D1:  DB 11        in a,($11)                   ; IN P1/coin/tilt/test
06D3:  E6 03        and $03                     
06D5:  4F           ld c,a                      
06D6:  DB 11        in a,($11)                   ; IN P1/coin/tilt/test
06D8:  E6 F0        and $F0                     
06DA:  0F           rrca                        
06DB:  0F           rrca                        
06DC:  B1           or c                        
06DD:  06 06        ld b,$06                    
06DF:  DD 21 E6 06  ld ix,$06E6                  ; post_show_p2_bits
06E3:  C3 A0 08     jp POST_SHOW_BITS            ; -> POST_SHOW_BITS

post_show_p2_bits:
06E6:  DB 12        in a,($12)                   ; IN P2/start
06E8:  E6 0F        and $0F                     
06EA:  4F           ld c,a                      
06EB:  DB 12        in a,($12)                   ; IN P2/start
06ED:  E6 C0        and $C0                     
06EF:  0F           rrca                        
06F0:  0F           rrca                        
06F1:  B1           or c                        
06F2:  06 06        ld b,$06                    
06F4:  DD 21 FB 06  ld ix,$06FB                  ; post_show_spinner1
06F8:  C3 A0 08     jp POST_SHOW_BITS            ; -> POST_SHOW_BITS

post_show_spinner1:
06FB:  DB 15        in a,($15)                   ; SPINNER 1
06FD:  0F           rrca                        
06FE:  0F           rrca                        
06FF:  E6 3F        and $3F                     
0701:  06 06        ld b,$06                    
0703:  DD 21 0A 07  ld ix,$070A                  ; post_show_spinner2
0707:  C3 A0 08     jp POST_SHOW_BITS            ; -> POST_SHOW_BITS

post_show_spinner2:
070A:  DB 16        in a,($16)                   ; SPINNER 2
070C:  E6 3F        and $3F                     
070E:  06 06        ld b,$06                    
0710:  DD 21 17 07  ld ix,$0717                  ; post_show_dsw_c4
0714:  C3 A0 08     jp POST_SHOW_BITS            ; -> POST_SHOW_BITS

post_show_dsw_c4:
0717:  DB 10        in a,($10)                   ; DSW C4
0719:  06 08        ld b,$08                    
071B:  DD 21 22 07  ld ix,$0722                  ; post_show_dsw_c6
071F:  C3 A0 08     jp POST_SHOW_BITS            ; -> POST_SHOW_BITS

post_show_dsw_c6:
0722:  DB 17        in a,($17)                   ; DSW C6
0724:  06 08        ld b,$08                    
0726:  DD 21 2D 07  ld ix,$072D                 
072A:  C3 A0 08     jp POST_SHOW_BITS            ; -> POST_SHOW_BITS

post_show_dsw_c6_072D:
072D:  DD 21 34 07  ld ix,$0734                  ; post_xhatch_page
0731:  C3 DD 07     jp EDGE_BIT6_IX              ; -> EDGE_BIT6_IX

post_xhatch_page:
0734:  28 98        jr z,post_input_loop         ; -> post_input_loop
0736:  DD 21 3D 07  ld ix,$073D                  ; post_xhatch_draw
073A:  C3 B6 07     jp VG_WAIT_WIPE              ; -> VG_WAIT_WIPE

post_xhatch_draw:
073D:  01 18 00     ld bc,$0018                 
0740:  11 00 80     ld de,vector_ram             ; vector_ram
0743:  21 2F 04     ld hl,$042F                 
0746:  ED B0        ldir                        

post_xhatch_loop:
0748:  FD 21 4F 07  ld iy,$074F                 
074C:  C3 CF 07     jp VG_KICK_IY                ; -> VG_KICK_IY

post_xhatch_loop_074F:
074F:  DD 21 56 07  ld ix,$0756                 
0753:  C3 DD 07     jp EDGE_BIT6_IX              ; -> EDGE_BIT6_IX

post_xhatch_loop_0756:
0756:  28 F0        jr z,post_xhatch_loop        ; -> post_xhatch_loop
0758:  DD 21 5F 07  ld ix,$075F                  ; post_grid_draw
075C:  C3 B6 07     jp VG_WAIT_WIPE              ; -> VG_WAIT_WIPE

post_grid_draw:
075F:  01 A0 00     ld bc,$00A0                 
0762:  11 00 80     ld de,vector_ram             ; vector_ram
0765:  21 47 04     ld hl,$0447                 
0768:  ED B0        ldir                        

post_grid_loop:
076A:  FD 21 71 07  ld iy,$0771                 
076E:  C3 CF 07     jp VG_KICK_IY                ; -> VG_KICK_IY

post_grid_loop_0771:
0771:  DD 21 78 07  ld ix,$0778                 
0775:  C3 DD 07     jp EDGE_BIT6_IX              ; -> EDGE_BIT6_IX

post_grid_loop_0778:
0778:  28 F0        jr z,post_grid_loop          ; -> post_grid_loop
077A:  DD 21 81 07  ld ix,$0781                  ; post_sound_test
077E:  C3 DD 07     jp EDGE_BIT6_IX              ; -> EDGE_BIT6_IX

post_sound_test:
0781:  3E 16        ld a,$16                    
0783:  D3 14        out ($14),a                  ; SOUND CMD latch
0785:  AF           xor a                       

post_sound_loop:
0786:  DD 21 8D 07  ld ix,$078D                 
078A:  C3 B6 07     jp VG_WAIT_WIPE              ; -> VG_WAIT_WIPE

post_sound_loop_078D:
078D:  3D           dec a                       
078E:  20 F6        jr nz,post_sound_loop        ; -> post_sound_loop
0790:  DD 21 97 07  ld ix,$0797                 
0794:  C3 DD 07     jp EDGE_BIT6_IX              ; -> EDGE_BIT6_IX

post_sound_loop_0797:
0797:  28 ED        jr z,post_sound_loop         ; -> post_sound_loop
0799:  3E 00        ld a,$00                    
079B:  D3 14        out ($14),a                  ; SOUND CMD latch
079D:  C3 EC 04     jp POST_CYCLE                ; -> POST_CYCLE

VG_WAIT_IY:
07A0:  D9           exx                         
07A1:  08           ex af,af'                   

VG_WAIT_TIMEOUT:
07A2:  21 00 30     ld hl,$3000                 

VG_WAIT_TIMEOUT_07A5:
07A5:  DB 09        in a,($09)                   ; WATCHDOG
07A7:  DB 0B        in a,($0B)                   ; VG_STATUS (d7=busy)
07A9:  CB 7F        bit 7,a                     
07AB:  28 05        jr z,VG_WAIT_TIMEOUT_07B2   
07AD:  2B           dec hl                      
07AE:  7C           ld a,h                      
07AF:  B5           or l                        
07B0:  20 F3        jr nz,VG_WAIT_TIMEOUT_07A5  

VG_WAIT_TIMEOUT_07B2:
07B2:  08           ex af,af'                   
07B3:  D9           exx                         
07B4:  FD E9        jp (iy)                     

VG_WAIT_WIPE:
07B6:  FD 21 BD 07  ld iy,$07BD                  ; vg_wipe_body
07BA:  C3 A0 07     jp VG_WAIT_IY                ; -> VG_WAIT_IY

vg_wipe_body:
07BD:  01 FE 0F     ld bc,$0FFE                 
07C0:  11 02 80     ld de,$8002                 
07C3:  21 01 80     ld hl,$8001                 
07C6:  36 B0        ld (hl),$B0                 
07C8:  2B           dec hl                      
07C9:  36 00        ld (hl),$00                 
07CB:  ED B0        ldir                        
07CD:  DD E9        jp (ix)                     

VG_KICK_IY:
07CF:  08           ex af,af'                   
07D0:  DB 09        in a,($09)                   ; WATCHDOG
07D2:  DB 0B        in a,($0B)                   ; VG_STATUS (d7=busy)
07D4:  CB 7F        bit 7,a                     
07D6:  20 02        jr nz,VG_KICK_IY_07DA       
07D8:  DB 08        in a,($08)                   ; VG_GO (read starts vector generator)

VG_KICK_IY_07DA:
07DA:  08           ex af,af'                   
07DB:  FD E9        jp (iy)                     

EDGE_BIT6_IX:
07DD:  D9           exx                         
07DE:  DB 11        in a,($11)                   ; IN P1/coin/tilt/test
07E0:  5F           ld e,a                      
07E1:  A2           and d                       
07E2:  AA           xor d                       
07E3:  53           ld d,e                      
07E4:  D9           exx                         
07E5:  CB 77        bit 6,a                     
07E7:  DD E9        jp (ix)                     

POST_CSUM_BLOCK:
07E9:  79           ld a,c                      
07EA:  B7           or a                        
07EB:  28 02        jr z,csum_page_loop          ; -> csum_page_loop
07ED:  04           inc b                       
07EE:  AF           xor a                       

csum_page_loop:
07EF:  CB 7C        bit 7,h                     
07F1:  20 07        jr nz,csum_page_loop_07FA   
07F3:  FD 21 FA 07  ld iy,$07FA                 
07F7:  C3 CF 07     jp VG_KICK_IY                ; -> VG_KICK_IY

csum_page_loop_07FA:
07FA:  86           add a,(hl)                  
07FB:  23           inc hl                      
07FC:  0D           dec c                       
07FD:  20 F0        jr nz,csum_page_loop         ; -> csum_page_loop
07FF:  05           dec b                       
0800:  20 ED        jr nz,csum_page_loop         ; -> csum_page_loop
0802:  FD 21 09 08  ld iy,$0809                  ; csum_result_mark
0806:  C3 A0 07     jp VG_WAIT_IY                ; -> VG_WAIT_IY

csum_result_mark:
0809:  FE FF        cp $FF                      
080B:  28 04        jr z,csum_result_mark_0811  
080D:  3E 0A        ld a,$0A                    
080F:  18 02        jr csum_result_mark_0813    

csum_result_mark_0811:
0811:  3E 05        ld a,$05                    

csum_result_mark_0813:
0813:  12           ld (de),a                   
0814:  21 12 00     ld hl,$0012                 
0817:  19           add hl,de                   
0818:  EB           ex de,hl                    
0819:  FE 05        cp $05                      
081B:  DD E9        jp (ix)                     

POST_RAMTEST:
081D:  CB 7C        bit 7,h                     
081F:  20 07        jr nz,ramtest_body           ; -> ramtest_body
0821:  FD 21 28 08  ld iy,$0828                  ; ramtest_body
0825:  C3 CF 07     jp VG_KICK_IY                ; -> VG_KICK_IY

ramtest_body:
0828:  DB 09        in a,($09)                   ; WATCHDOG
082A:  7E           ld a,(hl)                   
082B:  FE FF        cp $FF                      
082D:  20 21        jr nz,ramtest_body_0850     
082F:  36 55        ld (hl),$55                 
0831:  7E           ld a,(hl)                   
0832:  FE 55        cp $55                      
0834:  20 1A        jr nz,ramtest_body_0850     
0836:  36 AA        ld (hl),$AA                 
0838:  7E           ld a,(hl)                   
0839:  FE AA        cp $AA                      
083B:  20 13        jr nz,ramtest_body_0850     
083D:  36 00        ld (hl),$00                 
083F:  23           inc hl                      
0840:  0B           dec bc                      
0841:  78           ld a,b                      
0842:  B1           or c                        
0843:  20 D8        jr nz,POST_RAMTEST           ; -> POST_RAMTEST
0845:  FD 21 4C 08  ld iy,$084C                 
0849:  C3 A0 07     jp VG_WAIT_IY                ; -> VG_WAIT_IY

ramtest_body_084C:
084C:  3E 05        ld a,$05                    
084E:  18 09        jr ramtest_body_0859        

ramtest_body_0850:
0850:  FD 21 57 08  ld iy,$0857                 
0854:  C3 A0 07     jp VG_WAIT_IY                ; -> VG_WAIT_IY

ramtest_body_0857:
0857:  3E 0A        ld a,$0A                    

ramtest_body_0859:
0859:  12           ld (de),a                   
085A:  21 12 00     ld hl,$0012                 
085D:  19           add hl,de                   
085E:  EB           ex de,hl                    
085F:  FE 05        cp $05                      
0861:  DD E9        jp (ix)                     

POST_NVRAMTEST:
0863:  FD 21 6A 08  ld iy,$086A                  ; nvramtest_body
0867:  C3 CF 07     jp VG_KICK_IY                ; -> VG_KICK_IY

nvramtest_body:
086A:  36 0A        ld (hl),$0A                 
086C:  7E           ld a,(hl)                   
086D:  E6 0F        and $0F                     
086F:  FE 0A        cp $0A                      
0871:  20 1A        jr nz,nvramtest_body_088D   
0873:  36 05        ld (hl),$05                 
0875:  7E           ld a,(hl)                   
0876:  E6 0F        and $0F                     
0878:  FE 05        cp $05                      
087A:  20 11        jr nz,nvramtest_body_088D   
087C:  23           inc hl                      
087D:  0B           dec bc                      
087E:  78           ld a,b                      
087F:  B1           or c                        
0880:  20 E1        jr nz,POST_NVRAMTEST         ; -> POST_NVRAMTEST
0882:  FD 21 89 08  ld iy,$0889                 
0886:  C3 A0 07     jp VG_WAIT_IY                ; -> VG_WAIT_IY

nvramtest_body_0889:
0889:  3E 05        ld a,$05                    
088B:  18 09        jr nvramtest_body_0896      

nvramtest_body_088D:
088D:  FD 21 94 08  ld iy,$0894                 
0891:  C3 A0 07     jp VG_WAIT_IY                ; -> VG_WAIT_IY

nvramtest_body_0894:
0894:  3E 0A        ld a,$0A                    

nvramtest_body_0896:
0896:  12           ld (de),a                   
0897:  21 12 00     ld hl,$0012                 
089A:  19           add hl,de                   
089B:  EB           ex de,hl                    
089C:  FE 05        cp $05                      
089E:  DD E9        jp (ix)                     

POST_SHOW_BITS:
08A0:  4F           ld c,a                      
08A1:  FD 21 A8 08  ld iy,$08A8                  ; showbits_loop
08A5:  C3 A0 07     jp VG_WAIT_IY                ; -> VG_WAIT_IY

showbits_loop:
08A8:  CB 09        rrc c                       
08AA:  38 04        jr c,showbits_loop_08B0     
08AC:  3E 05        ld a,$05                    
08AE:  18 02        jr showbits_loop_08B2       

showbits_loop_08B0:
08B0:  3E 0A        ld a,$0A                    

showbits_loop_08B2:
08B2:  12           ld (de),a                   
08B3:  21 12 00     ld hl,$0012                 
08B6:  19           add hl,de                   
08B7:  EB           ex de,hl                    
08B8:  10 EE        djnz showbits_loop           ; -> showbits_loop
08BA:  21 12 00     ld hl,$0012                 
08BD:  19           add hl,de                   
08BE:  EB           ex de,hl                    
08BF:  FD 21 C6 08  ld iy,$08C6                 
08C3:  C3 CF 07     jp VG_KICK_IY                ; -> VG_KICK_IY

showbits_loop_08C6:
08C6:  DD E9        jp (ix)                     

DRAW_TEXT:
08C8:  DB 09        in a,($09)                   ; WATCHDOG
08CA:  7E           ld a,(hl)                   
08CB:  23           inc hl                      
08CC:  13           inc de                      
08CD:  13           inc de                      
08CE:  FD 21 00 00  ld iy,$0000                  ; RESET
08D2:  FD 19        add iy,de                   
08D4:  FD F9        ld sp,iy                    
08D6:  D9           exx                         
08D7:  16 00        ld d,$00                    
08D9:  FE 20        cp $20                      
08DB:  20 05        jr nz,dtxt_period            ; -> dtxt_period
08DD:  11 92 CD     ld de,$CD92                 
08E0:  18 2C        jr dtxt_push_glyph           ; -> dtxt_push_glyph

dtxt_period:
08E2:  FE 2E        cp $2E                      
08E4:  20 05        jr nz,dtxt_letter            ; -> dtxt_letter
08E6:  11 0F C0     ld de,$C00F                 
08E9:  18 23        jr dtxt_push_glyph           ; -> dtxt_push_glyph

dtxt_letter:
08EB:  FE 41        cp $41                      
08ED:  38 0D        jr c,dtxt_digit              ; -> dtxt_digit
08EF:  FE 5A        cp $5A                      
08F1:  38 02        jr c,dtxt_letter_08F5       
08F3:  3E 5A        ld a,$5A                    

dtxt_letter_08F5:
08F5:  D6 41        sub $41                     
08F7:  21 2E 3A     ld hl,$3A2E                 
08FA:  18 0B        jr dtxt_digit_0907          

dtxt_digit:
08FC:  FE 30        cp $30                      
08FE:  30 02        jr nc,dtxt_digit_0902       
0900:  3E 30        ld a,$30                    

dtxt_digit_0902:
0902:  D6 2F        sub $2F                     
0904:  21 D3 3F     ld hl,$3FD3                 

dtxt_digit_0907:
0907:  CB 27        sla a                       
0909:  5F           ld e,a                      
090A:  19           add hl,de                   
090B:  5E           ld e,(hl)                   
090C:  23           inc hl                      
090D:  56           ld d,(hl)                   

dtxt_push_glyph:
090E:  D5           push de                     
090F:  D9           exx                         
0910:  0B           dec bc                      
0911:  78           ld a,b                      
0912:  B1           or c                        
0913:  20 B3        jr nz,DRAW_TEXT              ; -> DRAW_TEXT
0915:  DD E9        jp (ix)                     

POST_FAIL_BLINK:
0917:  78           ld a,b                      
0918:  D9           exx                         
0919:  47           ld b,a                      
091A:  D9           exx                         
091B:  04           inc b                       

blink_loop:
091C:  3E C3        ld a,$C3                    
091E:  D3 13        out ($13),a                  ; LEDS/coin counters/flip
0920:  3E 07        ld a,$07                    
0922:  D3 14        out ($14),a                  ; SOUND CMD latch

blink_loop_0924:
0924:  21 00 30     ld hl,$3000                 

blink_loop_0927:
0927:  FD 21 2E 09  ld iy,$092E                 
092B:  C3 CF 07     jp VG_KICK_IY                ; -> VG_KICK_IY

blink_loop_092E:
092E:  2B           dec hl                      
092F:  7C           ld a,h                      
0930:  B5           or l                        
0931:  20 F4        jr nz,blink_loop_0927       
0933:  3E FF        ld a,$FF                    
0935:  D3 13        out ($13),a                  ; LEDS/coin counters/flip
0937:  3E 00        ld a,$00                    
0939:  D3 14        out ($14),a                  ; SOUND CMD latch
093B:  21 00 30     ld hl,$3000                 

blink_loop_093E:
093E:  FD 21 45 09  ld iy,$0945                 
0942:  C3 CF 07     jp VG_KICK_IY                ; -> VG_KICK_IY

blink_loop_0945:
0945:  2B           dec hl                      
0946:  7C           ld a,h                      
0947:  B5           or l                        
0948:  20 F4        jr nz,blink_loop_093E       
094A:  78           ld a,b                      
094B:  FE 02        cp $02                      
094D:  20 02        jr nz,blink_loop_0951       
094F:  10 D3        djnz blink_loop_0924        

blink_loop_0951:
0951:  10 C9        djnz blink_loop              ; -> blink_loop
0953:  DD E9        jp (ix)                     

GAME_INIT:
0955:  31 FC 4B     ld sp,$4BFC                 
0958:  FD 21 80 40  ld iy,$4080                 
095C:  01 81 04     ld bc,$0481                 
095F:  11 01 40     ld de,obj_flags1             ; obj_flags1
0962:  21 00 40     ld hl,obj_flags0             ; obj_flags0
0965:  36 00        ld (hl),$00                 
0967:  ED B0        ldir                        
0969:  FD 36 F2 FF  ld (iy-14),$FF              
096D:  CD BC 18     call WALL_BUFFERS_BUILD      ; -> WALL_BUFFERS_BUILD
0970:  DB 11        in a,($11)                   ; IN P1/coin/tilt/test
0972:  E6 03        and $03                     
0974:  32 35 40     ld (coin_lines),a            ; coin_lines
0977:  DB 17        in a,($17)                   ; DSW C6
0979:  E6 07        and $07                     
097B:  07           rlca                        
097C:  32 2B 40     ld (coin_mode1),a            ; coin_mode1
097F:  6F           ld l,a                      
0980:  26 00        ld h,$00                    
0982:  11 C3 2F     ld de,$2FC3                 
0985:  19           add hl,de                   
0986:  22 27 40     ld (coinage1_ptr),hl         ; coinage1_ptr
0989:  DB 17        in a,($17)                   ; DSW C6
098B:  E6 38        and $38                     
098D:  0F           rrca                        
098E:  0F           rrca                        
098F:  32 2C 40     ld (coin_mode2),a            ; coin_mode2
0992:  6F           ld l,a                      
0993:  26 00        ld h,$00                    
0995:  11 C3 2F     ld de,$2FC3                 
0998:  19           add hl,de                   
0999:  22 29 40     ld (coinage2_ptr),hl         ; coinage2_ptr
099C:  01 2A 00     ld bc,$002A                 
099F:  11 A9 43     ld de,hiscore_work           ; hiscore_work
09A2:  21 F7 2F     ld hl,$2FF7                 
09A5:  ED B0        ldir                        
09A7:  01 54 00     ld bc,$0054                 
09AA:  11 D3 43     ld de,$43D3                 
09AD:  21 A9 43     ld hl,hiscore_work           ; hiscore_work
09B0:  ED B0        ldir                        
09B2:  0E 02        ld c,$02                    
09B4:  11 1A 5C     ld de,$5C1A                 

GAME_INIT_09B7:
09B7:  06 10        ld b,$10                    
09B9:  21 C9 30     ld hl,$30C9                 

GAME_INIT_09BC:
09BC:  1A           ld a,(de)                   
09BD:  E6 0F        and $0F                     
09BF:  BE           cp (hl)                     
09C0:  20 79        jr nz,NVRAM_WIPE             ; -> NVRAM_WIPE
09C2:  13           inc de                      
09C3:  23           inc hl                      
09C4:  10 F6        djnz GAME_INIT_09BC         
09C6:  0D           dec c                       
09C7:  20 EE        jr nz,GAME_INIT_09B7        
09C9:  06 02        ld b,$02                    
09CB:  11 3A 5C     ld de,$5C3A                 
09CE:  21 D3 43     ld hl,$43D3                 

GAME_INIT_09D1:
09D1:  0E 04        ld c,$04                    

GAME_INIT_09D3:
09D3:  CD 18 0A     call NVRAM_RD_BYTE           ; -> NVRAM_RD_BYTE
09D6:  30 63        jr nc,NVRAM_WIPE             ; -> NVRAM_WIPE
09D8:  0D           dec c                       
09D9:  20 F8        jr nz,GAME_INIT_09D3        
09DB:  0E 03        ld c,$03                    

GAME_INIT_09DD:
09DD:  1A           ld a,(de)                   
09DE:  13           inc de                      
09DF:  ED 67        rrd                         
09E1:  1A           ld a,(de)                   
09E2:  13           inc de                      
09E3:  ED 67        rrd                         
09E5:  7E           ld a,(hl)                   
09E6:  23           inc hl                      
09E7:  FE 20        cp $20                      
09E9:  28 08        jr z,GAME_INIT_09F3         
09EB:  FE 41        cp $41                      
09ED:  38 4C        jr c,NVRAM_WIPE              ; -> NVRAM_WIPE
09EF:  FE 5B        cp $5B                      
09F1:  30 48        jr nc,NVRAM_WIPE             ; -> NVRAM_WIPE

GAME_INIT_09F3:
09F3:  0D           dec c                       
09F4:  20 E7        jr nz,GAME_INIT_09DD        
09F6:  21 FD 43     ld hl,$43FD                 
09F9:  10 D6        djnz GAME_INIT_09D1         
09FB:  06 40        ld b,$40                    
09FD:  11 58 5C     ld de,$5C58                 
0A00:  21 27 44     ld hl,bookkeep_ctrs          ; bookkeep_ctrs

GAME_INIT_0A03:
0A03:  CD 18 0A     call NVRAM_RD_BYTE           ; -> NVRAM_RD_BYTE
0A06:  30 33        jr nc,NVRAM_WIPE             ; -> NVRAM_WIPE
0A08:  10 F9        djnz GAME_INIT_0A03         
0A0A:  11 56 5C     ld de,nvram_credits          ; nvram_credits
0A0D:  21 2D 40     ld hl,credits_bcd            ; credits_bcd
0A10:  CD 18 0A     call NVRAM_RD_BYTE           ; -> NVRAM_RD_BYTE
0A13:  30 26        jr nc,NVRAM_WIPE             ; -> NVRAM_WIPE
0A15:  C3 9D 0A     jp ATTRACT_INIT              ; -> ATTRACT_INIT

NVRAM_RD_BYTE:
0A18:  1A           ld a,(de)                   
0A19:  13           inc de                      
0A1A:  E6 0F        and $0F                     
0A1C:  FE 0A        cp $0A                      
0A1E:  30 0D        jr nc,NVRAM_RD_BYTE_0A2D    
0A20:  ED 67        rrd                         
0A22:  1A           ld a,(de)                   
0A23:  13           inc de                      
0A24:  E6 0F        and $0F                     
0A26:  FE 0A        cp $0A                      
0A28:  30 03        jr nc,NVRAM_RD_BYTE_0A2D    
0A2A:  ED 67        rrd                         
0A2C:  23           inc hl                      

NVRAM_RD_BYTE_0A2D:
0A2D:  C9           ret                         

NVRAM_WR_BYTES:
0A2E:  1A           ld a,(de)                   
0A2F:  13           inc de                      
0A30:  77           ld (hl),a                   
0A31:  23           inc hl                      
0A32:  0F           rrca                        
0A33:  0F           rrca                        
0A34:  0F           rrca                        
0A35:  0F           rrca                        
0A36:  77           ld (hl),a                   
0A37:  23           inc hl                      
0A38:  10 F4        djnz NVRAM_WR_BYTES          ; -> NVRAM_WR_BYTES
0A3A:  C9           ret                         

NVRAM_WIPE:
0A3B:  AF           xor a                       
0A3C:  32 2D 40     ld (credits_bcd),a           ; credits_bcd
0A3F:  01 3F 00     ld bc,$003F                 
0A42:  11 28 44     ld de,$4428                 
0A45:  21 27 44     ld hl,bookkeep_ctrs          ; bookkeep_ctrs
0A48:  36 00        ld (hl),$00                 
0A4A:  ED B0        ldir                        

SCORES_RESET:
0A4C:  01 2A 00     ld bc,$002A                 
0A4F:  11 A9 43     ld de,hiscore_work           ; hiscore_work
0A52:  21 F7 2F     ld hl,$2FF7                 
0A55:  ED B0        ldir                        
0A57:  01 54 00     ld bc,$0054                 
0A5A:  11 D3 43     ld de,$43D3                 
0A5D:  21 A9 43     ld hl,hiscore_work           ; hiscore_work
0A60:  ED B0        ldir                        
0A62:  CD 68 0A     call NVRAM_SAVE_ALL          ; -> NVRAM_SAVE_ALL
0A65:  C3 9D 0A     jp ATTRACT_INIT              ; -> ATTRACT_INIT

NVRAM_SAVE_ALL:
0A68:  06 07        ld b,$07                    
0A6A:  11 D3 43     ld de,$43D3                 
0A6D:  21 3A 5C     ld hl,$5C3A                 
0A70:  CD 2E 0A     call NVRAM_WR_BYTES          ; -> NVRAM_WR_BYTES
0A73:  06 07        ld b,$07                    
0A75:  11 FD 43     ld de,$43FD                 
0A78:  CD 2E 0A     call NVRAM_WR_BYTES          ; -> NVRAM_WR_BYTES
0A7B:  06 40        ld b,$40                    
0A7D:  11 27 44     ld de,bookkeep_ctrs          ; bookkeep_ctrs
0A80:  21 58 5C     ld hl,$5C58                 
0A83:  CD 2E 0A     call NVRAM_WR_BYTES          ; -> NVRAM_WR_BYTES
0A86:  CD E3 16     call CREDITS_NVRAM_SYNC      ; -> CREDITS_NVRAM_SYNC
0A89:  01 10 00     ld bc,$0010                 
0A8C:  11 1A 5C     ld de,$5C1A                 
0A8F:  21 C9 30     ld hl,$30C9                 
0A92:  ED B0        ldir                        
0A94:  01 10 00     ld bc,$0010                 
0A97:  21 1A 5C     ld hl,$5C1A                 
0A9A:  ED B0        ldir                        
0A9C:  C9           ret                         

ATTRACT_INIT:
0A9D:  FD 21 80 40  ld iy,$4080                 
0AA1:  FB           ei                          
0AA2:  CD 48 12     call START_LEDS_OFF          ; -> START_LEDS_OFF
0AA5:  CD 6C 12     call START_LEDS_OFF_126C    
0AA8:  FD 36 AE 00  ld (iy-82),$00              
0AAC:  FD 36 A4 01  ld (iy-92),$01              
0AB0:  FD CB C1 7E  bit 7,(iy-63)               
0AB4:  28 0C        jr z,ATTRACT_INIT_0AC2      
0AB6:  FD CB C1 76  bit 6,(iy-63)               
0ABA:  20 06        jr nz,ATTRACT_INIT_0AC2     
0ABC:  21 46 40     ld hl,p1_save                ; p1_save
0ABF:  CD 8E 2C     call PLAYER_STATE_SAVE       ; -> PLAYER_STATE_SAVE

ATTRACT_INIT_0AC2:
0AC2:  DB 17        in a,($17)                   ; DSW C6
0AC4:  CB 77        bit 6,a                     
0AC6:  28 04        jr z,ATTRACT_INIT_0ACC      
0AC8:  FD 36 AD 04  ld (iy-83),$04              

ATTRACT_INIT_0ACC:
0ACC:  3A 41 40     ld a,(game_flags)            ; game_flags
0ACF:  E6 40        and $40                     
0AD1:  32 41 40     ld (game_flags),a            ; game_flags
0AD4:  FD 36 BF 01  ld (iy-65),$01              
0AD8:  FD 36 BE 01  ld (iy-66),$01              
0ADC:  FD 36 04 00  ld (iy+4),$00               
0AE0:  FD 36 0B 00  ld (iy+11),$00              
0AE4:  FD 36 09 01  ld (iy+9),$01               
0AE8:  FD 36 03 01  ld (iy+3),$01               
0AEC:  FD 36 A0 00  ld (iy-96),$00              
0AF0:  FD 36 A1 00  ld (iy-95),$00              
0AF4:  FD 36 E7 04  ld (iy-25),$04              
0AF8:  FD 36 02 00  ld (iy+2),$00               
0AFC:  FD 36 F0 00  ld (iy-16),$00              
0B00:  CD 23 18     call DLIST_RESET             ; -> DLIST_RESET
0B03:  3A 2D 40     ld a,(credits_bcd)           ; credits_bcd
0B06:  B7           or a                        
0B07:  CA 76 0B     jp z,ATTRACT_INIT_0B76      
0B0A:  FD 36 A1 3C  ld (iy-95),$3C              
0B0E:  DB 10        in a,($10)                   ; DSW C4
0B10:  E6 03        and $03                     
0B12:  07           rlca                        
0B13:  5F           ld e,a                      
0B14:  16 00        ld d,$00                    
0B16:  21 E5 2F     ld hl,$2FE5                 
0B19:  19           add hl,de                   
0B1A:  5E           ld e,(hl)                   
0B1B:  23           inc hl                      
0B1C:  56           ld d,(hl)                   
0B1D:  ED 53 60 40  ld (xlife_thr1),de           ; xlife_thr1
0B21:  DB 10        in a,($10)                   ; DSW C4
0B23:  E6 0C        and $0C                     
0B25:  0F           rrca                        
0B26:  5F           ld e,a                      
0B27:  16 00        ld d,$00                    
0B29:  21 ED 2F     ld hl,$2FED                 
0B2C:  19           add hl,de                   
0B2D:  5E           ld e,(hl)                   
0B2E:  23           inc hl                      
0B2F:  56           ld d,(hl)                   
0B30:  23           inc hl                      
0B31:  ED 53 62 40  ld (xlife_thr2),de           ; xlife_thr2
0B35:  5E           ld e,(hl)                   
0B36:  23           inc hl                      
0B37:  56           ld d,(hl)                   
0B38:  ED 53 64 40  ld (xlife_thr3),de           ; xlife_thr3
0B3C:  FD 36 04 04  ld (iy+4),$04               
0B40:  CD F3 2A     call DRAW_PAGE               ; -> DRAW_PAGE
0B43:  DB 10        in a,($10)                   ; DSW C4
0B45:  E6 30        and $30                     
0B47:  0F           rrca                        
0B48:  0F           rrca                        
0B49:  0F           rrca                        
0B4A:  5F           ld e,a                      
0B4B:  16 00        ld d,$00                    
0B4D:  21 D3 2F     ld hl,$2FD3                 
0B50:  19           add hl,de                   
0B51:  46           ld b,(hl)                   
0B52:  23           inc hl                      
0B53:  3E 08        ld a,$08                    
0B55:  96           sub (hl)                    
0B56:  4F           ld c,a                      
0B57:  21 00 F0     ld hl,$F000                 
0B5A:  78           ld a,b                      
0B5B:  FE 02        cp $02                      
0B5D:  20 03        jr nz,ATTRACT_INIT_0B62     
0B5F:  22 F0 80     ld ($80F0),hl               

ATTRACT_INIT_0B62:
0B62:  0D           dec c                       
0B63:  28 16        jr z,ATTRACT_INIT_0B7B      
0B65:  0D           dec c                       
0B66:  28 09        jr z,ATTRACT_INIT_0B71      
0B68:  0D           dec c                       
0B69:  28 03        jr z,ATTRACT_INIT_0B6E      
0B6B:  22 30 81     ld ($8130),hl               

ATTRACT_INIT_0B6E:
0B6E:  22 32 81     ld ($8132),hl               

ATTRACT_INIT_0B71:
0B71:  22 34 81     ld ($8134),hl               
0B74:  18 05        jr ATTRACT_INIT_0B7B        

ATTRACT_INIT_0B76:
0B76:  3E 00        ld a,$00                    
0B78:  CD 04 2E     call SOUND_CMD_SEND          ; -> SOUND_CMD_SEND

ATTRACT_INIT_0B7B:
0B7B:  06 01        ld b,$01                    
0B7D:  CD 1D 29     call OBJ_SPAWN               ; -> OBJ_SPAWN
0B80:  C3 30 0F     jp MAINLOOP_SYNC             ; -> MAINLOOP_SYNC

GAME_START:
0B83:  CD 48 12     call START_LEDS_OFF          ; -> START_LEDS_OFF
0B86:  CD 54 18     call VG_WAIT_DONE            ; -> VG_WAIT_DONE
0B89:  CD 6C 12     call START_LEDS_OFF_126C    
0B8C:  FD 36 A4 02  ld (iy-92),$02              
0B90:  FD 36 AE 00  ld (iy-82),$00              
0B94:  FD CB C1 7E  bit 7,(iy-63)               
0B98:  28 0C        jr z,GAME_START_0BA6        
0B9A:  FD CB C1 76  bit 6,(iy-63)               
0B9E:  20 06        jr nz,GAME_START_0BA6       
0BA0:  21 46 40     ld hl,p1_save                ; p1_save
0BA3:  CD 8E 2C     call PLAYER_STATE_SAVE       ; -> PLAYER_STATE_SAVE

GAME_START_0BA6:
0BA6:  3A 83 40     ld a,(attract_step)          ; attract_step
0BA9:  FE 07        cp $07                      
0BAB:  28 06        jr z,GAME_START_0BB3        
0BAD:  3A 2D 40     ld a,(credits_bcd)           ; credits_bcd
0BB0:  B7           or a                        
0BB1:  20 1E        jr nz,PLAY_BEGIN             ; -> PLAY_BEGIN

GAME_START_0BB3:
0BB3:  FD CB C1 EE  set 5,(iy-63)               
0BB7:  FD 36 BE 01  ld (iy-66),$01              
0BBB:  FD CB C1 76  bit 6,(iy-63)               
0BBF:  28 5D        jr z,play_lives_setup        ; -> play_lives_setup
0BC1:  FD 36 BE 02  ld (iy-66),$02              
0BC5:  DB 17        in a,($17)                   ; DSW C6
0BC7:  CB 7F        bit 7,a                     
0BC9:  28 53        jr z,play_lives_setup        ; -> play_lives_setup
0BCB:  FD CB C1 D6  set 2,(iy-63)               
0BCF:  18 4D        jr play_lives_setup          ; -> play_lives_setup

PLAY_BEGIN:
0BD1:  FD 36 A4 03  ld (iy-92),$03              
0BD5:  FD CB C1 FE  set 7,(iy-63)               
0BD9:  DB 17        in a,($17)                   ; DSW C6
0BDB:  CB 77        bit 6,a                     
0BDD:  20 0C        jr nz,play_score_setup       ; -> play_score_setup
0BDF:  F3           di                          
0BE0:  3A 2D 40     ld a,(credits_bcd)           ; credits_bcd
0BE3:  FD 96 C0     sub (iy-64)                 
0BE6:  27           daa                         
0BE7:  32 2D 40     ld (credits_bcd),a           ; credits_bcd
0BEA:  FB           ei                          

play_score_setup:
0BEB:  FD CB AE FE  set 7,(iy-82)               
0BEF:  2A 36 40     ld hl,(score_cur)            ; score_cur
0BF2:  22 42 40     ld ($4042),hl               
0BF5:  2A 38 40     ld hl,($4038)               
0BF8:  22 44 40     ld ($4044),hl               
0BFB:  21 00 00     ld hl,$0000                  ; RESET
0BFE:  22 36 40     ld (score_cur),hl            ; score_cur
0C01:  22 38 40     ld ($4038),hl               
0C04:  22 3A 40     ld (time_played_bcd),hl      ; time_played_bcd
0C07:  22 3C 40     ld ($403C),hl               
0C0A:  01 2A 00     ld bc,$002A                 
0C0D:  11 A9 43     ld de,hiscore_work           ; hiscore_work
0C10:  21 D3 43     ld hl,$43D3                 
0C13:  FD CB C1 6E  bit 5,(iy-63)               
0C17:  28 03        jr z,play_score_setup_0C1C  
0C19:  21 FD 43     ld hl,$43FD                 

play_score_setup_0C1C:
0C1C:  ED B0        ldir                        

play_lives_setup:
0C1E:  DB 10        in a,($10)                   ; DSW C4
0C20:  E6 30        and $30                     
0C22:  0F           rrca                        
0C23:  0F           rrca                        
0C24:  0F           rrca                        
0C25:  FD CB C1 6E  bit 5,(iy-63)               
0C29:  28 01        jr z,play_lives_setup_0C2C  
0C2B:  3C           inc a                       

play_lives_setup_0C2C:
0C2C:  5F           ld e,a                      
0C2D:  16 00        ld d,$00                    
0C2F:  21 D3 2F     ld hl,$2FD3                 
0C32:  19           add hl,de                   
0C33:  7E           ld a,(hl)                   
0C34:  32 66 40     ld (lives),a                 ; lives
0C37:  FD 36 E7 06  ld (iy-25),$06              
0C3B:  FD 36 ED 00  ld (iy-19),$00              
0C3F:  FD 36 A0 00  ld (iy-96),$00              
0C43:  FD 36 A1 00  ld (iy-95),$00              
0C47:  FD 36 A2 19  ld (iy-94),$19              
0C4B:  FD 36 EA 00  ld (iy-22),$00              
0C4F:  FD 36 EB 00  ld (iy-21),$00              
0C53:  FD 36 EC 00  ld (iy-20),$00              
0C57:  FD 36 E8 01  ld (iy-24),$01              
0C5B:  21 46 40     ld hl,p1_save                ; p1_save
0C5E:  CD 8E 2C     call PLAYER_STATE_SAVE       ; -> PLAYER_STATE_SAVE
0C61:  21 53 40     ld hl,p2_save                ; p2_save
0C64:  CD 8E 2C     call PLAYER_STATE_SAVE       ; -> PLAYER_STATE_SAVE
0C67:  CD 23 18     call DLIST_RESET             ; -> DLIST_RESET
0C6A:  CD 02 19     call GAME_DLIST_BUILD        ; -> GAME_DLIST_BUILD
0C6D:  FD CB C1 76  bit 6,(iy-63)               
0C71:  28 15        jr z,play_roster_init        ; -> play_roster_init
0C73:  FD 36 04 07  ld (iy+4),$07               
0C77:  CD F3 2A     call DRAW_PAGE               ; -> DRAW_PAGE
0C7A:  FD 36 04 08  ld (iy+4),$08               
0C7E:  CD F3 2A     call DRAW_PAGE               ; -> DRAW_PAGE
0C81:  FD 36 04 0A  ld (iy+4),$0A               
0C85:  CD F3 2A     call DRAW_PAGE               ; -> DRAW_PAGE

play_roster_init:
0C88:  CD 68 0A     call NVRAM_SAVE_ALL          ; -> NVRAM_SAVE_ALL
0C8B:  06 20        ld b,$20                    

play_roster_init_0C8D:
0C8D:  CD 1D 29     call OBJ_SPAWN               ; -> OBJ_SPAWN
0C90:  CD B2 29     call OBJ_KILL                ; -> OBJ_KILL
0C93:  10 F8        djnz play_roster_init_0C8D  
0C95:  FD 34 EE     inc (iy-18)                 

PLAY_FRAME:
0C98:  CD 54 18     call VG_WAIT_DONE            ; -> VG_WAIT_DONE
0C9B:  FD CB EB 76  bit 6,(iy-21)               
0C9F:  28 7A        jr z,TURN_SWITCH_CHECK       ; -> TURN_SWITCH_CHECK
0CA1:  FD 34 EC     inc (iy-20)                 
0CA4:  3A 6C 40     ld a,(wave_ctr)              ; wave_ctr
0CA7:  E6 03        and $03                     
0CA9:  20 70        jr nz,TURN_SWITCH_CHECK      ; -> TURN_SWITCH_CHECK
0CAB:  3A 6C 40     ld a,(wave_ctr)              ; wave_ctr
0CAE:  CB 2F        sra a                       
0CB0:  CB 2F        sra a                       
0CB2:  FE 06        cp $06                      
0CB4:  38 02        jr c,PLAY_FRAME_0CB8        
0CB6:  3E 06        ld a,$06                    

PLAY_FRAME_0CB8:
0CB8:  C6 16        add a,$16                   
0CBA:  32 84 40     ld (page_id),a               ; page_id
0CBD:  CD F3 2A     call DRAW_PAGE               ; -> DRAW_PAGE
0CC0:  FD 36 04 16  ld (iy+4),$16               
0CC4:  CD F3 2A     call DRAW_PAGE               ; -> DRAW_PAGE
0CC7:  21 00 B0     ld hl,$B000                 
0CCA:  22 92 80     ld ($8092),hl               
0CCD:  22 94 80     ld ($8094),hl               
0CD0:  FD 36 A1 05  ld (iy-95),$05              

WAVE_CLEAR_ANIM:
0CD4:  DB 09        in a,($09)                   ; WATCHDOG
0CD6:  CD D5 17     call VG_KICK_TITLE           ; -> VG_KICK_TITLE
0CD9:  3A 17 40     ld a,(tick_244hz)            ; tick_244hz
0CDC:  47           ld b,a                      
0CDD:  FD 96 98     sub (iy-104)                
0CE0:  FD 70 98     ld (iy-104),b               
0CE3:  30 EF        jr nc,WAVE_CLEAR_ANIM        ; -> WAVE_CLEAR_ANIM
0CE5:  11 3A 40     ld de,time_played_bcd        ; time_played_bcd
0CE8:  21 F7 3F     ld hl,$3FF7                 
0CEB:  CD D5 16     call BCD_ADD4                ; -> BCD_ADD4
0CEE:  FD 35 A1     dec (iy-95)                 
0CF1:  3A 21 40     ld a,(timer_seconds)         ; timer_seconds
0CF4:  B7           or a                        
0CF5:  28 13        jr z,WAVE_CLEAR_ANIM_0D0A   
0CF7:  FE 02        cp $02                      
0CF9:  20 D9        jr nz,WAVE_CLEAR_ANIM        ; -> WAVE_CLEAR_ANIM
0CFB:  FD 36 A1 00  ld (iy-95),$00              
0CFF:  3E 14        ld a,$14                    
0D01:  CD 04 2E     call SOUND_CMD_SEND          ; -> SOUND_CMD_SEND
0D04:  FD 36 A1 02  ld (iy-95),$02              
0D08:  18 CA        jr WAVE_CLEAR_ANIM           ; -> WAVE_CLEAR_ANIM

WAVE_CLEAR_ANIM_0D0A:
0D0A:  20 C8        jr nz,WAVE_CLEAR_ANIM        ; -> WAVE_CLEAR_ANIM
0D0C:  CD 54 18     call VG_WAIT_DONE            ; -> VG_WAIT_DONE
0D0F:  06 02        ld b,$02                    

WAVE_CLEAR_ANIM_0D11:
0D11:  3E 05        ld a,$05                    
0D13:  CD 35 23     call SCORE_ADD               ; -> SCORE_ADD
0D16:  10 F9        djnz WAVE_CLEAR_ANIM_0D11   
0D18:  CD 54 18     call VG_WAIT_DONE            ; -> VG_WAIT_DONE

TURN_SWITCH_CHECK:
0D1B:  FD CB C1 76  bit 6,(iy-63)               
0D1F:  CA DF 0D     jp z,FRAME_TAIL              ; -> FRAME_TAIL
0D22:  FD CB 32 7E  bit 7,(iy+50)               
0D26:  28 07        jr z,TURN_SWITCH_CHECK_0D2F 
0D28:  FD CB 32 66  bit 4,(iy+50)               
0D2C:  C2 DF 0D     jp nz,FRAME_TAIL             ; -> FRAME_TAIL

TURN_SWITCH_CHECK_0D2F:
0D2F:  3A 6D 40     ld a,($406D)                
0D32:  B7           or a                        
0D33:  28 1C        jr z,PLAYER_TURN_SWAP        ; -> PLAYER_TURN_SWAP
0D35:  FD 35 ED     dec (iy-19)                 
0D38:  2A F3 3E     ld hl,($3EF3)               
0D3B:  22 42 82     ld ($8242),hl               
0D3E:  FD 36 04 0B  ld (iy+4),$0B               
0D42:  CD F3 2A     call DRAW_PAGE               ; -> DRAW_PAGE
0D45:  FD 36 A1 00  ld (iy-95),$00              
0D49:  3E 14        ld a,$14                    
0D4B:  CD 04 2E     call SOUND_CMD_SEND          ; -> SOUND_CMD_SEND
0D4E:  C3 DF 0D     jp FRAME_TAIL                ; -> FRAME_TAIL

PLAYER_TURN_SWAP:
0D51:  3A 3E 40     ld a,(cur_player_num)        ; cur_player_num
0D54:  FE 01        cp $01                      
0D56:  28 32        jr z,PLAYER_TURN_SWAP_0D8A  
0D58:  21 53 40     ld hl,p2_save                ; p2_save
0D5B:  CD 8E 2C     call PLAYER_STATE_SAVE       ; -> PLAYER_STATE_SAVE
0D5E:  FD 36 BE 01  ld (iy-66),$01              
0D62:  21 46 40     ld hl,p1_save                ; p1_save
0D65:  CD BD 2C     call PLAYER_STATE_LOAD       ; -> PLAYER_STATE_LOAD
0D68:  CD 55 12     call START_LEDS_OFF_1255    
0D6B:  01 10 00     ld bc,$0010                 
0D6E:  11 34 82     ld de,$8234                 
0D71:  21 F5 3E     ld hl,$3EF5                 
0D74:  ED B0        ldir                        
0D76:  01 64 00     ld bc,$0064                 
0D79:  11 58 82     ld de,$8258                 
0D7C:  21 B1 31     ld hl,$31B1                 
0D7F:  ED B0        ldir                        
0D81:  FD 36 04 0C  ld (iy+4),$0C               
0D85:  CD F3 2A     call DRAW_PAGE               ; -> DRAW_PAGE
0D88:  18 30        jr PLAYER_TURN_SWAP_0DBA    

PLAYER_TURN_SWAP_0D8A:
0D8A:  21 46 40     ld hl,p1_save                ; p1_save
0D8D:  CD 8E 2C     call PLAYER_STATE_SAVE       ; -> PLAYER_STATE_SAVE
0D90:  FD 36 BE 02  ld (iy-66),$02              
0D94:  21 53 40     ld hl,p2_save                ; p2_save
0D97:  CD BD 2C     call PLAYER_STATE_LOAD       ; -> PLAYER_STATE_LOAD
0D9A:  CD 55 12     call START_LEDS_OFF_1255    
0D9D:  01 10 00     ld bc,$0010                 
0DA0:  11 34 82     ld de,$8234                 
0DA3:  21 05 3F     ld hl,$3F05                 
0DA6:  ED B0        ldir                        
0DA8:  01 64 00     ld bc,$0064                 
0DAB:  11 58 82     ld de,$8258                 
0DAE:  21 69 3F     ld hl,$3F69                 
0DB1:  ED B0        ldir                        
0DB3:  FD 36 04 0D  ld (iy+4),$0D               
0DB7:  CD F3 2A     call DRAW_PAGE               ; -> DRAW_PAGE

PLAYER_TURN_SWAP_0DBA:
0DBA:  FD 36 EB 00  ld (iy-21),$00              
0DBE:  06 04        ld b,$04                    
0DC0:  11 39 40     ld de,$4039                 
0DC3:  21 17 46     ld hl,$4617                 
0DC6:  CD 0E 24     call BCD_TO_DIGITS           ; -> BCD_TO_DIGITS
0DC9:  06 01        ld b,$01                    
0DCB:  11 2D 40     ld de,credits_bcd            ; credits_bcd
0DCE:  21 2B 46     ld hl,$462B                 
0DD1:  CD 0E 24     call BCD_TO_DIGITS           ; -> BCD_TO_DIGITS
0DD4:  06 04        ld b,$04                    
0DD6:  11 45 40     ld de,$4045                 
0DD9:  21 46 82     ld hl,$8246                 
0DDC:  CD 0E 24     call BCD_TO_DIGITS           ; -> BCD_TO_DIGITS

FRAME_TAIL:
0DDF:  06 08        ld b,$08                    
0DE1:  21 9B 40     ld hl,$409B                 

FRAME_TAIL_0DE4:
0DE4:  36 01        ld (hl),$01                 
0DE6:  23           inc hl                      
0DE7:  10 FB        djnz FRAME_TAIL_0DE4        
0DE9:  FD 36 1A 80  ld (iy+26),$80              
0DED:  FD 36 24 01  ld (iy+36),$01              
0DF1:  FD 36 27 01  ld (iy+39),$01              
0DF5:  FD 36 2A 01  ld (iy+42),$01              
0DF9:  FD 36 2D 01  ld (iy+45),$01              
0DFD:  3A 23 40     ld a,(wave_start_timer)      ; wave_start_timer
0E00:  B7           or a                        
0E01:  20 04        jr nz,FRAME_TAIL_0E07       
0E03:  FD 36 A3 02  ld (iy-93),$02              

FRAME_TAIL_0E07:
0E07:  3A 23 40     ld a,(wave_start_timer)      ; wave_start_timer
0E0A:  32 21 40     ld (timer_seconds),a         ; timer_seconds
0E0D:  FD 36 E8 01  ld (iy-24),$01              
0E11:  FD 36 E9 00  ld (iy-23),$00              
0E15:  CD 86 21     call DIFFICULTY_CALC         ; -> DIFFICULTY_CALC
0E18:  CD 06 29     call RANDOM_NEXT             ; -> RANDOM_NEXT
0E1B:  32 79 40     ld (prng_cache),a            ; prng_cache

WAVE_ADVANCE:
0E1E:  FD CB EB 76  bit 6,(iy-21)               
0E22:  28 19        jr z,WAVE_ROSTER             ; -> WAVE_ROSTER
0E24:  FD 34 E7     inc (iy-25)                 
0E27:  FD 34 E7     inc (iy-25)                 
0E2A:  3A 67 40     ld a,(wave_num)              ; wave_num
0E2D:  FE 0A        cp $0A                      
0E2F:  38 04        jr c,WAVE_ADVANCE_0E35      
0E31:  FD CB EA FE  set 7,(iy-22)               

WAVE_ADVANCE_0E35:
0E35:  FE 0C        cp $0C                      
0E37:  38 04        jr c,WAVE_ROSTER             ; -> WAVE_ROSTER
0E39:  FD 36 E7 0C  ld (iy-25),$0C              

WAVE_ROSTER:
0E3D:  3A 67 40     ld a,(wave_num)              ; wave_num
0E40:  06 15        ld b,$15                    
0E42:  FE 06        cp $06                      
0E44:  20 02        jr nz,WAVE_ROSTER_0E48      
0E46:  06 20        ld b,$20                    

WAVE_ROSTER_0E48:
0E48:  CD B2 29     call OBJ_KILL                ; -> OBJ_KILL
0E4B:  10 FB        djnz WAVE_ROSTER_0E48       
0E4D:  FD 46 E7     ld b,(iy-25)                
0E50:  04           inc b                       

WAVE_ROSTER_0E51:
0E51:  CD 1D 29     call OBJ_SPAWN               ; -> OBJ_SPAWN
0E54:  10 FB        djnz WAVE_ROSTER_0E51       
0E56:  3A 67 40     ld a,(wave_num)              ; wave_num
0E59:  FE 06        cp $06                      
0E5B:  20 30        jr nz,WAVE_COMPLETE_DET      ; -> WAVE_COMPLETE_DET
0E5D:  DD 21 C9 40  ld ix,$40C9                 
0E61:  DD CB 00 B6  res 6,(ix+0)                
0E65:  DD 21 E0 40  ld ix,$40E0                 
0E69:  DD CB 00 B6  res 6,(ix+0)                
0E6D:  DD 21 F7 40  ld ix,$40F7                 
0E71:  DD CB 00 B6  res 6,(ix+0)                
0E75:  DD 21 0E 41  ld ix,$410E                 
0E79:  DD CB 00 B6  res 6,(ix+0)                
0E7D:  DD 21 25 41  ld ix,$4125                 
0E81:  DD CB 00 B6  res 6,(ix+0)                
0E85:  DD 21 3C 41  ld ix,$413C                 
0E89:  DD CB 00 B6  res 6,(ix+0)                

WAVE_COMPLETE_DET:
0E8D:  FD 36 FA 00  ld (iy-6),$00               
0E91:  FD 36 FB 00  ld (iy-5),$00               
0E95:  FD 36 EB 00  ld (iy-21),$00              
0E99:  DD 21 C9 40  ld ix,$40C9                 
0E9D:  FD 36 F7 02  ld (iy-9),$02               
0EA1:  CD 67 24     call ENEMY_ALIVE_SCAN        ; -> ENEMY_ALIVE_SCAN

DEMO_AUTODESTRUCT:
0EA4:  3A 24 40     ld a,(game_mode)             ; game_mode
0EA7:  FE 02        cp $02                      
0EA9:  20 10        jr nz,DEMO_AUTODESTRUCT_0EBB
0EAB:  FD CB 33 FE  set 7,(iy+51)               
0EAF:  FD CB 32 C6  set 0,(iy+50)               
0EB3:  FD 36 44 0E  ld (iy+68),$0E              
0EB7:  FD 36 40 14  ld (iy+64),$14              

DEMO_AUTODESTRUCT_0EBB:
0EBB:  CD F9 2C     call LIVES_ICON_DRAW         ; -> LIVES_ICON_DRAW
0EBE:  FD CB C1 76  bit 6,(iy-63)               
0EC2:  20 06        jr nz,DEMO_AUTODESTRUCT_0ECA
0EC4:  21 00 D0     ld hl,$D000                 
0EC7:  22 34 83     ld ($8334),hl               

DEMO_AUTODESTRUCT_0ECA:
0ECA:  3A 67 40     ld a,(wave_num)              ; wave_num
0ECD:  FE 0C        cp $0C                      
0ECF:  30 04        jr nc,DEMO_AUTODESTRUCT_0ED5
0ED1:  FD 36 A2 19  ld (iy-94),$19              

DEMO_AUTODESTRUCT_0ED5:
0ED5:  FD 36 FC FA  ld (iy-4),$FA               
0ED9:  3A 6C 40     ld a,(wave_ctr)              ; wave_ctr
0EDC:  FE 07        cp $07                      
0EDE:  38 04        jr c,DEMO_AUTODESTRUCT_0EE4 
0EE0:  FD 36 FC 4B  ld (iy-4),$4B               

DEMO_AUTODESTRUCT_0EE4:
0EE4:  18 4A        jr MAINLOOP_SYNC             ; -> MAINLOOP_SYNC

SHIP_RESPAWN:
0EE6:  3E 00        ld a,$00                    
0EE8:  CD 04 2E     call SOUND_CMD_SEND          ; -> SOUND_CMD_SEND
0EEB:  06 01        ld b,$01                    
0EED:  CD 1D 29     call OBJ_SPAWN               ; -> OBJ_SPAWN
0EF0:  FD 36 F7 02  ld (iy-9),$02               
0EF4:  CD 23 18     call DLIST_RESET             ; -> DLIST_RESET
0EF7:  01 7A 00     ld bc,$007A                 
0EFA:  11 81 44     ld de,dlist_shadow           ; dlist_shadow
0EFD:  21 0E 3A     ld hl,$3A0E                 
0F00:  ED B0        ldir                        
0F02:  FD 36 18 00  ld (iy+24),$00              
0F06:  FD 36 A0 00  ld (iy-96),$00              
0F0A:  FD 36 04 11  ld (iy+4),$11               
0F0E:  CD F3 2A     call DRAW_PAGE               ; -> DRAW_PAGE
0F11:  FD CB C1 76  bit 6,(iy-63)               
0F15:  28 19        jr z,MAINLOOP_SYNC           ; -> MAINLOOP_SYNC
0F17:  CD 55 12     call START_LEDS_OFF_1255    
0F1A:  FD CB BE 46  bit 0,(iy-66)               
0F1E:  28 09        jr z,SHIP_RESPAWN_0F29      
0F20:  FD 36 04 12  ld (iy+4),$12               
0F24:  CD F3 2A     call DRAW_PAGE               ; -> DRAW_PAGE
0F27:  18 07        jr MAINLOOP_SYNC             ; -> MAINLOOP_SYNC

SHIP_RESPAWN_0F29:
0F29:  FD 36 04 13  ld (iy+4),$13               
0F2D:  CD F3 2A     call DRAW_PAGE               ; -> DRAW_PAGE

MAINLOOP_SYNC:
0F30:  3E 00        ld a,$00                    
0F32:  32 17 40     ld (tick_244hz),a            ; tick_244hz
0F35:  32 18 40     ld (tick_prev),a             ; tick_prev
0F38:  FB           ei                          

MAINLOOP:
0F39:  FD 21 80 40  ld iy,$4080                 
0F3D:  DB 09        in a,($09)                   ; WATCHDOG
0F3F:  3A 17 40     ld a,(tick_244hz)            ; tick_244hz
0F42:  FD 96 98     sub (iy-104)                
0F45:  32 19 40     ld (tick_delta),a            ; tick_delta
0F48:  47           ld b,a                      
0F49:  3A 17 40     ld a,(tick_244hz)            ; tick_244hz
0F4C:  32 18 40     ld (tick_prev),a             ; tick_prev
0F4F:  30 7B        jr nc,MAINLOOP_0FCC         
0F51:  FD 34 A0     inc (iy-96)                 
0F54:  11 3A 40     ld de,time_played_bcd        ; time_played_bcd
0F57:  21 F7 3F     ld hl,$3FF7                 
0F5A:  CD D5 16     call BCD_ADD4                ; -> BCD_ADD4
0F5D:  3A 21 40     ld a,(timer_seconds)         ; timer_seconds
0F60:  B7           or a                        
0F61:  28 30        jr z,MAINLOOP_0F93          
0F63:  FD 35 A1     dec (iy-95)                 
0F66:  20 2B        jr nz,MAINLOOP_0F93         
0F68:  3A 82 40     ld a,(slam_latch)            ; slam_latch
0F6B:  B7           or a                        
0F6C:  FD 36 02 00  ld (iy+2),$00               
0F70:  28 08        jr z,MAINLOOP_0F7A          
0F72:  3E 00        ld a,$00                    
0F74:  CD 04 2E     call SOUND_CMD_SEND          ; -> SOUND_CMD_SEND
0F77:  C3 9D 0A     jp ATTRACT_INIT              ; -> ATTRACT_INIT

MAINLOOP_0F7A:
0F7A:  FD 36 E9 00  ld (iy-23),$00              
0F7E:  3A 68 40     ld a,(last_sound_cmd)        ; last_sound_cmd
0F81:  FD 36 E8 00  ld (iy-24),$00              
0F85:  CD F6 2D     call HEARTBEAT_SOUND         ; -> HEARTBEAT_SOUND
0F88:  FD CB AF 6E  bit 5,(iy-81)               
0F8C:  20 05        jr nz,MAINLOOP_0F93         
0F8E:  3E 09        ld a,$09                    
0F90:  CD 04 2E     call SOUND_CMD_SEND          ; -> SOUND_CMD_SEND

MAINLOOP_0F93:
0F93:  3A 22 40     ld a,(wave_pace_timer)       ; wave_pace_timer
0F96:  B7           or a                        
0F97:  28 09        jr z,MAINLOOP_0FA2          
0F99:  FD 35 A2     dec (iy-94)                 
0F9C:  20 04        jr nz,MAINLOOP_0FA2         
0F9E:  FD CB EA FE  set 7,(iy-22)               

MAINLOOP_0FA2:
0FA2:  3A 23 40     ld a,(wave_start_timer)      ; wave_start_timer
0FA5:  B7           or a                        
0FA6:  28 24        jr z,MAINLOOP_0FCC          
0FA8:  3D           dec a                       
0FA9:  32 23 40     ld (wave_start_timer),a      ; wave_start_timer
0FAC:  20 1E        jr nz,MAINLOOP_0FCC         
0FAE:  FD CB EA F6  set 6,(iy-22)               
0FB2:  FD CB EB FE  set 7,(iy-21)               
0FB6:  FD CB AE 7E  bit 7,(iy-82)               
0FBA:  28 10        jr z,MAINLOOP_0FCC          
0FBC:  3A 24 40     ld a,(game_mode)             ; game_mode
0FBF:  FE 01        cp $01                      
0FC1:  28 09        jr z,MAINLOOP_0FCC          
0FC3:  CD 54 18     call VG_WAIT_DONE            ; -> VG_WAIT_DONE
0FC6:  21 00 F0     ld hl,$F000                 
0FC9:  22 42 82     ld ($8242),hl               

MAINLOOP_0FCC:
0FCC:  3A 70 40     ld a,(fire_cooldown)         ; fire_cooldown
0FCF:  90           sub b                       
0FD0:  30 02        jr nc,MAINLOOP_0FD4         
0FD2:  3E 00        ld a,$00                    

MAINLOOP_0FD4:
0FD4:  32 70 40     ld (fire_cooldown),a         ; fire_cooldown
0FD7:  CD F4 14     call INPUTS_SAMPLE           ; -> INPUTS_SAMPLE
0FDA:  FD CB A4 4E  bit 1,(iy-92)               
0FDE:  28 24        jr z,MAINLOOP_1004          
0FE0:  CD 50 17     call VG_RESTART_FRAME        ; -> VG_RESTART_FRAME
0FE3:  CD 65 19     call OBJECTS_UPDATE          ; -> OBJECTS_UPDATE
0FE6:  FD CB A4 46  bit 0,(iy-92)               
0FEA:  C2 D2 12     jp nz,FRAME_ENGINE           ; -> FRAME_ENGINE
0FED:  CD AA 11     call START_BTN_CHECK         ; -> START_BTN_CHECK
0FF0:  C2 83 0B     jp nz,GAME_START             ; -> GAME_START
0FF3:  FD CB AE 76  bit 6,(iy-82)               
0FF7:  C2 9D 0A     jp nz,ATTRACT_INIT           ; -> ATTRACT_INIT
0FFA:  3A 20 40     ld a,(seconds_ctr)           ; seconds_ctr
0FFD:  FE 2D        cp $2D                      
0FFF:  DA D2 12     jp c,FRAME_ENGINE            ; -> FRAME_ENGINE
1002:  18 2C        jr ATTRACT_STEP_DISP         ; -> ATTRACT_STEP_DISP

MAINLOOP_1004:
1004:  CD AA 11     call START_BTN_CHECK         ; -> START_BTN_CHECK
1007:  C2 83 0B     jp nz,GAME_START             ; -> GAME_START
100A:  3A 24 40     ld a,(game_mode)             ; game_mode
100D:  FE 01        cp $01                      
100F:  C2 7B 12     jp nz,HISCORE_ENTRY_FRAME    ; -> HISCORE_ENTRY_FRAME
1012:  FD CB AE 76  bit 6,(iy-82)               
1016:  C4 E3 16     call nz,CREDITS_NVRAM_SYNC   ; -> CREDITS_NVRAM_SYNC
1019:  DB 11        in a,($11)                   ; IN P1/coin/tilt/test
101B:  CB 7F        bit 7,a                     
101D:  CA 37 11     jp z,OPERATOR_MENU           ; -> OPERATOR_MENU
1020:  3A 21 40     ld a,(timer_seconds)         ; timer_seconds
1023:  B7           or a                        
1024:  C2 94 11     jp nz,ATTRACT_FRAME_TAIL     ; -> ATTRACT_FRAME_TAIL
1027:  3A 8B 40     ld a,(page_line_total)       ; page_line_total
102A:  FD BE 09     cp (iy+9)                   
102D:  D2 94 11     jp nc,ATTRACT_FRAME_TAIL     ; -> ATTRACT_FRAME_TAIL

ATTRACT_STEP_DISP:
1030:  FD 34 03     inc (iy+3)                  
1033:  3A 83 40     ld a,(attract_step)          ; attract_step
1036:  3D           dec a                       
1037:  28 2A        jr z,att_step1_idle          ; -> att_step1_idle
1039:  3D           dec a                       
103A:  28 2A        jr z,att_step2_title         ; -> att_step2_title
103C:  3D           dec a                       
103D:  28 38        jr z,att_step3_instr         ; -> att_step3_instr
103F:  3D           dec a                       
1040:  28 46        jr z,att_step4_instr2        ; -> att_step4_instr2
1042:  3D           dec a                       
1043:  28 54        jr z,att_step5_scores        ; -> att_step5_scores
1045:  3D           dec a                       
1046:  28 6D        jr z,att_step6_scores2       ; -> att_step6_scores2
1048:  3D           dec a                       
1049:  CA D1 10     jp z,att_step7_demo          ; -> att_step7_demo
104C:  3D           dec a                       
104D:  CA D8 10     jp z,att_step7_demo_10D8    
1050:  3D           dec a                       
1051:  CA DA 10     jp z,att_step7_demo_10DA    
1054:  3D           dec a                       
1055:  CA DC 10     jp z,ATTRACT_PRICING         ; -> ATTRACT_PRICING
1058:  3D           dec a                       
1059:  CA 31 11     jp z,ATTRACT_PRICING_1131   
105C:  3D           dec a                       
105D:  CA 34 11     jp z,ATTRACT_PRICING_1134   
1060:  C3 9D 0A     jp ATTRACT_INIT              ; -> ATTRACT_INIT

att_step1_idle:
1063:  C3 94 11     jp ATTRACT_FRAME_TAIL        ; -> ATTRACT_FRAME_TAIL

att_step2_title:
1066:  FD 36 04 01  ld (iy+4),$01               
106A:  FD 36 A1 0F  ld (iy-95),$0F              
106E:  CD F1 16     call ATTRACT_PAGE_LOAD       ; -> ATTRACT_PAGE_LOAD
1071:  CD F3 2A     call DRAW_PAGE               ; -> DRAW_PAGE
1074:  C3 94 11     jp ATTRACT_FRAME_TAIL        ; -> ATTRACT_FRAME_TAIL

att_step3_instr:
1077:  CD 23 18     call DLIST_RESET             ; -> DLIST_RESET
107A:  FD 36 04 02  ld (iy+4),$02               
107E:  FD 36 A1 06  ld (iy-95),$06              
1082:  CD F3 2A     call DRAW_PAGE               ; -> DRAW_PAGE
1085:  C3 94 11     jp ATTRACT_FRAME_TAIL        ; -> ATTRACT_FRAME_TAIL

att_step4_instr2:
1088:  FD 36 04 03  ld (iy+4),$03               
108C:  FD 36 A1 08  ld (iy-95),$08              
1090:  CD F1 16     call ATTRACT_PAGE_LOAD       ; -> ATTRACT_PAGE_LOAD
1093:  CD F3 2A     call DRAW_PAGE               ; -> DRAW_PAGE
1096:  C3 94 11     jp ATTRACT_FRAME_TAIL        ; -> ATTRACT_FRAME_TAIL

att_step5_scores:
1099:  FD 36 A4 01  ld (iy-92),$01              
109D:  CD 23 18     call DLIST_RESET             ; -> DLIST_RESET
10A0:  FD 36 A1 0A  ld (iy-95),$0A              
10A4:  FD 36 BE 01  ld (iy-66),$01              
10A8:  CD 6C 12     call START_LEDS_OFF_126C    
10AB:  FD 36 04 0F  ld (iy+4),$0F               
10AF:  CD F3 2A     call DRAW_PAGE               ; -> DRAW_PAGE
10B2:  C3 94 11     jp ATTRACT_FRAME_TAIL        ; -> ATTRACT_FRAME_TAIL

att_step6_scores2:
10B5:  FD 36 A4 01  ld (iy-92),$01              
10B9:  CD 23 18     call DLIST_RESET             ; -> DLIST_RESET
10BC:  FD 36 A1 0A  ld (iy-95),$0A              
10C0:  FD 36 BE 01  ld (iy-66),$01              
10C4:  CD 6C 12     call START_LEDS_OFF_126C    
10C7:  FD 36 04 10  ld (iy+4),$10               
10CB:  CD F3 2A     call DRAW_PAGE               ; -> DRAW_PAGE
10CE:  C3 94 11     jp ATTRACT_FRAME_TAIL        ; -> ATTRACT_FRAME_TAIL

att_step7_demo:
10D1:  FD 36 A1 1E  ld (iy-95),$1E              
10D5:  C3 83 0B     jp GAME_START                ; -> GAME_START

att_step7_demo_10D8:
10D8:  18 BF        jr att_step5_scores          ; -> att_step5_scores

att_step7_demo_10DA:
10DA:  18 D9        jr att_step6_scores2         ; -> att_step6_scores2

ATTRACT_PRICING:
10DC:  3A 2D 40     ld a,(credits_bcd)           ; credits_bcd
10DF:  B7           or a                        
10E0:  C2 9D 0A     jp nz,ATTRACT_INIT           ; -> ATTRACT_INIT
10E3:  CD 23 18     call DLIST_RESET             ; -> DLIST_RESET
10E6:  FD 36 04 14  ld (iy+4),$14               
10EA:  CD F3 2A     call DRAW_PAGE               ; -> DRAW_PAGE
10ED:  FD 36 A1 05  ld (iy-95),$05              
10F1:  2A 27 40     ld hl,(coinage1_ptr)         ; coinage1_ptr
10F4:  4E           ld c,(hl)                   
10F5:  23           inc hl                      
10F6:  46           ld b,(hl)                   
10F7:  11 D5 3F     ld de,$3FD5                 
10FA:  69           ld l,c                      
10FB:  CD 27 11     call ATTRACT_PRICING_1127   
10FE:  22 24 80     ld ($8024),hl               
1101:  68           ld l,b                      
1102:  CD 27 11     call ATTRACT_PRICING_1127   
1105:  22 3A 80     ld ($803A),hl               
1108:  79           ld a,c                      
1109:  81           add a,c                     
110A:  6F           ld l,a                      
110B:  CD 27 11     call ATTRACT_PRICING_1127   
110E:  22 54 80     ld ($8054),hl               
1111:  78           ld a,b                      
1112:  80           add a,b                     
1113:  6F           ld l,a                      
1114:  CD 27 11     call ATTRACT_PRICING_1127   
1117:  22 6A 80     ld ($806A),hl               
111A:  78           ld a,b                      
111B:  FE 02        cp $02                      
111D:  38 06        jr c,ATTRACT_PRICING_1125   
111F:  21 00 B0     ld hl,$B000                 
1122:  22 54 80     ld ($8054),hl               

ATTRACT_PRICING_1125:
1125:  18 6D        jr ATTRACT_FRAME_TAIL        ; -> ATTRACT_FRAME_TAIL

ATTRACT_PRICING_1127:
1127:  CB 25        sla l                       
1129:  26 00        ld h,$00                    
112B:  19           add hl,de                   
112C:  7E           ld a,(hl)                   
112D:  23           inc hl                      
112E:  66           ld h,(hl)                   
112F:  6F           ld l,a                      
1130:  C9           ret                         

ATTRACT_PRICING_1131:
1131:  C3 99 10     jp att_step5_scores          ; -> att_step5_scores

ATTRACT_PRICING_1134:
1134:  C3 B5 10     jp att_step6_scores2         ; -> att_step6_scores2

OPERATOR_MENU:
1137:  01 33 44     ld bc,$4433                 
113A:  11 71 44     ld de,$4471                 
113D:  21 43 44     ld hl,$4443                 
1140:  CD 46 16     call COINAGE_DISP_PREP       ; -> COINAGE_DISP_PREP
1143:  01 35 44     ld bc,$4435                 
1146:  11 79 44     ld de,$4479                 
1149:  21 5B 44     ld hl,$445B                 
114C:  CD 46 16     call COINAGE_DISP_PREP       ; -> COINAGE_DISP_PREP
114F:  FD 36 04 15  ld (iy+4),$15               
1153:  CD 23 18     call DLIST_RESET             ; -> DLIST_RESET
1156:  CD F3 2A     call DRAW_PAGE               ; -> DRAW_PAGE
1159:  FD CB AE B6  res 6,(iy-82)               

opmenu_loop:
115D:  CD D5 17     call VG_KICK_TITLE           ; -> VG_KICK_TITLE
1160:  CD F4 14     call INPUTS_SAMPLE           ; -> INPUTS_SAMPLE
1163:  FD CB AE 76  bit 6,(iy-82)               
1167:  20 CE        jr nz,OPERATOR_MENU          ; -> OPERATOR_MENU
1169:  FD CB B0 6E  bit 5,(iy-80)               
116D:  20 16        jr nz,opmenu_zero_credits    ; -> opmenu_zero_credits
116F:  FD CB B0 76  bit 6,(iy-80)               
1173:  C2 4C 0A     jp nz,SCORES_RESET           ; -> SCORES_RESET
1176:  FD CB B2 76  bit 6,(iy-78)               
117A:  28 0F        jr z,opmenu_wipe_check       ; -> opmenu_wipe_check

opmenu_loop_117C:
117C:  FD CB B1 7E  bit 7,(iy-79)               
1180:  C2 9D 0A     jp nz,ATTRACT_INIT           ; -> ATTRACT_INIT
1183:  18 D8        jr opmenu_loop               ; -> opmenu_loop

opmenu_zero_credits:
1185:  FD 36 AD 00  ld (iy-83),$00              
1189:  18 AC        jr OPERATOR_MENU             ; -> OPERATOR_MENU

opmenu_wipe_check:
118B:  FD CB B3 7E  bit 7,(iy-77)               
118F:  C2 3B 0A     jp nz,NVRAM_WIPE             ; -> NVRAM_WIPE
1192:  18 E8        jr opmenu_loop_117C         

ATTRACT_FRAME_TAIL:
1194:  3A 8B 40     ld a,(page_line_total)       ; page_line_total
1197:  FD BE 09     cp (iy+9)                   
119A:  D4 6D 2B     call nc,PAGE_SCRIPT_STEP     ; -> PAGE_SCRIPT_STEP
119D:  CD D5 17     call VG_KICK_TITLE           ; -> VG_KICK_TITLE
11A0:  FD CB AE 76  bit 6,(iy-82)               
11A4:  C2 9D 0A     jp nz,ATTRACT_INIT           ; -> ATTRACT_INIT
11A7:  C3 D2 12     jp FRAME_ENGINE              ; -> FRAME_ENGINE

START_BTN_CHECK:
11AA:  3A 2D 40     ld a,(credits_bcd)           ; credits_bcd
11AD:  B7           or a                        
11AE:  CA 47 12     jp z,start_none              ; -> start_none
11B1:  FD CB 97 76  bit 6,(iy-105)              
11B5:  28 1D        jr z,START_BTN_CHECK_11D4   
11B7:  F3           di                          
11B8:  FD 46 F2     ld b,(iy-14)                
11BB:  CB 90        res 2,b                     
11BD:  FE 02        cp $02                      
11BF:  38 0A        jr c,START_BTN_CHECK_11CB   
11C1:  CB 98        res 3,b                     
11C3:  CB A0        res 4,b                     
11C5:  FE 04        cp $04                      
11C7:  38 02        jr c,START_BTN_CHECK_11CB   
11C9:  CB A8        res 5,b                     

START_BTN_CHECK_11CB:
11CB:  78           ld a,b                      
11CC:  D3 13        out ($13),a                  ; LEDS/coin counters/flip
11CE:  32 72 40     ld (led_shadow),a            ; led_shadow
11D1:  FB           ei                          
11D2:  18 03        jr start_led_blink           ; -> start_led_blink

START_BTN_CHECK_11D4:
11D4:  CD 48 12     call START_LEDS_OFF          ; -> START_LEDS_OFF

start_led_blink:
11D7:  3A 2D 40     ld a,(credits_bcd)           ; credits_bcd
11DA:  FE 02        cp $02                      
11DC:  38 52        jr c,start_2p_setup_1230    
11DE:  FE 04        cp $04                      
11E0:  38 14        jr c,start_led_blink_11F6   
11E2:  FD CB B3 5E  bit 3,(iy-77)               
11E6:  28 0E        jr z,start_led_blink_11F6   
11E8:  FD 36 C1 00  ld (iy-63),$00              
11EC:  FD 36 C0 04  ld (iy-64),$04              
11F0:  FD CB C1 EE  set 5,(iy-63)               
11F4:  18 0E        jr start_2p_setup            ; -> start_2p_setup

start_led_blink_11F6:
11F6:  FD CB B3 56  bit 2,(iy-77)               
11FA:  28 20        jr z,start_2p_setup_121C    
11FC:  FD 36 C1 00  ld (iy-63),$00              
1200:  FD 36 C0 02  ld (iy-64),$02              

start_2p_setup:
1204:  FD 36 BF 02  ld (iy-65),$02              
1208:  FD 36 BE 02  ld (iy-66),$02              
120C:  FD CB C1 F6  set 6,(iy-63)               
1210:  DB 17        in a,($17)                   ; DSW C6
1212:  CB 7F        bit 7,a                     
1214:  28 28        jr z,start_2p_setup_123E    
1216:  FD CB C1 D6  set 2,(iy-63)               
121A:  18 22        jr start_2p_setup_123E      

start_2p_setup_121C:
121C:  FD CB B3 7E  bit 7,(iy-77)               
1220:  28 0E        jr z,start_2p_setup_1230    
1222:  FD 36 C1 00  ld (iy-63),$00              
1226:  FD CB C1 EE  set 5,(iy-63)               
122A:  FD 36 C0 02  ld (iy-64),$02              
122E:  18 0E        jr start_2p_setup_123E      

start_2p_setup_1230:
1230:  FD CB B3 76  bit 6,(iy-77)               
1234:  28 11        jr z,start_none              ; -> start_none
1236:  FD 36 C1 00  ld (iy-63),$00              
123A:  FD 36 C0 01  ld (iy-64),$01              

start_2p_setup_123E:
123E:  CD 48 12     call START_LEDS_OFF          ; -> START_LEDS_OFF
1241:  FD 36 03 01  ld (iy+3),$01               
1245:  F6 FF        or $FF                      

start_none:
1247:  C9           ret                         

START_LEDS_OFF:
1248:  F3           di                          
1249:  3A 72 40     ld a,(led_shadow)            ; led_shadow
124C:  F6 3C        or $3C                      
124E:  D3 13        out ($13),a                  ; LEDS/coin counters/flip
1250:  32 72 40     ld (led_shadow),a            ; led_shadow
1253:  FB           ei                          
1254:  C9           ret                         

START_LEDS_OFF_1255:
1255:  FD CB C1 56  bit 2,(iy-63)               
1259:  28 1C        jr z,START_LEDS_OFF_1277    
125B:  F3           di                          
125C:  3A 3E 40     ld a,(cur_player_num)        ; cur_player_num
125F:  FD AE EE     xor (iy-18)                 
1262:  CB 47        bit 0,a                     
1264:  28 07        jr z,START_LEDS_OFF_126D    
1266:  FD CB F2 B6  res 6,(iy-14)               
126A:  18 05        jr START_LEDS_OFF_1271      

START_LEDS_OFF_126C:
126C:  F3           di                          

START_LEDS_OFF_126D:
126D:  FD CB F2 F6  set 6,(iy-14)               

START_LEDS_OFF_1271:
1271:  3A 72 40     ld a,(led_shadow)            ; led_shadow
1274:  D3 13        out ($13),a                  ; LEDS/coin counters/flip
1276:  FB           ei                          

START_LEDS_OFF_1277:
1277:  C9           ret                         
1278:  db  C3 83 0B                                        ; ...

HISCORE_ENTRY_FRAME:
127B:  CD 0D 18     call VG_KICK_TITLE_180D     
127E:  CD 35 2A     call HISCORE_INITIALS        ; -> HISCORE_INITIALS
1281:  CD AA 11     call START_BTN_CHECK         ; -> START_BTN_CHECK
1284:  C2 83 0B     jp nz,GAME_START             ; -> GAME_START
1287:  3A 24 40     ld a,(game_mode)             ; game_mode
128A:  FE 04        cp $04                      
128C:  28 44        jr z,FRAME_ENGINE            ; -> FRAME_ENGINE
128E:  FD 36 03 0C  ld (iy+3),$0C               
1292:  01 2A 00     ld bc,$002A                 
1295:  21 A9 43     ld hl,hiscore_work           ; hiscore_work
1298:  11 D3 43     ld de,$43D3                 
129B:  FD CB C1 6E  bit 5,(iy-63)               
129F:  28 03        jr z,HISCORE_ENTRY_FRAME_12A4
12A1:  11 FD 43     ld de,$43FD                 

HISCORE_ENTRY_FRAME_12A4:
12A4:  ED B0        ldir                        
12A6:  FD CB C1 E6  set 4,(iy-63)               
12AA:  FD 35 BF     dec (iy-65)                 
12AD:  28 19        jr z,HISCORE_ENTRY_FRAME_12C8
12AF:  FD CB C1 5E  bit 3,(iy-63)               
12B3:  20 09        jr nz,HISCORE_ENTRY_FRAME_12BE
12B5:  FD 36 BE 02  ld (iy-66),$02              
12B9:  21 53 40     ld hl,p2_save                ; p2_save
12BC:  18 07        jr HISCORE_ENTRY_FRAME_12C5 

HISCORE_ENTRY_FRAME_12BE:
12BE:  FD 36 BE 01  ld (iy-66),$01              
12C2:  21 46 40     ld hl,p1_save                ; p1_save

HISCORE_ENTRY_FRAME_12C5:
12C5:  C3 78 14     jp fe_respawn_tick_1478     

HISCORE_ENTRY_FRAME_12C8:
12C8:  FD CB C1 6E  bit 5,(iy-63)               
12CC:  CA 99 10     jp z,att_step5_scores        ; -> att_step5_scores
12CF:  C3 B5 10     jp att_step6_scores2         ; -> att_step6_scores2

FRAME_ENGINE:
12D2:  CD 37 17     call SPINNER_READ            ; -> SPINNER_READ
12D5:  EE 3F        xor $3F                     
12D7:  E6 3F        and $3F                     
12D9:  5F           ld e,a                      
12DA:  16 00        ld d,$00                    
12DC:  21 61 30     ld hl,$3061                 
12DF:  19           add hl,de                   
12E0:  7E           ld a,(hl)                   
12E1:  EE 3F        xor $3F                     
12E3:  47           ld b,a                      
12E4:  3A 24 40     ld a,(game_mode)             ; game_mode
12E7:  FE 03        cp $03                      
12E9:  38 0E        jr c,fe_thrust_input         ; -> fe_thrust_input
12EB:  FE 04        cp $04                      
12ED:  28 06        jr z,FRAME_ENGINE_12F5      
12EF:  FD CB 33 6E  bit 5,(iy+51)               
12F3:  20 04        jr nz,fe_thrust_input        ; -> fe_thrust_input

FRAME_ENGINE_12F5:
12F5:  78           ld a,b                      
12F6:  32 BC 40     ld (ship_angle),a            ; ship_angle

fe_thrust_input:
12F9:  FD CB AF 6E  bit 5,(iy-81)               
12FD:  20 0A        jr nz,fe_fire_request        ; -> fe_fire_request
12FF:  FD CB 32 6E  bit 5,(iy+50)               
1303:  28 04        jr z,fe_fire_request         ; -> fe_fire_request
1305:  FD CB 32 C6  set 0,(iy+50)               

fe_fire_request:
1309:  FD CB B0 76  bit 6,(iy-80)               
130D:  28 14        jr z,fe_death_check          ; -> fe_death_check
130F:  3A 70 40     ld a,(fire_cooldown)         ; fire_cooldown
1312:  B7           or a                        
1313:  20 0E        jr nz,fe_death_check         ; -> fe_death_check
1315:  FD CB 33 6E  bit 5,(iy+51)               
1319:  20 08        jr nz,fe_death_check         ; -> fe_death_check
131B:  FD CB 32 D6  set 2,(iy+50)               
131F:  FD 36 F0 02  ld (iy-16),$02              

fe_death_check:
1323:  FD CB 32 7E  bit 7,(iy+50)               
1327:  28 0D        jr z,fe_death_check_1336    
1329:  3A 77 40     ld a,(shooter_state)         ; shooter_state
132C:  B7           or a                        
132D:  C2 94 14     jp nz,fe_tail_sounds         ; -> fe_tail_sounds
1330:  FD CB 33 6E  bit 5,(iy+51)               
1334:  28 14        jr z,fe_death_start          ; -> fe_death_start

fe_death_check_1336:
1336:  FD CB EB 6E  bit 5,(iy-21)               
133A:  20 5A        jr nz,fe_state_gate          ; -> fe_state_gate
133C:  FD CB EB EE  set 5,(iy-21)               
1340:  FD 36 A1 0A  ld (iy-95),$0A              
1344:  FD 36 A3 04  ld (iy-93),$04              
1348:  18 4C        jr fe_state_gate             ; -> fe_state_gate

fe_death_start:
134A:  FD CB EB 6E  bit 5,(iy-21)               
134E:  20 46        jr nz,fe_state_gate          ; -> fe_state_gate
1350:  FD CB EB EE  set 5,(iy-21)               
1354:  DD 21 0E 00  ld ix,$000E                 
1358:  DD CB 00 BE  res 7,(ix+0)                
135C:  DD 21 0F 00  ld ix,$000F                 
1360:  DD CB 00 BE  res 7,(ix+0)                
1364:  DD 21 10 00  ld ix,$0010                 
1368:  DD CB 00 BE  res 7,(ix+0)                
136C:  DD 21 11 00  ld ix,$0011                 
1370:  DD CB 00 BE  res 7,(ix+0)                
1374:  FD 36 E9 00  ld (iy-23),$00              
1378:  3E 00        ld a,$00                    
137A:  CD 04 2E     call SOUND_CMD_SEND          ; -> SOUND_CMD_SEND
137D:  FD 36 A1 00  ld (iy-95),$00              
1381:  3E 04        ld a,$04                    
1383:  CD 04 2E     call SOUND_CMD_SEND          ; -> SOUND_CMD_SEND
1386:  FD 36 A1 05  ld (iy-95),$05              
138A:  FD 36 A3 05  ld (iy-93),$05              
138E:  FD 36 97 00  ld (iy-105),$00             
1392:  FD 36 98 00  ld (iy-104),$00             

fe_state_gate:
1396:  3A 23 40     ld a,(wave_start_timer)      ; wave_start_timer
1399:  FE 03        cp $03                      
139B:  D2 94 14     jp nc,fe_tail_sounds         ; -> fe_tail_sounds
139E:  FD CB 33 6E  bit 5,(iy+51)               
13A2:  20 07        jr nz,fe_gameover_check      ; -> fe_gameover_check
13A4:  FD CB 32 7E  bit 7,(iy+50)               
13A8:  C2 98 0C     jp nz,PLAY_FRAME             ; -> PLAY_FRAME

fe_gameover_check:
13AB:  FD CB 32 7E  bit 7,(iy+50)               
13AF:  C2 94 14     jp nz,fe_tail_sounds         ; -> fe_tail_sounds
13B2:  3A 66 40     ld a,(lives)                 ; lives
13B5:  FE 01        cp $01                      
13B7:  20 71        jr nz,fe_respawn_tick        ; -> fe_respawn_tick
13B9:  FD CB AE 7E  bit 7,(iy-82)               
13BD:  28 0D        jr z,fe_gameover_check_13CC 
13BF:  FD CB EA 4E  bit 1,(iy-22)               
13C3:  20 07        jr nz,fe_gameover_check_13CC
13C5:  FD CB EA CE  set 1,(iy-22)               
13C9:  CD 40 15     call AUDIT_SNAPSHOT          ; -> AUDIT_SNAPSHOT

fe_gameover_check_13CC:
13CC:  FD CB C1 76  bit 6,(iy-63)               
13D0:  28 07        jr z,fe_join_check           ; -> fe_join_check
13D2:  3A 3E 40     ld a,(cur_player_num)        ; cur_player_num
13D5:  FE 02        cp $02                      
13D7:  20 51        jr nz,fe_respawn_tick        ; -> fe_respawn_tick

fe_join_check:
13D9:  CD AA 11     call START_BTN_CHECK         ; -> START_BTN_CHECK
13DC:  C2 83 0B     jp nz,GAME_START             ; -> GAME_START
13DF:  FD CB C1 4E  bit 1,(iy-63)               
13E3:  20 32        jr nz,fe_spawn_gate          ; -> fe_spawn_gate
13E5:  FD CB C1 CE  set 1,(iy-63)               
13E9:  06 20        ld b,$20                    

fe_join_check_13EB:
13EB:  CD B2 29     call OBJ_KILL                ; -> OBJ_KILL
13EE:  10 FB        djnz fe_join_check_13EB     
13F0:  FD 36 A2 05  ld (iy-94),$05              
13F4:  3E 00        ld a,$00                    
13F6:  CD 04 2E     call SOUND_CMD_SEND          ; -> SOUND_CMD_SEND
13F9:  01 1A 00     ld bc,$001A                 
13FC:  11 36 83     ld de,$8336                 
13FF:  21 00 D0     ld hl,$D000                 
1402:  22 34 83     ld ($8334),hl               
1405:  21 34 83     ld hl,$8334                 
1408:  ED B0        ldir                        
140A:  2A F3 3E     ld hl,($3EF3)               
140D:  22 42 82     ld ($8242),hl               
1410:  FD 36 04 0E  ld (iy+4),$0E               
1414:  CD F3 2A     call DRAW_PAGE               ; -> DRAW_PAGE

fe_spawn_gate:
1417:  3A 8B 40     ld a,(page_line_total)       ; page_line_total
141A:  FD BE 09     cp (iy+9)                   
141D:  38 05        jr c,fe_spawn_gate_1424     
141F:  CD 6D 2B     call PAGE_SCRIPT_STEP        ; -> PAGE_SCRIPT_STEP
1422:  18 70        jr fe_tail_sounds            ; -> fe_tail_sounds

fe_spawn_gate_1424:
1424:  3A 22 40     ld a,(wave_pace_timer)       ; wave_pace_timer
1427:  B7           or a                        
1428:  20 6A        jr nz,fe_tail_sounds         ; -> fe_tail_sounds

fe_respawn_tick:
142A:  3E 00        ld a,$00                    
142C:  CD 04 2E     call SOUND_CMD_SEND          ; -> SOUND_CMD_SEND
142F:  FD 35 E6     dec (iy-26)                 
1432:  C2 98 0C     jp nz,PLAY_FRAME             ; -> PLAY_FRAME
1435:  FD CB EB B6  res 6,(iy-21)               
1439:  FD CB AE 7E  bit 7,(iy-82)               
143D:  CA 9D 0A     jp z,ATTRACT_INIT            ; -> ATTRACT_INIT
1440:  FD CB C1 76  bit 6,(iy-63)               
1444:  28 35        jr z,fe_next_player          ; -> fe_next_player
1446:  3A 3E 40     ld a,(cur_player_num)        ; cur_player_num
1449:  FE 02        cp $02                      
144B:  C2 98 0C     jp nz,PLAY_FRAME             ; -> PLAY_FRAME
144E:  21 53 40     ld hl,p2_save                ; p2_save
1451:  CD 8E 2C     call PLAYER_STATE_SAVE       ; -> PLAYER_STATE_SAVE
1454:  06 04        ld b,$04                    
1456:  11 4B 40     ld de,$404B                 
1459:  21 58 40     ld hl,$4058                 
145C:  AF           xor a                       

fe_respawn_tick_145D:
145D:  1A           ld a,(de)                   
145E:  9E           sbc a,(hl)                  
145F:  27           daa                         
1460:  13           inc de                      
1461:  23           inc hl                      
1462:  10 F9        djnz fe_respawn_tick_145D   
1464:  FD 36 BE 01  ld (iy-66),$01              
1468:  21 46 40     ld hl,p1_save                ; p1_save
146B:  30 0B        jr nc,fe_respawn_tick_1478  
146D:  FD 36 BE 02  ld (iy-66),$02              
1471:  21 53 40     ld hl,p2_save                ; p2_save
1474:  FD CB C1 DE  set 3,(iy-63)               

fe_respawn_tick_1478:
1478:  CD BD 2C     call PLAYER_STATE_LOAD       ; -> PLAYER_STATE_LOAD

fe_next_player:
147B:  CD CD 29     call HISCORE_CHECK           ; -> HISCORE_CHECK
147E:  3A 24 40     ld a,(game_mode)             ; game_mode
1481:  FE 04        cp $04                      
1483:  CA E6 0E     jp z,SHIP_RESPAWN            ; -> SHIP_RESPAWN
1486:  FD 36 03 09  ld (iy+3),$09               
148A:  FD CB C1 66  bit 4,(iy-63)               
148E:  C2 C8 12     jp nz,HISCORE_ENTRY_FRAME_12C8
1491:  C3 9D 0A     jp ATTRACT_INIT              ; -> ATTRACT_INIT

fe_tail_sounds:
1494:  FD CB 33 6E  bit 5,(iy+51)               
1498:  20 18        jr nz,fe_halfplane_toggle    ; -> fe_halfplane_toggle
149A:  FD CB B0 6E  bit 5,(iy-80)               
149E:  28 07        jr z,fe_tail_sounds_14A7    
14A0:  3E 09        ld a,$09                    
14A2:  CD 04 2E     call SOUND_CMD_SEND          ; -> SOUND_CMD_SEND
14A5:  18 0B        jr fe_halfplane_toggle       ; -> fe_halfplane_toggle

fe_tail_sounds_14A7:
14A7:  FD CB B1 6E  bit 5,(iy-79)               
14AB:  28 05        jr z,fe_halfplane_toggle     ; -> fe_halfplane_toggle
14AD:  3E 0A        ld a,$0A                    
14AF:  CD 04 2E     call SOUND_CMD_SEND          ; -> SOUND_CMD_SEND

fe_halfplane_toggle:
14B2:  FD CB EB 66  bit 4,(iy-21)               
14B6:  20 39        jr nz,fe_halfplane_toggle_14F1
14B8:  3A B9 40     ld a,(ship_x)                ; ship_x
14BB:  FE D0        cp $D0                      
14BD:  30 32        jr nc,fe_halfplane_toggle_14F1
14BF:  FE 30        cp $30                      
14C1:  38 2E        jr c,fe_halfplane_toggle_14F1
14C3:  FD CB EB E6  set 4,(iy-21)               
14C7:  3A BB 40     ld a,(ship_y)                ; ship_y
14CA:  FE 80        cp $80                      
14CC:  38 23        jr c,fe_halfplane_toggle_14F1
14CE:  3A 67 40     ld a,(wave_num)              ; wave_num
14D1:  CB 2F        sra a                       
14D3:  47           ld b,a                      
14D4:  C6 01        add a,$01                   
14D6:  CB 27        sla a                       
14D8:  4F           ld c,a                      
14D9:  CB 27        sla a                       
14DB:  81           add a,c                     
14DC:  4F           ld c,a                      

fe_halfplane_toggle_14DD:
14DD:  21 90 2E     ld hl,$2E90                 
14E0:  59           ld e,c                      
14E1:  16 00        ld d,$00                    
14E3:  19           add hl,de                   
14E4:  5E           ld e,(hl)                   
14E5:  23           inc hl                      
14E6:  56           ld d,(hl)                   
14E7:  1A           ld a,(de)                   
14E8:  EE 02        xor $02                     
14EA:  12           ld (de),a                   
14EB:  3E 06        ld a,$06                    
14ED:  81           add a,c                     
14EE:  4F           ld c,a                      
14EF:  10 EC        djnz fe_halfplane_toggle_14DD

fe_halfplane_toggle_14F1:
14F1:  C3 39 0F     jp MAINLOOP                  ; -> MAINLOOP

INPUTS_SAMPLE:
14F4:  FD 56 AF     ld d,(iy-81)                
14F7:  CD 11 17     call P1_INPUT_READ           ; -> P1_INPUT_READ
14FA:  32 2F 40     ld (in1_state),a             ; in1_state
14FD:  5F           ld e,a                      
14FE:  A2           and d                       
14FF:  AA           xor d                       
1500:  32 30 40     ld (in1_pressed),a           ; in1_pressed
1503:  7B           ld a,e                      
1504:  B2           or d                        
1505:  AA           xor d                       
1506:  32 31 40     ld (in1_released),a          ; in1_released
1509:  FD 56 B2     ld d,(iy-78)                
150C:  DB 12        in a,($12)                   ; IN P2/start
150E:  32 32 40     ld (in2_state),a             ; in2_state
1511:  5F           ld e,a                      
1512:  A2           and d                       
1513:  AA           xor d                       
1514:  32 33 40     ld (in2_pressed),a           ; in2_pressed
1517:  C9           ret                         

BOOKKEEP_COIN:
1518:  E5           push hl                     
1519:  21 F7 3F     ld hl,$3FF7                 
151C:  CD 21 15     call BOOKKEEP_ADD            ; -> BOOKKEEP_ADD
151F:  E1           pop hl                      
1520:  C9           ret                         

BOOKKEEP_ADD:
1521:  F5           push af                     
1522:  C5           push bc                     
1523:  E5           push hl                     
1524:  B7           or a                        
1525:  07           rlca                        
1526:  07           rlca                        
1527:  4F           ld c,a                      
1528:  06 00        ld b,$00                    
152A:  7E           ld a,(hl)                   
152B:  21 27 44     ld hl,bookkeep_ctrs          ; bookkeep_ctrs
152E:  09           add hl,bc                   
152F:  06 03        ld b,$03                    
1531:  86           add a,(hl)                  
1532:  27           daa                         
1533:  77           ld (hl),a                   

BOOKKEEP_ADD_1534:
1534:  23           inc hl                      
1535:  7E           ld a,(hl)                   
1536:  CE 00        adc a,$00                   
1538:  27           daa                         
1539:  77           ld (hl),a                   
153A:  10 F8        djnz BOOKKEEP_ADD_1534      
153C:  E1           pop hl                      
153D:  C1           pop bc                      
153E:  F1           pop af                      
153F:  C9           ret                         

AUDIT_SNAPSHOT:
1540:  FD 36 12 00  ld (iy+18),$00              
1544:  FD CB C1 6E  bit 5,(iy-63)               
1548:  20 07        jr nz,AUDIT_SNAPSHOT_1551   
154A:  3E 04        ld a,$04                    
154C:  11 33 44     ld de,$4433                 
154F:  18 09        jr AUDIT_SNAPSHOT_155A      

AUDIT_SNAPSHOT_1551:
1551:  3E 0A        ld a,$0A                    
1553:  11 35 44     ld de,$4435                 
1556:  FD CB 12 FE  set 7,(iy+18)               

AUDIT_SNAPSHOT_155A:
155A:  ED 53 67 44  ld ($4467),de               
155E:  FD CB EA 6E  bit 5,(iy-22)               
1562:  C4 18 15     call nz,BOOKKEEP_COIN        ; -> BOOKKEEP_COIN
1565:  3C           inc a                       
1566:  FD CB EA 66  bit 4,(iy-22)               
156A:  C4 18 15     call nz,BOOKKEEP_COIN        ; -> BOOKKEEP_COIN
156D:  3C           inc a                       
156E:  FD CB EA 5E  bit 3,(iy-22)               
1572:  C4 18 15     call nz,BOOKKEEP_COIN        ; -> BOOKKEEP_COIN
1575:  3C           inc a                       
1576:  21 36 40     ld hl,score_cur              ; score_cur
1579:  CD 8C 15     call BOOKKEEP_DECAY_AVG      ; -> BOOKKEEP_DECAY_AVG
157C:  3C           inc a                       
157D:  FD CB 12 F6  set 6,(iy+18)               
1581:  21 3A 40     ld hl,time_played_bcd        ; time_played_bcd
1584:  CD 8C 15     call BOOKKEEP_DECAY_AVG      ; -> BOOKKEEP_DECAY_AVG
1587:  3C           inc a                       
1588:  CD 14 16     call BOOKKEEP_HIGHWATER      ; -> BOOKKEEP_HIGHWATER
158B:  C9           ret                         

BOOKKEEP_DECAY_AVG:
158C:  F5           push af                     
158D:  C5           push bc                     
158E:  D5           push de                     
158F:  E5           push hl                     
1590:  CD 39 16     call BOOKKEEP_PTR            ; -> BOOKKEEP_PTR
1593:  E5           push hl                     
1594:  13           inc de                      
1595:  1A           ld a,(de)                   
1596:  B7           or a                        
1597:  28 3C        jr z,BOOKKEEP_DECAY_AVG_15D5
1599:  23           inc hl                      
159A:  11 69 44     ld de,$4469                 
159D:  01 03 00     ld bc,$0003                 
15A0:  ED B0        ldir                        
15A2:  AF           xor a                       
15A3:  12           ld (de),a                   
15A4:  06 04        ld b,$04                    
15A6:  D1           pop de                      
15A7:  D5           push de                     
15A8:  21 69 44     ld hl,$4469                 

BOOKKEEP_DECAY_AVG_15AB:
15AB:  1A           ld a,(de)                   
15AC:  9E           sbc a,(hl)                  
15AD:  27           daa                         
15AE:  12           ld (de),a                   
15AF:  13           inc de                      
15B0:  23           inc hl                      
15B1:  10 F8        djnz BOOKKEEP_DECAY_AVG_15AB
15B3:  AF           xor a                       
15B4:  06 03        ld b,$03                    
15B6:  D1           pop de                      
15B7:  E1           pop hl                      
15B8:  E5           push hl                     
15B9:  FD CB 12 76  bit 6,(iy+18)               
15BD:  20 01        jr nz,BOOKKEEP_DECAY_AVG_15C0
15BF:  23           inc hl                      

BOOKKEEP_DECAY_AVG_15C0:
15C0:  1A           ld a,(de)                   
15C1:  8E           adc a,(hl)                  
15C2:  27           daa                         
15C3:  12           ld (de),a                   
15C4:  13           inc de                      
15C5:  23           inc hl                      
15C6:  10 F8        djnz BOOKKEEP_DECAY_AVG_15C0
15C8:  FD CB 12 76  bit 6,(iy+18)               
15CC:  20 41        jr nz,BOOKKEEP_DECAY_AVG_160F
15CE:  1A           ld a,(de)                   
15CF:  CE 00        adc a,$00                   
15D1:  27           daa                         
15D2:  12           ld (de),a                   
15D3:  18 3A        jr BOOKKEEP_DECAY_AVG_160F  

BOOKKEEP_DECAY_AVG_15D5:
15D5:  06 04        ld b,$04                    
15D7:  E1           pop hl                      
15D8:  D1           pop de                      
15D9:  D5           push de                     
15DA:  E5           push hl                     
15DB:  AF           xor a                       

BOOKKEEP_DECAY_AVG_15DC:
15DC:  1A           ld a,(de)                   
15DD:  8E           adc a,(hl)                  
15DE:  27           daa                         
15DF:  77           ld (hl),a                   
15E0:  13           inc de                      
15E1:  23           inc hl                      
15E2:  10 F8        djnz BOOKKEEP_DECAY_AVG_15DC
15E4:  2A 67 44     ld hl,($4467)               
15E7:  7E           ld a,(hl)                   
15E8:  C6 01        add a,$01                   
15EA:  27           daa                         
15EB:  38 0A        jr c,BOOKKEEP_DECAY_AVG_15F7
15ED:  FD CB 12 76  bit 6,(iy+18)               
15F1:  28 01        jr z,BOOKKEEP_DECAY_AVG_15F4
15F3:  77           ld (hl),a                   

BOOKKEEP_DECAY_AVG_15F4:
15F4:  E1           pop hl                      
15F5:  18 18        jr BOOKKEEP_DECAY_AVG_160F  

BOOKKEEP_DECAY_AVG_15F7:
15F7:  FD CB 12 76  bit 6,(iy+18)               
15FB:  20 0E        jr nz,BOOKKEEP_DECAY_AVG_160B
15FD:  E1           pop hl                      
15FE:  54           ld d,h                      
15FF:  5D           ld e,l                      
1600:  23           inc hl                      
1601:  01 03 00     ld bc,$0003                 
1604:  ED B0        ldir                        
1606:  3E 00        ld a,$00                    
1608:  12           ld (de),a                   
1609:  18 04        jr BOOKKEEP_DECAY_AVG_160F  

BOOKKEEP_DECAY_AVG_160B:
160B:  77           ld (hl),a                   
160C:  23           inc hl                      
160D:  34           inc (hl)                    
160E:  E1           pop hl                      

BOOKKEEP_DECAY_AVG_160F:
160F:  E1           pop hl                      
1610:  D1           pop de                      
1611:  C1           pop bc                      
1612:  F1           pop af                      
1613:  C9           ret                         

BOOKKEEP_HIGHWATER:
1614:  F5           push af                     
1615:  C5           push bc                     
1616:  D5           push de                     
1617:  E5           push hl                     
1618:  CD 39 16     call BOOKKEEP_PTR            ; -> BOOKKEEP_PTR
161B:  E5           push hl                     
161C:  AF           xor a                       
161D:  06 04        ld b,$04                    
161F:  11 3A 40     ld de,time_played_bcd        ; time_played_bcd

BOOKKEEP_HIGHWATER_1622:
1622:  1A           ld a,(de)                   
1623:  9E           sbc a,(hl)                  
1624:  27           daa                         
1625:  23           inc hl                      
1626:  13           inc de                      
1627:  10 F9        djnz BOOKKEEP_HIGHWATER_1622
1629:  D1           pop de                      
162A:  38 08        jr c,BOOKKEEP_HIGHWATER_1634
162C:  01 04 00     ld bc,$0004                 
162F:  21 3A 40     ld hl,time_played_bcd        ; time_played_bcd
1632:  ED B0        ldir                        

BOOKKEEP_HIGHWATER_1634:
1634:  E1           pop hl                      
1635:  D1           pop de                      
1636:  C1           pop bc                      
1637:  F1           pop af                      
1638:  C9           ret                         

BOOKKEEP_PTR:
1639:  C5           push bc                     
163A:  B7           or a                        
163B:  07           rlca                        
163C:  07           rlca                        
163D:  4F           ld c,a                      
163E:  06 00        ld b,$00                    
1640:  21 27 44     ld hl,bookkeep_ctrs          ; bookkeep_ctrs
1643:  09           add hl,bc                   
1644:  C1           pop bc                      
1645:  C9           ret                         

COINAGE_DISP_PREP:
1646:  C5           push bc                     
1647:  D5           push de                     
1648:  E5           push hl                     
1649:  03           inc bc                      
164A:  0A           ld a,(bc)                   
164B:  B7           or a                        
164C:  28 07        jr z,COINAGE_DISP_PREP_1655 
164E:  01 08 00     ld bc,$0008                 
1651:  ED B0        ldir                        
1653:  18 1B        jr COINAGE_DISP_PREP_1670   

COINAGE_DISP_PREP_1655:
1655:  0B           dec bc                      
1656:  0A           ld a,(bc)                   
1657:  32 93 40     ld ($4093),a                
165A:  CD 74 16     call BCD_DIV4                ; -> BCD_DIV4
165D:  CD 74 16     call BCD_DIV4                ; -> BCD_DIV4
1660:  E1           pop hl                      
1661:  D1           pop de                      
1662:  D5           push de                     
1663:  E5           push hl                     
1664:  21 06 00     ld hl,$0006                 
1667:  19           add hl,de                   
1668:  54           ld d,h                      
1669:  5D           ld e,l                      
166A:  13           inc de                      
166B:  01 03 00     ld bc,$0003                 
166E:  ED B8        lddr                        

COINAGE_DISP_PREP_1670:
1670:  E1           pop hl                      
1671:  D1           pop de                      
1672:  C1           pop bc                      
1673:  C9           ret                         

BCD_DIV4:
1674:  C5           push bc                     
1675:  D5           push de                     
1676:  01 04 00     ld bc,$0004                 
1679:  11 69 44     ld de,$4469                 
167C:  ED B0        ldir                        
167E:  E5           push hl                     
167F:  01 03 00     ld bc,$0003                 
1682:  11 6E 44     ld de,$446E                 
1685:  21 6D 44     ld hl,$446D                 
1688:  36 00        ld (hl),$00                 
168A:  ED B0        ldir                        
168C:  3A 93 40     ld a,($4093)                
168F:  B7           or a                        
1690:  28 33        jr z,BCD_DIV4_16C5          

BCD_DIV4_1692:
1692:  DB 09        in a,($09)                   ; WATCHDOG
1694:  21 69 44     ld hl,$4469                 
1697:  7E           ld a,(hl)                   
1698:  FD 96 13     sub (iy+19)                 
169B:  27           daa                         
169C:  77           ld (hl),a                   
169D:  30 0E        jr nc,BCD_DIV4_16AD         
169F:  06 03        ld b,$03                    

BCD_DIV4_16A1:
16A1:  23           inc hl                      
16A2:  7E           ld a,(hl)                   
16A3:  DE 00        sbc a,$00                   
16A5:  27           daa                         
16A6:  77           ld (hl),a                   
16A7:  30 04        jr nc,BCD_DIV4_16AD         
16A9:  10 F6        djnz BCD_DIV4_16A1          
16AB:  18 18        jr BCD_DIV4_16C5            

BCD_DIV4_16AD:
16AD:  21 6D 44     ld hl,$446D                 
16B0:  7E           ld a,(hl)                   
16B1:  C6 01        add a,$01                   
16B3:  27           daa                         
16B4:  77           ld (hl),a                   
16B5:  30 DB        jr nc,BCD_DIV4_1692         
16B7:  06 03        ld b,$03                    

BCD_DIV4_16B9:
16B9:  23           inc hl                      
16BA:  7E           ld a,(hl)                   
16BB:  CE 00        adc a,$00                   
16BD:  27           daa                         
16BE:  77           ld (hl),a                   
16BF:  30 D1        jr nc,BCD_DIV4_1692         
16C1:  10 F6        djnz BCD_DIV4_16B9          
16C3:  18 CD        jr BCD_DIV4_1692            

BCD_DIV4_16C5:
16C5:  01 04 00     ld bc,$0004                 
16C8:  E1           pop hl                      
16C9:  D1           pop de                      
16CA:  D5           push de                     
16CB:  E5           push hl                     
16CC:  21 6D 44     ld hl,$446D                 
16CF:  ED B0        ldir                        
16D1:  E1           pop hl                      
16D2:  C1           pop bc                      
16D3:  C1           pop bc                      
16D4:  C9           ret                         

BCD_ADD4:
16D5:  C5           push bc                     
16D6:  AF           xor a                       
16D7:  06 04        ld b,$04                    

BCD_ADD4_16D9:
16D9:  1A           ld a,(de)                   
16DA:  8E           adc a,(hl)                  
16DB:  27           daa                         
16DC:  12           ld (de),a                   
16DD:  23           inc hl                      
16DE:  13           inc de                      
16DF:  10 F8        djnz BCD_ADD4_16D9          
16E1:  C1           pop bc                      
16E2:  C9           ret                         

CREDITS_NVRAM_SYNC:
16E3:  3A 2D 40     ld a,(credits_bcd)           ; credits_bcd
16E6:  32 56 5C     ld (nvram_credits),a         ; nvram_credits
16E9:  0F           rrca                        
16EA:  0F           rrca                        
16EB:  0F           rrca                        
16EC:  0F           rrca                        
16ED:  32 57 5C     ld ($5C57),a                
16F0:  C9           ret                         

ATTRACT_PAGE_LOAD:
16F1:  CD 23 18     call DLIST_RESET             ; -> DLIST_RESET
16F4:  2A D9 33     ld hl,($33D9)               
16F7:  22 00 80     ld (vector_ram),hl           ; vector_ram
16FA:  01 00 03     ld bc,$0300                 
16FD:  11 02 80     ld de,$8002                 
1700:  21 00 80     ld hl,vector_ram             ; vector_ram
1703:  ED B0        ldir                        
1705:  01 1A 00     ld bc,$001A                 
1708:  11 00 83     ld de,$8300                 
170B:  21 DB 33     ld hl,$33DB                 
170E:  ED B0        ldir                        
1710:  C9           ret                         

P1_INPUT_READ:
1711:  FD CB C1 56  bit 2,(iy-63)               
1715:  20 04        jr nz,P1_INPUT_READ_171B    

P1_INPUT_READ_1717:
1717:  DB 11        in a,($11)                   ; IN P1/coin/tilt/test
1719:  18 1B        jr P1_INPUT_READ_1736       

P1_INPUT_READ_171B:
171B:  3A 3E 40     ld a,(cur_player_num)        ; cur_player_num
171E:  FD AE EE     xor (iy-18)                 
1721:  CB 47        bit 0,a                     
1723:  28 F2        jr z,P1_INPUT_READ_1717     
1725:  DB 12        in a,($12)                   ; IN P2/start
1727:  E6 03        and $03                     
1729:  0F           rrca                        
172A:  0F           rrca                        
172B:  0F           rrca                        
172C:  32 34 40     ld ($4034),a                
172F:  DB 11        in a,($11)                   ; IN P1/coin/tilt/test
1731:  E6 9F        and $9F                     
1733:  FD B6 B4     or (iy-76)                  

P1_INPUT_READ_1736:
1736:  C9           ret                         

SPINNER_READ:
1737:  FD CB C1 56  bit 2,(iy-63)               
173B:  20 06        jr nz,SPINNER_READ_1743     

SPINNER_READ_173D:
173D:  DB 15        in a,($15)                   ; SPINNER 1
173F:  0F           rrca                        
1740:  0F           rrca                        
1741:  18 0C        jr SPINNER_READ_174F        

SPINNER_READ_1743:
1743:  3A 3E 40     ld a,(cur_player_num)        ; cur_player_num
1746:  FD AE EE     xor (iy-18)                 
1749:  CB 47        bit 0,a                     
174B:  28 F0        jr z,SPINNER_READ_173D      
174D:  DB 16        in a,($16)                   ; SPINNER 2

SPINNER_READ_174F:
174F:  C9           ret                         

VG_RESTART_FRAME:
1750:  DB 09        in a,($09)                   ; WATCHDOG
1752:  DB 0B        in a,($0B)                   ; VG_STATUS (d7=busy)
1754:  CB 7F        bit 7,a                     
1756:  20 7C        jr nz,VG_RESTART_FRAME_17D4 
1758:  01 B0 01     ld bc,$01B0                 
175B:  11 00 80     ld de,vector_ram             ; vector_ram
175E:  21 81 44     ld hl,dlist_shadow           ; dlist_shadow
1761:  ED B0        ldir                        
1763:  21 00 D0     ld hl,$D000                 
1766:  FD CB AF 6E  bit 5,(iy-81)               
176A:  20 0C        jr nz,VG_RESTART_FRAME_1778 
176C:  FD 34 F1     inc (iy-15)                 
176F:  FD CB F1 46  bit 0,(iy-15)               
1773:  28 03        jr z,VG_RESTART_FRAME_1778  
1775:  2A 11 46     ld hl,($4611)               

VG_RESTART_FRAME_1778:
1778:  22 8C 81     ld ($818C),hl               
177B:  FD CB AE 76  bit 6,(iy-82)               
177F:  28 19        jr z,VG_RESTART_FRAME_179A  
1781:  3A 24 40     ld a,(game_mode)             ; game_mode
1784:  FE 03        cp $03                      
1786:  20 12        jr nz,VG_RESTART_FRAME_179A 
1788:  FD CB AE B6  res 6,(iy-82)               
178C:  06 01        ld b,$01                    
178E:  11 2D 40     ld de,credits_bcd            ; credits_bcd
1791:  21 2B 46     ld hl,$462B                 
1794:  CD 0E 24     call BCD_TO_DIGITS           ; -> BCD_TO_DIGITS
1797:  CD E3 16     call CREDITS_NVRAM_SYNC      ; -> CREDITS_NVRAM_SYNC

VG_RESTART_FRAME_179A:
179A:  FD CB C1 76  bit 6,(iy-63)               
179E:  28 17        jr z,VG_RESTART_FRAME_17B7  
17A0:  FD CB 97 6E  bit 5,(iy-105)              
17A4:  20 05        jr nz,VG_RESTART_FRAME_17AB 
17A6:  21 00 F0     ld hl,$F000                 
17A9:  18 09        jr VG_RESTART_FRAME_17B4    

VG_RESTART_FRAME_17AB:
17AB:  21 C2 0D     ld hl,$0DC2                 
17AE:  7C           ld a,h                      
17AF:  E6 0F        and $0F                     
17B1:  F6 C0        or $C0                      
17B3:  67           ld h,a                      

VG_RESTART_FRAME_17B4:
17B4:  22 A6 81     ld ($81A6),hl               

VG_RESTART_FRAME_17B7:
17B7:  3A 17 40     ld a,(tick_244hz)            ; tick_244hz
17BA:  FD 96 9A     sub (iy-102)                
17BD:  4F           ld c,a                      
17BE:  FE 0A        cp $0A                      
17C0:  38 03        jr c,VG_RESTART_FRAME_17C5  
17C2:  00           nop                         
17C3:  00           nop                         
17C4:  00           nop                         

VG_RESTART_FRAME_17C5:
17C5:  3A 17 40     ld a,(tick_244hz)            ; tick_244hz
17C8:  32 1A 40     ld (last_kick_tick),a        ; last_kick_tick
17CB:  FD CB 1A 7E  bit 7,(iy+26)               
17CF:  C4 68 18     call nz,WALL_FLASH_DECAY     ; -> WALL_FLASH_DECAY
17D2:  DB 08        in a,($08)                   ; VG_GO (read starts vector generator)

VG_RESTART_FRAME_17D4:
17D4:  C9           ret                         

VG_KICK_TITLE:
17D5:  DB 09        in a,($09)                   ; WATCHDOG
17D7:  DB 0B        in a,($0B)                   ; VG_STATUS (d7=busy)
17D9:  CB 7F        bit 7,a                     
17DB:  20 2F        jr nz,VG_KICK_TITLE_180C    
17DD:  3A 24 40     ld a,(game_mode)             ; game_mode
17E0:  FE 01        cp $01                      
17E2:  20 26        jr nz,VG_KICK_TITLE_180A    
17E4:  3A 84 40     ld a,(page_id)               ; page_id
17E7:  FE 02        cp $02                      
17E9:  20 1F        jr nz,VG_KICK_TITLE_180A    
17EB:  3A 17 40     ld a,(tick_244hz)            ; tick_244hz
17EE:  E6 0C        and $0C                     
17F0:  0F           rrca                        
17F1:  5F           ld e,a                      
17F2:  16 00        ld d,$00                    
17F4:  21 4D 3F     ld hl,$3F4D                 
17F7:  19           add hl,de                   
17F8:  4E           ld c,(hl)                   
17F9:  23           inc hl                      
17FA:  46           ld b,(hl)                   
17FB:  ED 43 80 80  ld ($8080),bc               
17FF:  21 55 3F     ld hl,$3F55                 
1802:  19           add hl,de                   
1803:  4E           ld c,(hl)                   
1804:  23           inc hl                      
1805:  46           ld b,(hl)                   
1806:  ED 43 A8 80  ld ($80A8),bc               

VG_KICK_TITLE_180A:
180A:  DB 08        in a,($08)                   ; VG_GO (read starts vector generator)

VG_KICK_TITLE_180C:
180C:  C9           ret                         

VG_KICK_TITLE_180D:
180D:  DB 09        in a,($09)                   ; WATCHDOG
180F:  DB 0B        in a,($0B)                   ; VG_STATUS (d7=busy)
1811:  CB 7F        bit 7,a                     
1813:  20 0D        jr nz,VG_KICK_TITLE_1822    
1815:  01 7A 00     ld bc,$007A                 
1818:  11 00 80     ld de,vector_ram             ; vector_ram
181B:  21 81 44     ld hl,dlist_shadow           ; dlist_shadow
181E:  ED B0        ldir                        
1820:  DB 08        in a,($08)                   ; VG_GO (read starts vector generator)

VG_KICK_TITLE_1822:
1822:  C9           ret                         

DLIST_RESET:
1823:  DB 09        in a,($09)                   ; WATCHDOG
1825:  CD 54 18     call VG_WAIT_DONE            ; -> VG_WAIT_DONE
1828:  21 00 B0     ld hl,$B000                 
182B:  22 81 44     ld (dlist_shadow),hl         ; dlist_shadow
182E:  22 00 80     ld (vector_ram),hl           ; vector_ram
1831:  01 00 06     ld bc,$0600                 
1834:  11 83 44     ld de,$4483                 
1837:  21 81 44     ld hl,dlist_shadow           ; dlist_shadow
183A:  ED B0        ldir                        
183C:  DB 09        in a,($09)                   ; WATCHDOG
183E:  11 08 04     ld de,$0408                 
1841:  21 FE 0F     ld hl,$0FFE                 
1844:  B7           or a                        
1845:  ED 52        sbc hl,de                   
1847:  44           ld b,h                      
1848:  4D           ld c,l                      
1849:  11 02 80     ld de,$8002                 
184C:  21 00 80     ld hl,vector_ram             ; vector_ram
184F:  ED B0        ldir                        
1851:  DB 09        in a,($09)                   ; WATCHDOG
1853:  C9           ret                         

VG_WAIT_DONE:
1854:  DB 0B        in a,($0B)                   ; VG_STATUS (d7=busy)
1856:  CB 7F        bit 7,a                     
1858:  20 FA        jr nz,VG_WAIT_DONE           ; -> VG_WAIT_DONE
185A:  DB 09        in a,($09)                   ; WATCHDOG
185C:  C9           ret                         

WAIT_NEXT_TICK:
185D:  F5           push af                     
185E:  3A 17 40     ld a,(tick_244hz)            ; tick_244hz

WAIT_NEXT_TICK_1861:
1861:  FD BE 97     cp (iy-105)                 
1864:  28 FB        jr z,WAIT_NEXT_TICK_1861    
1866:  F1           pop af                      
1867:  C9           ret                         

WALL_FLASH_DECAY:
1868:  06 13        ld b,$13                    
186A:  11 BB 81     ld de,$81BB                 
186D:  21 9B 40     ld hl,$409B                 

WALL_FLASH_DECAY_1870:
1870:  7E           ld a,(hl)                   
1871:  B7           or a                        
1872:  28 2F        jr z,WALL_FLASH_DECAY_18A3  
1874:  FD CB EA 76  bit 6,(iy-22)               
1878:  28 29        jr z,WALL_FLASH_DECAY_18A3  
187A:  91           sub c                       
187B:  30 02        jr nc,WALL_FLASH_DECAY_187F 
187D:  3E 00        ld a,$00                    

WALL_FLASH_DECAY_187F:
187F:  77           ld (hl),a                   
1880:  38 11        jr c,WALL_FLASH_DECAY_1893  
1882:  28 0F        jr z,WALL_FLASH_DECAY_1893  
1884:  FD CB 1A F6  set 6,(iy+26)               
1888:  CB 67        bit 4,a                     
188A:  20 07        jr nz,WALL_FLASH_DECAY_1893 
188C:  1A           ld a,(de)                   
188D:  E6 0F        and $0F                     
188F:  F6 C0        or $C0                      
1891:  18 0F        jr WALL_FLASH_DECAY_18A2    

WALL_FLASH_DECAY_1893:
1893:  78           ld a,b                      
1894:  FE 0B        cp $0B                      
1896:  38 05        jr c,WALL_FLASH_DECAY_189D  
1898:  1A           ld a,(de)                   
1899:  E6 0F        and $0F                     
189B:  18 05        jr WALL_FLASH_DECAY_18A2    

WALL_FLASH_DECAY_189D:
189D:  1A           ld a,(de)                   
189E:  E6 0F        and $0F                     
18A0:  F6 70        or $70                      

WALL_FLASH_DECAY_18A2:
18A2:  12           ld (de),a                   

WALL_FLASH_DECAY_18A3:
18A3:  13           inc de                      
18A4:  13           inc de                      
18A5:  13           inc de                      
18A6:  13           inc de                      
18A7:  13           inc de                      
18A8:  13           inc de                      
18A9:  23           inc hl                      
18AA:  10 C4        djnz WALL_FLASH_DECAY_1870  
18AC:  3A 9A 40     ld a,(wall_fx)               ; wall_fx
18AF:  CB 77        bit 6,a                     
18B1:  28 04        jr z,WALL_FLASH_DECAY_18B7  
18B3:  3E 80        ld a,$80                    
18B5:  18 01        jr WALL_FLASH_DECAY_18B8    

WALL_FLASH_DECAY_18B7:
18B7:  AF           xor a                       

WALL_FLASH_DECAY_18B8:
18B8:  32 9A 40     ld (wall_fx),a               ; wall_fx
18BB:  C9           ret                         

WALL_BUFFERS_BUILD:
18BC:  01 08 04     ld bc,$0408                 
18BF:  11 F8 9B     ld de,$9BF8                 
18C2:  21 00 90     ld hl,$9000                 
18C5:  ED B0        ldir                        
18C7:  DB 09        in a,($09)                   ; WATCHDOG
18C9:  01 08 04     ld bc,$0408                 
18CC:  11 F8 8B     ld de,$8BF8                 
18CF:  21 00 90     ld hl,$9000                 
18D2:  ED B0        ldir                        
18D4:  DB 09        in a,($09)                   ; WATCHDOG
18D6:  01 08 04     ld bc,$0408                 
18D9:  11 F0 87     ld de,$87F0                 
18DC:  21 00 90     ld hl,$9000                 
18DF:  ED B0        ldir                        
18E1:  DB 09        in a,($09)                   ; WATCHDOG
18E3:  01 08 04     ld bc,$0408                 
18E6:  DD 21 F8 9B  ld ix,$9BF8                 
18EA:  CD 56 2D     call VWALL_XFORM_A           ; -> VWALL_XFORM_A
18ED:  01 08 04     ld bc,$0408                 
18F0:  DD 21 F8 8B  ld ix,$8BF8                 
18F4:  CD 8A 2D     call VWALL_XFORM_B           ; -> VWALL_XFORM_B
18F7:  01 08 04     ld bc,$0408                 
18FA:  DD 21 F0 87  ld ix,$87F0                 
18FE:  CD C8 2D     call VWALL_XFORM_C           ; -> VWALL_XFORM_C
1901:  C9           ret                         

GAME_DLIST_BUILD:
1902:  CD BC 18     call WALL_BUFFERS_BUILD      ; -> WALL_BUFFERS_BUILD
1905:  01 30 00     ld bc,$0030                 
1908:  11 01 46     ld de,$4601                 
190B:  21 D9 30     ld hl,$30D9                 
190E:  ED B0        ldir                        
1910:  06 04        ld b,$04                    
1912:  11 39 40     ld de,$4039                 
1915:  21 17 46     ld hl,$4617                 
1918:  CD 0E 24     call BCD_TO_DIGITS           ; -> BCD_TO_DIGITS
191B:  06 01        ld b,$01                    
191D:  11 2D 40     ld de,credits_bcd            ; credits_bcd
1920:  21 2B 46     ld hl,$462B                 
1923:  CD 0E 24     call BCD_TO_DIGITS           ; -> BCD_TO_DIGITS
1926:  01 B0 01     ld bc,$01B0                 
1929:  11 00 80     ld de,vector_ram             ; vector_ram
192C:  21 81 44     ld hl,dlist_shadow           ; dlist_shadow
192F:  ED B0        ldir                        
1931:  01 0E 01     ld bc,$010E                 
1934:  21 09 31     ld hl,$3109                 
1937:  ED B0        ldir                        
1939:  01 10 00     ld bc,$0010                 
193C:  11 34 82     ld de,$8234                 
193F:  21 E5 3E     ld hl,$3EE5                 
1942:  ED B0        ldir                        
1944:  06 04        ld b,$04                    
1946:  11 45 40     ld de,$4045                 
1949:  21 46 82     ld hl,$8246                 
194C:  CD 0E 24     call BCD_TO_DIGITS           ; -> BCD_TO_DIGITS
194F:  FD 36 04 05  ld (iy+4),$05               
1953:  CD F3 2A     call DRAW_PAGE               ; -> DRAW_PAGE
1956:  FD 36 04 06  ld (iy+4),$06               
195A:  CD F3 2A     call DRAW_PAGE               ; -> DRAW_PAGE
195D:  FD 36 04 09  ld (iy+4),$09               
1961:  CD F3 2A     call DRAW_PAGE               ; -> DRAW_PAGE
1964:  C9           ret                         

OBJECTS_UPDATE:
1965:  DB 09        in a,($09)                   ; WATCHDOG
1967:  06 20        ld b,$20                    
1969:  FD 36 F5 80  ld (iy-11),$80              
196D:  FD 36 F6 80  ld (iy-10),$80              
1971:  FD 36 F4 00  ld (iy-12),$00              
1975:  FD 4E 99     ld c,(iy-103)               
1978:  F3           di                          
1979:  21 00 00     ld hl,$0000                  ; RESET
197C:  39           add hl,sp                   
197D:  EB           ex de,hl                    
197E:  31 90 2E     ld sp,$2E90                 
1981:  FD 36 FD 00  ld (iy-3),$00               

OBJ_UPDATE_LOOP:
1985:  FD 34 FD     inc (iy-3)                  
1988:  DD E1        pop ix                      
198A:  DD 7E 13     ld a,(ix+19)                
198D:  91           sub c                       
198E:  DD 77 13     ld (ix+19),a                
1991:  DD CB 00 7E  bit 7,(ix+0)                
1995:  28 04        jr z,obj_expire_slot         ; -> obj_expire_slot
1997:  30 63        jr nc,obj_proximity_upd      ; -> obj_proximity_upd
1999:  18 26        jr obj_build_check           ; -> obj_build_check

obj_expire_slot:
199B:  D2 4D 1A     jp nc,obj_skip_entry         ; -> obj_skip_entry
199E:  DD CB 01 5E  bit 3,(ix+1)                
19A2:  C2 4D 1A     jp nz,obj_skip_entry         ; -> obj_skip_entry
19A5:  DD CB 01 DE  set 3,(ix+1)                
19A9:  D9           exx                         
19AA:  C1           pop bc                      
19AB:  C1           pop bc                      
19AC:  60           ld h,b                      
19AD:  69           ld l,c                      
19AE:  11 75 84     ld de,$8475                 
19B1:  B7           or a                        
19B2:  ED 52        sbc hl,de                   
19B4:  CB 2C        sra h                       
19B6:  CB 1D        rr l                        
19B8:  7D           ld a,l                      
19B9:  02           ld (bc),a                   
19BA:  03           inc bc                      
19BB:  7C           ld a,h                      
19BC:  02           ld (bc),a                   
19BD:  D9           exx                         
19BE:  C3 51 1A     jp obj_next                  ; -> obj_next

obj_build_check:
19C1:  DD CB 01 4E  bit 1,(ix+1)                
19C5:  28 06        jr z,OBJ_STAGE_RUN           ; -> OBJ_STAGE_RUN
19C7:  DD CB 01 56  bit 2,(ix+1)                
19CB:  20 2F        jr nz,obj_proximity_upd      ; -> obj_proximity_upd

OBJ_STAGE_RUN:
19CD:  D9           exx                         
19CE:  E1           pop hl                      
19CF:  E1           pop hl                      
19D0:  D9           exx                         
19D1:  21 00 00     ld hl,$0000                  ; RESET
19D4:  39           add hl,sp                   
19D5:  EB           ex de,hl                    
19D6:  F9           ld sp,hl                    
19D7:  D9           exx                         
19D8:  E5           push hl                     
19D9:  DD E3        ex (sp),ix                  
19DB:  E1           pop hl                      
19DC:  FB           ei                          
19DD:  E5           push hl                     
19DE:  01 17 00     ld bc,$0017                 
19E1:  11 00 40     ld de,obj_flags0             ; obj_flags0
19E4:  ED B0        ldir                        
19E6:  CD 59 1A     call FRAME_SYNC_DISP         ; -> FRAME_SYNC_DISP
19E9:  01 17 00     ld bc,$0017                 
19EC:  D1           pop de                      
19ED:  21 00 40     ld hl,obj_flags0             ; obj_flags0
19F0:  ED B0        ldir                        
19F2:  F3           di                          
19F3:  D9           exx                         
19F4:  EB           ex de,hl                    
19F5:  F9           ld sp,hl                    
19F6:  FD CB F4 FE  set 7,(iy-12)               
19FA:  18 55        jr obj_next                  ; -> obj_next

obj_proximity_upd:
19FC:  FD CB F4 7E  bit 7,(iy-12)               
1A00:  28 3F        jr z,obj_track_pos           ; -> obj_track_pos
1A02:  3A 75 40     ld a,(settle_track_x)        ; settle_track_x
1A05:  DD 96 07     sub (ix+7)                  
1A08:  30 02        jr nc,obj_proximity_upd_1A0C
1A0A:  ED 44        neg                         

obj_proximity_upd_1A0C:
1A0C:  67           ld h,a                      
1A0D:  3A 76 40     ld a,(settle_track_y)        ; settle_track_y
1A10:  DD 96 09     sub (ix+9)                  
1A13:  30 02        jr nc,obj_proximity_upd_1A17
1A15:  ED 44        neg                         

obj_proximity_upd_1A17:
1A17:  BC           cp h                        
1A18:  38 01        jr c,obj_proximity_upd_1A1B 
1A1A:  67           ld h,a                      

obj_proximity_upd_1A1B:
1A1B:  2E 08        ld l,$08                    
1A1D:  3E 90        ld a,$90                    

obj_proximity_upd_1A1F:
1A1F:  CB 04        rlc h                       
1A21:  38 05        jr c,obj_proximity_upd_1A28 
1A23:  D6 10        sub $10                     
1A25:  2D           dec l                       
1A26:  20 F7        jr nz,obj_proximity_upd_1A1F

obj_proximity_upd_1A28:
1A28:  DD 96 0B     sub (ix+11)                 
1A2B:  67           ld h,a                      
1A2C:  DD 7E 07     ld a,(ix+7)                 
1A2F:  32 75 40     ld (settle_track_x),a        ; settle_track_x
1A32:  DD 7E 09     ld a,(ix+9)                 
1A35:  32 76 40     ld (settle_track_y),a        ; settle_track_y
1A38:  DD E1        pop ix                      
1A3A:  DD E1        pop ix                      
1A3C:  DD 74 07     ld (ix+7),h                 
1A3F:  18 10        jr obj_next                  ; -> obj_next

obj_track_pos:
1A41:  DD 7E 07     ld a,(ix+7)                 
1A44:  32 75 40     ld (settle_track_x),a        ; settle_track_x
1A47:  DD 7E 09     ld a,(ix+9)                 
1A4A:  32 76 40     ld (settle_track_y),a        ; settle_track_y

obj_skip_entry:
1A4D:  DD E1        pop ix                      
1A4F:  DD E1        pop ix                      

obj_next:
1A51:  05           dec b                       
1A52:  C2 85 19     jp nz,OBJ_UPDATE_LOOP        ; -> OBJ_UPDATE_LOOP
1A55:  EB           ex de,hl                    
1A56:  F9           ld sp,hl                    
1A57:  FB           ei                          
1A58:  C9           ret                         

FRAME_SYNC_DISP:
1A59:  DB 09        in a,($09)                   ; WATCHDOG
1A5B:  DB 0B        in a,($0B)                   ; VG_STATUS (d7=busy)
1A5D:  CB 7F        bit 7,a                     
1A5F:  CC 50 17     call z,VG_RESTART_FRAME      ; -> VG_RESTART_FRAME
1A62:  3A 17 40     ld a,(tick_244hz)            ; tick_244hz
1A65:  FD 96 98     sub (iy-104)                
1A68:  FD 86 92     add a,(iy-110)              
1A6B:  32 13 40     ld (obj_timer),a             ; obj_timer
1A6E:  FD CB 81 6E  bit 5,(iy-127)              
1A72:  C2 F9 1A     jp nz,fsd_slot_init_once     ; -> fsd_slot_init_once
1A75:  3A 7D 40     ld a,(obj_index)             ; obj_index
1A78:  FE 01        cp $01                      
1A7A:  28 28        jr z,fsd_ship_object         ; -> fsd_ship_object
1A7C:  FE 0E        cp $0E                      
1A7E:  38 58        jr c,fsd_mines_collide       ; -> fsd_mines_collide
1A80:  FE 16        cp $16                      
1A82:  30 75        jr nc,fsd_slot_init_once     ; -> fsd_slot_init_once
1A84:  FE 12        cp $12                      
1A86:  38 5E        jr c,fsd_mines_collide_1AE6 

fsd_shots_collide:
1A88:  DD E5        push ix                     
1A8A:  DD 21 96 2E  ld ix,$2E96                 
1A8E:  FD 46 E7     ld b,(iy-25)                
1A91:  FD 36 FE 02  ld (iy-2),$02               
1A95:  CD 99 21     call COLLIDE_SCAN            ; -> COLLIDE_SCAN
1A98:  DD 21 0E 2F  ld ix,$2F0E                 
1A9C:  06 0B        ld b,$0B                    
1A9E:  FD 36 FE 16  ld (iy-2),$16               
1AA2:  18 50        jr fsd_do_collide            ; -> fsd_do_collide

fsd_ship_object:
1AA4:  3A 7A 40     ld a,(shooter_a)             ; shooter_a
1AA7:  B7           or a                        
1AA8:  20 20        jr nz,fsd_droids_collide     ; -> fsd_droids_collide
1AAA:  3A 67 40     ld a,(wave_num)              ; wave_num
1AAD:  FE 06        cp $06                      
1AAF:  20 08        jr nz,fsd_ship_object_1AB9  
1AB1:  3A 22 40     ld a,(wave_pace_timer)       ; wave_pace_timer
1AB4:  B7           or a                        
1AB5:  20 13        jr nz,fsd_droids_collide     ; -> fsd_droids_collide
1AB7:  18 05        jr fsd_reset_pairmin         ; -> fsd_reset_pairmin

fsd_ship_object_1AB9:
1AB9:  FD 35 FC     dec (iy-4)                  
1ABC:  20 0C        jr nz,fsd_droids_collide     ; -> fsd_droids_collide

fsd_reset_pairmin:
1ABE:  FD CB 81 E6  set 4,(iy-127)              
1AC2:  FD 36 00 FF  ld (iy+0),$FF               
1AC6:  FD 36 01 FF  ld (iy+1),$FF               

fsd_droids_collide:
1ACA:  DD E5        push ix                     
1ACC:  DD 21 96 2E  ld ix,$2E96                 
1AD0:  06 1F        ld b,$1F                    
1AD2:  FD 36 FE 02  ld (iy-2),$02               
1AD6:  18 1C        jr fsd_do_collide            ; -> fsd_do_collide

fsd_mines_collide:
1AD8:  DD E5        push ix                     
1ADA:  DD 21 0E 2F  ld ix,$2F0E                 
1ADE:  06 0B        ld b,$0B                    
1AE0:  FD 36 FE 0E  ld (iy-2),$0E               
1AE4:  18 0E        jr fsd_do_collide            ; -> fsd_do_collide

fsd_mines_collide_1AE6:
1AE6:  18 11        jr fsd_slot_init_once        ; -> fsd_slot_init_once

fsd_mines_collide_1AE8:
1AE8:  DD E5        push ix                     
1AEA:  DD 21 90 2E  ld ix,$2E90                 
1AEE:  06 01        ld b,$01                    
1AF0:  FD 36 FE 01  ld (iy-2),$01               

fsd_do_collide:
1AF4:  CD 99 21     call COLLIDE_SCAN            ; -> COLLIDE_SCAN
1AF7:  DD E1        pop ix                      

fsd_slot_init_once:
1AF9:  FD CB 81 56  bit 2,(iy-127)              
1AFD:  20 17        jr nz,OBJ_PHYSICS            ; -> OBJ_PHYSICS
1AFF:  FD CB 81 D6  set 2,(iy-127)              
1B03:  DD E5        push ix                     
1B05:  E1           pop hl                      
1B06:  11 7F 84     ld de,$847F                 
1B09:  B7           or a                        
1B0A:  ED 52        sbc hl,de                   
1B0C:  CB 2C        sra h                       
1B0E:  CB 1D        rr l                        
1B10:  DD 74 01     ld (ix+1),h                 
1B13:  DD 75 00     ld (ix+0),l                 

OBJ_PHYSICS:
1B16:  FD CB 80 46  bit 0,(iy-128)              
1B1A:  28 77        jr z,OBJ_PHYSICS_1B93       
1B1C:  FD CB EB 7E  bit 7,(iy-21)               
1B20:  28 71        jr z,OBJ_PHYSICS_1B93       
1B22:  FD CB 81 7E  bit 7,(iy-127)              
1B26:  C4 3F 28     call nz,WALL_TRACK_STEER     ; -> WALL_TRACK_STEER
1B29:  3A 0A 40     ld a,(obj_angle)             ; obj_angle
1B2C:  CB 67        bit 4,a                     
1B2E:  28 02        jr z,OBJ_PHYSICS_1B32       
1B30:  EE 0F        xor $0F                     

OBJ_PHYSICS_1B32:
1B32:  E6 0F        and $0F                     
1B34:  07           rlca                        
1B35:  5F           ld e,a                      
1B36:  16 00        ld d,$00                    
1B38:  21 21 30     ld hl,$3021                 
1B3B:  19           add hl,de                   
1B3C:  5E           ld e,(hl)                   
1B3D:  23           inc hl                      
1B3E:  56           ld d,(hl)                   
1B3F:  3A 0A 40     ld a,(obj_angle)             ; obj_angle
1B42:  E6 30        and $30                     
1B44:  28 08        jr z,OBJ_PHYSICS_1B4E       
1B46:  FE 30        cp $30                      
1B48:  28 04        jr z,OBJ_PHYSICS_1B4E       
1B4A:  7A           ld a,d                      
1B4B:  ED 44        neg                         
1B4D:  57           ld d,a                      

OBJ_PHYSICS_1B4E:
1B4E:  3A 0A 40     ld a,(obj_angle)             ; obj_angle
1B51:  CB 6F        bit 5,a                     
1B53:  28 04        jr z,OBJ_PHYSICS_1B59       
1B55:  7B           ld a,e                      
1B56:  ED 44        neg                         
1B58:  5F           ld e,a                      

OBJ_PHYSICS_1B59:
1B59:  2A 02 40     ld hl,(obj_velx)             ; obj_velx
1B5C:  4A           ld c,d                      
1B5D:  06 00        ld b,$00                    
1B5F:  CB 7A        bit 7,d                     
1B61:  28 02        jr z,OBJ_PHYSICS_1B65       
1B63:  06 FF        ld b,$FF                    

OBJ_PHYSICS_1B65:
1B65:  CB 21        sla c                       
1B67:  09           add hl,bc                   
1B68:  CD 64 21     call CLAMP_SPEED             ; -> CLAMP_SPEED
1B6B:  22 02 40     ld (obj_velx),hl             ; obj_velx
1B6E:  2A 04 40     ld hl,(obj_vely)             ; obj_vely
1B71:  4B           ld c,e                      
1B72:  06 00        ld b,$00                    
1B74:  CB 7B        bit 7,e                     
1B76:  28 02        jr z,OBJ_PHYSICS_1B7A       
1B78:  06 FF        ld b,$FF                    

OBJ_PHYSICS_1B7A:
1B7A:  CB 21        sla c                       
1B7C:  09           add hl,bc                   
1B7D:  CD 64 21     call CLAMP_SPEED             ; -> CLAMP_SPEED
1B80:  22 04 40     ld (obj_vely),hl             ; obj_vely
1B83:  FD CB 81 7E  bit 7,(iy-127)              
1B87:  20 0A        jr nz,OBJ_PHYSICS_1B93      
1B89:  FD CB 81 76  bit 6,(iy-127)              
1B8D:  28 04        jr z,OBJ_PHYSICS_1B93       
1B8F:  FD CB 80 86  res 0,(iy-128)              

OBJ_PHYSICS_1B93:
1B93:  FD CB 80 5E  bit 3,(iy-128)              
1B97:  28 1C        jr z,OBJ_PHYSICS_1BB5       
1B99:  FD CB 81 7E  bit 7,(iy-127)              
1B9D:  20 04        jr nz,OBJ_PHYSICS_1BA3      
1B9F:  FD CB 80 9E  res 3,(iy-128)              

OBJ_PHYSICS_1BA3:
1BA3:  2A 02 40     ld hl,(obj_velx)             ; obj_velx
1BA6:  CD 48 21     call APPLY_FRICTION          ; -> APPLY_FRICTION
1BA9:  22 02 40     ld (obj_velx),hl             ; obj_velx
1BAC:  2A 04 40     ld hl,(obj_vely)             ; obj_vely
1BAF:  CD 48 21     call APPLY_FRICTION          ; -> APPLY_FRICTION
1BB2:  22 04 40     ld (obj_vely),hl             ; obj_vely

OBJ_PHYSICS_1BB5:
1BB5:  FD CB 80 76  bit 6,(iy-128)              
1BB9:  28 30        jr z,enemy_fire_gate         ; -> enemy_fire_gate
1BBB:  CD 2A 1F     call WALL_BOUNCE             ; -> WALL_BOUNCE
1BBE:  FD CB F8 7E  bit 7,(iy-8)                
1BC2:  28 11        jr z,OBJ_PHYSICS_1BD5       
1BC4:  FD CB 81 46  bit 0,(iy-127)              
1BC8:  28 0B        jr z,OBJ_PHYSICS_1BD5       
1BCA:  3A 7D 40     ld a,(obj_index)             ; obj_index
1BCD:  32 7E 40     ld (obj_class),a             ; obj_class
1BD0:  CD BF 27     call OBJ_DESTROY             ; -> OBJ_DESTROY
1BD3:  18 16        jr enemy_fire_gate           ; -> enemy_fire_gate

OBJ_PHYSICS_1BD5:
1BD5:  ED 5B 02 40  ld de,(obj_velx)             ; obj_velx
1BD9:  2A 06 40     ld hl,(obj_posx)             ; obj_posx
1BDC:  19           add hl,de                   
1BDD:  22 06 40     ld (obj_posx),hl             ; obj_posx
1BE0:  ED 5B 04 40  ld de,(obj_vely)             ; obj_vely
1BE4:  2A 08 40     ld hl,(obj_posy)             ; obj_posy
1BE7:  19           add hl,de                   
1BE8:  22 08 40     ld (obj_posy),hl             ; obj_posy

enemy_fire_gate:
1BEB:  3A 7B 40     ld a,(shooter_b)             ; shooter_b
1BEE:  FD BE FD     cp (iy-3)                   
1BF1:  20 30        jr nz,enemy_fire_gate_1C23  
1BF3:  FD BE FA     cp (iy-6)                   
1BF6:  28 06        jr z,enemy_fire_gate_1BFE   
1BF8:  FD CB 81 6E  bit 5,(iy-127)              
1BFC:  28 09        jr z,enemy_fire_gate_1C07   

enemy_fire_gate_1BFE:
1BFE:  FD 36 FB 00  ld (iy-5),$00               
1C02:  CD 67 24     call ENEMY_ALIVE_SCAN        ; -> ENEMY_ALIVE_SCAN
1C05:  18 1C        jr enemy_fire_gate_1C23     

enemy_fire_gate_1C07:
1C07:  3E 1E        ld a,$1E                    
1C09:  FD 96 E7     sub (iy-25)                 
1C0C:  FD 96 E7     sub (iy-25)                 
1C0F:  32 12 40     ld ($4012),a                
1C12:  FD CB 80 F6  set 6,(iy-128)              
1C16:  3A 0E 40     ld a,(obj_fire_timer)        ; obj_fire_timer
1C19:  B7           or a                        
1C1A:  20 04        jr nz,enemy_fire_gate_1C20  
1C1C:  FD 36 8E 0C  ld (iy-114),$0C             

enemy_fire_gate_1C20:
1C20:  CD 2F 25     call AIM_AT_SHIP             ; -> AIM_AT_SHIP

enemy_fire_gate_1C23:
1C23:  3A 7A 40     ld a,(shooter_a)             ; shooter_a
1C26:  FD BE FD     cp (iy-3)                   
1C29:  C2 B4 1D     jp nz,move_mode_dispatch     ; -> move_mode_dispatch
1C2C:  FD CB 81 6E  bit 5,(iy-127)              
1C30:  C2 AD 1D     jp nz,quadrant_engage_chk_1DAD
1C33:  FD CB 33 66  bit 4,(iy+51)               
1C37:  28 2E        jr z,enemy_fire_gate_1C67   
1C39:  FD CB 33 A6  res 4,(iy+51)               
1C3D:  3A 0A 40     ld a,(obj_angle)             ; obj_angle
1C40:  32 0F 40     ld (obj_aim),a               ; obj_aim
1C43:  CD 2F 25     call AIM_AT_SHIP             ; -> AIM_AT_SHIP
1C46:  3A 67 40     ld a,(wave_num)              ; wave_num
1C49:  FE 06        cp $06                      
1C4B:  28 16        jr z,enemy_fire_gate_1C63   
1C4D:  CD 06 29     call RANDOM_NEXT             ; -> RANDOM_NEXT
1C50:  E6 0F        and $0F                     
1C52:  F6 20        or $20                      
1C54:  FD 77 8E     ld (iy-114),a               
1C57:  E6 07        and $07                     
1C59:  F6 04        or $04                      
1C5B:  32 7F 40     ld (special_armed),a         ; special_armed
1C5E:  CD 58 27     call SPAWN_SPECIAL           ; -> SPAWN_SPECIAL
1C61:  18 04        jr enemy_fire_gate_1C67     

enemy_fire_gate_1C63:
1C63:  FD 36 FC FA  ld (iy-4),$FA               

enemy_fire_gate_1C67:
1C67:  FD CB 80 F6  set 6,(iy-128)              
1C6B:  FD 7E 8E     ld a,(iy-114)               
1C6E:  FE 01        cp $01                      
1C70:  20 09        jr nz,enemy_fire_gate_1C7B  
1C72:  FD 35 FF     dec (iy-1)                  
1C75:  20 04        jr nz,enemy_fire_gate_1C7B  
1C77:  FD 36 8E 00  ld (iy-114),$00             

enemy_fire_gate_1C7B:
1C7B:  3A 68 40     ld a,(last_sound_cmd)        ; last_sound_cmd
1C7E:  FE 0F        cp $0F                      
1C80:  28 22        jr z,enemy_fire_gate_1CA4   
1C82:  3A 22 40     ld a,(wave_pace_timer)       ; wave_pace_timer
1C85:  FE 0A        cp $0A                      
1C87:  38 0B        jr c,enemy_fire_gate_1C94   
1C89:  FE 0F        cp $0F                      
1C8B:  30 17        jr nc,enemy_fire_gate_1CA4  
1C8D:  3E 02        ld a,$02                    
1C8F:  CD F6 2D     call HEARTBEAT_SOUND         ; -> HEARTBEAT_SOUND
1C92:  18 10        jr enemy_fire_gate_1CA4     

enemy_fire_gate_1C94:
1C94:  FE 05        cp $05                      
1C96:  38 07        jr c,enemy_fire_gate_1C9F   
1C98:  3E 03        ld a,$03                    
1C9A:  CD F6 2D     call HEARTBEAT_SOUND         ; -> HEARTBEAT_SOUND
1C9D:  18 05        jr enemy_fire_gate_1CA4     

enemy_fire_gate_1C9F:
1C9F:  3E 04        ld a,$04                    
1CA1:  CD F6 2D     call HEARTBEAT_SOUND         ; -> HEARTBEAT_SOUND

enemy_fire_gate_1CA4:
1CA4:  3A 77 40     ld a,(shooter_state)         ; shooter_state
1CA7:  FE 01        cp $01                      
1CA9:  28 0A        jr z,enemy_fire_gate_1CB5   
1CAB:  3A 22 40     ld a,(wave_pace_timer)       ; wave_pace_timer
1CAE:  B7           or a                        
1CAF:  CA D6 1C     jp z,enemy_fire_gate_1CD6   
1CB2:  C3 3C 1D     jp enemy_fire_gate_1D3C     

enemy_fire_gate_1CB5:
1CB5:  FD CB EA 7E  bit 7,(iy-22)               
1CB9:  CA 3C 1D     jp z,enemy_fire_gate_1D3C   
1CBC:  FD 36 92 0C  ld (iy-110),$0C             
1CC0:  3A 22 40     ld a,(wave_pace_timer)       ; wave_pace_timer
1CC3:  FE 0C        cp $0C                      
1CC5:  30 75        jr nc,enemy_fire_gate_1D3C  
1CC7:  FD 36 92 0A  ld (iy-110),$0A             
1CCB:  FE 06        cp $06                      
1CCD:  30 6D        jr nc,enemy_fire_gate_1D3C  
1CCF:  FD 36 92 08  ld (iy-110),$08             
1CD3:  B7           or a                        
1CD4:  20 66        jr nz,enemy_fire_gate_1D3C  

enemy_fire_gate_1CD6:
1CD6:  FD CB EA FE  set 7,(iy-22)               
1CDA:  FD 36 92 04  ld (iy-110),$04             
1CDE:  3E 11        ld a,$11                    
1CE0:  FD 96 E7     sub (iy-25)                 
1CE3:  32 22 40     ld (wave_pace_timer),a       ; wave_pace_timer
1CE6:  CD 06 29     call RANDOM_NEXT             ; -> RANDOM_NEXT
1CE9:  E6 0F        and $0F                     
1CEB:  F6 10        or $10                      
1CED:  32 0E 40     ld (obj_fire_timer),a        ; obj_fire_timer
1CF0:  FD CB EA 46  bit 0,(iy-22)               
1CF4:  28 04        jr z,enemy_fire_gate_1CFA   
1CF6:  FD 36 8E 02  ld (iy-114),$02             

enemy_fire_gate_1CFA:
1CFA:  FD CB 81 BE  res 7,(iy-127)              
1CFE:  FD CB 80 DE  set 3,(iy-128)              
1D02:  FD 36 92 04  ld (iy-110),$04             
1D06:  FD CB EB 5E  bit 3,(iy-21)               
1D0A:  20 0C        jr nz,enemy_fire_gate_1D18  
1D0C:  FD CB EB DE  set 3,(iy-21)               
1D10:  3E 10        ld a,$10                    
1D12:  FD 96 E7     sub (iy-25)                 
1D15:  32 12 40     ld ($4012),a                

enemy_fire_gate_1D18:
1D18:  FD 36 FF FA  ld (iy-1),$FA               
1D1C:  3E 05        ld a,$05                    
1D1E:  CD F6 2D     call HEARTBEAT_SOUND         ; -> HEARTBEAT_SOUND
1D21:  3A 3B 40     ld a,($403B)                
1D24:  B7           or a                        
1D25:  20 58        jr nz,quadrant_engage_chk_1D7F
1D27:  3A 3A 40     ld a,(time_played_bcd)       ; time_played_bcd
1D2A:  FE 35        cp $35                      
1D2C:  30 51        jr nc,quadrant_engage_chk_1D7F
1D2E:  FD 36 8E 00  ld (iy-114),$00             
1D32:  FD 36 FF FA  ld (iy-1),$FA               
1D36:  FD 36 92 10  ld (iy-110),$10             
1D3A:  18 43        jr quadrant_engage_chk_1D7F 

enemy_fire_gate_1D3C:
1D3C:  FD CB 81 7E  bit 7,(iy-127)              
1D40:  20 4F        jr nz,quadrant_engage_chk_1D91
1D42:  3A 77 40     ld a,(shooter_state)         ; shooter_state
1D45:  FE 01        cp $01                      
1D47:  20 22        jr nz,quadrant_engage_chk_1D6B
1D49:  3A 7C 40     ld a,(special_countdown)     ; special_countdown
1D4C:  FE 14        cp $14                      
1D4E:  38 04        jr c,quadrant_engage_chk     ; -> quadrant_engage_chk
1D50:  FD 36 FC 14  ld (iy-4),$14               

quadrant_engage_chk:
1D54:  3A B9 40     ld a,(ship_x)                ; ship_x
1D57:  FD AE 87     xor (iy-121)                
1D5A:  4F           ld c,a                      
1D5B:  3A BB 40     ld a,(ship_y)                ; ship_y
1D5E:  FD AE 89     xor (iy-119)                
1D61:  B1           or c                        
1D62:  E6 80        and $80                     
1D64:  28 05        jr z,quadrant_engage_chk_1D6B
1D66:  CD 3F 28     call WALL_TRACK_STEER        ; -> WALL_TRACK_STEER
1D69:  18 26        jr quadrant_engage_chk_1D91 

quadrant_engage_chk_1D6B:
1D6B:  3A 0F 40     ld a,(obj_aim)               ; obj_aim
1D6E:  32 0A 40     ld (obj_angle),a             ; obj_angle
1D71:  3A 22 40     ld a,(wave_pace_timer)       ; wave_pace_timer
1D74:  B7           or a                        
1D75:  CA D6 1C     jp z,enemy_fire_gate_1CD6   
1D78:  E6 07        and $07                     
1D7A:  20 15        jr nz,quadrant_engage_chk_1D91
1D7C:  FD 35 A2     dec (iy-94)                 

quadrant_engage_chk_1D7F:
1D7F:  3A 0F 40     ld a,(obj_aim)               ; obj_aim
1D82:  C6 20        add a,$20                   
1D84:  32 0F 40     ld (obj_aim),a               ; obj_aim
1D87:  CD 06 29     call RANDOM_NEXT             ; -> RANDOM_NEXT
1D8A:  CB 47        bit 0,a                     
1D8C:  28 03        jr z,quadrant_engage_chk_1D91
1D8E:  FD 35 8F     dec (iy-113)                

quadrant_engage_chk_1D91:
1D91:  CD 2F 25     call AIM_AT_SHIP             ; -> AIM_AT_SHIP
1D94:  3A 7F 40     ld a,(special_armed)         ; special_armed
1D97:  B7           or a                        
1D98:  28 13        jr z,quadrant_engage_chk_1DAD
1D9A:  3A 7A 40     ld a,(shooter_a)             ; shooter_a
1D9D:  B7           or a                        
1D9E:  28 0D        jr z,quadrant_engage_chk_1DAD
1DA0:  FD 35 FC     dec (iy-4)                  
1DA3:  20 0F        jr nz,move_mode_dispatch     ; -> move_mode_dispatch
1DA5:  CD 58 27     call SPAWN_SPECIAL           ; -> SPAWN_SPECIAL
1DA8:  CD 86 21     call DIFFICULTY_CALC         ; -> DIFFICULTY_CALC
1DAB:  18 07        jr move_mode_dispatch        ; -> move_mode_dispatch

quadrant_engage_chk_1DAD:
1DAD:  FD 36 FA 00  ld (iy-6),$00               
1DB1:  CD 86 21     call DIFFICULTY_CALC         ; -> DIFFICULTY_CALC

move_mode_dispatch:
1DB4:  FD CB 81 6E  bit 5,(iy-127)              
1DB8:  28 17        jr z,move_mode_dispatch_1DD1
1DBA:  3A 0A 40     ld a,(obj_angle)             ; obj_angle
1DBD:  E6 03        and $03                     
1DBF:  07           rlca                        
1DC0:  FD 34 8A     inc (iy-118)                
1DC3:  5F           ld e,a                      
1DC4:  16 00        ld d,$00                    
1DC6:  21 5D 3F     ld hl,$3F5D                 
1DC9:  19           add hl,de                   
1DCA:  5E           ld e,(hl)                   
1DCB:  23           inc hl                      
1DCC:  56           ld d,(hl)                   
1DCD:  EB           ex de,hl                    
1DCE:  C3 5B 1E     jp move_mode_dispatch_1E5B  

move_mode_dispatch_1DD1:
1DD1:  FD CB 80 66  bit 4,(iy-128)              
1DD5:  CA 63 1E     jp z,move_mode_dispatch_1E63
1DD8:  FD CB 81 76  bit 6,(iy-127)              
1DDC:  20 3A        jr nz,move_mode_dispatch_1E18
1DDE:  FD 34 94     inc (iy-108)                
1DE1:  3A 14 40     ld a,($4014)                
1DE4:  07           rlca                        
1DE5:  E6 06        and $06                     
1DE7:  20 1A        jr nz,move_mode_dispatch_1E03
1DE9:  3A 0E 40     ld a,(obj_fire_timer)        ; obj_fire_timer
1DEC:  B7           or a                        
1DED:  3E 00        ld a,$00                    
1DEF:  28 0C        jr z,move_mode_dispatch_1DFD
1DF1:  FD CB 81 7E  bit 7,(iy-127)              
1DF5:  28 0C        jr z,move_mode_dispatch_1E03
1DF7:  FD 36 94 02  ld (iy-108),$02             
1DFB:  18 06        jr move_mode_dispatch_1E03  

move_mode_dispatch_1DFD:
1DFD:  3E 02        ld a,$02                    
1DFF:  FD 36 94 01  ld (iy-108),$01             

move_mode_dispatch_1E03:
1E03:  FD CB 81 7E  bit 7,(iy-127)              
1E07:  20 02        jr nz,move_mode_dispatch_1E0B
1E09:  CB DF        set 3,a                     

move_mode_dispatch_1E0B:
1E0B:  5F           ld e,a                      
1E0C:  16 00        ld d,$00                    
1E0E:  21 4D 3F     ld hl,$3F4D                 
1E11:  19           add hl,de                   
1E12:  5E           ld e,(hl)                   
1E13:  23           inc hl                      
1E14:  56           ld d,(hl)                   
1E15:  EB           ex de,hl                    
1E16:  18 43        jr move_mode_dispatch_1E5B  

move_mode_dispatch_1E18:
1E18:  3A 0A 40     ld a,(obj_angle)             ; obj_angle
1E1B:  32 0F 40     ld (obj_aim),a               ; obj_aim
1E1E:  E6 30        and $30                     
1E20:  0F           rrca                        
1E21:  0F           rrca                        
1E22:  0F           rrca                        
1E23:  5F           ld e,a                      
1E24:  16 00        ld d,$00                    
1E26:  21 A1 30     ld hl,$30A1                 
1E29:  19           add hl,de                   
1E2A:  5E           ld e,(hl)                   
1E2B:  23           inc hl                      
1E2C:  56           ld d,(hl)                   
1E2D:  3A 0A 40     ld a,(obj_angle)             ; obj_angle
1E30:  CB 67        bit 4,a                     
1E32:  28 02        jr z,move_mode_dispatch_1E36
1E34:  EE 0F        xor $0F                     

move_mode_dispatch_1E36:
1E36:  E6 0F        and $0F                     
1E38:  07           rlca                        
1E39:  4F           ld c,a                      
1E3A:  06 00        ld b,$00                    
1E3C:  21 A9 30     ld hl,$30A9                 
1E3F:  09           add hl,bc                   
1E40:  4E           ld c,(hl)                   
1E41:  23           inc hl                      
1E42:  46           ld b,(hl)                   
1E43:  EB           ex de,hl                    
1E44:  09           add hl,bc                   
1E45:  7C           ld a,h                      
1E46:  E6 0F        and $0F                     
1E48:  F6 C0        or $C0                      
1E4A:  67           ld h,a                      
1E4B:  22 0B 46     ld ($460B),hl               
1E4E:  11 F9 FF     ld de,$FFF9                 
1E51:  19           add hl,de                   
1E52:  22 0D 46     ld ($460D),hl               
1E55:  22 11 46     ld ($4611),hl               
1E58:  2A 65 3F     ld hl,($3F65)               

move_mode_dispatch_1E5B:
1E5B:  DD 74 0B     ld (ix+11),h                
1E5E:  DD 75 0A     ld (ix+10),l                
1E61:  18 0C        jr move_mode_dispatch_1E6F  

move_mode_dispatch_1E63:
1E63:  3A 16 40     ld a,($4016)                
1E66:  DD 77 0B     ld (ix+11),a                
1E69:  3A 15 40     ld a,(obj_shape_word)        ; obj_shape_word
1E6C:  DD 77 0A     ld (ix+10),a                

move_mode_dispatch_1E6F:
1E6F:  3A 0C 40     ld a,(obj_lifetime)          ; obj_lifetime
1E72:  B7           or a                        
1E73:  28 09        jr z,move_mode_dispatch_1E7E
1E75:  FD 35 8C     dec (iy-116)                
1E78:  20 04        jr nz,move_mode_dispatch_1E7E
1E7A:  FD CB 80 BE  res 7,(iy-128)              

move_mode_dispatch_1E7E:
1E7E:  3A 0E 40     ld a,(obj_fire_timer)        ; obj_fire_timer
1E81:  B7           or a                        
1E82:  28 17        jr z,move_mode_dispatch_1E9B
1E84:  FD 35 8E     dec (iy-114)                
1E87:  20 12        jr nz,move_mode_dispatch_1E9B
1E89:  3E 2C        ld a,$2C                    
1E8B:  FD 96 E7     sub (iy-25)                 
1E8E:  FD 96 E7     sub (iy-25)                 
1E91:  FD 96 E7     sub (iy-25)                 
1E94:  32 0E 40     ld (obj_fire_timer),a        ; obj_fire_timer
1E97:  FD CB 80 D6  set 2,(iy-128)              

move_mode_dispatch_1E9B:
1E9B:  FD CB 80 56  bit 2,(iy-128)              
1E9F:  C4 EB 25     call nz,SPAWN_MINE_SHOT      ; -> SPAWN_MINE_SHOT
1EA2:  2A 08 40     ld hl,(obj_posy)             ; obj_posy
1EA5:  7C           ld a,h                      
1EA6:  CB 04        rlc h                       
1EA8:  CB 04        rlc h                       
1EAA:  CB 05        rlc l                       
1EAC:  17           rla                         
1EAD:  CB 05        rlc l                       
1EAF:  17           rla                         
1EB0:  DD 77 02     ld (ix+2),a                 
1EB3:  7C           ld a,h                      
1EB4:  E6 03        and $03                     
1EB6:  F6 A0        or $A0                      
1EB8:  DD 77 03     ld (ix+3),a                 
1EBB:  2A 06 40     ld hl,(obj_posx)             ; obj_posx
1EBE:  7C           ld a,h                      
1EBF:  CB 04        rlc h                       
1EC1:  CB 04        rlc h                       
1EC3:  CB 05        rlc l                       
1EC5:  17           rla                         
1EC6:  CB 05        rlc l                       
1EC8:  17           rla                         
1EC9:  DD 77 04     ld (ix+4),a                 
1ECC:  7C           ld a,h                      
1ECD:  E6 03        and $03                     
1ECF:  67           ld h,a                      
1ED0:  3A 0B 40     ld a,($400B)                
1ED3:  E6 F0        and $F0                     
1ED5:  B4           or h                        
1ED6:  DD 77 05     ld (ix+5),a                 
1ED9:  DD 36 06 00  ld (ix+6),$00               
1EDD:  DD 36 08 00  ld (ix+8),$00               
1EE1:  DD 36 09 00  ld (ix+9),$00               
1EE5:  3A 75 40     ld a,(settle_track_x)        ; settle_track_x
1EE8:  FD 96 87     sub (iy-121)                
1EEB:  30 02        jr nc,move_mode_dispatch_1EEF
1EED:  ED 44        neg                         

move_mode_dispatch_1EEF:
1EEF:  57           ld d,a                      
1EF0:  3A 76 40     ld a,(settle_track_y)        ; settle_track_y
1EF3:  FD 96 89     sub (iy-119)                
1EF6:  30 02        jr nc,move_mode_dispatch_1EFA
1EF8:  ED 44        neg                         

move_mode_dispatch_1EFA:
1EFA:  BA           cp d                        
1EFB:  30 01        jr nc,move_mode_dispatch_1EFE
1EFD:  7A           ld a,d                      

move_mode_dispatch_1EFE:
1EFE:  06 08        ld b,$08                    
1F00:  4F           ld c,a                      
1F01:  3E 90        ld a,$90                    

move_mode_dispatch_1F03:
1F03:  CB 01        rlc c                       
1F05:  38 04        jr c,move_mode_dispatch_1F0B
1F07:  D6 10        sub $10                     
1F09:  10 F8        djnz move_mode_dispatch_1F03

move_mode_dispatch_1F0B:
1F0B:  FD 96 8B     sub (iy-117)                
1F0E:  DD 77 07     ld (ix+7),a                 
1F11:  3A 07 40     ld a,($4007)                
1F14:  32 75 40     ld (settle_track_x),a        ; settle_track_x
1F17:  3A 09 40     ld a,($4009)                
1F1A:  32 76 40     ld (settle_track_y),a        ; settle_track_y
1F1D:  DB 0B        in a,($0B)                   ; VG_STATUS (d7=busy)
1F1F:  CB FF        set 7,a                     
1F21:  CC 50 17     call z,VG_RESTART_FRAME      ; -> VG_RESTART_FRAME
1F24:  FD CB 81 D6  set 2,(iy-127)              
1F28:  C9           ret                         
1F29:  db  C3                                              ; .

WALL_BOUNCE:
1F2A:  DB 09        in a,($09)                   ; WATCHDOG
1F2C:  FD 36 F8 00  ld (iy-8),$00               
1F30:  ED 5B 08 40  ld de,(obj_posy)             ; obj_posy
1F34:  2A 04 40     ld hl,(obj_vely)             ; obj_vely
1F37:  19           add hl,de                   
1F38:  CB 7A        bit 7,d                     
1F3A:  20 0F        jr nz,WALL_BOUNCE_1F4B      
1F3C:  CB 72        bit 6,d                     
1F3E:  20 18        jr nz,WALL_BOUNCE_1F58      
1F40:  CB 7C        bit 7,h                     
1F42:  28 14        jr z,WALL_BOUNCE_1F58       
1F44:  CB 74        bit 6,h                     
1F46:  28 10        jr z,WALL_BOUNCE_1F58       
1F48:  C3 17 20     jp bounce_reflect_y          ; -> bounce_reflect_y

WALL_BOUNCE_1F4B:
1F4B:  CB 72        bit 6,d                     
1F4D:  28 09        jr z,WALL_BOUNCE_1F58       
1F4F:  CB 7C        bit 7,h                     
1F51:  20 05        jr nz,WALL_BOUNCE_1F58      
1F53:  CB 74        bit 6,h                     
1F55:  CA 17 20     jp z,bounce_reflect_y        ; -> bounce_reflect_y

WALL_BOUNCE_1F58:
1F58:  7C           ld a,h                      
1F59:  D6 80        sub $80                     
1F5B:  30 02        jr nc,WALL_BOUNCE_1F5F      
1F5D:  ED 44        neg                         

WALL_BOUNCE_1F5F:
1F5F:  5F           ld e,a                      
1F60:  ED 4B 06 40  ld bc,(obj_posx)             ; obj_posx
1F64:  2A 02 40     ld hl,(obj_velx)             ; obj_velx
1F67:  09           add hl,bc                   
1F68:  7C           ld a,h                      
1F69:  D6 80        sub $80                     
1F6B:  30 02        jr nc,WALL_BOUNCE_1F6F      
1F6D:  ED 44        neg                         

WALL_BOUNCE_1F6F:
1F6F:  57           ld d,a                      
1F70:  CB 78        bit 7,b                     
1F72:  20 0E        jr nz,WALL_BOUNCE_1F82      
1F74:  CB 70        bit 6,b                     
1F76:  20 1E        jr nz,WALL_BOUNCE_1F96      
1F78:  CB 7C        bit 7,h                     
1F7A:  28 1A        jr z,WALL_BOUNCE_1F96       
1F7C:  CB 74        bit 6,h                     
1F7E:  28 16        jr z,WALL_BOUNCE_1F96       
1F80:  18 0C        jr WALL_BOUNCE_1F8E         

WALL_BOUNCE_1F82:
1F82:  CB 70        bit 6,b                     
1F84:  28 10        jr z,WALL_BOUNCE_1F96       
1F86:  CB 7C        bit 7,h                     
1F88:  20 0C        jr nz,WALL_BOUNCE_1F96      
1F8A:  CB 74        bit 6,h                     
1F8C:  20 08        jr nz,WALL_BOUNCE_1F96      

WALL_BOUNCE_1F8E:
1F8E:  7B           ld a,e                      
1F8F:  FE 78        cp $78                      
1F91:  30 4B        jr nc,WALL_BOUNCE_1FDE      
1F93:  C3 30 20     jp bounce_reflect_x          ; -> bounce_reflect_x

WALL_BOUNCE_1F96:
1F96:  7B           ld a,e                      
1F97:  FE 78        cp $78                      
1F99:  38 07        jr c,WALL_BOUNCE_1FA2       
1F9B:  7A           ld a,d                      
1F9C:  FE 7F        cp $7F                      
1F9E:  38 77        jr c,bounce_reflect_y        ; -> bounce_reflect_y
1FA0:  18 3C        jr WALL_BOUNCE_1FDE         

WALL_BOUNCE_1FA2:
1FA2:  7A           ld a,d                      
1FA3:  FD CB 81 7E  bit 7,(iy-127)              
1FA7:  20 07        jr nz,WALL_BOUNCE_1FB0      
1FA9:  FE 7F        cp $7F                      
1FAB:  D2 30 20     jp nc,bounce_reflect_x       ; -> bounce_reflect_x
1FAE:  18 04        jr WALL_BOUNCE_1FB4         

WALL_BOUNCE_1FB0:
1FB0:  FE 78        cp $78                      
1FB2:  30 7C        jr nc,bounce_reflect_x       ; -> bounce_reflect_x

WALL_BOUNCE_1FB4:
1FB4:  FD CB F8 F6  set 6,(iy-8)                
1FB8:  FE 50        cp $50                      
1FBA:  28 17        jr z,WALL_BOUNCE_1FD3       
1FBC:  D2 CD 20     jp nc,bounce_halve_retest_20CD
1FBF:  7B           ld a,e                      
1FC0:  FE 21        cp $21                      
1FC2:  28 03        jr z,WALL_BOUNCE_1FC7       
1FC4:  D2 CD 20     jp nc,bounce_halve_retest_20CD

WALL_BOUNCE_1FC7:
1FC7:  C6 30        add a,$30                   
1FC9:  BA           cp d                        
1FCA:  CA DE 1F     jp z,WALL_BOUNCE_1FDE       
1FCD:  D2 17 20     jp nc,bounce_reflect_y       ; -> bounce_reflect_y
1FD0:  C3 30 20     jp bounce_reflect_x          ; -> bounce_reflect_x

WALL_BOUNCE_1FD3:
1FD3:  7B           ld a,e                      
1FD4:  FE 21        cp $21                      
1FD6:  CA DE 1F     jp z,WALL_BOUNCE_1FDE       
1FD9:  D2 CD 20     jp nc,bounce_halve_retest_20CD
1FDC:  18 52        jr bounce_reflect_x          ; -> bounce_reflect_x

WALL_BOUNCE_1FDE:
1FDE:  3A 07 40     ld a,($4007)                
1FE1:  FD AE 83     xor (iy-125)                
1FE4:  CB 7F        bit 7,a                     
1FE6:  20 2F        jr nz,bounce_reflect_y       ; -> bounce_reflect_y
1FE8:  3A 09 40     ld a,($4009)                
1FEB:  FD AE 85     xor (iy-123)                
1FEE:  CB 7F        bit 7,a                     
1FF0:  20 3E        jr nz,bounce_reflect_x       ; -> bounce_reflect_x
1FF2:  FD CB 81 46  bit 0,(iy-127)              
1FF6:  20 56        jr nz,bounce_reflect_x_204E 
1FF8:  ED 5B 02 40  ld de,(obj_velx)             ; obj_velx
1FFC:  2A 04 40     ld hl,(obj_vely)             ; obj_vely
1FFF:  7A           ld a,d                      
2000:  2F           cpl                         
2001:  57           ld d,a                      
2002:  7B           ld a,e                      
2003:  2F           cpl                         
2004:  5F           ld e,a                      
2005:  13           inc de                      
2006:  7C           ld a,h                      
2007:  2F           cpl                         
2008:  67           ld h,a                      
2009:  7D           ld a,l                      
200A:  2F           cpl                         
200B:  6F           ld l,a                      
200C:  23           inc hl                      
200D:  ED 53 04 40  ld (obj_vely),de             ; obj_vely
2011:  22 02 40     ld (obj_velx),hl             ; obj_velx
2014:  C3 74 20     jp bounce_halve_retest       ; -> bounce_halve_retest

bounce_reflect_y:
2017:  FD CB F8 EE  set 5,(iy-8)                
201B:  FD CB 81 46  bit 0,(iy-127)              
201F:  20 2D        jr nz,bounce_reflect_x_204E 
2021:  2A 04 40     ld hl,(obj_vely)             ; obj_vely
2024:  7C           ld a,h                      
2025:  2F           cpl                         
2026:  67           ld h,a                      
2027:  7D           ld a,l                      
2028:  2F           cpl                         
2029:  6F           ld l,a                      
202A:  23           inc hl                      
202B:  22 04 40     ld (obj_vely),hl             ; obj_vely
202E:  18 44        jr bounce_halve_retest       ; -> bounce_halve_retest

bounce_reflect_x:
2030:  FD CB 81 46  bit 0,(iy-127)              
2034:  20 0F        jr nz,bounce_reflect_x_2045 
2036:  2A 02 40     ld hl,(obj_velx)             ; obj_velx
2039:  7C           ld a,h                      
203A:  2F           cpl                         
203B:  67           ld h,a                      
203C:  7D           ld a,l                      
203D:  2F           cpl                         
203E:  6F           ld l,a                      
203F:  23           inc hl                      
2040:  22 02 40     ld (obj_velx),hl             ; obj_velx
2043:  18 2F        jr bounce_halve_retest       ; -> bounce_halve_retest

bounce_reflect_x_2045:
2045:  CD CE 20     call WALL_HIT_FX             ; -> WALL_HIT_FX
2048:  FD CB F8 FE  set 7,(iy-8)                
204C:  18 7F        jr bounce_halve_retest_20CD 

bounce_reflect_x_204E:
204E:  CD CE 20     call WALL_HIT_FX             ; -> WALL_HIT_FX
2051:  FD CB F8 FE  set 7,(iy-8)                
2055:  3A 07 40     ld a,($4007)                
2058:  FE 7E        cp $7E                      
205A:  30 71        jr nc,bounce_halve_retest_20CD
205C:  2A 06 40     ld hl,(obj_posx)             ; obj_posx
205F:  ED 5B 02 40  ld de,(obj_velx)             ; obj_velx
2063:  19           add hl,de                   
2064:  22 06 40     ld (obj_posx),hl             ; obj_posx
2067:  2A 08 40     ld hl,(obj_posy)             ; obj_posy
206A:  ED 5B 04 40  ld de,(obj_vely)             ; obj_vely
206E:  19           add hl,de                   
206F:  22 04 40     ld (obj_vely),hl             ; obj_vely
2072:  18 59        jr bounce_halve_retest_20CD 

bounce_halve_retest:
2074:  CD CE 20     call WALL_HIT_FX             ; -> WALL_HIT_FX
2077:  FD CB 83 2E  sra (iy-125)                
207B:  FD CB 82 1E  rr (iy-126)                 
207F:  FD CB 85 2E  sra (iy-123)                
2083:  FD CB 84 1E  rr (iy-124)                 
2087:  FD 6E 82     ld l,(iy-126)               
208A:  FD 66 83     ld h,(iy-125)               
208D:  23           inc hl                      
208E:  7C           ld a,h                      
208F:  B5           or l                        
2090:  20 08        jr nz,bounce_halve_retest_209A
2092:  FD 36 82 00  ld (iy-126),$00             
2096:  FD 36 83 00  ld (iy-125),$00             

bounce_halve_retest_209A:
209A:  FD 6E 84     ld l,(iy-124)               
209D:  FD 66 85     ld h,(iy-123)               
20A0:  23           inc hl                      
20A1:  7C           ld a,h                      
20A2:  B5           or l                        
20A3:  20 08        jr nz,bounce_halve_retest_20AD
20A5:  FD 36 84 00  ld (iy-124),$00             
20A9:  FD 36 85 00  ld (iy-123),$00             

bounce_halve_retest_20AD:
20AD:  3A 02 40     ld a,(obj_velx)              ; obj_velx
20B0:  FD B6 83     or (iy-125)                 
20B3:  FD B6 84     or (iy-124)                 
20B6:  FD B6 85     or (iy-123)                 
20B9:  28 12        jr z,bounce_halve_retest_20CD
20BB:  FD CB F8 FE  set 7,(iy-8)                
20BF:  FD CB 80 66  bit 4,(iy-128)              
20C3:  28 05        jr z,bounce_halve_retest_20CA
20C5:  3E 08        ld a,$08                    
20C7:  CD 04 2E     call SOUND_CMD_SEND          ; -> SOUND_CMD_SEND

bounce_halve_retest_20CA:
20CA:  C3 2A 1F     jp WALL_BOUNCE               ; -> WALL_BOUNCE

bounce_halve_retest_20CD:
20CD:  C9           ret                         

WALL_HIT_FX:
20CE:  FD 36 1A 80  ld (iy+26),$80              
20D2:  11 00 00     ld de,$0000                  ; RESET
20D5:  FD 66 87     ld h,(iy-121)               
20D8:  FD 6E 89     ld l,(iy-119)               
20DB:  3A 78 40     ld a,($4078)                
20DE:  CB 77        bit 6,a                     
20E0:  20 31        jr nz,WALL_HIT_FX_2113      
20E2:  CB 7C        bit 7,h                     
20E4:  28 17        jr z,WALL_HIT_FX_20FD       
20E6:  CB 7D        bit 7,l                     
20E8:  28 0A        jr z,WALL_HIT_FX_20F4       
20EA:  1E 01        ld e,$01                    
20EC:  CB 6F        bit 5,a                     
20EE:  28 51        jr z,WALL_HIT_FX_2141       
20F0:  1E 08        ld e,$08                    
20F2:  18 4D        jr WALL_HIT_FX_2141         

WALL_HIT_FX_20F4:
20F4:  1E 02        ld e,$02                    
20F6:  CB 6F        bit 5,a                     
20F8:  28 47        jr z,WALL_HIT_FX_2141       
20FA:  1C           inc e                       
20FB:  18 44        jr WALL_HIT_FX_2141         

WALL_HIT_FX_20FD:
20FD:  CB 7D        bit 7,l                     
20FF:  28 09        jr z,WALL_HIT_FX_210A       
2101:  1E 06        ld e,$06                    
2103:  CB 6F        bit 5,a                     
2105:  28 3A        jr z,WALL_HIT_FX_2141       
2107:  1C           inc e                       
2108:  18 37        jr WALL_HIT_FX_2141         

WALL_HIT_FX_210A:
210A:  1E 04        ld e,$04                    
210C:  CB 6F        bit 5,a                     
210E:  20 31        jr nz,WALL_HIT_FX_2141      
2110:  1C           inc e                       
2111:  18 2E        jr WALL_HIT_FX_2141         

WALL_HIT_FX_2113:
2113:  CB 7C        bit 7,h                     
2115:  28 16        jr z,WALL_HIT_FX_212D       
2117:  1E 0A        ld e,$0A                    
2119:  CB 7D        bit 7,l                     
211B:  28 08        jr z,WALL_HIT_FX_2125       
211D:  CB 6F        bit 5,a                     
211F:  28 20        jr z,WALL_HIT_FX_2141       
2121:  1E 13        ld e,$13                    
2123:  18 1C        jr WALL_HIT_FX_2141         

WALL_HIT_FX_2125:
2125:  CB 6F        bit 5,a                     
2127:  28 18        jr z,WALL_HIT_FX_2141       
2129:  1E 0D        ld e,$0D                    
212B:  18 14        jr WALL_HIT_FX_2141         

WALL_HIT_FX_212D:
212D:  1E 10        ld e,$10                    
212F:  CB 7D        bit 7,l                     
2131:  28 08        jr z,WALL_HIT_FX_213B       
2133:  CB 6F        bit 5,a                     
2135:  28 0A        jr z,WALL_HIT_FX_2141       
2137:  1E 13        ld e,$13                    
2139:  18 06        jr WALL_HIT_FX_2141         

WALL_HIT_FX_213B:
213B:  CB 6F        bit 5,a                     
213D:  28 02        jr z,WALL_HIT_FX_2141       
213F:  1E 0D        ld e,$0D                    

WALL_HIT_FX_2141:
2141:  21 9A 40     ld hl,wall_fx                ; wall_fx
2144:  19           add hl,de                   
2145:  36 96        ld (hl),$96                 
2147:  C9           ret                         

APPLY_FRICTION:
2148:  7C           ld a,h                      
2149:  B5           or l                        
214A:  28 17        jr z,APPLY_FRICTION_2163    
214C:  54           ld d,h                      
214D:  5D           ld e,l                      
214E:  CB 2A        sra d                       
2150:  CB 1B        rr e                        
2152:  CB 2A        sra d                       
2154:  CB 1B        rr e                        
2156:  CB 2A        sra d                       
2158:  CB 1B        rr e                        
215A:  7A           ld a,d                      
215B:  B3           or e                        
215C:  20 02        jr nz,APPLY_FRICTION_2160   
215E:  1E 01        ld e,$01                    

APPLY_FRICTION_2160:
2160:  AF           xor a                       
2161:  ED 52        sbc hl,de                   

APPLY_FRICTION_2163:
2163:  C9           ret                         

CLAMP_SPEED:
2164:  F5           push af                     
2165:  D5           push de                     
2166:  EB           ex de,hl                    
2167:  B7           or a                        
2168:  CB 7A        bit 7,d                     
216A:  20 0C        jr nz,CLAMP_SPEED_2178      
216C:  21 BC 02     ld hl,$02BC                 
216F:  ED 52        sbc hl,de                   
2171:  30 0F        jr nc,CLAMP_SPEED_2182      
2173:  11 BC 02     ld de,$02BC                 
2176:  18 0A        jr CLAMP_SPEED_2182         

CLAMP_SPEED_2178:
2178:  21 44 FD     ld hl,$FD44                 
217B:  ED 52        sbc hl,de                   
217D:  38 03        jr c,CLAMP_SPEED_2182       
217F:  11 44 FD     ld de,$FD44                 

CLAMP_SPEED_2182:
2182:  EB           ex de,hl                    
2183:  D1           pop de                      
2184:  F1           pop af                      
2185:  C9           ret                         

DIFFICULTY_CALC:
2186:  3A 67 40     ld a,(wave_num)              ; wave_num
2189:  07           rlca                        
218A:  E6 1E        and $1E                     
218C:  47           ld b,a                      
218D:  CD 06 29     call RANDOM_NEXT             ; -> RANDOM_NEXT
2190:  E6 2F        and $2F                     
2192:  CB EF        set 5,a                     
2194:  90           sub b                       
2195:  32 7C 40     ld (special_countdown),a     ; special_countdown
2198:  C9           ret                         

COLLIDE_SCAN:
2199:  DB 09        in a,($09)                   ; WATCHDOG

COLLIDE_SCAN_219B:
219B:  DD 5E 00     ld e,(ix+0)                 
219E:  DD 56 01     ld d,(ix+1)                 
21A1:  DD E5        push ix                     
21A3:  D5           push de                     
21A4:  DD E1        pop ix                      
21A6:  DD CB 00 7E  bit 7,(ix+0)                
21AA:  28 55        jr z,cs_next_entry           ; -> cs_next_entry
21AC:  DD CB 01 6E  bit 5,(ix+1)                
21B0:  20 4F        jr nz,cs_next_entry          ; -> cs_next_entry
21B2:  DD 7E 01     ld a,(ix+1)                 
21B5:  CB 4F        bit 1,a                     
21B7:  20 07        jr nz,cs_delta_calc          ; -> cs_delta_calc
21B9:  FD AE 81     xor (iy-127)                
21BC:  CB 77        bit 6,a                     
21BE:  28 41        jr z,cs_next_entry           ; -> cs_next_entry

cs_delta_calc:
21C0:  DD 7E 07     ld a,(ix+7)                 
21C3:  FD 96 87     sub (iy-121)                
21C6:  30 02        jr nc,cs_delta_calc_21CA    
21C8:  ED 44        neg                         

cs_delta_calc_21CA:
21CA:  57           ld d,a                      
21CB:  DD 7E 09     ld a,(ix+9)                 
21CE:  FD 96 89     sub (iy-119)                
21D1:  30 02        jr nc,cs_delta_calc_21D5    
21D3:  ED 44        neg                         

cs_delta_calc_21D5:
21D5:  5F           ld e,a                      
21D6:  FD CB 81 66  bit 4,(iy-127)              
21DA:  28 09        jr z,cs_delta_calc_21E5     
21DC:  DD CB 00 66  bit 4,(ix+0)                
21E0:  28 03        jr z,cs_delta_calc_21E5     
21E2:  CD EC 24     call SHOOTER_PROMOTE         ; -> SHOOTER_PROMOTE

cs_delta_calc_21E5:
21E5:  DD 7E 10     ld a,(ix+16)                
21E8:  FD BE 90     cp (iy-112)                 
21EB:  30 03        jr nc,cs_delta_calc_21F0    
21ED:  FD 7E 90     ld a,(iy-112)               

cs_delta_calc_21F0:
21F0:  BA           cp d                        
21F1:  38 0E        jr c,cs_next_entry           ; -> cs_next_entry
21F3:  DD 7E 11     ld a,(ix+17)                
21F6:  FD BE 91     cp (iy-111)                 
21F9:  30 03        jr nc,cs_delta_calc_21FE    
21FB:  FD 7E 91     ld a,(iy-111)               

cs_delta_calc_21FE:
21FE:  BB           cp e                        
21FF:  30 0F        jr nc,cs_hit_confirm         ; -> cs_hit_confirm

cs_next_entry:
2201:  DD E1        pop ix                      
2203:  11 06 00     ld de,$0006                 
2206:  DD 19        add ix,de                   
2208:  FD 34 FE     inc (iy-2)                  
220B:  10 8E        djnz COLLIDE_SCAN_219B      
220D:  C3 34 23     jp cs_kill_object_2334      

cs_hit_confirm:
2210:  FD CB 81 76  bit 6,(iy-127)              
2214:  20 3D        jr nz,cs_kill_object         ; -> cs_kill_object
2216:  DD CB 01 4E  bit 1,(ix+1)                
221A:  28 37        jr z,cs_kill_object          ; -> cs_kill_object
221C:  FD CB 80 66  bit 4,(iy-128)              
2220:  CA 4F 22     jp z,cs_hit_confirm_224F    
2223:  3A 0E 40     ld a,(obj_fire_timer)        ; obj_fire_timer
2226:  B7           or a                        
2227:  C2 4F 22     jp nz,cs_hit_confirm_224F   
222A:  DD 36 0D 02  ld (ix+13),$02              
222E:  DD 36 11 06  ld (ix+17),$06              
2232:  21 AF CB     ld hl,$CBAF                 
2235:  DD 74 16     ld (ix+22),h                
2238:  DD 75 15     ld (ix+21),l                
223B:  DD E1        pop ix                      
223D:  DD 56 05     ld d,(ix+5)                 
2240:  DD 5E 04     ld e,(ix+4)                 
2243:  D5           push de                     
2244:  DD E1        pop ix                      
2246:  DD 74 0B     ld (ix+11),h                
2249:  DD 75 0A     ld (ix+10),l                
224C:  C3 34 23     jp cs_kill_object_2334      

cs_hit_confirm_224F:
224F:  F1           pop af                      
2250:  C3 34 23     jp cs_kill_object_2334      

cs_kill_object:
2253:  F1           pop af                      
2254:  DD CB 00 BE  res 7,(ix+0)                
2258:  DD 36 13 00  ld (ix+19),$00              
225C:  3A 7D 40     ld a,(obj_index)             ; obj_index
225F:  FE 0E        cp $0E                      
2261:  38 17        jr c,cs_kill_object_227A    
2263:  3A 7E 40     ld a,(obj_class)             ; obj_class
2266:  FD BE FA     cp (iy-6)                   
2269:  28 05        jr z,cs_kill_object_2270    
226B:  FD BE FB     cp (iy-5)                   
226E:  20 55        jr nz,cs_kill_object_22C5   

cs_kill_object_2270:
2270:  3E 04        ld a,$04                    
2272:  DD CB 01 7E  bit 7,(ix+1)                
2276:  20 58        jr nz,cs_kill_object_22D0   
2278:  18 12        jr cs_kill_object_228C      

cs_kill_object_227A:
227A:  FD BE FA     cp (iy-6)                   
227D:  28 05        jr z,cs_kill_object_2284    
227F:  FD BE FB     cp (iy-5)                   
2282:  20 41        jr nz,cs_kill_object_22C5   

cs_kill_object_2284:
2284:  3E 04        ld a,$04                    
2286:  FD CB 81 7E  bit 7,(iy-127)              
228A:  20 44        jr nz,cs_kill_object_22D0   

cs_kill_object_228C:
228C:  3E 02        ld a,$02                    
228E:  CD F6 2D     call HEARTBEAT_SOUND         ; -> HEARTBEAT_SOUND
2291:  3E 05        ld a,$05                    
2293:  FD CB F7 46  bit 0,(iy-9)                
2297:  20 06        jr nz,cs_kill_object_229F   
2299:  FD CB EA 46  bit 0,(iy-22)               
229D:  20 31        jr nz,cs_kill_object_22D0   

cs_kill_object_229F:
229F:  DD E5        push ix                     
22A1:  DD 21 DD 41  ld ix,$41DD                 
22A5:  DD CB 00 BE  res 7,(ix+0)                
22A9:  DD 21 F4 41  ld ix,$41F4                 
22AD:  DD CB 00 BE  res 7,(ix+0)                
22B1:  DD 21 0B 42  ld ix,$420B                 
22B5:  DD CB 00 BE  res 7,(ix+0)                
22B9:  DD 21 22 42  ld ix,$4222                 
22BD:  DD CB 00 BE  res 7,(ix+0)                
22C1:  DD E1        pop ix                      
22C3:  18 0B        jr cs_kill_object_22D0      

cs_kill_object_22C5:
22C5:  3A 0D 40     ld a,($400D)                
22C8:  DD BE 0D     cp (ix+13)                  
22CB:  30 03        jr nc,cs_kill_object_22D0   
22CD:  DD 7E 0D     ld a,(ix+13)                

cs_kill_object_22D0:
22D0:  32 6F 40     ld (kill_tier),a             ; kill_tier
22D3:  B7           or a                        
22D4:  C4 35 23     call nz,SCORE_ADD            ; -> SCORE_ADD
22D7:  FD CB 81 46  bit 0,(iy-127)              
22DB:  28 0C        jr z,cs_kill_object_22E9    
22DD:  DD 7E 07     ld a,(ix+7)                 
22E0:  32 07 40     ld ($4007),a                
22E3:  DD 7E 09     ld a,(ix+9)                 
22E6:  32 09 40     ld ($4009),a                

cs_kill_object_22E9:
22E9:  3E 00        ld a,$00                    
22EB:  FD CB 80 66  bit 4,(iy-128)              
22EF:  28 02        jr z,cs_kill_object_22F3    
22F1:  3E 10        ld a,$10                    

cs_kill_object_22F3:
22F3:  CD BF 27     call OBJ_DESTROY             ; -> OBJ_DESTROY
22F6:  FD 77 8B     ld (iy-117),a               
22F9:  3A 7D 40     ld a,(obj_index)             ; obj_index
22FC:  FE 01        cp $01                      
22FE:  28 04        jr z,cs_kill_object_2304    
2300:  FE 0E        cp $0E                      
2302:  38 03        jr c,cs_kill_object_2307    

cs_kill_object_2304:
2304:  3A 7E 40     ld a,(obj_class)             ; obj_class

cs_kill_object_2307:
2307:  FD BE FA     cp (iy-6)                   
230A:  20 1A        jr nz,cs_kill_object_2326   
230C:  FD 36 FA 00  ld (iy-6),$00               
2310:  FD 36 A2 07  ld (iy-94),$07              
2314:  FD 36 FC 05  ld (iy-4),$05               
2318:  FD CB EA 46  bit 0,(iy-22)               
231C:  28 08        jr z,cs_kill_object_2326    
231E:  FD 36 A2 02  ld (iy-94),$02              
2322:  FD 36 FC 02  ld (iy-4),$02               

cs_kill_object_2326:
2326:  FD BE FB     cp (iy-5)                   
2329:  20 04        jr nz,cs_kill_object_232F   
232B:  FD 36 FB 00  ld (iy-5),$00               

cs_kill_object_232F:
232F:  FE 0E        cp $0E                      
2331:  DC 67 24     call c,ENEMY_ALIVE_SCAN      ; -> ENEMY_ALIVE_SCAN

cs_kill_object_2334:
2334:  C9           ret                         

SCORE_ADD:
2335:  F5           push af                     
2336:  C5           push bc                     
2337:  D5           push de                     
2338:  E5           push hl                     
2339:  FD CB AE 7E  bit 7,(iy-82)               
233D:  CA 09 24     jp z,SCORE_ADD_2409         
2340:  3D           dec a                       
2341:  B7           or a                        
2342:  07           rlca                        
2343:  5F           ld e,a                      
2344:  16 00        ld d,$00                    
2346:  21 DB 2F     ld hl,$2FDB                 
2349:  19           add hl,de                   
234A:  06 02        ld b,$02                    
234C:  11 36 40     ld de,score_cur              ; score_cur
234F:  AF           xor a                       

SCORE_ADD_2350:
2350:  1A           ld a,(de)                   
2351:  8E           adc a,(hl)                  
2352:  27           daa                         
2353:  12           ld (de),a                   
2354:  23           inc hl                      
2355:  13           inc de                      
2356:  10 F8        djnz SCORE_ADD_2350         
2358:  06 02        ld b,$02                    

SCORE_ADD_235A:
235A:  1A           ld a,(de)                   
235B:  CE 00        adc a,$00                   
235D:  27           daa                         
235E:  12           ld (de),a                   
235F:  13           inc de                      
2360:  10 F8        djnz SCORE_ADD_235A         
2362:  06 04        ld b,$04                    
2364:  11 39 40     ld de,$4039                 
2367:  21 17 46     ld hl,$4617                 
236A:  CD 0E 24     call BCD_TO_DIGITS           ; -> BCD_TO_DIGITS
236D:  ED 5B 60 40  ld de,(xlife_thr1)           ; xlife_thr1
2371:  2A 38 40     ld hl,($4038)               
2374:  B7           or a                        
2375:  ED 52        sbc hl,de                   
2377:  DA 09 24     jp c,SCORE_ADD_2409         
237A:  FD CB EA 6E  bit 5,(iy-22)               
237E:  20 19        jr nz,SCORE_ADD_2399        
2380:  FD CB EA EE  set 5,(iy-22)               
2384:  21 07 00     ld hl,$0007                 
2387:  11 FC 0D     ld de,$0DFC                 
238A:  19           add hl,de                   
238B:  7C           ld a,h                      
238C:  E6 0F        and $0F                     
238E:  F6 C0        or $C0                      
2390:  67           ld h,a                      
2391:  CD 54 18     call VG_WAIT_DONE            ; -> VG_WAIT_DONE
2394:  22 A6 82     ld ($82A6),hl               
2397:  18 54        jr SCORE_ADD_23ED           

SCORE_ADD_2399:
2399:  ED 5B 62 40  ld de,(xlife_thr2)           ; xlife_thr2
239D:  2A 38 40     ld hl,($4038)               
23A0:  B7           or a                        
23A1:  ED 52        sbc hl,de                   
23A3:  38 64        jr c,SCORE_ADD_2409         
23A5:  FD CB EA 66  bit 4,(iy-22)               
23A9:  20 19        jr nz,SCORE_ADD_23C4        
23AB:  FD CB EA E6  set 4,(iy-22)               
23AF:  21 07 00     ld hl,$0007                 
23B2:  11 FC 0D     ld de,$0DFC                 
23B5:  19           add hl,de                   
23B6:  7C           ld a,h                      
23B7:  E6 0F        and $0F                     
23B9:  F6 C0        or $C0                      
23BB:  67           ld h,a                      
23BC:  CD 54 18     call VG_WAIT_DONE            ; -> VG_WAIT_DONE
23BF:  22 B0 82     ld ($82B0),hl               
23C2:  18 29        jr SCORE_ADD_23ED           

SCORE_ADD_23C4:
23C4:  ED 5B 64 40  ld de,(xlife_thr3)           ; xlife_thr3
23C8:  2A 38 40     ld hl,($4038)               
23CB:  B7           or a                        
23CC:  ED 52        sbc hl,de                   
23CE:  38 39        jr c,SCORE_ADD_2409         
23D0:  FD CB EA 5E  bit 3,(iy-22)               
23D4:  20 33        jr nz,SCORE_ADD_2409        
23D6:  FD CB EA DE  set 3,(iy-22)               
23DA:  21 07 00     ld hl,$0007                 
23DD:  11 FC 0D     ld de,$0DFC                 
23E0:  19           add hl,de                   
23E1:  7C           ld a,h                      
23E2:  E6 0F        and $0F                     
23E4:  F6 C0        or $C0                      
23E6:  67           ld h,a                      
23E7:  CD 54 18     call VG_WAIT_DONE            ; -> VG_WAIT_DONE
23EA:  22 BA 82     ld ($82BA),hl               

SCORE_ADD_23ED:
23ED:  3E 00        ld a,$00                    
23EF:  CD 04 2E     call SOUND_CMD_SEND          ; -> SOUND_CMD_SEND
23F2:  FD 36 A1 00  ld (iy-95),$00              
23F6:  3E 14        ld a,$14                    
23F8:  CD 04 2E     call SOUND_CMD_SEND          ; -> SOUND_CMD_SEND
23FB:  FD 36 A1 03  ld (iy-95),$03              
23FF:  FD 36 E9 FF  ld (iy-23),$FF              
2403:  FD 34 E6     inc (iy-26)                 
2406:  FD 34 ED     inc (iy-19)                 

SCORE_ADD_2409:
2409:  E1           pop hl                      
240A:  D1           pop de                      
240B:  C1           pop bc                      
240C:  F1           pop af                      
240D:  C9           ret                         

BCD_TO_DIGITS:
240E:  FD 36 11 FF  ld (iy+17),$FF              

BCD_TO_DIGITS_2412:
2412:  1A           ld a,(de)                   
2413:  0F           rrca                        
2414:  0F           rrca                        
2415:  0F           rrca                        
2416:  0F           rrca                        
2417:  E6 0F        and $0F                     
2419:  20 0A        jr nz,BCD_TO_DIGITS_2425    
241B:  FD CB 11 7E  bit 7,(iy+17)               
241F:  28 04        jr z,BCD_TO_DIGITS_2425     
2421:  3E 0A        ld a,$0A                    
2423:  18 04        jr BCD_TO_DIGITS_2429       

BCD_TO_DIGITS_2425:
2425:  FD 36 11 00  ld (iy+17),$00              

BCD_TO_DIGITS_2429:
2429:  CD 4A 24     call BCD_TO_DIGITS_244A     
242C:  1A           ld a,(de)                   
242D:  E6 0F        and $0F                     
242F:  20 0E        jr nz,BCD_TO_DIGITS_243F    
2431:  FD CB 11 7E  bit 7,(iy+17)               
2435:  28 0C        jr z,BCD_TO_DIGITS_2443     
2437:  78           ld a,b                      
2438:  3D           dec a                       
2439:  28 08        jr z,BCD_TO_DIGITS_2443     
243B:  3E 0A        ld a,$0A                    
243D:  18 04        jr BCD_TO_DIGITS_2443       

BCD_TO_DIGITS_243F:
243F:  FD 36 11 00  ld (iy+17),$00              

BCD_TO_DIGITS_2443:
2443:  CD 4A 24     call BCD_TO_DIGITS_244A     
2446:  1B           dec de                      
2447:  10 C9        djnz BCD_TO_DIGITS_2412     
2449:  C9           ret                         

BCD_TO_DIGITS_244A:
244A:  D5           push de                     
244B:  DD E5        push ix                     
244D:  E6 0F        and $0F                     
244F:  07           rlca                        
2450:  5F           ld e,a                      
2451:  16 00        ld d,$00                    
2453:  DD 21 D5 3F  ld ix,$3FD5                 
2457:  DD 19        add ix,de                   
2459:  DD 7E 00     ld a,(ix+0)                 
245C:  77           ld (hl),a                   
245D:  23           inc hl                      
245E:  DD 7E 01     ld a,(ix+1)                 
2461:  77           ld (hl),a                   
2462:  23           inc hl                      
2463:  DD E1        pop ix                      
2465:  D1           pop de                      
2466:  C9           ret                         

ENEMY_ALIVE_SCAN:
2467:  F5           push af                     
2468:  C5           push bc                     
2469:  D5           push de                     
246A:  E5           push hl                     
246B:  DD E5        push ix                     
246D:  FD 46 E7     ld b,(iy-25)                
2470:  0E 02        ld c,$02                    
2472:  21 96 2E     ld hl,$2E96                 

ENEMY_ALIVE_SCAN_2475:
2475:  5E           ld e,(hl)                   
2476:  23           inc hl                      
2477:  56           ld d,(hl)                   
2478:  1A           ld a,(de)                   
2479:  CB 7F        bit 7,a                     
247B:  28 30        jr z,ENEMY_ALIVE_SCAN_24AD  
247D:  13           inc de                      
247E:  1A           ld a,(de)                   
247F:  1B           dec de                      
2480:  CB 6F        bit 5,a                     
2482:  20 29        jr nz,ENEMY_ALIVE_SCAN_24AD 
2484:  0D           dec c                       
2485:  28 5A        jr z,ENEMY_ALIVE_SCAN_24E1  
2487:  3A 7B 40     ld a,(shooter_b)             ; shooter_b
248A:  B7           or a                        
248B:  20 20        jr nz,ENEMY_ALIVE_SCAN_24AD 
248D:  3A 77 40     ld a,(shooter_state)         ; shooter_state
2490:  FE 01        cp $01                      
2492:  28 19        jr z,ENEMY_ALIVE_SCAN_24AD  
2494:  78           ld a,b                      
2495:  ED 44        neg                         
2497:  FD 86 E7     add a,(iy-25)               
249A:  C6 02        add a,$02                   
249C:  FD BE FA     cp (iy-6)                   
249F:  28 0C        jr z,ENEMY_ALIVE_SCAN_24AD  
24A1:  32 7B 40     ld (shooter_b),a             ; shooter_b
24A4:  D5           push de                     
24A5:  DD E1        pop ix                      
24A7:  DD 7E 0A     ld a,(ix+10)                
24AA:  DD 77 0F     ld (ix+15),a                

ENEMY_ALIVE_SCAN_24AD:
24AD:  11 05 00     ld de,$0005                 
24B0:  19           add hl,de                   
24B1:  10 C2        djnz ENEMY_ALIVE_SCAN_2475  
24B3:  CB 41        bit 0,c                     
24B5:  20 0A        jr nz,ENEMY_ALIVE_SCAN_24C1 
24B7:  FD CB EB F6  set 6,(iy-21)               
24BB:  FD 36 F7 00  ld (iy-9),$00               
24BF:  18 24        jr ENEMY_ALIVE_SCAN_24E5    

ENEMY_ALIVE_SCAN_24C1:
24C1:  FD 36 F7 01  ld (iy-9),$01               
24C5:  3A 67 40     ld a,(wave_num)              ; wave_num
24C8:  FE 0C        cp $0C                      
24CA:  38 08        jr c,ENEMY_ALIVE_SCAN_24D4  
24CC:  FD 36 A2 02  ld (iy-94),$02              
24D0:  FD 36 FC 02  ld (iy-4),$02               

ENEMY_ALIVE_SCAN_24D4:
24D4:  3A 3B 40     ld a,($403B)                
24D7:  FE 03        cp $03                      
24D9:  38 0A        jr c,ENEMY_ALIVE_SCAN_24E5  
24DB:  FD CB EA C6  set 0,(iy-22)               
24DF:  18 04        jr ENEMY_ALIVE_SCAN_24E5    

ENEMY_ALIVE_SCAN_24E1:
24E1:  FD 36 F7 02  ld (iy-9),$02               

ENEMY_ALIVE_SCAN_24E5:
24E5:  DD E1        pop ix                      
24E7:  E1           pop hl                      
24E8:  D1           pop de                      
24E9:  C1           pop bc                      
24EA:  F1           pop af                      
24EB:  C9           ret                         

SHOOTER_PROMOTE:
24EC:  D5           push de                     
24ED:  CD AA 25     call MUL8X8                  ; -> MUL8X8
24F0:  EB           ex de,hl                    
24F1:  2A 80 40     ld hl,($4080)               
24F4:  B7           or a                        
24F5:  ED 52        sbc hl,de                   
24F7:  38 34        jr c,SHOOTER_PROMOTE_252D   
24F9:  78           ld a,b                      
24FA:  ED 44        neg                         
24FC:  C6 21        add a,$21                   
24FE:  FD BE FB     cp (iy-5)                   
2501:  20 0D        jr nz,SHOOTER_PROMOTE_2510  
2503:  08           ex af,af'                   
2504:  3A 77 40     ld a,(shooter_state)         ; shooter_state
2507:  FE 02        cp $02                      
2509:  30 22        jr nc,SHOOTER_PROMOTE_252D  
250B:  08           ex af,af'                   
250C:  FD 36 FB 00  ld (iy-5),$00               

SHOOTER_PROMOTE_2510:
2510:  ED 53 80 40  ld ($4080),de               
2514:  32 7A 40     ld (shooter_a),a             ; shooter_a
2517:  3A 77 40     ld a,(shooter_state)         ; shooter_state
251A:  FE 01        cp $01                      
251C:  20 0F        jr nz,SHOOTER_PROMOTE_252D  
251E:  DD CB 00 8E  res 1,(ix+0)                
2522:  CD 06 29     call RANDOM_NEXT             ; -> RANDOM_NEXT
2525:  CB 67        bit 4,a                     
2527:  28 04        jr z,SHOOTER_PROMOTE_252D   
2529:  DD CB 00 CE  set 1,(ix+0)                

SHOOTER_PROMOTE_252D:
252D:  D1           pop de                      
252E:  C9           ret                         

AIM_AT_SHIP:
252F:  3A B9 40     ld a,(ship_x)                ; ship_x
2532:  FD 96 87     sub (iy-121)                
2535:  06 00        ld b,$00                    
2537:  30 04        jr nc,AIM_AT_SHIP_253D      
2539:  ED 44        neg                         
253B:  06 FF        ld b,$FF                    

AIM_AT_SHIP_253D:
253D:  57           ld d,a                      
253E:  3A BB 40     ld a,(ship_y)                ; ship_y
2541:  FD 96 89     sub (iy-119)                
2544:  0E 00        ld c,$00                    
2546:  30 04        jr nc,AIM_AT_SHIP_254C      
2548:  ED 44        neg                         
254A:  0E FF        ld c,$FF                    

AIM_AT_SHIP_254C:
254C:  5F           ld e,a                      
254D:  CD C1 25     call DIV_SLOPE               ; -> DIV_SLOPE
2550:  DD E5        push ix                     
2552:  3E 0F        ld a,$0F                    
2554:  EB           ex de,hl                    
2555:  DD 21 41 30  ld ix,$3041                 

AIM_AT_SHIP_2559:
2559:  DD 6E 00     ld l,(ix+0)                 
255C:  DD 23        inc ix                      
255E:  DD 66 00     ld h,(ix+0)                 
2561:  DD 23        inc ix                      
2563:  B7           or a                        
2564:  ED 52        sbc hl,de                   
2566:  38 03        jr c,AIM_AT_SHIP_256B       
2568:  3D           dec a                       
2569:  20 EE        jr nz,AIM_AT_SHIP_2559      

AIM_AT_SHIP_256B:
256B:  DD E1        pop ix                      
256D:  CB 78        bit 7,b                     
256F:  20 0A        jr nz,AIM_AT_SHIP_257B      
2571:  CB 79        bit 7,c                     
2573:  28 12        jr z,AIM_AT_SHIP_2587       
2575:  EE 0F        xor $0F                     
2577:  C6 30        add a,$30                   
2579:  18 0C        jr AIM_AT_SHIP_2587         

AIM_AT_SHIP_257B:
257B:  CB 79        bit 7,c                     
257D:  28 04        jr z,AIM_AT_SHIP_2583       
257F:  C6 20        add a,$20                   
2581:  18 04        jr AIM_AT_SHIP_2587         

AIM_AT_SHIP_2583:
2583:  EE 0F        xor $0F                     
2585:  C6 10        add a,$10                   

AIM_AT_SHIP_2587:
2587:  47           ld b,a                      
2588:  3A 0F 40     ld a,(obj_aim)               ; obj_aim
258B:  E6 3F        and $3F                     
258D:  4F           ld c,a                      
258E:  90           sub b                       
258F:  28 14        jr z,AIM_AT_SHIP_25A5       
2591:  38 06        jr c,AIM_AT_SHIP_2599       
2593:  FE 20        cp $20                      
2595:  30 0A        jr nc,AIM_AT_SHIP_25A1      
2597:  18 04        jr AIM_AT_SHIP_259D         

AIM_AT_SHIP_2599:
2599:  FE E0        cp $E0                      
259B:  30 04        jr nc,AIM_AT_SHIP_25A1      

AIM_AT_SHIP_259D:
259D:  79           ld a,c                      
259E:  3D           dec a                       
259F:  18 05        jr AIM_AT_SHIP_25A6         

AIM_AT_SHIP_25A1:
25A1:  79           ld a,c                      
25A2:  3C           inc a                       
25A3:  18 01        jr AIM_AT_SHIP_25A6         

AIM_AT_SHIP_25A5:
25A5:  79           ld a,c                      

AIM_AT_SHIP_25A6:
25A6:  32 0F 40     ld (obj_aim),a               ; obj_aim
25A9:  C9           ret                         

MUL8X8:
25AA:  C5           push bc                     
25AB:  D5           push de                     
25AC:  7A           ld a,d                      
25AD:  16 00        ld d,$00                    
25AF:  06 08        ld b,$08                    
25B1:  21 00 00     ld hl,$0000                  ; RESET

MUL8X8_25B4:
25B4:  0F           rrca                        
25B5:  30 01        jr nc,MUL8X8_25B8           
25B7:  19           add hl,de                   

MUL8X8_25B8:
25B8:  CB 23        sla e                       
25BA:  CB 12        rl d                        
25BC:  10 F6        djnz MUL8X8_25B4            
25BE:  D1           pop de                      
25BF:  C1           pop bc                      
25C0:  C9           ret                         

DIV_SLOPE:
25C1:  C5           push bc                     
25C2:  D5           push de                     
25C3:  D5           push de                     
25C4:  43           ld b,e                      
25C5:  0E 00        ld c,$00                    
25C7:  7A           ld a,d                      
25C8:  2F           cpl                         
25C9:  5F           ld e,a                      
25CA:  16 FF        ld d,$FF                    
25CC:  13           inc de                      
25CD:  21 00 00     ld hl,$0000                  ; RESET
25D0:  3E 11        ld a,$11                    

DIV_SLOPE_25D2:
25D2:  E5           push hl                     
25D3:  19           add hl,de                   
25D4:  30 01        jr nc,DIV_SLOPE_25D7        
25D6:  E3           ex (sp),hl                  

DIV_SLOPE_25D7:
25D7:  E1           pop hl                      
25D8:  F5           push af                     
25D9:  CB 11        rl c                        
25DB:  CB 10        rl b                        
25DD:  CB 15        rl l                        
25DF:  CB 14        rl h                        
25E1:  F1           pop af                      
25E2:  3D           dec a                       
25E3:  20 ED        jr nz,DIV_SLOPE_25D2        
25E5:  60           ld h,b                      
25E6:  69           ld l,c                      
25E7:  D1           pop de                      
25E8:  D1           pop de                      
25E9:  C1           pop bc                      
25EA:  C9           ret                         

SPAWN_MINE_SHOT:
25EB:  DD E5        push ix                     
25ED:  0E 04        ld c,$04                    
25EF:  FD CB 81 76  bit 6,(iy-127)              
25F3:  28 07        jr z,SPAWN_MINE_SHOT_25FC   
25F5:  06 08        ld b,$08                    
25F7:  21 09 2F     ld hl,$2F09                 
25FA:  18 05        jr SPAWN_MINE_SHOT_2601     

SPAWN_MINE_SHOT_25FC:
25FC:  06 04        ld b,$04                    
25FE:  21 F1 2E     ld hl,$2EF1                 

SPAWN_MINE_SHOT_2601:
2601:  56           ld d,(hl)                   
2602:  2B           dec hl                      
2603:  5E           ld e,(hl)                   
2604:  1A           ld a,(de)                   
2605:  CB 7F        bit 7,a                     
2607:  28 0B        jr z,SPAWN_MINE_SHOT_2614   
2609:  11 FB FF     ld de,$FFFB                 
260C:  19           add hl,de                   
260D:  05           dec b                       
260E:  0D           dec c                       
260F:  20 F0        jr nz,SPAWN_MINE_SHOT_2601  
2611:  C3 51 27     jp spawn_fail_clear          ; -> spawn_fail_clear

SPAWN_MINE_SHOT_2614:
2614:  D5           push de                     
2615:  DD E1        pop ix                      
2617:  78           ld a,b                      
2618:  C6 0D        add a,$0D                   
261A:  47           ld b,a                      
261B:  CD 1D 29     call OBJ_SPAWN               ; -> OBJ_SPAWN
261E:  FD 7E 86     ld a,(iy-122)               
2621:  DD 77 06     ld (ix+6),a                 
2624:  FD 7E 87     ld a,(iy-121)               
2627:  DD 77 07     ld (ix+7),a                 
262A:  FD 7E 88     ld a,(iy-120)               
262D:  DD 77 08     ld (ix+8),a                 
2630:  FD 7E 89     ld a,(iy-119)               
2633:  DD 77 09     ld (ix+9),a                 
2636:  FD 7E 8F     ld a,(iy-113)               
2639:  DD 77 0F     ld (ix+15),a                
263C:  FD CB 81 76  bit 6,(iy-127)              
2640:  28 04        jr z,SPAWN_MINE_SHOT_2646   
2642:  DD CB 01 F6  set 6,(ix+1)                

SPAWN_MINE_SHOT_2646:
2646:  DD 7E 0F     ld a,(ix+15)                
2649:  CB 67        bit 4,a                     
264B:  28 02        jr z,SPAWN_MINE_SHOT_264F   
264D:  EE 0F        xor $0F                     

SPAWN_MINE_SHOT_264F:
264F:  E6 0F        and $0F                     
2651:  07           rlca                        
2652:  21 21 30     ld hl,$3021                 
2655:  5F           ld e,a                      
2656:  16 00        ld d,$00                    
2658:  19           add hl,de                   
2659:  5E           ld e,(hl)                   
265A:  7B           ld a,e                      
265B:  CB 2F        sra a                       
265D:  CB 2F        sra a                       
265F:  DD 77 11     ld (ix+17),a                
2662:  23           inc hl                      
2663:  4E           ld c,(hl)                   
2664:  79           ld a,c                      
2665:  CB 2F        sra a                       
2667:  CB 2F        sra a                       
2669:  DD 77 10     ld (ix+16),a                
266C:  06 00        ld b,$00                    
266E:  16 00        ld d,$00                    
2670:  DD 7E 0F     ld a,(ix+15)                
2673:  E6 30        and $30                     
2675:  28 09        jr z,SPAWN_MINE_SHOT_2680   
2677:  FE 30        cp $30                      
2679:  28 05        jr z,SPAWN_MINE_SHOT_2680   
267B:  79           ld a,c                      
267C:  2F           cpl                         
267D:  4F           ld c,a                      
267E:  06 FF        ld b,$FF                    

SPAWN_MINE_SHOT_2680:
2680:  DD 7E 0F     ld a,(ix+15)                
2683:  CB 6F        bit 5,a                     
2685:  28 05        jr z,SPAWN_MINE_SHOT_268C   
2687:  7B           ld a,e                      
2688:  2F           cpl                         
2689:  5F           ld e,a                      
268A:  16 FF        ld d,$FF                    

SPAWN_MINE_SHOT_268C:
268C:  FD CB 81 76  bit 6,(iy-127)              
2690:  28 22        jr z,SPAWN_MINE_SHOT_26B4   
2692:  79           ld a,c                      
2693:  CB 2F        sra a                       
2695:  CB 2F        sra a                       
2697:  DD 86 07     add a,(ix+7)                
269A:  CB 78        bit 7,b                     
269C:  20 04        jr nz,SPAWN_MINE_SHOT_26A2  
269E:  38 07        jr c,SPAWN_MINE_SHOT_26A7   
26A0:  18 02        jr SPAWN_MINE_SHOT_26A4     

SPAWN_MINE_SHOT_26A2:
26A2:  30 03        jr nc,SPAWN_MINE_SHOT_26A7  

SPAWN_MINE_SHOT_26A4:
26A4:  DD 77 07     ld (ix+7),a                 

SPAWN_MINE_SHOT_26A7:
26A7:  7B           ld a,e                      
26A8:  CB 2F        sra a                       
26AA:  CB 2F        sra a                       
26AC:  DD 86 09     add a,(ix+9)                
26AF:  DD 77 09     ld (ix+9),a                 
26B2:  18 22        jr SPAWN_MINE_SHOT_26D6     

SPAWN_MINE_SHOT_26B4:
26B4:  DD 36 0C 19  ld (ix+12),$19              
26B8:  3A 0F 40     ld a,(obj_aim)               ; obj_aim
26BB:  FD 96 8A     sub (iy-118)                
26BE:  30 02        jr nc,SPAWN_MINE_SHOT_26C2  
26C0:  ED 44        neg                         

SPAWN_MINE_SHOT_26C2:
26C2:  FE 10        cp $10                      
26C4:  38 04        jr c,SPAWN_MINE_SHOT_26CA   
26C6:  FE 30        cp $30                      
26C8:  38 15        jr c,SPAWN_MINE_SHOT_26DF   

SPAWN_MINE_SHOT_26CA:
26CA:  FD CB EA 7E  bit 7,(iy-22)               
26CE:  28 0F        jr z,SPAWN_MINE_SHOT_26DF   
26D0:  DD CB 00 D6  set 2,(ix+0)                
26D4:  18 09        jr SPAWN_MINE_SHOT_26DF     

SPAWN_MINE_SHOT_26D6:
26D6:  3E 07        ld a,$07                    
26D8:  CD 04 2E     call SOUND_CMD_SEND          ; -> SOUND_CMD_SEND
26DB:  DD CB 00 D6  set 2,(ix+0)                

SPAWN_MINE_SHOT_26DF:
26DF:  21 00 00     ld hl,$0000                  ; RESET
26E2:  09           add hl,bc                   
26E3:  29           add hl,hl                   
26E4:  29           add hl,hl                   
26E5:  29           add hl,hl                   
26E6:  29           add hl,hl                   
26E7:  29           add hl,hl                   
26E8:  DD CB 00 56  bit 2,(ix+0)                
26EC:  28 03        jr z,SPAWN_MINE_SHOT_26F1   
26EE:  29           add hl,hl                   
26EF:  18 12        jr SPAWN_MINE_SHOT_2703     

SPAWN_MINE_SHOT_26F1:
26F1:  3A 67 40     ld a,(wave_num)              ; wave_num
26F4:  FD CB EB 5E  bit 3,(iy-21)               
26F8:  20 02        jr nz,SPAWN_MINE_SHOT_26FC  
26FA:  D6 03        sub $03                     

SPAWN_MINE_SHOT_26FC:
26FC:  09           add hl,bc                   
26FD:  09           add hl,bc                   
26FE:  09           add hl,bc                   
26FF:  09           add hl,bc                   
2700:  3D           dec a                       
2701:  20 F9        jr nz,SPAWN_MINE_SHOT_26FC  

SPAWN_MINE_SHOT_2703:
2703:  DD 74 03     ld (ix+3),h                 
2706:  DD 75 02     ld (ix+2),l                 
2709:  21 00 00     ld hl,$0000                  ; RESET
270C:  19           add hl,de                   
270D:  29           add hl,hl                   
270E:  29           add hl,hl                   
270F:  29           add hl,hl                   
2710:  29           add hl,hl                   
2711:  29           add hl,hl                   
2712:  DD CB 00 56  bit 2,(ix+0)                
2716:  28 03        jr z,SPAWN_MINE_SHOT_271B   
2718:  29           add hl,hl                   
2719:  18 12        jr SPAWN_MINE_SHOT_272D     

SPAWN_MINE_SHOT_271B:
271B:  3A 67 40     ld a,(wave_num)              ; wave_num
271E:  FD CB EB 5E  bit 3,(iy-21)               
2722:  20 02        jr nz,SPAWN_MINE_SHOT_2726  
2724:  D6 03        sub $03                     

SPAWN_MINE_SHOT_2726:
2726:  19           add hl,de                   
2727:  19           add hl,de                   
2728:  19           add hl,de                   
2729:  19           add hl,de                   
272A:  3D           dec a                       
272B:  20 F9        jr nz,SPAWN_MINE_SHOT_2726  

SPAWN_MINE_SHOT_272D:
272D:  DD CB 00 96  res 2,(ix+0)                
2731:  DD 74 05     ld (ix+5),h                 
2734:  DD 75 04     ld (ix+4),l                 
2737:  3A 0F 40     ld a,(obj_aim)               ; obj_aim
273A:  E6 3F        and $3F                     
273C:  5F           ld e,a                      
273D:  83           add a,e                     
273E:  83           add a,e                     
273F:  5F           ld e,a                      
2740:  16 00        ld d,$00                    
2742:  21 E7 0A     ld hl,$0AE7                 
2745:  19           add hl,de                   
2746:  7C           ld a,h                      
2747:  E6 0F        and $0F                     
2749:  F6 C0        or $C0                      
274B:  DD 77 16     ld (ix+22),a                
274E:  DD 75 15     ld (ix+21),l                

spawn_fail_clear:
2751:  FD CB 80 96  res 2,(iy-128)              
2755:  DD E1        pop ix                      
2757:  C9           ret                         

SPAWN_SPECIAL:
2758:  DD E5        push ix                     
275A:  FD 56 87     ld d,(iy-121)               
275D:  FD 5E 89     ld e,(iy-119)               
2760:  7B           ld a,e                      
2761:  FE A0        cp $A0                      
2763:  38 09        jr c,SPAWN_SPECIAL_276E     
2765:  7A           ld a,d                      
2766:  FE 10        cp $10                      
2768:  38 52        jr c,SPAWN_SPECIAL_27BC     
276A:  FE F0        cp $F0                      
276C:  30 4E        jr nc,SPAWN_SPECIAL_27BC    

SPAWN_SPECIAL_276E:
276E:  7A           ld a,d                      
276F:  FE 26        cp $26                      
2771:  38 0D        jr c,SPAWN_SPECIAL_2780     
2773:  FE DA        cp $DA                      
2775:  30 09        jr nc,SPAWN_SPECIAL_2780    
2777:  7B           ld a,e                      
2778:  FE 56        cp $56                      
277A:  38 04        jr c,SPAWN_SPECIAL_2780     
277C:  FE AA        cp $AA                      
277E:  38 3C        jr c,SPAWN_SPECIAL_27BC     

SPAWN_SPECIAL_2780:
2780:  0E 0C        ld c,$0C                    

SPAWN_SPECIAL_2782:
2782:  06 0B        ld b,$0B                    
2784:  21 4B 2F     ld hl,$2F4B                 

SPAWN_SPECIAL_2787:
2787:  56           ld d,(hl)                   
2788:  2B           dec hl                      
2789:  5E           ld e,(hl)                   
278A:  1A           ld a,(de)                   
278B:  CB 7F        bit 7,a                     
278D:  28 17        jr z,SPAWN_SPECIAL_27A6     
278F:  0D           dec c                       
2790:  28 14        jr z,SPAWN_SPECIAL_27A6     
2792:  11 FB FF     ld de,$FFFB                 
2795:  19           add hl,de                   
2796:  10 EF        djnz SPAWN_SPECIAL_2787     

SPAWN_SPECIAL_2798:
2798:  CD 06 29     call RANDOM_NEXT             ; -> RANDOM_NEXT
279B:  E6 0F        and $0F                     
279D:  28 F9        jr z,SPAWN_SPECIAL_2798     
279F:  FE 0B        cp $0B                      
27A1:  30 F5        jr nc,SPAWN_SPECIAL_2798    
27A3:  4F           ld c,a                      
27A4:  18 DC        jr SPAWN_SPECIAL_2782       

SPAWN_SPECIAL_27A6:
27A6:  D5           push de                     
27A7:  DD E1        pop ix                      
27A9:  3E 15        ld a,$15                    
27AB:  80           add a,b                     
27AC:  47           ld b,a                      
27AD:  CD 1D 29     call OBJ_SPAWN               ; -> OBJ_SPAWN
27B0:  3A 07 40     ld a,($4007)                
27B3:  DD 77 07     ld (ix+7),a                 
27B6:  3A 09 40     ld a,($4009)                
27B9:  DD 77 09     ld (ix+9),a                 

SPAWN_SPECIAL_27BC:
27BC:  DD E1        pop ix                      
27BE:  C9           ret                         

OBJ_DESTROY:
27BF:  F5           push af                     
27C0:  3A 00 40     ld a,(obj_flags0)            ; obj_flags0
27C3:  FD 66 87     ld h,(iy-121)               
27C6:  FD 6E 89     ld l,(iy-119)               
27C9:  E5           push hl                     
27CA:  01 17 00     ld bc,$0017                 
27CD:  11 00 40     ld de,obj_flags0             ; obj_flags0
27D0:  21 AC 2F     ld hl,$2FAC                 
27D3:  ED B0        ldir                        
27D5:  E1           pop hl                      
27D6:  FD 74 87     ld (iy-121),h               
27D9:  FD 75 89     ld (iy-119),l               
27DC:  3A 7D 40     ld a,(obj_index)             ; obj_index
27DF:  FE 0E        cp $0E                      
27E1:  38 07        jr c,OBJ_DESTROY_27EA       
27E3:  FE 16        cp $16                      
27E5:  30 03        jr nc,OBJ_DESTROY_27EA      
27E7:  3A 7E 40     ld a,(obj_class)             ; obj_class

OBJ_DESTROY_27EA:
27EA:  FE 16        cp $16                      
27EC:  30 41        jr nc,kill_snd_cmdship       ; -> kill_snd_cmdship
27EE:  FE 0E        cp $0E                      
27F0:  30 4B        jr nc,kill_snd_cmdship_283D 
27F2:  FE 02        cp $02                      
27F4:  30 23        jr nc,kill_snd_droid         ; -> kill_snd_droid
27F6:  3A 66 40     ld a,(lives)                 ; lives
27F9:  FE 01        cp $01                      
27FB:  20 04        jr nz,OBJ_DESTROY_2801      
27FD:  FD 36 A2 0A  ld (iy-94),$0A              

OBJ_DESTROY_2801:
2801:  FD 36 A1 00  ld (iy-95),$00              
2805:  3E 00        ld a,$00                    
2807:  CD 04 2E     call SOUND_CMD_SEND          ; -> SOUND_CMD_SEND
280A:  FD 36 8C 0F  ld (iy-116),$0F             
280E:  3E 01        ld a,$01                    
2810:  CD 04 2E     call SOUND_CMD_SEND          ; -> SOUND_CMD_SEND
2813:  FD 36 A1 05  ld (iy-95),$05              
2817:  18 24        jr kill_snd_cmdship_283D    

kill_snd_droid:
2819:  3A 6F 40     ld a,(kill_tier)             ; kill_tier
281C:  FE 03        cp $03                      
281E:  3E 02        ld a,$02                    
2820:  28 18        jr z,kill_snd_cmdship_283A  
2822:  3A 6F 40     ld a,(kill_tier)             ; kill_tier
2825:  FE 04        cp $04                      
2827:  3E 10        ld a,$10                    
2829:  28 0F        jr z,kill_snd_cmdship_283A  
282B:  3E 15        ld a,$15                    
282D:  18 0B        jr kill_snd_cmdship_283A    

kill_snd_cmdship:
282F:  3A 6F 40     ld a,(kill_tier)             ; kill_tier
2832:  FE 01        cp $01                      
2834:  3E 03        ld a,$03                    
2836:  28 02        jr z,kill_snd_cmdship_283A  
2838:  3E 11        ld a,$11                    

kill_snd_cmdship_283A:
283A:  CD 04 2E     call SOUND_CMD_SEND          ; -> SOUND_CMD_SEND

kill_snd_cmdship_283D:
283D:  F1           pop af                      
283E:  C9           ret                         

WALL_TRACK_STEER:
283F:  FD 56 87     ld d,(iy-121)               
2842:  FD 5E 89     ld e,(iy-119)               
2845:  FD CB 80 4E  bit 1,(iy-128)              
2849:  C2 76 28     jp nz,WALL_TRACK_STEER_2876 
284C:  CB 7A        bit 7,d                     
284E:  20 06        jr nz,WALL_TRACK_STEER_2856 
2850:  CB 7B        bit 7,e                     
2852:  20 0D        jr nz,WALL_TRACK_STEER_2861 
2854:  18 12        jr WALL_TRACK_STEER_2868    

WALL_TRACK_STEER_2856:
2856:  CB 7B        bit 7,e                     
2858:  28 15        jr z,WALL_TRACK_STEER_286F  
285A:  21 30 3E     ld hl,$3E30                 
285D:  0E 00        ld c,$00                    
285F:  18 3D        jr WALL_TRACK_STEER_289E    

WALL_TRACK_STEER_2861:
2861:  21 00 0E     ld hl,$0E00                 
2864:  0E 01        ld c,$01                    
2866:  18 36        jr WALL_TRACK_STEER_289E    

WALL_TRACK_STEER_2868:
2868:  21 10 1E     ld hl,$1E10                 
286B:  0E 00        ld c,$00                    
286D:  18 2F        jr WALL_TRACK_STEER_289E    

WALL_TRACK_STEER_286F:
286F:  21 20 2E     ld hl,$2E20                 
2872:  0E 01        ld c,$01                    
2874:  18 28        jr WALL_TRACK_STEER_289E    

WALL_TRACK_STEER_2876:
2876:  CB 7A        bit 7,d                     
2878:  20 06        jr nz,WALL_TRACK_STEER_2880 
287A:  CB 7B        bit 7,e                     
287C:  20 0D        jr nz,WALL_TRACK_STEER_288B 
287E:  18 12        jr WALL_TRACK_STEER_2892    

WALL_TRACK_STEER_2880:
2880:  CB 7B        bit 7,e                     
2882:  28 15        jr z,WALL_TRACK_STEER_2899  
2884:  21 20 12     ld hl,$1220                 
2887:  0E 01        ld c,$01                    
2889:  18 13        jr WALL_TRACK_STEER_289E    

WALL_TRACK_STEER_288B:
288B:  21 30 22     ld hl,$2230                 
288E:  0E 00        ld c,$00                    
2890:  18 0C        jr WALL_TRACK_STEER_289E    

WALL_TRACK_STEER_2892:
2892:  21 00 32     ld hl,$3200                 
2895:  0E 01        ld c,$01                    
2897:  18 05        jr WALL_TRACK_STEER_289E    

WALL_TRACK_STEER_2899:
2899:  21 10 02     ld hl,$0210                 
289C:  0E 00        ld c,$00                    

WALL_TRACK_STEER_289E:
289E:  7A           ld a,d                      
289F:  D6 80        sub $80                     
28A1:  30 02        jr nc,WALL_TRACK_STEER_28A5 
28A3:  ED 44        neg                         

WALL_TRACK_STEER_28A5:
28A5:  57           ld d,a                      
28A6:  7B           ld a,e                      
28A7:  D6 80        sub $80                     
28A9:  30 02        jr nc,WALL_TRACK_STEER_28AD 
28AB:  ED 44        neg                         

WALL_TRACK_STEER_28AD:
28AD:  5F           ld e,a                      
28AE:  CB 41        bit 0,c                     
28B0:  28 14        jr z,WALL_TRACK_STEER_28C6  
28B2:  7A           ld a,d                      
28B3:  FE 58        cp $58                      
28B5:  30 07        jr nc,WALL_TRACK_STEER_28BE 
28B7:  7B           ld a,e                      
28B8:  FE 21        cp $21                      
28BA:  38 2D        jr c,WALL_TRACK_STEER_28E9  
28BC:  18 2A        jr WALL_TRACK_STEER_28E8    

WALL_TRACK_STEER_28BE:
28BE:  7A           ld a,d                      
28BF:  D6 40        sub $40                     
28C1:  BB           cp e                        
28C2:  38 24        jr c,WALL_TRACK_STEER_28E8  
28C4:  18 23        jr WALL_TRACK_STEER_28E9    

WALL_TRACK_STEER_28C6:
28C6:  7A           ld a,d                      
28C7:  FE 21        cp $21                      
28C9:  38 1E        jr c,WALL_TRACK_STEER_28E9  
28CB:  FE 68        cp $68                      
28CD:  30 19        jr nc,WALL_TRACK_STEER_28E8 
28CF:  7B           ld a,e                      
28D0:  FE 30        cp $30                      
28D2:  38 08        jr c,WALL_TRACK_STEER_28DC  
28D4:  7A           ld a,d                      
28D5:  D6 10        sub $10                     
28D7:  BB           cp e                        
28D8:  38 0F        jr c,WALL_TRACK_STEER_28E9  
28DA:  18 0C        jr WALL_TRACK_STEER_28E8    

WALL_TRACK_STEER_28DC:
28DC:  7A           ld a,d                      
28DD:  FE 53        cp $53                      
28DF:  30 07        jr nc,WALL_TRACK_STEER_28E8 
28E1:  ED 44        neg                         
28E3:  C6 70        add a,$70                   
28E5:  BB           cp e                        
28E6:  30 01        jr nc,WALL_TRACK_STEER_28E9 

WALL_TRACK_STEER_28E8:
28E8:  65           ld h,l                      

WALL_TRACK_STEER_28E9:
28E9:  3A 0A 40     ld a,(obj_angle)             ; obj_angle
28EC:  E6 3F        and $3F                     
28EE:  4F           ld c,a                      
28EF:  94           sub h                       
28F0:  28 10        jr z,WALL_TRACK_STEER_2902  
28F2:  38 06        jr c,WALL_TRACK_STEER_28FA  
28F4:  FE 20        cp $20                      
28F6:  30 09        jr nc,WALL_TRACK_STEER_2901 
28F8:  18 04        jr WALL_TRACK_STEER_28FE    

WALL_TRACK_STEER_28FA:
28FA:  FE E0        cp $E0                      
28FC:  30 03        jr nc,WALL_TRACK_STEER_2901 

WALL_TRACK_STEER_28FE:
28FE:  0D           dec c                       
28FF:  18 01        jr WALL_TRACK_STEER_2902    

WALL_TRACK_STEER_2901:
2901:  0C           inc c                       

WALL_TRACK_STEER_2902:
2902:  FD 71 8A     ld (iy-118),c               
2905:  C9           ret                         

RANDOM_NEXT:
2906:  FD 86 F3     add a,(iy-13)               
2909:  32 73 40     ld (random_val),a            ; random_val
290C:  DB 09        in a,($09)                   ; WATCHDOG
290E:  ED 5F        ld a,r                      
2910:  FD 86 F3     add a,(iy-13)               
2913:  07           rlca                        
2914:  07           rlca                        
2915:  07           rlca                        
2916:  FD 96 97     sub (iy-105)                
2919:  32 73 40     ld (random_val),a            ; random_val
291C:  C9           ret                         

OBJ_SPAWN:
291D:  F5           push af                     
291E:  C5           push bc                     
291F:  D5           push de                     
2920:  E5           push hl                     
2921:  DD E5        push ix                     
2923:  C5           push bc                     
2924:  CB 20        sla b                       
2926:  78           ld a,b                      
2927:  80           add a,b                     
2928:  80           add a,b                     
2929:  4F           ld c,a                      
292A:  06 00        ld b,$00                    
292C:  DD 21 8A 2E  ld ix,$2E8A                 
2930:  DD 09        add ix,bc                   
2932:  DD 5E 00     ld e,(ix+0)                 
2935:  DD 56 01     ld d,(ix+1)                 
2938:  D5           push de                     
2939:  DD 6E 02     ld l,(ix+2)                 
293C:  DD 66 03     ld h,(ix+3)                 
293F:  01 17 00     ld bc,$0017                 
2942:  ED B0        ldir                        
2944:  DD E1        pop ix                      
2946:  C1           pop bc                      
2947:  DD 70 13     ld (ix+19),b                
294A:  78           ld a,b                      
294B:  FE 0E        cp $0E                      
294D:  30 5C        jr nc,OBJ_SPAWN_29AB        
294F:  05           dec b                       
2950:  20 1C        jr nz,OBJ_SPAWN_296E        
2952:  CD 06 29     call RANDOM_NEXT             ; -> RANDOM_NEXT
2955:  E6 0F        and $0F                     
2957:  F6 B0        or $B0                      
2959:  DD 77 09     ld (ix+9),a                 
295C:  FD CB F9 7E  bit 7,(iy-7)                
2960:  CA AB 29     jp z,OBJ_SPAWN_29AB         
2963:  DD 7E 07     ld a,(ix+7)                 
2966:  ED 44        neg                         
2968:  DD 77 07     ld (ix+7),a                 
296B:  C3 AB 29     jp OBJ_SPAWN_29AB           

OBJ_SPAWN_296E:
296E:  3E 1E        ld a,$1E                    
2970:  80           add a,b                     
2971:  DD 77 12     ld (ix+18),a                
2974:  11 C0 0D     ld de,$0DC0                 
2977:  21 00 19     ld hl,$1900                 
297A:  FD CB F9 7E  bit 7,(iy-7)                
297E:  28 12        jr z,OBJ_SPAWN_2992         
2980:  DD CB 00 CE  set 1,(ix+0)                
2984:  11 40 F2     ld de,$F240                 
2987:  21 00 E7     ld hl,$E700                 
298A:  DD 7E 0A     ld a,(ix+10)                
298D:  C6 20        add a,$20                   
298F:  DD 77 0A     ld (ix+10),a                

OBJ_SPAWN_2992:
2992:  19           add hl,de                   
2993:  10 FD        djnz OBJ_SPAWN_2992         
2995:  DD 75 06     ld (ix+6),l                 
2998:  DD 74 07     ld (ix+7),h                 

OBJ_SPAWN_299B:
299B:  CD 06 29     call RANDOM_NEXT             ; -> RANDOM_NEXT
299E:  CB BF        res 7,a                     
29A0:  FE 0C        cp $0C                      
29A2:  38 F7        jr c,OBJ_SPAWN_299B         
29A4:  FE 5A        cp $5A                      
29A6:  30 F3        jr nc,OBJ_SPAWN_299B        
29A8:  DD 77 09     ld (ix+9),a                 

OBJ_SPAWN_29AB:
29AB:  DD E1        pop ix                      
29AD:  E1           pop hl                      
29AE:  D1           pop de                      
29AF:  C1           pop bc                      
29B0:  F1           pop af                      
29B1:  C9           ret                         

OBJ_KILL:
29B2:  F5           push af                     
29B3:  C5           push bc                     
29B4:  D5           push de                     
29B5:  E5           push hl                     
29B6:  CB 20        sla b                       
29B8:  78           ld a,b                      
29B9:  80           add a,b                     
29BA:  80           add a,b                     
29BB:  4F           ld c,a                      
29BC:  06 00        ld b,$00                    
29BE:  21 8A 2E     ld hl,$2E8A                 
29C1:  09           add hl,bc                   
29C2:  5E           ld e,(hl)                   
29C3:  23           inc hl                      
29C4:  56           ld d,(hl)                   
29C5:  EB           ex de,hl                    
29C6:  CB BE        res 7,(hl)                  
29C8:  E1           pop hl                      
29C9:  D1           pop de                      
29CA:  C1           pop bc                      
29CB:  F1           pop af                      
29CC:  C9           ret                         

HISCORE_CHECK:
29CD:  F5           push af                     
29CE:  C5           push bc                     
29CF:  D5           push de                     
29D0:  E5           push hl                     
29D1:  06 06        ld b,$06                    
29D3:  21 A9 43     ld hl,hiscore_work           ; hiscore_work

HISCORE_CHECK_29D6:
29D6:  11 36 40     ld de,score_cur              ; score_cur
29D9:  0E 04        ld c,$04                    
29DB:  AF           xor a                       

HISCORE_CHECK_29DC:
29DC:  1A           ld a,(de)                   
29DD:  13           inc de                      
29DE:  9E           sbc a,(hl)                  
29DF:  27           daa                         
29E0:  23           inc hl                      
29E1:  0D           dec c                       
29E2:  20 F8        jr nz,HISCORE_CHECK_29DC    
29E4:  23           inc hl                      
29E5:  23           inc hl                      
29E6:  23           inc hl                      
29E7:  30 08        jr nc,HISCORE_INSERT         ; -> HISCORE_INSERT
29E9:  10 EB        djnz HISCORE_CHECK_29D6     
29EB:  FD 36 A4 01  ld (iy-92),$01              
29EF:  18 3F        jr HISCORE_INSERT_2A30      

HISCORE_INSERT:
29F1:  11 F9 FF     ld de,$FFF9                 
29F4:  19           add hl,de                   
29F5:  E5           push hl                     
29F6:  11 07 00     ld de,$0007                 
29F9:  21 00 00     ld hl,$0000                  ; RESET
29FC:  78           ld a,b                      

HISCORE_INSERT_29FD:
29FD:  05           dec b                       
29FE:  28 03        jr z,HISCORE_INSERT_2A03    
2A00:  19           add hl,de                   
2A01:  18 FA        jr HISCORE_INSERT_29FD      

HISCORE_INSERT_2A03:
2A03:  44           ld b,h                      
2A04:  4D           ld c,l                      
2A05:  11 D2 43     ld de,$43D2                 
2A08:  21 CB 43     ld hl,$43CB                 
2A0B:  3D           dec a                       
2A0C:  28 02        jr z,HISCORE_INSERT_2A10    
2A0E:  ED B8        lddr                        

HISCORE_INSERT_2A10:
2A10:  D1           pop de                      
2A11:  01 04 00     ld bc,$0004                 
2A14:  21 36 40     ld hl,score_cur              ; score_cur
2A17:  ED B0        ldir                        
2A19:  ED 53 96 40  ld (initials_cur),de         ; initials_cur
2A1D:  ED 53 94 40  ld (initials_ptr),de         ; initials_ptr
2A21:  FD 36 18 00  ld (iy+24),$00              
2A25:  3E 20        ld a,$20                    
2A27:  12           ld (de),a                   
2A28:  13           inc de                      
2A29:  12           ld (de),a                   
2A2A:  13           inc de                      
2A2B:  12           ld (de),a                   
2A2C:  FD 36 A4 04  ld (iy-92),$04              

HISCORE_INSERT_2A30:
2A30:  E1           pop hl                      
2A31:  D1           pop de                      
2A32:  C1           pop bc                      
2A33:  F1           pop af                      
2A34:  C9           ret                         

HISCORE_INITIALS:
2A35:  F5           push af                     
2A36:  C5           push bc                     
2A37:  D5           push de                     
2A38:  E5           push hl                     
2A39:  3A BC 40     ld a,(ship_angle)            ; ship_angle
2A3C:  E6 3E        and $3E                     
2A3E:  0F           rrca                        
2A3F:  32 99 40     ld (letter_sel),a            ; letter_sel
2A42:  57           ld d,a                      
2A43:  1E 20        ld e,$20                    
2A45:  CD AA 25     call MUL8X8                  ; -> MUL8X8
2A48:  11 20 00     ld de,$0020                 
2A4B:  19           add hl,de                   
2A4C:  7C           ld a,h                      
2A4D:  32 92 44     ld ($4492),a                
2A50:  7D           ld a,l                      
2A51:  32 91 44     ld ($4491),a                
2A54:  2A 96 40     ld hl,(initials_cur)         ; initials_cur
2A57:  CD AA 11     call START_BTN_CHECK         ; -> START_BTN_CHECK
2A5A:  20 32        jr nz,hs_finalize_filter     ; -> hs_finalize_filter
2A5C:  3A 20 40     ld a,(seconds_ctr)           ; seconds_ctr
2A5F:  FE 14        cp $14                      
2A61:  30 2B        jr nc,hs_finalize_filter     ; -> hs_finalize_filter
2A63:  FD CB 32 56  bit 2,(iy+50)               
2A67:  CA EE 2A     jp z,hs_redraw_initials_2AEE
2A6A:  FD CB 32 96  res 2,(iy+50)               
2A6E:  3A 99 40     ld a,(letter_sel)            ; letter_sel
2A71:  FE 1A        cp $1A                      
2A73:  38 43        jr c,hs_letter_commit        ; -> hs_letter_commit
2A75:  FE 1D        cp $1D                      
2A77:  28 15        jr z,hs_finalize_filter      ; -> hs_finalize_filter
2A79:  FE 1B        cp $1B                      
2A7B:  20 71        jr nz,hs_redraw_initials_2AEE
2A7D:  3A 98 40     ld a,(initials_cnt)          ; initials_cnt
2A80:  B7           or a                        
2A81:  28 6B        jr z,hs_redraw_initials_2AEE
2A83:  FD 35 18     dec (iy+24)                 
2A86:  2B           dec hl                      
2A87:  22 96 40     ld (initials_cur),hl         ; initials_cur
2A8A:  36 20        ld (hl),$20                 
2A8C:  18 3D        jr hs_redraw_initials        ; -> hs_redraw_initials

hs_finalize_filter:
2A8E:  FD 36 A4 01  ld (iy-92),$01              
2A92:  2A 94 40     ld hl,(initials_ptr)         ; initials_ptr
2A95:  7E           ld a,(hl)                   
2A96:  FE 46        cp $46                      
2A98:  28 04        jr z,hs_finalize_filter_2A9E
2A9A:  FE 53        cp $53                      
2A9C:  20 18        jr nz,hs_finalize_filter_2AB6

hs_finalize_filter_2A9E:
2A9E:  23           inc hl                      
2A9F:  7E           ld a,(hl)                   
2AA0:  FE 55        cp $55                      
2AA2:  20 12        jr nz,hs_finalize_filter_2AB6
2AA4:  23           inc hl                      
2AA5:  7E           ld a,(hl)                   
2AA6:  FE 43        cp $43                      
2AA8:  28 04        jr z,hs_finalize_filter_2AAE
2AAA:  FE 4B        cp $4B                      
2AAC:  20 08        jr nz,hs_finalize_filter_2AB6

hs_finalize_filter_2AAE:
2AAE:  36 20        ld (hl),$20                 
2AB0:  2B           dec hl                      
2AB1:  36 20        ld (hl),$20                 
2AB3:  2B           dec hl                      
2AB4:  36 20        ld (hl),$20                 

hs_finalize_filter_2AB6:
2AB6:  18 36        jr hs_redraw_initials_2AEE  

hs_letter_commit:
2AB8:  47           ld b,a                      
2AB9:  3A 98 40     ld a,(initials_cnt)          ; initials_cnt
2ABC:  FE 03        cp $03                      
2ABE:  30 2E        jr nc,hs_redraw_initials_2AEE
2AC0:  FD 34 18     inc (iy+24)                 
2AC3:  78           ld a,b                      
2AC4:  C6 41        add a,$41                   
2AC6:  77           ld (hl),a                   
2AC7:  23           inc hl                      
2AC8:  22 96 40     ld (initials_cur),hl         ; initials_cur

hs_redraw_initials:
2ACB:  FD 36 A0 00  ld (iy-96),$00              
2ACF:  FD 36 F0 14  ld (iy-16),$14              
2AD3:  ED 5B 94 40  ld de,(initials_ptr)         ; initials_ptr
2AD7:  21 89 44     ld hl,$4489                 
2ADA:  CD 36 2C     call CHAR_GLYPH_EMIT         ; -> CHAR_GLYPH_EMIT
2ADD:  CD 36 2C     call CHAR_GLYPH_EMIT         ; -> CHAR_GLYPH_EMIT
2AE0:  CD 36 2C     call CHAR_GLYPH_EMIT         ; -> CHAR_GLYPH_EMIT
2AE3:  3A 98 40     ld a,(initials_cnt)          ; initials_cnt
2AE6:  FE 03        cp $03                      
2AE8:  20 04        jr nz,hs_redraw_initials_2AEE
2AEA:  FD 36 A0 10  ld (iy-96),$10              

hs_redraw_initials_2AEE:
2AEE:  E1           pop hl                      
2AEF:  D1           pop de                      
2AF0:  C1           pop bc                      
2AF1:  F1           pop af                      
2AF2:  C9           ret                         

DRAW_PAGE:
2AF3:  F5           push af                     
2AF4:  C5           push bc                     
2AF5:  D5           push de                     
2AF6:  E5           push hl                     
2AF7:  DD E5        push ix                     
2AF9:  CD 54 18     call VG_WAIT_DONE            ; -> VG_WAIT_DONE
2AFC:  FD 36 09 01  ld (iy+9),$01               
2B00:  FD 36 0A 00  ld (iy+10),$00              
2B04:  FD 36 0B 00  ld (iy+11),$00              
2B08:  FD 4E 04     ld c,(iy+4)                 
2B0B:  0D           dec c                       
2B0C:  CB 21        sla c                       
2B0E:  06 00        ld b,$00                    
2B10:  21 15 3F     ld hl,$3F15                 
2B13:  09           add hl,bc                   
2B14:  5E           ld e,(hl)                   
2B15:  23           inc hl                      
2B16:  56           ld d,(hl)                   
2B17:  ED 53 85 40  ld (page_ptr),de             ; page_ptr
2B1B:  13           inc de                      
2B1C:  EB           ex de,hl                    
2B1D:  5E           ld e,(hl)                   
2B1E:  23           inc hl                      
2B1F:  56           ld d,(hl)                   
2B20:  23           inc hl                      
2B21:  E5           push hl                     
2B22:  EB           ex de,hl                    
2B23:  22 87 40     ld (page_rec_next),hl        ; page_rec_next
2B26:  B7           or a                        
2B27:  ED 52        sbc hl,de                   
2B29:  01 0A 00     ld bc,$000A                 
2B2C:  ED 42        sbc hl,bc                   
2B2E:  FD 75 0C     ld (iy+12),l                
2B31:  01 08 00     ld bc,$0008                 
2B34:  E1           pop hl                      
2B35:  5E           ld e,(hl)                   
2B36:  23           inc hl                      
2B37:  56           ld d,(hl)                   
2B38:  23           inc hl                      
2B39:  ED B0        ldir                        
2B3B:  22 8D 40     ld (page_script_ptr),hl      ; page_script_ptr
2B3E:  ED 53 8F 40  ld (vlist_cursor),de         ; vlist_cursor
2B42:  2A 87 40     ld hl,(page_rec_next)        ; page_rec_next

DRAW_PAGE_2B45:
2B45:  FD 34 0B     inc (iy+11)                 
2B48:  5E           ld e,(hl)                   
2B49:  23           inc hl                      
2B4A:  56           ld d,(hl)                   
2B4B:  EB           ex de,hl                    
2B4C:  7C           ld a,h                      
2B4D:  B5           or l                        
2B4E:  20 F5        jr nz,DRAW_PAGE_2B45        
2B50:  2A 85 40     ld hl,(page_ptr)             ; page_ptr
2B53:  CB 7E        bit 7,(hl)                  
2B55:  20 0F        jr nz,DRAW_PAGE_2B66        

DRAW_PAGE_2B57:
2B57:  FD 36 F0 00  ld (iy-16),$00              
2B5B:  CD 6D 2B     call PAGE_SCRIPT_STEP        ; -> PAGE_SCRIPT_STEP
2B5E:  3A 8B 40     ld a,(page_line_total)       ; page_line_total
2B61:  FD BE 09     cp (iy+9)                   
2B64:  30 F1        jr nc,DRAW_PAGE_2B57        

DRAW_PAGE_2B66:
2B66:  DD E1        pop ix                      
2B68:  E1           pop hl                      
2B69:  D1           pop de                      
2B6A:  C1           pop bc                      
2B6B:  F1           pop af                      
2B6C:  C9           ret                         

PAGE_SCRIPT_STEP:
2B6D:  F5           push af                     
2B6E:  C5           push bc                     
2B6F:  D5           push de                     
2B70:  E5           push hl                     
2B71:  DB 09        in a,($09)                   ; WATCHDOG
2B73:  3A 70 40     ld a,(fire_cooldown)         ; fire_cooldown
2B76:  B7           or a                        
2B77:  C2 0F 2C     jp nz,PAGE_SCRIPT_STEP_2C0F 
2B7A:  FD 36 F0 0A  ld (iy-16),$0A              
2B7E:  FD 34 0A     inc (iy+10)                 
2B81:  ED 5B 8D 40  ld de,(page_script_ptr)      ; page_script_ptr
2B85:  2A 8F 40     ld hl,(vlist_cursor)         ; vlist_cursor
2B88:  CB 7C        bit 7,h                     
2B8A:  C4 54 18     call nz,VG_WAIT_DONE         ; -> VG_WAIT_DONE
2B8D:  1A           ld a,(de)                   
2B8E:  CB 7F        bit 7,a                     
2B90:  28 2F        jr z,PAGE_SCRIPT_STEP_2BC1  
2B92:  FD 34 0A     inc (iy+10)                 
2B95:  FD 34 0A     inc (iy+10)                 
2B98:  13           inc de                      
2B99:  CB 77        bit 6,a                     
2B9B:  20 11        jr nz,PAGE_SCRIPT_STEP_2BAE 
2B9D:  E6 3F        and $3F                     
2B9F:  47           ld b,a                      
2BA0:  1A           ld a,(de)                   
2BA1:  13           inc de                      
2BA2:  4F           ld c,a                      
2BA3:  1A           ld a,(de)                   
2BA4:  13           inc de                      
2BA5:  D5           push de                     
2BA6:  57           ld d,a                      
2BA7:  59           ld e,c                      
2BA8:  CD 0E 24     call BCD_TO_DIGITS           ; -> BCD_TO_DIGITS
2BAB:  D1           pop de                      
2BAC:  18 16        jr PAGE_SCRIPT_STEP_2BC4    

PAGE_SCRIPT_STEP_2BAE:
2BAE:  E6 3F        and $3F                     
2BB0:  47           ld b,a                      
2BB1:  1A           ld a,(de)                   
2BB2:  13           inc de                      
2BB3:  4F           ld c,a                      
2BB4:  1A           ld a,(de)                   
2BB5:  13           inc de                      
2BB6:  D5           push de                     
2BB7:  57           ld d,a                      
2BB8:  59           ld e,c                      

PAGE_SCRIPT_STEP_2BB9:
2BB9:  CD 36 2C     call CHAR_GLYPH_EMIT         ; -> CHAR_GLYPH_EMIT
2BBC:  10 FB        djnz PAGE_SCRIPT_STEP_2BB9  
2BBE:  D1           pop de                      
2BBF:  18 03        jr PAGE_SCRIPT_STEP_2BC4    

PAGE_SCRIPT_STEP_2BC1:
2BC1:  CD 36 2C     call CHAR_GLYPH_EMIT         ; -> CHAR_GLYPH_EMIT

PAGE_SCRIPT_STEP_2BC4:
2BC4:  ED 53 8D 40  ld (page_script_ptr),de      ; page_script_ptr
2BC8:  22 8F 40     ld (vlist_cursor),hl         ; vlist_cursor
2BCB:  3A 8A 40     ld a,(page_line_glyph)       ; page_line_glyph
2BCE:  FD BE 0C     cp (iy+12)                  
2BD1:  38 3C        jr c,PAGE_SCRIPT_STEP_2C0F  
2BD3:  FD 36 F0 19  ld (iy-16),$19              
2BD7:  FD 34 09     inc (iy+9)                  
2BDA:  3A 8B 40     ld a,(page_line_total)       ; page_line_total
2BDD:  FD BE 09     cp (iy+9)                   
2BE0:  38 2D        jr c,PAGE_SCRIPT_STEP_2C0F  
2BE2:  2A 87 40     ld hl,(page_rec_next)        ; page_rec_next
2BE5:  5E           ld e,(hl)                   
2BE6:  23           inc hl                      
2BE7:  56           ld d,(hl)                   
2BE8:  23           inc hl                      
2BE9:  ED 53 87 40  ld (page_rec_next),de        ; page_rec_next
2BED:  E5           push hl                     
2BEE:  EB           ex de,hl                    
2BEF:  B7           or a                        
2BF0:  ED 52        sbc hl,de                   
2BF2:  01 0A 00     ld bc,$000A                 
2BF5:  ED 42        sbc hl,bc                   
2BF7:  FD 75 0C     ld (iy+12),l                
2BFA:  E1           pop hl                      
2BFB:  5E           ld e,(hl)                   
2BFC:  23           inc hl                      
2BFD:  56           ld d,(hl)                   
2BFE:  23           inc hl                      
2BFF:  01 08 00     ld bc,$0008                 
2C02:  ED B0        ldir                        
2C04:  22 8D 40     ld (page_script_ptr),hl      ; page_script_ptr
2C07:  ED 53 8F 40  ld (vlist_cursor),de         ; vlist_cursor
2C0B:  FD 36 0A 00  ld (iy+10),$00              

PAGE_SCRIPT_STEP_2C0F:
2C0F:  E1           pop hl                      
2C10:  D1           pop de                      
2C11:  C1           pop bc                      
2C12:  F1           pop af                      
2C13:  C9           ret                         

HISCORE_ROW_FMT:
2C14:  D5           push de                     
2C15:  06 04        ld b,$04                    
2C17:  CD 0E 24     call BCD_TO_DIGITS           ; -> BCD_TO_DIGITS
2C1A:  11 67 3F     ld de,$3F67                 
2C1D:  CD 36 2C     call CHAR_GLYPH_EMIT         ; -> CHAR_GLYPH_EMIT
2C20:  CD 36 2C     call CHAR_GLYPH_EMIT         ; -> CHAR_GLYPH_EMIT
2C23:  D1           pop de                      
2C24:  D5           push de                     
2C25:  13           inc de                      
2C26:  CD 36 2C     call CHAR_GLYPH_EMIT         ; -> CHAR_GLYPH_EMIT
2C29:  CD 36 2C     call CHAR_GLYPH_EMIT         ; -> CHAR_GLYPH_EMIT
2C2C:  CD 36 2C     call CHAR_GLYPH_EMIT         ; -> CHAR_GLYPH_EMIT
2C2F:  D1           pop de                      
2C30:  21 07 00     ld hl,$0007                 
2C33:  19           add hl,de                   
2C34:  EB           ex de,hl                    
2C35:  C9           ret                         

CHAR_GLYPH_EMIT:
2C36:  F5           push af                     
2C37:  C5           push bc                     
2C38:  D5           push de                     
2C39:  E5           push hl                     
2C3A:  DB 09        in a,($09)                   ; WATCHDOG
2C3C:  1A           ld a,(de)                   
2C3D:  FE 41        cp $41                      
2C3F:  38 0D        jr c,CHAR_GLYPH_EMIT_2C4E   
2C41:  FE 5B        cp $5B                      
2C43:  38 02        jr c,CHAR_GLYPH_EMIT_2C47   
2C45:  3E 5A        ld a,$5A                    

CHAR_GLYPH_EMIT_2C47:
2C47:  D6 41        sub $41                     
2C49:  21 2E 3A     ld hl,$3A2E                 
2C4C:  18 2C        jr CHAR_GLYPH_EMIT_2C7A     

CHAR_GLYPH_EMIT_2C4E:
2C4E:  FE 20        cp $20                      
2C50:  20 07        jr nz,CHAR_GLYPH_EMIT_2C59  
2C52:  3E 0B        ld a,$0B                    
2C54:  21 D3 3F     ld hl,$3FD3                 
2C57:  18 21        jr CHAR_GLYPH_EMIT_2C7A     

CHAR_GLYPH_EMIT_2C59:
2C59:  FE 2E        cp $2E                      
2C5B:  20 07        jr nz,CHAR_GLYPH_EMIT_2C64  
2C5D:  3E 00        ld a,$00                    
2C5F:  21 CF 3F     ld hl,$3FCF                 
2C62:  18 16        jr CHAR_GLYPH_EMIT_2C7A     

CHAR_GLYPH_EMIT_2C64:
2C64:  FE 2C        cp $2C                      
2C66:  20 07        jr nz,CHAR_GLYPH_EMIT_2C6F  
2C68:  3E 01        ld a,$01                    
2C6A:  21 CF 3F     ld hl,$3FCF                 
2C6D:  18 0B        jr CHAR_GLYPH_EMIT_2C7A     

CHAR_GLYPH_EMIT_2C6F:
2C6F:  FE 2F        cp $2F                      
2C71:  30 02        jr nc,CHAR_GLYPH_EMIT_2C75  
2C73:  3E 30        ld a,$30                    

CHAR_GLYPH_EMIT_2C75:
2C75:  D6 2F        sub $2F                     
2C77:  21 D3 3F     ld hl,$3FD3                 

CHAR_GLYPH_EMIT_2C7A:
2C7A:  CB 27        sla a                       
2C7C:  5F           ld e,a                      
2C7D:  16 00        ld d,$00                    
2C7F:  19           add hl,de                   
2C80:  EB           ex de,hl                    
2C81:  E1           pop hl                      
2C82:  1A           ld a,(de)                   
2C83:  13           inc de                      
2C84:  77           ld (hl),a                   
2C85:  23           inc hl                      
2C86:  1A           ld a,(de)                   
2C87:  77           ld (hl),a                   
2C88:  23           inc hl                      
2C89:  D1           pop de                      
2C8A:  13           inc de                      
2C8B:  C1           pop bc                      
2C8C:  F1           pop af                      
2C8D:  C9           ret                         

PLAYER_STATE_SAVE:
2C8E:  F5           push af                     
2C8F:  C5           push bc                     
2C90:  D5           push de                     
2C91:  3A 22 40     ld a,(wave_pace_timer)       ; wave_pace_timer
2C94:  77           ld (hl),a                   
2C95:  23           inc hl                      
2C96:  3A 66 40     ld a,(lives)                 ; lives
2C99:  77           ld (hl),a                   
2C9A:  23           inc hl                      
2C9B:  3A 67 40     ld a,(wave_num)              ; wave_num
2C9E:  77           ld (hl),a                   
2C9F:  23           inc hl                      
2CA0:  3A 6A 40     ld a,(xlife_flags)           ; xlife_flags
2CA3:  77           ld (hl),a                   
2CA4:  23           inc hl                      
2CA5:  3A 6C 40     ld a,(wave_ctr)              ; wave_ctr
2CA8:  77           ld (hl),a                   
2CA9:  23           inc hl                      
2CAA:  FD CB C1 7E  bit 7,(iy-63)               
2CAE:  28 09        jr z,PLAYER_STATE_SAVE_2CB9 
2CB0:  EB           ex de,hl                    
2CB1:  01 08 00     ld bc,$0008                 
2CB4:  21 36 40     ld hl,score_cur              ; score_cur
2CB7:  ED B0        ldir                        

PLAYER_STATE_SAVE_2CB9:
2CB9:  D1           pop de                      
2CBA:  C1           pop bc                      
2CBB:  F1           pop af                      
2CBC:  C9           ret                         

PLAYER_STATE_LOAD:
2CBD:  F5           push af                     
2CBE:  C5           push bc                     
2CBF:  D5           push de                     
2CC0:  7E           ld a,(hl)                   
2CC1:  23           inc hl                      
2CC2:  32 22 40     ld (wave_pace_timer),a       ; wave_pace_timer
2CC5:  7E           ld a,(hl)                   
2CC6:  23           inc hl                      
2CC7:  32 66 40     ld (lives),a                 ; lives
2CCA:  7E           ld a,(hl)                   
2CCB:  23           inc hl                      
2CCC:  32 67 40     ld (wave_num),a              ; wave_num
2CCF:  7E           ld a,(hl)                   
2CD0:  23           inc hl                      
2CD1:  32 6A 40     ld (xlife_flags),a           ; xlife_flags
2CD4:  7E           ld a,(hl)                   
2CD5:  23           inc hl                      
2CD6:  32 6C 40     ld (wave_ctr),a              ; wave_ctr
2CD9:  01 08 00     ld bc,$0008                 
2CDC:  11 36 40     ld de,score_cur              ; score_cur
2CDF:  ED B0        ldir                        
2CE1:  01 04 00     ld bc,$0004                 
2CE4:  11 42 40     ld de,$4042                 
2CE7:  21 4B 40     ld hl,$404B                 
2CEA:  FD CB BE 46  bit 0,(iy-66)               
2CEE:  28 03        jr z,PLAYER_STATE_LOAD_2CF3 
2CF0:  21 58 40     ld hl,$4058                 

PLAYER_STATE_LOAD_2CF3:
2CF3:  ED B0        ldir                        
2CF5:  D1           pop de                      
2CF6:  C1           pop bc                      
2CF7:  F1           pop af                      
2CF8:  C9           ret                         

LIVES_ICON_DRAW:
2CF9:  3A 66 40     ld a,(lives)                 ; lives
2CFC:  21 00 F0     ld hl,$F000                 
2CFF:  3D           dec a                       
2D00:  28 09        jr z,LIVES_ICON_DRAW_2D0B   
2D02:  FD CB EA 5E  bit 3,(iy-22)               
2D06:  28 03        jr z,LIVES_ICON_DRAW_2D0B   
2D08:  3D           dec a                       
2D09:  18 03        jr LIVES_ICON_DRAW_2D0E     

LIVES_ICON_DRAW_2D0B:
2D0B:  22 BA 82     ld ($82BA),hl               

LIVES_ICON_DRAW_2D0E:
2D0E:  B7           or a                        
2D0F:  28 09        jr z,LIVES_ICON_DRAW_2D1A   
2D11:  FD CB EA 66  bit 4,(iy-22)               
2D15:  28 03        jr z,LIVES_ICON_DRAW_2D1A   
2D17:  3D           dec a                       
2D18:  18 03        jr LIVES_ICON_DRAW_2D1D     

LIVES_ICON_DRAW_2D1A:
2D1A:  22 B0 82     ld ($82B0),hl               

LIVES_ICON_DRAW_2D1D:
2D1D:  B7           or a                        
2D1E:  28 09        jr z,LIVES_ICON_DRAW_2D29   
2D20:  FD CB EA 6E  bit 5,(iy-22)               
2D24:  28 03        jr z,LIVES_ICON_DRAW_2D29   
2D26:  3D           dec a                       
2D27:  18 03        jr LIVES_ICON_DRAW_2D2C     

LIVES_ICON_DRAW_2D29:
2D29:  22 A6 82     ld ($82A6),hl               

LIVES_ICON_DRAW_2D2C:
2D2C:  B7           or a                        
2D2D:  28 11        jr z,LIVES_ICON_DRAW_2D40   
2D2F:  3D           dec a                       
2D30:  28 11        jr z,LIVES_ICON_DRAW_2D43   
2D32:  3D           dec a                       
2D33:  28 11        jr z,LIVES_ICON_DRAW_2D46   
2D35:  3D           dec a                       
2D36:  28 11        jr z,LIVES_ICON_DRAW_2D49   
2D38:  3D           dec a                       
2D39:  28 11        jr z,LIVES_ICON_DRAW_2D4C   
2D3B:  3D           dec a                       
2D3C:  28 11        jr z,LIVES_ICON_DRAW_2D4F   
2D3E:  18 12        jr LIVES_ICON_DRAW_2D52     

LIVES_ICON_DRAW_2D40:
2D40:  22 9C 82     ld ($829C),hl               

LIVES_ICON_DRAW_2D43:
2D43:  22 92 82     ld ($8292),hl               

LIVES_ICON_DRAW_2D46:
2D46:  22 88 82     ld ($8288),hl               

LIVES_ICON_DRAW_2D49:
2D49:  22 7E 82     ld ($827E),hl               

LIVES_ICON_DRAW_2D4C:
2D4C:  22 74 82     ld ($8274),hl               

LIVES_ICON_DRAW_2D4F:
2D4F:  22 6A 82     ld ($826A),hl               

LIVES_ICON_DRAW_2D52:
2D52:  22 60 82     ld ($8260),hl               
2D55:  C9           ret                         

VWALL_XFORM_A:
2D56:  DB 09        in a,($09)                   ; WATCHDOG
2D58:  DD 7E 01     ld a,(ix+1)                 
2D5B:  FE F0        cp $F0                      
2D5D:  30 18        jr nc,VWALL_XFORM_A_2D77    
2D5F:  FE B0        cp $B0                      
2D61:  30 1C        jr nc,VWALL_XFORM_A_2D7F    
2D63:  FE A0        cp $A0                      
2D65:  30 08        jr nc,VWALL_XFORM_A_2D6F    
2D67:  DD 7E 03     ld a,(ix+3)                 
2D6A:  EE 04        xor $04                     
2D6C:  DD 77 03     ld (ix+3),a                 

VWALL_XFORM_A_2D6F:
2D6F:  0B           dec bc                      
2D70:  DD 23        inc ix                      
2D72:  0B           dec bc                      
2D73:  DD 23        inc ix                      
2D75:  18 08        jr VWALL_XFORM_A_2D7F       

VWALL_XFORM_A_2D77:
2D77:  DD 7E 00     ld a,(ix+0)                 
2D7A:  EE 04        xor $04                     
2D7C:  DD 77 00     ld (ix+0),a                 

VWALL_XFORM_A_2D7F:
2D7F:  DD 23        inc ix                      
2D81:  0B           dec bc                      
2D82:  DD 23        inc ix                      
2D84:  0B           dec bc                      
2D85:  78           ld a,b                      
2D86:  B1           or c                        
2D87:  20 CD        jr nz,VWALL_XFORM_A          ; -> VWALL_XFORM_A
2D89:  C9           ret                         

VWALL_XFORM_B:
2D8A:  DB 09        in a,($09)                   ; WATCHDOG
2D8C:  DD 7E 01     ld a,(ix+1)                 
2D8F:  FE F0        cp $F0                      
2D91:  30 1D        jr nc,VWALL_XFORM_B_2DB0    
2D93:  FE B0        cp $B0                      
2D95:  30 26        jr nc,VWALL_XFORM_B_2DBD    
2D97:  FE A0        cp $A0                      
2D99:  30 0D        jr nc,VWALL_XFORM_B_2DA8    
2D9B:  EE 04        xor $04                     
2D9D:  DD 77 01     ld (ix+1),a                 
2DA0:  DD 7E 03     ld a,(ix+3)                 
2DA3:  EE 04        xor $04                     
2DA5:  DD 77 03     ld (ix+3),a                 

VWALL_XFORM_B_2DA8:
2DA8:  DD 23        inc ix                      
2DAA:  0B           dec bc                      
2DAB:  DD 23        inc ix                      
2DAD:  0B           dec bc                      
2DAE:  18 0D        jr VWALL_XFORM_B_2DBD       

VWALL_XFORM_B_2DB0:
2DB0:  EE 04        xor $04                     
2DB2:  DD 77 01     ld (ix+1),a                 
2DB5:  DD 7E 00     ld a,(ix+0)                 
2DB8:  EE 04        xor $04                     
2DBA:  DD 77 00     ld (ix+0),a                 

VWALL_XFORM_B_2DBD:
2DBD:  DD 23        inc ix                      
2DBF:  0B           dec bc                      
2DC0:  DD 23        inc ix                      
2DC2:  0B           dec bc                      
2DC3:  78           ld a,b                      
2DC4:  B1           or c                        
2DC5:  20 C3        jr nz,VWALL_XFORM_B          ; -> VWALL_XFORM_B
2DC7:  C9           ret                         

VWALL_XFORM_C:
2DC8:  DB 09        in a,($09)                   ; WATCHDOG
2DCA:  DD 7E 01     ld a,(ix+1)                 
2DCD:  FE F0        cp $F0                      
2DCF:  30 15        jr nc,VWALL_XFORM_C_2DE6    
2DD1:  FE B0        cp $B0                      
2DD3:  30 16        jr nc,VWALL_XFORM_C_2DEB    
2DD5:  FE A0        cp $A0                      
2DD7:  30 05        jr nc,VWALL_XFORM_C_2DDE    
2DD9:  EE 04        xor $04                     
2DDB:  DD 77 01     ld (ix+1),a                 

VWALL_XFORM_C_2DDE:
2DDE:  DD 23        inc ix                      
2DE0:  0B           dec bc                      
2DE1:  DD 23        inc ix                      
2DE3:  0B           dec bc                      
2DE4:  18 05        jr VWALL_XFORM_C_2DEB       

VWALL_XFORM_C_2DE6:
2DE6:  EE 04        xor $04                     
2DE8:  DD 77 01     ld (ix+1),a                 

VWALL_XFORM_C_2DEB:
2DEB:  DD 23        inc ix                      
2DED:  0B           dec bc                      
2DEE:  DD 23        inc ix                      
2DF0:  0B           dec bc                      
2DF1:  78           ld a,b                      
2DF2:  B1           or c                        
2DF3:  20 D3        jr nz,VWALL_XFORM_C          ; -> VWALL_XFORM_C
2DF5:  C9           ret                         

HEARTBEAT_SOUND:
2DF6:  C6 0A        add a,$0A                   
2DF8:  FD BE E8     cp (iy-24)                  
2DFB:  28 06        jr z,HEARTBEAT_SOUND_2E03   
2DFD:  32 68 40     ld (last_sound_cmd),a        ; last_sound_cmd
2E00:  CD 04 2E     call SOUND_CMD_SEND          ; -> SOUND_CMD_SEND

HEARTBEAT_SOUND_2E03:
2E03:  C9           ret                         

SOUND_CMD_SEND:
2E04:  F5           push af                     
2E05:  FE 00        cp $00                      
2E07:  28 15        jr z,SOUND_CMD_SEND_2E1E    
2E09:  FD CB AE 7E  bit 7,(iy-82)               
2E0D:  28 3A        jr z,snd_do_send_2E49       
2E0F:  3A 21 40     ld a,(timer_seconds)         ; timer_seconds
2E12:  B7           or a                        
2E13:  20 19        jr nz,SOUND_CMD_SEND_2E2E   
2E15:  3A 24 40     ld a,(game_mode)             ; game_mode
2E18:  FE 03        cp $03                      
2E1A:  20 2D        jr nz,snd_do_send_2E49      
2E1C:  18 1D        jr snd_do_send               ; -> snd_do_send

SOUND_CMD_SEND_2E1E:
2E1E:  D3 14        out ($14),a                  ; SOUND CMD latch
2E20:  CD 5D 18     call WAIT_NEXT_TICK          ; -> WAIT_NEXT_TICK
2E23:  CD 5D 18     call WAIT_NEXT_TICK          ; -> WAIT_NEXT_TICK
2E26:  CD 5D 18     call WAIT_NEXT_TICK          ; -> WAIT_NEXT_TICK
2E29:  CD 5D 18     call WAIT_NEXT_TICK          ; -> WAIT_NEXT_TICK
2E2C:  18 1B        jr snd_do_send_2E49         

SOUND_CMD_SEND_2E2E:
2E2E:  3A 69 40     ld a,(sound_ready)           ; sound_ready
2E31:  B7           or a                        
2E32:  28 15        jr z,snd_do_send_2E49       
2E34:  F1           pop af                      
2E35:  F5           push af                     
2E36:  FD BE E8     cp (iy-24)                  
2E39:  28 0E        jr z,snd_do_send_2E49       

snd_do_send:
2E3B:  F1           pop af                      
2E3C:  F5           push af                     
2E3D:  D3 14        out ($14),a                  ; SOUND CMD latch
2E3F:  3E 00        ld a,$00                    

snd_do_send_2E41:
2E41:  00           nop                         
2E42:  00           nop                         
2E43:  00           nop                         
2E44:  3D           dec a                       
2E45:  20 FA        jr nz,snd_do_send_2E41      
2E47:  18 00        jr snd_do_send_2E49         

snd_do_send_2E49:
2E49:  F1           pop af                      
2E4A:  C9           ret                         

; ---- 2E4B-2E8F: scratch/padding + dummy entry ----
2E4B:  db  94 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ; ................
2E5B:  db  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ; ................
2E6B:  db  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ; ................
2E7B:  db  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ; ................
2E8B:  db  00 00 00 00 00                                  ; .....

; ---- 2E90-2F4F: object table: record/template/dlist entry (12-byte, in the shadow 0x4481+, mirrored to the 0x8000 object region per frame) ----
2E90:  OBJ  1: record=$40B2  template=$2F50  dlist_entry=$4481
2E96:  OBJ  2: record=$40C9  template=$2F67  dlist_entry=$448D
2E9C:  OBJ  3: record=$40E0  template=$2F67  dlist_entry=$4499
2EA2:  OBJ  4: record=$40F7  template=$2F67  dlist_entry=$44A5
2EA8:  OBJ  5: record=$410E  template=$2F67  dlist_entry=$44B1
2EAE:  OBJ  6: record=$4125  template=$2F67  dlist_entry=$44BD
2EB4:  OBJ  7: record=$413C  template=$2F67  dlist_entry=$44C9
2EBA:  OBJ  8: record=$4153  template=$2F67  dlist_entry=$44D5
2EC0:  OBJ  9: record=$416A  template=$2F67  dlist_entry=$44E1
2EC6:  OBJ 10: record=$4181  template=$2F67  dlist_entry=$44ED
2ECC:  OBJ 11: record=$4198  template=$2F67  dlist_entry=$44F9
2ED2:  OBJ 12: record=$41AF  template=$2F67  dlist_entry=$4505
2ED8:  OBJ 13: record=$41C6  template=$2F67  dlist_entry=$4511
2EDE:  OBJ 14: record=$41DD  template=$2F7E  dlist_entry=$451D
2EE4:  OBJ 15: record=$41F4  template=$2F7E  dlist_entry=$4529
2EEA:  OBJ 16: record=$420B  template=$2F7E  dlist_entry=$4535
2EF0:  OBJ 17: record=$4222  template=$2F7E  dlist_entry=$4541
2EF6:  OBJ 18: record=$4239  template=$2F7E  dlist_entry=$454D
2EFC:  OBJ 19: record=$4250  template=$2F7E  dlist_entry=$4559
2F02:  OBJ 20: record=$4267  template=$2F7E  dlist_entry=$4565
2F08:  OBJ 21: record=$427E  template=$2F7E  dlist_entry=$4571
2F0E:  OBJ 22: record=$4295  template=$2F95  dlist_entry=$457D
2F14:  OBJ 23: record=$42AC  template=$2F95  dlist_entry=$4589
2F1A:  OBJ 24: record=$42C3  template=$2F95  dlist_entry=$4595
2F20:  OBJ 25: record=$42DA  template=$2F95  dlist_entry=$45A1
2F26:  OBJ 26: record=$42F1  template=$2F95  dlist_entry=$45AD
2F2C:  OBJ 27: record=$4308  template=$2F95  dlist_entry=$45B9
2F32:  OBJ 28: record=$431F  template=$2F95  dlist_entry=$45C5
2F38:  OBJ 29: record=$4336  template=$2F95  dlist_entry=$45D1
2F3E:  OBJ 30: record=$434D  template=$2F95  dlist_entry=$45DD
2F44:  OBJ 31: record=$4364  template=$2F95  dlist_entry=$45E9
2F4A:  OBJ 32: record=$437B  template=$2F95  dlist_entry=$45F5

; ---- 2F50-2FC2: object templates: ship, droid, shot/mine, command + inert(2FAC) ----
2F50:  tmpl  F8 40 00 00 00 00 00 F5 80 ED 30 00 00 00 00 00 04 04 05 00 00 A1 30
2F67:  tmpl  F9 80 00 00 00 00 00 40 00 40 20 00 00 03 00 00 04 04 0C 00 00 04 CA
2F7E:  tmpl  C0 01 01 00 01 00 00 00 00 00 00 00 19 00 00 00 01 01 0C 00 00 F8 F8
2F95:  tmpl  80 02 00 00 00 00 00 00 00 00 00 00 00 01 00 00 03 03 FF 00 00 A7 CB
2FAC:  tmpl  80 20 00 00 00 00 00 00 00 00 03 00 04 00 00 00 01 01 19 00 00 BA CB

; ---- 2FC3-2FD2: coinage: coins-per-credit/credits ----
2FC3:  pairs (1,2) (1,3) (1,5) (4,5) (3,4) (2,3) (2,1) (1,1)

; ---- 2FD3-2FDA: lives per DIP (normal/alt) ----
2FD3:  pairs (2,4) (2,5) (3,6) (3,7)

; ---- 2FDB-2FE4: kill point values (BCD) ----
2FDB:  bcd  0350 0500 1000 1500 2500

; ---- 2FE5-2FEC: bonus-ship DIP thresholds ----
2FE5:  dw  0004 0005 0007 0010

; ---- 2FED-2FF6: extra-life thresholds 2/3 (sole reader: ATTRACT_INIT 0x0B29) ----
2FED:  dw  0015 0025 0050 0075 0150

; ---- 2FF7-3020: default high scores + initials ----
2FF7:  hiscore 00038250 "AE "
2FFE:  hiscore 00031350 "DLM"
3005:  hiscore 00030500 "MIC"
300C:  hiscore 00023350 "LRS"
3013:  hiscore 00022400 "JAC"
301A:  hiscore 00019550 "RDH"

; ---- 3021-3040: quarter-wave sin/cos (dx,dy) ----
3021:  pairs (0,32) (3,31) (6,31) (9,30) (12,29) (15,28) (18,26) (20,25)
3031:  pairs (22,22) (25,20) (26,18) (28,15) (29,12) (30,9) (31,6) (32,0)

; ---- 3041-3060: aim slope->angle buckets ----
3041:  dw  1400 0699 040A 02C3 0224 01AF 0155 011A
3051:  dw  00E7 00C0 0098 0077 005D 003F 0026 000C

; ---- 3061-30A0: spinner gray-code decode ----
3061:  db  00 01 03 02 07 06 04 05 0F 0E 0C 0D 08 09 0B 0A ; ................
3071:  db  1F 1E 1C 1D 18 19 1B 1A 10 11 13 12 17 16 14 15 ; ................
3081:  db  3F 3E 3C 3D 38 39 3B 3A 30 31 33 32 37 36 34 35 ; ?><=89;:01327645
3091:  db  20 21 23 22 27 26 24 25 2F 2E 2C 2D 28 29 2B 2A ;  !#"'&$%/.,-()+*

; ---- 30A1-30A8: SHIP_BASE: hull frame-set base per heading bits 4-5 (sets 2-3 live in vector RAM, built by 0x18BC) ----
30A1:  dw  0800 0DFC 05FC 03F8

; ---- 30A9-30C8: hull frame offsets[16]: shape = 0xC000|(base+ofs), flame = shape-7 ----
30A9:  dw  0007 0027 0048 0069 008B 00AB 00C9 00E9
30B9:  dw  0109 0128 0148 0168 0186 01A6 01C8 01E9

; ---- 30C9-30D8: NVRAM validation template ----
30C9:  db  0F 0D 0B 09 07 05 03 01 00 02 04 06 08 0A 0C 0E ; ................

; ---- 30D9-3108: status-line dlist template ----
30D9:  dw  E0D8 FFFF FFFF FFFF FFFF C807 C800 D000
30E9:  dw  D000 F000 D000 CD92 CD92 CD92 CD92 CD92
30F9:  dw  CD92 CD92 CD2F D000 D000 CD92 CD2F D000

; ---- 3109-3216: playfield walls display list -> 0x81B0 (318D-319C: 8x FFFF placeholders, runtime-patched at 0x8234 with the JSRLs from 0x3EE5) ----
3109:  dw  A3F8 03F8 9000 0000 87EF 7000 F0F0 87DF
3119:  dw  7000 F0F0 8000 77EF F0F0 8000 77EF F0F0
3129:  dw  83DF 7000 F0F0 83DF 7000 F0F0 8000 73EF
3139:  dw  F0F0 8000 73EF F0F0 86DF 056F F000 77FF
3149:  dw  7000 F0F0 F208 F0F0 F608 F00E F0F0 F00A
3159:  dw  9000 767F F0F0 F00A F0F0 F00E F208 F0F0
3169:  dw  F608 73FF 7000 F0F0 F608 F0F0 F208 F00A
3179:  dw  F0F0 F00E 9000 727F F0F0 F00E F0F0 F00A
3189:  dw  F608 F0F0 FFFF FFFF FFFF FFFF FFFF FFFF
3199:  dw  FFFF FFFF B000 CD92 CD92 CD92 CD92 CD92
31A9:  dw  CD92 CD92 CD2F D000 A1E0 010E 7000 0000
31B9:  dw  C807 A208 010E 5000 0000 C807 A230 010E
31C9:  dw  5000 0000 C807 A1B8 00DC 6000 0000 C807
31D9:  dw  A1E0 00DC 5000 0000 C807 A208 00DC 5000
31E9:  dw  0000 C807 A230 00DC 5000 0000 C807 A21C
31F9:  dw  0140 6000 0000 C9E9 A1F4 0140 5000 0000
3209:  dw  C9E9 A1CC 0140 5000 0000 C9E9 D000

; ---- 3217-33D8: attract/message page scripts ----
3217:  db  80 ; page-entry flag/pad

3218:  SEG next=$324F dest=$8000 hdr[A384 00F8 9000 0000]
3224:    text "IN THE YEAR 2003, THE OMEGA SYSTEM         "

324F:  SEG next=$3286 dest=$805E hdr[A364 00F0 9000 0000]
325B:    text "DEVELOPED A METHOD OF TRAINING ITS         "

3286:  SEG next=$32C4 dest=$80BC hdr[A342 00C0 9000 0000]
3292:    text "WARRIORS TO PROTECT THEIR STAR COLONIES.          "

32C4:  SEG next=$32FE dest=$8128 hdr[A322 00B0 9000 0000]
32D0:    text "OVER THE CITY OF KOMAR, ANDROID CONTROLLED    "

32FE:  SEG next=$3338 dest=$818C hdr[A302 00B0 9000 0000]
330A:    text "FIGHTERS RACED TO ENGAGE AND DESTROY THESE    "

3338:  SEG next=$3368 dest=$81F0 hdr[A2E2 0180 9000 0000]
3344:    text "OMEGAN WARRIORS.                    "

3368:  SEG next=$33A4 dest=$8240 hdr[A2A2 00D0 9000 0000]
3374:    text "POINTS WERE AWARDED FOR THE ABILITY TO          "

33A4:  SEG next=$33D7 dest=$82A8 hdr[A282 00C8 9000 0000]
33B0:    text "NEUTRALIZE THIS DROID FORCE AS FOLLOWS,"
33D7:  db  00 ; page-entry flag/pad
33D8:  db  00 ; page-entry flag/pad

; ---- 33D9-33F5: attract page dlist fragment (loaded by ATTRACT_PAGE_LOAD) ----
33D9:  dw  E180 A3FF 0000 9000 0000 9000 03FE 97FE
33E9:  dw  0000 9000 07FE 93FE 0000 B000
33F5:  dw  
33F6:  db  03

; ---- 33F6-3A0D: message page scripts (cont.) ----

33F6:  SEG next=$3403 dest=$8000 hdr[A2BC 00C8 9000 0000]
3402:    text ";"

3403:  SEG next=$3419 dest=$800A hdr[A2BC 11F4 7000 0000]
340F:    num  2 BCD bytes from $2FDC
3412:    text " POINTS"

3419:  SEG next=$3426 dest=$8028 hdr[A258 00C8 9000 0000]
3425:    text ">"

3426:  SEG next=$343C dest=$8032 hdr[A258 11F4 7000 0000]
3432:    num  2 BCD bytes from $2FDE
3435:    text " POINTS"

343C:  SEG next=$3449 dest=$8050 hdr[A1F4 00C8 9000 0000]
3448:    text "@"

3449:  SEG next=$345F dest=$805A hdr[A1F4 11F4 7000 0000]
3455:    num  2 BCD bytes from $2FE0
3458:    text " POINTS"

345F:  SEG next=$346C dest=$8078 hdr[A190 00C8 9000 0000]
346B:    text "?"

346C:  SEG next=$3482 dest=$8082 hdr[A190 11F4 7000 0000]
3478:    num  2 BCD bytes from $2FE2
347B:    text " POINTS"

3482:  SEG next=$348F dest=$80A0 hdr[A12C 00C8 9000 0000]
348E:    text "?"

348F:  SEG next=$34A5 dest=$80AA hdr[A12C 11F4 7000 0000]
349B:    num  2 BCD bytes from $2FE4
349E:    text " POINTS"
34A5:  db  00 ; page-entry flag/pad
34A6:  db  00 ; page-entry flag/pad
34A7:  db  80 ; page-entry flag/pad

34A8:  SEG next=$34E1 dest=$8000 hdr[A320 00E8 9000 0000]
34B4:    text "THE OMEGAN METHOD IS SO SUCCESSFUL,          "

34E1:  SEG next=$351C dest=$8062 hdr[A300 00D8 9000 0000]
34ED:    text "IT COMMANDS FEAR AND RESPECT FROM ALL          "

351C:  SEG next=$3562 dest=$80C8 hdr[A2DE 0040 9000 0000]
3528:    text "THROUGHOUT THE GALAXIES.  THE METHOD IS CODE NAMED . . .  "

3562:  SEG next=$3578 dest=$8144 hdr[A23E 1160 8000 0000]
356E:    text "OMEGA RACE"

3578:  SEG next=END dest=$9900 hdr[0035 2C80 00A1 0011]
3584:    text "p"

3585:  SEG next=END dest=$5250 hdr[5345 2053 4E4F 2045]
3591:    text "OF THE  "

3599:  SEG next=$35B7 dest=$802C hdr[A0FA 1100 8000 0000]
35A5:    text "FLASHING BUTTONS  "

35B7:  SEG next=$35E4 dest=$8058 hdr[A0C8 1060 8000 0000]
35C3:    text "OR INSERT ADDITIONAL COINS       "

35E4:  SEG next=$35FB dest=$80A2 hdr[A096 01B0 9000 0000]
35F0:    text "CREDIT  "
35F8:    num  1 BCD bytes from credits_bcd

35FB:  SEG next=$361F dest=$80BE hdr[A352 00D0 9000 0000]
3607:    text "      1 CREDIT GAME =///"

361F:  SEG next=$3649 dest=$80F6 hdr[A320 00B0 8000 0000]
362B:    text "        2 CREDIT GAME =///////"

3649:  SEG next=$3669 dest=$813A hdr[A28A 0198 7000 0000]
3655:    text "BONUS SHIPS AT      "

3669:  SEG next=$367C dest=$816A hdr[A258 01C0 8000 0000]
3675:    num  2 BCD bytes from $4061
3678:    text "0000"

367C:  SEG next=$368F dest=$8182 hdr[A226 01C0 6000 0000]
3688:    num  2 BCD bytes from $4063
368B:    text "0000"

368F:  SEG next=$36A2 dest=$819A hdr[A1F4 01C0 6000 0000]
369B:    num  2 BCD bytes from $4065
369E:    text "0000"
36A2:  db  00 ; page-entry flag/pad
36A3:  db  00 ; page-entry flag/pad
36A4:  db  00 ; page-entry flag/pad

36A5:  SEG next=$36BA dest=$82BE hdr[A21C 0210 8000 0000]
36B1:    text "SCORE    "

36BA:  SEG next=$36C7 dest=$82D8 hdr[A1F4 1160 7000 0000]
36C6:    text "<"
36C7:  db  00 ; page-entry flag/pad
36C8:  db  00 ; page-entry flag/pad
36C9:  db  00 ; page-entry flag/pad

36CA:  SEG next=$36E6 dest=$82E2 hdr[A1B8 01C0 6000 0000]
36D6:    text "LAST SCORE      "

36E6:  SEG next=$36F3 dest=$830A hdr[A190 1160 7000 0000]
36F2:    text "<"
36F3:  db  00 ; page-entry flag/pad
36F4:  db  00 ; page-entry flag/pad
36F5:  db  00 ; page-entry flag/pad

36F6:  SEG next=$370B dest=$82BE hdr[A271 0128 9000 0000]
3702:    text "PLAYER 1 "

370B:  SEG next=$3718 dest=$82D8 hdr[A251 1098 6000 0000]
3717:    text "<"
3718:  db  00 ; page-entry flag/pad
3719:  db  00 ; page-entry flag/pad
371A:  db  00 ; page-entry flag/pad

371B:  SEG next=$3730 dest=$82E2 hdr[A271 02A8 8000 0000]
3727:    text "PLAYER 2 "

3730:  SEG next=$373D dest=$82FC hdr[A251 1218 6000 0000]
373C:    text "<"

373D:  SEG next=END dest=$5500 hdr[1A37 9083 DCA1 0000]
3749:    text "`"

374A:  SEG next=END dest=$5243 hdr[4445 5449 2020 003C]
3756:    db   00 00

3758:  SEG next=$376D dest=$831A hdr[A190 01B8 7000 0000]
3764:    text "CREDIT  <"

376D:  SEG next=END dest=$9400 hdr[3437 BC83 F0A2 0010]
3779:    text "p"
377A:    db   00 00
377C:    text "SAME PLAYER AGAIN<      "
3794:    db   00 00 00

3797:  SEG next=$37B3 dest=$8334 hdr[A3A4 1110 8000 0000]
37A3:    text "FIRST PLAYER UP<"
37B3:  db  00 ; page-entry flag/pad
37B4:  db  00 ; page-entry flag/pad
37B5:  db  00 ; page-entry flag/pad

37B6:  SEG next=$37D3 dest=$8334 hdr[A3A4 1100 8000 0000]
37C2:    text "SECOND PLAYER UP<"
37D3:  db  00 ; page-entry flag/pad
37D4:  db  00 ; page-entry flag/pad
37D5:  db  80 ; page-entry flag/pad

37D6:  SEG next=$37EC dest=$8334 hdr[A2BC 1170 7000 0000]
37E2:    text "GAME OVER<"
37EC:  db  00 ; page-entry flag/pad
37ED:  db  00 ; page-entry flag/pad
37EE:  db  00 ; page-entry flag/pad

37EF:  SEG next=$380E dest=$8000 hdr[A2BC 10C8 7000 0000]
37FB:    text "1 CREDIT GAME      "

380E:  SEG next=$382F dest=$802E hdr[A294 10C8 8000 0000]
381A:    text "ALL TIME HIGH        "

382F:  SEG next=$3843 dest=$8060 hdr[A258 10C8 8000 0000]
383B:    num  4 BCD bytes from $43D6
383E:    text "  "
3840:    str  3 chars from $43D7

3843:  SEG next=$3864 dest=$8082 hdr[A208 10E8 7000 0000]
384F:    text "DAILY HIGHS          "

3864:  SEG next=$3878 dest=$80B4 hdr[A1CC 10C8 8000 0000]
3870:    num  4 BCD bytes from $43DD
3873:    text "  "
3875:    str  3 chars from $43DE

3878:  SEG next=$388C dest=$80D6 hdr[A1A4 10C8 7000 0000]
3884:    num  4 BCD bytes from $43E4
3887:    text "  "
3889:    str  3 chars from $43E5

388C:  SEG next=$38A0 dest=$80F8 hdr[A17C 10C8 7000 0000]
3898:    num  4 BCD bytes from $43EB
389B:    text "  "
389D:    str  3 chars from $43EC

38A0:  SEG next=$38B4 dest=$811A hdr[A154 10C8 7000 0000]
38AC:    num  4 BCD bytes from $43F2
38AF:    text "  "
38B1:    str  3 chars from $43F3

38B4:  SEG next=$38C8 dest=$813C hdr[A12C 10C8 7000 0000]
38C0:    num  4 BCD bytes from $43F9
38C3:    text "  "
38C5:    str  3 chars from $43FA

38C8:  SEG next=$38D6 dest=$815E hdr[A060 1113 7000 0000]
38D4:    text "00"

38D6:  SEG next=$38FB dest=$816A hdr[A064 0118 5000 0000]
38E2:    text "C P  1981 MIDWAY MFG. CO."
38FB:  db  00 ; page-entry flag/pad
38FC:  db  00 ; page-entry flag/pad
38FD:  db  00 ; page-entry flag/pad

38FE:  SEG next=$391F dest=$8000 hdr[A2BC 1198 7000 0000]
390A:    text "2 CREDIT GAME        "

391F:  SEG next=$3940 dest=$8032 hdr[A294 1198 8000 0000]
392B:    text "ALL TIME HIGH        "

3940:  SEG next=$3954 dest=$8064 hdr[A258 1198 8000 0000]
394C:    num  4 BCD bytes from $4400
394F:    text "  "
3951:    str  3 chars from $4401

3954:  SEG next=$3975 dest=$8086 hdr[A208 11B8 7000 0000]
3960:    text "DAILY HIGHS          "

3975:  SEG next=$3989 dest=$80B8 hdr[A1CC 1198 8000 0000]
3981:    num  4 BCD bytes from $4407
3984:    text "  "
3986:    str  3 chars from $4408

3989:  SEG next=$399D dest=$80DA hdr[A1A4 1198 7000 0000]
3995:    num  4 BCD bytes from $440E
3998:    text "  "
399A:    str  3 chars from $440F

399D:  SEG next=$39B1 dest=$80FC hdr[A17C 1198 7000 0000]
39A9:    num  4 BCD bytes from $4415
39AC:    text "  "
39AE:    str  3 chars from $4416

39B1:  SEG next=$39C5 dest=$811E hdr[A154 1198 7000 0000]
39BD:    num  4 BCD bytes from $441C
39C0:    text "  "
39C2:    str  3 chars from $441D

39C5:  SEG next=$39D9 dest=$8140 hdr[A12C 1198 7000 0000]
39D1:    num  4 BCD bytes from $4423
39D4:    text "  "
39D6:    str  3 chars from $4424

39D9:  SEG next=$39E7 dest=$8162 hdr[A060 1113 8000 0000]
39E5:    text "00"

39E7:  SEG next=$3A0C dest=$816E hdr[A064 0118 5000 0000]
39F3:    text "C P  1981 MIDWAY MFG. CO."
3A0C:  db  00 ; page-entry flag/pad
3A0D:  db  00 ; page-entry flag/pad

; ---- 3A0E-3A87: letter-wheel alphabet display list (0x7A bytes, SHIP_RESPAWN -> dlist in mode 4; 0x3A2E inside it doubles as DRAW_TEXT's A-Z JSRL table) ----
3A0E:  dw  A100 21A0 6000 0000 CD92 CD92 CD92 A1F4
3A1E:  dw  0200 7000 0000 F0FA A200 1020 8000 0000
3A2E:  dw  CC4D CC58 CC64 CC6C CC75 CC77 CC7F CC88
3A3E:  dw  CC91 CC9B CCA3 CCAB CCB2 CCBA CCC2 CCCA
3A4E:  dw  CCD2 CCDC CCE6 CCF0 CCF8 CD02 CD0B CD16
3A5E:  dw  CD1D CD27 CD92 CC75 CDBF CCDC CDBF CC4D
3A6E:  dw  CDBF CCE6 CDBF CC75 A200 13C0 8000 0000
3A7E:  dw  CC75 CDBF CCBA CDBF CC6C

; ---- 3A88-3EE4: message page scripts (cont.) ----
3A88:  db  00 ; page-entry flag/pad

3A89:  SEG next=$3AC8 dest=$807A hdr[A384 00C0 9000 0000]
3A95:    text "YOUR SCORE IS ONE OF THE TOP HIGH SCORES.          "

3AC8:  SEG next=$3B0F dest=$80E8 hdr[A364 00A0 9000 0000]
3AD4:    text "TO ENTER YOUR INITIALS, TURN THE ROTATE KNOB               "

3B0F:  SEG next=$3B4E dest=$8166 hdr[A342 00B8 9000 0000]
3B1B:    text "TO SELECT A LETTER AND THEN PRESS FIRE TO          "

3B4E:  SEG next=$3B93 dest=$81D4 hdr[A320 0178 9000 0000]
3B5A:    text "ENTER THE LETTER.                                        "
3B93:  db  00 ; page-entry flag/pad
3B94:  db  00 ; page-entry flag/pad
3B95:  db  00 ; page-entry flag/pad

3B96:  SEG next=$3BAB dest=$824E hdr[A3A4 1180 8000 0000]
3BA2:    text "PLAYER 1 "
3BAB:  db  00 ; page-entry flag/pad
3BAC:  db  00 ; page-entry flag/pad
3BAD:  db  00 ; page-entry flag/pad

3BAE:  SEG next=$3BC3 dest=$824E hdr[A3A4 1180 7000 0000]
3BBA:    text "PLAYER 2 "
3BC3:  db  00 ; page-entry flag/pad
3BC4:  db  00 ; page-entry flag/pad
3BC5:  db  00 ; page-entry flag/pad

3BC6:  SEG next=$3BDC dest=$8000 hdr[A258 1160 8000 0000]
3BD2:    text "OMEGA RACE"

3BDC:  SEG next=$3BFC dest=$801C hdr[A1F4 10F0 7000 0000]
3BE8:    text "1 COIN   = 1 CREDIT "

3BFC:  SEG next=$3C1C dest=$804C hdr[A190 10F0 8000 0000]
3C08:    text "2 COINS  = 2 CREDITS"
3C1C:  db  00 ; page-entry flag/pad
3C1D:  db  00 ; page-entry flag/pad
3C1E:  db  00 ; page-entry flag/pad

3C1F:  SEG next=$3C43 dest=$8000 hdr[A33E 00E0 9000 0000]
3C2B:    text "COIN CHUTE 1         "
3C40:    num  4 BCD bytes from $442A

3C43:  SEG next=$3C67 dest=$8042 hdr[A316 00E0 8000 0000]
3C4F:    text "COIN CHUTE 2         "
3C64:    num  4 BCD bytes from $442E

3C67:  SEG next=$3C8B dest=$8084 hdr[A2C6 00E0 8000 0000]
3C73:    text "TEST CREDITS         "
3C88:    num  4 BCD bytes from $4432

3C8B:  SEG next=$3CC0 dest=$80C6 hdr[A226 00E0 8000 0000]
3C97:    text "                      1 CREDIT   2 CREDIT"

3CC0:  SEG next=$3CEA dest=$8120 hdr[A1D6 00E0 9000 0000]
3CCC:    text "FIRST FREE SHIP      "
3CE1:    num  4 BCD bytes from $443A
3CE4:    text "   "
3CE7:    num  4 BCD bytes from $4452

3CEA:  SEG next=$3D14 dest=$8178 hdr[A1AE 00E0 9000 0000]
3CF6:    text "SECOND FREE SHIP     "
3D0B:    num  4 BCD bytes from $443E
3D0E:    text "   "
3D11:    num  4 BCD bytes from $4456

3D14:  SEG next=$3D3E dest=$81D0 hdr[A186 00E0 9000 0000]
3D20:    text "THIRD FREE SHIP      "
3D35:    num  4 BCD bytes from $4442
3D38:    text "   "
3D3B:    num  4 BCD bytes from $445A

3D3E:  SEG next=$3D68 dest=$8228 hdr[A136 00E0 9000 0000]
3D4A:    text "AVERAGE SCORE        "
3D5F:    num  4 BCD bytes from $4474
3D62:    text "   "
3D65:    num  4 BCD bytes from $447C

3D68:  SEG next=$3D92 dest=$8280 hdr[A10E 00E0 9000 0000]
3D74:    text "HIGHEST SCORE        "
3D89:    num  4 BCD bytes from $43D6
3D8C:    text "   "
3D8F:    num  4 BCD bytes from $4400

3D92:  SEG next=$3DC0 dest=$82D8 hdr[A0BE 00E0 9000 0000]
3D9E:    text "AVERAGE SEC. PER GAME  "
3DB5:    num  3 BCD bytes from $4478
3DB8:    text "     "
3DBD:    num  3 BCD bytes from $4480

3DC0:  SEG next=$3DEA dest=$8330 hdr[A096 00E0 9000 0000]
3DCC:    text "MAXIMUM SEC. PER GAME"
3DE1:    num  4 BCD bytes from $444E
3DE4:    text "   "
3DE7:    num  4 BCD bytes from $4466

3DEA:  SEG next=$3E0E dest=$8388 hdr[A064 00E0 9000 0000]
3DF6:    text "CURRENT CREDIT       "
3E0B:    num  1 BCD bytes from credits_bcd
3E0E:  db  00 ; page-entry flag/pad
3E0F:  db  00 ; page-entry flag/pad
3E10:  db  00 ; page-entry flag/pad

3E11:  SEG next=$3E3E dest=$8016 hdr[A28A 1100 8000 0000]
3E1D:    text " DROID FORCE ELIMINATED          "

3E3E:  SEG next=$3E5F dest=$8060 hdr[A190 10D0 8000 0000]
3E4A:    text "5,000 BONUS POINTS   "

3E5F:  SEG next=END dest=$7500 hdr[003E 8A80 20A2 0010]
3E6B:    num  0 BCD bytes from RESET
3E6E:    text "  FIRST"

3E75:  SEG next=END dest=$8B00 hdr[003E 8A80 20A2 0010]
3E81:    db   80

3E82:  SEG next=END dest=$5320 hdr[4345 4E4F 0044 0000]

3E8E:  SEG next=$3EA1 dest=$8000 hdr[A28A 1020 8000 0000]
3E9A:    text "  THIRD"
3EA1:  db  00 ; page-entry flag/pad
3EA2:  db  00 ; page-entry flag/pad
3EA3:  db  00 ; page-entry flag/pad

3EA4:  SEG next=$3EB7 dest=$8000 hdr[A28A 1020 8000 0000]
3EB0:    text " FOURTH"
3EB7:  db  00 ; page-entry flag/pad
3EB8:  db  00 ; page-entry flag/pad
3EB9:  db  00 ; page-entry flag/pad

3EBA:  SEG next=$3ECD dest=$8000 hdr[A28A 1020 8000 0000]
3EC6:    text "  FIFTH"
3ECD:  db  00 ; page-entry flag/pad
3ECE:  db  00 ; page-entry flag/pad
3ECF:  db  00 ; page-entry flag/pad

3ED0:  SEG next=$3EE3 dest=$8000 hdr[A28A 1020 8000 0000]
3EDC:    text "ANOTHER"
3EE3:  db  00 ; page-entry flag/pad
3EE4:  db  00 ; page-entry flag/pad

; ---- 3EE5-3F14: 3x8 JSRL words -> dlist 0x8234: calls to score/credit/lives sub-lists (rows 2/3 = player-swap orders, loaded at 0x0D71/0x0DA3) ----
3EE5:  dw  C15F C0CB C171 C123 C12C C18D C0D5 C19A
3EF5:  dw  C15F C0CB C12C C18D C0D5 C171 C123 C19A
3F05:  dw  C15F C123 C18D C0D5 C171 C0CB C12C C19A

; ---- 3F15-3F4C: page pointer table (page 1..) ----
3F15:  dw  3217 33F5 34A7 357A 36A4 36C9 36F5 371A
3F25:  dw  373F 3757 376F 3796 37B5 37D5 37EE 38FD
3F35:  dw  3A88 3B95 3BAD 3BC5 3C1E 3E10 3E61 3E77
3F45:  dw  3E8D 3EA3 3EB9 3ECF

; ---- 3F4D-3F5C: anim shape words: droid 3F4D[0-3] / death ship 3F55[0-3] (move_mode 0x1E03 arm; VG_KICK_TITLE reuses both rows as the title shimmer) ----
3F4D:  dw  CA04 CA2E CA51 CA74 CA97 CAAA CABE CAD2

; ---- 3F5D-3F64: explosion anim shape words (0x1DB4 arm, indexed angle&3) ----
3F5D:  dw  CC12 CBBA CBD8 CC12

; ---- 3F65-3FCC: hiscore-page dlist fragments (word 0 = 0xC0C5, the ship entry's JSRL to its 0x818A hull/flame sub-list) ----
; 0x3F69 + 0x64 bytes doubles as the player-2 lives/bonus icon-slot template: PLAYER_TURN_SWAP 0x0DAE copies it to dlist 0x8258 on the swap to player 2, mirroring the 0x31B1 -> 0x8258 copy (walls blob offset 0xA8) done for player 1
3F65:  dw  C0C5 2020 A1E0 02F2 6000 0000 C9E9 A208
3F75:  dw  02F2 5000 0000 C9E9 A230 02F2 5000 0000
3F85:  dw  C9E9 A1B8 0324 6000 0000 C9E9 A1E0 0324
3F95:  dw  5000 0000 C9E9 A208 0324 5000 0000 C9E9
3FA5:  dw  A230 0324 5000 0000 C9E9 A21C 02C0 6000
3FB5:  dw  0000 C807 A1F4 02C0 5000 0000 C807 A1CC
3FC5:  dw  02C0 5000 0000 C807

; ---- 3FCD-3FF6: digit/char glyph JSRL words (CHAR_GLYPH_EMIT 0x2C36; index base 0x3FD3 for chars 0x2F-0x40) ----
; 3FCD = D000 guard; 3FCF = '.' CDAA; 3FD1 = ',' CDB3
; '/'=CDD3  '0'-'9'=CD2F..CDA0  ':'=CD92 (space glyph)
; ';'=CBA7 (0x974E points-caption enemy icon)  '>'=CBAF (0x975E icon)
; '<'=D000 - an RTSL, the 'return' char game-mode pages end with, so a JSRL'd page sub returns to its caller
; '='=CDCA  '?'=CA04 / '@'=CA2E (droid anim frames 0/1 - page 2 draws its shimmer enemy icons as text)
3FCD:  dw  D000 CDAA CDB3 CDD3 CD2F CD3B CD46 CD52
3FDD:  dw  CD64 CD6F CD81 CD8D CD94 CDA0 CD92 CBA7
3FED:  dw  D000 CDCA CBAF CA04 CA2E

; ---- 3FF7-3FFF: constants (0x3FF7 = BCD +1) ----
3FF7:  db  01 00 00 00 D6 00 00 00 1F                      ; .........
