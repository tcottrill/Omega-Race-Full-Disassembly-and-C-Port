; omega.e1/f1 vector ROM 0x9000-0x9fff  (dvgdasm.py)
; word addresses (JSRL operands) are (byte-0x8000)/2
;
; Nothing JSRLs these shapes statically - the game builds
; display lists in RAM. Each block below carries its real
; provenance: 'refs:' = main-ROM data words that point at
; it, or a computed-word formula for the ship/shot
; families.
;
; [THRUST_n][SHIP_n] pairs sit 7 words apart: the entry
; writer takes flame = hull - 7 (ROM 0x1E58). Heading
; bits 4-5 pick a frame set (SHIP_BASE 0x30A1): set 0 =
; SHIP_* (word $800), set 1 = SHIP_M* X-flip (word $DFC),
; sets 2/3 live in vector RAM at 0x8BF8 (180 deg) and
; 0x87F0 (Y-flip) - WALL_BUFFERS_BUILD 0x18BC copies
; 0x9000-0x9407 there and flips SVEC/VCTR sign bits.
;

THRUST_00: (JSRL operand $800)
; flame of SHIP_00 (hull word - 7, ROM 0x1E58)
; refs: status tmpl 30D9+0C
9000/800:  30BF 07FF  VCTR  s=3  dx=-1023 dy=191   z=0
9004/802:  34BF F63F  VCTR  s=3  dx=-575  dy=-191  z=15
9008/804:  347F F23F  VCTR  s=3  dx=575   dy=-127  z=15
900C/806:  D000       RTSL

SHIP_00: (JSRL operand $807)
; hull, heading 0x00: word $800 + 30A9[0] (ship shape select 0x1E18)
; refs: status tmpl 30D9+0A
; refs: game screen 3109+B0
; refs: game screen 3109+BA
; refs: game screen 3109+C4
; refs: game screen 3109+CE
; refs: game screen 3109+D8
; refs: game screen 3109+E2
; refs: game screen 3109+EC
; refs: hiscore frags 3F65+52
; refs: hiscore frags 3F65+5C
; refs: hiscore frags 3F65+66
900E/807:  2000 027F  VCTR  s=2  dx=639   dy=0     z=0
9012/809:  413F F67F  VCTR  s=4  dx=-639  dy=319   z=15
9016/80B:  469F F15F  VCTR  s=4  dx=351   dy=-671  z=15
901A/80D:  F8F2       SVEC  s=3  dx=512   dy=0     z=15
901C/80E:  347F F67F  VCTR  s=3  dx=-639  dy=-127  z=15
9020/810:  41FF F21F  VCTR  s=4  dx=543   dy=511   z=15
9024/812:  257F F27F  VCTR  s=2  dx=639   dy=-383  z=15
9028/814:  24FF F67F  VCTR  s=2  dx=-639  dy=-255  z=15
902C/816:  41FF F61F  VCTR  s=4  dx=-543  dy=511   z=15
9030/818:  347F F27F  VCTR  s=3  dx=639   dy=-127  z=15
9034/81A:  F8F6       SVEC  s=3  dx=-512  dy=0     z=15
9036/81B:  469F F55F  VCTR  s=4  dx=-351  dy=-671  z=15
903A/81D:  411F F27F  VCTR  s=4  dx=639   dy=287   z=15
903E/81F:  D000       RTSL

THRUST_01: (JSRL operand $820)
; flame of SHIP_01 (hull word - 7, ROM 0x1E58)
9040/820:  303F 07FF  VCTR  s=3  dx=-1023 dy=63    z=0
9044/822:  34BF F63F  VCTR  s=3  dx=-575  dy=-191  z=15
9048/824:  347F F23F  VCTR  s=3  dx=575   dy=-127  z=15
904C/826:  D000       RTSL

SHIP_01: (JSRL operand $827)
; hull, heading 0x01: word $800 + 30A9[1] (ship shape select 0x1E18)
904E/827:  2000 027F  VCTR  s=2  dx=639   dy=0     z=0
9052/829:  40FF F69F  VCTR  s=4  dx=-671  dy=255   z=15
9056/82B:  465F F19F  VCTR  s=4  dx=415   dy=-607  z=15
905A/82D:  F8F2       SVEC  s=3  dx=512   dy=0     z=15
905C/82E:  34BF F67F  VCTR  s=3  dx=-639  dy=-191  z=15
9060/830:  423F F1FF  VCTR  s=4  dx=511   dy=575   z=15
9064/832:  24FF F27F  VCTR  s=2  dx=639   dy=-255  z=15
9068/834:  257F F67F  VCTR  s=2  dx=-639  dy=-383  z=15
906C/836:  41BF F63F  VCTR  s=4  dx=-575  dy=447   z=15
9070/838:  343F F27F  VCTR  s=3  dx=639   dy=-63   z=15
9074/83A:  247F F7FF  VCTR  s=2  dx=-1023 dy=-127  z=15
9078/83C:  46BF F51F  VCTR  s=4  dx=-287  dy=-703  z=15
907C/83E:  415F F25F  VCTR  s=4  dx=607   dy=351   z=15
9080/840:  D000       RTSL

THRUST_02: (JSRL operand $841)
; flame of SHIP_02 (hull word - 7, ROM 0x1E58)
9082/841:  3000 07FF  VCTR  s=3  dx=-1023 dy=0     z=0
9086/843:  25FF F7FF  VCTR  s=2  dx=-1023 dy=-511  z=15
908A/845:  343F F23F  VCTR  s=3  dx=575   dy=-63   z=15
908E/847:  D000       RTSL

SHIP_02: (JSRL operand $848)
; hull, heading 0x02: word $800 + 30A9[2] (ship shape select 0x1E18)
9090/848:  207F 027F  VCTR  s=2  dx=639   dy=127   z=0
9094/84A:  40BF F6BF  VCTR  s=4  dx=-703  dy=191   z=15
9098/84C:  465F F1DF  VCTR  s=4  dx=479   dy=-607  z=15
909C/84E:  20FF F3FF  VCTR  s=2  dx=1023  dy=255   z=15
90A0/850:  34FF F63F  VCTR  s=3  dx=-575  dy=-255  z=15
90A4/852:  425F F19F  VCTR  s=4  dx=415   dy=607   z=15
90A8/854:  F5F3       SVEC  s=2  dx=768   dy=-256  z=15
90AA/855:  257F F67F  VCTR  s=2  dx=-639  dy=-383  z=15
90AE/857:  417F F67F  VCTR  s=4  dx=-639  dy=383   z=15
90B2/859:  303F F2BF  VCTR  s=3  dx=703   dy=63    z=15
90B6/85B:  24FF F7FF  VCTR  s=2  dx=-1023 dy=-255  z=15
90BA/85D:  46DF F4DF  VCTR  s=4  dx=-223  dy=-735  z=15
90BE/85F:  419F F23F  VCTR  s=4  dx=575   dy=415   z=15
90C2/861:  D000       RTSL

THRUST_03: (JSRL operand $862)
; flame of SHIP_03 (hull word - 7, ROM 0x1E58)
90C4/862:  343F 07FF  VCTR  s=3  dx=-1023 dy=-63   z=0
90C8/864:  26FF F7FF  VCTR  s=2  dx=-1023 dy=-767  z=15
90CC/866:  3000 F27F  VCTR  s=3  dx=639   dy=0     z=15
90D0/868:  D000       RTSL

SHIP_03: (JSRL operand $869)
; hull, heading 0x03: word $800 + 30A9[3] (ship shape select 0x1E18)
90D2/869:  207F 027F  VCTR  s=2  dx=639   dy=127   z=0
90D6/86B:  407F F6BF  VCTR  s=4  dx=-703  dy=127   z=15
90DA/86D:  461F F21F  VCTR  s=4  dx=543   dy=-543  z=15
90DE/86F:  217F F37F  VCTR  s=2  dx=895   dy=383   z=15
90E2/871:  353F F63F  VCTR  s=3  dx=-575  dy=-319  z=15
90E6/873:  427F F17F  VCTR  s=4  dx=383   dy=639   z=15
90EA/875:  247F F27F  VCTR  s=2  dx=639   dy=-127  z=15
90EE/877:  267F F5FF  VCTR  s=2  dx=-511  dy=-639  z=15
90F2/879:  415F F69F  VCTR  s=4  dx=-671  dy=351   z=15
90F6/87B:  307F F27F  VCTR  s=3  dx=639   dy=127   z=15
90FA/87D:  257F F77F  VCTR  s=2  dx=-895  dy=-383  z=15
90FE/87F:  46DF F47F  VCTR  s=4  dx=-127  dy=-735  z=15
9102/881:  337F F3FF  VCTR  s=3  dx=1023  dy=895   z=15
9106/883:  D000       RTSL

THRUST_04: (JSRL operand $884)
; flame of SHIP_04 (hull word - 7, ROM 0x1E58)
9108/884:  34BF 07FF  VCTR  s=3  dx=-1023 dy=-191  z=0
910C/886:  26FF F77F  VCTR  s=2  dx=-895  dy=-767  z=15
9110/888:  303F F27F  VCTR  s=3  dx=639   dy=63    z=15
9114/88A:  D000       RTSL

SHIP_04: (JSRL operand $88B)
; hull, heading 0x04: word $800 + 30A9[4] (ship shape select 0x1E18)
9116/88B:  20FF 027F  VCTR  s=2  dx=639   dy=255   z=0
911A/88D:  401F F6DF  VCTR  s=4  dx=-735  dy=31    z=15
911E/88F:  45DF F23F  VCTR  s=4  dx=575   dy=-479  z=15
9122/891:  F9F2       SVEC  s=3  dx=512   dy=256   z=15
9124/892:  277F F7FF  VCTR  s=2  dx=-1023 dy=-895  z=15
9128/894:  42BF F11F  VCTR  s=4  dx=287   dy=703   z=15
912C/896:  247F F2FF  VCTR  s=2  dx=767   dy=-127  z=15
9130/898:  267F F5FF  VCTR  s=2  dx=-511  dy=-639  z=15
9134/89A:  411F F6BF  VCTR  s=4  dx=-703  dy=287   z=15
9138/89C:  30BF F2BF  VCTR  s=3  dx=703   dy=191   z=15
913C/89E:  FDF6       SVEC  s=3  dx=-512  dy=-256  z=15
913E/89F:  46DF F43F  VCTR  s=4  dx=-63   dy=-735  z=15
9142/8A1:  33FF F3BF  VCTR  s=3  dx=959   dy=1023  z=15
9146/8A3:  D000       RTSL

THRUST_05: (JSRL operand $8A4)
; flame of SHIP_05 (hull word - 7, ROM 0x1E58)
9148/8A4:  34FF 07BF  VCTR  s=3  dx=-959  dy=-255  z=0
914C/8A6:  277F F77F  VCTR  s=2  dx=-895  dy=-895  z=15
9150/8A8:  303F F23F  VCTR  s=3  dx=575   dy=63    z=15
9154/8AA:  D000       RTSL

SHIP_05: (JSRL operand $8AB)
; hull, heading 0x05: word $800 + 30A9[5] (ship shape select 0x1E18)
9156/8AB:  F102       SVEC  s=2  dx=512   dy=256   z=0
9158/8AC:  441F F6BF  VCTR  s=4  dx=-703  dy=-31   z=15
915C/8AE:  459F F25F  VCTR  s=4  dx=607   dy=-415  z=15
9160/8B0:  F9F2       SVEC  s=3  dx=512   dy=256   z=15
9162/8B1:  35BF F63F  VCTR  s=3  dx=-575  dy=-447  z=15
9166/8B3:  42DF F0FF  VCTR  s=4  dx=255   dy=735   z=15
916A/8B5:  F0F3       SVEC  s=2  dx=768   dy=0     z=15
916C/8B6:  267F F57F  VCTR  s=2  dx=-383  dy=-639  z=15
9170/8B8:  40BF F6DF  VCTR  s=4  dx=-735  dy=191   z=15
9174/8BA:  30BF F2BF  VCTR  s=3  dx=703   dy=191   z=15
9178/8BC:  FDF6       SVEC  s=3  dx=-512  dy=-256  z=15
917A/8BD:  46DF F01F  VCTR  s=4  dx=31    dy=-735  z=15
917E/8BF:  421F F17F  VCTR  s=4  dx=383   dy=543   z=15
9182/8C1:  D000       RTSL

THRUST_06: (JSRL operand $8C2)
; flame of SHIP_06 (hull word - 7, ROM 0x1E58)
9184/8C2:  357F 07BF  VCTR  s=3  dx=-959  dy=-383  z=0
9188/8C4:  277F F6FF  VCTR  s=2  dx=-767  dy=-895  z=15
918C/8C6:  307F F27F  VCTR  s=3  dx=639   dy=127   z=15
9190/8C8:  D000       RTSL

SHIP_06: (JSRL operand $8C9)
; hull, heading 0x06: word $800 + 30A9[6] (ship shape select 0x1E18)
9192/8C9:  12FF 03FF  VCTR  s=1  dx=1023  dy=767   z=0
9196/8CB:  445F F6BF  VCTR  s=4  dx=-703  dy=-95   z=15
919A/8CD:  457F F27F  VCTR  s=4  dx=639   dy=-383  z=15
919E/8CF:  227F F3FF  VCTR  s=2  dx=1023  dy=639   z=15
91A2/8D1:  FEF6       SVEC  s=3  dx=-512  dy=-512  z=15
91A4/8D2:  42FF F09F  VCTR  s=4  dx=159   dy=767   z=15
91A8/8D4:  F0F3       SVEC  s=2  dx=768   dy=0     z=15
91AA/8D5:  267F F4FF  VCTR  s=2  dx=-255  dy=-639  z=15
91AE/8D7:  407F F6FF  VCTR  s=4  dx=-767  dy=127   z=15
91B2/8D9:  30FF F2BF  VCTR  s=3  dx=703   dy=255   z=15
91B6/8DB:  267F F7FF  VCTR  s=2  dx=-1023 dy=-639  z=15
91BA/8DD:  46DF F05F  VCTR  s=4  dx=95    dy=-735  z=15
91BE/8DF:  425F F15F  VCTR  s=4  dx=351   dy=607   z=15
91C2/8E1:  D000       RTSL

THRUST_07: (JSRL operand $8E2)
; flame of SHIP_07 (hull word - 7, ROM 0x1E58)
91C4/8E2:  35BF 077F  VCTR  s=3  dx=-895  dy=-447  z=0
91C8/8E4:  27FF F67F  VCTR  s=2  dx=-639  dy=-1023 z=15
91CC/8E6:  30BF F23F  VCTR  s=3  dx=575   dy=191   z=15
91D0/8E8:  D000       RTSL

SHIP_07: (JSRL operand $8E9)
; hull, heading 0x07: word $800 + 30A9[7] (ship shape select 0x1E18)
91D2/8E9:  12FF 03FF  VCTR  s=1  dx=1023  dy=767   z=0
91D6/8EB:  447F F6BF  VCTR  s=4  dx=-703  dy=-127  z=15
91DA/8ED:  453F F2BF  VCTR  s=4  dx=703   dy=-319  z=15
91DE/8EF:  227F F37F  VCTR  s=2  dx=895   dy=639   z=15
91E2/8F1:  FEF6       SVEC  s=3  dx=-512  dy=-512  z=15
91E4/8F2:  42DF F07F  VCTR  s=4  dx=127   dy=735   z=15
91E8/8F4:  207F F27F  VCTR  s=2  dx=639   dy=127   z=15
91EC/8F6:  267F F4FF  VCTR  s=2  dx=-255  dy=-639  z=15
91F0/8F8:  401F F6DF  VCTR  s=4  dx=-735  dy=31    z=15
91F4/8FA:  317F F23F  VCTR  s=3  dx=575   dy=383   z=15
91F8/8FC:  F7F7       SVEC  s=2  dx=-768  dy=-768  z=15
91FA/8FD:  46DF F09F  VCTR  s=4  dx=159   dy=-735  z=15
91FE/8FF:  427F F11F  VCTR  s=4  dx=287   dy=639   z=15
9202/901:  D000       RTSL

THRUST_08: (JSRL operand $902)
; flame of SHIP_08 (hull word - 7, ROM 0x1E58)
9204/902:  363F 073F  VCTR  s=3  dx=-831  dy=-575  z=0
9208/904:  27FF F5FF  VCTR  s=2  dx=-511  dy=-1023 z=15
920C/906:  21FF F3FF  VCTR  s=2  dx=1023  dy=511   z=15
9210/908:  D000       RTSL

SHIP_08: (JSRL operand $909)
; hull, heading 0x08: word $800 + 30A9[8] (ship shape select 0x1E18)
9212/909:  F202       SVEC  s=2  dx=512   dy=512   z=0
9214/90A:  44DF F69F  VCTR  s=4  dx=-671  dy=-223  z=15
9218/90C:  44FF F2BF  VCTR  s=4  dx=703   dy=-255  z=15
921C/90E:  F3F3       SVEC  s=2  dx=768   dy=768   z=15
921E/90F:  363F F57F  VCTR  s=3  dx=-383  dy=-575  z=15
9222/911:  42FF F01F  VCTR  s=4  dx=31    dy=767   z=15
9226/913:  207F F27F  VCTR  s=2  dx=639   dy=127   z=15
922A/915:  267F F47F  VCTR  s=2  dx=-127  dy=-639  z=15
922E/917:  441F F6FF  VCTR  s=4  dx=-767  dy=-31   z=15
9232/919:  317F F23F  VCTR  s=3  dx=575   dy=383   z=15
9236/91B:  F7F7       SVEC  s=2  dx=-768  dy=-768  z=15
9238/91C:  46BF F0FF  VCTR  s=4  dx=255   dy=-703  z=15
923C/91E:  429F F0DF  VCTR  s=4  dx=223   dy=671   z=15
9240/920:  D000       RTSL

