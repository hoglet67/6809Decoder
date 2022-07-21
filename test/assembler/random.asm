
       ORG   $C000

ZP_RND_WA EQU $10
ZP_FP_TMP EQU $20
TEST      EQU $7E00

BEGIN  BRA   RESET

RESET  LDS   #$200

       JSR   ctrInc


       ;; Seed the random number generator from VIA T1C
       LDD   $fe44
       STD   ZP_RND_WA
       LDD   $fe44
       STD   ZP_RND_WA + 2
       LDA   $fe44
       STA   ZP_RND_WA + 4

       LDX  #TEST
writeLoop
       BSR  rndNext
       LDA  ZP_RND_WA + 0
       STA  ,X+
       LDA  ZP_RND_WA + 1
       STA  ,X+
       LDA  ZP_RND_WA + 2
       STA  ,X+
       LDA  ZP_RND_WA + 3
       STA  ,X+
       CMPA #TEST+$200
       BNE  writeLoop

       JMP  TEST+$100

       ;; 32-bit PRBS from BBC Basic
rndNext
		LDB	#4
		STB	ZP_FP_TMP
rndLoop
		ROR	ZP_RND_WA + 4
		LDA	ZP_RND_WA + 0
		TFR	A,B
		RORA
		STA 	ZP_RND_WA + 4
		LDA 	ZP_RND_WA + 1
		STA 	ZP_RND_WA + 0
		LSRA
		EORA	ZP_RND_WA + 2
		ANDA	#$0F
		EORA	ZP_RND_WA + 2
		RORA
		RORA
		RORA
		RORA
		EORA	ZP_RND_WA + 4
		STB	ZP_RND_WA + 4
		LDB	ZP_RND_WA + 2
		STB	ZP_RND_WA + 1
		LDB	ZP_RND_WA + 3
		STB	ZP_RND_WA + 2
		STA	ZP_RND_WA + 3
		DEC	ZP_FP_TMP
		BNE	rndLoop
		RTS

      ;; Inc

ctrInc
      LDX #$7C07
ctrLoop
      LDA  ,X
      ADDA #1
      STA  ,X
      CMPA #$3A
      BLT  ctrDone
      LDA  #$30
      STA  ,X
      LEAX -1,X
      CMPX #$7BFF
      BNE  ctrLoop
ctrDone
      RTS

      ORG $F7F0

      FDB RESET
      FDB RESET
      FDB RESET
      FDB RESET
      FDB RESET
      FDB RESET
      FDB RESET
      FDB RESET

      ORG $FFF0

      FDB RESET
      FDB RESET
      FDB RESET
      FDB RESET
      FDB RESET
      FDB RESET
      FDB RESET
      FDB RESET

      END BEGIN
