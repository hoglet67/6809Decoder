FOR I%=&10 TO &13 STEP 3
P%=&3000
[OPT I%
.test
LDX #cc_vals
.loop1
LDA ,X+
BEQ done
PSHS A
LDB #&00
.loop2
TFR B,A
PULS CC
PSHS CC
DAA
PSHS CC,A
PULS CC,A
INCB
BNE loop2
PULS A
BRA loop1
.done
RTS
.cc_vals
EQUB &C0
EQUB &C1
EQUB &C2
EQUB &C3
EQUB &E0
EQUB &E1
EQUB &E2
EQUB &E3
EQUB &00
]
NEXT
PRINT "Press a key to run test"
Z=GET
CALL test