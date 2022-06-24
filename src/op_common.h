
// ====================================================================
// Individual Instructions
// ====================================================================

static int op_fn_ABX(operand_t operand, ea_t ea, sample_t *sample_q) {
   // X = X + B
   if (X >= 0 && ACCB >= 0) {
      X = (X + ACCB) & 0xFFFF;
   } else {
      X = -1;
   }
   return -1;
}

static int add_helper(int val, int cin, operand_t operand) {
   if (val >= 0 && cin >= 0) {
      int tmp = val + operand + cin;
      // The carry flag is bit 8 of the result
      C = (tmp >> 8) & 1;
      // The overflow flag is: IF ((a^b^res^(res>>1))&0x80) SEV else CLV
      V = (((val ^ operand ^ tmp) >> 7) & 1) ^ C;
      // The half carry flag is: IF ((a^b^res)&0x10) SEH else CLH
      H =  ((val ^ operand ^ tmp) >> 4) & 1;
      // Truncate the result to 8 bits
      tmp &= 0xff;
      // Set the flags
      set_NZ(tmp);
      // Save the result back to the register
      return tmp;
   } else {
      set_HNZVC_unknown();
      return -1;
   }
}

static int op_fn_ADCA(operand_t operand, ea_t ea, sample_t *sample_q) {
   ACCA = add_helper(ACCA, C, operand);
   return -1;
}

static int op_fn_ADCB(operand_t operand, ea_t ea, sample_t *sample_q) {
   ACCB = add_helper(ACCB, C, operand);
   return -1;
}

static int op_fn_ADDA(operand_t operand, ea_t ea, sample_t *sample_q) {
   ACCA = add_helper(ACCA, 0, operand);
   return -1;
}

static int op_fn_ADDB(operand_t operand, ea_t ea, sample_t *sample_q) {
   ACCB = add_helper(ACCB, 0, operand);
   return -1;
}

static int op_fn_ADDD(operand_t operand, ea_t ea, sample_t *sample_q) {
   if (ACCA >= 0 && ACCB >= 0) {
      int d = (ACCA << 8) + ACCB;
      // Perform the addition (there is no carry in)
      int tmp = d + operand;
      // The carry flag is bit 16 of the result
      C = (tmp >> 16) & 1;
      // The overflow flag is: IF ((a^b^res^(res>>1))&0x80) SEV else CLV
      V = (((d ^ operand ^ tmp) >> 15) & 1) ^ C;
      // Truncate the result to 16 bits
      tmp &= 0xFFFF;
      // Set the flags
      set_NZ16(tmp);
      // Unpack back into A and B
      ACCA = (tmp >> 8) & 0xff;
      ACCB = tmp & 0xff;
   } else {
      ACCA = -1;
      ACCB = -1;
      set_NZVC_unknown();
   }
   return -1;
}

static int and_helper(int val, operand_t operand) {
   if (val >= 0) {
      val &= operand;
      set_NZ(val);
   } else {
      set_NZ_unknown();
   }
   V = 0;
   return val;
}

static int op_fn_ANDA(operand_t operand, ea_t ea, sample_t *sample_q) {
   ACCA = and_helper(ACCA, operand);
   return -1;
}

static int op_fn_ANDB(operand_t operand, ea_t ea, sample_t *sample_q) {
   ACCB = and_helper(ACCB, operand);
   return -1;
}

static int op_fn_ANDC(operand_t operand, ea_t ea, sample_t *sample_q) {
   if (!(operand & 0x80)) {
      E = 0;
   }
   if (!(operand & 0x40)) {
      F = 0;
   }
   if (!(operand & 0x20)) {
      H = 0;
   }
   if (!(operand & 0x10)) {
      I = 0;
   }
   if (!(operand & 0x08)) {
      N = 0;
   }
   if (!(operand & 0x04)) {
      Z = 0;
   }
   if (!(operand & 0x02)) {
      V = 0;
   }
   if (!(operand & 0x01)) {
      C = 0;
   }
   return -1;
}

static int asl_helper(int val) {
   if (val >= 0) {
      C = (val >> 7) & 1;
      val = (val << 1) & 0xff;
      V = (val >> 7) & C & 1;
      set_NZ(val);
   } else {
      set_NZVC_unknown();
   }
   // The datasheet says the half-carry flag is undefined, but in practice
   // it seems to be unchanged (i.e. no errors). TODO: needs further testing
   // H = -1;
   return val;
}

static int op_fn_ASL(operand_t operand, ea_t ea, sample_t *sample_q) {
   return asl_helper(operand);
}

static int op_fn_ASLA(operand_t operand, ea_t ea, sample_t *sample_q) {
   ACCA = asl_helper(ACCA);
   return -1;
}

static int op_fn_ASLB(operand_t operand, ea_t ea, sample_t *sample_q) {
   ACCB = asl_helper(ACCB);
   return -1;
}

static int asr_helper(int val) {
   if (val >= 0) {
      C = val & 1;
      val = (val & 0x80) | (val >> 1);
      set_NZ(val);
   } else {
      set_NZC_unknown();
   }
   // The datasheet says the half-carry flag is undefined, but in practice
   // it seems to be unchanged (i.e. no errors). TODO: needs further testing
   // H = -1;
   return val;
}

static int op_fn_ASR(operand_t operand, ea_t ea, sample_t *sample_q) {
   return asr_helper(operand);
}

static int op_fn_ASRA(operand_t operand, ea_t ea, sample_t *sample_q) {
   ACCA = asr_helper(ACCA);
   return -1;
}

static int op_fn_ASRB(operand_t operand, ea_t ea, sample_t *sample_q) {
   ACCB = asr_helper(ACCB);
   return -1;
}


static int op_fn_BCC(operand_t operand, ea_t ea, sample_t *sample_q) {
   if (C < 0) {
      PC = -1;
   } else if (C == 0) {
      PC = ea;
   }
   return -1;
}

static int op_fn_BEQ(operand_t operand, ea_t ea, sample_t *sample_q) {
   if (Z < 0) {
      PC = -1;
   } else if (Z == 1) {
      PC = ea;
   }
   return -1;
}

static int op_fn_BGE(operand_t operand, ea_t ea, sample_t *sample_q) {
   if (N < 0 || V < 0) {
      PC = -1;
   } else if (N == V) {
      PC = ea;
   }
   return -1;
}

static int op_fn_BGT(operand_t operand, ea_t ea, sample_t *sample_q) {
   if (Z < 0 || N < 0 || V < 0) {
      PC = -1;
   } else if (Z == 0 && N == V) {
      PC = ea;
   }
   return -1;
}