THRUST_09: (JSRL operand $921)
; flame of SHIP_09 (hull word - 7, ROM 0x1E58)
9242/921:  367F 06FF  VCTR  s=3  dx=-767  dy=-639  z=0
9246/923:  363F F4BF  VCTR  s=3  dx=-191  dy=-575  z=15
924A/925:  227F F3FF  VCTR  s=2  dx=1023  dy=639   z=15
924E/927:  D000       RTSL

SHIP_09: (JSRL operand $928)
; hull, heading 0x09: word $800 + 30A9[9] (ship shape select 0x1E18)
9250/928:  13FF 02FF  VCTR  s=1  dx=767   dy=1023  z=0
9254/92A:  451F F67F  VCTR  s=4  dx=-639  dy=-287  z=15
9258/92C:  449F F2DF  VCTR  s=4  dx=735   dy=-159  z=15
925C/92E:  F3F3       SVEC  s=2  dx=768   dy=768   z=15
925E/92F:  363F F57F  VCTR  s=3  dx=-383  dy=-575  z=15
9262/931:  42DF F41F  VCTR  s=4  dx=-31   dy=735   z=15
9266/933:  20FF F27F  VCTR  s=2  dx=639   dy=255   z=15
926A/935:  267F F47F  VCTR  s=2  dx=-127  dy=-639  z=15
926E/937:  447F F6DF  VCTR  s=4  dx=-735  dy=-127  z=15
9272/939:  FAF2       SVEC  s=3  dx=512   dy=512   z=15
9274/93A:  277F F67F  VCTR  s=2  dx=-639  dy=-895  z=15
9278/93C:  46BF F13F  VCTR  s=4  dx=319   dy=-703  z=15
927C/93E:  42BF F07F  VCTR  s=4  dx=127   dy=703   z=15
9280/940:  D000       RTSL

THRUST_10: (JSRL operand $941)
; flame of SHIP_10 (hull word - 7, ROM 0x1E58)
9282/941:  36BF 06BF  VCTR  s=3  dx=-703  dy=-703  z=0
9286/943:  367F F47F  VCTR  s=3  dx=-127  dy=-639  z=15
928A/945:  22FF F37F  VCTR  s=2  dx=895   dy=767   z=15
928E/947:  D000       RTSL

SHIP_10: (JSRL operand $948)
; hull, heading 0x0A: word $800 + 30A9[10] (ship shape select 0x1E18)
9290/948:  13FF 02FF  VCTR  s=1  dx=767   dy=1023  z=0
9294/94A:  455F F65F  VCTR  s=4  dx=-607  dy=-351  z=15
9298/94C:  445F F2DF  VCTR  s=4  dx=735   dy=-95   z=15
929C/94E:  23FF F27F  VCTR  s=2  dx=639   dy=1023  z=15
92A0/950:  36BF F4FF  VCTR  s=3  dx=-255  dy=-703  z=15
92A4/952:  42FF F47F  VCTR  s=4  dx=-127  dy=767   z=15
92A8/954:  20FF F27F  VCTR  s=2  dx=639   dy=255   z=15
92AC/956:  F7F0       SVEC  s=2  dx=0     dy=-768  z=15
92AE/957:  449F F6FF  VCTR  s=4  dx=-767  dy=-159  z=15
92B2/959:  FAF2       SVEC  s=3  dx=512   dy=512   z=15
92B4/95A:  27FF F67F  VCTR  s=2  dx=-639  dy=-1023 z=15
92B8/95C:  467F F17F  VCTR  s=4  dx=383   dy=-639  z=15
92BC/95E:  42BF F05F  VCTR  s=4  dx=95    dy=703   z=15
92C0/960:  D000       RTSL

THRUST_11: (JSRL operand $961)
; flame of SHIP_11 (hull word - 7, ROM 0x1E58)
92C2/961:  373F 067F  VCTR  s=3  dx=-639  dy=-831  z=0
92C6/963:  363F F43F  VCTR  s=3  dx=-63   dy=-575  z=15
92CA/965:  237F F37F  VCTR  s=2  dx=895   dy=895   z=15
92CE/967:  D000       RTSL

SHIP_11: (JSRL operand $968)
; hull, heading 0x0B: word $800 + 30A9[11] (ship shape select 0x1E18)
92D0/968:  F201       SVEC  s=2  dx=256   dy=512   z=0
92D2/969:  457F F61F  VCTR  s=4  dx=-543  dy=-383  z=15
92D6/96B:  441F F2DF  VCTR  s=4  dx=735   dy=-31   z=15
92DA/96D:  FAF1       SVEC  s=3  dx=256   dy=512   z=15
92DC/96E:  36BF F4BF  VCTR  s=3  dx=-191  dy=-703  z=15
92E0/970:  42DF F4BF  VCTR  s=4  dx=-191  dy=735   z=15
92E4/972:  217F F27F  VCTR  s=2  dx=639   dy=383   z=15
92E8/974:  F7F0       SVEC  s=2  dx=0     dy=-768  z=15
92EA/975:  44FF F6DF  VCTR  s=4  dx=-735  dy=-255  z=15
92EE/977:  323F F1BF  VCTR  s=3  dx=447   dy=575   z=15
92F2/979:  FEF5       SVEC  s=3  dx=-256  dy=-512  z=15
92F4/97A:  465F F19F  VCTR  s=4  dx=415   dy=-607  z=15
92F8/97C:  42BF F01F  VCTR  s=4  dx=31    dy=703   z=15
92FC/97E:  D000       RTSL

THRUST_12: (JSRL operand $97F)
; flame of SHIP_12 (hull word - 7, ROM 0x1E58)
92FE/97F:  373F 05FF  VCTR  s=3  dx=-511  dy=-831  z=0
9302/981:  367F F43F  VCTR  s=3  dx=-63   dy=-639  z=15
9306/983:  237F F2FF  VCTR  s=2  dx=767   dy=895   z=15
930A/985:  D000       RTSL

SHIP_12: (JSRL operand $986)
; hull, heading 0x0C: word $800 + 30A9[12] (ship shape select 0x1E18)
930C/986:  227F 00FF  VCTR  s=2  dx=255   dy=639   z=0
9310/988:  37BF F7FF  VCTR  s=3  dx=-1023 dy=-959  z=15
9314/98A:  403F F2DF  VCTR  s=4  dx=735   dy=63    z=15
9318/98C:  FAF1       SVEC  s=3  dx=256   dy=512   z=15
931A/98D:  36BF F4BF  VCTR  s=3  dx=-191  dy=-703  z=15
931E/98F:  42BF F51F  VCTR  s=4  dx=-287  dy=703   z=15
9322/991:  21FF F27F  VCTR  s=2  dx=639   dy=511   z=15
9326/993:  26FF F07F  VCTR  s=2  dx=127   dy=-767  z=15
932A/995:  451F F6BF  VCTR  s=4  dx=-703  dy=-287  z=15
932E/997:  23FF F37F  VCTR  s=2  dx=895   dy=1023  z=15
9332/999:  FEF5       SVEC  s=3  dx=-256  dy=-512  z=15
9334/99A:  463F F1DF  VCTR  s=4  dx=479   dy=-575  z=15
9338/99C:  42DF F41F  VCTR  s=4  dx=-31   dy=735   z=15
933C/99E:  D000       RTSL

THRUST_13: (JSRL operand $99F)
; flame of SHIP_13 (hull word - 7, ROM 0x1E58)
933E/99F:  377F 05BF  VCTR  s=3  dx=-447  dy=-895  z=0
9342/9A1:  367F F000  VCTR  s=3  dx=0     dy=-639  z=15
9346/9A3:  23FF F2FF  VCTR  s=2  dx=767   dy=1023  z=15
934A/9A5:  D000       RTSL

SHIP_13: (JSRL operand $9A6)
; hull, heading 0x0D: word $800 + 30A9[13] (ship shape select 0x1E18)
934C/9A6:  227F 007F  VCTR  s=2  dx=127   dy=639   z=0
9350/9A8:  37FF F77F  VCTR  s=3  dx=-895  dy=-1023 z=15
9354/9AA:  407F F2DF  VCTR  s=4  dx=735   dy=127   z=15
9358/9AC:  237F F17F  VCTR  s=2  dx=383   dy=895   z=15
935C/9AE:  367F F47F  VCTR  s=3  dx=-127  dy=-639  z=15
9360/9B0:  429F F55F  VCTR  s=4  dx=-351  dy=671   z=15
9364/9B2:  21FF F27F  VCTR  s=2  dx=639   dy=511   z=15
9368/9B4:  267F F07F  VCTR  s=2  dx=127   dy=-639  z=15
936C/9B6:  457F F67F  VCTR  s=4  dx=-639  dy=-383  z=15
9370/9B8:  323F F13F  VCTR  s=3  dx=319   dy=575   z=15
9374/9BA:  277F F57F  VCTR  s=2  dx=-383  dy=-895  z=15
9378/9BC:  461F F21F  VCTR  s=4  dx=543   dy=-543  z=15
937C/9BE:  42BF F47F  VCTR  s=4  dx=-127  dy=703   z=15
9380/9C0:  D000       RTSL

THRUST_14: (JSRL operand $9C1)
; flame of SHIP_14 (hull word - 7, ROM 0x1E58)
9382/9C1:  37BF 053F  VCTR  s=3  dx=-319  dy=-959  z=0
9386/9C3:  363F F03F  VCTR  s=3  dx=63    dy=-575  z=15
938A/9C5:  23FF F1FF  VCTR  s=2  dx=511   dy=1023  z=15
938E/9C7:  D000       RTSL

SHIP_14: (JSRL operand $9C8)
; hull, heading 0x0E: word $800 + 30A9[14] (ship shape select 0x1E18)
9390/9C8:  227F 007F  VCTR  s=2  dx=127   dy=639   z=0
9394/9CA:  463F F59F  VCTR  s=4  dx=-415  dy=-575  z=15
9398/9CC:  40DF F2DF  VCTR  s=4  dx=735   dy=223   z=15
939C/9CE:  23FF F0FF  VCTR  s=2  dx=255   dy=1023  z=15
93A0/9D0:  36BF F43F  VCTR  s=3  dx=-63   dy=-703  z=15
93A4/9D2:  427F F57F  VCTR  s=4  dx=-383  dy=639   z=15
93A8/9D4:  227F F17F  VCTR  s=2  dx=383   dy=639   z=15
93AC/9D6:  F7F1       SVEC  s=2  dx=256   dy=-768  z=15
93AE/9D7:  459F F65F  VCTR  s=4  dx=-607  dy=-415  z=15
93B2/9D9:  323F F0FF  VCTR  s=3  dx=255   dy=575   z=15
93B6/9DB:  27FF F4FF  VCTR  s=2  dx=-255  dy=-1023 z=15
93BA/9DD:  45DF F25F  VCTR  s=4  dx=607   dy=-479  z=15
93BE/9DF:  42BF F4BF  VCTR  s=4  dx=-191  dy=703   z=15
93C2/9E1:  D000       RTSL

THRUST_15: (JSRL operand $9E2)
; flame of SHIP_15 (hull word - 7, ROM 0x1E58)
93C4/9E2:  37FF 04FF  VCTR  s=3  dx=-255  dy=-1023 z=0
93C8/9E4:  363F F07F  VCTR  s=3  dx=127   dy=-575  z=15
93CC/9E6:  323F F0BF  VCTR  s=3  dx=191   dy=575   z=15
93D0/9E8:  D000       RTSL

SHIP_15: (JSRL operand $9E9)
; hull, heading 0x0F: word $800 + 30A9[15] (ship shape select 0x1E18)
; refs: game screen 3109+F6
; refs: game screen 3109+100
; refs: game screen 3109+10A
; refs: hiscore frags 3F65+0C
; refs: hiscore frags 3F65+16
; refs: hiscore frags 3F65+20
; refs: hiscore frags 3F65+2A
; refs: hiscore frags 3F65+34
; refs: hiscore frags 3F65+3E
; refs: hiscore frags 3F65+48
; also JSRL'd from within the vector ROM
93D2/9E9:  227F 0000  VCTR  s=2  dx=0     dy=639   z=0
93D6/9EB:  465F F55F  VCTR  s=4  dx=-351  dy=-607  z=15
93DA/9ED:  411F F2BF  VCTR  s=4  dx=703   dy=287   z=15
93DE/9EF:  23FF F07F  VCTR  s=2  dx=127   dy=1023  z=15
93E2/9F1:  367F F03F  VCTR  s=3  dx=63    dy=-639  z=15
93E6/9F3:  423F F5BF  VCTR  s=4  dx=-447  dy=575   z=15
93EA/9F5:  227F F17F  VCTR  s=2  dx=383   dy=639   z=15
93EE/9F7:  267F F0FF  VCTR  s=2  dx=255   dy=-639  z=15
93F2/9F9:  45FF F63F  VCTR  s=4  dx=-575  dy=-511  z=15
93F6/9FB:  327F F0BF  VCTR  s=3  dx=191   dy=639   z=15
93FA/9FD:  FEF0       SVEC  s=3  dx=0     dy=-512  z=15
93FC/9FE:  459F F25F  VCTR  s=4  dx=607   dy=-415  z=15
9400/A00:  429F F4FF  VCTR  s=4  dx=-255  dy=671   z=15
9404/A02:  D000       RTSL
9406/A03:  FF3C       (orphan word; real shape starts at 9408)

CMDSHIP_00: (JSRL operand $A04)
; refs: anim tbl 3F4D[0] = droid anim 0 (also title shimmer)
; refs: DROID template shape word (2F67+21 = CA04)
9408/A04:  FA00       SVEC  s=3  dx=0     dy=512   z=0
940A/A05:  36BF F1BF  VCTR  s=3  dx=447   dy=-703  z=15
940E/A07:  3000 F73F  VCTR  s=3  dx=-831  dy=0     z=15
9412/A09:  32BF F17F  VCTR  s=3  dx=383   dy=703   z=15
9416/A0B:  347F 03FF  VCTR  s=3  dx=1023  dy=-127  z=0
941A/A0D:  327F A67F  VCTR  s=3  dx=-639  dy=639   z=10
941E/A0F:  3000 A6BF  VCTR  s=3  dx=-703  dy=0     z=10
9422/A11:  367F A67F  VCTR  s=3  dx=-639  dy=-639  z=10
9426/A13:  36BF A000  VCTR  s=3  dx=0     dy=-703  z=10
942A/A15:  367F A27F  VCTR  s=3  dx=639   dy=-639  z=10
942E/A17:  3000 A2BF  VCTR  s=3  dx=703   dy=0     z=10
9432/A19:  327F A27F  VCTR  s=3  dx=639   dy=639   z=10
9436/A1B:  429F C53F  VCTR  s=4  dx=-319  dy=671   z=12
943A/A1D:  453F C69F  VCTR  s=4  dx=-671  dy=-319  z=12
943E/A1F:  469F C13F  VCTR  s=4  dx=319   dy=-671  z=12
9442/A21:  413F C29F  VCTR  s=4  dx=671   dy=319   z=12
9446/A23:  32BF A000  VCTR  s=3  dx=0     dy=703   z=10
944A/A25:  413F C69F  VCTR  s=4  dx=-671  dy=319   z=12
944E/A27:  469F C53F  VCTR  s=4  dx=-319  dy=-671  z=12
9452/A29:  453F C29F  VCTR  s=4  dx=671   dy=-319  z=12
9456/A2B:  429F C13F  VCTR  s=4  dx=319   dy=671   z=12
945A/A2D:  D000       RTSL

DROID_00: (JSRL operand $A2E)
; refs: anim tbl 3F4D[1] = droid anim 1 (also title shimmer)
945C/A2E:  317F 03FF  VCTR  s=3  dx=1023  dy=383   z=0
9460/A30:  327F 667F  VCTR  s=3  dx=-639  dy=639   z=6
9464/A32:  3000 66BF  VCTR  s=3  dx=-703  dy=0     z=6
9468/A34:  367F 667F  VCTR  s=3  dx=-639  dy=-639  z=6
946C/A36:  36BF 6000  VCTR  s=3  dx=0     dy=-703  z=6
9470/A38:  367F 627F  VCTR  s=3  dx=639   dy=-639  z=6
9474/A3A:  3000 62BF  VCTR  s=3  dx=703   dy=0     z=6
9478/A3C:  327F 627F  VCTR  s=3  dx=639   dy=639   z=6
947C/A3E:  429F 853F  VCTR  s=4  dx=-319  dy=671   z=8
9480/A40:  453F 869F  VCTR  s=4  dx=-671  dy=-319  z=8
9484/A42:  469F 813F  VCTR  s=4  dx=319   dy=-671  z=8
9488/A44:  413F 829F  VCTR  s=4  dx=671   dy=319   z=8
948C/A46:  32BF 6000  VCTR  s=3  dx=0     dy=703   z=6
9490/A48:  413F 869F  VCTR  s=4  dx=-671  dy=319   z=8
9494/A4A:  469F 853F  VCTR  s=4  dx=-319  dy=-671  z=8
9498/A4C:  453F 829F  VCTR  s=4  dx=671   dy=-319  z=8
949C/A4E:  429F 813F  VCTR  s=4  dx=319   dy=671   z=8
94A0/A50:  D000       RTSL

