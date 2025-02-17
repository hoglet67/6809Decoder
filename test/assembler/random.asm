
       ORG   $C000

ZP_RND_WA EQU $10
ZP_FP_TMP EQU $20
TEST      EQU $7E00

BEGIN

RESET  ;; Push all the registers so we expose state
       PSHS  CC,A,B,DP,X,Y,U
       LDS   #$200
       ORCC  #$50

       ;; Disable all interrupts
       LDA   #$7F
       STA   $FE4E
       STA   $FE6E

       ;; Page in ROM 0
       LDA   #$00
       STA   $FE30

       ;; Test if we are already write protected
       LDA   $FE31
       BITA  #$10
       BNE   PROT

       ;; Enable reset from config pin 14
       LDB   #$FF
       STB   $FE35

       ;; Set the write protect bit (just once!)
       ORA   #$10
       STA   $FE31

       ;; Clear the screen and set the counter
       LDX   #$7C00
       LDA   #'0'
CLEAR1
       STA   ,X+
       CMPX  #$7C08
       BNE   CLEAR1
       CLRA
CLEAR2
       STA   ,X+
       CMPX  #$8000
       BNE   CLEAR2

PROT
       ;; Clear the data pointer, so the PRBS seed is accessible
       CLRA
       TFR   A,DP
       JSR   ctrInc

       ;; Seed the random number generator from VIA T1C
       LDD   $fe44
       STD   <ZP_RND_WA
;;       LDD   $fe44
;;       STD   <ZP_RND_WA + 2
;;       LDA   $fe44
;;       STA   <ZP_RND_WA + 4

       LDX  #TEST
writeLoop
       BSR  rndNext
       LDA  <ZP_RND_WA + 0
       STA  ,X+
       LDA  <ZP_RND_WA + 1
       STA  ,X+
       LDA  <ZP_RND_WA + 2
       STA  ,X+
       LDA  <ZP_RND_WA + 3
       STA  ,X+
       CMPX #TEST+$80
       BNE  writeLoop

       ;; Clear unused regsters to avoid false memory errors
       LDU   #$4000
       LDW   #$0000
       LDY   #$0000
       TFR   Y,V

       ;; Set the new M register to $AA
       LDA  #$55
       STA  $0000
       COM  $0000

       JMP  TEST

       ;; 32-bit PRBS from BBC Basic
rndNext
		LDB	#4
		STB	<ZP_FP_TMP
rndLoop
		ROR	<ZP_RND_WA + 4
		LDA	<ZP_RND_WA + 0
		TFR	A,B
		RORA
		STA 	<ZP_RND_WA + 4
		LDA 	<ZP_RND_WA + 1
		STA 	<ZP_RND_WA + 0
		LSRA
		EORA	<ZP_RND_WA + 2
		ANDA	#$0F
		EORA	<ZP_RND_WA + 2
		RORA
		RORA
		RORA
		RORA
		EORA	<ZP_RND_WA + 4
		STB	<ZP_RND_WA + 4
		LDB	<ZP_RND_WA + 2
		STB	<ZP_RND_WA + 1
		LDB	<ZP_RND_WA + 3
		STB	<ZP_RND_WA + 2
		STA	<ZP_RND_WA + 3
		DEC	<ZP_FP_TMP
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

ILL_HANDLER
      LDX #$0000
      ADDR DP,X
      PSHS X
      PULS X
      LDX #$0000
      ADDR CC,X
      PSHS X
      PULS X
      RTI
SWI3_HANDLER
      NOP
      RTI
SWI2_HANDLER
      NOP
      RTI
FIRQ_HANDLER
      NOP
      RTI
IRQ_HANDLER
      NOP
      PSHS A
      LDA #$7F
      STA $FE4D
      STA $FE6D
      PULS A
      RTI
SWI_HANDLER
      NOP
      RTI
NMI_HANDLER
      NOP
      JMP RESET
RST_HANDLER
      NOP
      JMP RESET

      ORG $F7F0

      FDB ILL_HANDLER
      FDB SWI3_HANDLER
      FDB SWI2_HANDLER
      FDB FIRQ_HANDLER
      FDB IRQ_HANDLER
      FDB SWI_HANDLER
      FDB NMI_HANDLER
      FDB RST_HANDLER

      ORG $FFF0

      FDB ILL_HANDLER
      FDB SWI3_HANDLER
      FDB SWI2_HANDLER
      FDB FIRQ_HANDLER
      FDB IRQ_HANDLER
      FDB SWI_HANDLER
      FDB NMI_HANDLER
      FDB RST_HANDLER

      END BEGIN
