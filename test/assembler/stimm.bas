@%=0
N%=12
DIM TD%(N%,4)
DIM TN$(N%)
FOR A%=0 TO N%-1
READ TN$(A%)
PRINT CHR$(ASC("A")+A%);") ";TN$(A%);" [ ";
FOR B%=0 TO 3
READ TD%(A%,B%)
PRINT ~TD%(A%,B%);" ";
NEXT
PRINT " ]"
NEXT
PRINT "Press A-";CHR$(ASC("A")+N%-1);" to select test"
REPEAT
Z%=GET-ASC("A")
UNTIL Z%>=0 AND Z%<N%
PRINT "Randomizing 5000-5400"
FOR A%=&5000 TO &53FF STEP 4
!A%=RND
NEXT
PRINT "Assembling ";TN$(Z%)
:
opcode=&80
prefix=&81
acca=&82
stack=&83
param=&08
:
FOR I%=&10 TO &12 STEP 2
P%=&3000
[OPT I%
.test
PSHS CC
ORCC #&50
STS <stack
CLR <acca
CLR <opcode
LDA #&12
STA <prefix
.loop
\ Setup two instruction test sequence
LDA #TD%(Z%,0)
STA instr2
LDA #TD%(Z%,1)
STA instr2+1
LDA #TD%(Z%,2)
STA instr2+2
LDA #TD%(Z%,3)
STA instr2+3
\ Make the ZP and Extended modes
LDD #&F000
STD <param
LDD #&1000
STD &1200+param
LDA #&12
STA instr1
STA instr1+1
STA instr1+2
LDA #param
STA instr1+3
LDX #op_table
LDA <opcode
LDB A,X
BMI skip
\ B is instr len (1,2,3)
NEGB
\ B is offset before instr2 (-1,-2,-3)
LDX #instr2
STA B,X
\ Add a prefix
LEAX -1,X
LDA <prefix
STA B,X
\ Load A from counter
LDA <acca
\ Load X,Y,U,S with random values from Basic ROM
LDS #&8080
LEAS A,S
LDB ,S+
LDX #&8080
LEAX B,X
LDB ,S+
LDY #&5180
LEAY B,Y
LDB ,S+
LDU #&5280
LEAU B,U
LDB ,S+
LDS #&5380
LEAS B,S
\ Load B from inverse of conuter
LDB <acca
COMB
.instr1 \ Some other instruction
NOP
NOP
NOP
NOP
.instr2 \ The store #imm instruction
NOP
NOP
NOP
NOP
TFR CC,DP
LDS stack
TFR DP,CC
PSHS CC,A,B
CLRA
TFR A,DP
INC <acca
LBNE loop
.skip
INC <opcode
LBNE loop
LDA <prefix
SUBA #1
STA <prefix
CMPA #&10
LBGE loop
LDS <stack
PULS CC
RTS
\ 8x
EQUB &02:EQUB &02:EQUB &02:EQUB &03
EQUB &02:EQUB &02:EQUB &02:EQUB &02
EQUB &02:EQUB &02:EQUB &02:EQUB &02
EQUB &03:EQUB &FF:EQUB &03:EQUB &02
\ 9x
EQUB &02:EQUB &02:EQUB &02:EQUB &02
EQUB &02:EQUB &02:EQUB &02:EQUB &02
EQUB &02:EQUB &02:EQUB &02:EQUB &02
EQUB &02:EQUB &FF:EQUB &02:EQUB &02
\ Ax
EQUB &02:EQUB &02:EQUB &02:EQUB &02
EQUB &02:EQUB &02:EQUB &02:EQUB &02
EQUB &02:EQUB &02:EQUB &02:EQUB &02
EQUB &02:EQUB &FF:EQUB &02:EQUB &02
\ Bx
EQUB &03:EQUB &03:EQUB &03:EQUB &03
EQUB &03:EQUB &03:EQUB &03:EQUB &03
EQUB &03:EQUB &03:EQUB &03:EQUB &03
EQUB &03:EQUB &FF:EQUB &03:EQUB &03
\ Cx
EQUB &02:EQUB &02:EQUB &02:EQUB &03
EQUB &02:EQUB &02:EQUB &02:EQUB &02
EQUB &02:EQUB &02:EQUB &02:EQUB &02
EQUB &03:EQUB &FF:EQUB &03:EQUB &02
\ Dx
EQUB &02:EQUB &02:EQUB &02:EQUB &02
EQUB &02:EQUB &02:EQUB &02:EQUB &02
EQUB &02:EQUB &02:EQUB &02:EQUB &02
EQUB &02:EQUB &02:EQUB &02:EQUB &02
\ Ex
EQUB &02:EQUB &02:EQUB &02:EQUB &02
EQUB &02:EQUB &02:EQUB &02:EQUB &02
EQUB &02:EQUB &02:EQUB &02:EQUB &02
EQUB &02:EQUB &02:EQUB &02:EQUB &02
\ Fx
EQUB &03:EQUB &03:EQUB &03:EQUB &03
EQUB &03:EQUB &03:EQUB &03:EQUB &03
EQUB &03:EQUB &03:EQUB &03:EQUB &03
EQUB &03:EQUB &03:EQUB &03:EQUB &03
.op_table
\ 0x
EQUB &02:EQUB &02:EQUB &02:EQUB &02
EQUB &02:EQUB &02:EQUB &02:EQUB &02
EQUB &02:EQUB &02:EQUB &02:EQUB &02
EQUB &02:EQUB &02:EQUB &FF:EQUB &02
\ 1x
EQUB &FF:EQUB &FF:EQUB &01:EQUB &FF
EQUB &FF:EQUB &FF:EQUB &FF:EQUB &FF
EQUB &FF:EQUB &01:EQUB &FF:EQUB &FF
EQUB &FF:EQUB &01:EQUB &02:EQUB &02
\ 2x
EQUB &FF:EQUB &FF:EQUB &FF:EQUB &FF
EQUB &FF:EQUB &FF:EQUB &FF:EQUB &FF
EQUB &FF:EQUB &FF:EQUB &FF:EQUB &FF
EQUB &FF:EQUB &FF:EQUB &FF:EQUB &FF
\ 3x
EQUB &02:EQUB &02:EQUB &FF:EQUB &02
EQUB &02:EQUB &02:EQUB &02:EQUB &02
EQUB &FF:EQUB &FF:EQUB &01:EQUB &FF
EQUB &FF:EQUB &01:EQUB &FF:EQUB &FF
\ 4x
EQUB &01:EQUB &01:EQUB &01:EQUB &01
EQUB &01:EQUB &01:EQUB &01:EQUB &01
EQUB &01:EQUB &01:EQUB &01:EQUB &01
EQUB &01:EQUB &01:EQUB &01:EQUB &01
\ 5x
EQUB &01:EQUB &01:EQUB &01:EQUB &01
EQUB &01:EQUB &01:EQUB &01:EQUB &01
EQUB &01:EQUB &01:EQUB &01:EQUB &01
EQUB &01:EQUB &01:EQUB &01:EQUB &01
\ 6x
EQUB &02:EQUB &02:EQUB &02:EQUB &02
EQUB &02:EQUB &02:EQUB &02:EQUB &02
EQUB &02:EQUB &02:EQUB &02:EQUB &02
EQUB &02:EQUB &02:EQUB &FF:EQUB &02
\ 7x
EQUB &03:EQUB &03:EQUB &03:EQUB &03
EQUB &03:EQUB &03:EQUB &03:EQUB &03
EQUB &03:EQUB &03:EQUB &03:EQUB &03
EQUB &03:EQUB &03:EQUB &FF:EQUB &03
]
NEXT
PRINT "Press a key to run test"
Z=GET
PRINT "Running..."
CALL test
PRINT "Done!"
DATA "    STA #imm8 ",&87,&AA,&12,&12
DATA "    STX #imm16",&8F,&AA,&BB,&12
DATA "    STB #imm8 ",&C7,&AA,&12,&12
DATA "    STU #imm16",&CF,&AA,&BB,&12
DATA "(p) STA #imm8 ",&10,&87,&AA,&12
DATA "    STY #imm16",&10,&8F,&AA,&BB
DATA "(p) STB #imm8 ",&10,&C7,&AA,&12
DATA "    STS #imm16",&10,&CF,&AA,&BB
DATA "(p) STA #imm8 ",&11,&87,&AA,&12
DATA "(p) STX #imm16",&11,&8F,&AA,&BB
DATA "(p) STB #imm8 ",&11,&C7,&AA,&12
DATA "(p) STU #imm16",&11,&CF,&AA,&BB