DROID_01: (JSRL operand $A51)
; refs: anim tbl 3F4D[2] = droid anim 2 (also title shimmer)
94A2/A51:  327F 037F  VCTR  s=3  dx=895   dy=639   z=0
94A6/A53:  31BF 673F  VCTR  s=3  dx=-831  dy=447   z=6
94AA/A55:  34BF 667F  VCTR  s=3  dx=-639  dy=-191  z=6
94AE/A57:  373F 65BF  VCTR  s=3  dx=-447  dy=-831  z=6
94B2/A59:  367F 60BF  VCTR  s=3  dx=191   dy=-639  z=6
94B6/A5B:  35BF 633F  VCTR  s=3  dx=831   dy=-447  z=6
94BA/A5D:  30BF 627F  VCTR  s=3  dx=639   dy=191   z=6
94BE/A5F:  333F 61BF  VCTR  s=3  dx=447   dy=831   z=6
94C2/A61:  421F 85FF  VCTR  s=4  dx=-511  dy=543   z=8
94C6/A63:  45FF 861F  VCTR  s=4  dx=-543  dy=-511  z=8
94CA/A65:  461F 81FF  VCTR  s=4  dx=511   dy=-543  z=8
94CE/A67:  41FF 821F  VCTR  s=4  dx=543   dy=511   z=8
94D2/A69:  327F 64BF  VCTR  s=3  dx=-191  dy=639   z=6
94D6/A6B:  407F 86DF  VCTR  s=4  dx=-735  dy=127   z=8
94DA/A6D:  46DF 847F  VCTR  s=4  dx=-127  dy=-735  z=8
94DE/A6F:  447F 82DF  VCTR  s=4  dx=735   dy=-127  z=8
94E2/A71:  42DF 807F  VCTR  s=4  dx=127   dy=735   z=8
94E6/A73:  D000       RTSL

DROID_02: (JSRL operand $A74)
; refs: anim tbl 3F4D[3] = droid anim 3 (also title shimmer)
94E8/A74:  337F 02BF  VCTR  s=3  dx=703   dy=895   z=0
94EC/A76:  30BF 673F  VCTR  s=3  dx=-831  dy=191   z=6
94F0/A78:  357F 66BF  VCTR  s=3  dx=-703  dy=-383  z=6
94F4/A7A:  373F 64BF  VCTR  s=3  dx=-191  dy=-831  z=6
94F8/A7C:  36BF 617F  VCTR  s=3  dx=383   dy=-703  z=6
94FC/A7E:  34BF 633F  VCTR  s=3  dx=831   dy=-191  z=6
9500/A80:  317F 62BF  VCTR  s=3  dx=703   dy=383   z=6
9504/A82:  333F 60BF  VCTR  s=3  dx=191   dy=831   z=6
9508/A84:  41BF 865F  VCTR  s=4  dx=-607  dy=447   z=8
950C/A86:  465F 85BF  VCTR  s=4  dx=-447  dy=-607  z=8
9510/A88:  45BF 825F  VCTR  s=4  dx=607   dy=-447  z=8
9514/A8A:  425F 81BF  VCTR  s=4  dx=447   dy=607   z=8
9518/A8C:  32BF 657F  VCTR  s=3  dx=-383  dy=703   z=6
951C/A8E:  445F 86FF  VCTR  s=4  dx=-767  dy=-95   z=8
9520/A90:  46FF 805F  VCTR  s=4  dx=95    dy=-767  z=8
9524/A92:  405F 82FF  VCTR  s=4  dx=767   dy=95    z=8
9528/A94:  42FF 845F  VCTR  s=4  dx=-95   dy=767   z=8
952C/A96:  D000       RTSL

DEATH_SHIP_00: (JSRL operand $A97)
; refs: anim tbl 3F4D[4] = death-ship anim 0 (also title shimmer)
952E/A97:  12FF D2FF  VCTR  s=1  dx=767   dy=767   z=13
9532/A99:  34BF D33F  VCTR  s=3  dx=831   dy=-191  z=13
9536/A9B:  4000 D7DF  VCTR  s=4  dx=-991  dy=0     z=13
953A/A9D:  347F D33F  VCTR  s=3  dx=831   dy=-127  z=13
953E/A9F:  F1D1       SVEC  s=2  dx=256   dy=256   z=13
9540/AA0:  F2D8       SVEC  s=4  dx=0     dy=512   z=13
9542/AA1:  373F D47F  VCTR  s=3  dx=-127  dy=-831  z=13
9546/AA3:  267F D27F  VCTR  s=2  dx=639   dy=-639  z=13
954A/AA5:  373F D4BF  VCTR  s=3  dx=-191  dy=-831  z=13
954E/AA7:  33BF D000  VCTR  s=3  dx=0     dy=959   z=13
9552/AA9:  EBA7       JMPL  $974E   ; -> PHOTON_MINE

DEATH_SHIP_01: (JSRL operand $AAA)
; refs: anim tbl 3F4D[5] = death-ship anim 1 (also title shimmer)
9554/AAA:  F2D1       SVEC  s=2  dx=256   dy=512   z=13
9556/AAB:  307F D33F  VCTR  s=3  dx=831   dy=127   z=13
955A/AAD:  455F D79F  VCTR  s=4  dx=-927  dy=-351  z=13
955E/AAF:  307F D33F  VCTR  s=3  dx=831   dy=127   z=13
9562/AB1:  12FF D0FF  VCTR  s=1  dx=255   dy=767   z=13
9566/AB3:  33BF D53F  VCTR  s=3  dx=-319  dy=959   z=13
956A/AB5:  373F D07F  VCTR  s=3  dx=127   dy=-831  z=13
956E/AB7:  257F D37F  VCTR  s=2  dx=895   dy=-383  z=13
9572/AB9:  373F D07F  VCTR  s=3  dx=127   dy=-831  z=13
9576/ABB:  337F D57F  VCTR  s=3  dx=-383  dy=895   z=13
957A/ABD:  EBA7       JMPL  $974E   ; -> PHOTON_MINE

DEATH_SHIP_02: (JSRL operand $ABE)
; refs: anim tbl 3F4D[6] = death-ship anim 2 (also title shimmer)
957C/ABE:  F2D0       SVEC  s=2  dx=0     dy=512   z=13
957E/ABF:  31BF D2BF  VCTR  s=3  dx=703   dy=447   z=13
9582/AC1:  469F D69F  VCTR  s=4  dx=-671  dy=-671  z=13
9586/AC3:  31BF D27F  VCTR  s=3  dx=639   dy=447   z=13
958A/AC5:  12FF D000  VCTR  s=1  dx=0     dy=767   z=13
958E/AC7:  32BF D67F  VCTR  s=3  dx=-639  dy=703   z=13
9592/AC9:  36BF D1BF  VCTR  s=3  dx=447   dy=-703  z=13
9596/ACB:  2000 D37F  VCTR  s=2  dx=895   dy=0     z=13
959A/ACD:  367F D1BF  VCTR  s=3  dx=447   dy=-639  z=13
959E/ACF:  327F D6BF  VCTR  s=3  dx=-703  dy=639   z=13
95A2/AD1:  EBA7       JMPL  $974E   ; -> PHOTON_MINE

DEATH_SHIP_03: (JSRL operand $AD2)
; refs: anim tbl 3F4D[7] = death-ship anim 3 (also title shimmer)
95A4/AD2:  13FF D4FF  VCTR  s=1  dx=-255  dy=1023  z=13
95A8/AD4:  32BF D1BF  VCTR  s=3  dx=447   dy=703   z=13
95AC/AD6:  479F D55F  VCTR  s=4  dx=-351  dy=-927  z=13
95B0/AD8:  32BF D1BF  VCTR  s=3  dx=447   dy=703   z=13
95B4/ADA:  12FF D5FF  VCTR  s=1  dx=-511  dy=767   z=13
95B8/ADC:  317F D77F  VCTR  s=3  dx=-895  dy=383   z=13
95BC/ADE:  35BF D2BF  VCTR  s=3  dx=703   dy=-447  z=13
95C0/AE0:  217F D37F  VCTR  s=2  dx=895   dy=383   z=13
95C4/AE2:  35BF D2BF  VCTR  s=3  dx=703   dy=-447  z=13
95C8/AE4:  313F D7BF  VCTR  s=3  dx=-959  dy=319   z=13
95CC/AE6:  EBA7       JMPL  $974E   ; -> PHOTON_MINE

SHOT_00: (JSRL operand $AE7)
; shot/mine at aim angle 0: word $AE7 + 3*0 (SPAWN_MINE_SHOT 0x2737)
95CE/AE7:  3000 F7BF  VCTR  s=3  dx=-959  dy=0     z=15
95D2/AE9:  D000       RTSL

SHOT_01: (JSRL operand $AEA)
; shot/mine at aim angle 1: word $AE7 + 3*1 (SPAWN_MINE_SHOT 0x2737)
95D4/AEA:  343F F77F  VCTR  s=3  dx=-895  dy=-63   z=15
95D8/AEC:  D000       RTSL

SHOT_02: (JSRL operand $AED)
; shot/mine at aim angle 2: word $AE7 + 3*2 (SPAWN_MINE_SHOT 0x2737)
95DA/AED:  347F F77F  VCTR  s=3  dx=-895  dy=-127  z=15
95DE/AEF:  D000       RTSL

SHOT_03: (JSRL operand $AF0)
; shot/mine at aim angle 3: word $AE7 + 3*3 (SPAWN_MINE_SHOT 0x2737)
95E0/AF0:  34FF F73F  VCTR  s=3  dx=-831  dy=-255  z=15
95E4/AF2:  D000       RTSL

SHOT_04: (JSRL operand $AF3)
; shot/mine at aim angle 4: word $AE7 + 3*4 (SPAWN_MINE_SHOT 0x2737)
95E6/AF3:  353F F73F  VCTR  s=3  dx=-831  dy=-319  z=15
95EA/AF5:  D000       RTSL

SHOT_05: (JSRL operand $AF6)
; shot/mine at aim angle 5: word $AE7 + 3*5 (SPAWN_MINE_SHOT 0x2737)
95EC/AF6:  35BF F6FF  VCTR  s=3  dx=-767  dy=-447  z=15
95F0/AF8:  D000       RTSL

SHOT_06: (JSRL operand $AF9)
; shot/mine at aim angle 6: word $AE7 + 3*6 (SPAWN_MINE_SHOT 0x2737)
95F2/AF9:  35FF F6BF  VCTR  s=3  dx=-703  dy=-511  z=15
95F6/AFB:  D000       RTSL

SHOT_07: (JSRL operand $AFC)
; shot/mine at aim angle 7: word $AE7 + 3*7 (SPAWN_MINE_SHOT 0x2737)
95F8/AFC:  35FF F67F  VCTR  s=3  dx=-639  dy=-511  z=15
95FC/AFE:  D000       RTSL

SHOT_08: (JSRL operand $AFF)
; shot/mine at aim angle 8: word $AE7 + 3*8 (SPAWN_MINE_SHOT 0x2737)
95FE/AFF:  363F F63F  VCTR  s=3  dx=-575  dy=-575  z=15
9602/B01:  D000       RTSL

SHOT_09: (JSRL operand $B02)
; shot/mine at aim angle 9: word $AE7 + 3*9 (SPAWN_MINE_SHOT 0x2737)
9604/B02:  367F F5FF  VCTR  s=3  dx=-511  dy=-639  z=15
9608/B04:  D000       RTSL

SHOT_10: (JSRL operand $B05)
; shot/mine at aim angle 10: word $AE7 + 3*10 (SPAWN_MINE_SHOT 0x2737)
960A/B05:  36BF F5BF  VCTR  s=3  dx=-447  dy=-703  z=15
960E/B07:  D000       RTSL

SHOT_11: (JSRL operand $B08)
; shot/mine at aim angle 11: word $AE7 + 3*11 (SPAWN_MINE_SHOT 0x2737)
9610/B08:  36FF F53F  VCTR  s=3  dx=-319  dy=-767  z=15
9614/B0A:  D000       RTSL

SHOT_12: (JSRL operand $B0B)
; shot/mine at aim angle 12: word $AE7 + 3*12 (SPAWN_MINE_SHOT 0x2737)
9616/B0B:  373F F4FF  VCTR  s=3  dx=-255  dy=-831  z=15
961A/B0D:  D000       RTSL

SHOT_13: (JSRL operand $B0E)
; shot/mine at aim angle 13: word $AE7 + 3*13 (SPAWN_MINE_SHOT 0x2737)
961C/B0E:  377F F47F  VCTR  s=3  dx=-127  dy=-895  z=15
9620/B10:  D000       RTSL

SHOT_14: (JSRL operand $B11)
; shot/mine at aim angle 14: word $AE7 + 3*14 (SPAWN_MINE_SHOT 0x2737)
9622/B11:  377F F43F  VCTR  s=3  dx=-63   dy=-895  z=15
9626/B13:  D000       RTSL

SHOT_15: (JSRL operand $B14)
; shot/mine at aim angle 15: word $AE7 + 3*15 (SPAWN_MINE_SHOT 0x2737)
9628/B14:  37BF F000  VCTR  s=3  dx=0     dy=-959  z=15
962C/B16:  D000       RTSL

SHOT_16: (JSRL operand $B17)
; shot/mine at aim angle 16: word $AE7 + 3*16 (SPAWN_MINE_SHOT 0x2737)
962E/B17:  37BF F03F  VCTR  s=3  dx=63    dy=-959  z=15
9632/B19:  D000       RTSL

SHOT_17: (JSRL operand $B1A)
; shot/mine at aim angle 17: word $AE7 + 3*17 (SPAWN_MINE_SHOT 0x2737)
9634/B1A:  377F F07F  VCTR  s=3  dx=127   dy=-895  z=15
9638/B1C:  D000       RTSL

SHOT_18: (JSRL operand $B1D)
; shot/mine at aim angle 18: word $AE7 + 3*18 (SPAWN_MINE_SHOT 0x2737)
963A/B1D:  377F F0BF  VCTR  s=3  dx=191   dy=-895  z=15
963E/B1F:  D000       RTSL

SHOT_19: (JSRL operand $B20)
; shot/mine at aim angle 19: word $AE7 + 3*19 (SPAWN_MINE_SHOT 0x2737)
9640/B20:  373F F13F  VCTR  s=3  dx=319   dy=-831  z=15
9644/B22:  D000       RTSL

SHOT_20: (JSRL operand $B23)
; shot/mine at aim angle 20: word $AE7 + 3*20 (SPAWN_MINE_SHOT 0x2737)
9646/B23:  36FF F17F  VCTR  s=3  dx=383   dy=-767  z=15
964A/B25:  D000       RTSL

SHOT_21: (JSRL operand $B26)
; shot/mine at aim angle 21: word $AE7 + 3*21 (SPAWN_MINE_SHOT 0x2737)
964C/B26:  36BF F1FF  VCTR  s=3  dx=511   dy=-703  z=15
9650/B28:  D000       RTSL

SHOT_22: (JSRL operand $B29)
; shot/mine at aim angle 22: word $AE7 + 3*22 (SPAWN_MINE_SHOT 0x2737)
9652/B29:  367F F23F  VCTR  s=3  dx=575   dy=-639  z=15
9656/B2B:  D000       RTSL

SHOT_23: (JSRL operand $B2C)
; shot/mine at aim angle 23: word $AE7 + 3*23 (SPAWN_MINE_SHOT 0x2737)
9658/B2C:  363F F27F  VCTR  s=3  dx=639   dy=-575  z=15
965C/B2E:  D000       RTSL

SHOT_24: (JSRL operand $B2F)
; shot/mine at aim angle 24: word $AE7 + 3*24 (SPAWN_MINE_SHOT 0x2737)
965E/B2F:  35FF F2BF  VCTR  s=3  dx=703   dy=-511  z=15
9662/B31:  D000       RTSL

SHOT_25: (JSRL operand $B32)
; shot/mine at aim angle 25: word $AE7 + 3*25 (SPAWN_MINE_SHOT 0x2737)
9664/B32:  35FF F2FF  VCTR  s=3  dx=767   dy=-511  z=15
9668/B34:  D000       RTSL

SHOT_26: (JSRL operand $B35)
; shot/mine at aim angle 26: word $AE7 + 3*26 (SPAWN_MINE_SHOT 0x2737)
966A/B35:  35BF F33F  VCTR  s=3  dx=831   dy=-447  z=15
966E/B37:  D000       RTSL

SHOT_27: (JSRL operand $B38)
; shot/mine at aim angle 27: word $AE7 + 3*27 (SPAWN_MINE_SHOT 0x2737)
9670/B38:  353F F37F  VCTR  s=3  dx=895   dy=-319  z=15
9674/B3A:  D000       RTSL

SHOT_28: (JSRL operand $B3B)
; shot/mine at aim angle 28: word $AE7 + 3*28 (SPAWN_MINE_SHOT 0x2737)
9676/B3B:  34FF F37F  VCTR  s=3  dx=895   dy=-255  z=15
967A/B3D:  D000       RTSL

SHOT_29: (JSRL operand $B3E)
; shot/mine at aim angle 29: word $AE7 + 3*29 (SPAWN_MINE_SHOT 0x2737)
967C/B3E:  347F F3BF  VCTR  s=3  dx=959   dy=-127  z=15
9680/B40:  D000       RTSL

