FOR I%=&10 TO &13 STEP 3
P%=&3000
[OPT I%
.test
ORCC  #&50
LDX #cc_vals
.loop1
LDA ,X+
BEQ done
PSHS A
LDB #&00
.loop2
LDY #&FFFF
TFR B,DP
PULS CC
PSHS CC
ADDR DP,Y
PSHS CC,Y
PULS CC,Y
INCB
BNE loop2
PULS A
BRA loop1
.done
CLRA
TFR A,DP
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
