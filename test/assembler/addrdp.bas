FOR I%=&10 TO &13 STEP 3
P%=&3000
[OPT I%
.test
ORCC #&50
LDMD #&00
CLRA
STA  &80
COM  &80
LDY  #cc_vals
.loop1
LDA  ,Y+
BEQ done
PSHS A
LDB  #&00
.loop2
LDX  #&0000
TFR  B,DP
PULS CC
PSHS CC
ADDR DP,X
PSHS CC,X
PULS CC,X
INCB
BNE loop2
PULS A
BRA loop1
.done
CLRA
TFR  A,DP
LDMD #&01
ANDCC #&AF
RTS
.cc_vals
EQUB &D0
EQUB &D1
EQUB &D2
EQUB &D3
EQUB &F0
EQUB &F1
EQUB &F2
EQUB &F3
EQUB &00
]
NEXT
PRINT "Press a key to run test"
Z=GET
CALL test
