savs=&80
savu=&82
tmps=&84
tmpu=&86
tmpx=&88
tmpy=&8A
tmpa=&8C
illegalvec=&232
FOR I%=&10 TO &13 STEP 3
P%=&3000
[OPT I%
.test
STS savs
STU savu
LDX illegalvec
STX oldvec
LDX #next
STX illegalvec
.loop1
.ldmd_instr
LDMD #&00
LDA #&00
STA lda_instr+1
.loop2
LDS savs
LDU #&9100
LDX #&A100
LDY #&B100
.lda_instr
LDA ,X
NOP
NOP
NOP
NOP
.next
STA tmpa
STS tmps
STU tmpu
STX tmpx
STY tmpy
INC lda_instr+1
BNE loop2
LDA ldmd_instr+2
EORA #&01
STA ldmd_instr+2
BNE loop1
LDX oldvec
STX illegalvec
LDS savs
LDU savu
RTS
.oldvec
EQUW &0000
]
NEXT
PRINT "Press a key to run test"
Z=GET
CALL test