static int op_fn_BHI(operand_t operand, ea_t ea, sample_t *sample_q) {
   if (Z < 0 || C < 0) {
      PC = -1;
   } else if (Z == 0 && C == 0) {
      PC = ea;
   }
   return -1;
}

static void bit_helper(int val, operand_t operand) {
   if (val >= 0) {
      set_NZ(val & operand);
   } else {
      set_NZ_unknown();
   }
   V = 0;
}

static int op_fn_BITA(operand_t operand, ea_t ea, sample_t *sample_q) {
   bit_helper(ACCA, operand);
   return -1;
}

static int op_fn_BITB(operand_t operand, ea_t ea, sample_t *sample_q) {
   bit_helper(ACCB, operand);
   return -1;
}

static int op_fn_BLE(operand_t operand, ea_t ea, sample_t *sample_q) {
   // TODO: Overly pessimistic
   if (Z < 0 || N < 0 || V < 0) {
      PC = -1;
   } else if (Z == 0 || N != V) {
      PC = ea;
   }
   return -1;
}

static int op_fn_BLO(operand_t operand, ea_t ea, sample_t *sample_q) {
   if (C < 0) {
      PC = -1;
   } else if (C == 1) {
      PC = ea;
   }
   return -1;
}

static int op_fn_BLS(operand_t operand, ea_t ea, sample_t *sample_q) {
   if (Z < 0 || C < 0) {
      PC = -1;
   } else if (Z == 1 || C == 1) {
      PC = ea;
   }
   return -1;
}

static int op_fn_BLT(operand_t operand, ea_t ea, sample_t *sample_q) {
   if (N < 0 || V < 0) {
      PC = -1;
   } else if (N != V) {
      PC = ea;
   }
   return -1;
}

static int op_fn_BMI(operand_t operand, ea_t ea, sample_t *sample_q) {
   if (N < 0) {
      PC = -1;
   } else if (N == 1) {
      PC = ea;
   }
   return -1;
}

static int op_fn_BNE(operand_t operand, ea_t ea, sample_t *sample_q) {
   if (Z < 0) {
      PC = -1;
   } else if (Z == 0) {
      PC = ea;
   }
   return -1;
}

static int op_fn_BPL(operand_t operand, ea_t ea, sample_t *sample_q) {
   if (N < 0) {
      PC = -1;
   } else if (N == 0) {
      PC = ea;
   }
   return -1;
}

static int op_fn_BRA(operand_t operand, ea_t ea, sample_t *sample_q) {
   PC = ea;
   return -1;
}

static int op_fn_BRN(operand_t operand, ea_t ea, sample_t *sample_q) {
   return -1;
}

static int op_fn_BSR(operand_t operand, ea_t ea, sample_t *sample_q) {
   // operand is actually byte swapped at this point
   push8s(operand >> 8); // this pushes the low byte
   push8s(operand);      // this pushes he high byte
   PC = ea & 0xffff;
   return -1;
}

static int op_fn_BVC(operand_t operand, ea_t ea, sample_t *sample_q) {
   if (V < 0) {
      PC = -1;
   } else if (V == 0) {
      PC = ea;
   }
   return -1;
}

static int op_fn_BVS(operand_t operand, ea_t ea, sample_t *sample_q) {
   if (V < 0) {
      PC = -1;
   } else if (V == 1) {
      PC = ea;
   }
   return -1;
}

static int clr_helper() {
   N = 0;
   Z = 1;
   C = 0;
   V = 0;
   return 0;

}
static int op_fn_CLR(operand_t operand, ea_t ea, sample_t *sample_q) {
   return clr_helper();
}

static int op_fn_CLRA(operand_t operand, ea_t ea, sample_t *sample_q) {
   ACCA = clr_helper();
   return -1;
}

static int op_fn_CLRB(operand_t operand, ea_t ea, sample_t *sample_q) {
   ACCB = clr_helper();
   return -1;
}

static void cmp_helper8(int val, operand_t operand) {
   if (val >= 0) {
      int tmp = val - operand;
      // The carry flag is bit 8 of the result
      C = (tmp >> 8) & 1;
      // The overflow flag is: IF ((a^b^res^(res>>1))&0x80) SEV else CLV
      V = (((val ^ operand ^ tmp) >> 7) & 1) ^ C;
      tmp &= 0xff;
      set_NZ(tmp);
   } else {
      set_NZVC_unknown();
   }
   // The datasheet says the half-carry flag is undefined, but in practice
   // it seems to be unchanged (i.e. no errors). TODO: needs further testing
   // H = -1;
}

static void cmp_helper16(int val, operand_t operand) {
   if (val >= 0) {
      int tmp = val - operand;
      // The carry flag is bit 16 of the result
      C = (tmp >> 16) & 1;
      // The overflow flag is: IF ((a^b^res^(res>>1))&0x8000) SEV else CLV
      V = (((val ^ operand ^ tmp) >> 15) & 1) ^ C;
      tmp &= 0xffff;
      set_NZ16(tmp);
   } else {
      set_NZVC_unknown();
   }
}

static int op_fn_CMPA(operand_t operand, ea_t ea, sample_t *sample_q) {
   cmp_helper8(ACCA, operand);
   return -1;
}

static int op_fn_CMPB(operand_t operand, ea_t ea, sample_t *sample_q) {
   cmp_helper8(ACCB, operand);
   return -1;
}

static int op_fn_CMPD(operand_t operand, ea_t ea, sample_t *sample_q) {
   int D = (ACCA << 8) + ACCB;
   cmp_helper16(D, operand);
   return -1;
}

static int op_fn_CMPS(operand_t operand, ea_t ea, sample_t *sample_q) {
   cmp_helper16(S, operand);
   return -1;
}

static int op_fn_CMPU(operand_t operand, ea_t ea, sample_t *sample_q) {
   cmp_helper16(U, operand);
   return -1;
}

static int op_fn_CMPX(operand_t operand, ea_t ea, sample_t *sample_q) {
   cmp_helper16(X, operand);
   return -1;
}

static int op_fn_CMPY(operand_t operand, ea_t ea, sample_t *sample_q) {
   cmp_helper16(Y, operand);
   return -1;
}

static int com_helper(int val) {
   if (val >= 0) {
      val ^= 0xff;
      set_NZ(val);
   } else {
      set_NZ_unknown();
   }
   V = 0;
   C = 1;
   return val;
}

static int op_fn_COM(operand_t operand, ea_t ea, sample_t *sample_q) {
   return com_helper(operand);
}

