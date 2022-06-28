#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "memory.h"
#include "types_6809.h"
#include "em_6809.h"
#include "dis_6809.h"

// ====================================================================
// CPU state display
// ====================================================================

static const char cpu_6809_state[] = "A=?? B=?? X=???? Y=???? U=???? S=???? DP=?? E=? F=? H=? I=? N=? Z=? V=? C=?";

static const char cpu_6309_state[] = "A=?? B=?? E=?? F=?? X=???? Y=???? U=???? S=???? DP=?? T=???? E=? F=? H=? I=? N=? Z=? V=? C=? DZ=? IL=? FM=? NM=?";

// ====================================================================
// Instruction cycle extras
// ====================================================================

// In indexed indirect modes, there are extra cycles computing the effective
// address that cannot be included in the opcode table, as they depend on the
// indexing mode in the post byte.

// On the 6809 there are 15 undefined postbytes, marked as XX below
// On the 6309 8 of these are used for the W based modes, leaving 7 as traps

// TODO: What actually happens on the 6809 with these?

#define XX -1

#define MAX_VALID_POSTBYTE_CYCLES 8

static int postbyte_cycles_6809[0x100] = {
// x0  x1  x2  x3  x4  x5  x6  x7  x8  x9  xA  xB  xC  xD  xE  xF
   1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 0x
   1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 1x
   1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 2x
   1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 3x
   1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 4x
   1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 5x
   1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 6x
   1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 7x
   2,  3,  2,  3,  0,  1,  1, XX,  1,  4, XX,  4,  1,  5, XX, XX,  // 8x
  XX,  6, XX,  6,  3,  4,  4, XX,  4,  7, XX,  8,  4,  8, XX,  5,  // 9x
   2,  3,  2,  3,  0,  1,  1, XX,  1,  4, XX,  4,  1,  5, XX, XX,  // Ax
  XX,  6, XX,  6,  3,  4,  4, XX,  4,  7, XX,  8,  4,  8, XX, XX,  // Bx
   2,  3,  2,  3,  0,  1,  1, XX,  1,  4, XX,  4,  1,  5, XX, XX,  // Cx
  XX,  6, XX,  6,  3,  4,  4, XX,  4,  7, XX,  8,  4,  8, XX, XX,  // Dx
   2,  3,  2,  3,  0,  1,  1, XX,  1,  4, XX,  4,  1,  5, XX, XX,  // Ex
  XX,  6, XX,  6,  3,  4,  4, XX,  4,  7, XX,  8,  4,  8, XX, XX   // Fx
};

// From Addendum to The 6309 Book
// Indexed Addressing Mode Post Bytes
// https://colorcomputerarchive.com/repo/Documents/Microprocessors/HD6309/6309%20Indexed%20Cycle%20Counts.pdf

static int postbyte_cycles_6309_emu[0x100] = {
// x0  x1  x2  x3  x4  x5  x6  x7  x8  x9  xA  xB  xC  xD  xE  xF
   1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 0x
   1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 1x
   1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 2x
   1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 3x
   1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 4x
   1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 5x
   1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 6x
   1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 7x
   2,  3,  2,  3,  0,  1,  1,  1,  1,  4,  1,  4,  1,  5,  4,  0,  // 8x
   3,  6, 20,  6,  3,  4,  4,  4,  4,  7,  4,  7,  4,  8,  7,  5,  // 9x
   2,  3,  2,  3,  0,  1,  1,  1,  1,  4,  1,  4,  1,  5,  4,  5,  // Ax
   5,  6, 20,  6,  3,  4,  4,  4,  4,  7,  4,  7,  4,  8,  7, 20,  // Bx
   2,  3,  2,  3,  0,  1,  1,  1,  1,  4,  1,  4,  1,  5,  4,  3,  // Cx
   4,  6, 20,  6,  3,  4,  4,  4,  4,  7,  4,  7,  4,  8,  7, 20,  // Dx
   2,  3,  2,  3,  0,  1,  1,  1,  1,  4,  1,  4,  1,  5,  4,  3,  // Ex
   4,  6, 20,  6,  3,  4,  4,  4,  4,  7,  4,  7,  4,  8,  7, 20   // Fx
};

// From Addendum to The 6309 Book
// Indexed Addressing Mode Post Bytes
// https://colorcomputerarchive.com/repo/Documents/Microprocessors/HD6309/6309%20Indexed%20Cycle%20Counts.pdf

static int postbyte_cycles_6309_nat[0x100] = {
// x0  x1  x2  x3  x4  x5  x6  x7  x8  x9  xA  xB  xC  xD  xE  xF
   1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 0x
   1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 1x
   1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 2x
   1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 3x
   1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 4x
   1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 5x
   1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 6x
   1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 7x
   1,  2,  1,  2,  0,  1,  1,  1,  1,  3,  1,  2,  1,  3,  1,  0,  // 8x
   3,  5, 19,  5,  3,  4,  4,  4,  4,  7,  4,  5,  4,  6,  4,  4,  // 9x
   1,  2,  1,  2,  0,  1,  1,  1,  1,  3,  1,  2,  1,  3,  1,  2,  // Ax
   5,  5, 19,  5,  3,  4,  4,  4,  4,  7,  4,  5,  4,  6,  4, 19,  // Bx
   1,  2,  1,  2,  0,  1,  1,  1,  1,  3,  1,  2,  1,  3,  1,  1,  // Cx
   4,  5, 19,  5,  3,  4,  4,  4,  4,  7,  4,  5,  4,  6,  4, 19,  // Dx
   1,  2,  1,  2,  0,  1,  1,  1,  1,  3,  1,  2,  1,  3,  1,  1,  // Ex
   4,  5, 19,  5,  3,  4,  4,  4,  4,  7,  4,  5,  4,  6,  4, 19   // Fx
};

// In PSHS/PSHU/PULS/PULU, the postbyte controls which registers are pulled
//
// It helps to have a quick way to count the number of '1' in a nibble, in order
// to quickly calculate the total cycles.

static int count_ones_in_nibble[] =    { 0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4};

// ====================================================================
// Static variables
// ====================================================================

// Which state string to use
static const char *cpu_state;

// Is the CPU the 6309?
static int cpu6309 = 0;

// Which opcode table to use
static opcode_t *instr_table;

// 6809 registers: -1 means unknown
static int ACCA = -1;
static int ACCB = -1;
static int X    = -1;
static int Y    = -1;
static int S    = -1;
static int U    = -1;
static int DP   = -1;
static int PC   = -1;

// 6809 flags: -1 means unknown
static int E    = -1;
static int F    = -1;
static int H    = -1;
static int I    = -1;
static int N    = -1;
static int Z    = -1;
static int V    = -1;
static int C    = -1;

// Additional 6309 registers: -1 means unknown
static int ACCE = -1;
static int ACCF = -1;
static int TV   = -1;

// Additional 6309 flags: -1 means unknown
static int NM   = -1; // Native Mode
static int FM   = -1; // FIRQ Mode
static int IL   = -1; // Illegal Instruction Trap
static int DZ   = -1; // Divide by zero trap

// Misc
static int show_cycle_errors = 0;

// ====================================================================
// Fail flags
// ====================================================================

enum {
   FAIL_ACCA   = 0x00000010,
   FAIL_ACCB   = 0x00000020,
   FAIL_ACCE   = 0x00000040,
   FAIL_ACCF   = 0x00000080,
   FAIL_X      = 0x00000100,
   FAIL_Y      = 0x00000200,
   FAIL_U      = 0x00000400,
   FAIL_S      = 0x00000800,
   FAIL_DP     = 0x00001000,
   FAIL_E      = 0x00002000,
   FAIL_F      = 0x00004000,
   FAIL_H      = 0x00008000,
   FAIL_I      = 0x00010000,
   FAIL_N      = 0x00020000,
   FAIL_Z      = 0x00040000,
   FAIL_V      = 0x00080000,
   FAIL_C      = 0x00100000,
   FAIL_RESULT = 0x00200000,
   FAIL_VECTOR = 0x00400000,
   FAIL_CYCLES = 0x00800000,
   FAIL_UNDOC  = 0x01000000,
   FAIL_BADM   = 0x02000000,
};

static const char * fail_hints[32] = {
   "PC",
   "Memory",
   "?",
   "?",
   "ACCA",
   "ACCB",
   "ACCE",
   "ACCF",
   "X",
   "Y",
   "U",
   "S",
   "DP",
   "E",
   "F",
   "H",
   "I",
   "N",
   "Z",
   "V",
   "C",
   "Result",
   "Vector",
   "Cycles",
   "Undoc",
   "BadMode",
   "?",
   "?",
   "?",
   "?",
   "?",
   "?"
};

// ====================================================================
// Forward declarations
// ====================================================================

static opcode_t instr_table_6809[];
static opcode_t instr_table_6309[];

static operation_t op_TST  ;
static operation_t op_XSTX ;
static operation_t op_XSTU ;

// ====================================================================
// Helper Methods
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

static int get_FLAG(int i) {
   switch (i) {
   case 0: return C;
   case 1: return V;
   case 2: return Z;
   case 3: return N;
   case 4: return I;
   case 5: return H;
   case 6: return F;
   case 7: return E;
   }
   return -1;
}

static void set_FLAG(int i, int val) {
   switch (i) {
   case 0: C = val; break;
   case 1: V = val; break;
   case 2: Z = val; break;
   case 3: N = val; break;
   case 4: I = val; break;
   case 5: H = val; break;
   case 6: F = val; break;
   case 7: E = val; break;
   }
}

static int get_FLAGS() {
   if (E < 0 || F < 0 || H < 0 || I < 0 || N < 0 || Z < 0 || V < 0 || C < 0) {
      return -1;
   } else {
      return (E << 7) | (F << 6) | (H << 5) | (I << 4) | (N << 3) | (Z << 2) | (V << 1) | C;
   }
}

