// ====================================================================
// Instruction Helper Methods
// ====================================================================

static instr_mode_t *get_instruction(uint8_t b0, uint8_t b1) {
   if (b0 == 0x11) {
      return instr_table_6809_map2 + b1;
   } else if (b0 == 0x10) {
      return instr_table_6809_map1 + b1;
   } else {
      return instr_table_6809_map0 + b0;
   }
}

// ====================================================================
// Flag Helper Methods
// ====================================================================

static void check_FLAGS(int operand) {
   if (E >= 0) {
      if (E != ((operand >> 7) & 1)) {
         failflag |= FAIL_E;
      }
   }
   if (F >= 0) {
      if (F != ((operand >> 6) & 1)) {
         failflag |= FAIL_F;
      }
   }
   if (H >= 0) {
      if (H != ((operand >> 5) & 1)) {
         failflag |= FAIL_H;
      }
   }
   if (I >= 0) {
      if (I != ((operand >> 4) & 1)) {
         failflag |= FAIL_I;
      }
   }
   if (N >= 0) {
      if (N != ((operand >> 3) & 1)) {
         failflag |= FAIL_N;
      }
   }
   if (Z >= 0) {
      if (Z != ((operand >> 2) & 1)) {
         failflag |= FAIL_Z;
      }
   }
   if (V >= 0) {
      if (V != ((operand >> 1) & 1)) {
         failflag |= FAIL_V;
      }
   }
   if (C >= 0) {
      if (C != ((operand >> 0) & 1)) {
         failflag |= FAIL_C;
      }
   }
}

static int get_FLAGS() {
   if (E < 0 || F < 0 || H < 0 || I < 0 || N < 0 || Z < 0 || V < 0 || C < 0) {
      return -1;
   } else {
      return (E << 7) | (F << 6) | (H << 5) | (I << 4) | (N << 3) | (Z << 2) | (V << 1) | C;
   }
}

static void set_FLAGS(int operand) {
   E = (operand >> 7) & 1;
   F = (operand >> 6) & 1;
   H = (operand >> 5) & 1;
   I = (operand >> 4) & 1;
   N = (operand >> 3) & 1;
   Z = (operand >> 2) & 1;
   V = (operand >> 1) & 1;
   C = (operand >> 0) & 1;
}

static void set_NZ_unknown() {
   N = -1;
   Z = -1;
}

static void set_NZC_unknown() {
   N = -1;
   Z = -1;
   C = -1;
}

static void set_NZV_unknown() {
   N = -1;
   Z = -1;
   V = -1;
}

static void set_HNZVC_unknown() {
   H = -1;
   N = -1;
   Z = -1;
   V = -1;
   C = -1;
}

static void set_NZVC_unknown() {
   N = -1;
   Z = -1;
   V = -1;
   C = -1;
}

static void set_EFHINZVC_unknown() {
   E = -1;
   F = -1;
   H = -1;
   I = -1;
   N = -1;
   Z = -1;
   V = -1;
   C = -1;
}

static void set_NZ(int value) {
   N = (value >> 7) & 1;
   Z = value == 0;
}

static void set_NZ16(int value) {
   N = (value >> 15) & 1;
   Z = value == 0;
}


static void pop8s(int value) {
   if (S >= 0) {
      memory_read(value & 0xff, S, MEM_STACK);
      S = (S + 1) & 0xffff;
   }
}

static void push8s(int value) {
   if (S >= 0) {
      S = (S - 1) & 0xffff;
      memory_write(value & 0xff, S, MEM_STACK);
   }
}

static void pop16s(int value) {
   pop8s(value >> 8);
   pop8s(value);
}

static void push16s(int value) {
   push8s(value);
   push8s(value >> 8);
}


static void pop8u(int value) {
   if (U >= 0) {
      memory_read(value & 0xff, U, MEM_STACK);
      U = (U + 1) & 0xffff;
   }
}

static void push8u(int value) {
   if (U >= 0) {
      U = (U - 1) & 0xffff;
      memory_write(value & 0xff, U, MEM_STACK);
   }
}

static void pop16u(int value) {
   pop8u(value >> 8);
   pop8u(value);
}

static void push16u(int value) {
   push8u(value);
   push8u(value >> 8);
}