static int op_fn_COMA(operand_t operand, ea_t ea, sample_t *sample_q) {
   ACCA = com_helper(ACCA);
   return -1;
}

static int op_fn_COMB(operand_t operand, ea_t ea, sample_t *sample_q) {
   ACCB = com_helper(ACCB);
   return -1;
}

static int op_fn_CWAI(operand_t operand, ea_t ea, sample_t *sample_q) {
   // TODO
   return -1;
}

static int op_fn_DAA(operand_t operand, ea_t ea, sample_t *sample_q) {
   if (ACCA >= 0 && H >= 0 && C >= 0) {
      int correction = 0x00;
      if (H == 1 && (ACCA & 0x0F) > 0x09) {
         correction |= 0x06;
      }
      if (C == 1 || (ACCA & 0xF0) > 0x90 || ((ACCA & 0xF0) > 0x80 && (ACCA & 0x0F) > 0x09)) {
         correction |= 0x60;
      }
      int tmp = ACCA + correction;
      // C is apparently only ever set by DAA, never cleared
      C |= (tmp >> 8) & 1;
      tmp &= 0xff;
      set_NZ(tmp);
      ACCA = tmp;
   } else {
      ACCA = -1;
      set_NZC_unknown();
   }
   // The datasheet says V is 0; this reference says V is undefined:
   // https://colorcomputerarchive.com/repo/Documents/Books/Motorola%206809%20and%20Hitachi%206309%20Programming%20Reference%20(Darren%20Atkinson).pdf

   // John Kent, in CPU09, says: DAA (Decimal Adjust Accumulator)
   // should set the Negative (N) and Zero Flags. It will also affect
   // the Overflow (V) flag although the operation is undefined in the
   // M6809 Programming Reference Manual. It's anyone's guess what DAA
   // does to V although I found Exclusive ORing Bit 7 of the original
   // ACCA value with B7 of the Decimal Adjusted value and Exclusive
   // ORing that with the pre Decimal Adjust Carry input resulting in
   // something approximating what you find on an EF68A09P.

   V = 0;
   return -1;
}

static int dec_helper(int val) {
   if (val >= 0) {
      val = (val - 1) & 0xff;
      set_NZ(val);
      // V indicates signed overflow, which onlt happens when going from 128->127
      V = (val == 0x7f);
   } else {
      val = -1;
      set_NZV_unknown();
   }
   return val;
}

static int op_fn_DEC(operand_t operand, ea_t ea, sample_t *sample_q) {
   return dec_helper(operand);
}

static int op_fn_DECA(operand_t operand, ea_t ea, sample_t *sample_q) {
   ACCA = dec_helper(ACCA);
   return -1;
}

static int op_fn_DECB(operand_t operand, ea_t ea, sample_t *sample_q) {
   ACCB = dec_helper(ACCB);
   return -1;
}

static int eor_helper(int val, operand_t operand) {
   if (val >= 0) {
      val ^= operand;
      set_NZ(val);
   } else {
      set_NZ_unknown();
   }
   V = 0;
   return val;
}

static int op_fn_EORA(operand_t operand, ea_t ea, sample_t *sample_q) {
   ACCA = eor_helper(ACCA, operand);
   return -1;
}

static int op_fn_EORB(operand_t operand, ea_t ea, sample_t *sample_q) {
   ACCB = eor_helper(ACCB, operand);
   return -1;
}

// Operand is the postbyte
static int op_fn_EXG(operand_t operand, ea_t ea, sample_t *sample_q) {
   int reg1 = (operand >> 4) & 15;
   int reg2 = operand  & 15;
   int tmp1 = get_regp(reg1);
   int tmp2 = get_regp(reg2);
   set_regp(reg1, tmp2);
   set_regp(reg2, tmp1);
   return -1;
}

static int inc_helper(int val) {
   if (val >= 0) {
      val = (val + 1) & 0xff;
      set_NZ(val);
      // V indicates signed overflow, which only happens when going from 127->128
      V = (val == 0x80);
   } else {
      val = -1;
      set_NZV_unknown();
   }
   return val;
}

static int op_fn_INC(operand_t operand, ea_t ea, sample_t *sample_q) {
   return inc_helper(operand);
}

static int op_fn_INCA(operand_t operand, ea_t ea, sample_t *sample_q) {
   ACCA = inc_helper(ACCA);
   return -1;
}

static int op_fn_INCB(operand_t operand, ea_t ea, sample_t *sample_q) {
   ACCB = inc_helper(ACCB);
   return -1;
}

static int op_fn_JMP(operand_t operand, ea_t ea, sample_t *sample_q) {
   PC = ea;
   return -1;
}

static int op_fn_JSR(operand_t operand, ea_t ea, sample_t *sample_q) {
   // operand is actually byte swapped at this point
   push8s(operand >> 8); // this pushes the low byte
   push8s(operand);      // this pushes he high byte
   PC = ea & 0xffff;
   return -1;
}

static int ld_helper8(int val) {
   val &= 0xff;
   set_NZ(val);
   V = 0;
   return val;
}

static int ld_helper16(int val) {
   val &= 0xffff;
   set_NZ16(val);
   V = 0;
   return val;
}

static int op_fn_LDA(operand_t operand, ea_t ea, sample_t *sample_q) {
   ACCA = ld_helper8(operand);
   return -1;
}

static int op_fn_LDB(operand_t operand, ea_t ea, sample_t *sample_q) {
   ACCB = ld_helper8(operand);
   return -1;
}

static int op_fn_LDD(operand_t operand, ea_t ea, sample_t *sample_q) {
   int tmp = ld_helper16(operand);
   ACCA = (tmp >> 8) & 0xff;
   ACCB = tmp & 0xff;
   return -1;
}

static int op_fn_LDS(operand_t operand, ea_t ea, sample_t *sample_q) {
   S = ld_helper16(operand);
   return -1;
}

static int op_fn_LDU(operand_t operand, ea_t ea, sample_t *sample_q) {
   U = ld_helper16(operand);
   return -1;
}

static int op_fn_LDX(operand_t operand, ea_t ea, sample_t *sample_q) {
   X = ld_helper16(operand);
   return -1;
}

static int op_fn_LDY(operand_t operand, ea_t ea, sample_t *sample_q) {
   Y = ld_helper16(operand);
   return -1;
}

static int op_fn_LEAS(operand_t operand, ea_t ea, sample_t *sample_q) {
   S = ea;
   return -1;
}

static int op_fn_LEAU(operand_t operand, ea_t ea, sample_t *sample_q) {
   U = ea;
   return -1;
}