SHOT_30: (JSRL operand $B41)
; shot/mine at aim angle 30: word $AE7 + 3*30 (SPAWN_MINE_SHOT 0x2737)
9682/B41:  343F F3BF  VCTR  s=3  dx=959   dy=-63   z=15
9686/B43:  D000       RTSL

SHOT_31: (JSRL operand $B44)
; shot/mine at aim angle 31: word $AE7 + 3*31 (SPAWN_MINE_SHOT 0x2737)
9688/B44:  3000 F3FF  VCTR  s=3  dx=1023  dy=0     z=15
968C/B46:  D000       RTSL

SHOT_32: (JSRL operand $B47)
; shot/mine at aim angle 32: word $AE7 + 3*32 (SPAWN_MINE_SHOT 0x2737)
968E/B47:  3000 F3FF  VCTR  s=3  dx=1023  dy=0     z=15
9692/B49:  D000       RTSL

SHOT_33: (JSRL operand $B4A)
; shot/mine at aim angle 33: word $AE7 + 3*33 (SPAWN_MINE_SHOT 0x2737)
9694/B4A:  307F F3BF  VCTR  s=3  dx=959   dy=127   z=15
9698/B4C:  D000       RTSL

SHOT_34: (JSRL operand $B4D)
; shot/mine at aim angle 34: word $AE7 + 3*34 (SPAWN_MINE_SHOT 0x2737)
969A/B4D:  30BF F3BF  VCTR  s=3  dx=959   dy=191   z=15
969E/B4F:  D000       RTSL

SHOT_35: (JSRL operand $B50)
; shot/mine at aim angle 35: word $AE7 + 3*35 (SPAWN_MINE_SHOT 0x2737)
96A0/B50:  313F F37F  VCTR  s=3  dx=895   dy=319   z=15
96A4/B52:  D000       RTSL

SHOT_36: (JSRL operand $B53)
; shot/mine at aim angle 36: word $AE7 + 3*36 (SPAWN_MINE_SHOT 0x2737)
96A6/B53:  317F F37F  VCTR  s=3  dx=895   dy=383   z=15
96AA/B55:  D000       RTSL

SHOT_37: (JSRL operand $B56)
; shot/mine at aim angle 37: word $AE7 + 3*37 (SPAWN_MINE_SHOT 0x2737)
96AC/B56:  31FF F33F  VCTR  s=3  dx=831   dy=511   z=15
96B0/B58:  D000       RTSL

SHOT_38: (JSRL operand $B59)
; shot/mine at aim angle 38: word $AE7 + 3*38 (SPAWN_MINE_SHOT 0x2737)
96B2/B59:  323F F2FF  VCTR  s=3  dx=767   dy=575   z=15
96B6/B5B:  D000       RTSL

SHOT_39: (JSRL operand $B5C)
; shot/mine at aim angle 39: word $AE7 + 3*39 (SPAWN_MINE_SHOT 0x2737)
96B8/B5C:  323F F2BF  VCTR  s=3  dx=703   dy=575   z=15
96BC/B5E:  D000       RTSL

SHOT_40: (JSRL operand $B5F)
; shot/mine at aim angle 40: word $AE7 + 3*40 (SPAWN_MINE_SHOT 0x2737)
96BE/B5F:  327F F27F  VCTR  s=3  dx=639   dy=639   z=15
96C2/B61:  D000       RTSL

SHOT_41: (JSRL operand $B62)
; shot/mine at aim angle 41: word $AE7 + 3*41 (SPAWN_MINE_SHOT 0x2737)
96C4/B62:  32BF F23F  VCTR  s=3  dx=575   dy=703   z=15
96C8/B64:  D000       RTSL

SHOT_42: (JSRL operand $B65)
; shot/mine at aim angle 42: word $AE7 + 3*42 (SPAWN_MINE_SHOT 0x2737)
96CA/B65:  32FF F1FF  VCTR  s=3  dx=511   dy=767   z=15
96CE/B67:  D000       RTSL

SHOT_43: (JSRL operand $B68)
; shot/mine at aim angle 43: word $AE7 + 3*43 (SPAWN_MINE_SHOT 0x2737)
96D0/B68:  333F F17F  VCTR  s=3  dx=383   dy=831   z=15
96D4/B6A:  D000       RTSL

SHOT_44: (JSRL operand $B6B)
; shot/mine at aim angle 44: word $AE7 + 3*44 (SPAWN_MINE_SHOT 0x2737)
96D6/B6B:  337F F13F  VCTR  s=3  dx=319   dy=895   z=15
96DA/B6D:  D000       RTSL

SHOT_45: (JSRL operand $B6E)
; shot/mine at aim angle 45: word $AE7 + 3*45 (SPAWN_MINE_SHOT 0x2737)
96DC/B6E:  33BF F0BF  VCTR  s=3  dx=191   dy=959   z=15
96E0/B70:  D000       RTSL

SHOT_46: (JSRL operand $B71)
; shot/mine at aim angle 46: word $AE7 + 3*46 (SPAWN_MINE_SHOT 0x2737)
96E2/B71:  33BF F07F  VCTR  s=3  dx=127   dy=959   z=15
96E6/B73:  D000       RTSL

SHOT_47: (JSRL operand $B74)
; shot/mine at aim angle 47: word $AE7 + 3*47 (SPAWN_MINE_SHOT 0x2737)
96E8/B74:  33FF F000  VCTR  s=3  dx=0     dy=1023  z=15
96EC/B76:  D000       RTSL

SHOT_48: (JSRL operand $B77)
; shot/mine at aim angle 48: word $AE7 + 3*48 (SPAWN_MINE_SHOT 0x2737)
96EE/B77:  33FF F000  VCTR  s=3  dx=0     dy=1023  z=15
96F2/B79:  D000       RTSL

SHOT_49: (JSRL operand $B7A)
; shot/mine at aim angle 49: word $AE7 + 3*49 (SPAWN_MINE_SHOT 0x2737)
96F4/B7A:  33BF F43F  VCTR  s=3  dx=-63   dy=959   z=15
96F8/B7C:  D000       RTSL

SHOT_50: (JSRL operand $B7D)
; shot/mine at aim angle 50: word $AE7 + 3*50 (SPAWN_MINE_SHOT 0x2737)
96FA/B7D:  33BF F47F  VCTR  s=3  dx=-127  dy=959   z=15
96FE/B7F:  D000       RTSL

SHOT_51: (JSRL operand $B80)
; shot/mine at aim angle 51: word $AE7 + 3*51 (SPAWN_MINE_SHOT 0x2737)
9700/B80:  337F F4FF  VCTR  s=3  dx=-255  dy=895   z=15
9704/B82:  D000       RTSL

SHOT_52: (JSRL operand $B83)
; shot/mine at aim angle 52: word $AE7 + 3*52 (SPAWN_MINE_SHOT 0x2737)
9706/B83:  333F F53F  VCTR  s=3  dx=-319  dy=831   z=15
970A/B85:  D000       RTSL

SHOT_53: (JSRL operand $B86)
; shot/mine at aim angle 53: word $AE7 + 3*53 (SPAWN_MINE_SHOT 0x2737)
970C/B86:  32FF F5BF  VCTR  s=3  dx=-447  dy=767   z=15
9710/B88:  D000       RTSL

SHOT_54: (JSRL operand $B89)
; shot/mine at aim angle 54: word $AE7 + 3*54 (SPAWN_MINE_SHOT 0x2737)
9712/B89:  32BF F5FF  VCTR  s=3  dx=-511  dy=703   z=15
9716/B8B:  D000       RTSL

SHOT_55: (JSRL operand $B8C)
; shot/mine at aim angle 55: word $AE7 + 3*55 (SPAWN_MINE_SHOT 0x2737)
9718/B8C:  327F F63F  VCTR  s=3  dx=-575  dy=639   z=15
971C/B8E:  D000       RTSL

SHOT_56: (JSRL operand $B8F)
; shot/mine at aim angle 56: word $AE7 + 3*56 (SPAWN_MINE_SHOT 0x2737)
971E/B8F:  323F F67F  VCTR  s=3  dx=-639  dy=575   z=15
9722/B91:  D000       RTSL

SHOT_57: (JSRL operand $B92)
; shot/mine at aim angle 57: word $AE7 + 3*57 (SPAWN_MINE_SHOT 0x2737)
9724/B92:  323F F6BF  VCTR  s=3  dx=-703  dy=575   z=15
9728/B94:  D000       RTSL

SHOT_58: (JSRL operand $B95)
; shot/mine at aim angle 58: word $AE7 + 3*58 (SPAWN_MINE_SHOT 0x2737)
972A/B95:  31FF F6FF  VCTR  s=3  dx=-767  dy=511   z=15
972E/B97:  D000       RTSL

SHOT_59: (JSRL operand $B98)
; shot/mine at aim angle 59: word $AE7 + 3*59 (SPAWN_MINE_SHOT 0x2737)
9730/B98:  317F F73F  VCTR  s=3  dx=-831  dy=383   z=15
9734/B9A:  D000       RTSL

SHOT_60: (JSRL operand $B9B)
; shot/mine at aim angle 60: word $AE7 + 3*60 (SPAWN_MINE_SHOT 0x2737)
9736/B9B:  313F F73F  VCTR  s=3  dx=-831  dy=319   z=15
973A/B9D:  D000       RTSL

SHOT_61: (JSRL operand $B9E)
; shot/mine at aim angle 61: word $AE7 + 3*61 (SPAWN_MINE_SHOT 0x2737)
973C/B9E:  30BF F77F  VCTR  s=3  dx=-895  dy=191   z=15
9740/BA0:  D000       RTSL

SHOT_62: (JSRL operand $BA1)
; shot/mine at aim angle 62: word $AE7 + 3*62 (SPAWN_MINE_SHOT 0x2737)
9742/BA1:  307F F77F  VCTR  s=3  dx=-895  dy=127   z=15
9746/BA3:  D000       RTSL

SHOT_63: (JSRL operand $BA4)
; shot/mine at aim angle 63: word $AE7 + 3*63 (SPAWN_MINE_SHOT 0x2737)
9748/BA4:  3000 F7BF  VCTR  s=3  dx=-959  dy=0     z=15
974C/BA6:  D000       RTSL

PHOTON_MINE: (JSRL operand $BA7)
; refs: glyph tbl 3FCD+1E
; refs: COMMAND template shape word (2F95+21 = CBA7)
; also JSRL'd from within the vector ROM
974E/BA7:  357F 02FF  VCTR  s=3  dx=767   dy=-383  z=0
9752/BA9:  33FF F6FF  VCTR  s=3  dx=-767  dy=1023  z=15
9756/BAB:  37FF F6FF  VCTR  s=3  dx=-767  dy=-1023 z=15
975A/BAD:  F0FB       SVEC  s=4  dx=768   dy=0     z=15
975C/BAE:  D000       RTSL

VAPOR_MINE: (JSRL operand $BAF)
; refs: glyph tbl 3FCD+24
; refs: cs_hit_confirm mutate word CBAF (0x222A)
975E/BAF:  F803       SVEC  s=3  dx=768   dy=0     z=0
9760/BB0:  33FF F6FF  VCTR  s=3  dx=-767  dy=1023  z=15
9764/BB2:  37FF F6FF  VCTR  s=3  dx=-767  dy=-1023 z=15
9768/BB4:  37FF F2FF  VCTR  s=3  dx=767   dy=-1023 z=15
976C/BB6:  33FF F2FF  VCTR  s=3  dx=767   dy=1023  z=15
9770/BB8:  F0FF       SVEC  s=4  dx=-768  dy=0     z=15
9772/BB9:  D000       RTSL

SHIP_EXPLODE_00: (JSRL operand $BBA)
; refs: explosion anim 3F5D[1]
; refs: INERT template shape word (2FAC+21 = CBBA)
9774/BBA:  32BF F000  VCTR  s=3  dx=0     dy=703   z=15
9778/BBC:  46BF F000  VCTR  s=4  dx=0     dy=-703  z=15
977C/BBE:  21FF 067F  VCTR  s=2  dx=-639  dy=511   z=0
9780/BC0:  33BF F23F  VCTR  s=3  dx=575   dy=959   z=15
9784/BC2:  F600       SVEC  s=2  dx=0     dy=-512  z=0
9786/BC3:  FEF6       SVEC  s=3  dx=-512  dy=-512  z=15
9788/BC4:  20FF 067F  VCTR  s=2  dx=-639  dy=255   z=0
978C/BC6:  40DF F25F  VCTR  s=4  dx=607   dy=223   z=15
9790/BC8:  267F 00FF  VCTR  s=2  dx=255   dy=-639  z=0
9794/BCA:  4000 F6BF  VCTR  s=4  dx=-703  dy=0     z=15
9798/BCC:  13FF 04FF  VCTR  s=1  dx=-255  dy=1023  z=0
979C/BCE:  44FF F29F  VCTR  s=4  dx=671   dy=-255  z=15
97A0/BD0:  F700       SVEC  s=2  dx=0     dy=-768  z=0
97A2/BD1:  337F F77F  VCTR  s=3  dx=-895  dy=895   z=15
97A6/BD3:  227F 0000  VCTR  s=2  dx=0     dy=639   z=0
97AA/BD5:  37FF F1BF  VCTR  s=3  dx=447   dy=-1023 z=15
97AE/BD7:  D000       RTSL

SHIP_EXPLODE_01: (JSRL operand $BD8)
; refs: explosion anim 3F5D[2]
97B0/BD8:  32BF 0000  VCTR  s=3  dx=0     dy=703   z=0
97B4/BDA:  327F 5000  VCTR  s=3  dx=0     dy=639   z=5
97B8/BDC:  267F 03FF  VCTR  s=2  dx=1023  dy=-639  z=0
97BC/BDE:  FE55       SVEC  s=3  dx=-256  dy=-512  z=5
97BE/BDF:  F600       SVEC  s=2  dx=0     dy=-512  z=0
97C0/BE0:  FB53       SVEC  s=3  dx=768   dy=768   z=5
97C2/BE1:  36BF 057F  VCTR  s=3  dx=-383  dy=-703  z=0
97C6/BE3:  317F 52FF  VCTR  s=3  dx=767   dy=383   z=5
97CA/BE5:  36BF 047F  VCTR  s=3  dx=-127  dy=-703  z=0
97CE/BE7:  F856       SVEC  s=3  dx=-512  dy=0     z=5
97D0/BE8:  F605       SVEC  s=2  dx=-256  dy=-512  z=0
97D2/BE9:  257F 53FF  VCTR  s=2  dx=1023  dy=-383  z=5
97D6/BEB:  373F 007F  VCTR  s=3  dx=127   dy=-831  z=0
97DA/BED:  327F 567F  VCTR  s=3  dx=-639  dy=639   z=5
97DE/BEF:  217F 077F  VCTR  s=2  dx=-895  dy=383   z=0
97E2/BF1:  363F 50FF  VCTR  s=3  dx=255   dy=-575  z=5
97E6/BF3:  25FF 077F  VCTR  s=2  dx=-895  dy=-511  z=0
97EA/BF5:  FA50       SVEC  s=3  dx=0     dy=512   z=5
97EC/BF6:  227F 067F  VCTR  s=2  dx=-639  dy=639   z=0
97F0/BF8:  277F 567F  VCTR  s=2  dx=-639  dy=-895  z=5
97F4/BFA:  217F 07FF  VCTR  s=2  dx=-1023 dy=383   z=0
97F8/BFC:  31FF 533F  VCTR  s=3  dx=831   dy=511   z=5
97FC/BFE:  10FF 07FF  VCTR  s=1  dx=-1023 dy=255   z=0
9800/C00:  24FF 577F  VCTR  s=2  dx=-895  dy=-255  z=5
9804/C02:  21FF 077F  VCTR  s=2  dx=-895  dy=511   z=0
9808/C04:  3000 533F  VCTR  s=3  dx=831   dy=0     z=5
980C/C06:  13FF 04FF  VCTR  s=1  dx=-255  dy=1023  z=0
9810/C08:  217F 57FF  VCTR  s=2  dx=-1023 dy=383   z=5
9814/C0A:  FA01       SVEC  s=3  dx=256   dy=512   z=0
9816/C0B:  36BF 52BF  VCTR  s=3  dx=703   dy=-703  z=5
981A/C0D:  227F 0000  VCTR  s=2  dx=0     dy=639   z=0
981E/C0F:  237F 557F  VCTR  s=2  dx=-383  dy=895   z=5
9822/C11:  D000       RTSL

