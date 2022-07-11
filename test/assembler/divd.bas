FOR I%=&10 TO &13 STEP 3
P%=&3000
[OPT I%
.dotest
LDY #&0000
.loop
STY &82
TFR Y,D
ANDCC #&F0
.orcc
ORCC #&00
DIVD &80
PSHS CC
PULS CC
STD  &82
LEAY 1,Y
BNE loop
RTS
.divisor
EQUB &01
EQUB &10
EQUB &7F
EQUB &80
EQUB &F0
EQUB &FF
EQUB &00
.test
LDA #&00
STA orcc+1
.loop1
LDX #&0000
.loop2
LDA divisor,X
STA &80
BEQ done
JSR dotest
LEAX 1,X
BRA loop2
.done
LDA orcc+1
EORA #&0F
STA orcc+1
BNE loop1
LDA #&0B
STA orcc+1
LDA #&00
STA &80
LDY #&FFFF
JSR loop
RTS
]
NEXT
PRINT "Press a key to run test"
Z=GET
CALL test
