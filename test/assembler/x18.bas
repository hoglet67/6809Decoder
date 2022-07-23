FOR I%=&10 TO &13 STEP 3
P%=&3000
[OPT I%
.test
LDA #0
.loop
PSHS A
PULS CC
EQUB &18
NOP
PSHS CC,A
PULS CC,A
INCA
BNE loop
RTS
]
NEXT
PRINT "Press a key to run test"
Z=GET
CALL test