SHIP_EXPLODE_02: (JSRL operand $C12)
; refs: explosion anim 3F5D[0]
; refs: explosion anim 3F5D[3]
9824/C12:  429F 0000  VCTR  s=4  dx=0     dy=671   z=0
9828/C14:  32BF F000  VCTR  s=3  dx=0     dy=703   z=15
982C/C16:  FE03       SVEC  s=3  dx=768   dy=-512  z=0
982E/C17:  FEF5       SVEC  s=3  dx=-256  dy=-512  z=15
9830/C18:  F802       SVEC  s=3  dx=512   dy=0     z=0
9832/C19:  32FF F27F  VCTR  s=3  dx=639   dy=767   z=15
9836/C1B:  36FF 017F  VCTR  s=3  dx=383   dy=-767  z=0
983A/C1D:  353F F67F  VCTR  s=3  dx=-639  dy=-319  z=15
983E/C1F:  36BF 047F  VCTR  s=3  dx=-127  dy=-703  z=0
9842/C21:  2000 F37F  VCTR  s=2  dx=895   dy=0     z=15
9846/C23:  FF01       SVEC  s=3  dx=256   dy=-768  z=0
9848/C24:  313F F73F  VCTR  s=3  dx=-831  dy=319   z=15
984C/C26:  36FF 003F  VCTR  s=3  dx=63    dy=-767  z=0
9850/C28:  267F F27F  VCTR  s=2  dx=639   dy=-639  z=15
9854/C2A:  357F 06BF  VCTR  s=3  dx=-703  dy=-383  z=0
9858/C2C:  337F F57F  VCTR  s=3  dx=-383  dy=895   z=15
985C/C2E:  25FF 077F  VCTR  s=2  dx=-895  dy=-511  z=0
9860/C30:  FFF0       SVEC  s=3  dx=0     dy=-768  z=15
9862/C31:  423F 053F  VCTR  s=4  dx=-319  dy=575   z=0
9866/C33:  277F F67F  VCTR  s=2  dx=-639  dy=-895  z=15
986A/C35:  30FF 073F  VCTR  s=3  dx=-831  dy=255   z=0
986E/C37:  317F F2BF  VCTR  s=3  dx=703   dy=383   z=15
9872/C39:  237F 007F  VCTR  s=2  dx=127   dy=895   z=0
9876/C3B:  34FF F7FF  VCTR  s=3  dx=-1023 dy=-255  z=15
987A/C3D:  FA01       SVEC  s=3  dx=256   dy=512   z=0
987C/C3E:  F0F3       SVEC  s=2  dx=768   dy=0     z=15
987E/C3F:  237F 017F  VCTR  s=2  dx=383   dy=895   z=0
9882/C41:  F1F7       SVEC  s=2  dx=-768  dy=256   z=15
9884/C42:  427F 047F  VCTR  s=4  dx=-127  dy=639   z=0
9888/C44:  377F F37F  VCTR  s=3  dx=895   dy=-895  z=15
988C/C46:  207F 03FF  VCTR  s=2  dx=1023  dy=127   z=0
9890/C48:  327F F53F  VCTR  s=3  dx=-319  dy=639   z=15
9894/C4A:  473F 017F  VCTR  s=4  dx=383   dy=-831  z=0
9898/C4C:  D000       RTSL

CHAR_A: (JSRL operand $C4D)
; text glyph (CHAR_GLYPH_EMIT 0x2C36)
; refs: letter tbl 3A2E 'A'
; refs: letter wheel 3A0E+5E
989A/C4D:  FA90       SVEC  s=3  dx=0     dy=512   z=9
989C/C4E:  F191       SVEC  s=2  dx=256   dy=256   z=9
989E/C4F:  F092       SVEC  s=2  dx=512   dy=0     z=9
98A0/C50:  F591       SVEC  s=2  dx=256   dy=-256  z=9
98A2/C51:  FE90       SVEC  s=3  dx=0     dy=-512  z=9
98A4/C52:  22FF 07FF  VCTR  s=2  dx=-1023 dy=767   z=0
98A8/C54:  F892       SVEC  s=3  dx=512   dy=0     z=9
98AA/C55:  26FF 03FF  VCTR  s=2  dx=1023  dy=-767  z=0
98AE/C57:  D000       RTSL

CHAR_B: (JSRL operand $C58)
; text glyph (CHAR_GLYPH_EMIT 0x2C36)
; refs: letter tbl 3A2E 'B'
98B0/C58:  327F 9000  VCTR  s=3  dx=0     dy=639   z=9
98B4/C5A:  F093       SVEC  s=2  dx=768   dy=0     z=9
98B6/C5B:  F591       SVEC  s=2  dx=256   dy=-256  z=9
98B8/C5C:  F595       SVEC  s=2  dx=-256  dy=-256  z=9
98BA/C5D:  F097       SVEC  s=2  dx=-768  dy=0     z=9
98BC/C5E:  F003       SVEC  s=2  dx=768   dy=0     z=0
98BE/C5F:  F691       SVEC  s=2  dx=256   dy=-512  z=9
98C0/C60:  F595       SVEC  s=2  dx=-256  dy=-256  z=9
98C2/C61:  F097       SVEC  s=2  dx=-768  dy=0     z=9
98C4/C62:  F00A       SVEC  s=4  dx=512   dy=0     z=0
98C6/C63:  D000       RTSL

CHAR_C: (JSRL operand $C64)
; text glyph (CHAR_GLYPH_EMIT 0x2C36)
; refs: letter tbl 3A2E 'C'
98C8/C64:  327F 9000  VCTR  s=3  dx=0     dy=639   z=9
98CC/C66:  F892       SVEC  s=3  dx=512   dy=0     z=9
98CE/C67:  367F 0000  VCTR  s=3  dx=0     dy=-639  z=0
98D2/C69:  F896       SVEC  s=3  dx=-512  dy=0     z=9
98D4/C6A:  F00A       SVEC  s=4  dx=512   dy=0     z=0
98D6/C6B:  D000       RTSL

CHAR_D: (JSRL operand $C6C)
; text glyph (CHAR_GLYPH_EMIT 0x2C36)
; refs: letter tbl 3A2E 'D'
; refs: letter wheel 3A0E+78
98D8/C6C:  327F 9000  VCTR  s=3  dx=0     dy=639   z=9
98DC/C6E:  F093       SVEC  s=2  dx=768   dy=0     z=9
98DE/C6F:  F591       SVEC  s=2  dx=256   dy=-256  z=9
98E0/C70:  F790       SVEC  s=2  dx=0     dy=-768  z=9
98E2/C71:  F595       SVEC  s=2  dx=-256  dy=-256  z=9
98E4/C72:  F097       SVEC  s=2  dx=-768  dy=0     z=9
98E6/C73:  F00A       SVEC  s=4  dx=512   dy=0     z=0
98E8/C74:  D000       RTSL

CHAR_E: (JSRL operand $C75)
; text glyph (CHAR_GLYPH_EMIT 0x2C36)
; refs: letter tbl 3A2E 'E'
; refs: letter wheel 3A0E+56
; refs: letter wheel 3A0E+66
; refs: letter wheel 3A0E+70
98EA/C75:  F892       SVEC  s=3  dx=512   dy=0     z=9
98EC/C76:  F806       SVEC  s=3  dx=-512  dy=0     z=0

CHAR_F: (JSRL operand $C77)
; (entered by fall-through: the previous shape runs into this one, so a linear sweep never sees a block start here)
; text glyph (CHAR_GLYPH_EMIT 0x2C36)
; refs: letter tbl 3A2E 'F'
98EE/C77:  327F 9000  VCTR  s=3  dx=0     dy=639   z=9
98F2/C79:  F892       SVEC  s=3  dx=512   dy=0     z=9
98F4/C7A:  F605       SVEC  s=2  dx=-256  dy=-512  z=0
98F6/C7B:  F097       SVEC  s=2  dx=-768  dy=0     z=9
98F8/C7C:  357F 03FF  VCTR  s=3  dx=1023  dy=-383  z=0
98FC/C7E:  D000       RTSL

CHAR_G: (JSRL operand $C7F)
; text glyph (CHAR_GLYPH_EMIT 0x2C36)
; refs: POST dlist 01A0+18
; refs: letter tbl 3A2E 'G'
98FE/C7F:  327F 9000  VCTR  s=3  dx=0     dy=639   z=9
9902/C81:  F892       SVEC  s=3  dx=512   dy=0     z=9
9904/C82:  F605       SVEC  s=2  dx=-256  dy=-512  z=0
9906/C83:  F091       SVEC  s=2  dx=256   dy=0     z=9
9908/C84:  F790       SVEC  s=2  dx=0     dy=-768  z=9
990A/C85:  F896       SVEC  s=3  dx=-512  dy=0     z=9
990C/C86:  F00A       SVEC  s=4  dx=512   dy=0     z=0
990E/C87:  D000       RTSL

CHAR_H: (JSRL operand $C88)
; text glyph (CHAR_GLYPH_EMIT 0x2C36)
; refs: POST input dlist 026E+16
; refs: letter tbl 3A2E 'H'
9910/C88:  327F 9000  VCTR  s=3  dx=0     dy=639   z=9
9914/C8A:  F600       SVEC  s=2  dx=0     dy=-512  z=0
9916/C8B:  F892       SVEC  s=3  dx=512   dy=0     z=9
9918/C8C:  F200       SVEC  s=2  dx=0     dy=512   z=0
991A/C8D:  367F 9000  VCTR  s=3  dx=0     dy=-639  z=9
991E/C8F:  F802       SVEC  s=3  dx=512   dy=0     z=0
9920/C90:  D000       RTSL

CHAR_I: (JSRL operand $C91)
; text glyph (CHAR_GLYPH_EMIT 0x2C36)
; refs: POST input dlist 026E+18
; refs: letter tbl 3A2E 'I'
9922/C91:  F001       SVEC  s=2  dx=256   dy=0     z=0
9924/C92:  F092       SVEC  s=2  dx=512   dy=0     z=9
9926/C93:  F005       SVEC  s=2  dx=-256  dy=0     z=0
9928/C94:  327F 9000  VCTR  s=3  dx=0     dy=639   z=9
992C/C96:  F005       SVEC  s=2  dx=-256  dy=0     z=0
992E/C97:  F092       SVEC  s=2  dx=512   dy=0     z=9
9930/C98:  367F 027F  VCTR  s=3  dx=639   dy=-639  z=0
9934/C9A:  D000       RTSL

CHAR_J: (JSRL operand $C9B)
; text glyph (CHAR_GLYPH_EMIT 0x2C36)
; refs: letter tbl 3A2E 'J'
9936/C9B:  F100       SVEC  s=2  dx=0     dy=256   z=0
9938/C9C:  F591       SVEC  s=2  dx=256   dy=-256  z=9
993A/C9D:  F093       SVEC  s=2  dx=768   dy=0     z=9
993C/C9E:  327F 9000  VCTR  s=3  dx=0     dy=639   z=9
9940/CA0:  367F 01FF  VCTR  s=3  dx=511   dy=-639  z=0
9944/CA2:  D000       RTSL

CHAR_K: (JSRL operand $CA3)
; text glyph (CHAR_GLYPH_EMIT 0x2C36)
; refs: POST dlist 01A0+10
; refs: letter tbl 3A2E 'K'
9946/CA3:  327F 9000  VCTR  s=3  dx=0     dy=639   z=9
994A/CA5:  F802       SVEC  s=3  dx=512   dy=0     z=0
994C/CA6:  FD96       SVEC  s=3  dx=-512  dy=-256  z=9
994E/CA7:  26FF 93FF  VCTR  s=2  dx=1023  dy=-767  z=9
9952/CA9:  F802       SVEC  s=3  dx=512   dy=0     z=0
9954/CAA:  D000       RTSL

CHAR_L: (JSRL operand $CAB)
; text glyph (CHAR_GLYPH_EMIT 0x2C36)
; refs: POST input dlist 026E+0C
; refs: letter tbl 3A2E 'L'
9956/CAB:  327F 0000  VCTR  s=3  dx=0     dy=639   z=0
995A/CAD:  367F 9000  VCTR  s=3  dx=0     dy=-639  z=9
995E/CAF:  F892       SVEC  s=3  dx=512   dy=0     z=9
9960/CB0:  F802       SVEC  s=3  dx=512   dy=0     z=0
9962/CB1:  D000       RTSL

CHAR_M: (JSRL operand $CB2)
; text glyph (CHAR_GLYPH_EMIT 0x2C36)
; refs: letter tbl 3A2E 'M'
9964/CB2:  327F 9000  VCTR  s=3  dx=0     dy=639   z=9
9968/CB4:  F692       SVEC  s=2  dx=512   dy=-512  z=9
996A/CB5:  F292       SVEC  s=2  dx=512   dy=512   z=9
996C/CB6:  367F 9000  VCTR  s=3  dx=0     dy=-639  z=9
9970/CB8:  F802       SVEC  s=3  dx=512   dy=0     z=0
9972/CB9:  D000       RTSL

CHAR_N: (JSRL operand $CBA)
; text glyph (CHAR_GLYPH_EMIT 0x2C36)
; refs: POST dlist 01A0+16
; refs: letter tbl 3A2E 'N'
; refs: letter wheel 3A0E+74
9974/CBA:  327F 9000  VCTR  s=3  dx=0     dy=639   z=9
9978/CBC:  FE92       SVEC  s=3  dx=512   dy=-512  z=9
997A/CBD:  FA00       SVEC  s=3  dx=0     dy=512   z=0
997C/CBE:  367F 9000  VCTR  s=3  dx=0     dy=-639  z=9
9980/CC0:  F802       SVEC  s=3  dx=512   dy=0     z=0
9982/CC1:  D000       RTSL

CHAR_O: (JSRL operand $CC2)
; text glyph (CHAR_GLYPH_EMIT 0x2C36)
; refs: POST dlist 01A0+0E
; refs: POST input dlist 026E+0E
; refs: letter tbl 3A2E 'O'
9984/CC2:  327F 9000  VCTR  s=3  dx=0     dy=639   z=9
9988/CC4:  F892       SVEC  s=3  dx=512   dy=0     z=9
998A/CC5:  367F 9000  VCTR  s=3  dx=0     dy=-639  z=9
998E/CC7:  F896       SVEC  s=3  dx=-512  dy=0     z=9
9990/CC8:  F00A       SVEC  s=4  dx=512   dy=0     z=0
9992/CC9:  D000       RTSL

CHAR_P: (JSRL operand $CCA)
; text glyph (CHAR_GLYPH_EMIT 0x2C36)
; refs: letter tbl 3A2E 'P'
9994/CCA:  327F 9000  VCTR  s=3  dx=0     dy=639   z=9
9998/CCC:  F892       SVEC  s=3  dx=512   dy=0     z=9
999A/CCD:  F690       SVEC  s=2  dx=0     dy=-512  z=9
999C/CCE:  F896       SVEC  s=3  dx=-512  dy=0     z=9
999E/CCF:  357F 03FF  VCTR  s=3  dx=1023  dy=-383  z=0
99A2/CD1:  D000       RTSL

CHAR_Q: (JSRL operand $CD2)
; text glyph (CHAR_GLYPH_EMIT 0x2C36)
; refs: letter tbl 3A2E 'Q'
99A4/CD2:  327F 9000  VCTR  s=3  dx=0     dy=639   z=9
99A8/CD4:  F892       SVEC  s=3  dx=512   dy=0     z=9
99AA/CD5:  FE90       SVEC  s=3  dx=0     dy=-512  z=9
99AC/CD6:  F595       SVEC  s=2  dx=-256  dy=-256  z=9
99AE/CD7:  F097       SVEC  s=2  dx=-768  dy=0     z=9
99B0/CD8:  F103       SVEC  s=2  dx=768   dy=256   z=0
99B2/CD9:  F591       SVEC  s=2  dx=256   dy=-256  z=9
99B4/CDA:  F802       SVEC  s=3  dx=512   dy=0     z=0
99B6/CDB:  D000       RTSL

CHAR_R: (JSRL operand $CDC)
; text glyph (CHAR_GLYPH_EMIT 0x2C36)
; refs: letter tbl 3A2E 'R'
; refs: letter wheel 3A0E+5A
99B8/CDC:  327F 9000  VCTR  s=3  dx=0     dy=639   z=9
99BC/CDE:  F892       SVEC  s=3  dx=512   dy=0     z=9
99BE/CDF:  267F 9000  VCTR  s=2  dx=0     dy=-639  z=9
99C2/CE1:  F896       SVEC  s=3  dx=-512  dy=0     z=9
99C4/CE2:  267F 93FF  VCTR  s=2  dx=1023  dy=-639  z=9
99C8/CE4:  F802       SVEC  s=3  dx=512   dy=0     z=0
99CA/CE5:  D000       RTSL

CHAR_S: (JSRL operand $CE6)
; text glyph (CHAR_GLYPH_EMIT 0x2C36)
; refs: letter tbl 3A2E 'S'
; refs: letter wheel 3A0E+62
99CC/CE6:  F892       SVEC  s=3  dx=512   dy=0     z=9
99CE/CE7:  227F 9000  VCTR  s=2  dx=0     dy=639   z=9
99D2/CE9:  F896       SVEC  s=3  dx=-512  dy=0     z=9
99D4/CEA:  227F 9000  VCTR  s=2  dx=0     dy=639   z=9
99D8/CEC:  F892       SVEC  s=3  dx=512   dy=0     z=9
99DA/CED:  367F 01FF  VCTR  s=3  dx=511   dy=-639  z=0
99DE/CEF:  D000       RTSL

CHAR_T: (JSRL operand $CF0)
; text glyph (CHAR_GLYPH_EMIT 0x2C36)
; refs: letter tbl 3A2E 'T'
99E0/CF0:  327F 0000  VCTR  s=3  dx=0     dy=639   z=0
99E4/CF2:  F892       SVEC  s=3  dx=512   dy=0     z=9
99E6/CF3:  F006       SVEC  s=2  dx=-512  dy=0     z=0
99E8/CF4:  367F 9000  VCTR  s=3  dx=0     dy=-639  z=9
99EC/CF6:  F803       SVEC  s=3  dx=768   dy=0     z=0
99EE/CF7:  D000       RTSL