static int op_fn_LEAX(operand_t operand, ea_t ea, sample_t *sample_q) {
   X = ea;
   Z = (X == 0);
   return -1;
}

static int op_fn_LEAY(operand_t operand, ea_t ea, sample_t *sample_q) {
   Y = ea;
   Z = (Y == 0);
   return -1;
}

static int lsr_helper(int val) {
   if (val >= 0) {
      C = val & 1;
      val >>= 1;
      Z = (val == 0);
   } else {
      C = -1;
      Z = -1;
   }
   N = 0;
   return val;
}

static int op_fn_LSR(operand_t operand, ea_t ea, sample_t *sample_q) {
   return lsr_helper(operand);
}

static int op_fn_LSRA(operand_t operand, ea_t ea, sample_t *sample_q) {
   ACCA = lsr_helper(ACCA);
   return -1;
}

static int op_fn_LSRB(operand_t operand, ea_t ea, sample_t *sample_q) {
   ACCB = lsr_helper(ACCB);
   return -1;
}

static int op_fn_MUL(operand_t operand, ea_t ea, sample_t *sample_q) {
   // D = A * B
   if (ACCA >= 0 && ACCB >= 0) {
      uint16_t tmp = ACCA * ACCB;
      ACCA = (tmp >> 8) & 0xff;
      ACCB = tmp & 0xff;
      Z = (tmp == 0);
      C = (ACCB >> 7) & 1;
   } else {
      ACCA = -1;
      ACCB = -1;
      Z = -1;
      C = -1;
   }
   return -1;
}

static int neg_helper(int val) {
   V = (val == 0x80);
   C = (val == 0x00);
   val = (-val) & 0xff;
   set_NZ(val);
   // The datasheet says the half-carry flag is undefined, but in practice
   // it seems to be unchanged (i.e. no errors). TODO: needs further testing
   // H = -1;
   return val;
}

static int op_fn_NEG(operand_t operand, ea_t ea, sample_t *sample_q) {
   return neg_helper(operand);
}

static int op_fn_NEGA(operand_t operand, ea_t ea, sample_t *sample_q) {
   ACCA = neg_helper(ACCA);
   return -1;
}

static int op_fn_NEGB(operand_t operand, ea_t ea, sample_t *sample_q) {
   ACCB = neg_helper(ACCB);
   return -1;
}

static int op_fn_NOP(operand_t operand, ea_t ea, sample_t *sample_q) {
   return -1;
}

static int or_helper(int val, operand_t operand) {
   if (val >= 0) {
      val |= operand;
      set_NZ(val);
   } else {
      set_NZ_unknown();
   }
   V = 0;
   return val;
}

static int op_fn_ORA(operand_t operand, ea_t ea, sample_t *sample_q) {
   ACCA = or_helper(ACCA, operand);
   return -1;
}

static int op_fn_ORB(operand_t operand, ea_t ea, sample_t *sample_q) {
   ACCB = or_helper(ACCB, operand);
   return -1;
}

static int op_fn_ORCC(operand_t operand, ea_t ea, sample_t *sample_q) {
   if (operand & 0x80) {
      E = 1;
   }
   if (operand & 0x40) {
      F = 1;
   }
   if (operand & 0x20) {
      H = 1;
   }
   if (operand & 0x10) {
      I = 1;
   }
   if (operand & 0x08) {
      N = 1;
   }
   if (operand & 0x04) {
      Z = 1;
   }
   if (operand & 0x02) {
      V = 1;
   }
   if (operand & 0x01) {
      C = 1;
   }
   return -1;
}

static void push_helper(sample_t *sample_q, int system) {
   //  0 opcode
   //  1 postbyte
   //  2 ---
   //  3 ---
   //  4 ---
   //  5 PCL    skipped if bit 7=0
   //  6 PCH    skipped if bit 7=0
   //  7 UL/SL  skipped if bit 6=0
   //  8 UH/SH  skipped if bit 6=0
   //  9 YL     skipped if bit 5=0
   // 10 YH     skipped if bit 5=0
   // 11 XL     skipped if bit 4=0
   // 12 XH     skipped if bit 4=0
   // 13 DP     skipped if bit 3=0
   // 14 B      skipped if bit 2=0
   // 15 A      skipped if bit 1=0
   // 16 Flags  skipped if bit 0=0
   int *us;
   void (*push8)(int);
   void (*push16)(int);
   int fail_us;
   if (system) {
      push8 = push8s;
      push16 = push16s;
      us = &U;
      fail_us = FAIL_U;
   } else {
      push8 = push8u;
      push16 = push16u;
      us = &S;
      fail_us = FAIL_S;
   }

   int pb = sample_q[1].data;
   int tmp;
   int i = 5;
   if (pb & 0x80) {
      tmp = (sample_q[i + 1].data << 8) + sample_q[i].data;
      i += 2;
      push16(tmp);
      if (PC >= 0 && PC != tmp) {
         failflag |= FAIL_PC;
      }
      PC = tmp;
   }
   if (pb & 0x40) {
      tmp = (sample_q[i + 1].data << 8) + sample_q[i].data;
      i += 2;
      push16(tmp);
      if (*us >= 0 && *us != tmp) {
         failflag |= fail_us;
      }
      *us = tmp;
   }
   if (pb & 0x20) {
      tmp = (sample_q[i + 1].data << 8) + sample_q[i].data;
      i += 2;
      push16(tmp);
      if (Y >= 0 && Y != tmp) {
         failflag |= FAIL_Y;
      }
      Y = tmp;
   }
   if (pb & 0x10) {
      tmp = (sample_q[i + 1].data << 8) + sample_q[i].data;
      i += 2;
      push16(tmp);
      if (X >= 0 && X != tmp) {
         failflag |= FAIL_X;
      }
      X = tmp;
   }
   if (pb & 0x08) {
      tmp = sample_q[i++].data;
      push8(tmp);
      if (DP >= 0 && DP != tmp) {
         failflag |= FAIL_DP;
      }
      DP = tmp;
   }
   if (pb & 0x04) {
      tmp = sample_q[i++].data;
      push8(tmp);
      if (ACCB >= 0 && ACCB != tmp) {
         failflag |= FAIL_B;
      }
      ACCB = tmp;
   }
   if (pb & 0x02) {
      tmp = sample_q[i++].data;
      push8(tmp);
      if (ACCA >= 0 && ACCA != tmp) {
         failflag |= FAIL_A;
      }
      ACCA = tmp;
   }
   if (pb & 0x01) {
      tmp = sample_q[i++].data;
      push8(tmp);
      check_FLAGS(tmp);
      set_FLAGS(tmp);
   }
}

