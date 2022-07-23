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
MULD &80
PSHS CC
PULS CC
STD  &82
STW  &84
LEAY 1,Y
BNE loop
RTS
.multiplier
EQUW &0001
EQUW &0010
EQUW &0100
EQUW &1000
EQUW &7FFF
EQUW &8000
EQUW &F000
EQUW &FF00
EQUW &FFF0
EQUW &FFFF
EQUW &0000
.test
LDA #&00
STA orcc+1
.loop1
LDX #&0000
.loop2
LDD multiplier,X
STD &80
BEQ done
JSR dotest
LEAX 2,X
BRA loop2
.done
LDA orcc+1
EORA #&0F
STA orcc+1
BNE loop1
RTS
]
NEXT
PRINT "Press a key to run test"
Z=GET
CALL test