CHAR_U: (JSRL operand $CF8)
; text glyph (CHAR_GLYPH_EMIT 0x2C36)
; refs: letter tbl 3A2E 'U'
99F0/CF8:  327F 0000  VCTR  s=3  dx=0     dy=639   z=0
99F4/CFA:  367F 9000  VCTR  s=3  dx=0     dy=-639  z=9
99F8/CFC:  F892       SVEC  s=3  dx=512   dy=0     z=9
99FA/CFD:  327F 9000  VCTR  s=3  dx=0     dy=639   z=9
99FE/CFF:  367F 01FF  VCTR  s=3  dx=511   dy=-639  z=0
9A02/D01:  D000       RTSL

CHAR_V: (JSRL operand $D02)
; text glyph (CHAR_GLYPH_EMIT 0x2C36)
; refs: letter tbl 3A2E 'V'
9A04/D02:  327F 0000  VCTR  s=3  dx=0     dy=639   z=0
9A08/D04:  F790       SVEC  s=2  dx=0     dy=-768  z=9
9A0A/D05:  F692       SVEC  s=2  dx=512   dy=-512  z=9
9A0C/D06:  F292       SVEC  s=2  dx=512   dy=512   z=9
9A0E/D07:  F390       SVEC  s=2  dx=0     dy=768   z=9
9A10/D08:  367F 01FF  VCTR  s=3  dx=511   dy=-639  z=0
9A14/D0A:  D000       RTSL

CHAR_W: (JSRL operand $D0B)
; text glyph (CHAR_GLYPH_EMIT 0x2C36)
; refs: POST input dlist 026E+10
; refs: letter tbl 3A2E 'W'
9A16/D0B:  327F 0000  VCTR  s=3  dx=0     dy=639   z=0
9A1A/D0D:  367F 9000  VCTR  s=3  dx=0     dy=-639  z=9
9A1E/D0F:  F292       SVEC  s=2  dx=512   dy=512   z=9
9A20/D10:  F692       SVEC  s=2  dx=512   dy=-512  z=9
9A22/D11:  327F 9000  VCTR  s=3  dx=0     dy=639   z=9
9A26/D13:  367F 01FF  VCTR  s=3  dx=511   dy=-639  z=0
9A2A/D15:  D000       RTSL

CHAR_X: (JSRL operand $D16)
; text glyph (CHAR_GLYPH_EMIT 0x2C36)
; refs: letter tbl 3A2E 'X'
9A2C/D16:  327F 91FF  VCTR  s=3  dx=511   dy=639   z=9
9A30/D18:  F806       SVEC  s=3  dx=-512  dy=0     z=0
9A32/D19:  367F 91FF  VCTR  s=3  dx=511   dy=-639  z=9
9A36/D1B:  F802       SVEC  s=3  dx=512   dy=0     z=0
9A38/D1C:  D000       RTSL

CHAR_Y: (JSRL operand $D1D)
; text glyph (CHAR_GLYPH_EMIT 0x2C36)
; refs: letter tbl 3A2E 'Y'
9A3A/D1D:  327F 0000  VCTR  s=3  dx=0     dy=639   z=0
9A3E/D1F:  267F 91FF  VCTR  s=2  dx=511   dy=-639  z=9
9A42/D21:  227F 91FF  VCTR  s=2  dx=511   dy=639   z=9
9A46/D23:  F606       SVEC  s=2  dx=-512  dy=-512  z=0
9A48/D24:  F790       SVEC  s=2  dx=0     dy=-768  z=9
9A4A/D25:  F803       SVEC  s=3  dx=768   dy=0     z=0
9A4C/D26:  D000       RTSL

CHAR_Z: (JSRL operand $D27)
; text glyph (CHAR_GLYPH_EMIT 0x2C36)
; refs: letter tbl 3A2E 'Z'
9A4E/D27:  327F 0000  VCTR  s=3  dx=0     dy=639   z=0
9A52/D29:  F892       SVEC  s=3  dx=512   dy=0     z=9
9A54/D2A:  367F 95FF  VCTR  s=3  dx=-511  dy=-639  z=9
9A58/D2C:  F892       SVEC  s=3  dx=512   dy=0     z=9
9A5A/D2D:  F802       SVEC  s=3  dx=512   dy=0     z=0
9A5C/D2E:  D000       RTSL

CHAR_0: (JSRL operand $D2F)
; text glyph (CHAR_GLYPH_EMIT 0x2C36)
; refs: status tmpl 30D9+24
; refs: status tmpl 30D9+2C
; refs: game screen 3109+A4
; refs: glyph tbl 3FCD+08
9A5E/D2F:  F001       SVEC  s=2  dx=256   dy=0     z=0
9A60/D30:  F195       SVEC  s=2  dx=-256  dy=256   z=9
9A62/D31:  F390       SVEC  s=2  dx=0     dy=768   z=9
9A64/D32:  F191       SVEC  s=2  dx=256   dy=256   z=9
9A66/D33:  F092       SVEC  s=2  dx=512   dy=0     z=9
9A68/D34:  F591       SVEC  s=2  dx=256   dy=-256  z=9
9A6A/D35:  F790       SVEC  s=2  dx=0     dy=-768  z=9
9A6C/D36:  F595       SVEC  s=2  dx=-256  dy=-256  z=9
9A6E/D37:  F096       SVEC  s=2  dx=-512  dy=0     z=9
9A70/D38:  3000 037F  VCTR  s=3  dx=895   dy=0     z=0
9A74/D3A:  D000       RTSL

CHAR_1: (JSRL operand $D3B)
; text glyph (CHAR_GLYPH_EMIT 0x2C36)
; refs: glyph tbl 3FCD+0A
9A76/D3B:  237F 00FF  VCTR  s=2  dx=255   dy=895   z=0
9A7A/D3D:  12FF 91FF  VCTR  s=1  dx=511   dy=767   z=9
9A7E/D3F:  367F 9000  VCTR  s=3  dx=0     dy=-639  z=9
9A82/D41:  F005       SVEC  s=2  dx=-256  dy=0     z=0
9A84/D42:  F092       SVEC  s=2  dx=512   dy=0     z=9
9A86/D43:  3000 027F  VCTR  s=3  dx=639   dy=0     z=0
9A8A/D45:  D000       RTSL

CHAR_2: (JSRL operand $D46)
; text glyph (CHAR_GLYPH_EMIT 0x2C36)
; refs: glyph tbl 3FCD+0C
9A8C/D46:  FA00       SVEC  s=3  dx=0     dy=512   z=0
9A8E/D47:  F191       SVEC  s=2  dx=256   dy=256   z=9
9A90/D48:  F092       SVEC  s=2  dx=512   dy=0     z=9
9A92/D49:  F591       SVEC  s=2  dx=256   dy=-256  z=9
9A94/D4A:  16FF 9000  VCTR  s=1  dx=0     dy=-767  z=9
9A98/D4C:  FD96       SVEC  s=3  dx=-512  dy=-256  z=9
9A9A/D4D:  05FF 9000  VCTR  s=0  dx=0     dy=-511  z=9
9A9E/D4F:  F892       SVEC  s=3  dx=512   dy=0     z=9
9AA0/D50:  F802       SVEC  s=3  dx=512   dy=0     z=0
9AA2/D51:  D000       RTSL

CHAR_3: (JSRL operand $D52)
; text glyph (CHAR_GLYPH_EMIT 0x2C36)
; refs: glyph tbl 3FCD+0E
9AA4/D52:  FA00       SVEC  s=3  dx=0     dy=512   z=0
9AA6/D53:  F191       SVEC  s=2  dx=256   dy=256   z=9
9AA8/D54:  F092       SVEC  s=2  dx=512   dy=0     z=9
9AAA/D55:  F591       SVEC  s=2  dx=256   dy=-256  z=9
9AAC/D56:  F590       SVEC  s=2  dx=0     dy=-256  z=9
9AAE/D57:  05FF 97FF  VCTR  s=0  dx=-1023 dy=-511  z=9
9AB2/D59:  F095       SVEC  s=2  dx=-256  dy=0     z=9
9AB4/D5A:  F001       SVEC  s=2  dx=256   dy=0     z=0
9AB6/D5B:  05FF 93FF  VCTR  s=0  dx=1023  dy=-511  z=9
9ABA/D5D:  F590       SVEC  s=2  dx=0     dy=-256  z=9
9ABC/D5E:  F595       SVEC  s=2  dx=-256  dy=-256  z=9
9ABE/D5F:  F096       SVEC  s=2  dx=-512  dy=0     z=9
9AC0/D60:  F195       SVEC  s=2  dx=-256  dy=256   z=9
9AC2/D61:  347F 03FF  VCTR  s=3  dx=1023  dy=-127  z=0
9AC6/D63:  D000       RTSL

CHAR_4: (JSRL operand $D64)
; text glyph (CHAR_GLYPH_EMIT 0x2C36)
; refs: glyph tbl 3FCD+10
9AC8/D64:  327F 0000  VCTR  s=3  dx=0     dy=639   z=0
9ACC/D66:  267F 9000  VCTR  s=2  dx=0     dy=-639  z=9
9AD0/D68:  F892       SVEC  s=3  dx=512   dy=0     z=9
9AD2/D69:  227F 0000  VCTR  s=2  dx=0     dy=639   z=0
9AD6/D6B:  367F 9000  VCTR  s=3  dx=0     dy=-639  z=9
9ADA/D6D:  F802       SVEC  s=3  dx=512   dy=0     z=0
9ADC/D6E:  D000       RTSL

CHAR_5: (JSRL operand $D6F)
; text glyph (CHAR_GLYPH_EMIT 0x2C36)
; refs: glyph tbl 3FCD+12
9ADE/D6F:  327F 01FF  VCTR  s=3  dx=511   dy=639   z=0
9AE2/D71:  F896       SVEC  s=3  dx=-512  dy=0     z=9
9AE4/D72:  267F 9000  VCTR  s=2  dx=0     dy=-639  z=9
9AE8/D74:  01FF 93FF  VCTR  s=0  dx=1023  dy=511   z=9
9AEC/D76:  F092       SVEC  s=2  dx=512   dy=0     z=9
9AEE/D77:  05FF 93FF  VCTR  s=0  dx=1023  dy=-511  z=9
9AF2/D79:  16FF 9000  VCTR  s=1  dx=0     dy=-767  z=9
9AF6/D7B:  F595       SVEC  s=2  dx=-256  dy=-256  z=9
9AF8/D7C:  F096       SVEC  s=2  dx=-512  dy=0     z=9
9AFA/D7D:  F195       SVEC  s=2  dx=-256  dy=256   z=9
9AFC/D7E:  347F 03FF  VCTR  s=3  dx=1023  dy=-127  z=0
9B00/D80:  D000       RTSL

CHAR_6: (JSRL operand $D81)
; text glyph (CHAR_GLYPH_EMIT 0x2C36)
; refs: glyph tbl 3FCD+14
9B02/D81:  327F 01FF  VCTR  s=3  dx=511   dy=639   z=0
9B06/D83:  F097       SVEC  s=2  dx=-768  dy=0     z=9
9B08/D84:  F595       SVEC  s=2  dx=-256  dy=-256  z=9
9B0A/D85:  FE90       SVEC  s=3  dx=0     dy=-512  z=9
9B0C/D86:  F892       SVEC  s=3  dx=512   dy=0     z=9
9B0E/D87:  227F 9000  VCTR  s=2  dx=0     dy=639   z=9
9B12/D89:  F896       SVEC  s=3  dx=-512  dy=0     z=9
9B14/D8A:  353F 03FF  VCTR  s=3  dx=1023  dy=-319  z=0
9B18/D8C:  D000       RTSL

CHAR_7: (JSRL operand $D8D)
; text glyph (CHAR_GLYPH_EMIT 0x2C36)
; refs: glyph tbl 3FCD+16
9B1A/D8D:  327F 0000  VCTR  s=3  dx=0     dy=639   z=0
9B1E/D8F:  F892       SVEC  s=3  dx=512   dy=0     z=9
9B20/D90:  F590       SVEC  s=2  dx=0     dy=-256  z=9
9B22/D91:  FE96       SVEC  s=3  dx=-512  dy=-512  z=9

CHAR_SPACE: (JSRL operand $D92)
; (entered by fall-through: the previous shape runs into this one, so a linear sweep never sees a block start here)
; text glyph (CHAR_GLYPH_EMIT 0x2C36)
; refs: POST dlist 01A0+0A
; refs: POST dlist 01A0+0C
; refs: POST dlist 01A0+14
; refs: POST dlist 01A0+1A
; refs: POST dlist 01A0+1E
; refs: POST dlist 01A0+20
; refs: POST dlist 01A0+22
; refs: POST dlist 01A0+24
; refs: POST input dlist 026E+0A
; refs: POST input dlist 026E+14
; refs: POST input dlist 026E+1A
; refs: POST input dlist 026E+1E
; refs: POST input dlist 026E+20
; refs: POST input dlist 026E+22
; refs: POST input dlist 026E+24
; refs: status tmpl 30D9+16
; refs: status tmpl 30D9+18
; refs: status tmpl 30D9+1A
; refs: status tmpl 30D9+1C
; refs: status tmpl 30D9+1E
; refs: status tmpl 30D9+20
; refs: status tmpl 30D9+22
; refs: status tmpl 30D9+2A
; refs: game screen 3109+96
; refs: game screen 3109+98
; refs: game screen 3109+9A
; refs: game screen 3109+9C
; refs: game screen 3109+9E
; refs: game screen 3109+A0
; refs: game screen 3109+A2
; refs: letter wheel 3A0E+08
; refs: letter wheel 3A0E+0A
; refs: letter wheel 3A0E+0C
; refs: letter wheel 3A0E+54
; refs: glyph tbl 3FCD+1C
9B24/D92:  F00A       SVEC  s=4  dx=512   dy=0     z=0
9B26/D93:  D000       RTSL

CHAR_8: (JSRL operand $D94)
; text glyph (CHAR_GLYPH_EMIT 0x2C36)
; refs: glyph tbl 3FCD+18
9B28/D94:  327F 9000  VCTR  s=3  dx=0     dy=639   z=9
9B2C/D96:  F892       SVEC  s=3  dx=512   dy=0     z=9
9B2E/D97:  367F 9000  VCTR  s=3  dx=0     dy=-639  z=9
9B32/D99:  F896       SVEC  s=3  dx=-512  dy=0     z=9
9B34/D9A:  227F 0000  VCTR  s=2  dx=0     dy=639   z=0
9B38/D9C:  F892       SVEC  s=3  dx=512   dy=0     z=9
9B3A/D9D:  267F 03FF  VCTR  s=2  dx=1023  dy=-639  z=0
9B3E/D9F:  D000       RTSL

CHAR_9: (JSRL operand $DA0)
; text glyph (CHAR_GLYPH_EMIT 0x2C36)
; refs: glyph tbl 3FCD+1A
9B40/DA0:  F802       SVEC  s=3  dx=512   dy=0     z=0
9B42/DA1:  327F 9000  VCTR  s=3  dx=0     dy=639   z=9
9B46/DA3:  F896       SVEC  s=3  dx=-512  dy=0     z=9
9B48/DA4:  267F 9000  VCTR  s=2  dx=0     dy=-639  z=9
9B4C/DA6:  F892       SVEC  s=3  dx=512   dy=0     z=9
9B4E/DA7:  267F 03FF  VCTR  s=2  dx=1023  dy=-639  z=0
9B52/DA9:  D000       RTSL

CHAR_PERIOD: (JSRL operand $DAA)
; text glyph (CHAR_GLYPH_EMIT 0x2C36)
; refs: glyph tbl 3FCD+02
9B54/DAA:  01FF 91FF  VCTR  s=0  dx=511   dy=511   z=9
9B58/DAC:  0000 05FF  VCTR  s=0  dx=-511  dy=0     z=0
9B5C/DAE:  05FF 91FF  VCTR  s=0  dx=511   dy=-511  z=9
9B60/DB0:  3000 03BF  VCTR  s=3  dx=959   dy=0     z=0
9B64/DB2:  D000       RTSL

CHAR_COMMA: (JSRL operand $DB3)
; text glyph (CHAR_GLYPH_EMIT 0x2C36)
; refs: glyph tbl 3FCD+04
9B66/DB3:  0000 01FF  VCTR  s=0  dx=511   dy=0     z=0
9B6A/DB5:  0000 95FF  VCTR  s=0  dx=-511  dy=0     z=9
9B6E/DB7:  01FF 9000  VCTR  s=0  dx=0     dy=511   z=9
9B72/DB9:  0000 91FF  VCTR  s=0  dx=511   dy=0     z=9
9B76/DBB:  F590       SVEC  s=2  dx=0     dy=-256  z=9
9B78/DBC:  303F 03BF  VCTR  s=3  dx=959   dy=63    z=0
9B7C/DBE:  D000       RTSL

CHAR_SPACE2: (JSRL operand $DBF)
; text glyph (CHAR_GLYPH_EMIT 0x2C36)
; refs: letter wheel 3A0E+58
; refs: letter wheel 3A0E+5C
; refs: letter wheel 3A0E+60
; refs: letter wheel 3A0E+64
; refs: letter wheel 3A0E+72
; refs: letter wheel 3A0E+76
9B7E/DBF:  467F 05FF  VCTR  s=4  dx=-511  dy=-639  z=0
9B82/DC1:  D000       RTSL