static void set_FLAGS(int val) {
   if (val >= 0) {
      E = (val >> 7) & 1;
      F = (val >> 6) & 1;
      H = (val >> 5) & 1;
      I = (val >> 4) & 1;
      N = (val >> 3) & 1;
      Z = (val >> 2) & 1;
      V = (val >> 1) & 1;
      C = (val >> 0) & 1;
   } else {
      E = -1;
      F = -1;
      H = -1;
      I = -1;
      N = -1;
      Z = -1;
      V = -1;
      C = -1;
   }
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

static int pack0(int byte) {
   return (byte >= 0) ? ((byte << 8) + byte) : -1;
}

static int pack(int hi_byte, int lo_byte) {
   return (hi_byte >= 0 && lo_byte >= 0) ? ((hi_byte << 8) + lo_byte) : -1;
}

static void unpack(int result, int *hi_byte, int *lo_byte) {
   if (hi_byte) {
      *hi_byte = result < 0 ? -1 : (result >> 8) & 0xff;
   }
   if (lo_byte) {
      *lo_byte = result < 0 ? -1 : result & 0xff;
   }
}

static int *get_regi(int i) {
   i &= 3;
   switch(i) {
   case 0: return &X;
   case 1: return &Y;
   case 2: return &U;
   default: return &S;
   }
}

// Used in EXN/TRV on the 6809
static int get_regp_6809(int i) {
   i &= 15;
   int ret;
   switch(i) {
   case  0: ret = pack(ACCA, ACCB); break;
   case  1: ret = X;                break;
   case  2: ret = Y;                break;
   case  3: ret = U;                break;
   case  4: ret = S;                break;
   case  5: ret = PC;               break;
   case  8: ret = ACCA;             break;
   case  9: ret = ACCB;             break;
   case 10: ret = get_FLAGS();      break;
   case 11: ret = DP;               break;
   default: ret = 0xffff;           break;
   }
   if (ret >= 0 && i >= 8) {
      // Extend 8-bit values to 16 bits by padding with FF
      return ret | 0xff00;
   } else {
      // Return 16-bit and undefined values as-is
      return ret;
   }
}

// Used in EXN/TRV on the 6809
static void set_regp_6809(int i, int val) {
   i &= 15;
   switch(i) {
   case  0: unpack(val, &ACCA, &ACCB); break;
   case  1: X  = val;                  break;
   case  2: Y  = val;                  break;
   case  3: U  = val;                  break;
   case  4: S  = val;                  break;
   case  5: PC = val;                  break;
   case  8: ACCA  = val & 0xff;        break;
   case  9: ACCB  = val & 0xff;        break;
   case 10: set_FLAGS(val);            break;
   case 11: DP = val & 0xff;           break;
   }
}

// Used in EXN/TRV on the 6309
static int get_regp_6309(int i) {
   i &= 15;
   int ret;
   switch(i) {
   case  0: ret = pack(ACCA, ACCB);   break;
   case  1: ret = X;                  break;
   case  2: ret = Y;                  break;
   case  3: ret = U;                  break;
   case  4: ret = S;                  break;
   case  5: ret = PC;                 break;
   case  6: ret = pack(ACCE, ACCF);   break;
   case  7: ret = TV;                 break;
   case  8: ret = pack0(ACCA);        break;
   case  9: ret = pack0(ACCB);        break;
   case 10: ret = pack0(get_FLAGS()); break;
   case 11: ret = pack0(DP);          break;
   case 12: ret = 0;                  break;
   case 13: ret = 0;                  break;
   case 14: ret = pack0(ACCE);        break;
   case 15: ret = pack0(ACCF);        break;
   }
   return ret;
}

// Used in EXN/TRV on the 6309
static void set_regp_6309(int i, int val) {
   i &= 15;
   switch(i) {
   case  0: unpack(val, &ACCA, &ACCB); break;
   case  1: X  = val;                  break;
   case  2: Y  = val;                  break;
   case  3: U  = val;                  break;
   case  4: S  = val;                  break;
   case  5: PC = val;                  break;
   case  6: unpack(val, &ACCE, &ACCF); break;
   case  7: TV = val;                  break;
   case  8: unpack(val, &ACCA,  NULL); break;
   case  9: unpack(val,  NULL, &ACCB); break;
   case 10: set_FLAGS(val & 0xff);     break;
   case 11: DP = (val >> 8) & 0xff;    break;
   case 14: unpack(val, &ACCE,  NULL); break;
   case 15: unpack(val,  NULL, &ACCF); break;
   }
}

// Used in EXN/TFR
static int get_regp(int i) {
   if (cpu6309) {
      return get_regp_6309(i);
   } else {
      return get_regp_6809(i);
   }
}

// Used in EXN/TFR
static void set_regp(int i, int val) {
   if (cpu6309) {
      set_regp_6309(i, val);
   } else {
      set_regp_6809(i, val);
   }
}

// ====================================================================
// Public Methods
// ====================================================================

static void em_6809_init(arguments_t *args) {
   show_cycle_errors = args->show_cycles;
   if (args->reg_s >= 0) {
      S = args->reg_s;
   }
   if (args->reg_u >= 0) {
      U = args->reg_u;
   }
   if (args->reg_pc >= 0) {
      PC = args->reg_pc;
   }
   if (args->reg_dp >= 0) {
      DP = args->reg_dp;
   }
   if (args->reg_nm >= 0) {
      NM = (args->reg_nm > 0);
   }
   if (args->reg_fm >= 0) {
      FM = (args->reg_fm > 0);
   }
   cpu6309 = args->cpu_type == CPU_6309 || args->cpu_type == CPU_6309E;

   if (cpu6309) {
      cpu_state = cpu_6309_state;
      instr_table = instr_table_6309;
   } else {
      cpu_state = cpu_6809_state;
      instr_table = instr_table_6809;
   }

   dis_6809_init(args->cpu_type, instr_table);

   // Validate the cycles in the maps are consistent
   opcode_t *instr_6309 = instr_table_6309;
   opcode_t *instr_6809 = instr_table_6809;
   int fail = 0;
   for (int i = 0; i < 0x300; i++) {
      if (!instr_6809->undocumented) {
         if (instr_6309->cycles != instr_6809->cycles) {
            printf("cycle mismatch in instruction table: %04x (%d cf %d)\n", i, instr_6309->cycles, instr_6809->cycles);
            fail = 1;
         }
      }
      instr_6309++;
      instr_6809++;
   }
   if (fail) {
      exit(1);
   }
}

static int em_6809_match_interrupt(sample_t *sample_q, int num_samples) {
   // Calculate expected offset to vector fetch taking account of
   // native mode on the 6309 pushing two extra bytes (ACCE/ACCF)
   // (and one pipeline stall cycle ??)
   int fast_o = (NM == 1) ? 10 :  7;
   int full_o = (NM == 1) ? 19 : 16;
   // FIQ:
   //    m +  7   addr=6 ba=0 bs=1
   //    m +  8   addr=7 ba=0 bs=1
   //    m +  9   addr=X ba=0 bs=0
   //    m + 10   <Start of first instruction>
   //
   if (sample_q[fast_o].ba < 1 && sample_q[fast_o].bs == 1 && sample_q[fast_o].addr == 0x6) {
      return fast_o + 3;
   }
   // IRQ:
   //    m + 16    addr=8 ba=0 bs=1
   //    m + 17    addr=9 ba=0 bs=1
   //    m + 18    addr=X ba=0 bs=0
   //    m + 19    <Start of first instruction>
   //
   if (sample_q[full_o].ba < 1 && sample_q[full_o].bs == 1 && sample_q[full_o].addr == 0x8) {
      return full_o + 3;;
   }
   // NMI:
   //    m + full_o    addr=C ba=0 bs=1
   //    m + 17    addr=D ba=0 bs=1
   //    m + 18    addr=X ba=0 bs=0
   //    m + 19    <Start of first instruction>
   //
   if (sample_q[full_o].ba < 1 && sample_q[full_o].bs == 1 && sample_q[full_o].addr == 0xC) {
      return full_o + 3;
   }
   return 0;
}

static int em_6809_match_reset(sample_t *sample_q, int num_samples) {
   // i        addr=E ba=0 bs=1
   // i + 1    addr=F ba=0 bs=1
   // i + 2    addr=X ba=0 bs=0
   // <Start of first instruction>
   for (int i = 0; i < num_samples - 3; i++) {
      if (sample_q[i].ba < 1 && sample_q[i].bs == 1 && sample_q[i].addr == 0x0E) {
         return i + 3;
      }
   }
   return 0;
}

// TODO: Cycle predition cases we don't yet handle correctly
//
// Instructions that OPTIONALLY trap
//   Indexed addressing with an illegal postbyte (92 b2 bf d2 df f2 ff)
//   EXN/TFR/xxxR writing back to the zero register (12/13) which may trap
//   DIRECTBIT addressing with an illegal register (3)
//   TFM with an illegal register (>4)
//   DIVD division by zero
//   DIVQ division by zero
//
// Long running instructions:
//   TFM
//
// Indefinte running instructions:
//   SYNC
//   CWAIT

static int get_num_cycles(sample_t *sample_q) {
   uint8_t b0 = sample_q[0].data;
   uint8_t b1 = sample_q[1].data;
   opcode_t *instr = get_instruction(instr_table, b0, b1);
   int cycle_count = (NM == 1) ? instr->cycles_native : instr->cycles;
   // Long Branch, one additional cycle if branch taken (unless in native mode)
   if (NM !=1 && b0 == 0x10) {
      switch (b1) {
      case 0x21: /* LBRN */
         cycle_count++;
         break;
      case 0x22: /* LBHI */
         if (Z == 0 && C == 0) {
            cycle_count++;
         }
         break;
      case 0x23: /* LBLS */
         if (Z == 1 || C == 1) {
            cycle_count++;
         }
         break;
      case 0x24: /* LBCC */
         if (C == 0) {
            cycle_count++;
         }
         break;
      case 0x25: /* LBLO */
         if (C == 1) {
            cycle_count++;
         }
         break;
      case 0x26: /* LBNE */
         if (Z == 0) {
            cycle_count++;
         }
         break;
      case 0x27: /* LBEQ */
         if (Z == 1) {
            cycle_count++;
         }
         break;
      case 0x28: /* LBVC */
         if (V == 0) {
            cycle_count++;
         }
         break;
      case 0x29: /* LBVS */
         if (V == 1) {
            cycle_count++;
         }
         break;
      case 0x2A: /* LBPL */
         if (N == 0) {
            cycle_count++;
         }
         break;
      case 0x2B: /* LBMI */
         if (N == 1) {
            cycle_count++;
         }
         break;
      case 0x2C: /* LBGE */
         if ((N == 0 && V == 0) || (N == 1 && V == 1))  {
            cycle_count++;
         }
         break;
      case 0x2D: /* LBLT */
         if ((N == 1 && V == 0) || (N == 0 && V == 1))  {
            cycle_count++;
         }
         break;
      case 0x2E: /* LBGT */
         if (Z == 0 && ((N == 0 && V == 0) || (N == 1 && V == 1)))  {
            cycle_count++;
         }
         break;
      case 0x2F: /* LBLE */
         if (Z == 0 || (N == 1 && V == 0) || (N == 0 && V == 1))  {
            cycle_count++;
         }
         break;
      }
   }

   if (b0 == 0x3B && E == 1) {
      // RTI takes 9 addition cycles if E = 1 (and two more if in native mode)
      cycle_count += (NM == 1) ? 11 : 9;
   } else if (b0 >= 0x34 && b0 <= 0x37) {
      // PSHS/PULS/PSHU/PULU
      cycle_count += count_ones_in_nibble[b1 & 0x0f];            // bits 0..3 are 8 bit registers
      cycle_count += count_ones_in_nibble[(b1 >> 4) & 0x0f] * 2; // bits 4..7 are 16 bit registers
   } else if (instr->mode == INDEXED) {
      // For INDEXED address, the instruction table cycles
      // are the minimum the instruction will execute in
      int postindex = (b0 == 0x10 || b0 == 0x11) ? 2 : 1;
      int postbyte = sample_q[postindex].data;
      int postbyte_cycles;
      if (cpu6309) {
         if (NM == 1) {
            postbyte_cycles = postbyte_cycles_6309_nat[postbyte];
         } else {
            postbyte_cycles = postbyte_cycles_6309_emu[postbyte];
         }
      } else {
         postbyte_cycles = postbyte_cycles_6809[postbyte];
      }
      // Negative indicates an illegal index mode
      if (postbyte_cycles < 0) {
         postbyte_cycles = 0;
      }
      cycle_count += postbyte_cycles;
   }
   return cycle_count;
}

static int count_cycles_with_lic(sample_t *sample_q) {
   int offset = (NM == 1) ? 1 : 0;
   for (int i = offset; i < LONGEST_INSTRUCTION; i++) {
      if (sample_q[i].type == LAST) {
         return 0;
      }
      if (sample_q[i].lic == 1) {
         i += (1 - offset);
         // Validate the num_cycles passed in
         if (show_cycle_errors) {
            int expected = get_num_cycles(sample_q);
            if (expected >= 0) {
               if (i != expected) {
                  failflag |= FAIL_CYCLES;
               }
            }
         }
         return i;
      }
   }
   return 1;
}

static int count_cycles_without_lic(sample_t *sample_q) {
   int num_cycles = get_num_cycles(sample_q);
   if (num_cycles >= 0) {
      return num_cycles;
   }
   printf ("cycle prediction unknown\n");
   return 1;
}

static int em_6809_count_cycles(sample_t *sample_q) {
   if (sample_q[0].lic < 0) {
      return count_cycles_without_lic(sample_q);
   } else {
      return count_cycles_with_lic(sample_q);
   }
}

static void em_6809_reset(sample_t *sample_q, int num_cycles, instruction_t *instruction) {
   instruction->pc = -1;
   // All other registers are unchanged on reset
   DP = 0;
   F  = 1;
   I  = 1;
   if (cpu6309) {
      NM = 0;
      FM = 0;
      IL = 0;
      DZ = 0;
   }
   PC = (sample_q[num_cycles - 3].data << 8) + sample_q[num_cycles - 2].data;
}

// Returns the old PC value
static int interrupt_helper(sample_t *sample_q, int offset, int full, int vector) {

   // FIQ
   //  0 Opcode
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
   //  5 UL
   //  6 UH
   //  7 YL
   //  8 YH
   //  9 XL
   // 10 XH
   // 11 DP
   //           <<< ACCF then ACCE in native mode
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

      if (NM == 1) {

         int f  = sample_q[i++].data;
         push8s(f);
         if (ACCF >= 0 && f != ACCF) {
            failflag |= FAIL_ACCF;
         }
         ACCF = f;

         int e  = sample_q[i++].data;
         push8s(e);
         if (ACCE >= 0 && e != ACCE) {
            failflag |= FAIL_ACCE;
         }
         ACCE = e;

      }

      int b  = sample_q[i++].data;
      push8s(b);
      if (ACCB >= 0 && b != ACCB) {
         failflag |= FAIL_ACCB;
      }
      ACCB = b;

      int a  = sample_q[i++].data;
      push8s(a);
      if (ACCA >= 0 && a != ACCA) {
         failflag |= FAIL_ACCA;
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

   // Is it the illegal instruction vector?
   if ((vector & 0xfffe) == 0xfff0) {
      if (vector & 1) {
         // DZ Trap
         DZ = 1;
      } else {
         // IL Trap
         IL = 1;
      }
   }

   // The vector must be even!
   vector &= 0xfffe;

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

static void em_6809_interrupt(sample_t *sample_q, int num_cycles, instruction_t *instruction) {
   // Calculate expected number of cycles in the interrupt dispatch,
   // taking account native mode on the 6309 pushing two extra bytes
   // (ACCE/ACCF)
   // (and one pipeline stall cycle ??)
   int fast_c = (NM == 1) ? 13 : 10;
   int full_c = (NM == 1) ? 22 : 19;
   int offset = (NM == 1) ?  4 :  3;
   if (num_cycles == fast_c) {
      instruction->pc = interrupt_helper(sample_q, offset, 0, 0xfff6);
   } else if (num_cycles == full_c && sample_q[full_c - 3].addr == 0x8) {
      // IRQ
      instruction->pc = interrupt_helper(sample_q, offset, 1, 0xfff8);
   } else if (num_cycles == full_c && sample_q[full_c - 3].addr == 0xC) {
      // NMI
      instruction->pc = interrupt_helper(sample_q, offset, 1, 0xfffC);
   } else {
      printf("*** could not determine interrupt type ***\n");
   }
}

static void em_6809_emulate(sample_t *sample_q, int num_cycles, instruction_t *instruction) {

   int b0 = sample_q[0].data;
   int b1 = sample_q[1].data;
   int oi = 0;
   int pb = 0;
   int index = 0;
   opcode_t *instr = get_instruction(instr_table, b0, b1);

   // Flag that an instruction marked as undocumented has been encoutered
   if (instr->undocumented) {
      failflag |= FAIL_UNDOC;
   }

   // Sanity check the LS 4 bits of the PC Address
   // TODO: A3 not reliable for reasons I don't understand
   if (PC >= 0 && sample_q[0].addr >= 0) {
      if ((PC & 7) != (sample_q[0].addr & 7)) {
         failflag |= FAIL_PC;
      }
   }

   // Memory modelling of the opcode and the prefic
   if (PC >= 0) {
      memory_read(b0, PC + index, MEM_INSTR);
   }
   index++;

   // If there is a prefix, skip past it and read the opcode
   if (b0 == 0x10 || b0 == 0x11) {
      if (PC >= 0) {
         memory_read(b0, PC + index, MEM_INSTR);
      }
      index++;
      // Increment opcode index (oi), which allows the rest of the code to ignore the prefix
      oi++;
   }

   // If there is a post byte, skip past it
   if (instr->mode == REGISTER || instr->mode == INDEXED || instr->mode == DIRECTBIT) {
      pb = sample_q[index].data;
      if (PC >= 0) {
         memory_read(pb, PC + index, MEM_INSTR);
      }
      index++;
   }

   // Process any additional operand bytes
   switch (instr->mode) {
   case INDEXED:
      // In some indexed addressing modes there is also a displacement
      if (pb & 0x80) {
         int type = pb & 0x0f;
         if (type == 8 || type == 9 || type == 12 || type == 13 || type == 15) {
            if (PC >= 0) {
               memory_read(sample_q[index].data, PC + index, MEM_INSTR);
            }
            index++;
         }
         if (type == 9 || type == 13 || type == 15) {
            if (PC >= 0) {
               memory_read(sample_q[index].data, PC + index, MEM_INSTR);
            }
            index++;
         }
      }
      break;
   case DIRECTBIT:
   case DIRECT:
   case RELATIVE_8:
   case IMMEDIATE_8:
      if (PC >= 0) {
         memory_read(sample_q[index].data, PC + index, MEM_INSTR);
      }
      index++;
      break;
   case EXTENDED:
   case RELATIVE_16:
   case IMMEDIATE_16:
      if (PC >= 0) {
         memory_read(sample_q[index    ].data, PC + index    , MEM_INSTR);
         memory_read(sample_q[index + 1].data, PC + index + 1, MEM_INSTR);
      }
      index += 2;
      break;
   case IMMEDIATE_32:
      if (PC >= 0) {
         memory_read(sample_q[index    ].data, PC + index    , MEM_INSTR);
         memory_read(sample_q[index + 1].data, PC + index + 1, MEM_INSTR);
         memory_read(sample_q[index + 2].data, PC + index + 2, MEM_INSTR);
         memory_read(sample_q[index + 3].data, PC + index + 3, MEM_INSTR);
      }
      index += 4;
      break;
   default:
      break;
   }

   // Copy instruction for ease of hex printing
   for (int i = 0; i < index; i++) {
      instruction->instr[i] = sample_q[i].data;
   }
   instruction->length = index;

   // In main, instruction->pc is checked against the emulated PC, so in the case
   // of JSR/BSR/LBSR this provides a means of sanity checking
   if (instr->op->type == JSROP) {
      instruction->pc = ((sample_q[num_cycles - 1].data << 8) + sample_q[num_cycles - 2].data - instruction->length) & 0xffff;
   } else {
      instruction->pc = PC;
   }

   // Update the PC assuming not change of flow takes place
   if (PC >= 0) {
      PC = (PC + instruction->length) & 0xffff;
   }

   // Pick out the operand (Fig 17 in datasheet, esp sheet 5)
   operand_t operand;
   if (instr->op == &op_TST && (NM != 1)) {
      // There are two dead cycles at the end of TST
      operand = sample_q[num_cycles - 3].data;
   } else if (instr->mode == REGISTER) {
      operand = sample_q[oi + 1].data; // This is the postbyte
   } else if (instr->op->type == RMWOP) {
      // Read-modify-write instruction (always 8-bit)
      operand = sample_q[num_cycles - 3].data;
   } else if (instr->op->size == SIZE_32) {
      operand = (sample_q[num_cycles - 4].data << 24) + (sample_q[num_cycles - 3].data << 16) + (sample_q[num_cycles - 2].data << 8) + sample_q[num_cycles - 1].data;
   } else if (instr->op->size == SIZE_16) {
      if (instr->op->type == LOADOP || instr->op->type == STOREOP || instr->op->type == JSROP || (NM == 1)) {
         // No dead cycle at the end with LDD/LDS/LDU/LDX/LDY/STD/STS/STU/STX/STY/JSR
         operand = (sample_q[num_cycles - 2].data << 8) + sample_q[num_cycles - 1].data;
      } else {
         // Dead cycle at the end in ADDD/CMPD/CMPS/CMPU/CMPX/CMPY/SUBD
         operand = (sample_q[num_cycles - 3].data << 8) + sample_q[num_cycles - 2].data;
      }
   } else {
      operand = sample_q[num_cycles - 1].data;
   }

   // Operand 2 is the value written back in a store or read-modify-write
   operand_t operand2 = operand;
   if (instr->op->type == RMWOP || instr->op->type == STOREOP) {
      if (instr->op->size == SIZE_32) {
         operand2 = (sample_q[num_cycles - 4].data << 24) + (sample_q[num_cycles - 3].data << 16) + (sample_q[num_cycles - 2].data << 8) + sample_q[num_cycles - 1].data;
      } else if (instr->op->size == SIZE_16) {
         operand2 = (sample_q[num_cycles - 2].data << 8) + sample_q[num_cycles - 1].data;
      } else {
         operand2 = sample_q[num_cycles - 1].data;
      }
   }

   // Calculate the effective address (for additional memory reads)
   // Note: oi is the opcode index (0 = no prefix, 1 = prefix)
   ea_t ea = -1;
   switch (instr->mode) {
   case RELATIVE_8:
      if (PC >= 0) {
         ea = (PC + (int8_t)sample_q[oi + 1].data) & 0xffff;
      }
      break;
   case RELATIVE_16:
      if (PC >= 0) {
         ea = (PC + (int16_t)((sample_q[oi + 1].data << 8) + sample_q[oi + 2].data)) & 0xffff;
      }
      break;
   case DIRECT:
      if (DP >= 0) {
         ea = (DP << 8) + sample_q[oi + 1].data;
      }
      break;
   case DIRECTBIT:
      // There is a postbyte
      if (DP >= 0) {
         ea = (DP << 8) + sample_q[oi + 2].data;
      }
      break;
   case EXTENDED:
      ea = (sample_q[oi + 1].data << 8) + sample_q[oi + 2].data;
      break;
   case INDEXED:
      {
         int *reg = get_regi((pb >> 5) & 0x03);

         // In 6309 mode, the 7 illegal postbytes cause illegal instruction traps
         // 92 b2 d2 f2   1xx10010
         //    bf df ff   1xx11111
         if (cpu6309 && (pb != 0x9f) && (((pb & 0x9f) == 0x92) || (pb & 0x9f) == 0x9f)) {
            interrupt_helper(sample_q, oi + 2, 1, 0xfff0);
            return;
         }

         if (!(pb & 0x80)) {       /* n4,R */
            if (*reg >= 0) {
               if (pb & 0x10) {
                  ea = (*reg - ((pb & 0x0f) ^ 0x0f) - 1) & 0xffff;
               } else {
                  ea = (*reg + (pb & 0x0f)) & 0xffff;
               }
            }
         } else if (cpu6309 && ((pb & 0x1f) == 0x0f || (pb & 0x1f) == 0x10)) {

            // Extra 6309 W indexed modes
            int W = pack(ACCE, ACCF);
            if (W >= 0) {
               switch ((pb >> 5) & 3) {
               case 0:           /* ,W */
                  ea = W;
                  break;
               case 1:           /* n15,W */
                  ea = (W + (sample_q[oi + 2].data << 8) + sample_q[oi + 3].data) & 0xffff;
                  break;
               case 2:           /* ,W++ */
                  ea = W;
                  W = (W + 2) & 0xffff;
                  break;
               case 3:           /* ,--W */
                  W = (W - 2) & 0xffff;
                  ea = W;
                  break;
               }
            }

         } else {

            switch (pb & 0x0f) {
            case 0:                 /* ,R+ */
               if (*reg >= 0) {
                  ea = *reg;
                  *reg = (*reg + 1) & 0xffff;
               }
               break;
            case 1:                 /* ,R++ */
               if (*reg >= 0) {
                  ea = *reg;
                  *reg = (*reg + 2) & 0xffff;
               }
               break;
            case 2:                 /* ,-R */
               if (*reg >= 0) {
                  *reg = (*reg - 1) & 0xffff;
                  ea = *reg;
               }
               break;
            case 3:                 /* ,--R */
               if (*reg >= 0) {
                  *reg = (*reg - 2) & 0xffff;
                  ea = *reg;
               }
               break;
            case 4:                 /* ,R */
               if (*reg >= 0) {
                  ea = *reg;
               }
               break;
            case 5:                 /* B,R */
               if (*reg >= 0 && ACCB >= 0) {
                  ea = (*reg + ACCB) & 0xffff;
               }
               break;
            case 6:                 /* A,R */
               if (*reg >= 0 && ACCA >= 0) {
                  ea = (*reg + ACCA) & 0xffff;
               }
               break;
            case 7:                 /* E,R */
               if (cpu6309 && *reg >= 0 && ACCE >= 0) {
                  ea = (*reg + ACCE) & 0xffff;
               }
               break;
            case 8:                 /* n7,R */
               if (*reg >= 0) {
                  ea = (*reg + (int8_t)(sample_q[oi + 2].data)) & 0xffff;
               }
               break;
            case 9:                 /* n15,R */
               if (*reg >= 0) {
                  ea = (*reg + (int16_t)((sample_q[oi + 2].data << 8) + sample_q[oi + 3].data)) & 0xffff;
               }
               break;
            case 10:                /* F,R */
               if (cpu6309 && *reg >= 0 && ACCF >= 0) {
                  ea = (*reg + ACCF) & 0xffff;
               }
               break;
            case 11:                /* D,R */
               if (*reg >= 0 && ACCA >= 0 && ACCB >= 0) {
                  ea = (*reg + (ACCA << 8) + ACCB) & 0xffff;
               }
               break;
            case 12:                /* n7,PCR */
               if (PC >= 0) {
                  ea = (PC + (int8_t)(sample_q[oi + 2].data)) & 0xffff;
               }
               break;
            case 13:                /* n15,PCR */
               if (PC >= 0) {
                  ea = (PC + (int16_t)((sample_q[oi + 2].data << 8) + sample_q[oi + 3].data)) & 0xffff;
               }
               break;
            case 14:                /* W,R */
               if (cpu6309 && *reg >= 0 && ACCE >= 0 && ACCF >= 0) {
                  ea = (*reg + (ACCE << 8) + ACCF) & 0xffff;
               }
               break;
            case 15:                /* [n] */
               ea = ((sample_q[oi + 2].data << 8) + sample_q[oi + 3].data) & 0xffff;
               break;
            }
            if (pb & 0x10) {
               // In this mode there is a further level of indirection to find the ea
               // The postbyte_cycles tables contain the number of extra cycles for this indexed mode
               int offset = 0;
               if (cpu6309) {
                  if (NM == 1) {
                     offset += postbyte_cycles_6309_nat[pb];
                  } else {
                     offset += postbyte_cycles_6309_emu[pb];
                  }
               } else {
                  offset += postbyte_cycles_6809[pb];
               }
               if (offset >= 0 && offset <= MAX_VALID_POSTBYTE_CYCLES) {
                  // In long form: offset = oi + 2 + postbyte_cycles - 2;
                  // - the oi skips the prefix
                  // - the first 2 skips the opcode and postbyte
                  // - the final 2 steps back to the effective address read
                  offset += oi;
                  if (ea >= 0) {
                     memory_read(sample_q[offset    ].data, ea,     MEM_POINTER);
                     memory_read(sample_q[offset + 1].data, ea + 1, MEM_POINTER);
                  }
                  ea = ((sample_q[offset].data << 8) + sample_q[offset + 1].data) & 0xffff;
               } else {
                  failflag |= FAIL_BADM;
               }
            }
         }
      }
      break;
   default:
      break;
   }
   // Special Case XSTX/XSTU
   if (PC >= 0 && (instr->op == &op_XSTX || instr->op == &op_XSTU)) {
      // The write happens to second byte of immediate data
      ea = (PC - 1) & 0xffff;
   }

   // Model memory reads
   if (ea >= 0 && (instr->op->type == LOADOP || instr->op->type == READOP || instr->op->type == RMWOP)) {
      if (instr->op->size == SIZE_32) {
         memory_read((operand >> 24) & 0xff, ea,     MEM_DATA);
         memory_read((operand >> 16) & 0xff, ea + 1, MEM_DATA);
         memory_read((operand >>  8) & 0xff, ea + 2, MEM_DATA);
         memory_read( operand         & 0xff, ea + 3, MEM_DATA);
      } else if (instr->op->size == SIZE_16) {
         memory_read((operand >>  8) & 0xff, ea,     MEM_DATA);
         memory_read( operand        & 0xff, ea + 1, MEM_DATA);
      } else {
         memory_read( operand        & 0xff, ea,     MEM_DATA);
      }
   }

   // Emulate the instruction
   if (instr->op->emulate) {
      int result = instr->op->emulate(operand, ea, sample_q);

      if (instr->op->type == STOREOP || instr->op->type == RMWOP) {

         // WRTEOP:
         //    8-bit: STA STB
         //   16-bit: STD STS STU STX STY
         //
         // RMWOP:
         //    8-bit: ASL ASR CLK COM DEC INC LSR BEG ROL ROR

         // Check result of instruction against bye
         if (result >= 0 && result != operand2) {
            failflag |= FAIL_RESULT;
         }

         // Model memory writes based on result seen on bus
         if (ea >= 0) {
            if (instr->op->size == SIZE_32) {
               memory_write((operand2 >> 24) & 0xff, ea,     MEM_DATA);
               memory_write((operand2 >> 16) & 0xff, ea + 1, MEM_DATA);
               memory_write((operand2 >>  8) & 0xff, ea + 2, MEM_DATA);
               memory_write( operand2        & 0xff, ea + 3, MEM_DATA);
            } else if (instr->op->size == SIZE_16) {
               memory_write((operand2 >>  8) & 0xff, ea,     MEM_DATA);
               memory_write( operand2        & 0xff, ea + 1, MEM_DATA);
            } else {
               memory_write( operand2        & 0xff, ea,     MEM_DATA);
            }
         }
      }
   }
}

static int em_6809_get_PC() {
   return PC;
}

static int em_6809_read_memory(int address) {
   return memory_read_raw(address);
}

static char *em_6809_get_state(char *buffer) {
   // 6809: A=?? B=?? X=???? Y=???? U=???? S=???? DP=?? E=? F=? H=? I=? N=? Z=? V=? C=?";
   // 6309: A=?? B=?? E=?? F=?? X=???? Y=???? U=???? S=???? DP=?? T=???? E=? F=? H=? I=? N=? Z=? V=? C=? DZ=? IL=? FM=? NM=?"
   char *bp = buffer;
   strcpy(bp, cpu_state);
   bp += 2;
   if (ACCA >= 0) {
      write_hex2(bp, ACCA);
   }
   bp += 5;
   if (ACCB >= 0) {
      write_hex2(bp, ACCB);
   }
   bp += 5;
   if (cpu6309) {
      if (ACCE >= 0) {
         write_hex2(bp, ACCE);
      }
      bp += 5;
      if (ACCF >= 0) {
         write_hex2(bp, ACCF);
      }
      bp += 5;
   }
   if (X >= 0) {
      write_hex4(bp, X);
   }
   bp += 7;
   if (Y >= 0) {
      write_hex4(bp, Y);
   }
   bp += 7;
   if (U >= 0) {
      write_hex4(bp, U);
   }
   bp += 7;
   if (S >= 0) {
      write_hex4(bp, S);
   }
   bp += 8; // One extra as DP is a two-character name
   if (DP >= 0) {
      write_hex2(bp, DP);
   }
   bp += 5;
   if (cpu6309) {
      if (TV >= 0) {
         write_hex4(bp, TV);
      }
      bp += 7;
   }
   if (E >= 0) {
      *bp = '0' + E;
   }
   bp += 4;
   if (F >= 0) {
      *bp = '0' + F;
   }
   bp += 4;
   if (H >= 0) {
      *bp = '0' + H;
   }
   bp += 4;
   if (I >= 0) {
      *bp = '0' + I;
   }
   bp += 4;
   if (N >= 0) {
      *bp = '0' + N;
   }
   bp += 4;
   if (Z >= 0) {
      *bp = '0' + Z;
   }
   bp += 4;
   if (V >= 0) {
      *bp = '0' + V;
   }
   bp += 4;
   if (C >= 0) {
      *bp = '0' + C;
   }
   if (cpu6309) {
      bp += 5;
      if (DZ >= 0) {
         *bp = '0' + DZ;
      }
      bp += 5;
      if (IL >= 0) {
         *bp = '0' + IL;
      }
      bp += 5;
      if (FM >= 0) {
         *bp = '0' + FM;
      }
      bp += 5;
      if (NM >= 0) {
         *bp = '0' + NM;
      }
   }
   bp += 1;
   return bp;
}

static uint32_t em_6809_get_and_clear_fail() {
   uint32_t ret = failflag;
   failflag = 0;
   return ret;
}

static int em_6809_write_fail(char *bp, uint32_t fail) {
   char *ptr = bp;
   if (fail) {
      ptr += write_s(ptr, " : Prediction failed for: ");
      int comma = 0;
      for (int i = 0; i < 32; i++) {
         if (fail & 1) {
            if (comma) {
               *ptr++ = ',';
            }
            ptr += write_s(ptr, fail_hints[i]);
            comma = 1;
         }
         fail >>= 1;
      }
      return ptr - bp;
   } else {
      return 0;
   }
}

cpu_emulator_t em_6809 = {
   .init = em_6809_init,
   .match_reset = em_6809_match_reset,
   .match_interrupt = em_6809_match_interrupt,
   .count_cycles = em_6809_count_cycles,
   .reset = em_6809_reset,
   .interrupt = em_6809_interrupt,
   .emulate = em_6809_emulate,
   .disassemble = dis_6809_disassemble,
   .get_PC = em_6809_get_PC,
   .read_memory = em_6809_read_memory,
   .get_state = em_6809_get_state,
   .get_and_clear_fail = em_6809_get_and_clear_fail,
   .write_fail = em_6809_write_fail,
};

// ====================================================================
// Instruction helpers
// ====================================================================

static int add_helper(int val, int cin, operand_t operand) {
   if (val >= 0 && cin >= 0 && operand >= 0) {
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
      // Return the 8-bit result
      return tmp;
   } else {
      set_HNZVC_unknown();
      return -1;
   }
}

static int add16_helper(int val, int cin, int operand) {
   if (val >= 0 && cin >= 0 && operand >= 0) {
      // Perform the addition (there is no carry in)
      int tmp = val + operand + cin;
      // The carry flag is bit 16 of the result
      C = (tmp >> 16) & 1;
      // The overflow flag is: IF ((a^b^res^(res>>1))&0x80) SEV else CLV
      V = (((val ^ operand ^ tmp) >> 15) & 1) ^ C;
      // Truncate the result to 16 bits
      tmp &= 0xffff;
      // Set the flags
      set_NZ16(tmp);
      // Return the 16-bit result
      return tmp;
   } else {
      set_NZVC_unknown();
      return -1;
   }
}

static int and_helper(int val, operand_t operand) {
   if (val >= 0 && operand >= 0) {
      val &= operand;
      set_NZ(val);
   } else {
      set_NZ_unknown();
   }
   V = 0;
   return val;
}

static int and16_helper(int val, operand_t operand) {
   if (val >= 0 && operand >= 0) {
      val &= operand;
      set_NZ16(val);
   } else {
      set_NZ_unknown();
   }
   V = 0;
   return val;
}

static int asl_helper(int val) {
   if (val >= 0) {
      C = (val >> 7) & 1;
      // V is the xor of bits 7,6 of val
      V = ((val >> 6) & 1) ^ C;
      val = (val << 1) & 0xff;
      set_NZ(val);
   } else {
      set_NZVC_unknown();
   }
   // The datasheet says the half-carry flag is undefined, but in practice
   // it seems to be unchanged (i.e. no errors). TODO: needs further testing
   // H = -1;
   return val;
}

static int asl16_helper(int val) {
   if (val >= 0) {
      C = (val >> 15) & 1;
      // V is the xor of bits 15,14 of val
      V = ((val >> 14) & 1) ^ C;
      val = (val << 1) & 0xffff;
      set_NZ(val);
   } else {
      set_NZVC_unknown();
   }
   return val;
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

static int asr16_helper(int val) {
   if (val >= 0) {
      C = val & 1;
      val = (val & 0x8000) | (val >> 1);
      set_NZ(val);
   } else {
      set_NZC_unknown();
   }
   return val;
}

static void bit_helper(int val, operand_t operand) {
   if (val >= 0 && operand >= 0) {
      set_NZ(val & operand);
   } else {
      set_NZ_unknown();
   }
   V = 0;
}

static void bit16_helper(int val, operand_t operand) {
   if (val >= 0 && operand >= 0) {
      set_NZ16(val & operand);
   } else {
      set_NZ_unknown();
   }
   V = 0;
}

static int clr_helper() {
   N = 0;
   Z = 1;
   C = 0;
   V = 0;
   return 0;
}

static void cmp_helper(int val, operand_t operand) {
   if (val >= 0 && operand >= 0) {
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

static void cmp16_helper(int val, operand_t operand) {
   if (val >= 0 && operand >= 0) {
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

static int com16_helper(int val) {
   if (val >= 0) {
      val ^= 0xffff;
      set_NZ16(val);
   } else {
      set_NZ_unknown();
   }
   V = 0;
   C = 1;
   return val;
}

static int dec_helper(int val) {
   if (val >= 0) {
      val = (val - 1) & 0xff;
      set_NZ(val);
      // V indicates signed overflow, which onlt happens when going from 0x80->0x7f
      V = (val == 0x7f);
   } else {
      val = -1;
      set_NZV_unknown();
   }
   return val;
}

static int dec16_helper(int val) {
   if (val >= 0) {
      val = (val - 1) & 0xffff;
      set_NZ16(val);
      // V indicates signed overflow, which onlt happens when going from 0x8000->0x7fff
      V = (val == 0x7fff);
   } else {
      val = -1;
      set_NZV_unknown();
   }
   return val;
}

static int eor_helper(int val, operand_t operand) {
   if (val >= 0 && operand >= 0) {
      val ^= operand;
      set_NZ(val);
   } else {
      set_NZ_unknown();
   }
   V = 0;
   return val;
}

static int eor16_helper(int val, operand_t operand) {
   if (val >= 0 && operand >= 0) {
      val ^= operand;
      set_NZ16(val);
   } else {
      set_NZ_unknown();
   }
   V = 0;
   return val;
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

static int inc16_helper(int val) {
   if (val >= 0) {
      val = (val + 1) & 0xffff;
      set_NZ(val);
      // V indicates signed overflow, which only happens when going from 127->128
      V = (val == 0x8000);
   } else {
      val = -1;
      set_NZV_unknown();
   }
   return val;
}

static int ld_helper(int val) {
   val &= 0xff;
   set_NZ(val);
   V = 0;
   return val;
}

static int ld16_helper(int val) {
   val &= 0xffff;
   set_NZ16(val);
   V = 0;
   return val;
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

static int neg16_helper(int val) {
   V = (val == 0x8000);
   C = (val == 0x0000);
   val = (-val) & 0xffff;
   set_NZ16(val);
   return val;
}

static int or_helper(int val, operand_t operand) {
   if (val >= 0 && operand >= 0) {
      val |= operand;
      set_NZ(val);
   } else {
      set_NZ_unknown();
   }
   V = 0;
   return val;
}

static int or16_helper(int val, operand_t operand) {
   if (val >= 0 && operand >= 0) {
      val |= operand;
      set_NZ16(val);
   } else {
      set_NZ_unknown();
   }
   V = 0;
   return val;
}

static void push_helper(sample_t *sample_q, int system) {
   //  0 opcode
   //  1 postbyte
   //  2 ---
   //  3 ---
   //  4 ---    skipped in 6309 native mode
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
   int i = (NM == 1) ? 4 : 5;
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
         failflag |= FAIL_ACCB;
      }
      ACCB = tmp;
   }
   if (pb & 0x02) {
      tmp = sample_q[i++].data;
      push8(tmp);
      if (ACCA >= 0 && ACCA != tmp) {
         failflag |= FAIL_ACCA;
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

static void pull_helper(sample_t *sample_q, int system) {
   //  0 opcode
   //  1 postbyte
   //  2 ---
   //  3 ---    skipped in 6309 native mode
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
   int i = (NM == 1) ? 3 : 4;
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

static int rol_helper(int val) {
   if (val >= 0 && C >= 0) {
      int tmp = (val << 1) + C;
      // C is bit 7 of val
      C = (val >> 7) & 1;
      // V is the xor of bits 7,6 of val
      V = ((val >> 6) & 1) ^ C;
      // truncate to 8 bits
      val = tmp & 0xff;
      set_NZ(val);
   } else {
      val = -1;
      set_NZVC_unknown();
   }
   return val;
}

static int rol16_helper(int val) {
   if (val >= 0 && C >= 0) {
      int tmp = (val << 1) + C;
      // C is bit 15 of val
      C = (val >> 15) & 1;
      // V is the xor of bits 15,14 of val
      V = ((val >> 14) & 1) ^ C;
      // truncate to 8 bits
      val = tmp & 0xffff;
      set_NZ(val);
   } else {
      val = -1;
      set_NZVC_unknown();
   }
   return val;
}

static int ror_helper(int val) {
   if (val >= 0 && C >= 0) {
      int tmp = (val >> 1) + (C << 7);
      // C is bit 0 of val (V is unaffected)
      C = val & 1;
      // truncate to 8 bits
      val = tmp & 0xff;
      set_NZ16(val);
   } else {
      val = -1;
      set_NZC_unknown();
   }
   return val;
}

static int ror16_helper(int val) {
   if (val >= 0 && C >= 0) {
      int tmp = (val >> 1) + (C << 15);
      // C is bit 0 of val (V is unaffected)
      C = val & 1;
      // truncate to 8 bits
      val = tmp & 0xffff;
      set_NZ16(val);
   } else {
      val = -1;
      set_NZC_unknown();
   }
   return val;
}

static int st_helper(int val, operand_t operand, int fail) {
   if (val >= 0 && operand >= 0) {
      if (operand != val) {
         failflag |= fail;
      }
   }
   V = 0;
   set_NZ(operand);
   return operand;
}

static int st16_helper(int val, operand_t operand, int fail) {
   if (val >= 0 && operand >= 0) {
      if (operand != val) {
         failflag |= fail;
      }
   }
   V = 0;
   set_NZ16(operand);
   return operand;
}

static int sub_helper(int val, int cin, operand_t operand) {
   if (val >= 0 && cin >= 0  && operand >= 0) {
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

static int sub16_helper(int val, int cin, operand_t operand) {
   if (val >= 0 && cin >= 0 && operand >= 0) {
      int tmp = val - operand - cin;
      // The carry flag is bit 16 of the result
      C = (tmp >> 16) & 1;
      // The overflow flag is: IF ((a^b^res^(res>>1))&0x8000) SEV else CLV
      V = (((val ^ operand ^ tmp) >> 15) & 1) ^ C;
      // Truncate the result to 16 bits
      tmp &= 0xffff;
      // Set the flags
      set_NZ16(tmp);
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

// ====================================================================
// Common 6809/6309 Instructions
// ====================================================================

static int op_fn_ABX(operand_t operand, ea_t ea, sample_t *sample_q) {
   // X = X + B
   if (X >= 0 && ACCB >= 0) {
      X = (X + ACCB) & 0xffff;
   } else {
      X = -1;
   }
   return -1;
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
   int D = pack(ACCA, ACCB);
   D = add16_helper(D, 0, operand);
   unpack(D, &ACCA, &ACCB);
   return -1;
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

static int op_fn_BITA(operand_t operand, ea_t ea, sample_t *sample_q) {
   bit_helper(ACCA, operand);
   return -1;
}

static int op_fn_BITB(operand_t operand, ea_t ea, sample_t *sample_q) {
   bit_helper(ACCB, operand);
   return -1;
}

static int op_fn_BLE(operand_t operand, ea_t ea, sample_t *sample_q) {
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

static int op_fn_CMPA(operand_t operand, ea_t ea, sample_t *sample_q) {
   cmp_helper(ACCA, operand);
   return -1;
}

static int op_fn_CMPB(operand_t operand, ea_t ea, sample_t *sample_q) {
   cmp_helper(ACCB, operand);
   return -1;
}

static int op_fn_CMPD(operand_t operand, ea_t ea, sample_t *sample_q) {
   cmp16_helper(pack(ACCA, ACCB), operand);
   return -1;
}

static int op_fn_CMPS(operand_t operand, ea_t ea, sample_t *sample_q) {
   cmp16_helper(S, operand);
   return -1;
}

static int op_fn_CMPU(operand_t operand, ea_t ea, sample_t *sample_q) {
   cmp16_helper(U, operand);
   return -1;
}

static int op_fn_CMPX(operand_t operand, ea_t ea, sample_t *sample_q) {
   cmp16_helper(X, operand);
   return -1;
}

static int op_fn_CMPY(operand_t operand, ea_t ea, sample_t *sample_q) {
   cmp16_helper(Y, operand);
   return -1;
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
   // According to Atkinson, page 66, there is a 6809 corner case where:
   //   EXC A,D
   // should behave as:
   //   EXC A,B
   // i.e. do A<==>B
   if (!cpu6309 && reg1 == 8 && reg2 == 0) {
      reg2 = 9;
   }
   int tmp1 = get_regp(reg1);
   int tmp2 = get_regp(reg2);
   set_regp(reg1, tmp2);
   set_regp(reg2, tmp1);
   return -1;
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

static int op_fn_LDA(operand_t operand, ea_t ea, sample_t *sample_q) {
   ACCA = ld_helper(operand);
   return -1;
}

static int op_fn_LDB(operand_t operand, ea_t ea, sample_t *sample_q) {
   ACCB = ld_helper(operand);
   return -1;
}

static int op_fn_LDD(operand_t operand, ea_t ea, sample_t *sample_q) {
   int tmp = ld16_helper(operand);
   ACCA = (tmp >> 8) & 0xff;
   ACCB = tmp & 0xff;
   return -1;
}

static int op_fn_LDS(operand_t operand, ea_t ea, sample_t *sample_q) {
   S = ld16_helper(operand);
   return -1;
}

static int op_fn_LDU(operand_t operand, ea_t ea, sample_t *sample_q) {
   U = ld16_helper(operand);
   return -1;
}

static int op_fn_LDX(operand_t operand, ea_t ea, sample_t *sample_q) {
   X = ld16_helper(operand);
   return -1;
}

static int op_fn_LDY(operand_t operand, ea_t ea, sample_t *sample_q) {
   Y = ld16_helper(operand);
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
   // D = A * B (unsigned)
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

static int op_fn_PSHS(operand_t operand, ea_t ea, sample_t *sample_q) {
   push_helper(sample_q, 1); // 1 = PSHS
   return -1;
}

static int op_fn_PSHU(operand_t operand, ea_t ea, sample_t *sample_q) {
   push_helper(sample_q, 0); // 0 = PSHU
   return -1;
}
static int op_fn_PULS(operand_t operand, ea_t ea, sample_t *sample_q) {
   pull_helper(sample_q, 1); // 1 = PULS
   return -1;
}

static int op_fn_PULU(operand_t operand, ea_t ea, sample_t *sample_q) {
   pull_helper(sample_q, 0); // 0 = PULU
   return -1;
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
   //           <<< ACCE then ACCF in native mode
   //   5 DP
   //   6 XH
   //   7 XL
   //   8 YH
   //   9 YL
   //  10 UH
   //  11 UL
   //  12 PCH
   //  13 PCL
   //  14 ---

   int i = 2;

   // Do the flags first, as the stacked E indicates how much to restore
   set_FLAGS(sample_q[i++].data);

   // Update the register state
   if (E == 1) {
      ACCA = sample_q[i++].data;
      ACCB = sample_q[i++].data;
      if (NM == 1) {
         ACCE = sample_q[i++].data;
         ACCF = sample_q[i++].data;
      }
      DP = sample_q[i++].data;
      X  = sample_q[i++].data << 8;
      X |= sample_q[i++].data;
      Y  = sample_q[i++].data << 8;
      Y |= sample_q[i++].data;
      U  = sample_q[i++].data << 8;
      U |= sample_q[i++].data;
   }
   PC  = sample_q[i++].data << 8;
   PC |= sample_q[i++].data;

   // Memory modelling
   for (int j = 2; j < i; j++) {
      pop8s(sample_q[j].data);
   }

   return -1;
}

static int op_fn_RTS(operand_t operand, ea_t ea, sample_t *sample_q) {
   pop8s(sample_q[2].data);
   pop8s(sample_q[3].data);
   PC = (sample_q[2].data << 8) + sample_q[3].data;
   return -1;
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
         ACCA = 0xff;
      } else {
         ACCA = 0x00;
      }
      set_NZ(ACCB);
   } else {
      ACCA = -1;
      set_NZ_unknown();
   }
   // TODO: Confirm V is cleared, documentation is inconsistent
   V = 0;
   return -1;
}

static int op_fn_STA(operand_t operand, ea_t ea, sample_t *sample_q) {
   ACCA = st_helper(ACCA, operand, FAIL_ACCA);
   return operand;
}

static int op_fn_STB(operand_t operand, ea_t ea, sample_t *sample_q) {
   ACCB = st_helper(ACCB, operand, FAIL_ACCB);
   return operand;
}

static int op_fn_STD(operand_t operand, ea_t ea, sample_t *sample_q) {
   int D = pack(ACCA, ACCB);
   D = st16_helper(D, operand, FAIL_ACCA | FAIL_ACCB);
   unpack(D, &ACCA, &ACCB);
   return operand;
}

static int op_fn_STS(operand_t operand, ea_t ea, sample_t *sample_q) {
   S = st16_helper(S, operand, FAIL_S);
   return operand;
}

static int op_fn_STU(operand_t operand, ea_t ea, sample_t *sample_q) {
   U = st16_helper(U, operand, FAIL_U);
   return operand;
}

static int op_fn_STX(operand_t operand, ea_t ea, sample_t *sample_q) {
   X = st16_helper(X, operand, FAIL_X);
   return operand;
}

static int op_fn_STY(operand_t operand, ea_t ea, sample_t *sample_q) {
   Y = st16_helper(Y, operand, FAIL_Y);
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
   int D = pack(ACCA, ACCB);
   D = sub16_helper(D, 0, operand);
   unpack(D, &ACCA, &ACCB);
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
// Undocumented Instructions
// ====================================================================

// Much of the information on the undocumented instructions comes from here:
// https://colorcomputerarchive.com/repo/Documents/Books/Motorola%206809%20and%20Hitachi%206309%20Programming%20Reference%20(Darren%20Atkinson).pdf

static int op_fn_XX(operand_t operand, ea_t ea, sample_t *sample_q) {
   return -1;
}

// Undefined opcodes in row 2 execute as a NEG instruction when the
// Carry bit in CC is 0, and as a COM instruction when the Carry bit
// is 1

static int op_fn_XNC(operand_t operand, ea_t ea, sample_t *sample_q) {
   if (C == 0) {
      neg_helper(operand);
   } else if (C == 1) {
      com_helper(operand);
   } else {
      set_NZVC_unknown();
   }
   return -1;
}

// Opcodes $14, $15 and $CD all cause the CPU to stop functioning
// normally. One or more of these may be the HCF (Halt and Catch Fire)
// instruction. The HCF instruction was provided for manufacturing
// test purposes. Its causes the CPU to halt execution and enter a
// mode in which the Address lines are incrementally strobed.

static int op_fn_XHCF(operand_t operand, ea_t ea, sample_t *sample_q) {
   // TODO
   return -1;
}

// Opcode $18 affects only the Condition Codes register (CC). The
// value in the Overflow bit (V) is shifted into the Zero bit (Z)
// while the value in the IRQ Mask bit (I) is shifted into the Half
// Carry bit (H). All other bits in the CC register are
// cleared. Execution of this opcode takes 3 MPU cycles.

static int op_fn_X18(operand_t operand, ea_t ea, sample_t *sample_q) {
   E = 0; // Bit 7
   F = 0;
   H = I;
   I = 0;
   N = 0;
   Z = V;
   V = 0;
   C = 0; // Bit 0
   return -1;
}

// Opcodes $87 and $C7 read and discard an 8-bit Immediate operand
// which follows the opcode. The value of the immediate byte is
// irrelevant. The Negative bit (N) in the CC register is always set,
// while the Zero (Z) and Overflow (V) bits are always cleared. No
// other bits in the Condition Codes register are affected. Each of
// these opcodes execute in 2 MPU cycles.

static int op_fn_X8C7(operand_t operand, ea_t ea, sample_t *sample_q) {
   N = 1;
   Z = 0;
   V = 0;
   return -1;
}

// Opcode $3E is similar to the SWI instruction. It stacks the Entire
// register state, sets the I and F bits in the Condition Codes
// register and then loads the PC register with an address obtained
// from the RESET vector ($FFFE:F). This could potentially be used as
// a fourth Software Interrupt instruction, so long as the code
// invoked by the Reset vector is able to differentiate between a
// software reset and a hardware reset. It does NOT set the Entire bit
// (E) in the CC register prior to stacking the register state. This
// could cause an RTI instruction for a Reset handler to fail to
// operate as expected. This opcode uses the same number of MPU cycles
// as SWI (15).

static int op_fn_XRES(operand_t operand, ea_t ea, sample_t *sample_q) {
   interrupt_helper(sample_q, 3, 1, 0xfffe);
   return -1;
}

// Opcodes $8F and $CF are STX Immediate and STU Immediate
// respectively. These instructions are partially functional. Two
// bytes of immediate data follow the opcode. The first immediate byte
// is read and discarded by the instruction. The lower half (LSB) of
// the X or U register is then written into the second immediate
// byte. The Negative bit (N) in the CC register is always set, while
// the Zero (Z) and Overflow (V) bits are always cleared. No other
// bits in the Condition Codes register are affected. Each of these
// opcodes execute in 3 MPU cycles.

static int op_fn_XSTX(operand_t operand, ea_t ea, sample_t *sample_q) {
   N = 1;
   Z = 0;
   V = 0;
   return X & 0xff;
}

static int op_fn_XSTU(operand_t operand, ea_t ea, sample_t *sample_q) {
   N = 1;
   Z = 0;
   V = 0;
   return U & 0xff;
}

// ====================================================================
// 6309 Helpers
// ====================================================================

// Used in ADCR/ADDR/ANDR/CMPR/EORR/ORRR/SBCR/SUBR on the 6309 only
//
// 8->16 requires promotion to D or W
// 16->8 requires demotion (just use LSB)
//
// These rules are different to EXN/TRF

// r0 is the right operand
static int get_r0(int pb) {
   int ret;
   int src = (pb >> 4) & 0xf;
   int dst = pb & 0xf;
   if (dst >= 8) {
      // dst is 8 bits
      switch(src) {
         // src is 16 bits, demote to 8 bits
      case  0: ret = ACCB;                        break;
      case  1: ret = ( X < 0) ? -1 : ( X & 0xff); break;
      case  2: ret = ( Y < 0) ? -1 : ( Y & 0xff); break;
      case  3: ret = ( U < 0) ? -1 : ( U & 0xff); break;
      case  4: ret = ( S < 0) ? -1 : ( S & 0xff); break;
      case  5: ret = (PC < 0) ? -1 : (PC & 0xff); break;
      case  6: ret = ACCF;                        break;
      case  7: ret = (TV < 0) ? -1 : (TV & 0xff); break;
         // src is 8 bits
      case  8: ret = ACCA;                        break;
      case  9: ret = ACCB;                        break;
      case 10: ret = get_FLAGS();                 break;
      case 11: ret = DP;                          break;
      case 14: ret = ACCE;                        break;
      case 15: ret = ACCF;                        break;
      default: ret = 0;
      }
   } else {
      // dst is 16 bits
      switch(src) {
         // src is 16 bits
      case  0: ret = pack(ACCA, ACCB);            break;
      case  1: ret = X;                           break;
      case  2: ret = Y;                           break;
      case  3: ret = U;                           break;
      case  4: ret = S;                           break;
      case  5: ret = PC;                          break;
      case  6: ret = pack(ACCE, ACCF);            break;
      case  7: ret = TV;                          break;
         // src is 8 bits, promote to 16 bits
      case  8: ret = pack(ACCA, ACCB);            break;
      case  9: ret = pack(ACCA, ACCB);            break;
      case 10: ret = get_FLAGS();                 break;
      case 11: ret = (DP < 0) ? -1 : (DP << 8);   break;
      case 14: ret = pack(ACCE, ACCF);            break;
      case 15: ret = pack(ACCE, ACCF);            break;
      default: ret = 0;
      }
   }
   return ret;
}

// r1 is the left operand
static int get_r1(int pb) {
   int ret;
   int dst = pb & 0xf;
   switch(dst) {
   case  0: ret = pack(ACCA, ACCB);            break;
   case  1: ret = X;                           break;
   case  2: ret = Y;                           break;
   case  3: ret = U;                           break;
   case  4: ret = S;                           break;
   case  5: ret = PC;                          break;
   case  6: ret = pack(ACCE, ACCF);            break;
   case  7: ret = TV;                          break;
   case  8: ret = ACCA;                        break;
   case  9: ret = ACCB;                        break;
   case 10: ret = get_FLAGS();                 break;
   case 11: ret = DP;                          break;
   case 14: ret = ACCE;                        break;
   case 15: ret = ACCF;                        break;
   default: ret = 0;
   }
   return ret;
}

// r1 is the where the result will be written
static void set_r1(int pb, int val) {
   int dst = pb & 0xf;
   switch(dst) {
   case  0: unpack(val, &ACCA, &ACCB); break;
   case  1: X  = val;                  break;
   case  2: Y  = val;                  break;
   case  3: U  = val;                  break;
   case  4: S  = val;                  break;
   case  5: PC = val;                  break;
   case  6: unpack(val, &ACCE, &ACCF); break;
   case  7: TV = val;                  break;
   case  8: ACCA = val;                break;
   case  9: ACCB = val;                break;
   case 10: set_FLAGS(val);            break;
   case 11: DP = val;                  break;
   case 14: ACCE = val;                break;
   case 15: ACCF = val;                break;
   }
}

static void singlebit_helper(operand_t operand, sample_t *sample_q) {
   // Pickout the opcode and the postbyte from the samples
   int opcode = sample_q[1].data;
   int postbyte = sample_q[2].data;

   // Parse the post byte
   int reg_num    = (postbyte >> 6) & 3; // Bits 7..6
   int mem_bitnum = (postbyte >> 3) & 7; // Bits 5..3
   int reg_bitnum = (postbyte     ) & 7; // Bits 2..0

   // Extract the memory bit), which must be 0 or 1
   int mem_bit = (operand >> mem_bitnum) & 1;

   // Extract register bit, which can be 0, 1 or -1
   int reg_bit;
   switch (reg_num) {
   case 0: reg_bit = get_FLAG(reg_bitnum);                       break;
   case 1: reg_bit = (ACCA < 0) ? -1 : (ACCA >> reg_bitnum) & 1; break;
   case 2: reg_bit = (ACCB < 0) ? -1 : (ACCB >> reg_bitnum) & 1; break;
   default:  return; // TODO Illegal Instruction Trap ?
   }

   // Compute the bit operation, allowing for reg_bit to be unknown
   switch (opcode) {
   case 0x30: // BAND
      if (reg_bit >= 0) {
         reg_bit &= mem_bit;
      } else if (!mem_bit) {
         reg_bit = 0;
      }
      break;
   case 0x31: // BIAND
      if (reg_bit >= 0) {
         reg_bit &= !mem_bit;
      } else if (mem_bit) {
         reg_bit = 0;
      }
      break;
   case 0x32: // BOR
      if (reg_bit >= 0) {
         reg_bit |= mem_bit;
      } else if (mem_bit) {
         reg_bit = 1;
      }
      break;
   case 0x33: // BIOR
      if (reg_bit >= 0) {
         reg_bit |= !mem_bit;
      } else if (!mem_bit) {
         reg_bit = 1;
      }
      break;
   case 0x34: // BEOR
      if (reg_bit >= 0) {
         reg_bit ^= mem_bit;
      }
      break;
   case 0x35: // BIEOR
      if (reg_bit >= 0) {
         reg_bit ^= !mem_bit;
      }
      break;
   case 0x36: // LDBT
      reg_bit = mem_bit;
      break;
   }

   int reg_mask = 0xff ^ (1 << reg_bitnum);
   switch (reg_num) {
   case 0:
      set_FLAG(reg_bitnum, reg_bit);
      break;
   case 1:
      if (reg_bit >= 0) {
         ACCA = (ACCA & reg_mask) | (reg_bit << reg_bitnum);
      } else {
         ACCA = -1;
      }
      break;
   case 2:
      if (reg_bit >= 0) {
         ACCB = (ACCB & reg_mask) | (reg_bit << reg_bitnum);
      } else {
         ACCB = -1;
      }
      break;
   }
}

static void set_q_nz(uint32_t val) {
   ACCA = (val >> 24) & 0xff;
   ACCB = (val >> 16) & 0xff;
   ACCE = (val >>  8) & 0xff;
   ACCF =  val        & 0xff;
      N = (val >> 31) & 1;
      Z = (val == 0);
}

static void set_q_nz_unknown() {
   ACCA = -1;
   ACCB = -1;
   ACCE = -1;
   ACCF = -1;
   N    = -1;
   Z    = -1;
}

static void pushw_helper(sample_t *sample_q, int system) {
   void (*push8)(int) = system ? push8s : push8u;
   int e = sample_q[5].data;
   int f = sample_q[4].data;
   push8(f);
   if (ACCF >= 0 && ACCF != f) {
      failflag |= FAIL_ACCF;
   }
   ACCF = f;
   push8(e);
   if (ACCE >= 0 && ACCE != e) {
      failflag |= FAIL_ACCE;
   }
   ACCE = e;
}

static void pullw_helper(sample_t *sample_q, int system) {
   void (*pop8)(int) = system ? pop8s : pop8u;
   int e = sample_q[4].data;
   int f = sample_q[5].data;
   pop8(e);
   ACCE = e;
   pop8(f);
   ACCF = f;
}

// ====================================================================
// 6309 Instructions
// ====================================================================

static int op_fn_ADCD(operand_t operand, ea_t ea, sample_t *sample_q) {
   int D = pack(ACCA, ACCB);
   D = add16_helper(D, C, operand);
   unpack(D, &ACCA, &ACCB);
   return -1;
}

static int op_fn_ADCR(operand_t operand, ea_t ea, sample_t *sample_q) {
   // r1 := r1 + r0 + C
   int r0 = get_r0(operand);
   int r1 = get_r1(operand);
   int result;
   if ((operand & 0x0f) < 8) {
      result = add16_helper(r1, C, r0);
   } else {
      result = add_helper(r1, C, r0);
   }
   set_r1(operand, result);
   return -1;
}

static int op_fn_ADDE(operand_t operand, ea_t ea, sample_t *sample_q) {
   ACCE = add_helper(ACCE, 0, operand);
   return -1;
}

static int op_fn_ADDF(operand_t operand, ea_t ea, sample_t *sample_q) {
   ACCF = add_helper(ACCF, 0, operand);
   return -1;
}

static int op_fn_ADDR(operand_t operand, ea_t ea, sample_t *sample_q) {
   // r1 := r1 + r0
   int r0 = get_r0(operand);
   int r1 = get_r1(operand);
   int result;
   if ((operand & 0x0f) < 8) {
      result = add16_helper(r1, 0, r0);
   } else {
      result = add_helper(r1, 0, r0);
   }
   set_r1(operand, result);
   return -1;
}

static int op_fn_ADDW(operand_t operand, ea_t ea, sample_t *sample_q) {
   int W = pack(ACCE, ACCF);
   W = add16_helper(W, 0, operand);
   unpack(W, &ACCE, &ACCF);
   return -1;
}

static int op_fn_AIM(operand_t operand, ea_t ea, sample_t *sample_q) {
   return and_helper(operand, sample_q[1].data);
}

static int op_fn_ANDD(operand_t operand, ea_t ea, sample_t *sample_q) {
   int D = pack(ACCA, ACCB);
   D = and16_helper(D, operand);
   unpack(D, &ACCA, &ACCB);
   return -1;
}

static int op_fn_ANDR(operand_t operand, ea_t ea, sample_t *sample_q) {
   // r1 := r1 & r0
   int r0 = get_r0(operand);
   int r1 = get_r1(operand);
   int result;
   if ((operand & 0x0f) < 8) {
      result = and16_helper(r1, r0);
   } else {
      result = and_helper(r1, r0);
   }
   set_r1(operand, result);
   return -1;
}

static int op_fn_ASLD(operand_t operand, ea_t ea, sample_t *sample_q) {
   int D = pack(ACCA, ACCB);
   D = asl16_helper(D);
   unpack(D, &ACCA, &ACCB);
   return -1;
}

static int op_fn_ASRD(operand_t operand, ea_t ea, sample_t *sample_q) {
   int D = pack(ACCA, ACCB);
   D = asr16_helper(D);
   unpack(D, &ACCA, &ACCB);
   return -1;
}

static int op_fn_BAND(operand_t operand, ea_t ea, sample_t *sample_q) {
   singlebit_helper(operand, sample_q);
   return -1;
}

static int op_fn_BEOR(operand_t operand, ea_t ea, sample_t *sample_q) {
   singlebit_helper(operand, sample_q);
   return -1;
}

static int op_fn_BIAND(operand_t operand, ea_t ea, sample_t *sample_q) {
   singlebit_helper(operand, sample_q);
   return -1;
}

static int op_fn_BIEOR(operand_t operand, ea_t ea, sample_t *sample_q) {
   singlebit_helper(operand, sample_q);
   return -1;
}

static int op_fn_BIOR(operand_t operand, ea_t ea, sample_t *sample_q) {
   singlebit_helper(operand, sample_q);
   return -1;
}

static int op_fn_BITD(operand_t operand, ea_t ea, sample_t *sample_q) {
   int D = pack(ACCA, ACCB);
   bit16_helper(D, operand);
   return -1;
}

static int op_fn_BITMD(operand_t operand, ea_t ea, sample_t *sample_q) {
   int b7 = 0;
   if (operand & 0x80) {
      b7 = DZ;
      DZ = 0;
   }
   int b6 = 0;
   if (operand & 0x40) {
      b6 = IL;
      IL = 0;
   }
   if (b6 == 0 && b7 == 0) {
      Z = 1;
   } else if (b6 == 1 || b7 == 1) {
      Z = 0;
   } else {
      Z = -1;
   }
   return -1;
}

static int op_fn_BOR(operand_t operand, ea_t ea, sample_t *sample_q) {
   singlebit_helper(operand, sample_q);
   return -1;
}

static int op_fn_CLRD(operand_t operand, ea_t ea, sample_t *sample_q) {
   ACCA = ACCB = clr_helper();
   return -1;
}

static int op_fn_CLRE(operand_t operand, ea_t ea, sample_t *sample_q) {
   ACCE = clr_helper();
   return -1;
}

static int op_fn_CLRF(operand_t operand, ea_t ea, sample_t *sample_q) {
   ACCF = clr_helper();
   return -1;
}

static int op_fn_CLRW(operand_t operand, ea_t ea, sample_t *sample_q) {
   ACCE = ACCF = clr_helper();
   return -1;
}

static int op_fn_CMPE(operand_t operand, ea_t ea, sample_t *sample_q) {
   cmp_helper(ACCE, operand);
   return -1;
}

static int op_fn_CMPF(operand_t operand, ea_t ea, sample_t *sample_q) {
   cmp_helper(ACCF, operand);
   return -1;
}

static int op_fn_CMPR(operand_t operand, ea_t ea, sample_t *sample_q) {
   // r1 := r1 & r0
   int r0 = get_r0(operand);
   int r1 = get_r1(operand);
   if ((operand & 0x0f) < 8) {
      cmp16_helper(r1, r0);
   } else {
      cmp_helper(r1, r0);
   }
   return -1;
}

static int op_fn_CMPW(operand_t operand, ea_t ea, sample_t *sample_q) {
   cmp16_helper(pack(ACCE, ACCF), operand);
   return -1;
}

static int op_fn_COMD(operand_t operand, ea_t ea, sample_t *sample_q) {
   int D = pack(ACCA, ACCB);
   D = com16_helper(D);
   unpack(D, &ACCA, &ACCB);
   return -1;
}

static int op_fn_COME(operand_t operand, ea_t ea, sample_t *sample_q) {
   ACCE = com_helper(ACCE);
   return -1;
}

static int op_fn_COMF(operand_t operand, ea_t ea, sample_t *sample_q) {
   ACCF = com_helper(ACCF);
   return -1;
}

static int op_fn_COMW(operand_t operand, ea_t ea, sample_t *sample_q) {
   int W = pack(ACCE, ACCF);
   W = com16_helper(W);
   unpack(W, &ACCE, &ACCF);
   return -1;
}

static int op_fn_DECD(operand_t operand, ea_t ea, sample_t *sample_q) {
   int D = pack(ACCA, ACCB);
   D = dec16_helper(D);
   unpack(D, &ACCA, &ACCB);
   return -1;
}

static int op_fn_DECE(operand_t operand, ea_t ea, sample_t *sample_q) {
   ACCE = dec_helper(ACCE);
   return -1;
}

static int op_fn_DECF(operand_t operand, ea_t ea, sample_t *sample_q) {
   ACCF = dec_helper(ACCF);
   return -1;
}

static int op_fn_DECW(operand_t operand, ea_t ea, sample_t *sample_q) {
   int W = pack(ACCE, ACCF);
   W = dec16_helper(W);
   unpack(W, &ACCE, &ACCF);
   return -1;
}

static int op_fn_DIVD(operand_t operand, ea_t ea, sample_t *sample_q) {
   if (operand == 0) {
      // Set bit 0 of the vector to differentiate DZ Trap from IL Trap
      interrupt_helper(sample_q, 3, 1, 0xfff1);
   } else if (ACCA < 0 || ACCB < 0) {
      ACCA = -1;
      ACCB = -1;
      set_NZVC_unknown();
   } else {
      int16_t         a = (int16_t)((ACCA << 8) + ACCB);
       int8_t         b = ( int8_t)operand;
      int16_t  quotient = a / b;
      int16_t remainder = a % b;
      // TODO: Check the details of overflow with an exhaustive test
      if (quotient < -256 || quotient > 255) {
         // A range overflow has occurred, accumulators not modified
         N = 0;
         Z = 0;
         V = 1;
         C = 0;
      } else {
         ACCA = remainder & 0xff;
         ACCB = quotient  & 0xff;
         C    = quotient  & 1;
         set_NZ(ACCB);
         if (quotient < -128 || quotient > 127) {
            // A two-complement overflow has occurred, set overflow
            V = 1;
         }
      }
   }
   return -1;
}

static int op_fn_DIVQ(operand_t operand, ea_t ea, sample_t *sample_q) {
   if (operand == 0) {
      // Set bit 0 of the vector to differentiate DZ Trap from IL Trap
      interrupt_helper(sample_q, 3, 1, 0xfff1);
   } else if (ACCA < 0 || ACCB < 0 || ACCE < 0 || ACCF < 0) {
      ACCA = -1;
      ACCB = -1;
      ACCE = -1;
      ACCF = -1;
      set_NZVC_unknown();
   } else {
      int32_t         a = (int32_t)((ACCA << 24) + (ACCB << 16) + (ACCE << 8) + ACCF);
      int16_t         b = (int16_t)operand;
      int32_t  quotient = a / b;
      int32_t remainder = a % b;
      // TODO: Check the details of overflow with an exhaustive test
      if (quotient < -65536 || quotient > 65535) {
         // A range overflow has occurred, accumulators not modified
         N = 0;
         Z = 0;
         V = 1;
         C = 0;
      } else {
         ACCA = (remainder >> 8) & 0xff;
         ACCB =  remainder       & 0xff;
         ACCE = (quotient  >> 8) & 0xff;
         ACCF =  quotient        & 0xff;
         C    =  quotient & 1;
         set_NZ(ACCB);
         if (quotient < -32768 || quotient > 32767) {
            // A two-complement overflow has occurred, set overflow
            V = 1;
         }
      }
   }
   return -1;
}

static int op_fn_EIM(operand_t operand, ea_t ea, sample_t *sample_q) {
   return eor_helper(operand, sample_q[1].data);
}

static int op_fn_EORD(operand_t operand, ea_t ea, sample_t *sample_q) {
   int D = pack(ACCA, ACCB);
   D = eor16_helper(D, operand);
   unpack(D, &ACCA, &ACCB);
   return -1;
}

static int op_fn_EORR(operand_t operand, ea_t ea, sample_t *sample_q) {
   // r1 := r1 ^ r0
   int r0 = get_r0(operand);
   int r1 = get_r1(operand);
   int result;
   if ((operand & 0x0f) < 8) {
      result = eor16_helper(r1, r0);
   } else {
      result = eor_helper(r1, r0);
   }
   set_r1(operand, result);
   return -1;
}

static int op_fn_INCD(operand_t operand, ea_t ea, sample_t *sample_q) {
   int D = pack(ACCA, ACCB);
   D  = inc16_helper(D);
   unpack(D, &ACCA, &ACCB);
   return -1;
}

static int op_fn_INCE(operand_t operand, ea_t ea, sample_t *sample_q) {
   ACCE = inc_helper(ACCE);
   return -1;
}

static int op_fn_INCF(operand_t operand, ea_t ea, sample_t *sample_q) {
   ACCF = inc_helper(ACCF);
   return -1;
}

static int op_fn_INCW(operand_t operand, ea_t ea, sample_t *sample_q) {
   int W = pack(ACCE, ACCF);
   W = inc16_helper(W);
   unpack(W, &ACCE, &ACCF);
   return -1;
}

static int op_fn_LDBT(operand_t operand, ea_t ea, sample_t *sample_q) {
   singlebit_helper(operand, sample_q);
   return -1;
}

static int op_fn_LDE(operand_t operand, ea_t ea, sample_t *sample_q) {
   ACCE = ld_helper(operand);
   return -1;
}

static int op_fn_LDF(operand_t operand, ea_t ea, sample_t *sample_q) {
   ACCF = ld_helper(operand);
   return -1;
}

static int op_fn_LDMD(operand_t operand, ea_t ea, sample_t *sample_q) {
   NM = operand & 1;
   FM = (operand >> 1) & 1;
   return -1;
}

static int op_fn_LDQ(operand_t operand, ea_t ea, sample_t *sample_q) {
   set_q_nz((uint32_t) operand);
   V = 0;
   return -1;
}

static int op_fn_LDW(operand_t operand, ea_t ea, sample_t *sample_q) {
   int W = ld16_helper(operand);
   unpack(W, &ACCE, &ACCF);
   return -1;
}

static int op_fn_LSRD(operand_t operand, ea_t ea, sample_t *sample_q) {
   int D = pack(ACCA, ACCB);
   D = lsr_helper(D);
   unpack(D, &ACCA, &ACCB);
   return -1;
}

static int op_fn_LSRW(operand_t operand, ea_t ea, sample_t *sample_q) {
   int W = pack(ACCE, ACCF);
   W = lsr_helper(W);
   unpack(W, &ACCE, &ACCF);
   return -1;
}

static int op_fn_MULD(operand_t operand, ea_t ea, sample_t *sample_q) {
   // Q = D * imm16 (signed)
   int D = pack(ACCA, ACCB);
   if (D >= 0) {
      int16_t a = (int16_t)D;
      int16_t b = (int16_t)operand;
      set_q_nz((uint32_t)(a * b));
   } else {
      set_q_nz_unknown();
   }
   return -1;
}

static int op_fn_NEGD(operand_t operand, ea_t ea, sample_t *sample_q) {
   int D = pack(ACCA, ACCB);
   D = neg16_helper(D);
   unpack(D, &ACCA, &ACCB);
   return -1;
}

static int op_fn_OIM(operand_t operand, ea_t ea, sample_t *sample_q) {
   return or_helper(operand, sample_q[1].data);
}

static int op_fn_ORD(operand_t operand, ea_t ea, sample_t *sample_q) {
   int D = pack(ACCA, ACCB);
   D = or16_helper(D, operand);
   unpack(D, &ACCA, &ACCB);
   return -1;
}

static int op_fn_ORR(operand_t operand, ea_t ea, sample_t *sample_q) {
   // r1 := r1 | r0
   int r0 = get_r0(operand);
   int r1 = get_r1(operand);
   int result;
   if ((operand & 0x0f) < 8) {
      result = or16_helper(r1, r0);
   } else {
      result = or_helper(r1, r0);
   }
   set_r1(operand, result);
   return -1;
}

static int op_fn_PSHSW(operand_t operand, ea_t ea, sample_t *sample_q) {
   pushw_helper(sample_q, 1);
   return -1;
}

static int op_fn_PSHUW(operand_t operand, ea_t ea, sample_t *sample_q) {
   pushw_helper(sample_q, 0);
   return -1;
}

static int op_fn_PULSW(operand_t operand, ea_t ea, sample_t *sample_q) {
   pullw_helper(sample_q, 1);
   return -1;
}

static int op_fn_PULUW(operand_t operand, ea_t ea, sample_t *sample_q) {
   pullw_helper(sample_q, 0);
   return -1;
}

static int op_fn_ROLD(operand_t operand, ea_t ea, sample_t *sample_q) {
   int D = pack(ACCA, ACCB);
   D = rol16_helper(D);
   unpack(D, &ACCA, &ACCB);
   return -1;
}

static int op_fn_ROLW(operand_t operand, ea_t ea, sample_t *sample_q) {
   int W = pack(ACCE, ACCF);
   W = rol16_helper(W);
   unpack(W, &ACCE, &ACCF);
   return -1;
}

static int op_fn_RORD(operand_t operand, ea_t ea, sample_t *sample_q) {
   int D = pack(ACCA, ACCB);
   D = ror16_helper(D);
   unpack(D, &ACCA, &ACCB);
   return -1;
}

static int op_fn_RORW(operand_t operand, ea_t ea, sample_t *sample_q) {
   int W = pack(ACCE, ACCF);
   W = ror16_helper(W);
   unpack(W, &ACCE, &ACCF);
   return -1;
}

static int op_fn_SBCD(operand_t operand, ea_t ea, sample_t *sample_q) {
   int D = pack(ACCA, ACCB);
   D = sub16_helper(D, C, operand);
   unpack(D, &ACCA, &ACCB);
   return -1;
}

static int op_fn_SBCR(operand_t operand, ea_t ea, sample_t *sample_q) {
   // r1 := r1 - r0 - C
   int r0 = get_r0(operand);
   int r1 = get_r1(operand);
   int result;
   if ((operand & 0x0f) < 8) {
      result = sub16_helper(r1, C, r0);
   } else {
      result = sub_helper(r1, C, r0);
   }
   set_r1(operand, result);
   return -1;
}

static int op_fn_SEXW(operand_t operand, ea_t ea, sample_t *sample_q) {
   // Sign extend 16-bit value in W to 32-bit value in Q
   // W =           ACCE ACCF
   // Q = ACCA ACCB ACCE ACCF
   if (ACCE >= 0) {
      if (ACCE & 0x80) {
         ACCA = 0xff;
         ACCB = 0xff;
         N = 1;
      } else {
         ACCA = 0x00;
         ACCB = 0x00;
         N = 0;
      }
      // By calculating the flags this way, we can be slightly less pessimistic
      if (ACCF >= 0) {
         Z = (ACCE == 0 && ACCF == 0);
      } else {
         Z = -1;
      }
   } else {
      ACCA = -1;
      ACCB = -1;
      set_NZ_unknown();
   }
   // V flag is not affected, see:
   // https://github.com/mamedev/mame/blob/ab6237da/src/devices/cpu/m6809/hd6309.cpp#L108
   return -1;
}

static int op_fn_STBT(operand_t operand, ea_t ea, sample_t *sample_q) {
   // Pickout the postbyte from the samples
   int postbyte = sample_q[2].data;

   // Parse the post byte
   int reg_num    = (postbyte >> 6) & 3; // Bits 7..6
   int mem_bitnum = (postbyte >> 3) & 7; // Bits 5..3
   int reg_bitnum = (postbyte     ) & 7; // Bits 2..0

   // Extract register bit, which can be 0, 1 or -1
   int reg_bit;
   switch (reg_num) {
   case 0: reg_bit = get_FLAG(reg_bitnum);                       break;
   case 1: reg_bit = (ACCA < 0) ? -1 : (ACCA >> reg_bitnum) & 1; break;
   case 2: reg_bit = (ACCB < 0) ? -1 : (ACCB >> reg_bitnum) & 1; break;
   default:  return -1; // TODO Illegal Instruction Trap ?
   }

   if (reg_bit >= 0) {
      // Calculate the value that is expected to be written back
      return (operand & (0xff ^ (1 << mem_bitnum))) | (reg_bit << mem_bitnum);
   } else {
      // No memory modelling is possible
      return -1;
   }
}

static int op_fn_STE(operand_t operand, ea_t ea, sample_t *sample_q) {
   ACCE = st_helper(ACCE, operand, FAIL_ACCE);
   return -1;
}

static int op_fn_STF(operand_t operand, ea_t ea, sample_t *sample_q) {
   ACCF = st_helper(ACCF, operand, FAIL_ACCF);
   return -1;
}

static int op_fn_STQ(operand_t operand, ea_t ea, sample_t *sample_q) {
   uint32_t result = (uint32_t) operand;
   if (ACCF >= 0 && (uint32_t)ACCF != (result & 0xff)) {
      failflag |= FAIL_ACCF;
   }
   ACCF = result & 0xff;
   result >>= 8;
   if (ACCE >= 0 && (uint32_t)ACCE != (result & 0xff)) {
      failflag |= FAIL_ACCE;
   }
   ACCE = result & 0xff;
   result >>= 8;
   if (ACCB >= 0 && (uint32_t)ACCB != (result & 0xff)) {
      failflag |= FAIL_ACCB;
   }
   ACCB = result & 0xff;
   result >>= 8;
   if (ACCA >= 0 && (uint32_t)ACCA != (result & 0xff)) {
      failflag |= FAIL_ACCA;
   }
   ACCA = result & 0xff;
   Z = (ACCA == 0 && ACCB == 0 && ACCE == 0 && ACCF == 0);
   N = (ACCA >> 7) & 1;
   return -1;
}

static int op_fn_STW(operand_t operand, ea_t ea, sample_t *sample_q) {
   int W = pack(ACCE, ACCF);
   W = st16_helper(W, operand, FAIL_ACCE | FAIL_ACCF);
   unpack(W, &ACCE, &ACCF);
   return -1;
}

static int op_fn_SUBE(operand_t operand, ea_t ea, sample_t *sample_q) {
   ACCE = sub_helper(ACCE, 0, operand);
   return -1;
}

static int op_fn_SUBF(operand_t operand, ea_t ea, sample_t *sample_q) {
   ACCF = sub_helper(ACCF, 0, operand);
   return -1;
}

static int op_fn_SUBR(operand_t operand, ea_t ea, sample_t *sample_q) {
   // r1 := r1 + r0
   int r0 = get_r0(operand);
   int r1 = get_r1(operand);
   int result;
   if ((operand & 0x0f) < 8) {
      result = sub16_helper(r1, 0, r0);
   } else {
      result = sub_helper(r1, 0, r0);
   }
   set_r1(operand, result);
   return -1;
}

static int op_fn_SUBW(operand_t operand, ea_t ea, sample_t *sample_q) {
   int W = pack(ACCE, ACCF);
   W = sub16_helper(W, 0, operand);
   unpack(W, &ACCE, &ACCF);
   return -1;
}

// TODO: TFM is interruptable (!!!) and can be upto ~192K cycles (!!!!)
//
// TODO: We need a pattern to cope with variable/very long instructions like CWAIT / SYNC / TFM

static int op_fn_TFM(operand_t operand, ea_t ea, sample_t *sample_q) {
   // 1138 TFM r0+, r1+
   // 1139 TFM r0-, r1-
   // 113A TFM r0+, r1
   // 113B TFM r0 , r1+
   int opcode = sample_q[1].data;
   int postbyte = sample_q[2].data;
   int r0 = (postbyte >> 4) & 0xf;
   int r1 = postbyte & 0xf;

   // Only D, X, Y, U, S are legal, anything else causes an illegal instruction trap
   if (r0 > 4 || r1 > 4) {

      // TODO: confirm offset of the start of the trap
      interrupt_helper(sample_q, 4, 1, 0xfff0);

   } else {

      // Get reg0 (the source address)
      int reg0;
      switch (r0) {
      case 1:  reg0 = X;                break;
      case 2:  reg0 = Y;                break;
      case 3:  reg0 = U;                break;
      case 4:  reg0 = S;                break;
      default: reg0 = pack(ACCA, ACCB); break;
      }

      // Get reg1 (the destination address)
      int reg1;
      switch (r1) {
      case 1:  reg1 = X;                break;
      case 2:  reg1 = Y;                break;
      case 3:  reg1 = U;                break;
      case 4:  reg1 = S;                break;
      default: reg1 = pack(ACCA, ACCB); break;
      }

      // TODO: For now we skip the memory modelling, as we don't have many samples queued

      // The number of bytes transferred is in W
      int W = pack(ACCE, ACCF);

      // Update final value of R0
      if (reg0 >= 0) {
         if (opcode == 0x38 || opcode == 0x3a) {
            if (W >= 0) {
               reg0 = (reg0 + W) & 0xffff;
            } else {
               reg0 = -1;
            }
         } else if (opcode == 0x39) {
            if (W >= 0) {
               reg0 = (reg0 - W) & 0xffff;
            } else {
               reg0 = -1;
            }
         }
         switch (r0) {
         case 1:  X = reg0;                   break;
         case 2:  Y = reg0;                   break;
         case 3:  U = reg0;                   break;
         case 4:  S = reg0;                   break;
         default: unpack(reg0, &ACCA, &ACCB); break;
         }
      }

      // Update final value of R1
      if (reg1 >= 0) {
         if (opcode == 0x38 || opcode == 0x3b) {
            if (W >= 0) {
               reg1 = (reg1 + W) & 0xffff;
            } else {
               reg1 = -1;
            }
         } else if (opcode == 0x39) {
            if (W >= 0) {
               reg1 = (reg1 - W) & 0xffff;
            } else {
               reg1 = -1;
            }
         }
         switch (r1) {
         case 1:  X = reg1;                   break;
         case 2:  Y = reg1;                   break;
         case 3:  U = reg1;                   break;
         case 4:  S = reg1;                   break;
         default: unpack(reg1, &ACCA, &ACCB); break;
         }
      }

      // Update the final value of W, which is always zero
      ACCE = 0;
      ACCF = 0;
   }

   return -1;
}

static int op_fn_TIM(operand_t operand, ea_t ea, sample_t *sample_q) {
   set_NZ(operand & sample_q[1].data);
   V = 0;
   return -1;
}

static int op_fn_TRAP(operand_t operand, ea_t ea, sample_t *sample_q) {
   if (sample_q[0].data == 0x10 || sample_q[0].data == 0x11) {
      interrupt_helper(sample_q, 4, 1, 0xfff0);
   } else {
      interrupt_helper(sample_q, 3, 1, 0xfff0);
   }
   return -1;
}

static int op_fn_TSTD(operand_t operand, ea_t ea, sample_t *sample_q) {
   int D = pack(ACCA, ACCB);
   set_NZ16(D);
   V = 0;
   return -1;
}

static int op_fn_TSTE(operand_t operand, ea_t ea, sample_t *sample_q) {
   set_NZ(ACCE);
   V = 0;
   return -1;
}

static int op_fn_TSTF(operand_t operand, ea_t ea, sample_t *sample_q) {
   set_NZ(ACCF);
   V = 0;
   return -1;
}

static int op_fn_TSTW(operand_t operand, ea_t ea, sample_t *sample_q) {
   int W = pack(ACCA, ACCB);
   set_NZ16(W);
   V = 0;
   return -1;
}

// ====================================================================
// Opcode Tables
// ====================================================================

static operation_t op_ABX   = { "ABX",   op_fn_ABX ,     REGOP , 0 };
static operation_t op_ADCA  = { "ADCA",  op_fn_ADCA,    READOP , 0 };
static operation_t op_ADCB  = { "ADCB",  op_fn_ADCB,    READOP , 0 };
static operation_t op_ADDA  = { "ADDA",  op_fn_ADDA,    READOP , 0 };
static operation_t op_ADDB  = { "ADDB",  op_fn_ADDB,    READOP , 0 };
static operation_t op_ADDD  = { "ADDD",  op_fn_ADDD,    READOP , 1 };
static operation_t op_ANDA  = { "ANDA",  op_fn_ANDA,    READOP , 0 };
static operation_t op_ANDB  = { "ANDB",  op_fn_ANDB,    READOP , 0 };
static operation_t op_ANDC  = { "ANDC",  op_fn_ANDC,     REGOP , 0 };
static operation_t op_ASL   = { "ASL",   op_fn_ASL ,     RMWOP , 0 };
static operation_t op_ASLA  = { "ASLA",  op_fn_ASLA,     REGOP , 0 };
static operation_t op_ASLB  = { "ASLB",  op_fn_ASLB,     REGOP , 0 };
static operation_t op_ASR   = { "ASR",   op_fn_ASR ,     RMWOP , 0 };
static operation_t op_ASRA  = { "ASRA",  op_fn_ASRA,     REGOP , 0 };
static operation_t op_ASRB  = { "ASRB",  op_fn_ASRB,     REGOP , 0 };
static operation_t op_BCC   = { "BCC",   op_fn_BCC ,  BRANCHOP , 0 };
static operation_t op_BEQ   = { "BEQ",   op_fn_BEQ ,  BRANCHOP , 0 };
static operation_t op_BGE   = { "BGE",   op_fn_BGE ,  BRANCHOP , 0 };
static operation_t op_BGT   = { "BGT",   op_fn_BGT ,  BRANCHOP , 0 };
static operation_t op_BHI   = { "BHI",   op_fn_BHI ,  BRANCHOP , 0 };
static operation_t op_BITA  = { "BITA",  op_fn_BITA,    READOP , 0 };
static operation_t op_BITB  = { "BITB",  op_fn_BITB,    READOP , 0 };
static operation_t op_BLE   = { "BLE",   op_fn_BLE ,  BRANCHOP , 0 };
static operation_t op_BLO   = { "BLO",   op_fn_BLO ,  BRANCHOP , 0 };
static operation_t op_BLS   = { "BLS",   op_fn_BLS ,  BRANCHOP , 0 };
static operation_t op_BLT   = { "BLT",   op_fn_BLT ,  BRANCHOP , 0 };
static operation_t op_BMI   = { "BMI",   op_fn_BMI ,  BRANCHOP , 0 };
static operation_t op_BNE   = { "BNE",   op_fn_BNE ,  BRANCHOP , 0 };
static operation_t op_BPL   = { "BPL",   op_fn_BPL ,  BRANCHOP , 0 };
static operation_t op_BRA   = { "BRA",   op_fn_BRA ,  BRANCHOP , 0 };
static operation_t op_BRN   = { "BRN",   op_fn_BRN ,  BRANCHOP , 0 };
static operation_t op_BSR   = { "BSR",   op_fn_BSR ,     JSROP , 1 };
static operation_t op_BVC   = { "BVC",   op_fn_BVC ,  BRANCHOP , 0 };
static operation_t op_BVS   = { "BVS",   op_fn_BVS ,  BRANCHOP , 0 };
static operation_t op_CLR   = { "CLR",   op_fn_CLR ,     RMWOP , 0 };
static operation_t op_CLRA  = { "CLRA",  op_fn_CLRA,     REGOP , 0 };
static operation_t op_CLRB  = { "CLRB",  op_fn_CLRB,     REGOP , 0 };
static operation_t op_CMPA  = { "CMPA",  op_fn_CMPA,    READOP , 0 };
static operation_t op_CMPB  = { "CMPB",  op_fn_CMPB,    READOP , 0 };
static operation_t op_CMPD  = { "CMPD",  op_fn_CMPD,    READOP , 1 };
static operation_t op_CMPS  = { "CMPS",  op_fn_CMPS,    READOP , 1 };
static operation_t op_CMPU  = { "CMPU",  op_fn_CMPU,    READOP , 1 };
static operation_t op_CMPX  = { "CMPX",  op_fn_CMPX,    READOP , 1 };
static operation_t op_CMPY  = { "CMPY",  op_fn_CMPY,    READOP , 1 };
static operation_t op_COM   = { "COM",   op_fn_COM ,     RMWOP , 0 };
static operation_t op_COMA  = { "COMA",  op_fn_COMA,     REGOP , 0 };
static operation_t op_COMB  = { "COMB",  op_fn_COMB,     REGOP , 0 };
static operation_t op_CWAI  = { "CWAI",  op_fn_CWAI,     OTHER , 0 };
static operation_t op_DAA   = { "DAA",   op_fn_DAA ,     REGOP , 0 };
static operation_t op_DEC   = { "DEC",   op_fn_DEC ,     RMWOP , 0 };
static operation_t op_DECA  = { "DECA",  op_fn_DECA,     REGOP , 0 };
static operation_t op_DECB  = { "DECB",  op_fn_DECB,     REGOP , 0 };
static operation_t op_EORA  = { "EORA",  op_fn_EORA,    READOP , 0 };
static operation_t op_EORB  = { "EORB",  op_fn_EORB,    READOP , 0 };
static operation_t op_EXG   = { "EXG",   op_fn_EXG ,     REGOP , 0 };
static operation_t op_INC   = { "INC",   op_fn_INC ,     RMWOP , 0 };
static operation_t op_INCA  = { "INCA",  op_fn_INCA,     REGOP , 0 };
static operation_t op_INCB  = { "INCB",  op_fn_INCB,     REGOP , 0 };
static operation_t op_JMP   = { "JMP",   op_fn_JMP ,     LEAOP , 0 };
static operation_t op_JSR   = { "JSR",   op_fn_JSR ,     JSROP , 1 };
static operation_t op_LBCC  = { "LBCC",  op_fn_BCC ,  BRANCHOP , 0 };
static operation_t op_LBEQ  = { "LBEQ",  op_fn_BEQ ,  BRANCHOP , 0 };
static operation_t op_LBGE  = { "LBGE",  op_fn_BGE ,  BRANCHOP , 0 };
static operation_t op_LBGT  = { "LBGT",  op_fn_BGT ,  BRANCHOP , 0 };
static operation_t op_LBHI  = { "LBHI",  op_fn_BHI ,  BRANCHOP , 0 };
static operation_t op_LBLE  = { "LBLE",  op_fn_BLE ,  BRANCHOP , 0 };
static operation_t op_LBLO  = { "LBLO",  op_fn_BLO ,  BRANCHOP , 0 };
static operation_t op_LBLS  = { "LBLS",  op_fn_BLS ,  BRANCHOP , 0 };
static operation_t op_LBLT  = { "LBLT",  op_fn_BLT ,  BRANCHOP , 0 };
static operation_t op_LBMI  = { "LBMI",  op_fn_BMI ,  BRANCHOP , 0 };
static operation_t op_LBNE  = { "LBNE",  op_fn_BNE ,  BRANCHOP , 0 };
static operation_t op_LBPL  = { "LBPL",  op_fn_BPL ,  BRANCHOP , 0 };
static operation_t op_LBRA  = { "LBRA",  op_fn_BRA ,  BRANCHOP , 0 };
static operation_t op_LBRN  = { "LBRN",  op_fn_BRN ,  BRANCHOP , 0 };
static operation_t op_LBSR  = { "LBSR",  op_fn_BSR ,     JSROP , 1 };
static operation_t op_LBVC  = { "LBVC",  op_fn_BVC ,  BRANCHOP , 0 };
static operation_t op_LBVS  = { "LBVS",  op_fn_BVS ,  BRANCHOP , 0 };
static operation_t op_LDA   = { "LDA",   op_fn_LDA ,    LOADOP , 0 };
static operation_t op_LDB   = { "LDB",   op_fn_LDB ,    LOADOP , 0 };
static operation_t op_LDD   = { "LDD",   op_fn_LDD ,    LOADOP , 1 };
static operation_t op_LDS   = { "LDS",   op_fn_LDS ,    LOADOP , 1 };
static operation_t op_LDU   = { "LDU",   op_fn_LDU ,    LOADOP , 1 };
static operation_t op_LDX   = { "LDX",   op_fn_LDX ,    LOADOP , 1 };
static operation_t op_LDY   = { "LDY",   op_fn_LDY ,    LOADOP , 1 };
static operation_t op_LEAS  = { "LEAS",  op_fn_LEAS,     LEAOP , 0 };
static operation_t op_LEAU  = { "LEAU",  op_fn_LEAU,     LEAOP , 0 };
static operation_t op_LEAX  = { "LEAX",  op_fn_LEAX,     LEAOP , 0 };
static operation_t op_LEAY  = { "LEAY",  op_fn_LEAY,     LEAOP , 0 };
static operation_t op_LSR   = { "LSR",   op_fn_LSR ,     RMWOP , 0 };
static operation_t op_LSRA  = { "LSRA",  op_fn_LSRA,     REGOP , 0 };
static operation_t op_LSRB  = { "LSRB",  op_fn_LSRB,     REGOP , 0 };
static operation_t op_MUL   = { "MUL",   op_fn_MUL ,     REGOP , 0 };
static operation_t op_NEG   = { "NEG",   op_fn_NEG ,     RMWOP , 0 };
static operation_t op_NEGA  = { "NEGA",  op_fn_NEGA,     REGOP , 0 };
static operation_t op_NEGB  = { "NEGB",  op_fn_NEGB,     REGOP , 0 };
static operation_t op_NOP   = { "NOP",   op_fn_NOP ,     REGOP , 0 };
static operation_t op_ORA   = { "ORA",   op_fn_ORA ,    READOP , 0 };
static operation_t op_ORB   = { "ORB",   op_fn_ORB ,    READOP , 0 };
static operation_t op_ORCC  = { "ORCC",  op_fn_ORCC,     REGOP , 0 };
static operation_t op_PSHS  = { "PSHS",  op_fn_PSHS,     OTHER , 0 };
static operation_t op_PSHU  = { "PSHU",  op_fn_PSHU,     OTHER , 0 };
static operation_t op_PULS  = { "PULS",  op_fn_PULS,     OTHER , 0 };
static operation_t op_PULU  = { "PULU",  op_fn_PULU,     OTHER , 0 };
static operation_t op_ROL   = { "ROL",   op_fn_ROL ,     RMWOP , 0 };
static operation_t op_ROLA  = { "ROLA",  op_fn_ROLA,     REGOP , 0 };
static operation_t op_ROLB  = { "ROLB",  op_fn_ROLB,     REGOP , 0 };
static operation_t op_ROR   = { "ROR",   op_fn_ROR ,     RMWOP , 0 };
static operation_t op_RORA  = { "RORA",  op_fn_RORA,     REGOP , 0 };
static operation_t op_RORB  = { "RORB",  op_fn_RORB,     REGOP , 0 };
static operation_t op_RTI   = { "RTI",   op_fn_RTI ,     OTHER , 0 };
static operation_t op_RTS   = { "RTS",   op_fn_RTS ,     OTHER , 0 };
static operation_t op_SBCA  = { "SBCA",  op_fn_SBCA,    READOP , 0 };
static operation_t op_SBCB  = { "SBCB",  op_fn_SBCB,    READOP , 0 };
static operation_t op_SEX   = { "SEX",   op_fn_SEX ,     REGOP , 0 };
static operation_t op_STA   = { "STA",   op_fn_STA ,   STOREOP , 0 };
static operation_t op_STB   = { "STB",   op_fn_STB ,   STOREOP , 0 };
static operation_t op_STD   = { "STD",   op_fn_STD ,   STOREOP , 1 };
static operation_t op_STS   = { "STS",   op_fn_STS ,   STOREOP , 1 };
static operation_t op_STU   = { "STU",   op_fn_STU ,   STOREOP , 1 };
static operation_t op_STX   = { "STX",   op_fn_STX ,   STOREOP , 1 };
static operation_t op_STY   = { "STY",   op_fn_STY ,   STOREOP , 1 };
static operation_t op_SUBA  = { "SUBA",  op_fn_SUBA,    READOP , 0 };
static operation_t op_SUBB  = { "SUBB",  op_fn_SUBB,    READOP , 0 };
static operation_t op_SUBD  = { "SUBD",  op_fn_SUBD,    READOP , 1 };
static operation_t op_SWI   = { "SWI",   op_fn_SWI ,     OTHER , 0 };
static operation_t op_SWI2  = { "SWI2",  op_fn_SWI2,     OTHER , 0 };
static operation_t op_SWI3  = { "SWI3",  op_fn_SWI3,     OTHER , 0 };
static operation_t op_SYNC  = { "SYNC",  op_fn_SYNC,     OTHER , 0 };
static operation_t op_TFR   = { "TFR",   op_fn_TFR ,     REGOP , 0 };
static operation_t op_TST   = { "TST",   op_fn_TST ,    READOP , 0 };
static operation_t op_TSTA  = { "TSTA",  op_fn_TSTA,     REGOP , 0 };
static operation_t op_TSTB  = { "TSTB",  op_fn_TSTB,     REGOP , 0 };
static operation_t op_UU    = { "???",   op_fn_UU,       OTHER , 0 };

// 6809 Undocumented

static operation_t op_XX    = { "???",   op_fn_XX,       OTHER , 0 };
static operation_t op_X18   = { "X18",   op_fn_X18,      REGOP , 0 };
static operation_t op_X8C7  = { "X8C7",  op_fn_X8C7,    READOP , 0 };
static operation_t op_XHCF  = { "XHCF",  op_fn_XHCF,    READOP , 0 };
static operation_t op_XNC   = { "XNC",   op_fn_XNC,     READOP , 0 };
static operation_t op_XSTX  = { "XSTX",  op_fn_XSTX,   STOREOP , 0 };
static operation_t op_XSTU  = { "XSTU",  op_fn_XSTU,   STOREOP , 0 };
static operation_t op_XRES  = { "XRES",  op_fn_XRES,     OTHER , 0 };

// 6309

static operation_t op_ADCD  = { "ADCD",  op_fn_ADCD,    READOP , 1 };
static operation_t op_ADCR  = { "ADCR",  op_fn_ADCR,     REGOP , 0 };
static operation_t op_ADDE  = { "ADDE",  op_fn_ADDE,    READOP , 0 };
static operation_t op_ADDF  = { "ADDF",  op_fn_ADDF,    READOP , 0 };
static operation_t op_ADDR  = { "ADDR",  op_fn_ADDR,     REGOP , 0 };
static operation_t op_ADDW  = { "ADDW",  op_fn_ADDW,    READOP , 1 };
static operation_t op_AIM   = { "AIM",   op_fn_AIM,      RMWOP , 0 };
static operation_t op_ANDD  = { "ANDD",  op_fn_ANDD,    READOP , 1 };
static operation_t op_ANDR  = { "ANDR",  op_fn_ANDR,     REGOP , 0 };
static operation_t op_ASLD  = { "ASLD",  op_fn_ASLD,     REGOP , 0 };
static operation_t op_ASRD  = { "ASRD",  op_fn_ASRD,     REGOP , 0 };
static operation_t op_BAND  = { "BAND",  op_fn_BAND,    READOP , 0 };
static operation_t op_BEOR  = { "BEOR",  op_fn_BEOR,    READOP , 0 };
static operation_t op_BIAND = { "BIAND", op_fn_BIAND,   READOP , 0 };
static operation_t op_BIEOR = { "BIEOR", op_fn_BIEOR,   READOP , 0 };
static operation_t op_BIOR  = { "BIOR",  op_fn_BIOR,    READOP , 0 };
static operation_t op_BITD  = { "BITD",  op_fn_BITD,    READOP , 1 };
static operation_t op_BITMD = { "BITMD", op_fn_BITMD,   READOP , 0 };
static operation_t op_BOR   = { "BOR",   op_fn_BOR,     READOP , 0 };
static operation_t op_CLRD  = { "CLRD",  op_fn_CLRD,     REGOP , 0 };
static operation_t op_CLRE  = { "CLRE",  op_fn_CLRE,     REGOP , 0 };
static operation_t op_CLRF  = { "CLRF",  op_fn_CLRF,     REGOP , 0 };
static operation_t op_CLRW  = { "CLRW",  op_fn_CLRW,     REGOP , 0 };
static operation_t op_CMPE  = { "CMPE",  op_fn_CMPE,    READOP , 0 };
static operation_t op_CMPF  = { "CMPF",  op_fn_CMPF,    READOP , 0 };
static operation_t op_CMPR  = { "CMPR",  op_fn_CMPR,     REGOP , 0 };
static operation_t op_CMPW  = { "CMPW",  op_fn_CMPW,    READOP , 1 };
static operation_t op_COMD  = { "COMD",  op_fn_COMD,     REGOP , 0 };
static operation_t op_COME  = { "COME",  op_fn_COME,     REGOP , 0 };
static operation_t op_COMF  = { "COMF",  op_fn_COMF,     REGOP , 0 };
static operation_t op_COMW  = { "COMW",  op_fn_COMW,     REGOP , 0 };
static operation_t op_DECD  = { "DECD",  op_fn_DECD,     REGOP , 0 };
static operation_t op_DECE  = { "DECE",  op_fn_DECE,     REGOP , 0 };
static operation_t op_DECF  = { "DECF",  op_fn_DECF,     REGOP , 0 };
static operation_t op_DECW  = { "DECW",  op_fn_DECW,     REGOP , 0 };
static operation_t op_DIVD  = { "DIVD",  op_fn_DIVD,    READOP , 0 };
static operation_t op_DIVQ  = { "DIVQ",  op_fn_DIVQ,    READOP , 1 };
static operation_t op_EIM   = { "EIM",   op_fn_EIM,      RMWOP , 0 };
static operation_t op_EORD  = { "EORD",  op_fn_EORD,    READOP , 1 };
static operation_t op_EORR  = { "EORR",  op_fn_EORR,     REGOP , 0 };
static operation_t op_INCD  = { "INCD",  op_fn_INCD,     REGOP , 0 };
static operation_t op_INCE  = { "INCE",  op_fn_INCE,     REGOP , 0 };
static operation_t op_INCF  = { "INCF",  op_fn_INCF,     REGOP , 0 };
static operation_t op_INCW  = { "INCW",  op_fn_INCW,     REGOP , 0 };
static operation_t op_LDBT  = { "LDBT",  op_fn_LDBT,    READOP , 0 };
static operation_t op_LDE   = { "LDE",   op_fn_LDE,     LOADOP , 0 };
static operation_t op_LDF   = { "LDF",   op_fn_LDF,     LOADOP , 0 };
static operation_t op_LDMD  = { "LDMD",  op_fn_LDMD,     REGOP , 0 };
static operation_t op_LDQ   = { "LDQ",   op_fn_LDQ,     LOADOP , 2 };
static operation_t op_LDW   = { "LDW",   op_fn_LDW,     LOADOP , 1 };
static operation_t op_LSRD  = { "LSRD",  op_fn_LSRD,     REGOP , 0 };
static operation_t op_LSRW  = { "LSRW",  op_fn_LSRW,     REGOP , 0 };
static operation_t op_MULD  = { "MULD",  op_fn_MULD,    READOP , 1 };
static operation_t op_NEGD  = { "NEGD",  op_fn_NEGD,     REGOP , 0 };
static operation_t op_OIM   = { "OIM",   op_fn_OIM,      RMWOP , 0 };
static operation_t op_ORD   = { "ORD",   op_fn_ORD,     READOP , 1 };
static operation_t op_ORR   = { "ORR",   op_fn_ORR,      REGOP , 0 };
static operation_t op_PSHSW = { "PSHSW", op_fn_PSHSW,    OTHER , 0 };
static operation_t op_PSHUW = { "PSHUW", op_fn_PSHUW,    OTHER , 0 };
static operation_t op_PULSW = { "PULSW", op_fn_PULSW,    OTHER , 0 };
static operation_t op_PULUW = { "PULUW", op_fn_PULUW,    OTHER , 0 };
static operation_t op_ROLD  = { "ROLD",  op_fn_ROLD,     REGOP , 0 };
static operation_t op_ROLW  = { "ROLW",  op_fn_ROLW,     REGOP , 0 };
static operation_t op_RORD  = { "RORD",  op_fn_RORD,     REGOP , 0 };
static operation_t op_RORW  = { "RORW",  op_fn_RORW,     REGOP , 0 };
static operation_t op_SBCD  = { "SBCD",  op_fn_SBCD,    READOP , 1 };
static operation_t op_SBCR  = { "SBCR",  op_fn_SBCR,     REGOP , 0 };
static operation_t op_SEXW  = { "SEXW",  op_fn_SEXW,     REGOP , 0 };
static operation_t op_STBT  = { "STBT",  op_fn_STBT,     RMWOP , 0 };
static operation_t op_STE   = { "STE",   op_fn_STE,    STOREOP , 0 };
static operation_t op_STF   = { "STF",   op_fn_STF,    STOREOP , 0 };
static operation_t op_STQ   = { "STQ",   op_fn_STQ,    STOREOP , 2 };
static operation_t op_STW   = { "STW",   op_fn_STW,    STOREOP , 1 };
static operation_t op_SUBE  = { "SUBE",  op_fn_SUBE,    READOP , 0 };
static operation_t op_SUBF  = { "SUBF",  op_fn_SUBF,    READOP , 0 };
static operation_t op_SUBR  = { "SUBR",  op_fn_SUBR,     REGOP , 0 };
static operation_t op_SUBW  = { "SUBW",  op_fn_SUBW,    READOP , 1 };
static operation_t op_TFM   = { "TFM",   op_fn_TFM,      OTHER , 0 };
static operation_t op_TIM   = { "TIM",   op_fn_TIM,     READOP , 0 };
static operation_t op_TRAP  = { "???",   op_fn_TRAP,     OTHER , 0 };
static operation_t op_TSTD  = { "TSTD",  op_fn_TSTD,    READOP , 1 };
static operation_t op_TSTE  = { "TSTE",  op_fn_TSTE,    READOP , 0 };
static operation_t op_TSTF  = { "TSTF",  op_fn_TSTF,    READOP , 0 };
static operation_t op_TSTW  = { "TSTW",  op_fn_TSTW,    READOP , 1 };

// ====================================================================
// 6809 Opcode Tables
// ====================================================================

static opcode_t instr_table_6809[] = {

   /* 00 */    { &op_NEG  , DIRECT       , 0, 6 },
   /* 01 */    { &op_NEG  , DIRECT       , 1, 6 },
   /* 02 */    { &op_XNC  , DIRECT       , 1, 6 },
   /* 03 */    { &op_COM  , DIRECT       , 0, 6 },
   /* 04 */    { &op_LSR  , DIRECT       , 0, 6 },
   /* 05 */    { &op_LSR  , DIRECT       , 1, 6 },
   /* 06 */    { &op_ROR  , DIRECT       , 0, 6 },
   /* 07 */    { &op_ASR  , DIRECT       , 0, 6 },
   /* 08 */    { &op_ASL  , DIRECT       , 0, 6 },
   /* 09 */    { &op_ROL  , DIRECT       , 0, 6 },
   /* 0A */    { &op_DEC  , DIRECT       , 0, 6 },
   /* 0B */    { &op_DEC  , DIRECT       , 1, 6 },
   /* 0C */    { &op_INC  , DIRECT       , 0, 6 },
   /* 0D */    { &op_TST  , DIRECT       , 0, 6 },
   /* 0E */    { &op_JMP  , DIRECT       , 0, 3 },
   /* 0F */    { &op_CLR  , DIRECT       , 0, 6 },
   /* 10 */    { &op_UU   , INHERENT     , 0, 1 },
   /* 11 */    { &op_UU   , INHERENT     , 0, 1 },
   /* 12 */    { &op_NOP  , INHERENT     , 0, 2 },
   /* 13 */    { &op_SYNC , INHERENT     , 0, 4 },
   /* 14 */    { &op_XHCF , INHERENT     , 1, 1 },
   /* 15 */    { &op_XHCF , INHERENT     , 1, 1 },
   /* 16 */    { &op_LBRA , RELATIVE_16  , 0, 5 },
   /* 17 */    { &op_LBSR , RELATIVE_16  , 0, 9 },
   /* 18 */    { &op_X18  , INHERENT     , 1, 3 },
   /* 19 */    { &op_DAA  , INHERENT     , 0, 2 },
   /* 1A */    { &op_ORCC , REGISTER     , 0, 3 },
   /* 1B */    { &op_NOP  , INHERENT     , 1, 2 },
   /* 1C */    { &op_ANDC , REGISTER     , 0, 3 },
   /* 1D */    { &op_SEX  , INHERENT     , 0, 2 },
   /* 1E */    { &op_EXG  , REGISTER     , 0, 8 },
   /* 1F */    { &op_TFR  , REGISTER     , 0, 6 },
   /* 20 */    { &op_BRA  , RELATIVE_8   , 0, 3 },
   /* 21 */    { &op_BRN  , RELATIVE_8   , 0, 3 },
   /* 22 */    { &op_BHI  , RELATIVE_8   , 0, 3 },
   /* 23 */    { &op_BLS  , RELATIVE_8   , 0, 3 },
   /* 24 */    { &op_BCC  , RELATIVE_8   , 0, 3 },
   /* 25 */    { &op_BLO  , RELATIVE_8   , 0, 3 },
   /* 26 */    { &op_BNE  , RELATIVE_8   , 0, 3 },
   /* 27 */    { &op_BEQ  , RELATIVE_8   , 0, 3 },
   /* 28 */    { &op_BVC  , RELATIVE_8   , 0, 3 },
   /* 29 */    { &op_BVS  , RELATIVE_8   , 0, 3 },
   /* 2A */    { &op_BPL  , RELATIVE_8   , 0, 3 },
   /* 2B */    { &op_BMI  , RELATIVE_8   , 0, 3 },
   /* 2C */    { &op_BGE  , RELATIVE_8   , 0, 3 },
   /* 2D */    { &op_BLT  , RELATIVE_8   , 0, 3 },
   /* 2E */    { &op_BGT  , RELATIVE_8   , 0, 3 },
   /* 2F */    { &op_BLE  , RELATIVE_8   , 0, 3 },
   /* 30 */    { &op_LEAX , INDEXED      , 0, 4 },
   /* 31 */    { &op_LEAY , INDEXED      , 0, 4 },
   /* 32 */    { &op_LEAS , INDEXED      , 0, 4 },
   /* 33 */    { &op_LEAU , INDEXED      , 0, 4 },
   /* 34 */    { &op_PSHS , REGISTER     , 0, 5 },
   /* 35 */    { &op_PULS , REGISTER     , 0, 5 },
   /* 36 */    { &op_PSHU , REGISTER     , 0, 5 },
   /* 37 */    { &op_PULU , REGISTER     , 0, 5 },
   /* 38 */    { &op_ANDC , REGISTER     , 1, 4 }, // 4 cycle version of ANCDD
   /* 39 */    { &op_RTS  , INHERENT     , 0, 5 },
   /* 3A */    { &op_ABX  , INHERENT     , 0, 3 },
   /* 3B */    { &op_RTI  , INHERENT     , 0, 6 },
   /* 3C */    { &op_CWAI , IMMEDIATE_8  , 0,20 },
   /* 3D */    { &op_MUL  , INHERENT     , 0,11 },
   /* 3E */    { &op_XRES , INHERENT     , 1,19 },
   /* 3F */    { &op_SWI  , INHERENT     , 0,19 },
   /* 40 */    { &op_NEGA , INHERENT     , 0, 2 },
   /* 41 */    { &op_NEGA , INHERENT     , 1, 2 },
   /* 42 */    { &op_COMA , INHERENT     , 1, 2 },
   /* 43 */    { &op_COMA , INHERENT     , 0, 2 },
   /* 44 */    { &op_LSRA , INHERENT     , 0, 2 },
   /* 45 */    { &op_LSRA , INHERENT     , 1, 2 },
   /* 46 */    { &op_RORA , INHERENT     , 0, 2 },
   /* 47 */    { &op_ASRA , INHERENT     , 0, 2 },
   /* 48 */    { &op_ASLA , INHERENT     , 0, 2 },
   /* 49 */    { &op_ROLA , INHERENT     , 0, 2 },
   /* 4A */    { &op_DECA , INHERENT     , 0, 2 },
   /* 4B */    { &op_DECA , INHERENT     , 1, 2 },
   /* 4C */    { &op_INCA , INHERENT     , 0, 2 },
   /* 4D */    { &op_TSTA , INHERENT     , 0, 2 },
   /* 4E */    { &op_CLRA , INHERENT     , 1, 2 },
   /* 4F */    { &op_CLRA , INHERENT     , 0, 2 },
   /* 50 */    { &op_NEGB , INHERENT     , 0, 2 },
   /* 51 */    { &op_NEGB , INHERENT     , 1, 2 },
   /* 52 */    { &op_COMB , INHERENT     , 1, 2 },
   /* 53 */    { &op_COMB , INHERENT     , 0, 2 },
   /* 54 */    { &op_LSRB , INHERENT     , 0, 2 },
   /* 55 */    { &op_LSRB , INHERENT     , 1, 2 },
   /* 56 */    { &op_RORB , INHERENT     , 0, 2 },
   /* 57 */    { &op_ASRB , INHERENT     , 0, 2 },
   /* 58 */    { &op_ASLB , INHERENT     , 0, 2 },
   /* 59 */    { &op_ROLB , INHERENT     , 0, 2 },
   /* 5A */    { &op_DECB , INHERENT     , 0, 2 },
   /* 5B */    { &op_DECB , INHERENT     , 1, 2 },
   /* 5C */    { &op_INCB , INHERENT     , 0, 2 },
   /* 5D */    { &op_TSTB , INHERENT     , 0, 2 },
   /* 5E */    { &op_CLRB , INHERENT     , 1, 2 },
   /* 5F */    { &op_CLRB , INHERENT     , 0, 2 },
   /* 60 */    { &op_NEG  , INDEXED      , 0, 6 },
   /* 61 */    { &op_NEG  , INDEXED      , 1, 6 },
   /* 62 */    { &op_COM  , INDEXED      , 1, 6 },
   /* 63 */    { &op_COM  , INDEXED      , 0, 6 },
   /* 64 */    { &op_LSR  , INDEXED      , 0, 6 },
   /* 65 */    { &op_LSR  , INDEXED      , 1, 6 },
   /* 66 */    { &op_ROR  , INDEXED      , 0, 6 },
   /* 67 */    { &op_ASR  , INDEXED      , 0, 6 },
   /* 68 */    { &op_ASL  , INDEXED      , 0, 6 },
   /* 69 */    { &op_ROL  , INDEXED      , 0, 6 },
   /* 6A */    { &op_DEC  , INDEXED      , 0, 6 },
   /* 6B */    { &op_DEC  , INDEXED      , 1, 6 },
   /* 6C */    { &op_INC  , INDEXED      , 0, 6 },
   /* 6D */    { &op_TST  , INDEXED      , 0, 6 },
   /* 6E */    { &op_JMP  , INDEXED      , 0, 3 },
   /* 6F */    { &op_CLR  , INDEXED      , 0, 6 },
   /* 70 */    { &op_NEG  , EXTENDED     , 0, 7 },
   /* 71 */    { &op_NEG  , EXTENDED     , 1, 7 },
   /* 72 */    { &op_COM  , EXTENDED     , 1, 7 },
   /* 73 */    { &op_COM  , EXTENDED     , 0, 7 },
   /* 74 */    { &op_LSR  , EXTENDED     , 0, 7 },
   /* 75 */    { &op_LSR  , EXTENDED     , 1, 7 },
   /* 76 */    { &op_ROR  , EXTENDED     , 0, 7 },
   /* 77 */    { &op_ASR  , EXTENDED     , 0, 7 },
   /* 78 */    { &op_ASL  , EXTENDED     , 0, 7 },
   /* 79 */    { &op_ROL  , EXTENDED     , 0, 7 },
   /* 7A */    { &op_DEC  , EXTENDED     , 0, 7 },
   /* 7B */    { &op_DEC  , EXTENDED     , 1, 7 },
   /* 7C */    { &op_INC  , EXTENDED     , 0, 7 },
   /* 7D */    { &op_TST  , EXTENDED     , 0, 7 },
   /* 7E */    { &op_JMP  , EXTENDED     , 0, 4 },
   /* 7F */    { &op_CLR  , EXTENDED     , 0, 7 },
   /* 80 */    { &op_SUBA , IMMEDIATE_8  , 0, 2 },
   /* 81 */    { &op_CMPA , IMMEDIATE_8  , 0, 2 },
   /* 82 */    { &op_SBCA , IMMEDIATE_8  , 0, 2 },
   /* 83 */    { &op_SUBD , IMMEDIATE_16 , 0, 4 },
   /* 84 */    { &op_ANDA , IMMEDIATE_8  , 0, 2 },
   /* 85 */    { &op_BITA , IMMEDIATE_8  , 0, 2 },
   /* 86 */    { &op_LDA  , IMMEDIATE_8  , 0, 2 },
   /* 87 */    { &op_X8C7 , IMMEDIATE_8  , 1, 2 },
   /* 88 */    { &op_EORA , IMMEDIATE_8  , 0, 2 },
   /* 89 */    { &op_ADCA , IMMEDIATE_8  , 0, 2 },
   /* 8A */    { &op_ORA  , IMMEDIATE_8  , 0, 2 },
   /* 8B */    { &op_ADDA , IMMEDIATE_8  , 0, 2 },
   /* 8C */    { &op_CMPX , IMMEDIATE_16 , 0, 4 },
   /* 8D */    { &op_BSR  , RELATIVE_8   , 0, 7 },
   /* 8E */    { &op_LDX  , IMMEDIATE_16 , 0, 3 },
   /* 8F */    { &op_XSTX , IMMEDIATE_8  , 1, 3 },
   /* 90 */    { &op_SUBA , DIRECT       , 0, 4 },
   /* 91 */    { &op_CMPA , DIRECT       , 0, 4 },
   /* 92 */    { &op_SBCA , DIRECT       , 0, 4 },
   /* 93 */    { &op_SUBD , DIRECT       , 0, 6 },
   /* 94 */    { &op_ANDA , DIRECT       , 0, 4 },
   /* 95 */    { &op_BITA , DIRECT       , 0, 4 },
   /* 96 */    { &op_LDA  , DIRECT       , 0, 4 },
   /* 97 */    { &op_STA  , DIRECT       , 0, 4 },
   /* 98 */    { &op_EORA , DIRECT       , 0, 4 },
   /* 99 */    { &op_ADCA , DIRECT       , 0, 4 },
   /* 9A */    { &op_ORA  , DIRECT       , 0, 4 },
   /* 9B */    { &op_ADDA , DIRECT       , 0, 4 },
   /* 9C */    { &op_CMPX , DIRECT       , 0, 6 },
   /* 9D */    { &op_JSR  , DIRECT       , 0, 7 },
   /* 9E */    { &op_LDX  , DIRECT       , 0, 5 },
   /* 9F */    { &op_STX  , DIRECT       , 0, 5 },
   /* A0 */    { &op_SUBA , INDEXED      , 0, 4 },
   /* A1 */    { &op_CMPA , INDEXED      , 0, 4 },
   /* A2 */    { &op_SBCA , INDEXED      , 0, 4 },
   /* A3 */    { &op_SUBD , INDEXED      , 0, 6 },
   /* A4 */    { &op_ANDA , INDEXED      , 0, 4 },
   /* A5 */    { &op_BITA , INDEXED      , 0, 4 },
   /* A6 */    { &op_LDA  , INDEXED      , 0, 4 },
   /* A7 */    { &op_STA  , INDEXED      , 0, 4 },
   /* A8 */    { &op_EORA , INDEXED      , 0, 4 },
   /* A9 */    { &op_ADCA , INDEXED      , 0, 4 },
   /* AA */    { &op_ORA  , INDEXED      , 0, 4 },
   /* AB */    { &op_ADDA , INDEXED      , 0, 4 },
   /* AC */    { &op_CMPX , INDEXED      , 0, 6 },
   /* AD */    { &op_JSR  , INDEXED      , 0, 7 },
   /* AE */    { &op_LDX  , INDEXED      , 0, 5 },
   /* AF */    { &op_STX  , INDEXED      , 0, 5 },
   /* B0 */    { &op_SUBA , EXTENDED     , 0, 5 },
   /* B1 */    { &op_CMPA , EXTENDED     , 0, 5 },
   /* B2 */    { &op_SBCA , EXTENDED     , 0, 5 },
   /* B3 */    { &op_SUBD , EXTENDED     , 0, 7 },
   /* B4 */    { &op_ANDA , EXTENDED     , 0, 5 },
   /* B5 */    { &op_BITA , EXTENDED     , 0, 5 },
   /* B6 */    { &op_LDA  , EXTENDED     , 0, 5 },
   /* B7 */    { &op_STA  , EXTENDED     , 0, 5 },
   /* B8 */    { &op_EORA , EXTENDED     , 0, 5 },
   /* B9 */    { &op_ADCA , EXTENDED     , 0, 5 },
   /* BA */    { &op_ORA  , EXTENDED     , 0, 5 },
   /* BB */    { &op_ADDA , EXTENDED     , 0, 5 },
   /* BC */    { &op_CMPX , EXTENDED     , 0, 7 },
   /* BD */    { &op_JSR  , EXTENDED     , 0, 8 },
   /* BE */    { &op_LDX  , EXTENDED     , 0, 6 },
   /* BF */    { &op_STX  , EXTENDED     , 0, 6 },
   /* C0 */    { &op_SUBB , IMMEDIATE_8  , 0, 2 },
   /* C1 */    { &op_CMPB , IMMEDIATE_8  , 0, 2 },
   /* C2 */    { &op_SBCB , IMMEDIATE_8  , 0, 2 },
   /* C3 */    { &op_ADDD , IMMEDIATE_16 , 0, 4 },
   /* C4 */    { &op_ANDB , IMMEDIATE_8  , 0, 2 },
   /* C5 */    { &op_BITB , IMMEDIATE_8  , 0, 2 },
   /* C6 */    { &op_LDB  , IMMEDIATE_8  , 0, 2 },
   /* C7 */    { &op_X8C7 , IMMEDIATE_8  , 1, 2 },
   /* C8 */    { &op_EORB , IMMEDIATE_8  , 0, 2 },
   /* C9 */    { &op_ADCB , IMMEDIATE_8  , 0, 2 },
   /* CA */    { &op_ORB  , IMMEDIATE_8  , 0, 2 },
   /* CB */    { &op_ADDB , IMMEDIATE_8  , 0, 2 },
   /* CC */    { &op_LDD  , IMMEDIATE_16 , 0, 3 },
   /* CD */    { &op_XHCF , INHERENT     , 1, 1 },
   /* CE */    { &op_LDU  , IMMEDIATE_16 , 0, 3 },
   /* 8F */    { &op_XSTU , IMMEDIATE_8  , 1, 3 },
   /* D0 */    { &op_SUBB , DIRECT       , 0, 4 },
   /* D1 */    { &op_CMPB , DIRECT       , 0, 4 },
   /* D2 */    { &op_SBCB , DIRECT       , 0, 4 },
   /* D3 */    { &op_ADDD , DIRECT       , 0, 6 },
   /* D4 */    { &op_ANDB , DIRECT       , 0, 4 },
   /* D5 */    { &op_BITB , DIRECT       , 0, 4 },
   /* D6 */    { &op_LDB  , DIRECT       , 0, 4 },
   /* D7 */    { &op_STB  , DIRECT       , 0, 4 },
   /* D8 */    { &op_EORB , DIRECT       , 0, 4 },
   /* D9 */    { &op_ADCB , DIRECT       , 0, 4 },
   /* DA */    { &op_ORB  , DIRECT       , 0, 4 },
   /* DB */    { &op_ADDB , DIRECT       , 0, 4 },
   /* DC */    { &op_LDD  , DIRECT       , 0, 5 },
   /* DD */    { &op_STD  , DIRECT       , 0, 5 },
   /* DE */    { &op_LDU  , DIRECT       , 0, 5 },
   /* DF */    { &op_STU  , DIRECT       , 0, 5 },
   /* E0 */    { &op_SUBB , INDEXED      , 0, 4 },
   /* E1 */    { &op_CMPB , INDEXED      , 0, 4 },
   /* E2 */    { &op_SBCB , INDEXED      , 0, 4 },
   /* E3 */    { &op_ADDD , INDEXED      , 0, 6 },
   /* E4 */    { &op_ANDB , INDEXED      , 0, 4 },
   /* E5 */    { &op_BITB , INDEXED      , 0, 4 },
   /* E6 */    { &op_LDB  , INDEXED      , 0, 4 },
   /* E7 */    { &op_STB  , INDEXED      , 0, 4 },
   /* E8 */    { &op_EORB , INDEXED      , 0, 4 },
   /* E9 */    { &op_ADCB , INDEXED      , 0, 4 },
   /* EA */    { &op_ORB  , INDEXED      , 0, 4 },
   /* EB */    { &op_ADDB , INDEXED      , 0, 4 },
   /* EC */    { &op_LDD  , INDEXED      , 0, 5 },
   /* ED */    { &op_STD  , INDEXED      , 0, 5 },
   /* EE */    { &op_LDU  , INDEXED      , 0, 5 },
   /* EF */    { &op_STU  , INDEXED      , 0, 5 },
   /* F0 */    { &op_SUBB , EXTENDED     , 0, 5 },
   /* F1 */    { &op_CMPB , EXTENDED     , 0, 5 },
   /* F2 */    { &op_SBCB , EXTENDED     , 0, 5 },
   /* F3 */    { &op_ADDD , EXTENDED     , 0, 7 },
   /* F4 */    { &op_ANDB , EXTENDED     , 0, 5 },
   /* F5 */    { &op_BITB , EXTENDED     , 0, 5 },
   /* F6 */    { &op_LDB  , EXTENDED     , 0, 5 },
   /* F7 */    { &op_STB  , EXTENDED     , 0, 5 },
   /* F8 */    { &op_EORB , EXTENDED     , 0, 5 },
   /* F9 */    { &op_ADCB , EXTENDED     , 0, 5 },
   /* FA */    { &op_ORB  , EXTENDED     , 0, 5 },
   /* FB */    { &op_ADDB , EXTENDED     , 0, 5 },
   /* FC */    { &op_LDD  , EXTENDED     , 0, 6 },
   /* FD */    { &op_STD  , EXTENDED     , 0, 6 },
   /* FE */    { &op_LDU  , EXTENDED     , 0, 6 },
   /* FF */    { &op_STU  , EXTENDED     , 0, 6 },

   /* 1000 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1001 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1002 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1003 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1004 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1005 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1006 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1007 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1008 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1009 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 100A */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 100B */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 100C */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 100D */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 100E */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 100F */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1010 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1011 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1012 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1013 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1014 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1015 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1016 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1017 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1018 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1019 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 101A */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 101B */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 101C */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 101D */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 101E */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 101F */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1020 */  { &op_LBRA , RELATIVE_16  , 1, 5 },
   /* 1021 */  { &op_LBRN , RELATIVE_16  , 0, 5 },
   /* 1022 */  { &op_LBHI , RELATIVE_16  , 0, 5 },
   /* 1023 */  { &op_LBLS , RELATIVE_16  , 0, 5 },
   /* 1024 */  { &op_LBCC , RELATIVE_16  , 0, 5 },
   /* 1025 */  { &op_LBLO , RELATIVE_16  , 0, 5 },
   /* 1026 */  { &op_LBNE , RELATIVE_16  , 0, 5 },
   /* 1027 */  { &op_LBEQ , RELATIVE_16  , 0, 5 },
   /* 1028 */  { &op_LBVC , RELATIVE_16  , 0, 5 },
   /* 1029 */  { &op_LBVS , RELATIVE_16  , 0, 5 },
   /* 102A */  { &op_LBPL , RELATIVE_16  , 0, 5 },
   /* 102B */  { &op_LBMI , RELATIVE_16  , 0, 5 },
   /* 102C */  { &op_LBGE , RELATIVE_16  , 0, 5 },
   /* 102D */  { &op_LBLT , RELATIVE_16  , 0, 5 },
   /* 102E */  { &op_LBGT , RELATIVE_16  , 0, 5 },
   /* 102F */  { &op_LBLE , RELATIVE_16  , 0, 5 },
   /* 1030 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1031 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1032 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1033 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1034 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1035 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1036 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1037 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1038 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1039 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 103A */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 103B */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 103C */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 103D */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 103E */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 103F */  { &op_SWI2 , INHERENT     , 0,20 },
   /* 1040 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1041 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1042 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1043 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1044 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1045 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1046 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1047 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1048 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1049 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 104A */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 104B */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 104C */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 104D */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 104E */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 104F */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1050 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1051 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1052 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1053 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1054 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1055 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1056 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1057 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1058 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1059 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 105A */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 105B */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 105C */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 105D */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 105E */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 105F */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1060 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1061 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1062 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1063 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1064 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1065 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1066 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1067 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1068 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1069 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 106A */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 106B */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 106C */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 106D */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 106E */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 106F */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1070 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1071 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1072 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1073 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1074 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1075 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1076 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1077 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1078 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1079 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 107A */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 107B */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 107C */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 107D */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 107E */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 107F */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1080 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1081 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1082 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1083 */  { &op_CMPD , IMMEDIATE_16 , 0, 5 },
   /* 1084 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1085 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1086 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1087 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1088 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1089 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 108A */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 108B */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 108C */  { &op_CMPY , IMMEDIATE_16 , 0, 5 },
   /* 108D */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 108E */  { &op_LDY  , IMMEDIATE_16 , 0, 4 },
   /* 108F */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1090 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1091 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1092 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1093 */  { &op_CMPD , DIRECT       , 0, 7 },
   /* 1094 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1095 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1096 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1097 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1098 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1099 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 109A */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 109B */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 109C */  { &op_CMPY , DIRECT       , 0, 7 },
   /* 109D */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 109E */  { &op_LDY  , DIRECT       , 0, 6 },
   /* 109F */  { &op_STY  , DIRECT       , 0, 6 },
   /* 10A0 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10A1 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10A2 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10A3 */  { &op_CMPD , INDEXED      , 0, 7 },
   /* 10A4 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10A5 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10A6 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10A7 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10A8 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10A9 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10AA */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10AB */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10AC */  { &op_CMPY , INDEXED      , 0, 7 },
   /* 10AD */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10AE */  { &op_LDY  , INDEXED      , 0, 6 },
   /* 10AF */  { &op_STY  , INDEXED      , 0, 6 },
   /* 10B0 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10B1 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10B2 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10B3 */  { &op_CMPD , EXTENDED     , 0, 8 },
   /* 10B4 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10B5 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10B6 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10B7 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10B8 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10B9 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10BA */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10BB */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10BC */  { &op_CMPY , EXTENDED     , 0, 8 },
   /* 10BD */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10BE */  { &op_LDY  , EXTENDED     , 0, 7 },
   /* 10BF */  { &op_STY  , EXTENDED     , 0, 7 },
   /* 10C0 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10C1 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10C2 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10C3 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10C4 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10C5 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10C6 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10C7 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10C8 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10C9 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10CA */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10CB */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10CC */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10CD */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10CE */  { &op_LDS  , IMMEDIATE_16 , 0, 4 },
   /* 10CF */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10D0 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10D1 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10D2 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10D3 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10D4 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10D5 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10D6 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10D7 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10D8 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10D9 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10DA */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10DB */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10DC */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10DD */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10DE */  { &op_LDS  , DIRECT       , 0, 6 },
   /* 10DF */  { &op_STS  , DIRECT       , 0, 6 },
   /* 10E0 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10E1 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10E2 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10E3 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10E4 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10E5 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10E6 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10E7 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10E8 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10E9 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10EA */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10EB */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10EC */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10ED */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10EE */  { &op_LDS  , INDEXED      , 0, 6 },
   /* 10EF */  { &op_STS  , INDEXED      , 0, 6 },
   /* 10F0 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10F1 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10F2 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10F3 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10F4 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10F5 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10F6 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10F7 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10F8 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10F9 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10FA */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10FB */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10FC */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10FD */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10FE */  { &op_LDS  , EXTENDED     , 0, 7 },
   /* 10FF */  { &op_STS  , EXTENDED     , 0, 7 },

   /* 1100 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1101 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1102 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1103 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1104 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1105 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1106 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1107 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1108 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1109 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 110A */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 110B */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 110C */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 110D */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 110E */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 110F */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1110 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1111 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1112 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1113 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1114 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1115 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1116 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1117 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1118 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1119 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 111A */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 111B */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 111C */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 111D */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 111E */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 111F */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1120 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1121 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1122 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1123 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1124 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1125 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1126 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1127 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1128 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1129 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 112A */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 112B */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 112C */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 112D */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 112E */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 112F */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1130 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1131 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1132 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1133 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1134 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1135 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1136 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1137 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1138 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1139 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 113A */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 113B */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 113C */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 113D */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 113E */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 113F */  { &op_SWI3 , INHERENT     , 0,20 },
   /* 1140 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1141 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1142 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1143 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1144 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1145 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1146 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1147 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1148 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1149 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 114A */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 114B */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 114C */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 114D */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 114E */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 114F */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1150 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1151 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1152 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1153 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1154 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1155 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1156 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1157 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1158 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1159 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 115A */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 115B */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 115C */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 115D */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 115E */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 115F */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1160 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1161 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1162 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1163 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1164 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1165 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1166 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1167 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1168 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1169 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 116A */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 116B */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 116C */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 116D */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 116E */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 116F */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1170 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1171 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1172 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1173 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1174 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1175 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1176 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1177 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1178 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1179 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 117A */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 117B */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 117C */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 117D */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 117E */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 117F */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1180 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1181 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1182 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1183 */  { &op_CMPU , IMMEDIATE_16 , 0, 5 },
   /* 1184 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1185 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1186 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1187 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1188 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1189 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 118A */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 118B */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 118C */  { &op_CMPS , IMMEDIATE_16 , 0, 5 },
   /* 118D */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 118E */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 118F */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1190 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1191 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1192 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1193 */  { &op_CMPU , DIRECT       , 0, 7 },
   /* 1194 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1195 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1196 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1197 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1198 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 1199 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 119A */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 119B */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 119C */  { &op_CMPS , DIRECT       , 0, 7 },
   /* 119D */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 119E */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 119F */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11A0 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11A1 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11A2 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11A3 */  { &op_CMPU , INDEXED      , 0, 7 },
   /* 11A4 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11A5 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11A6 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11A7 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11A8 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11A9 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11AA */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11AB */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11AC */  { &op_CMPS , INDEXED      , 0, 7 },
   /* 11AD */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11AE */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11AF */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11B0 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11B1 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11B2 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11B3 */  { &op_CMPU , EXTENDED     , 0, 8 },
   /* 11B4 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11B5 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11B6 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11B7 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11B8 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11B9 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11BA */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11BB */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11BC */  { &op_CMPS , EXTENDED     , 0, 8 },
   /* 11BD */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11BE */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11BF */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11C0 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11C1 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11C2 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11C3 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11C4 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11C5 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11C6 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11C7 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11C8 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11C9 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11CA */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11CB */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11CC */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11CD */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11CE */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11CF */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11D0 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11D1 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11D2 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11D3 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11D4 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11D5 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11D6 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11D7 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11D8 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11D9 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11DA */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11DB */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11DC */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11DD */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11DE */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11DF */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11E0 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11E1 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11E2 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11E3 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11E4 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11E5 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11E6 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11E7 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11E8 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11E9 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11EA */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11EB */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11EC */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11ED */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11EE */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11EF */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11F0 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11F1 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11F2 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11F3 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11F4 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11F5 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11F6 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11F7 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11F8 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11F9 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11FA */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11FB */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11FC */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11FD */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11FE */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 11FF */  { &op_XX   , ILLEGAL      , 1, 1 }
};

// ====================================================================
// 6309 Opcode Tables
// ====================================================================

static opcode_t instr_table_6309[] = {
   /* 00 */    { &op_NEG  , DIRECT       , 0, 6, 5 },
   /* 01 */    { &op_OIM  , IMMEDIATE_8  , 0, 6, 6 },
   /* 02 */    { &op_AIM  , IMMEDIATE_8  , 0, 6, 6 },
   /* 03 */    { &op_COM  , DIRECT       , 0, 6, 5 },
   /* 04 */    { &op_LSR  , DIRECT       , 0, 6, 5 },
   /* 05 */    { &op_EIM  , IMMEDIATE_8  , 0, 6, 6 },
   /* 06 */    { &op_ROR  , DIRECT       , 0, 6, 5 },
   /* 07 */    { &op_ASR  , DIRECT       , 0, 6, 5 },
   /* 08 */    { &op_ASL  , DIRECT       , 0, 6, 5 },
   /* 09 */    { &op_ROL  , DIRECT       , 0, 6, 5 },
   /* 0A */    { &op_DEC  , DIRECT       , 0, 6, 5 },
   /* 0B */    { &op_TIM  , IMMEDIATE_8  , 0, 6, 6 },
   /* 0C */    { &op_INC  , DIRECT       , 0, 6, 5 },
   /* 0D */    { &op_TST  , DIRECT       , 0, 6, 4 },
   /* 0E */    { &op_JMP  , DIRECT       , 0, 3, 2 },
   /* 0F */    { &op_CLR  , DIRECT       , 0, 6, 5 },
   /* 10 */    { &op_UU   , INHERENT     , 0, 1, 1 },
   /* 11 */    { &op_UU   , INHERENT     , 0, 1, 1 },
   /* 12 */    { &op_NOP  , INHERENT     , 0, 2, 1 },
   /* 13 */    { &op_SYNC , INHERENT     , 0, 4, 4 },
   /* 14 */    { &op_SEXW , INHERENT     , 0, 2, 1 },
   /* 15 */    { &op_TRAP , ILLEGAL      , 1,19,21 },
   /* 16 */    { &op_LBRA , RELATIVE_16  , 0, 5, 4 },
   /* 17 */    { &op_LBSR , RELATIVE_16  , 0, 9, 7 },
   /* 18 */    { &op_TRAP , ILLEGAL      , 1,19,21 },
   /* 19 */    { &op_DAA  , INHERENT     , 0, 2, 1 },
   /* 1A */    { &op_ORCC , REGISTER     , 0, 3, 3 },
   /* 1B */    { &op_TRAP , ILLEGAL      , 1,19,21 },
   /* 1C */    { &op_ANDC , REGISTER     , 0, 3, 3 },
   /* 1D */    { &op_SEX  , INHERENT     , 0, 2, 1 },
   /* 1E */    { &op_EXG  , REGISTER     , 0, 8, 5 },
   /* 1F */    { &op_TFR  , REGISTER     , 0, 6, 4 },
   /* 20 */    { &op_BRA  , RELATIVE_8   , 0, 3, 3 },
   /* 21 */    { &op_BRN  , RELATIVE_8   , 0, 3, 3 },
   /* 22 */    { &op_BHI  , RELATIVE_8   , 0, 3, 3 },
   /* 23 */    { &op_BLS  , RELATIVE_8   , 0, 3, 3 },
   /* 24 */    { &op_BCC  , RELATIVE_8   , 0, 3, 3 },
   /* 25 */    { &op_BLO  , RELATIVE_8   , 0, 3, 3 },
   /* 26 */    { &op_BNE  , RELATIVE_8   , 0, 3, 3 },
   /* 27 */    { &op_BEQ  , RELATIVE_8   , 0, 3, 3 },
   /* 28 */    { &op_BVC  , RELATIVE_8   , 0, 3, 3 },
   /* 29 */    { &op_BVS  , RELATIVE_8   , 0, 3, 3 },
   /* 2A */    { &op_BPL  , RELATIVE_8   , 0, 3, 3 },
   /* 2B */    { &op_BMI  , RELATIVE_8   , 0, 3, 3 },
   /* 2C */    { &op_BGE  , RELATIVE_8   , 0, 3, 3 },
   /* 2D */    { &op_BLT  , RELATIVE_8   , 0, 3, 3 },
   /* 2E */    { &op_BGT  , RELATIVE_8   , 0, 3, 3 },
   /* 2F */    { &op_BLE  , RELATIVE_8   , 0, 3, 3 },
   /* 30 */    { &op_LEAX , INDEXED      , 0, 4, 4 },
   /* 31 */    { &op_LEAY , INDEXED      , 0, 4, 4 },
   /* 32 */    { &op_LEAS , INDEXED      , 0, 4, 4 },
   /* 33 */    { &op_LEAU , INDEXED      , 0, 4, 4 },
   /* 34 */    { &op_PSHS , REGISTER     , 0, 5, 4 },
   /* 35 */    { &op_PULS , REGISTER     , 0, 5, 4 },
   /* 36 */    { &op_PSHU , REGISTER     , 0, 5, 4 },
   /* 37 */    { &op_PULU , REGISTER     , 0, 5, 4 },
   /* 38 */    { &op_XX,    ILLEGAL      , 1,19,21 },
   /* 39 */    { &op_RTS  , INHERENT     , 0, 5, 4 },
   /* 3A */    { &op_ABX  , INHERENT     , 0, 3, 1 },
   /* 3B */    { &op_RTI  , INHERENT     , 0, 6, 6 },
   /* 3C */    { &op_CWAI , IMMEDIATE_8  , 0,20,22 },
   /* 3D */    { &op_MUL  , INHERENT     , 0,11,10 },
   /* 3E */    { &op_TRAP , ILLEGAL      , 1,19,21 },
   /* 3F */    { &op_SWI  , INHERENT     , 0,19,21 },
   /* 40 */    { &op_NEGA , INHERENT     , 0, 2, 1 },
   /* 41 */    { &op_TRAP , ILLEGAL      , 1,19,21 },
   /* 42 */    { &op_TRAP , ILLEGAL      , 1,19,21 },
   /* 43 */    { &op_COMA , INHERENT     , 0, 2, 1 },
   /* 44 */    { &op_LSRA , INHERENT     , 0, 2, 1 },
   /* 45 */    { &op_TRAP , ILLEGAL      , 1,19,21 },
   /* 46 */    { &op_RORA , INHERENT     , 0, 2, 1 },
   /* 47 */    { &op_ASRA , INHERENT     , 0, 2, 1 },
   /* 48 */    { &op_ASLA , INHERENT     , 0, 2, 1 },
   /* 49 */    { &op_ROLA , INHERENT     , 0, 2, 1 },
   /* 4A */    { &op_DECA , INHERENT     , 0, 2, 1 },
   /* 4B */    { &op_TRAP , ILLEGAL      , 1,19,21 },
   /* 4C */    { &op_INCA , INHERENT     , 0, 2, 1 },
   /* 4D */    { &op_TSTA , INHERENT     , 0, 2, 1 },
   /* 4E */    { &op_TRAP , ILLEGAL      , 1,19,21 },
   /* 4F */    { &op_CLRA , INHERENT     , 0, 2, 1 },
   /* 50 */    { &op_NEGB , INHERENT     , 0, 2, 1 },
   /* 51 */    { &op_TRAP , ILLEGAL      , 1,19,21 },
   /* 52 */    { &op_TRAP , ILLEGAL      , 1,19,21 },
   /* 53 */    { &op_COMB , INHERENT     , 0, 2, 1 },
   /* 54 */    { &op_LSRB , INHERENT     , 0, 2, 1 },
   /* 55 */    { &op_TRAP , ILLEGAL      , 1,19,21 },
   /* 56 */    { &op_RORB , INHERENT     , 0, 2, 1 },
   /* 57 */    { &op_ASRB , INHERENT     , 0, 2, 1 },
   /* 58 */    { &op_ASLB , INHERENT     , 0, 2, 1 },
   /* 59 */    { &op_ROLB , INHERENT     , 0, 2, 1 },
   /* 5A */    { &op_DECB , INHERENT     , 0, 2, 1 },
   /* 5B */    { &op_TRAP , ILLEGAL      , 1,19,21 },
   /* 5C */    { &op_INCB , INHERENT     , 0, 2, 1 },
   /* 5D */    { &op_TSTB , INHERENT     , 0, 2, 1 },
   /* 5E */    { &op_TRAP , ILLEGAL      , 1,19,21 },
   /* 5F */    { &op_CLRB , INHERENT     , 0, 2, 1 },
   /* 60 */    { &op_NEG  , INDEXED      , 0, 6, 6 },
   /* 61 */    { &op_OIM  , INDEXED      , 0, 6, 6 },
   /* 62 */    { &op_AIM  , INDEXED      , 0, 6, 6 },
   /* 63 */    { &op_COM  , INDEXED      , 0, 6, 6 },
   /* 64 */    { &op_LSR  , INDEXED      , 0, 6, 6 },
   /* 65 */    { &op_EIM  , INDEXED      , 0, 6, 6 },
   /* 66 */    { &op_ROR  , INDEXED      , 0, 6, 6 },
   /* 67 */    { &op_ASR  , INDEXED      , 0, 6, 6 },
   /* 68 */    { &op_ASL  , INDEXED      , 0, 6, 6 },
   /* 69 */    { &op_ROL  , INDEXED      , 0, 6, 6 },
   /* 6A */    { &op_DEC  , INDEXED      , 0, 6, 6 },
   /* 6B */    { &op_TIM  , INDEXED      , 0, 6, 6 },
   /* 6C */    { &op_INC  , INDEXED      , 0, 6, 6 },
   /* 6D */    { &op_TST  , INDEXED      , 0, 6, 5 },
   /* 6E */    { &op_JMP  , INDEXED      , 0, 3, 3 },
   /* 6F */    { &op_CLR  , INDEXED      , 0, 6, 6 },
   /* 70 */    { &op_NEG  , EXTENDED     , 0, 7, 6 },
   /* 71 */    { &op_OIM  , EXTENDED     , 0, 7, 7 },
   /* 72 */    { &op_AIM  , EXTENDED     , 0, 7, 7 },
   /* 73 */    { &op_COM  , EXTENDED     , 0, 7, 6 },
   /* 74 */    { &op_LSR  , EXTENDED     , 0, 7, 6 },
   /* 75 */    { &op_EIM  , EXTENDED     , 0, 7, 7 },
   /* 76 */    { &op_ROR  , EXTENDED     , 0, 7, 6 },
   /* 77 */    { &op_ASR  , EXTENDED     , 0, 7, 6 },
   /* 78 */    { &op_ASL  , EXTENDED     , 0, 7, 6 },
   /* 79 */    { &op_ROL  , EXTENDED     , 0, 7, 6 },
   /* 7A */    { &op_DEC  , EXTENDED     , 0, 7, 6 },
   /* 7B */    { &op_TIM  , EXTENDED     , 0, 7, 7 },
   /* 7C */    { &op_INC  , EXTENDED     , 0, 7, 6 },
   /* 7D */    { &op_TST  , EXTENDED     , 0, 7, 5 },
   /* 7E */    { &op_JMP  , EXTENDED     , 0, 4, 3 },
   /* 7F */    { &op_CLR  , EXTENDED     , 0, 7, 6 },
   /* 80 */    { &op_SUBA , IMMEDIATE_8  , 0, 2, 2 },
   /* 81 */    { &op_CMPA , IMMEDIATE_8  , 0, 2, 2 },
   /* 82 */    { &op_SBCA , IMMEDIATE_8  , 0, 2, 2 },
   /* 83 */    { &op_SUBD , IMMEDIATE_16 , 0, 4, 3 },
   /* 84 */    { &op_ANDA , IMMEDIATE_8  , 0, 2, 2 },
   /* 85 */    { &op_BITA , IMMEDIATE_8  , 0, 2, 2 },
   /* 86 */    { &op_LDA  , IMMEDIATE_8  , 0, 2, 2 },
   /* 87 */    { &op_TRAP , ILLEGAL      , 1,19,21 },
   /* 88 */    { &op_EORA , IMMEDIATE_8  , 0, 2, 2 },
   /* 89 */    { &op_ADCA , IMMEDIATE_8  , 0, 2, 2 },
   /* 8A */    { &op_ORA  , IMMEDIATE_8  , 0, 2, 2 },
   /* 8B */    { &op_ADDA , IMMEDIATE_8  , 0, 2, 2 },
   /* 8C */    { &op_CMPX , IMMEDIATE_16 , 0, 4, 3 },
   /* 8D */    { &op_BSR  , RELATIVE_8   , 0, 7, 6 },
   /* 8E */    { &op_LDX  , IMMEDIATE_16 , 0, 3, 3 },
   /* 8F */    { &op_TRAP , ILLEGAL      , 1,19,21 },
   /* 90 */    { &op_SUBA , DIRECT       , 0, 4, 3 },
   /* 91 */    { &op_CMPA , DIRECT       , 0, 4, 3 },
   /* 92 */    { &op_SBCA , DIRECT       , 0, 4, 3 },
   /* 93 */    { &op_SUBD , DIRECT       , 0, 6, 4 },
   /* 94 */    { &op_ANDA , DIRECT       , 0, 4, 3 },
   /* 95 */    { &op_BITA , DIRECT       , 0, 4, 3 },
   /* 96 */    { &op_LDA  , DIRECT       , 0, 4, 3 },
   /* 97 */    { &op_STA  , DIRECT       , 0, 4, 3 },
   /* 98 */    { &op_EORA , DIRECT       , 0, 4, 3 },
   /* 99 */    { &op_ADCA , DIRECT       , 0, 4, 3 },
   /* 9A */    { &op_ORA  , DIRECT       , 0, 4, 3 },
   /* 9B */    { &op_ADDA , DIRECT       , 0, 4, 3 },
   /* 9C */    { &op_CMPX , DIRECT       , 0, 6, 4 },
   /* 9D */    { &op_JSR  , DIRECT       , 0, 7, 6 },
   /* 9E */    { &op_LDX  , DIRECT       , 0, 5, 4 },
   /* 9F */    { &op_STX  , DIRECT       , 0, 5, 4 },
   /* A0 */    { &op_SUBA , INDEXED      , 0, 4, 4 },
   /* A1 */    { &op_CMPA , INDEXED      , 0, 4, 4 },
   /* A2 */    { &op_SBCA , INDEXED      , 0, 4, 4 },
   /* A3 */    { &op_SUBD , INDEXED      , 0, 6, 5 },
   /* A4 */    { &op_ANDA , INDEXED      , 0, 4, 4 },
   /* A5 */    { &op_BITA , INDEXED      , 0, 4, 4 },
   /* A6 */    { &op_LDA  , INDEXED      , 0, 4, 4 },
   /* A7 */    { &op_STA  , INDEXED      , 0, 4, 4 },
   /* A8 */    { &op_EORA , INDEXED      , 0, 4, 4 },
   /* A9 */    { &op_ADCA , INDEXED      , 0, 4, 4 },
   /* AA */    { &op_ORA  , INDEXED      , 0, 4, 4 },
   /* AB */    { &op_ADDA , INDEXED      , 0, 4, 4 },
   /* AC */    { &op_CMPX , INDEXED      , 0, 6, 5 },
   /* AD */    { &op_JSR  , INDEXED      , 0, 7, 6 },
   /* AE */    { &op_LDX  , INDEXED      , 0, 5, 5 },
   /* AF */    { &op_STX  , INDEXED      , 0, 5, 5 },
   /* B0 */    { &op_SUBA , EXTENDED     , 0, 5, 4 },
   /* B1 */    { &op_CMPA , EXTENDED     , 0, 5, 4 },
   /* B2 */    { &op_SBCA , EXTENDED     , 0, 5, 4 },
   /* B3 */    { &op_SUBD , EXTENDED     , 0, 7, 5 },
   /* B4 */    { &op_ANDA , EXTENDED     , 0, 5, 4 },
   /* B5 */    { &op_BITA , EXTENDED     , 0, 5, 4 },
   /* B6 */    { &op_LDA  , EXTENDED     , 0, 5, 4 },
   /* B7 */    { &op_STA  , EXTENDED     , 0, 5, 4 },
   /* B8 */    { &op_EORA , EXTENDED     , 0, 5, 4 },
   /* B9 */    { &op_ADCA , EXTENDED     , 0, 5, 4 },
   /* BA */    { &op_ORA  , EXTENDED     , 0, 5, 4 },
   /* BB */    { &op_ADDA , EXTENDED     , 0, 5, 4 },
   /* BC */    { &op_CMPX , EXTENDED     , 0, 7, 5 },
   /* BD */    { &op_JSR  , EXTENDED     , 0, 8, 7 },
   /* BE */    { &op_LDX  , EXTENDED     , 0, 6, 5 },
   /* BF */    { &op_STX  , EXTENDED     , 0, 6, 5 },
   /* C0 */    { &op_SUBB , IMMEDIATE_8  , 0, 2, 2 },
   /* C1 */    { &op_CMPB , IMMEDIATE_8  , 0, 2, 2 },
   /* C2 */    { &op_SBCB , IMMEDIATE_8  , 0, 2, 2 },
   /* C3 */    { &op_ADDD , IMMEDIATE_16 , 0, 4, 3 },
   /* C4 */    { &op_ANDB , IMMEDIATE_8  , 0, 2, 2 },
   /* C5 */    { &op_BITB , IMMEDIATE_8  , 0, 2, 2 },
   /* C6 */    { &op_LDB  , IMMEDIATE_8  , 0, 2, 2 },
   /* C7 */    { &op_TRAP , ILLEGAL      , 1,19,21 },
   /* C8 */    { &op_EORB , IMMEDIATE_8  , 0, 2, 2 },
   /* C9 */    { &op_ADCB , IMMEDIATE_8  , 0, 2, 2 },
   /* CA */    { &op_ORB  , IMMEDIATE_8  , 0, 2, 2 },
   /* CB */    { &op_ADDB , IMMEDIATE_8  , 0, 2, 2 },
   /* CC */    { &op_LDD  , IMMEDIATE_16 , 0, 3, 3 },
   /* CD */    { &op_LDQ  , IMMEDIATE_32 , 0, 5, 5 },
   /* CE */    { &op_LDU  , IMMEDIATE_16 , 0, 3, 3 },
   /* CF */    { &op_TRAP , ILLEGAL      , 1,19,21 },
   /* D0 */    { &op_SUBB , DIRECT       , 0, 4, 3 },
   /* D1 */    { &op_CMPB , DIRECT       , 0, 4, 3 },
   /* D2 */    { &op_SBCB , DIRECT       , 0, 4, 3 },
   /* D3 */    { &op_ADDD , DIRECT       , 0, 6, 4 },
   /* D4 */    { &op_ANDB , DIRECT       , 0, 4, 3 },
   /* D5 */    { &op_BITB , DIRECT       , 0, 4, 3 },
   /* D6 */    { &op_LDB  , DIRECT       , 0, 4, 3 },
   /* D7 */    { &op_STB  , DIRECT       , 0, 4, 3 },
   /* D8 */    { &op_EORB , DIRECT       , 0, 4, 3 },
   /* D9 */    { &op_ADCB , DIRECT       , 0, 4, 3 },
   /* DA */    { &op_ORB  , DIRECT       , 0, 4, 3 },
   /* DB */    { &op_ADDB , DIRECT       , 0, 4, 3 },
   /* DC */    { &op_LDD  , DIRECT       , 0, 5, 4 },
   /* DD */    { &op_STD  , DIRECT       , 0, 5, 4 },
   /* DE */    { &op_LDU  , DIRECT       , 0, 5, 4 },
   /* DF */    { &op_STU  , DIRECT       , 0, 5, 4 },
   /* E0 */    { &op_SUBB , INDEXED      , 0, 4, 4 },
   /* E1 */    { &op_CMPB , INDEXED      , 0, 4, 4 },
   /* E2 */    { &op_SBCB , INDEXED      , 0, 4, 4 },
   /* E3 */    { &op_ADDD , INDEXED      , 0, 6, 5 },
   /* E4 */    { &op_ANDB , INDEXED      , 0, 4, 4 },
   /* E5 */    { &op_BITB , INDEXED      , 0, 4, 4 },
   /* E6 */    { &op_LDB  , INDEXED      , 0, 4, 4 },
   /* E7 */    { &op_STB  , INDEXED      , 0, 4, 4 },
   /* E8 */    { &op_EORB , INDEXED      , 0, 4, 4 },
   /* E9 */    { &op_ADCB , INDEXED      , 0, 4, 4 },
   /* EA */    { &op_ORB  , INDEXED      , 0, 4, 4 },
   /* EB */    { &op_ADDB , INDEXED      , 0, 4, 4 },
   /* EC */    { &op_LDD  , INDEXED      , 0, 5, 5 },
   /* ED */    { &op_STD  , INDEXED      , 0, 5, 5 },
   /* EE */    { &op_LDU  , INDEXED      , 0, 5, 5 },
   /* EF */    { &op_STU  , INDEXED      , 0, 5, 5 },
   /* F0 */    { &op_SUBB , EXTENDED     , 0, 5, 4 },
   /* F1 */    { &op_CMPB , EXTENDED     , 0, 5, 4 },
   /* F2 */    { &op_SBCB , EXTENDED     , 0, 5, 4 },
   /* F3 */    { &op_ADDD , EXTENDED     , 0, 7, 5 },
   /* F4 */    { &op_ANDB , EXTENDED     , 0, 5, 4 },
   /* F5 */    { &op_BITB , EXTENDED     , 0, 5, 4 },
   /* F6 */    { &op_LDB  , EXTENDED     , 0, 5, 4 },
   /* F7 */    { &op_STB  , EXTENDED     , 0, 5, 4 },
   /* F8 */    { &op_EORB , EXTENDED     , 0, 5, 4 },
   /* F9 */    { &op_ADCB , EXTENDED     , 0, 5, 4 },
   /* FA */    { &op_ORB  , EXTENDED     , 0, 5, 4 },
   /* FB */    { &op_ADDB , EXTENDED     , 0, 5, 4 },
   /* FC */    { &op_LDD  , EXTENDED     , 0, 6, 5 },
   /* FD */    { &op_STD  , EXTENDED     , 0, 6, 5 },
   /* FE */    { &op_LDU  , EXTENDED     , 0, 6, 5 },
   /* FF */    { &op_STU  , EXTENDED     , 0, 6, 5 },

   /* 1000 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 1001 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 1002 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 1003 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 1004 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 1005 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 1006 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 1007 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 1008 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 1009 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 100A */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 100B */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 100C */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 100D */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 100E */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 100F */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 1010 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 1011 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 1012 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 1013 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 1014 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 1015 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 1016 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 1017 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 1018 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 1019 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 101A */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 101B */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 101C */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 101D */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 101E */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 101F */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 1020 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 1021 */  { &op_LBRN , RELATIVE_16  , 0, 5, 5 },
   /* 1022 */  { &op_LBHI , RELATIVE_16  , 0, 5, 5 },
   /* 1023 */  { &op_LBLS , RELATIVE_16  , 0, 5, 5 },
   /* 1024 */  { &op_LBCC , RELATIVE_16  , 0, 5, 5 },
   /* 1025 */  { &op_LBLO , RELATIVE_16  , 0, 5, 5 },
   /* 1026 */  { &op_LBNE , RELATIVE_16  , 0, 5, 5 },
   /* 1027 */  { &op_LBEQ , RELATIVE_16  , 0, 5, 5 },
   /* 1028 */  { &op_LBVC , RELATIVE_16  , 0, 5, 5 },
   /* 1029 */  { &op_LBVS , RELATIVE_16  , 0, 5, 5 },
   /* 102A */  { &op_LBPL , RELATIVE_16  , 0, 5, 5 },
   /* 102B */  { &op_LBMI , RELATIVE_16  , 0, 5, 5 },
   /* 102C */  { &op_LBGE , RELATIVE_16  , 0, 5, 5 },
   /* 102D */  { &op_LBLT , RELATIVE_16  , 0, 5, 5 },
   /* 102E */  { &op_LBGT , RELATIVE_16  , 0, 5, 5 },
   /* 102F */  { &op_LBLE , RELATIVE_16  , 0, 5, 5 },
   /* 1030 */  { &op_ADDR , REGISTER     , 0, 4, 4 },
   /* 1031 */  { &op_ADCR , REGISTER     , 0, 4, 4 },
   /* 1032 */  { &op_SUBR , REGISTER     , 0, 4, 4 },
   /* 1033 */  { &op_SBCR , REGISTER     , 0, 4, 4 },
   /* 1034 */  { &op_ANDR , REGISTER     , 0, 4, 4 },
   /* 1035 */  { &op_ORR  , REGISTER     , 0, 4, 4 },
   /* 1036 */  { &op_EORR , REGISTER     , 0, 4, 4 },
   /* 1037 */  { &op_CMPR , REGISTER     , 0, 4, 4 },
   /* 1038 */  { &op_PSHSW, INHERENT     , 0, 6, 6 },
   /* 1039 */  { &op_PULSW, INHERENT     , 0, 6, 6 },
   /* 103A */  { &op_PSHUW, INHERENT     , 0, 6, 6 },
   /* 103B */  { &op_PULUW, INHERENT     , 0, 6, 6 },
   /* 103C */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 103D */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 103E */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 103F */  { &op_SWI2 , INHERENT     , 0,20,22 },
   /* 1040 */  { &op_NEGD , INHERENT     , 0, 2, 1 },
   /* 1041 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 1042 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 1043 */  { &op_COMD , INHERENT     , 0, 2, 1 },
   /* 1044 */  { &op_LSRD , INHERENT     , 0, 2, 1 },
   /* 1045 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 1046 */  { &op_RORD , INHERENT     , 0, 2, 1 },
   /* 1047 */  { &op_ASRD , INHERENT     , 0, 2, 1 },
   /* 1048 */  { &op_ASLD , INHERENT     , 0, 2, 1 },
   /* 1049 */  { &op_ROLD , INHERENT     , 0, 2, 1 },
   /* 104A */  { &op_DECD , INHERENT     , 0, 2, 1 },
   /* 104B */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 104C */  { &op_INCD , INHERENT     , 0, 2, 1 },
   /* 104D */  { &op_TSTD , INHERENT     , 0, 2, 1 },
   /* 104E */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 104F */  { &op_CLRD , INHERENT     , 0, 2, 1 },
   /* 1050 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 1051 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 1052 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 1053 */  { &op_COMW , INHERENT     , 0, 3, 2 },
   /* 1054 */  { &op_LSRW , INHERENT     , 0, 3, 2 },
   /* 1055 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 1056 */  { &op_RORW , INHERENT     , 0, 3, 2 },
   /* 1057 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 1058 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 1059 */  { &op_ROLW , INHERENT     , 0, 3, 2 },
   /* 105A */  { &op_DECW , INHERENT     , 0, 3, 2 },
   /* 105B */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 105C */  { &op_INCW , INHERENT     , 0, 3, 2 },
   /* 105D */  { &op_TSTW , INHERENT     , 0, 3, 2 },
   /* 105E */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 105F */  { &op_CLRW , INHERENT     , 0, 3, 2 },
   /* 1060 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 1061 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 1062 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 1063 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 1064 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 1065 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 1066 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 1067 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 1068 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 1069 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 106A */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 106B */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 106C */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 106D */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 106E */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 106F */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 1070 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 1071 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 1072 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 1073 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 1074 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 1075 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 1076 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 1077 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 1078 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 1079 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 107A */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 107B */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 107C */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 107D */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 107E */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 107F */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 1080 */  { &op_SUBW , IMMEDIATE_16 , 0, 5, 4 },
   /* 1081 */  { &op_CMPW , IMMEDIATE_16 , 0, 5, 4 },
   /* 1082 */  { &op_SBCD , IMMEDIATE_16 , 0, 5, 4 },
   /* 1083 */  { &op_CMPD , IMMEDIATE_16 , 0, 5, 4 },
   /* 1084 */  { &op_ANDD , IMMEDIATE_16 , 0, 5, 4 },
   /* 1085 */  { &op_BITD , IMMEDIATE_16 , 0, 5, 4 },
   /* 1086 */  { &op_LDW  , IMMEDIATE_16 , 0, 4, 4 },
   /* 1087 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 1088 */  { &op_EORD , IMMEDIATE_16 , 0, 5, 4 },
   /* 1089 */  { &op_ADCD , IMMEDIATE_16 , 0, 5, 4 },
   /* 108A */  { &op_ORD  , IMMEDIATE_16 , 0, 5, 4 },
   /* 108B */  { &op_ADDW , IMMEDIATE_16 , 0, 5, 4 },
   /* 108C */  { &op_CMPY , IMMEDIATE_16 , 0, 5, 4 },
   /* 108D */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 108E */  { &op_LDY  , IMMEDIATE_16 , 0, 4, 4 },
   /* 108F */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 1090 */  { &op_SUBW , DIRECT       , 0, 7, 5 },
   /* 1091 */  { &op_CMPW , DIRECT       , 0, 7, 5 },
   /* 1092 */  { &op_SBCD , DIRECT       , 0, 7, 5 },
   /* 1093 */  { &op_CMPD , DIRECT       , 0, 7, 5 },
   /* 1094 */  { &op_ANDD , DIRECT       , 0, 7, 5 },
   /* 1095 */  { &op_BITD , DIRECT       , 0, 7, 5 },
   /* 1096 */  { &op_LDW  , DIRECT       , 0, 6, 5 },
   /* 1097 */  { &op_STW  , DIRECT       , 0, 6, 5 },
   /* 1098 */  { &op_EORD , DIRECT       , 0, 7, 5 },
   /* 1099 */  { &op_ADCD , DIRECT       , 0, 7, 5 },
   /* 109A */  { &op_ORD  , DIRECT       , 0, 7, 5 },
   /* 109B */  { &op_ADDW , DIRECT       , 0, 7, 5 },
   /* 109C */  { &op_CMPY , DIRECT       , 0, 7, 5 },
   /* 109D */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 109E */  { &op_LDY  , DIRECT       , 0, 6, 5 },
   /* 109F */  { &op_STY  , DIRECT       , 0, 6, 5 },
   /* 10A0 */  { &op_SUBW , INDEXED      , 0, 7, 6 },
   /* 10A1 */  { &op_CMPW , INDEXED      , 0, 7, 6 },
   /* 10A2 */  { &op_SBCD , INDEXED      , 0, 7, 6 },
   /* 10A3 */  { &op_CMPD , INDEXED      , 0, 7, 6 },
   /* 10A4 */  { &op_ANDD , INDEXED      , 0, 7, 6 },
   /* 10A5 */  { &op_BITD , INDEXED      , 0, 7, 6 },
   /* 10A6 */  { &op_LDW  , INDEXED      , 0, 6, 6 },
   /* 10A7 */  { &op_STW  , INDEXED      , 0, 6, 6 },
   /* 10A8 */  { &op_EORD , INDEXED      , 0, 7, 6 },
   /* 10A9 */  { &op_ADCD , INDEXED      , 0, 7, 6 },
   /* 10AA */  { &op_ORD  , INDEXED      , 0, 7, 6 },
   /* 10AB */  { &op_ADDW , INDEXED      , 0, 7, 6 },
   /* 10AC */  { &op_CMPY , INDEXED      , 0, 7, 6 },
   /* 10AD */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 10AE */  { &op_LDY  , INDEXED      , 0, 6, 5 },
   /* 10AF */  { &op_STY  , INDEXED      , 0, 6, 5 },
   /* 10B0 */  { &op_SUBW , EXTENDED     , 0, 8, 6 },
   /* 10B1 */  { &op_CMPW , EXTENDED     , 0, 8, 6 },
   /* 10B2 */  { &op_SBCD , EXTENDED     , 0, 8, 6 },
   /* 10B3 */  { &op_CMPD , EXTENDED     , 0, 8, 6 },
   /* 10B4 */  { &op_ANDD , EXTENDED     , 0, 8, 6 },
   /* 10B5 */  { &op_BITD , EXTENDED     , 0, 8, 6 },
   /* 10B6 */  { &op_LDW  , EXTENDED     , 0, 7, 6 },
   /* 10B7 */  { &op_STW  , EXTENDED     , 0, 7, 6 },
   /* 10B8 */  { &op_EORD , EXTENDED     , 0, 8, 6 },
   /* 10B9 */  { &op_ADCD , EXTENDED     , 0, 8, 6 },
   /* 10BA */  { &op_ORD  , EXTENDED     , 0, 8, 6 },
   /* 10BB */  { &op_ADDW , EXTENDED     , 0, 8, 6 },
   /* 10BC */  { &op_CMPY , EXTENDED     , 0, 8, 6 },
   /* 10BD */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 10BE */  { &op_LDY  , EXTENDED     , 0, 7, 6 },
   /* 10BF */  { &op_STY  , EXTENDED     , 0, 7, 6 },
   /* 10C0 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 10C1 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 10C2 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 10C3 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 10C4 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 10C5 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 10C6 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 10C7 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 10C8 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 10C9 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 10CA */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 10CB */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 10CC */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 10CD */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 10CE */  { &op_LDS  , IMMEDIATE_16 , 0, 4, 4 },
   /* 10CF */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 10D0 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 10D1 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 10D2 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 10D3 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 10D4 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 10D5 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 10D6 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 10D7 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 10D8 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 10D9 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 10DA */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 10DB */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 10DC */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 10DD */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 10DE */  { &op_LDS  , DIRECT       , 0, 6, 5 },
   /* 10DF */  { &op_STS  , DIRECT       , 0, 6, 5 },
   /* 10E0 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 10E1 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 10E2 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 10E3 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 10E4 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 10E5 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 10E6 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 10E7 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 10E8 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 10E9 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 10EA */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 10EB */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 10EC */  { &op_LDQ  , INDEXED      , 0, 8, 6 },
   /* 10ED */  { &op_STQ  , INDEXED      , 0, 8, 6 },
   /* 10EE */  { &op_LDS  , INDEXED      , 0, 6, 6 },
   /* 10EF */  { &op_STS  , INDEXED      , 0, 6, 6 },
   /* 10F0 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 10F1 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 10F2 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 10F3 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 10F4 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 10F5 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 10F6 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 10F7 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 10F8 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 10F9 */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 10FA */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 10FB */  { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 10FC */  { &op_LDQ  , EXTENDED     , 0, 9, 8 },
   /* 10FD */  { &op_STQ  , EXTENDED     , 0, 9, 8 },
   /* 10FE */  { &op_LDS  , EXTENDED     , 0, 7, 6 },
   /* 10FF */  { &op_STS  , EXTENDED     , 0, 7, 6 },

   /* 1100 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1101 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1102 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1103 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1104 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1105 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1106 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1107 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1108 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1109 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 110A */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 110B */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 110C */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 110D */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 110E */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 110F */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1110 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1111 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1112 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1113 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1114 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1115 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1116 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1117 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1118 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1119 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 111A */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 111B */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 111C */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 111D */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 111E */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 111F */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1120 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1121 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1122 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1123 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1124 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1125 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1126 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1127 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1128 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1129 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 112A */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 112B */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 112C */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 112D */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 112E */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 112F */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1130 */  { &op_BAND , DIRECTBIT    , 0, 7, 6 },
   /* 1131 */  { &op_BIAND, DIRECTBIT    , 0, 7, 6 },
   /* 1132 */  { &op_BOR  , DIRECTBIT    , 0, 7, 6 },
   /* 1133 */  { &op_BIOR , DIRECTBIT    , 0, 7, 6 },
   /* 1134 */  { &op_BEOR , DIRECTBIT    , 0, 7, 6 },
   /* 1135 */  { &op_BIEOR, DIRECTBIT    , 0, 7, 6 },
   /* 1136 */  { &op_LDBT , DIRECTBIT    , 0, 7, 6 },
   /* 1137 */  { &op_STBT , DIRECTBIT    , 0, 8, 7 },
   /* 1138 */  { &op_TFM  , REGISTER     , 0, 6, 6 },
   /* 1139 */  { &op_TFM  , REGISTER     , 0, 6, 6 },
   /* 113A */  { &op_TFM  , REGISTER     , 0, 6, 6 },
   /* 113B */  { &op_TFM  , REGISTER     , 0, 6, 6 },
   /* 113C */  { &op_BITMD, IMMEDIATE_8  , 0, 4, 4 },
   /* 113D */  { &op_LDMD , IMMEDIATE_8  , 0, 4, 5 },
   /* 113E */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 113F */  { &op_SWI3 , INHERENT     , 0,20,22 },
   /* 1140 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1141 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1142 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1143 */  { &op_COME , INHERENT     , 0, 2, 2 },
   /* 1144 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1145 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1146 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1147 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1148 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1149 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 114A */  { &op_DECE , INHERENT     , 0, 2, 2 },
   /* 114B */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 114C */  { &op_INCE , INHERENT     , 0, 2, 2 },
   /* 114D */  { &op_TSTE , INHERENT     , 0, 2, 2 },
   /* 114E */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 114F */  { &op_CLRE , INHERENT     , 0, 2, 2 },
   /* 1150 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1151 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1152 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1153 */  { &op_COMF , INHERENT     , 0, 2, 2 },
   /* 1154 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1155 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1156 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1157 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1158 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1159 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 115A */  { &op_DECF , INHERENT     , 0, 2, 2 },
   /* 115B */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 115C */  { &op_INCF , INHERENT     , 0, 2, 2 },
   /* 115D */  { &op_TSTF , ILLEGAL      , 0, 2, 2 },
   /* 115E */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 115F */  { &op_CLRF , INHERENT     , 0, 2, 2 },
   /* 1160 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1161 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1162 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1163 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1164 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1165 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1166 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1167 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1168 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1169 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 116A */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 116B */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 116C */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 116D */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 116E */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 116F */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1170 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1171 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1172 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1173 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1174 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1175 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1176 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1177 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1178 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1179 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 117A */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 117B */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 117C */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 117D */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 117E */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 117F */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1180 */  { &op_SUBE , IMMEDIATE_8  , 0, 3, 3 },
   /* 1181 */  { &op_CMPE , IMMEDIATE_8  , 0, 3, 3 },
   /* 1182 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1183 */  { &op_CMPU , IMMEDIATE_16 , 0, 5, 4 },
   /* 1184 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1185 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1186 */  { &op_LDE  , IMMEDIATE_8  , 0, 3, 3 },
   /* 1187 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1188 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1189 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 118A */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 118B */  { &op_ADDE , IMMEDIATE_8  , 0, 3, 3 },
   /* 118C */  { &op_CMPS , IMMEDIATE_16 , 0, 5, 4 },
   /* 118D */  { &op_DIVD , IMMEDIATE_8  , 0,25,25 },
   /* 118E */  { &op_DIVQ , IMMEDIATE_16 , 0,34,34 },
   /* 118F */  { &op_MULD , IMMEDIATE_16 , 0,28,28 },
   /* 1190 */  { &op_SUBE , DIRECT       , 0, 5, 4 },
   /* 1191 */  { &op_CMPE , DIRECT       , 0, 5, 4 },
   /* 1192 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1193 */  { &op_CMPU , DIRECT       , 0, 7, 5 },
   /* 1194 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1195 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1196 */  { &op_LDE  , DIRECT       , 0, 5, 4 },
   /* 1197 */  { &op_STE  , DIRECT       , 0, 5, 4 },
   /* 1198 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1199 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 119A */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 119B */  { &op_ADDE , DIRECT       , 0, 5, 4 },
   /* 119C */  { &op_CMPS , DIRECT       , 0, 7, 5 },
   /* 119D */  { &op_DIVD , DIRECT       , 0,27,26 },
   /* 119E */  { &op_DIVQ , DIRECT       , 0,26,35 },
   /* 119F */  { &op_MULD , DIRECT       , 0,30,29 },
   /* 11A0 */  { &op_SUBE , INDEXED      , 0, 5, 5 },
   /* 11A1 */  { &op_CMPE , INDEXED      , 0, 5, 5 },
   /* 11A2 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 11A3 */  { &op_CMPU , INDEXED      , 0, 7, 6 },
   /* 11A4 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 11A5 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 11A6 */  { &op_LDE  , INDEXED      , 0, 5, 5 },
   /* 11A7 */  { &op_STE  , INDEXED      , 0, 5, 5 },
   /* 11A8 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 11A9 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 11AA */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 11AB */  { &op_ADDE , INDEXED      , 0, 5, 5 },
   /* 11AC */  { &op_CMPS , INDEXED      , 0, 7, 6 },
   /* 11AD */  { &op_DIVD , INDEXED      , 0,27,27 },
   /* 11AE */  { &op_DIVQ , INDEXED      , 0,36,36 },
   /* 11AF */  { &op_MULD , INDEXED      , 0,30,30 },
   /* 11B0 */  { &op_SUBE , EXTENDED     , 0, 6, 5 },
   /* 11B1 */  { &op_CMPE , EXTENDED     , 0, 6, 5 },
   /* 11B2 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 11B3 */  { &op_CMPU , EXTENDED     , 0, 8, 6 },
   /* 11B4 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 11B5 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 11B6 */  { &op_LDE  , EXTENDED     , 0, 6, 5 },
   /* 11B7 */  { &op_STE  , EXTENDED     , 0, 6, 5 },
   /* 11B8 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 11B9 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 11BA */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 11BB */  { &op_ADDE , EXTENDED     , 0, 6, 5 },
   /* 11BC */  { &op_CMPS , EXTENDED     , 0, 8, 6 },
   /* 11BD */  { &op_DIVD , EXTENDED     , 0,28,27 },
   /* 11BE */  { &op_DIVQ , EXTENDED     , 0,37,36 },
   /* 11BF */  { &op_MULD , EXTENDED     , 0,31,30 },
   /* 11C0 */  { &op_SUBF , IMMEDIATE_8  , 0, 3, 3 },
   /* 11C1 */  { &op_CMPF , IMMEDIATE_8  , 0, 3, 3 },
   /* 11C2 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 11C3 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 11C4 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 11C5 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 11C6 */  { &op_LDF  , IMMEDIATE_8  , 0, 3, 3 },
   /* 11C7 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 11C8 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 11C9 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 11CA */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 11CB */  { &op_ADDF , IMMEDIATE_8  , 0, 3, 3 },
   /* 11CC */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 11CD */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 11CE */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 11CF */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 11D0 */  { &op_SUBF , DIRECT       , 0, 5, 4 },
   /* 11D1 */  { &op_CMPF , DIRECT       , 0, 5, 4 },
   /* 11D2 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 11D3 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 11D4 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 11D5 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 11D6 */  { &op_LDF  , DIRECT       , 0, 5, 4 },
   /* 11D7 */  { &op_STF  , DIRECT       , 0, 5, 4 },
   /* 11D8 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 11D9 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 11DA */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 11DB */  { &op_ADDF , DIRECT       , 0, 5, 4 },
   /* 11DC */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 11DD */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 11DE */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 11DF */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 11E0 */  { &op_SUBF , INDEXED      , 0, 5, 5 },
   /* 11E1 */  { &op_CMPF , INDEXED      , 0, 5, 5 },
   /* 11E2 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 11E3 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 11E4 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 11E5 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 11E6 */  { &op_LDF  , INDEXED      , 0, 5, 5 },
   /* 11E7 */  { &op_STF  , INDEXED      , 0, 5, 5 },
   /* 11E8 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 11E9 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 11EA */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 11EB */  { &op_ADDF , INDEXED      , 0, 5, 5 },
   /* 11EC */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 11ED */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 11EE */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 11EF */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 11F0 */  { &op_SUBF , EXTENDED     , 0, 6, 5 },
   /* 11F1 */  { &op_CMPF , EXTENDED     , 0, 6, 5 },
   /* 11F2 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 11F3 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 11F4 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 11F5 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 11F6 */  { &op_LDF  , EXTENDED     , 0, 6, 5 },
   /* 11F7 */  { &op_STF  , EXTENDED     , 0, 6, 5 },
   /* 11F8 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 11F9 */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 11FA */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 11FB */  { &op_ADDF , EXTENDED     , 0, 6, 5 },
   /* 11FC */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 11FD */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 11FE */  { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 11FF */  { &op_TRAP , ILLEGAL      , 0,20,22 }
};
