savs=&80
savu=&82
tmps=&84
tmpu=&86
tmpx=&88
tmpy=&8A
tmpa=&8C
tmpb=&8D
tmp12=&8E
tmp1212=&8F
INPUT "numextra:",numextra
FOR I%=&10 TO &13 STEP 3
P%=&3000
[OPT I%
.test
PSHS CC
STS savs
STU savu
LDA &12
STA tmp12
LDA &1212
STA tmp1212
LDA #&10
STA instr
LDA #&00
STA instr+1+numextra
.loop
CLRA
LDB instr+1+numextra
LDX #skip_table
LDA D,X
BNE skip
LDS savs
LDX #&8111
LDY #&9122
LDU #&A133
LDA #&12
STA instr+2+numextra
STA instr+3+numextra
LDA #&56
LDB #&78
ORCC #&10
PSHS CC
.instr
NOP
]
FOR N=1 TO numextra
[OPT I%
NOP
]
NEXT
[OPT I%
NOP
NOP
NOP
NOP
JMP exposepc
.exposepc
TFR CC,DP
LDS savs
TFR DP,CC
PSHS CC
STS tmps
STU tmpu
STX tmpx
STY tmpy
STA tmpa
STB tmpb
LDA #&00
TFR A,DP
LDA &12
LDA &1212
.skip
INC instr+1+numextra
BNE loop
INC instr
LDA instr
CMPA #&12
BNE loop
LDS savs
LDU savu
LDA tmp12
STA &12
LDA tmp1212
STA &1212
PULS CC
RTS
.skip_table
\ 0x
EQUB 0:EQUB 0:EQUB 0:EQUB 0
EQUB 0:EQUB 0:EQUB 0:EQUB 0
EQUB 0:EQUB 0:EQUB 0:EQUB 0
EQUB 0:EQUB 0:EQUB 1:EQUB 0
\ 1x
EQUB 1:EQUB 1:EQUB 0:EQUB 0
EQUB 1:EQUB 1:EQUB 1:EQUB 1
EQUB 0:EQUB 0:EQUB 0:EQUB 0
EQUB 0:EQUB 0:EQUB 0:EQUB 0
\ 2x
EQUB 1:EQUB 1:EQUB 1:EQUB 1
EQUB 1:EQUB 1:EQUB 1:EQUB 1
EQUB 1:EQUB 1:EQUB 1:EQUB 1
EQUB 1:EQUB 1:EQUB 1:EQUB 1
\ 3x
EQUB 0:EQUB 0:EQUB 0:EQUB 0
EQUB 0:EQUB 0:EQUB 0:EQUB 0
EQUB 0:EQUB 1:EQUB 0:EQUB 1
EQUB 1:EQUB 0:EQUB 1:EQUB 1
\ 4x
EQUB 0:EQUB 0:EQUB 0:EQUB 0
EQUB 0:EQUB 0:EQUB 0:EQUB 0
EQUB 0:EQUB 0:EQUB 0:EQUB 0
EQUB 0:EQUB 0:EQUB 0:EQUB 0
\ 5x
EQUB 0:EQUB 0:EQUB 0:EQUB 0
EQUB 0:EQUB 0:EQUB 0:EQUB 0
EQUB 0:EQUB 0:EQUB 0:EQUB 0
EQUB 0:EQUB 0:EQUB 0:EQUB 0
\ 6x
EQUB 0:EQUB 0:EQUB 0:EQUB 0
EQUB 0:EQUB 0:EQUB 0:EQUB 0
EQUB 0:EQUB 0:EQUB 0:EQUB 0
EQUB 0:EQUB 0:EQUB 1:EQUB 0
\ 7x
EQUB 0:EQUB 0:EQUB 0:EQUB 0
EQUB 0:EQUB 0:EQUB 0:EQUB 0
EQUB 0:EQUB 0:EQUB 0:EQUB 0
EQUB 0:EQUB 0:EQUB 1:EQUB 0
\ 8x
EQUB 0:EQUB 0:EQUB 0:EQUB 0
EQUB 0:EQUB 0:EQUB 0:EQUB 0
EQUB 0:EQUB 0:EQUB 0:EQUB 0
EQUB 0:EQUB 1:EQUB 0:EQUB 0
\ 9x
EQUB 0:EQUB 0:EQUB 0:EQUB 0
EQUB 0:EQUB 0:EQUB 0:EQUB 0
EQUB 0:EQUB 0:EQUB 0:EQUB 0
EQUB 0:EQUB 1:EQUB 0:EQUB 0
\ Ax
EQUB 0:EQUB 0:EQUB 0:EQUB 0
EQUB 0:EQUB 0:EQUB 0:EQUB 0
EQUB 0:EQUB 0:EQUB 0:EQUB 0
EQUB 0:EQUB 1:EQUB 0:EQUB 0
\ Bx
EQUB 0:EQUB 0:EQUB 0:EQUB 0
EQUB 0:EQUB 0:EQUB 0:EQUB 0
EQUB 0:EQUB 0:EQUB 0:EQUB 0
EQUB 0:EQUB 1:EQUB 0:EQUB 0
\ Cx
EQUB 0:EQUB 0:EQUB 0:EQUB 0
EQUB 0:EQUB 0:EQUB 0:EQUB 0
EQUB 0:EQUB 0:EQUB 0:EQUB 0
EQUB 0:EQUB 1:EQUB 0:EQUB 0
\ Dx
EQUB 0:EQUB 0:EQUB 0:EQUB 0
EQUB 0:EQUB 0:EQUB 0:EQUB 0
EQUB 0:EQUB 0:EQUB 0:EQUB 0
EQUB 0:EQUB 0:EQUB 0:EQUB 0
\ Ex
EQUB 0:EQUB 0:EQUB 0:EQUB 0
EQUB 0:EQUB 0:EQUB 0:EQUB 0
EQUB 0:EQUB 0:EQUB 0:EQUB 0
EQUB 0:EQUB 0:EQUB 0:EQUB 0
\ Fx
EQUB 0:EQUB 0:EQUB 0:EQUB 0
EQUB 0:EQUB 0:EQUB 0:EQUB 0
EQUB 0:EQUB 0:EQUB 0:EQUB 0
EQUB 0:EQUB 0:EQUB 0:EQUB 0
]
NEXT
PRINT "Press a key to run test"
Z=GET
FOR P=&10 TO &11
FOR I=1 TO numextra
I?instr=P
NEXT
CALL test
NEXT