// ====================================================================
// Interrupt Helper Method
// ====================================================================

// Returns the old PC value
static int interrupt_helper(sample_t *sample_q, int offset, int full, int vector) {

   // FIQ
   //  0
   //  1
   //  2
   //  3 PCL
   //  4 PCH
   //  5 Flags
   //  6
   //  7 New PCH
   //  8 New PCL
   //  9

   // IRQ / NMI / SWI / SWI2 / SWI3
   //  0
   //  1
   //  2
   //  3 PCL
   //  4 PCH
   //  5 USL
   //  6 USH
   //  7 IYL
   //  8 IYH
   //  9 IXL
   // 10 IXH
   // 11 DP
   // 12 ACCB
   // 13 ACCA
   // 14 Flags
   // 15
   // 16 New PCH
   // 17 New PCL
   // 18
   //
   // Note there is a one cycle offset for SWI2/3 due to the additiona prefix byte

   int i = offset;

   // The PC is pushed in all cases
   int pc = (sample_q[i + 1].data << 8) + sample_q[i].data;
   i += 2;
   push16s(pc);

   // The full state is pushed in IRQ/NMI/SWI/SWI2/SWI3
   if (full) {

      int u  = (sample_q[i + 1].data << 8) + sample_q[i].data;
      i += 2;
      push16s(u);
      if (U >= 0 && u != U) {
         failflag |= FAIL_U;
      }
      U = u;

      int y  = (sample_q[i + 1].data << 8) + sample_q[i].data;
      i += 2;
      push16s(y);
      if (Y >= 0 && y != Y) {
         failflag |= FAIL_Y;
      }
      Y = y;

      int x  = (sample_q[i + 1].data << 8) + sample_q[i].data;
      i += 2;
      push16s(x);
      if (X >= 0 && x != X) {
         failflag |= FAIL_X;
      }
      X = x;

      int dp = sample_q[i++].data;
      push8s(dp);
      if (DP >= 0 && dp != DP) {
         failflag |= FAIL_DP;
      }
      DP = dp;

      int b  = sample_q[i++].data;
      push8s(b);
      if (ACCB >= 0 && b != ACCB) {
         failflag |= FAIL_B;
      }
      ACCB = b;

      int a  = sample_q[i++].data;
      push8s(a);
      if (ACCA >= 0 && a != ACCA) {
         failflag |= FAIL_A;
      }
      ACCA = a;
      // Set E to indicate the full state was saved
      E = 1;
   } else {
      // Clear E to indicate just PC/flags were saved
      E = 0;
   }

   // The flags are pushed in all cases
   int flags = sample_q[i].data;
   push8s(flags);
   check_FLAGS(flags);
   set_FLAGS(flags);
   i++;

   // Skip a dead cycle becfore the vector
   i++;

   // Read the vector and compare against what's expected
   int vechi = sample_q[i].data;
   memory_read(vechi, vector, MEM_POINTER);
   if (sample_q[i].addr >= 0 && (sample_q[i].addr != (vector & 0x000F))) {
      failflag |= FAIL_VECTOR;
   }
   i++;
   int veclo = sample_q[i].data;
   memory_read(veclo, vector + 1, MEM_POINTER);
   if (sample_q[i].addr >= 0 && (sample_q[i].addr != ((vector + 1) & 0x000F))) {
      failflag  |= FAIL_VECTOR;
   }
   PC = (vechi << 8) + veclo;

   // FFF0 : reserved :
   // FFF2 : SWI3     : flags unchanged
   // FFF4 : SWI2     : flags unchanged
   // FFF6 : FIQ      : I = 1; F = 1
   // FFF8 : IRQ      : I = 1
   // FFFA : SWI      : I = 1; F = 1
   // FFFC : NMI      : I = 1; F = 1
   // FFFE : Reset    : I = 1; F = 1

   switch (vector) {
   case 0xfff8: // IRQ
      I = 1;
      break;
   case 0xfff6: // FIQ
   case 0xfffa: // SWI
   case 0xfffc: // NMI
   case 0xfffe: // Reset
      I = 1;
      F = 1;
      break;
   }

   // Return the old PC value that was pushed to the stack
   return pc;
}