static int op_fn_PSHS(operand_t operand, ea_t ea, sample_t *sample_q) {
   push_helper(sample_q, 1); // 1 = PSHS
   return -1;
}

static int op_fn_PSHU(operand_t operand, ea_t ea, sample_t *sample_q) {
   push_helper(sample_q, 0); // 0 = PSHU
   return -1;
}


static void pull_helper(sample_t *sample_q, int system) {
   //  0 opcode
   //  1 postbyte
   //  2 ---
   //  3 ---
   //  4 Flags  skipped if bit 0=0
   //  5 A      skipped if bit 1=0
   //  6 B      skipped if bit 2=0
   //  7 DP     skipped if bit 3=0
   //  8 XH     skipped if bit 4=0
   //  9 XL     skipped if bit 4=0
   // 10 YH     skipped if bit 5=0
   // 11 YL     skipped if bit 5=0
   // 12 UH/SH  skipped if bit 6=0
   // 13 UL/SL  skipped if bit 6=0
   // 14 PCH    skipped if bit 7=0
   // 15 PCL    skipped if bit 7=0
   // 16 --

   int *us;
   void (*pop8)(int);
   void (*pop16)(int);
   if (system) {
      pop8 = pop8s;
      pop16 = pop16s;
      us = &U;
   } else {
      pop8 = pop8u;
      pop16 = pop16u;
      us = &S;
   }

   int pb = sample_q[1].data;
   int tmp;
   int i = 4;
   if (pb & 0x01) {
      tmp = sample_q[i++].data;
      pop8(tmp);
      set_FLAGS(tmp);
   }
   if (pb & 0x02) {
      tmp = sample_q[i++].data;
      pop8(tmp);
      ACCA = tmp;
   }
   if (pb & 0x04) {
      tmp = sample_q[i++].data;
      pop8(tmp);
      ACCB = tmp;
   }
   if (pb & 0x08) {
      tmp = sample_q[i++].data;
      pop8(tmp);
      DP = tmp;
   }
   if (pb & 0x10) {
      tmp = (sample_q[i].data << 8) + sample_q[i + 1].data;
      i += 2;
      pop16(tmp);
      X = tmp;
   }
   if (pb & 0x20) {
      tmp = (sample_q[i].data << 8) + sample_q[i + 1].data;
      i += 2;
      pop16(tmp);
      Y = tmp;
   }
   if (pb & 0x40) {
      tmp = (sample_q[i].data << 8) + sample_q[i + 1].data;
      i += 2;
      pop16(tmp);
      *us = tmp;
   }
   if (pb & 0x80) {
      tmp = (sample_q[i].data << 8) + sample_q[i + 1].data;
      i += 2;
      pop16(tmp);
      PC = tmp;
   }
}

static int op_fn_PULS(operand_t operand, ea_t ea, sample_t *sample_q) {
   pull_helper(sample_q, 1); // 1 = PULS
   return -1;
}

static int op_fn_PULU(operand_t operand, ea_t ea, sample_t *sample_q) {
   pull_helper(sample_q, 0); // 0 = PULU
   return -1;
}

static int rol_helper(int val) {
   if (val >= 0 && C >= 0) {
      int tmp = (val << 1) + C;
      // C is bit 7 of val
      C = (val >> 7) & 1;
      // V is the xor of bits 7,6 of val
      V = ((tmp ^ val) >> 7) & 1;
      // truncate to 8 bits
      val = tmp & 255;
      set_NZ(val);
   } else {
      val = -1;
      set_NZVC_unknown();
   }
   return val;
}

static int op_fn_ROL(operand_t operand, ea_t ea, sample_t *sample_q) {
   return rol_helper(operand);
}

static int op_fn_ROLA(operand_t operand, ea_t ea, sample_t *sample_q) {
   ACCA = rol_helper(ACCA);
   return -1;
}

static int op_fn_ROLB(operand_t operand, ea_t ea, sample_t *sample_q) {
   ACCB = rol_helper(ACCB);
   return -1;
}

static int ror_helper(int val) {
   if (val >= 0 && C >= 0) {
      int tmp = (val >> 1) + (C << 7);
      // C is bit 0 of val (V is unaffected)
      C = val & 1;
      // truncate to 8 bits
      val = tmp & 255;
      set_NZ(val);
   } else {
      val = -1;
      set_NZC_unknown();
   }
   return val;
}

static int op_fn_ROR(operand_t operand, ea_t ea, sample_t *sample_q) {
   return ror_helper(operand);
}

static int op_fn_RORA(operand_t operand, ea_t ea, sample_t *sample_q) {
   ACCA = ror_helper(ACCA);
   return -1;
}

static int op_fn_RORB(operand_t operand, ea_t ea, sample_t *sample_q) {
   ACCB = ror_helper(ACCB);
   return -1;
}

static int op_fn_RTI(operand_t operand, ea_t ea, sample_t *sample_q) {
   // E = 0
   //   0 Opcode
   //   1 ---
   //   2 flags
   //   3 PCH
   //   4 PCL
   //   5 ---
   //
   // E = 1
   //   0 Opcode
   //   1 ---
   //   2 flags
   //   3 A
   //   4 B
   //   5 DP
   //   6 XHI
   //   7 XLO
   //   8 YHI
   //   9 YLO
   //  10 UHI
   //  11 HLO
   //  12 PCH
   //  13 PCL
   //  14 ---

   // Do the flags first, as the stacked E indicates how much to restore
   set_FLAGS(sample_q[2].data);

   // Update the register state
   if (E == 1) {
      ACCA  = sample_q[3].data;
      ACCB  = sample_q[4].data;
      DP = sample_q[5].data;
      X  = (sample_q[6].data << 8) + sample_q[7].data;
      Y  = (sample_q[8].data << 8) + sample_q[9].data;
      U  = (sample_q[10].data << 8) + sample_q[11].data;
      PC = (sample_q[12].data << 8) + sample_q[13].data;
   } else {
      PC = (sample_q[3].data << 8) + sample_q[4].data;
   }

   // Memory modelling
   int n = (E == 1) ? 12 : 3;
   for (int i = 2; i < 2 + n; i++) {
      pop8s(sample_q[i].data);
   }

   return -1;
}

static int op_fn_RTS(operand_t operand, ea_t ea, sample_t *sample_q) {
   pop8s(sample_q[2].data);
   pop8s(sample_q[3].data);
   PC = (sample_q[2].data << 8) + sample_q[3].data;
   return -1;
}