UNUSED_9B84: (JSRL operand $DC2)
; NOT unused: 2P active-player score blink mark; rename to PLAYER_UP_MARK deferred - the C port's omega_shapes.h still uses SHAPE_IDX_UNUSED_9B84
; refs: VG_RESTART 0x17AB (2P active-score blink, word CDC2 -> 0x81A6)
9B84/DC2:  F802       SVEC  s=3  dx=512   dy=0     z=0
9B86/DC3:  227F C7FF  VCTR  s=2  dx=-1023 dy=639   z=12
9B8A/DC5:  227F C3FF  VCTR  s=2  dx=1023  dy=639   z=12
9B8E/DC7:  367F C000  VCTR  s=3  dx=0     dy=-639  z=12
9B92/DC9:  D000       RTSL

CHAR_x3D: (JSRL operand $DCA)
; text glyph (CHAR_GLYPH_EMIT 0x2C36)
; refs: glyph tbl 3FCD+22
9B94/DCA:  237F 007F  VCTR  s=2  dx=127   dy=895   z=0
9B98/DCC:  F093       SVEC  s=2  dx=768   dy=0     z=9
9B9A/DCD:  16FF 0000  VCTR  s=1  dx=0     dy=-767  z=0
9B9E/DCF:  F097       SVEC  s=2  dx=-768  dy=0     z=9
9BA0/DD0:  34FF 03BF  VCTR  s=3  dx=959   dy=-255  z=0
9BA4/DD2:  D000       RTSL

CHAR_x2F: (JSRL operand $DD3)
; text glyph (CHAR_GLYPH_EMIT 0x2C36)
; refs: glyph tbl 3FCD+06
9BA6/DD3:  544F 031F  VCTR  s=5  dx=799   dy=-79   z=0
9BAA/DD5:  C9E9       JSRL  $93D2   ; -> SHIP_15
9BAC/DD6:  D000       RTSL

UNUSED_9BAE: (JSRL operand $DD7)
; unused art - no reference anywhere in either ROM
9BAE/DD7:  0000 0000  VCTR  s=0  dx=0     dy=0     z=0
9BB2/DD9:  0000 002D  VCTR  s=0  dx=45    dy=0     z=0
9BB6/DDB:  0000 0000  VCTR  s=0  dx=0     dy=0     z=0
9BBA/DDD:  0000 0000  VCTR  s=0  dx=0     dy=0     z=0
9BBE/DDF:  0000 0000  VCTR  s=0  dx=0     dy=0     z=0
9BC2/DE1:  0000 0000  VCTR  s=0  dx=0     dy=0     z=0
9BC6/DE3:  0000 0000  VCTR  s=0  dx=0     dy=0     z=0
9BCA/DE5:  0000 0000  VCTR  s=0  dx=0     dy=0     z=0
9BCE/DE7:  0000 0000  VCTR  s=0  dx=0     dy=0     z=0
9BD2/DE9:  0000 0000  VCTR  s=0  dx=0     dy=0     z=0
9BD6/DEB:  0000 0000  VCTR  s=0  dx=0     dy=0     z=0
9BDA/DED:  0000 0000  VCTR  s=0  dx=0     dy=0     z=0
9BDE/DEF:  0000 0000  VCTR  s=0  dx=0     dy=0     z=0
9BE2/DF1:  0000 0000  VCTR  s=0  dx=0     dy=0     z=0
9BE6/DF3:  0000 0000  VCTR  s=0  dx=0     dy=0     z=0
9BEA/DF5:  0000 0000  VCTR  s=0  dx=0     dy=0     z=0
9BEE/DF7:  0000 0000  VCTR  s=0  dx=0     dy=0     z=0
9BF2/DF9:  0000 0000  VCTR  s=0  dx=0     dy=0     z=0
9BF6/DFB:  0000 30BF  VCTR  s=0  dx=191   dy=0     z=3
9BFA/DFD:  03FF 34BF  VCTR  s=0  dx=-191  dy=1023  z=3
9BFE/DFF:  F23F       SVEC  s=4  dx=-768  dy=512   z=3
9C00/E00:  347F F63F  VCTR  s=3  dx=-575  dy=-127  z=15
9C04/E02:  D000       RTSL

SHIP_M00: (JSRL operand $E03)
; hull, heading 0x10: X-flip set, SHIP_BASE 30A1[1] = word $DFC + 30A9 offset
9C06/E03:  2000 067F  VCTR  s=2  dx=-639  dy=0     z=0
9C0A/E05:  413F F27F  VCTR  s=4  dx=639   dy=319   z=15
9C0E/E07:  469F F55F  VCTR  s=4  dx=-351  dy=-671  z=15
9C12/E09:  F8F6       SVEC  s=3  dx=-512  dy=0     z=15
9C14/E0A:  347F F27F  VCTR  s=3  dx=639   dy=-127  z=15
9C18/E0C:  41FF F61F  VCTR  s=4  dx=-543  dy=511   z=15
9C1C/E0E:  257F F67F  VCTR  s=2  dx=-639  dy=-383  z=15
9C20/E10:  24FF F27F  VCTR  s=2  dx=639   dy=-255  z=15
9C24/E12:  41FF F21F  VCTR  s=4  dx=543   dy=511   z=15
9C28/E14:  347F F67F  VCTR  s=3  dx=-639  dy=-127  z=15
9C2C/E16:  F8F2       SVEC  s=3  dx=512   dy=0     z=15
9C2E/E17:  469F F15F  VCTR  s=4  dx=351   dy=-671  z=15
9C32/E19:  411F F67F  VCTR  s=4  dx=-639  dy=287   z=15
9C36/E1B:  D000       RTSL

THRUST_M00: (JSRL operand $E1C)
; flame of SHIP_M00 (hull word - 7, ROM 0x1E58)
9C38/E1C:  303F 03FF  VCTR  s=3  dx=1023  dy=63    z=0
9C3C/E1E:  34BF F23F  VCTR  s=3  dx=575   dy=-191  z=15
9C40/E20:  347F F63F  VCTR  s=3  dx=-575  dy=-127  z=15
9C44/E22:  D000       RTSL

SHIP_M01: (JSRL operand $E23)
; hull, heading 0x11: X-flip set, SHIP_BASE 30A1[1] = word $DFC + 30A9 offset
9C46/E23:  2000 067F  VCTR  s=2  dx=-639  dy=0     z=0
9C4A/E25:  40FF F29F  VCTR  s=4  dx=671   dy=255   z=15
9C4E/E27:  465F F59F  VCTR  s=4  dx=-415  dy=-607  z=15
9C52/E29:  F8F6       SVEC  s=3  dx=-512  dy=0     z=15
9C54/E2A:  34BF F27F  VCTR  s=3  dx=639   dy=-191  z=15
9C58/E2C:  423F F5FF  VCTR  s=4  dx=-511  dy=575   z=15
9C5C/E2E:  24FF F67F  VCTR  s=2  dx=-639  dy=-255  z=15
9C60/E30:  257F F27F  VCTR  s=2  dx=639   dy=-383  z=15
9C64/E32:  41BF F23F  VCTR  s=4  dx=575   dy=447   z=15
9C68/E34:  343F F67F  VCTR  s=3  dx=-639  dy=-63   z=15
9C6C/E36:  247F F3FF  VCTR  s=2  dx=1023  dy=-127  z=15
9C70/E38:  46BF F11F  VCTR  s=4  dx=287   dy=-703  z=15
9C74/E3A:  415F F65F  VCTR  s=4  dx=-607  dy=351   z=15
9C78/E3C:  D000       RTSL

THRUST_M01: (JSRL operand $E3D)
; flame of SHIP_M01 (hull word - 7, ROM 0x1E58)
9C7A/E3D:  3000 03FF  VCTR  s=3  dx=1023  dy=0     z=0
9C7E/E3F:  25FF F3FF  VCTR  s=2  dx=1023  dy=-511  z=15
9C82/E41:  343F F63F  VCTR  s=3  dx=-575  dy=-63   z=15
9C86/E43:  D000       RTSL

SHIP_M02: (JSRL operand $E44)
; hull, heading 0x12: X-flip set, SHIP_BASE 30A1[1] = word $DFC + 30A9 offset
9C88/E44:  207F 067F  VCTR  s=2  dx=-639  dy=127   z=0
9C8C/E46:  40BF F2BF  VCTR  s=4  dx=703   dy=191   z=15
9C90/E48:  465F F5DF  VCTR  s=4  dx=-479  dy=-607  z=15
9C94/E4A:  20FF F7FF  VCTR  s=2  dx=-1023 dy=255   z=15
9C98/E4C:  34FF F23F  VCTR  s=3  dx=575   dy=-255  z=15
9C9C/E4E:  425F F59F  VCTR  s=4  dx=-415  dy=607   z=15
9CA0/E50:  F5F7       SVEC  s=2  dx=-768  dy=-256  z=15
9CA2/E51:  257F F27F  VCTR  s=2  dx=639   dy=-383  z=15
9CA6/E53:  417F F27F  VCTR  s=4  dx=639   dy=383   z=15
9CAA/E55:  303F F6BF  VCTR  s=3  dx=-703  dy=63    z=15
9CAE/E57:  24FF F3FF  VCTR  s=2  dx=1023  dy=-255  z=15
9CB2/E59:  46DF F0DF  VCTR  s=4  dx=223   dy=-735  z=15
9CB6/E5B:  419F F63F  VCTR  s=4  dx=-575  dy=415   z=15
9CBA/E5D:  D000       RTSL

THRUST_M02: (JSRL operand $E5E)
; flame of SHIP_M02 (hull word - 7, ROM 0x1E58)
9CBC/E5E:  343F 03FF  VCTR  s=3  dx=1023  dy=-63   z=0
9CC0/E60:  26FF F3FF  VCTR  s=2  dx=1023  dy=-767  z=15
9CC4/E62:  3000 F67F  VCTR  s=3  dx=-639  dy=0     z=15
9CC8/E64:  D000       RTSL

SHIP_M03: (JSRL operand $E65)
; hull, heading 0x13: X-flip set, SHIP_BASE 30A1[1] = word $DFC + 30A9 offset
9CCA/E65:  207F 067F  VCTR  s=2  dx=-639  dy=127   z=0
9CCE/E67:  407F F2BF  VCTR  s=4  dx=703   dy=127   z=15
9CD2/E69:  461F F61F  VCTR  s=4  dx=-543  dy=-543  z=15
9CD6/E6B:  217F F77F  VCTR  s=2  dx=-895  dy=383   z=15
9CDA/E6D:  353F F23F  VCTR  s=3  dx=575   dy=-319  z=15
9CDE/E6F:  427F F57F  VCTR  s=4  dx=-383  dy=639   z=15
9CE2/E71:  247F F67F  VCTR  s=2  dx=-639  dy=-127  z=15
9CE6/E73:  267F F1FF  VCTR  s=2  dx=511   dy=-639  z=15
9CEA/E75:  415F F29F  VCTR  s=4  dx=671   dy=351   z=15
9CEE/E77:  307F F67F  VCTR  s=3  dx=-639  dy=127   z=15
9CF2/E79:  257F F37F  VCTR  s=2  dx=895   dy=-383  z=15
9CF6/E7B:  46DF F07F  VCTR  s=4  dx=127   dy=-735  z=15
9CFA/E7D:  337F F7FF  VCTR  s=3  dx=-1023 dy=895   z=15
9CFE/E7F:  D000       RTSL

THRUST_M03: (JSRL operand $E80)
; flame of SHIP_M03 (hull word - 7, ROM 0x1E58)
9D00/E80:  34BF 03FF  VCTR  s=3  dx=1023  dy=-191  z=0
9D04/E82:  26FF F37F  VCTR  s=2  dx=895   dy=-767  z=15
9D08/E84:  303F F67F  VCTR  s=3  dx=-639  dy=63    z=15
9D0C/E86:  D000       RTSL

SHIP_M04: (JSRL operand $E87)
; hull, heading 0x14: X-flip set, SHIP_BASE 30A1[1] = word $DFC + 30A9 offset
9D0E/E87:  20FF 067F  VCTR  s=2  dx=-639  dy=255   z=0
9D12/E89:  401F F2DF  VCTR  s=4  dx=735   dy=31    z=15
9D16/E8B:  45DF F63F  VCTR  s=4  dx=-575  dy=-479  z=15
9D1A/E8D:  F9F6       SVEC  s=3  dx=-512  dy=256   z=15
9D1C/E8E:  277F F3FF  VCTR  s=2  dx=1023  dy=-895  z=15
9D20/E90:  42BF F51F  VCTR  s=4  dx=-287  dy=703   z=15
9D24/E92:  247F F6FF  VCTR  s=2  dx=-767  dy=-127  z=15
9D28/E94:  267F F1FF  VCTR  s=2  dx=511   dy=-639  z=15
9D2C/E96:  411F F2BF  VCTR  s=4  dx=703   dy=287   z=15
9D30/E98:  30BF F6BF  VCTR  s=3  dx=-703  dy=191   z=15
9D34/E9A:  FDF2       SVEC  s=3  dx=512   dy=-256  z=15
9D36/E9B:  46DF F03F  VCTR  s=4  dx=63    dy=-735  z=15
9D3A/E9D:  33FF F7BF  VCTR  s=3  dx=-959  dy=1023  z=15
9D3E/E9F:  D000       RTSL

THRUST_M04: (JSRL operand $EA0)
; flame of SHIP_M04 (hull word - 7, ROM 0x1E58)
9D40/EA0:  34FF 03BF  VCTR  s=3  dx=959   dy=-255  z=0
9D44/EA2:  277F F37F  VCTR  s=2  dx=895   dy=-895  z=15
9D48/EA4:  303F F63F  VCTR  s=3  dx=-575  dy=63    z=15
9D4C/EA6:  D000       RTSL

SHIP_M05: (JSRL operand $EA7)
; hull, heading 0x15: X-flip set, SHIP_BASE 30A1[1] = word $DFC + 30A9 offset
9D4E/EA7:  F106       SVEC  s=2  dx=-512  dy=256   z=0
9D50/EA8:  441F F2BF  VCTR  s=4  dx=703   dy=-31   z=15
9D54/EAA:  459F F65F  VCTR  s=4  dx=-607  dy=-415  z=15
9D58/EAC:  F9F6       SVEC  s=3  dx=-512  dy=256   z=15
9D5A/EAD:  35BF F23F  VCTR  s=3  dx=575   dy=-447  z=15
9D5E/EAF:  42DF F4FF  VCTR  s=4  dx=-255  dy=735   z=15
9D62/EB1:  F0F7       SVEC  s=2  dx=-768  dy=0     z=15
9D64/EB2:  267F F17F  VCTR  s=2  dx=383   dy=-639  z=15
9D68/EB4:  40BF F2DF  VCTR  s=4  dx=735   dy=191   z=15
9D6C/EB6:  30BF F6BF  VCTR  s=3  dx=-703  dy=191   z=15
9D70/EB8:  FDF2       SVEC  s=3  dx=512   dy=-256  z=15
9D72/EB9:  46DF F41F  VCTR  s=4  dx=-31   dy=-735  z=15
9D76/EBB:  421F F57F  VCTR  s=4  dx=-383  dy=543   z=15
9D7A/EBD:  D000       RTSL

THRUST_M05: (JSRL operand $EBE)
; flame of SHIP_M05 (hull word - 7, ROM 0x1E58)
9D7C/EBE:  357F 03BF  VCTR  s=3  dx=959   dy=-383  z=0
9D80/EC0:  277F F2FF  VCTR  s=2  dx=767   dy=-895  z=15
9D84/EC2:  307F F67F  VCTR  s=3  dx=-639  dy=127   z=15
9D88/EC4:  D000       RTSL

SHIP_M06: (JSRL operand $EC5)
; hull, heading 0x16: X-flip set, SHIP_BASE 30A1[1] = word $DFC + 30A9 offset
9D8A/EC5:  12FF 07FF  VCTR  s=1  dx=-1023 dy=767   z=0
9D8E/EC7:  445F F2BF  VCTR  s=4  dx=703   dy=-95   z=15
9D92/EC9:  457F F67F  VCTR  s=4  dx=-639  dy=-383  z=15
9D96/ECB:  227F F7FF  VCTR  s=2  dx=-1023 dy=639   z=15
9D9A/ECD:  FEF2       SVEC  s=3  dx=512   dy=-512  z=15
9D9C/ECE:  42FF F49F  VCTR  s=4  dx=-159  dy=767   z=15
9DA0/ED0:  F0F7       SVEC  s=2  dx=-768  dy=0     z=15
9DA2/ED1:  267F F0FF  VCTR  s=2  dx=255   dy=-639  z=15
9DA6/ED3:  407F F2FF  VCTR  s=4  dx=767   dy=127   z=15
9DAA/ED5:  30FF F6BF  VCTR  s=3  dx=-703  dy=255   z=15
9DAE/ED7:  267F F3FF  VCTR  s=2  dx=1023  dy=-639  z=15
9DB2/ED9:  46DF F45F  VCTR  s=4  dx=-95   dy=-735  z=15
9DB6/EDB:  425F F55F  VCTR  s=4  dx=-351  dy=607   z=15
9DBA/EDD:  D000       RTSL

THRUST_M06: (JSRL operand $EDE)
; flame of SHIP_M06 (hull word - 7, ROM 0x1E58)
9DBC/EDE:  35BF 037F  VCTR  s=3  dx=895   dy=-447  z=0
9DC0/EE0:  27FF F27F  VCTR  s=2  dx=639   dy=-1023 z=15
9DC4/EE2:  30BF F63F  VCTR  s=3  dx=-575  dy=191   z=15
9DC8/EE4:  D000       RTSL

