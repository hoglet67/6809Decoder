FOR I%=&10 TO &13 STEP 3
P%=&3000
[OPT I%
.test
STS &0080
LDX &0232
STX oldvec
.loop1
LDMD #&01
LDX #loop2
STX &0232
LDX #illegal-2
.loop2
LDS &0080
LEAX 2, X
JMP ,X
.oldvec
EQUW &0000
.illegal
EQUB &12
EQUB &15
EQUB &12
EQUB &18
EQUB &12
EQUB &1B
EQUB &12
EQUB &38
EQUB &12
EQUB &3E
EQUB &12
EQUB &41
EQUB &12
EQUB &42
EQUB &12
EQUB &45
EQUB &12
EQUB &4B
EQUB &12
EQUB &4E
EQUB &12
EQUB &51
EQUB &12
EQUB &52
EQUB &12
EQUB &55
EQUB &12
EQUB &5B
EQUB &12
EQUB &5E
EQUB &12
EQUB &87
EQUB &12
EQUB &8F
EQUB &12
EQUB &C7
EQUB &12
EQUB &CF
EQUB &10
EQUB &00
EQUB &10
EQUB &01
EQUB &10
EQUB &02
EQUB &10
EQUB &03
EQUB &10
EQUB &04
EQUB &10
EQUB &05
EQUB &10
EQUB &06
EQUB &10
EQUB &07
EQUB &10
EQUB &08
EQUB &10
EQUB &09
EQUB &10
EQUB &0A
EQUB &10
EQUB &0B
EQUB &10
EQUB &0C
EQUB &10
EQUB &0D
EQUB &10
EQUB &0E
EQUB &10
EQUB &0F
EQUB &10
EQUB &10
EQUB &10
EQUB &11
EQUB &10
EQUB &12
EQUB &10
EQUB &13
EQUB &10
EQUB &14
EQUB &10
EQUB &15
EQUB &10
EQUB &16
EQUB &10
EQUB &17
EQUB &10
EQUB &18
EQUB &10
EQUB &19
EQUB &10
EQUB &1A
EQUB &10
EQUB &1B
EQUB &10
EQUB &1C
EQUB &10
EQUB &1D
EQUB &10
EQUB &1E
EQUB &10
EQUB &1F
EQUB &10
EQUB &20
EQUB &10
EQUB &3C
EQUB &10
EQUB &3D
EQUB &10
EQUB &3E
EQUB &10
EQUB &41
EQUB &10
EQUB &42
EQUB &10
EQUB &45
EQUB &10
EQUB &4B
EQUB &10
EQUB &4E
EQUB &10
EQUB &50
EQUB &10
EQUB &51
EQUB &10
EQUB &52
EQUB &10
EQUB &55
EQUB &10
EQUB &57
EQUB &10
EQUB &58
EQUB &10
EQUB &5B
EQUB &10
EQUB &5E
EQUB &10
EQUB &60
EQUB &10
EQUB &61
EQUB &10
EQUB &62
EQUB &10
EQUB &63
EQUB &10
EQUB &64
EQUB &10
EQUB &65
EQUB &10
EQUB &66
EQUB &10
EQUB &67
EQUB &10
EQUB &68
EQUB &10
EQUB &69
EQUB &10
EQUB &6A
EQUB &10
EQUB &6B
EQUB &10
EQUB &6C
EQUB &10
EQUB &6D
EQUB &10
EQUB &6E
EQUB &10
EQUB &6F
EQUB &10
EQUB &70
EQUB &10
EQUB &71
EQUB &10
EQUB &72
EQUB &10
EQUB &73
EQUB &10
EQUB &74
EQUB &10
EQUB &75
EQUB &10
EQUB &76
EQUB &10
EQUB &77
EQUB &10
EQUB &78
EQUB &10
EQUB &79
EQUB &10
EQUB &7A
EQUB &10
EQUB &7B
EQUB &10
EQUB &7C
EQUB &10
EQUB &7D
EQUB &10
EQUB &7E
EQUB &10
EQUB &7F
EQUB &10
EQUB &87
EQUB &10
EQUB &8D
EQUB &10
EQUB &8F
EQUB &10
EQUB &9D
EQUB &10
EQUB &AD
EQUB &10
EQUB &BD
EQUB &10
EQUB &C0
EQUB &10
EQUB &C1
EQUB &10
EQUB &C2
EQUB &10
EQUB &C3
EQUB &10
EQUB &C4
EQUB &10
EQUB &C5
EQUB &10
EQUB &C6
EQUB &10
EQUB &C7
EQUB &10
EQUB &C8
EQUB &10
EQUB &C9
EQUB &10
EQUB &CA
EQUB &10
EQUB &CB
EQUB &10
EQUB &CC
EQUB &10
EQUB &CD
EQUB &10
EQUB &CF
EQUB &10
EQUB &D0
EQUB &10
EQUB &D1
EQUB &10
EQUB &D2
EQUB &10
EQUB &D3
EQUB &10
EQUB &D4
EQUB &10
EQUB &D5
EQUB &10
EQUB &D6
EQUB &10
EQUB &D7
EQUB &10
EQUB &D8
EQUB &10
EQUB &D9
EQUB &10
EQUB &DA
EQUB &10
EQUB &DB
EQUB &10
EQUB &E0
EQUB &10
EQUB &E1
EQUB &10
EQUB &E2
EQUB &10
EQUB &E3
EQUB &10
EQUB &E4
EQUB &10
EQUB &E5
EQUB &10
EQUB &E6
EQUB &10
EQUB &E7
EQUB &10
EQUB &E8
EQUB &10
EQUB &E9
EQUB &10
EQUB &EA
EQUB &10
EQUB &EB
EQUB &10
EQUB &F0
EQUB &10
EQUB &F1
EQUB &10
EQUB &F2
EQUB &10
EQUB &F3
EQUB &10
EQUB &F4
EQUB &10
EQUB &F5
EQUB &10
EQUB &F6
EQUB &10
EQUB &F7
EQUB &10
EQUB &F8
EQUB &10
EQUB &F9
EQUB &10
EQUB &FA
EQUB &10
EQUB &FB
EQUB &11
EQUB &00
EQUB &11
EQUB &01
EQUB &11
EQUB &02
EQUB &11
EQUB &03
EQUB &11
EQUB &04
EQUB &11
EQUB &05
EQUB &11
EQUB &06
EQUB &11
EQUB &07
EQUB &11
EQUB &08
EQUB &11
EQUB &09
EQUB &11
EQUB &0A
EQUB &11
EQUB &0B
EQUB &11
EQUB &0C
EQUB &11
EQUB &0D
EQUB &11
EQUB &0E
EQUB &11
EQUB &0F
EQUB &11
EQUB &10
EQUB &11
EQUB &11
EQUB &11
EQUB &12
EQUB &11
EQUB &13
EQUB &11
EQUB &14
EQUB &11
EQUB &15
EQUB &11
EQUB &16
EQUB &11
EQUB &17
EQUB &11
EQUB &18
EQUB &11
EQUB &19
EQUB &11
EQUB &1A
EQUB &11
EQUB &1B
EQUB &11
EQUB &1C
EQUB &11
EQUB &1D
EQUB &11
EQUB &1E
EQUB &11
EQUB &1F
EQUB &11
EQUB &20
EQUB &11
EQUB &21
EQUB &11
EQUB &22
EQUB &11
EQUB &23
EQUB &11
EQUB &24
EQUB &11
EQUB &25
EQUB &11
EQUB &26
EQUB &11
EQUB &27
EQUB &11
EQUB &28
EQUB &11
EQUB &29
EQUB &11
EQUB &2A
EQUB &11
EQUB &2B
EQUB &11
EQUB &2C
EQUB &11
EQUB &2D
EQUB &11
EQUB &2E
EQUB &11
EQUB &2F
EQUB &11
EQUB &3E
EQUB &11
EQUB &40
EQUB &11
EQUB &41
EQUB &11
EQUB &42
EQUB &11
EQUB &44
EQUB &11
EQUB &45
EQUB &11
EQUB &46
EQUB &11
EQUB &47
EQUB &11
EQUB &48
EQUB &11
EQUB &49
EQUB &11
EQUB &4B
EQUB &11
EQUB &4E
EQUB &11
EQUB &50
EQUB &11
EQUB &51
EQUB &11
EQUB &52
EQUB &11
EQUB &54
EQUB &11
EQUB &55
EQUB &11
EQUB &56
EQUB &11
EQUB &57
EQUB &11
EQUB &58
EQUB &11
EQUB &59
EQUB &11
EQUB &5B
EQUB &11
EQUB &5E
EQUB &11
EQUB &60
EQUB &11
EQUB &61
EQUB &11
EQUB &62
EQUB &11
EQUB &63
EQUB &11
EQUB &64
EQUB &11
EQUB &65
EQUB &11
EQUB &66
EQUB &11
EQUB &67
EQUB &11
EQUB &68
EQUB &11
EQUB &69
EQUB &11
EQUB &6A
EQUB &11
EQUB &6B
EQUB &11
EQUB &6C
EQUB &11
EQUB &6D
EQUB &11
EQUB &6E
EQUB &11
EQUB &6F
EQUB &11
EQUB &70
EQUB &11
EQUB &71
EQUB &11
EQUB &72
EQUB &11
EQUB &73
EQUB &11
EQUB &74
EQUB &11
EQUB &75
EQUB &11
EQUB &76
EQUB &11
EQUB &77
EQUB &11
EQUB &78
EQUB &11
EQUB &79
EQUB &11
EQUB &7A
EQUB &11
EQUB &7B
EQUB &11
EQUB &7C
EQUB &11
EQUB &7D
EQUB &11
EQUB &7E
EQUB &11
EQUB &7F
EQUB &11
EQUB &82
EQUB &11
EQUB &84
EQUB &11
EQUB &85
EQUB &11
EQUB &87
EQUB &11
EQUB &88
EQUB &11
EQUB &89
EQUB &11
EQUB &8A
EQUB &11
EQUB &92
EQUB &11
EQUB &94
EQUB &11
EQUB &95
EQUB &11
EQUB &98
EQUB &11
EQUB &99
EQUB &11
EQUB &9A
EQUB &11
EQUB &A2
EQUB &11
EQUB &A4
EQUB &11
EQUB &A5
EQUB &11
EQUB &A8
EQUB &11
EQUB &A9
EQUB &11
EQUB &AA
EQUB &11
EQUB &B2
EQUB &11
EQUB &B4
EQUB &11
EQUB &B5
EQUB &11
EQUB &B8
EQUB &11
EQUB &B9
EQUB &11
EQUB &BA
EQUB &11
EQUB &C2
EQUB &11
EQUB &C3
EQUB &11
EQUB &C4
EQUB &11
EQUB &C5
EQUB &11
EQUB &C7
EQUB &11
EQUB &C8
EQUB &11
EQUB &C9
EQUB &11
EQUB &CA
EQUB &11
EQUB &CC
EQUB &11
EQUB &CD
EQUB &11
EQUB &CE
EQUB &11
EQUB &CF
EQUB &11
EQUB &D2
EQUB &11
EQUB &D3
EQUB &11
EQUB &D4
EQUB &11
EQUB &D5
EQUB &11
EQUB &D8
EQUB &11
EQUB &D9
EQUB &11
EQUB &DA
EQUB &11
EQUB &DC
EQUB &11
EQUB &DD
EQUB &11
EQUB &DE
EQUB &11
EQUB &DF
EQUB &11
EQUB &E2
EQUB &11
EQUB &E3
EQUB &11
EQUB &E4
EQUB &11
EQUB &E5
EQUB &11
EQUB &E8
EQUB &11
EQUB &E9
EQUB &11
EQUB &EA
EQUB &11
EQUB &EC
EQUB &11
EQUB &ED
EQUB &11
EQUB &EE
EQUB &11
EQUB &EF
EQUB &11
EQUB &F2
EQUB &11
EQUB &F3
EQUB &11
EQUB &F4
EQUB &11
EQUB &F5
EQUB &11
EQUB &F8
EQUB &11
EQUB &F9
EQUB &11
EQUB &FA
EQUB &11
EQUB &FC
EQUB &11
EQUB &FD
EQUB &11
EQUB &FE
EQUB &11
EQUB &FF
LDA loop1+2
ADDA #&04
ANDA #&3F
STA loop1+2
ANDA #&FC
LBNE loop1
.exit
LDX oldvec
STX &0232
LDS &0080
RTS
]
NEXT
Z=GET
CALL test