static int sub_helper(int val, int cin, operand_t operand) {
   if (val >= 0 && cin >= 0) {
      int tmp = val - operand - cin;
      // The carry flag is bit 8 of the result
      C = (tmp >> 8) & 1;
      // The overflow flag is: IF ((a^b^res^(res>>1))&0x80) SEV else CLV
      V = (((val ^ operand ^ tmp) >> 7) & 1) ^ C;
      // Truncate the result to 8 bits
      tmp &= 0xff;
      // Set the flags
      set_NZ(tmp);
      // Save the result back to the register
      return tmp;
   } else {
      set_NZVC_unknown();
      return -1;
   }
   // The datasheet says the half-carry flag is undefined, but in practice
   // it seems to be unchanged (i.e. no errors). TODO: needs further testing
   // H = -1;
}

static int op_fn_SBCA(operand_t operand, ea_t ea, sample_t *sample_q) {
   ACCA = sub_helper(ACCA, C, operand);
   return -1;
}

static int op_fn_SBCB(operand_t operand, ea_t ea, sample_t *sample_q) {
   ACCB = sub_helper(ACCB, C, operand);
   return -1;
}

static int op_fn_SEX(operand_t operand, ea_t ea, sample_t *sample_q) {
   if (ACCB >= 0) {
      if (ACCB & 0x80) {
         ACCA = 0xFF;
      } else {
         ACCA = 0x00;
      }
      set_NZ(ACCB);
   } else {
      ACCA = -1;
      set_NZ_unknown();
   }
   V = 0;
   return -1;
}

static int st_helper8(int val, operand_t operand, int fail) {
   if (val >= 0) {
      if (operand != val) {
         failflag |= fail;
      }
   }
   V = 0;
   set_NZ(operand);
   return operand;
}

static int st_helper16(int val, operand_t operand, int fail) {
   if (val >= 0) {
      if (operand != val) {
         failflag |= fail;
      }
   }
   V = 0;
   set_NZ16(operand);
   return operand;
}

static int op_fn_STA(operand_t operand, ea_t ea, sample_t *sample_q) {
   ACCA = st_helper8(ACCA, operand, FAIL_A);
   return operand;
}

static int op_fn_STB(operand_t operand, ea_t ea, sample_t *sample_q) {
   ACCB = st_helper8(ACCB, operand, FAIL_B);
   return operand;
}

static int op_fn_STD(operand_t operand, ea_t ea, sample_t *sample_q) {
   int D = (ACCA >= 0 && ACCB >= 0) ? (ACCA << 8) + ACCB : -1;
   D = st_helper16(D, operand, FAIL_A | FAIL_B);
   ACCA = (D >> 8) & 0xff;
   ACCB = D & 0xff;
   return operand;
}

static int op_fn_STS(operand_t operand, ea_t ea, sample_t *sample_q) {
   S = st_helper16(S, operand, FAIL_S);
   return operand;
}

static int op_fn_STU(operand_t operand, ea_t ea, sample_t *sample_q) {
   U = st_helper16(U, operand, FAIL_U);
   return operand;
}

static int op_fn_STX(operand_t operand, ea_t ea, sample_t *sample_q) {
   X = st_helper16(X, operand, FAIL_X);
   return operand;
}

static int op_fn_STY(operand_t operand, ea_t ea, sample_t *sample_q) {
   Y = st_helper16(Y, operand, FAIL_Y);
   return operand;
}

static int op_fn_SUBA(operand_t operand, ea_t ea, sample_t *sample_q) {
   ACCA = sub_helper(ACCA, 0, operand);
   return -1;
}

static int op_fn_SUBB(operand_t operand, ea_t ea, sample_t *sample_q) {
   ACCB = sub_helper(ACCB, 0, operand);
   return -1;
}

static int op_fn_SUBD(operand_t operand, ea_t ea, sample_t *sample_q) {
   if (ACCA >= 0 && ACCB >= 0) {
      int d = (ACCA << 8) + ACCB;
      // Perform the addition (there is no carry in)
      int tmp = d - operand;
      // The carry flag is bit 16 of the result
      C = (tmp >> 16) & 1;
      // The overflow flag is: IF ((a^b^res^(res>>1))&0x8000) SEV else CLV
      V = (((d ^ operand ^ tmp) >> 15) & 1) ^ C;
      // Truncate the result to 16 bits
      tmp &= 0xFFFF;
      // Set the flags
      set_NZ16(tmp);
      // Unpack back into A and B
      ACCA = (tmp >> 8) & 0xff;
      ACCB = tmp & 0xff;
   } else {
      ACCA = -1;
      ACCB = -1;
      set_NZVC_unknown();
   }
   return -1;
}


static int op_fn_SWI(operand_t operand, ea_t ea, sample_t *sample_q) {
   interrupt_helper(sample_q, 3, 1, 0xfffa);
   return -1;
}

static int op_fn_SWI2(operand_t operand, ea_t ea, sample_t *sample_q) {
   interrupt_helper(sample_q, 4, 1, 0xfff4);
   return -1;
}

static int op_fn_SWI3(operand_t operand, ea_t ea, sample_t *sample_q) {
   interrupt_helper(sample_q, 4, 1, 0xfff2);
   return -1;
}

static int op_fn_SYNC(operand_t operand, ea_t ea, sample_t *sample_q) {
   return -1;
}

// Operand is the postbyte
static int op_fn_TFR(operand_t operand, ea_t ea, sample_t *sample_q) {
   int reg1 = (operand >> 4) & 15;
   int reg2 = operand  & 15;
   set_regp(reg2, get_regp(reg1));
   return -1;
}

static int op_fn_TST(operand_t operand, ea_t ea, sample_t *sample_q) {
   set_NZ(operand);
   V = 0;
   return -1;
}

static int op_fn_TSTA(operand_t operand, ea_t ea, sample_t *sample_q) {
   set_NZ(ACCA);
   V = 0;
   return -1;
}

static int op_fn_TSTB(operand_t operand, ea_t ea, sample_t *sample_q) {
   set_NZ(ACCB);
   V = 0;
   return -1;
}

static int op_fn_UU(operand_t operand, ea_t ea, sample_t *sample_q) {
   return -1;
}

// ====================================================================
// Opcode Tables
// ====================================================================