SHIP_M07: (JSRL operand $EE5)
; hull, heading 0x17: X-flip set, SHIP_BASE 30A1[1] = word $DFC + 30A9 offset
9DCA/EE5:  12FF 07FF  VCTR  s=1  dx=-1023 dy=767   z=0
9DCE/EE7:  447F F2BF  VCTR  s=4  dx=703   dy=-127  z=15
9DD2/EE9:  453F F6BF  VCTR  s=4  dx=-703  dy=-319  z=15
9DD6/EEB:  227F F77F  VCTR  s=2  dx=-895  dy=639   z=15
9DDA/EED:  FEF2       SVEC  s=3  dx=512   dy=-512  z=15
9DDC/EEE:  42DF F47F  VCTR  s=4  dx=-127  dy=735   z=15
9DE0/EF0:  207F F67F  VCTR  s=2  dx=-639  dy=127   z=15
9DE4/EF2:  267F F0FF  VCTR  s=2  dx=255   dy=-639  z=15
9DE8/EF4:  401F F2DF  VCTR  s=4  dx=735   dy=31    z=15
9DEC/EF6:  317F F63F  VCTR  s=3  dx=-575  dy=383   z=15
9DF0/EF8:  F7F3       SVEC  s=2  dx=768   dy=-768  z=15
9DF2/EF9:  46DF F49F  VCTR  s=4  dx=-159  dy=-735  z=15
9DF6/EFB:  427F F51F  VCTR  s=4  dx=-287  dy=639   z=15
9DFA/EFD:  D000       RTSL

THRUST_M07: (JSRL operand $EFE)
; flame of SHIP_M07 (hull word - 7, ROM 0x1E58)
9DFC/EFE:  363F 033F  VCTR  s=3  dx=831   dy=-575  z=0
9E00/F00:  27FF F1FF  VCTR  s=2  dx=511   dy=-1023 z=15
9E04/F02:  21FF F7FF  VCTR  s=2  dx=-1023 dy=511   z=15
9E08/F04:  D000       RTSL

SHIP_M08: (JSRL operand $F05)
; hull, heading 0x18: X-flip set, SHIP_BASE 30A1[1] = word $DFC + 30A9 offset
9E0A/F05:  F206       SVEC  s=2  dx=-512  dy=512   z=0
9E0C/F06:  44DF F29F  VCTR  s=4  dx=671   dy=-223  z=15
9E10/F08:  44FF F6BF  VCTR  s=4  dx=-703  dy=-255  z=15
9E14/F0A:  F3F7       SVEC  s=2  dx=-768  dy=768   z=15
9E16/F0B:  363F F17F  VCTR  s=3  dx=383   dy=-575  z=15
9E1A/F0D:  42FF F41F  VCTR  s=4  dx=-31   dy=767   z=15
9E1E/F0F:  207F F67F  VCTR  s=2  dx=-639  dy=127   z=15
9E22/F11:  267F F07F  VCTR  s=2  dx=127   dy=-639  z=15
9E26/F13:  441F F2FF  VCTR  s=4  dx=767   dy=-31   z=15
9E2A/F15:  317F F63F  VCTR  s=3  dx=-575  dy=383   z=15
9E2E/F17:  F7F3       SVEC  s=2  dx=768   dy=-768  z=15
9E30/F18:  46BF F4FF  VCTR  s=4  dx=-255  dy=-703  z=15
9E34/F1A:  429F F4DF  VCTR  s=4  dx=-223  dy=671   z=15
9E38/F1C:  D000       RTSL

THRUST_M08: (JSRL operand $F1D)
; flame of SHIP_M08 (hull word - 7, ROM 0x1E58)
9E3A/F1D:  367F 02FF  VCTR  s=3  dx=767   dy=-639  z=0
9E3E/F1F:  363F F0BF  VCTR  s=3  dx=191   dy=-575  z=15
9E42/F21:  227F F7FF  VCTR  s=2  dx=-1023 dy=639   z=15
9E46/F23:  D000       RTSL

SHIP_M09: (JSRL operand $F24)
; hull, heading 0x19: X-flip set, SHIP_BASE 30A1[1] = word $DFC + 30A9 offset
9E48/F24:  13FF 06FF  VCTR  s=1  dx=-767  dy=1023  z=0
9E4C/F26:  451F F27F  VCTR  s=4  dx=639   dy=-287  z=15
9E50/F28:  449F F6DF  VCTR  s=4  dx=-735  dy=-159  z=15
9E54/F2A:  F3F7       SVEC  s=2  dx=-768  dy=768   z=15
9E56/F2B:  363F F17F  VCTR  s=3  dx=383   dy=-575  z=15
9E5A/F2D:  42DF F01F  VCTR  s=4  dx=31    dy=735   z=15
9E5E/F2F:  20FF F67F  VCTR  s=2  dx=-639  dy=255   z=15
9E62/F31:  267F F07F  VCTR  s=2  dx=127   dy=-639  z=15
9E66/F33:  447F F2DF  VCTR  s=4  dx=735   dy=-127  z=15
9E6A/F35:  FAF6       SVEC  s=3  dx=-512  dy=512   z=15
9E6C/F36:  277F F27F  VCTR  s=2  dx=639   dy=-895  z=15
9E70/F38:  46BF F53F  VCTR  s=4  dx=-319  dy=-703  z=15
9E74/F3A:  42BF F47F  VCTR  s=4  dx=-127  dy=703   z=15
9E78/F3C:  D000       RTSL

THRUST_M09: (JSRL operand $F3D)
; flame of SHIP_M09 (hull word - 7, ROM 0x1E58)
9E7A/F3D:  36BF 02BF  VCTR  s=3  dx=703   dy=-703  z=0
9E7E/F3F:  367F F07F  VCTR  s=3  dx=127   dy=-639  z=15
9E82/F41:  22FF F77F  VCTR  s=2  dx=-895  dy=767   z=15
9E86/F43:  D000       RTSL

SHIP_M10: (JSRL operand $F44)
; hull, heading 0x1A: X-flip set, SHIP_BASE 30A1[1] = word $DFC + 30A9 offset
9E88/F44:  13FF 06FF  VCTR  s=1  dx=-767  dy=1023  z=0
9E8C/F46:  455F F25F  VCTR  s=4  dx=607   dy=-351  z=15
9E90/F48:  445F F6DF  VCTR  s=4  dx=-735  dy=-95   z=15
9E94/F4A:  23FF F67F  VCTR  s=2  dx=-639  dy=1023  z=15
9E98/F4C:  36BF F0FF  VCTR  s=3  dx=255   dy=-703  z=15
9E9C/F4E:  42FF F07F  VCTR  s=4  dx=127   dy=767   z=15
9EA0/F50:  20FF F67F  VCTR  s=2  dx=-639  dy=255   z=15
9EA4/F52:  F7F4       SVEC  s=2  dx=0     dy=-768  z=15
9EA6/F53:  449F F2FF  VCTR  s=4  dx=767   dy=-159  z=15
9EAA/F55:  FAF6       SVEC  s=3  dx=-512  dy=512   z=15
9EAC/F56:  27FF F27F  VCTR  s=2  dx=639   dy=-1023 z=15
9EB0/F58:  467F F57F  VCTR  s=4  dx=-383  dy=-639  z=15
9EB4/F5A:  42BF F45F  VCTR  s=4  dx=-95   dy=703   z=15
9EB8/F5C:  D000       RTSL

THRUST_M10: (JSRL operand $F5D)
; flame of SHIP_M10 (hull word - 7, ROM 0x1E58)
9EBA/F5D:  373F 027F  VCTR  s=3  dx=639   dy=-831  z=0
9EBE/F5F:  363F F03F  VCTR  s=3  dx=63    dy=-575  z=15
9EC2/F61:  237F F77F  VCTR  s=2  dx=-895  dy=895   z=15
9EC6/F63:  D000       RTSL

SHIP_M11: (JSRL operand $F64)
; hull, heading 0x1B: X-flip set, SHIP_BASE 30A1[1] = word $DFC + 30A9 offset
9EC8/F64:  F205       SVEC  s=2  dx=-256  dy=512   z=0
9ECA/F65:  457F F21F  VCTR  s=4  dx=543   dy=-383  z=15
9ECE/F67:  441F F6DF  VCTR  s=4  dx=-735  dy=-31   z=15
9ED2/F69:  FAF5       SVEC  s=3  dx=-256  dy=512   z=15
9ED4/F6A:  36BF F0BF  VCTR  s=3  dx=191   dy=-703  z=15
9ED8/F6C:  42DF F0BF  VCTR  s=4  dx=191   dy=735   z=15
9EDC/F6E:  217F F67F  VCTR  s=2  dx=-639  dy=383   z=15
9EE0/F70:  F7F4       SVEC  s=2  dx=0     dy=-768  z=15
9EE2/F71:  44FF F2DF  VCTR  s=4  dx=735   dy=-255  z=15
9EE6/F73:  323F F5BF  VCTR  s=3  dx=-447  dy=575   z=15
9EEA/F75:  FEF1       SVEC  s=3  dx=256   dy=-512  z=15
9EEC/F76:  465F F59F  VCTR  s=4  dx=-415  dy=-607  z=15
9EF0/F78:  42BF F41F  VCTR  s=4  dx=-31   dy=703   z=15
9EF4/F7A:  D000       RTSL

THRUST_M11: (JSRL operand $F7B)
; flame of SHIP_M11 (hull word - 7, ROM 0x1E58)
9EF6/F7B:  373F 01FF  VCTR  s=3  dx=511   dy=-831  z=0
9EFA/F7D:  367F F03F  VCTR  s=3  dx=63    dy=-639  z=15
9EFE/F7F:  237F F6FF  VCTR  s=2  dx=-767  dy=895   z=15
9F02/F81:  D000       RTSL

SHIP_M12: (JSRL operand $F82)
; hull, heading 0x1C: X-flip set, SHIP_BASE 30A1[1] = word $DFC + 30A9 offset
9F04/F82:  227F 04FF  VCTR  s=2  dx=-255  dy=639   z=0
9F08/F84:  37BF F3FF  VCTR  s=3  dx=1023  dy=-959  z=15
9F0C/F86:  403F F6DF  VCTR  s=4  dx=-735  dy=63    z=15
9F10/F88:  FAF5       SVEC  s=3  dx=-256  dy=512   z=15
9F12/F89:  36BF F0BF  VCTR  s=3  dx=191   dy=-703  z=15
9F16/F8B:  42BF F11F  VCTR  s=4  dx=287   dy=703   z=15
9F1A/F8D:  21FF F67F  VCTR  s=2  dx=-639  dy=511   z=15
9F1E/F8F:  26FF F47F  VCTR  s=2  dx=-127  dy=-767  z=15
9F22/F91:  451F F2BF  VCTR  s=4  dx=703   dy=-287  z=15
9F26/F93:  23FF F77F  VCTR  s=2  dx=-895  dy=1023  z=15
9F2A/F95:  FEF1       SVEC  s=3  dx=256   dy=-512  z=15
9F2C/F96:  463F F5DF  VCTR  s=4  dx=-479  dy=-575  z=15
9F30/F98:  42DF F01F  VCTR  s=4  dx=31    dy=735   z=15
9F34/F9A:  D000       RTSL

THRUST_M12: (JSRL operand $F9B)
; flame of SHIP_M12 (hull word - 7, ROM 0x1E58)
9F36/F9B:  377F 01BF  VCTR  s=3  dx=447   dy=-895  z=0
9F3A/F9D:  367F F400  VCTR  s=3  dx=0     dy=-639  z=15
9F3E/F9F:  23FF F6FF  VCTR  s=2  dx=-767  dy=1023  z=15
9F42/FA1:  D000       RTSL

SHIP_M13: (JSRL operand $FA2)
; hull, heading 0x1D: X-flip set, SHIP_BASE 30A1[1] = word $DFC + 30A9 offset
9F44/FA2:  227F 047F  VCTR  s=2  dx=-127  dy=639   z=0
9F48/FA4:  37FF F37F  VCTR  s=3  dx=895   dy=-1023 z=15
9F4C/FA6:  407F F6DF  VCTR  s=4  dx=-735  dy=127   z=15
9F50/FA8:  237F F57F  VCTR  s=2  dx=-383  dy=895   z=15
9F54/FAA:  367F F07F  VCTR  s=3  dx=127   dy=-639  z=15
9F58/FAC:  429F F15F  VCTR  s=4  dx=351   dy=671   z=15
9F5C/FAE:  21FF F67F  VCTR  s=2  dx=-639  dy=511   z=15
9F60/FB0:  267F F47F  VCTR  s=2  dx=-127  dy=-639  z=15
9F64/FB2:  457F F27F  VCTR  s=4  dx=639   dy=-383  z=15
9F68/FB4:  323F F53F  VCTR  s=3  dx=-319  dy=575   z=15
9F6C/FB6:  277F F17F  VCTR  s=2  dx=383   dy=-895  z=15
9F70/FB8:  461F F61F  VCTR  s=4  dx=-543  dy=-543  z=15
9F74/FBA:  42BF F07F  VCTR  s=4  dx=127   dy=703   z=15
9F78/FBC:  D000       RTSL

THRUST_M13: (JSRL operand $FBD)
; flame of SHIP_M13 (hull word - 7, ROM 0x1E58)
9F7A/FBD:  37BF 013F  VCTR  s=3  dx=319   dy=-959  z=0
9F7E/FBF:  363F F43F  VCTR  s=3  dx=-63   dy=-575  z=15
9F82/FC1:  23FF F5FF  VCTR  s=2  dx=-511  dy=1023  z=15
9F86/FC3:  D000       RTSL

SHIP_M14: (JSRL operand $FC4)
; hull, heading 0x1E: X-flip set, SHIP_BASE 30A1[1] = word $DFC + 30A9 offset
9F88/FC4:  227F 047F  VCTR  s=2  dx=-127  dy=639   z=0
9F8C/FC6:  463F F19F  VCTR  s=4  dx=415   dy=-575  z=15
9F90/FC8:  40DF F6DF  VCTR  s=4  dx=-735  dy=223   z=15
9F94/FCA:  23FF F4FF  VCTR  s=2  dx=-255  dy=1023  z=15
9F98/FCC:  36BF F03F  VCTR  s=3  dx=63    dy=-703  z=15
9F9C/FCE:  427F F17F  VCTR  s=4  dx=383   dy=639   z=15
9FA0/FD0:  227F F57F  VCTR  s=2  dx=-383  dy=639   z=15
9FA4/FD2:  F7F5       SVEC  s=2  dx=-256  dy=-768  z=15
9FA6/FD3:  459F F25F  VCTR  s=4  dx=607   dy=-415  z=15
9FAA/FD5:  323F F4FF  VCTR  s=3  dx=-255  dy=575   z=15
9FAE/FD7:  27FF F0FF  VCTR  s=2  dx=255   dy=-1023 z=15
9FB2/FD9:  45DF F65F  VCTR  s=4  dx=-607  dy=-479  z=15
9FB6/FDB:  42BF F0BF  VCTR  s=4  dx=191   dy=703   z=15
9FBA/FDD:  D000       RTSL

THRUST_M14: (JSRL operand $FDE)
; flame of SHIP_M14 (hull word - 7, ROM 0x1E58)
9FBC/FDE:  37FF 00FF  VCTR  s=3  dx=255   dy=-1023 z=0
9FC0/FE0:  363F F47F  VCTR  s=3  dx=-127  dy=-575  z=15
9FC4/FE2:  323F F4BF  VCTR  s=3  dx=-191  dy=575   z=15
9FC8/FE4:  D000       RTSL

SHIP_M15: (JSRL operand $FE5)
; hull, heading 0x1F: X-flip set, SHIP_BASE 30A1[1] = word $DFC + 30A9 offset
9FCA/FE5:  227F 0400  VCTR  s=2  dx=0     dy=639   z=0
9FCE/FE7:  465F F15F  VCTR  s=4  dx=351   dy=-607  z=15
9FD2/FE9:  411F F6BF  VCTR  s=4  dx=-703  dy=287   z=15
9FD6/FEB:  23FF F47F  VCTR  s=2  dx=-127  dy=1023  z=15
9FDA/FED:  367F F43F  VCTR  s=3  dx=-63   dy=-639  z=15
9FDE/FEF:  423F F1BF  VCTR  s=4  dx=447   dy=575   z=15
9FE2/FF1:  227F F57F  VCTR  s=2  dx=-383  dy=639   z=15
9FE6/FF3:  267F F4FF  VCTR  s=2  dx=-255  dy=-639  z=15
9FEA/FF5:  45FF F23F  VCTR  s=4  dx=575   dy=-511  z=15
9FEE/FF7:  327F F4BF  VCTR  s=3  dx=-191  dy=639   z=15
9FF2/FF9:  FEF4       SVEC  s=3  dx=0     dy=-512  z=15
9FF4/FFA:  459F F65F  VCTR  s=4  dx=-607  dy=-415  z=15
9FF8/FFC:  429F F0FF  VCTR  s=4  dx=255   dy=671   z=15
9FFC/FFE:  D000       RTSL

THRUST_M15: (JSRL operand $FFF)
; flame of SHIP_M15 (hull word - 7, ROM 0x1E58)
9FFE/FFF:  FFFB       SVEC  s=5  dx=768   dy=-768  z=15