static operation_t op_ABX  = { "ABX ", op_fn_ABX ,     REGOP , 0 };
static operation_t op_ADCA = { "ADCA", op_fn_ADCA,    READOP , 0 };
static operation_t op_ADCB = { "ADCB", op_fn_ADCB,    READOP , 0 };
static operation_t op_ADDA = { "ADDA", op_fn_ADDA,    READOP , 0 };
static operation_t op_ADDB = { "ADDB", op_fn_ADDB,    READOP , 0 };
static operation_t op_ADDD = { "ADDD", op_fn_ADDD,    READOP , 1 };
static operation_t op_ANDA = { "ANDA", op_fn_ANDA,    READOP , 0 };
static operation_t op_ANDB = { "ANDB", op_fn_ANDB,    READOP , 0 };
static operation_t op_ANDC = { "ANDC", op_fn_ANDC,     REGOP , 0 };
static operation_t op_ASL  = { "ASL ", op_fn_ASL ,     RMWOP , 0 };
static operation_t op_ASLA = { "ASLA", op_fn_ASLA,     REGOP , 0 };
static operation_t op_ASLB = { "ASLB", op_fn_ASLB,     REGOP , 0 };
static operation_t op_ASR  = { "ASR ", op_fn_ASR ,     RMWOP , 0 };
static operation_t op_ASRA = { "ASRA", op_fn_ASRA,     REGOP , 0 };
static operation_t op_ASRB = { "ASRB", op_fn_ASRB,     REGOP , 0 };
static operation_t op_BCC  = { "BCC ", op_fn_BCC ,  BRANCHOP , 0 };
static operation_t op_BEQ  = { "BEQ ", op_fn_BEQ ,  BRANCHOP , 0 };
static operation_t op_BGE  = { "BGE ", op_fn_BGE ,  BRANCHOP , 0 };
static operation_t op_BGT  = { "BGT ", op_fn_BGT ,  BRANCHOP , 0 };
static operation_t op_BHI  = { "BHI ", op_fn_BHI ,  BRANCHOP , 0 };
static operation_t op_BITA = { "BITA", op_fn_BITA,    READOP , 0 };
static operation_t op_BITB = { "BITB", op_fn_BITB,    READOP , 0 };
static operation_t op_BLE  = { "BLE ", op_fn_BLE ,  BRANCHOP , 0 };
static operation_t op_BLO  = { "BLO ", op_fn_BLO ,  BRANCHOP , 0 };
static operation_t op_BLS  = { "BLS ", op_fn_BLS ,  BRANCHOP , 0 };
static operation_t op_BLT  = { "BLT ", op_fn_BLT ,  BRANCHOP , 0 };
static operation_t op_BMI  = { "BMI ", op_fn_BMI ,  BRANCHOP , 0 };
static operation_t op_BNE  = { "BNE ", op_fn_BNE ,  BRANCHOP , 0 };
static operation_t op_BPL  = { "BPL ", op_fn_BPL ,  BRANCHOP , 0 };
static operation_t op_BRA  = { "BRA ", op_fn_BRA ,  BRANCHOP , 0 };
static operation_t op_BRN  = { "BRN ", op_fn_BRN ,  BRANCHOP , 0 };
static operation_t op_BSR  = { "BSR ", op_fn_BSR ,     JSROP , 1 };
static operation_t op_BVC  = { "BVC ", op_fn_BVC ,  BRANCHOP , 0 };
static operation_t op_BVS  = { "BVS ", op_fn_BVS ,  BRANCHOP , 0 };
static operation_t op_CLR  = { "CLR ", op_fn_CLR ,     RMWOP , 0 };
static operation_t op_CLRA = { "CLRA", op_fn_CLRA,     REGOP , 0 };
static operation_t op_CLRB = { "CLRB", op_fn_CLRB,     REGOP , 0 };
static operation_t op_CMPA = { "CMPA", op_fn_CMPA,    READOP , 0 };
static operation_t op_CMPB = { "CMPB", op_fn_CMPB,    READOP , 0 };
static operation_t op_CMPD = { "CMPD", op_fn_CMPD,    READOP , 1 };
static operation_t op_CMPS = { "CMPS", op_fn_CMPS,    READOP , 1 };
static operation_t op_CMPU = { "CMPU", op_fn_CMPU,    READOP , 1 };
static operation_t op_CMPX = { "CMPX", op_fn_CMPX,    READOP , 1 };
static operation_t op_CMPY = { "CMPY", op_fn_CMPY,    READOP , 1 };
static operation_t op_COM  = { "COM ", op_fn_COM ,     RMWOP , 0 };
static operation_t op_COMA = { "COMA", op_fn_COMA,     REGOP , 0 };
static operation_t op_COMB = { "COMB", op_fn_COMB,     REGOP , 0 };
static operation_t op_CWAI = { "CWAI", op_fn_CWAI,     OTHER , 0 };
static operation_t op_DAA  = { "DAA ", op_fn_DAA ,     REGOP , 0 };
static operation_t op_DEC  = { "DEC ", op_fn_DEC ,     RMWOP , 0 };
static operation_t op_DECA = { "DECA", op_fn_DECA,     REGOP , 0 };
static operation_t op_DECB = { "DECB", op_fn_DECB,     REGOP , 0 };
static operation_t op_EORA = { "EORA", op_fn_EORA,    READOP , 0 };
static operation_t op_EORB = { "EORB", op_fn_EORB,    READOP , 0 };
static operation_t op_EXG  = { "EXG ", op_fn_EXG ,     REGOP , 0 };
static operation_t op_INC  = { "INC ", op_fn_INC ,     RMWOP , 0 };
static operation_t op_INCA = { "INCA", op_fn_INCA,     REGOP , 0 };
static operation_t op_INCB = { "INCB", op_fn_INCB,     REGOP , 0 };
static operation_t op_JMP  = { "JMP ", op_fn_JMP ,     LEAOP , 0 };
static operation_t op_JSR  = { "JSR ", op_fn_JSR ,     JSROP , 1 };
static operation_t op_LBCC = { "LBCC", op_fn_BCC ,  BRANCHOP , 0 };
static operation_t op_LBEQ = { "LBEQ", op_fn_BEQ ,  BRANCHOP , 0 };
static operation_t op_LBGE = { "LBGE", op_fn_BGE ,  BRANCHOP , 0 };
static operation_t op_LBGT = { "LBGT", op_fn_BGT ,  BRANCHOP , 0 };
static operation_t op_LBHI = { "LBHI", op_fn_BHI ,  BRANCHOP , 0 };
static operation_t op_LBLE = { "LBLE", op_fn_BLE ,  BRANCHOP , 0 };
static operation_t op_LBLO = { "LBLO", op_fn_BLO ,  BRANCHOP , 0 };
static operation_t op_LBLS = { "LBLS", op_fn_BLS ,  BRANCHOP , 0 };
static operation_t op_LBLT = { "LBLT", op_fn_BLT ,  BRANCHOP , 0 };
static operation_t op_LBMI = { "LBMI", op_fn_BMI ,  BRANCHOP , 0 };
static operation_t op_LBNE = { "LBNE", op_fn_BNE ,  BRANCHOP , 0 };
static operation_t op_LBPL = { "LBPL", op_fn_BPL ,  BRANCHOP , 0 };
static operation_t op_LBRA = { "LBRA", op_fn_BRA ,  BRANCHOP , 0 };
static operation_t op_LBRN = { "LBRN", op_fn_BRN ,  BRANCHOP , 0 };
static operation_t op_LBSR = { "LBSR", op_fn_BSR ,     JSROP , 1 };
static operation_t op_LBVC = { "LBVC", op_fn_BVC ,  BRANCHOP , 0 };
static operation_t op_LBVS = { "LBVS", op_fn_BVS ,  BRANCHOP , 0 };
static operation_t op_LDA  = { "LDA ", op_fn_LDA ,    LOADOP , 0 };
static operation_t op_LDB  = { "LDB ", op_fn_LDB ,    LOADOP , 0 };
static operation_t op_LDD  = { "LDD ", op_fn_LDD ,    LOADOP , 1 };
static operation_t op_LDS  = { "LDS ", op_fn_LDS ,    LOADOP , 1 };
static operation_t op_LDU  = { "LDU ", op_fn_LDU ,    LOADOP , 1 };
static operation_t op_LDX  = { "LDX ", op_fn_LDX ,    LOADOP , 1 };
static operation_t op_LDY  = { "LDY ", op_fn_LDY ,    LOADOP , 1 };
static operation_t op_LEAS = { "LEAS", op_fn_LEAS,     LEAOP , 0 };
static operation_t op_LEAU = { "LEAU", op_fn_LEAU,     LEAOP , 0 };
static operation_t op_LEAX = { "LEAX", op_fn_LEAX,     LEAOP , 0 };
static operation_t op_LEAY = { "LEAY", op_fn_LEAY,     LEAOP , 0 };
static operation_t op_LSR  = { "LSR ", op_fn_LSR ,     RMWOP , 0 };
static operation_t op_LSRA = { "LSRA", op_fn_LSRA,     REGOP , 0 };
static operation_t op_LSRB = { "LSRB", op_fn_LSRB,     REGOP , 0 };
static operation_t op_MUL  = { "MUL ", op_fn_MUL ,     REGOP , 0 };
static operation_t op_NEG  = { "NEG ", op_fn_NEG ,     RMWOP , 0 };
static operation_t op_NEGA = { "NEGA", op_fn_NEGA,     REGOP , 0 };
static operation_t op_NEGB = { "NEGB", op_fn_NEGB,     REGOP , 0 };
static operation_t op_NOP  = { "NOP ", op_fn_NOP ,     REGOP , 0 };
static operation_t op_ORA  = { "ORA ", op_fn_ORA ,    READOP , 0 };
static operation_t op_ORB  = { "ORB ", op_fn_ORB ,    READOP , 0 };
static operation_t op_ORCC = { "ORCC", op_fn_ORCC,     REGOP , 0 };
static operation_t op_PSHS = { "PSHS", op_fn_PSHS,     OTHER , 0 };
static operation_t op_PSHU = { "PSHU", op_fn_PSHU,     OTHER , 0 };
static operation_t op_PULS = { "PULS", op_fn_PULS,     OTHER , 0 };
static operation_t op_PULU = { "PULU", op_fn_PULU,     OTHER , 0 };
static operation_t op_ROL  = { "ROL ", op_fn_ROL ,     RMWOP , 0 };
static operation_t op_ROLA = { "ROLA", op_fn_ROLA,     REGOP , 0 };
static operation_t op_ROLB = { "ROLB", op_fn_ROLB,     REGOP , 0 };
static operation_t op_ROR  = { "ROR ", op_fn_ROR ,     RMWOP , 0 };
static operation_t op_RORA = { "RORA", op_fn_RORA,     REGOP , 0 };
static operation_t op_RORB = { "RORB", op_fn_RORB,     REGOP , 0 };
static operation_t op_RTI  = { "RTI ", op_fn_RTI ,     OTHER , 0 };
static operation_t op_RTS  = { "RTS ", op_fn_RTS ,     OTHER , 0 };
static operation_t op_SBCA = { "SBCA", op_fn_SBCA,    READOP , 0 };
static operation_t op_SBCB = { "SBCB", op_fn_SBCB,    READOP , 0 };
static operation_t op_SEX  = { "SEX ", op_fn_SEX ,     REGOP , 0 };
static operation_t op_STA  = { "STA ", op_fn_STA ,   STOREOP , 0 };
static operation_t op_STB  = { "STB ", op_fn_STB ,   STOREOP , 0 };
static operation_t op_STD  = { "STD ", op_fn_STD ,   STOREOP , 1 };
static operation_t op_STS  = { "STS ", op_fn_STS ,   STOREOP , 1 };
static operation_t op_STU  = { "STU ", op_fn_STU ,   STOREOP , 1 };
static operation_t op_STX  = { "STX ", op_fn_STX ,   STOREOP , 1 };
static operation_t op_STY  = { "STY ", op_fn_STY ,   STOREOP , 1 };
static operation_t op_SUBA = { "SUBA", op_fn_SUBA,    READOP , 0 };
static operation_t op_SUBB = { "SUBB", op_fn_SUBB,    READOP , 0 };
static operation_t op_SUBD = { "SUBD", op_fn_SUBD,    READOP , 1 };
static operation_t op_SWI  = { "SWI ", op_fn_SWI ,     OTHER , 0 };
static operation_t op_SWI2 = { "SWI2", op_fn_SWI2,     OTHER , 0 };
static operation_t op_SWI3 = { "SWI3", op_fn_SWI3,     OTHER , 0 };
static operation_t op_SYNC = { "SYNC", op_fn_SYNC,     OTHER , 0 };
static operation_t op_TFR  = { "TFR ", op_fn_TFR ,     REGOP , 0 };
static operation_t op_TST  = { "TST ", op_fn_TST ,    READOP , 0 };
static operation_t op_TSTA = { "TSTA", op_fn_TSTA,     REGOP , 0 };
static operation_t op_TSTB = { "TSTB", op_fn_TSTB,     REGOP , 0 };
static operation_t op_UU   = { "??? ", op_fn_UU,       OTHER , 0 };
