#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "memory.h"
#include "types_6809.h"
#include "em_6809.h"
#include "dis_6809.h"

// This controls whether the operand is located by working forward
// from the start of the instruction, or working backwards from the
// end. It seems with DIVD, working backwards is tricky because the
// number of cycles varies depending on the operand value. This
// is particularly problematic when LIC is not available.

#define WORK_FORWARD_TO_OPERAND

// ====================================================================
// CPU state display
// ====================================================================

static const char cpu_6809_state[] = "A=?? B=?? X=???? Y=???? U=???? S=???? DP=?? E=? F=? H=? I=? N=? Z=? V=? C=?";

static const char cpu_6309_state[] = "A=?? B=?? E=?? F=?? X=???? Y=???? U=???? S=???? DP=?? M=?? T=???? E=? F=? H=? I=? N=? Z=? V=? C=? DZ=? IL=? FM=? NM=?";

// ====================================================================
// Instruction cycle extras
// ====================================================================

// In indexed indirect modes, there are extra cycles computing the effective
// address that cannot be included in the opcode table, as they depend on the
// indexing mode in the post byte.

// Negative values indicate undefined postbytes
//  -- On the 6809 there are 39
//  -- On the 6309 there are  7

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
   2,  3,  2,  3,  0,  1,  1, -1,  1,  4, -4,  4,  1,  5, -5, -2,  // 8x
  -5,  6, -5,  6,  3,  4,  4, -4,  4,  7, -7,  7,  4,  8, -8,  5,  // 9x
   2,  3,  2,  3,  0,  1,  1, -1,  1,  4, -4,  4,  1,  5, -5, -2,  // Ax
  -5,  6, -5,  6,  3,  4,  4, -4,  4,  7, -7,  7,  4,  8, -8, -5,  // Bx
   2,  3,  2,  3,  0,  1,  1, -1,  1,  4, -4,  4,  1,  5, -5, -2,  // Cx
  -5,  6, -5,  6,  3,  4,  4, -4,  4,  7, -7,  7,  4,  8, -8, -5,  // Dx
   2,  3,  2,  3,  0,  1,  1, -1,  1,  4, -4,  4,  1,  5, -5, -2,  // Ex
  -5,  6, -5,  6,  3,  4,  4, -4,  4,  7, -7,  7,  4,  8, -8, -5   // Fx
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
   2,  3,  2,  3,  0,  1,  1,  1,  1,  4,  1,  4,  1,  5,  1,  0,  // 8x
   3,  6,-21,  6,  3,  4,  4,  4,  4,  7,  4,  7,  4,  8,  4,  5,  // 9x
   2,  3,  2,  3,  0,  1,  1,  1,  1,  4,  1,  4,  1,  5,  1,  2,  // Ax
   5,  6,-21,  6,  3,  4,  4,  4,  4,  7,  4,  7,  4,  8,  4,-21,  // Bx
   2,  3,  2,  3,  0,  1,  1,  1,  1,  4,  1,  4,  1,  5,  1,  1,  // Cx
   4,  6,-21,  6,  3,  4,  4,  4,  4,  7,  4,  7,  4,  8,  4,-21,  // Dx
   2,  3,  2,  3,  0,  1,  1,  1,  1,  4,  1,  4,  1,  5,  1,  1,  // Ex
   4,  6,-21,  6,  3,  4,  4,  4,  4,  7,  4,  7,  4,  8,  4,-21   // Fx
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
   3,  5,-23,  5,  3,  4,  4,  4,  4,  6,  4,  5,  4,  6,  4,  4,  // 9x
   1,  2,  1,  2,  0,  1,  1,  1,  1,  3,  1,  2,  1,  3,  1,  2,  // Ax
   5,  5,-23,  5,  3,  4,  4,  4,  4,  6,  4,  5,  4,  6,  4,-23,  // Bx
   1,  2,  1,  2,  0,  1,  1,  1,  1,  3,  1,  2,  1,  3,  1,  1,  // Cx
   4,  5,-23,  5,  3,  4,  4,  4,  4,  6,  4,  5,  4,  6,  4,-23,  // Dx
   1,  2,  1,  2,  0,  1,  1,  1,  1,  3,  1,  2,  1,  3,  1,  1,  // Ex
   4,  5,-23,  5,  3,  4,  4,  4,  4,  6,  4,  5,  4,  6,  4,-23   // Fx
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
static int M    = -1;

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

// Vector base (which might change on a machine-by-machine basis)
static int vector_base = 0xfff0;

// Used to supress errors in the instruction following xxxR reg,PC
static int async_pc_write = 0;
static int fail_syncbug = 0;

enum {
   VEC_IL   = 0x00,
   VEC_DZ   = 0x01, // the LSB ends up being masked off
   VEC_SWI3 = 0x02,
   VEC_SWI2 = 0x04,
   VEC_FIQ  = 0x06,
   VEC_IRQ  = 0x08,
   VEC_SWI  = 0x0A,
   VEC_NMI  = 0x0C,
   VEC_RST  = 0x0E,
   VEC_XRST = 0x0F  // the LSB ends up being masked off
};

// ====================================================================
// Fail flags
// ====================================================================

static const char * fail_hints[32] = {
   "AddrInstr",
   "AddrPointer",
   "AddrData",
   "AddrStack",
   "RnW",
   "Memory",
   "PC",
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
   "?"
};

// ====================================================================
// Forward declarations
// ====================================================================

static opcode_t instr_table_6809[];
static opcode_t instr_table_6309[];

static operation_t op_MULD ;
static operation_t op_LDMD ;
static operation_t op_SYNC ;
static operation_t op_TST  ;
static operation_t op_TFM  ;
static operation_t op_XSTX ;
static operation_t op_XSTY ;
static operation_t op_XSTU ;
static operation_t op_XSTS ;

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

static int pop8s(sample_t *sample) {
   memory_read(sample, S, MEM_STACK);
   if (S >= 0) {
      S = (S + 1) & 0xffff;
   }
   return sample->data;
}

static int push8s(sample_t *sample) {
   if (S >= 0) {
      S = (S - 1) & 0xffff;
   }
   memory_write(sample, S, MEM_STACK);
   return sample->data;
}

static int pop16s(sample_t *sample) {
   pop8s(sample);
   pop8s(sample + 1);
   return (sample->data << 8) + (sample + 1)->data;

}

static int push16s(sample_t *sample) {
   push8s(sample);
   push8s(sample + 1);
   return ((sample + 1)->data << 8) + sample->data;
}


static int pop8u(sample_t *sample) {
   memory_read(sample, U, MEM_STACK);
   if (U >= 0) {
      U = (U + 1) & 0xffff;
   }
   return sample->data;
}

static int push8u(sample_t *sample) {
   if (U >= 0) {
      U = (U - 1) & 0xffff;
   }
   memory_write(sample, U, MEM_STACK);
   return sample->data;
}

static int pop16u(sample_t *sample) {
   pop8u(sample);
   pop8u(sample + 1);
   return (sample->data << 8) + (sample + 1)->data;
}

static int push16u(sample_t *sample) {
   push8u(sample);
   push8u(sample + 1);
   return ((sample + 1)->data << 8) + sample->data;
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

static int *get_index_reg(int i) {
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
   case  0:
      ret = pack(ACCA, ACCB);
      break;
   case  1:
      ret = X;
      break;
   case  2:
      ret = Y;
      break;
   case  3:
      ret = U;
      break;
   case  4:
      ret = S;
      break;
   case  5:
      ret = PC;
      break;
   case  8:
      // Extend ACCA value to 16 bits by padding with FF
      ret = ACCA;
      if (ret >= 0) {
         ret |= 0xff00;
      }
      break;
   case  9:
      // Extend ACCB value to 16 bits by padding with FF
      ret = ACCB;
      if (ret >= 0) {
         ret |= 0xff00;
      }
      break;
   case 10:
      // Extend CC value to 16 bits by replicating
      ret = get_FLAGS();
      if (ret >= 0) {
         ret |= ret << 8;
      }
      break;
   case 11:
      // Extend DP value to 16 bits by replicating
      ret = DP;
      if (ret >= 0) {
         ret |= ret << 8;
      }
      break;
   default:
      ret = 0xffff;
      break;
   }
   return ret;
}

// Used in EXN/TRV on the 6809
static void set_regp_6809(int i, int val) {
   // Must correctly handle case where val<0 (undefined)
   i &= 15;
   switch(i) {
   case  0: unpack(val, &ACCA, &ACCB);         break;
   case  1: X  = val;                          break;
   case  2: Y  = val;                          break;
   case  3: U  = val;                          break;
   case  4: S  = val;                          break;
   case  5: PC = val;                          break;
   case  8: ACCA = val < 0 ? val : val & 0xff; break;
   case  9: ACCB = val < 0 ? val : val & 0xff; break;
   case 10: set_FLAGS(val);                    break;
   case 11: DP = val < 0 ? val : val & 0xff;   break;
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
   // Must correctly handle case where val<0 (undefined)
   i &= 15;
   // cases 12 and 13 (writing back to the 0 register) are NOPs (they don't trap)
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
   case 10: set_FLAGS(val);            break;
   case 11: DP = (val < 0) ? val : (val >> 8) & 0xff; break;
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
   // Set everything to unknown
   ACCA = -1;
   ACCB = -1;
   ACCE = -1;
   ACCF = -1;
   X    = -1;
   Y    = -1;
   S    = -1;
   U    = -1;
   DP   = -1;
   PC   = -1;
   TV   = -1;
   E    = -1;
   F    = -1;
   H    = -1;
   I    = -1;
   N    = -1;
   Z    = -1;
   V    = -1;
   C    = -1;
   NM   = -1;
   FM   = -1;
   IL   = -1;
   DZ   = -1;
   // Parse arguments
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
   if (args->machine == MACHINE_BEEB) {
      vector_base = 0xf7f0; // A11 is inverted in vector pull
   }
   cpu6309 = args->cpu_type == CPU_6309 || args->cpu_type == CPU_6309E;
   fail_syncbug = args->fail_syncbug && cpu6309;

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
      if (instr_6809->undocumented) {
         // If the MODE is marked as illegal, then fall-through to the base instruction
         if (i >= 0x100 && instr_6809->mode == ILLEGAL) {
            *instr_6809 = instr_table_6809[i & 0xff];
            instr_6809->undocumented = 1;
            instr_6809->cycles++;
         }
      } else {
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

static int em_6809_match_interrupt(sample_t *sample_q, int num_samples, int pc) {
   // An interrupt always starts with the current PC being pushed to the stack
   // so check for this. This conveniently excludes CWAI, where PC+1 will be
   // pushed to the stack, to prevent the CWAI being endlessly executed.
   if (pc >= 0) {
      int offset = (NM == 1) ?  4 : 3;
      int pushedPC = sample_q[offset].data + (sample_q[offset + 1].data << 8);
      if (pc != pushedPC) {
         return 0;
      }
   }
   // Calculate expected offset to vector fetch taking account of
   // native mode on the 6309 pushing two extra bytes (ACCE/ACCF)
   // (and one pipeline stall cycle ??)
   int fast_o = (NM == 1) ? 10 :  7;
   int full_o = (NM == 1) ? 19 : 16;
   // FIQ:
   //    m +  7   addr=6 ba=0 bs=1 <<<<<< fast_o
   //    m +  8   addr=7 ba=0 bs=1
   //    m +  9   addr=X ba=0 bs=0
   //    m + 10   <Start of first instruction>
   //
   if (sample_q[fast_o].ba < 1 && sample_q[fast_o].bs == 1 && sample_q[fast_o].addr == 0x6) {
      return fast_o + 3;
   }
   // IRQ:
   //    m + 16    addr=8 ba=0 bs=1 <<<<<< full_o
   //    m + 17    addr=9 ba=0 bs=1
   //    m + 18    addr=X ba=0 bs=0
   //    m + 19    <Start of first instruction>
   //
   if (sample_q[full_o].ba < 1 && sample_q[full_o].bs == 1 && sample_q[full_o].addr == 0x8) {
      return full_o + 3;;
   }
   // NMI:
   //    m + 16    addr=C ba=0 bs=1 <<<<<< full_o
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
   // To match reset A3..0 of the address bus must be connected, so we see the vector
   //
   // i - n    lic=0 addr=8 ba=0 bs=0
   // i - 1    lic=0 addr=8 ba=0 bs=0
   // i        lic=0 addr=E ba=0 bs=1
   // i + 1    lic=0 addr=F ba=0 bs=1
   // i + 2    lic=1 addr=X ba=0 bs=0
   // <Start of first instruction>
   if (sample_q->addr >= 0) {
      int i = 0;
      if (sample_q->lic >= 0) {
         // LIC is available, so find the next instruction boundary
         if (NM == 1) {
            i++;
         }
         while (i < num_samples - 3 && !sample_q[i].lic) {
            i++;
         }
         // The vector is two cycles before LIC is seen (NM=0 always now due to reset)
         if (i < 2) {
            return 0;
         }
         i -= 2;
      } else {
         // LIC is not available, so look for a change in the address bus instead
         // The reason for using the address bus and not bs is efficiency, given
         // num_samples is very large, and bs is normally 0 for long periods.
         int addr = sample_q->addr;
         while (i < num_samples - 3 && sample_q[i].addr == addr) {
            i++;
         }
         // The vector is the first change of the address bus
      }
      if (sample_q[i].ba < 1 && sample_q[i].bs == 1 && sample_q[i].addr == 0x0E) {
         // Avoid matching XRES on the 6809 (opcode 0x3e, length 19)
         if (cpu6309 || sample_q[0].data != 0x3E || i != 16) {
            return i + 3;
         }
      }
   }
   return 0;
}

static inline int is_prefix(sample_t *sample) {
   return (sample->data & 0xfe) == 0x10;
}

static inline opcode_t *get_instruction(opcode_t *instr_table, sample_t *sample) {
   int prefix = 0;
   if (is_prefix(sample)) {
      prefix = 1 + (sample->data & 1);
      sample++;
      // On the 6809, additional prefixes are ignored
      while (!cpu6309 && is_prefix(sample)) {
         sample++;
      }
   }
   return instr_table + 0x100 * prefix + sample->data;
}

static int count_cycles_with_lic(sample_q_t *sample_q) {
   sample_t *sample = sample_q->sample;
   int num_samples = sample_q->num_samples;
   // If NM==0 then LIC set on the last cycle of the instruction
   // If NM==1 then LIC set on the first cycle of the instruction
   int offset = (NM == 1) ? 1 : 0;
   // Search for LIC
   for (int i = offset; i < num_samples; i++) {
      if (sample[i].type == LAST) {
         return 0;
      }
      if (sample[i].lic == 1) {
         return i + 1 - offset;
      }
   }
   return CYCLES_TRUNCATED;
}


static void em_6809_reset(sample_t *sample_q, int num_cycles, instruction_t *instruction) {
   instruction->pc = -1;
   instruction->length = 0;
   instruction->rst_seen = 1;
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
static int interrupt_helper(sample_q_t *sample_q, int offset, int full, int vector) {
   sample_t *sample = sample_q->sample;
   // FIQ
   //  0 Opcode
   //  1
   //  2
   //  3 PCL   <===== offset
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
   //  3 PCL   <===== offset
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

   int i = sample_q->oi + offset;

   // The PC is pushed in all cases
   int pc = push16s(sample + i);
   i += 2;

   // The full state is pushed in IRQ/NMI/SWI/SWI2/SWI3
   if (full) {


      int u  = push16s(sample + i);
      i += 2;
      if (U >= 0 && u != U) {
         failflag |= FAIL_U;
      }
      U = u;

      int y  = push16s(sample + i);
      i += 2;
      if (Y >= 0 && y != Y) {
         failflag |= FAIL_Y;
      }
      Y = y;

      int x = push16s(sample + i);
      i += 2;
      if (X >= 0 && x != X) {
         failflag |= FAIL_X;
      }
      X = x;

      int dp = push8s(sample + i);
      i++;
      if (DP >= 0 && dp != DP) {
         failflag |= FAIL_DP;
      }
      DP = dp;

      if (NM == 1) {

         int f = push8s(sample + i);
         i++;
         if (ACCF >= 0 && f != ACCF) {
            failflag |= FAIL_ACCF;
         }
         ACCF = f;

         int e = push8s(sample + i);
         i++;
         if (ACCE >= 0 && e != ACCE) {
            failflag |= FAIL_ACCE;
         }
         ACCE = e;

      }

      int b = push8s(sample + i);
      i++;
      if (ACCB >= 0 && b != ACCB) {
         failflag |= FAIL_ACCB;
      }
      ACCB = b;

      int a = push8s(sample + i);
      i++;
      if (ACCA >= 0 && a != ACCA) {
         failflag |= FAIL_ACCA;
      }
      ACCA = a;
      // Set E to indicate the full state was saved (apart from for XRES)
      if (vector != VEC_XRST) {
         E = 1;
      }
   } else {
      // Clear E to indicate just PC/flags were saved
      E = 0;
   }

   // The flags are pushed in all cases
   int flags = push8s(sample + i);
   check_FLAGS(flags);
   set_FLAGS(flags);

   // The vector fetch is always at the end
   // (even for CWAI)
   i = sample_q->num_cycles - 3;

   // Is an illegal instruction trap?
   if (vector == VEC_IL) {
      IL = 1;
   }

   // Is it a division by zero trap?
   if (vector == VEC_DZ) {
      DZ = 1;
   }

   // Mask off the LSB of the vector, which is used as a flag
   vector &= 0xFFFE;

   // Read the vector and compare against what's expected
   int vechi = sample[i].data;
   memory_read(sample + i, vector_base + vector, MEM_POINTER);
   if (sample[i].addr >= 0 && (sample[i].addr != vector)) {
      failflag |= FAIL_VECTOR;
   }
   i++;
   int veclo = sample[i].data;
   memory_read(sample + i, vector_base + vector + 1, MEM_POINTER);
   if (sample[i].addr >= 0 && (sample[i].addr != vector + 1)) {
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
   case VEC_IRQ:
      I = 1;
      break;
   case VEC_FIQ:
   case VEC_SWI:
   case VEC_NMI:
   case VEC_RST:
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
   int pc;
   sample_q_t sample_ref;
   sample_ref.sample = sample_q;
   sample_ref.oi = 0;
   sample_ref.num_cycles = num_cycles;
   if (num_cycles == fast_c) {
      pc = interrupt_helper(&sample_ref, offset, 0, VEC_FIQ);
   } else if (num_cycles == full_c && sample_q[full_c - 3].addr == VEC_IRQ) {
      // IRQ
      pc = interrupt_helper(&sample_ref, offset, 1, VEC_IRQ);
   } else if (num_cycles == full_c && sample_q[full_c - 3].addr == VEC_NMI) {
      // NMI
      pc = interrupt_helper(&sample_ref, offset, 1, VEC_NMI);
   } else {
      printf("*** could not determine interrupt type ***\n");
      pc = -1;
   }
   instruction->pc = pc;
   instruction->length = 0;
   instruction->intr_seen = 1;
}

static inline int offset_address(int base, int offset) {
   return (base >= 0) ? (base + offset) & 0xffff : base;
}

static int em_6809_emulate(sample_t *sample_q, int num_samples, instruction_t *instruction) {
   int num_cycles;

   instruction->intr_seen = 0;
   instruction->rst_seen = 0;
   instruction->pc = PC;

   if ((num_cycles = em_6809_match_reset(sample_q, num_samples)) > 0) {
      em_6809_reset(sample_q, num_cycles, instruction);
      return num_cycles;
   }

   if ((num_cycles = em_6809_match_interrupt(sample_q, num_samples, PC)) > 0) {
      em_6809_interrupt(sample_q, num_cycles, instruction);
      return num_cycles;
   }

   int pb = 0;
   int index = 0;
   opcode_t *instr = get_instruction(instr_table, sample_q);
   int mode = instr->mode;

   // Start with the base number of cycles from the instruction table
   num_cycles = (NM == 1) ? instr->cycles_native : instr->cycles;

   // If we have fewer samples that this, then bail early to prevent suprious errors
   if (num_samples < num_cycles) {
      failflag = 0;
      return CYCLES_TRUNCATED;
   }

   // Flag that an instruction marked as undocumented has been encoutered
   if (instr->undocumented) {
      failflag |= FAIL_UNDOC;
   }

   // Memory modelling of the prefix
   if (is_prefix(sample_q + index)) {
      memory_read(sample_q + index, offset_address(PC, index), MEM_INSTR);
      index++;
      // On the 6809, additional prefixes are ignored
      while (!cpu6309 && is_prefix(sample_q + index)) {
         memory_read(sample_q + index, offset_address(PC, index), MEM_INSTR);
         index++;
         // But they do take an additional cycle
         num_cycles++;
      }
   }

   int oi = index;

   // Memory modelling of the opcode
   memory_read(sample_q + index, offset_address(PC, index), MEM_INSTR);
   index++;

   // If there is an immediate byte (AIM/EIM/OIM/TIM only), skip past it
   if (mode == DIRECTIM || mode == EXTENDEDIM || mode == INDEXEDIM) {
      // The immediate constant is held in the M register
      M = sample_q[index].data;
      // Memory modelling
      memory_read(sample_q + index, offset_address(PC, index), MEM_INSTR);
      index++;
      // Decrement the mode to get back to the base addressing mode
      mode--;
      // Increment opcode index (oi), which allows the rest of the code to ignore the immediate byte
      oi++;
   }

   sample_q_t sample_ref;
   sample_ref.sample = sample_q;
   sample_ref.num_samples = num_samples;
   sample_ref.oi = oi; // This is the opcode index after prefixes have been skipped

   // If there is a post byte, skip past it
   if (mode == REGISTER || mode == INDEXED || mode == DIRECTBIT) {
      pb = sample_q[index].data;
      memory_read(sample_q + index, offset_address(PC, index), MEM_INSTR);
      index++;
   }

   // Process any additional instruction bytes
   switch (mode) {
   case INDEXED:
      // In some indexed addressing modes there is also a displacement
      if (pb & 0x80) {
         int type = pb & 0x0f;
         int disp_bytes = 0;
         if (cpu6309) {
            if (type == 9 || type == 13 || pb == 0x9f || pb == 0xaf || pb == 0xb0) {
               disp_bytes = 2;
            } else if (type == 8 || type == 12) {
               disp_bytes = 1;
            }
         } else {
            if (type == 9 || type == 13 || type == 15) {
               disp_bytes = 2;
            } else if (type == 8 || type == 12) {
               disp_bytes = 1;
            }
         }
         // Memory modelling of the displacement bytes
         for (int i = 0; i < disp_bytes; i++) {
            memory_read(sample_q + index + i, offset_address(PC, index + i), MEM_INSTR);
         }
         index += disp_bytes;
      }
      break;
   case DIRECTBIT:
   case DIRECT:
   case RELATIVE_8:
   case IMMEDIATE_8:
      memory_read(sample_q + index, offset_address(PC, index), MEM_INSTR);
      index++;
      break;
   case EXTENDED:
   case RELATIVE_16:
   case IMMEDIATE_16:
      memory_read(sample_q + index    , offset_address(PC, index)    , MEM_INSTR);
      memory_read(sample_q + index + 1, offset_address(PC, index + 1), MEM_INSTR);
      index += 2;
      break;
   case IMMEDIATE_32:
      memory_read(sample_q + index    , offset_address(PC, index)    , MEM_INSTR);
      memory_read(sample_q + index + 1, offset_address(PC, index + 1), MEM_INSTR);
      memory_read(sample_q + index + 2, offset_address(PC, index + 2), MEM_INSTR);
      memory_read(sample_q + index + 3, offset_address(PC, index + 3), MEM_INSTR);
      index += 4;
      break;
   default:
      break;
   }

   // Special Case XSTS/XSTU/XSTX/XSTY
   if (instr->op == &op_XSTS || instr->op == &op_XSTU || instr->op == &op_XSTX || instr->op == &op_XSTY) {
      // The instruction is marked as IMMEDIATE_8, because the final byte is written not read
      index++;
   }

   // Copy instruction for ease of hex printing
   for (int i = 0; i < index; i++) {
      instruction->instr[i] = sample_q[i].data;
   }
   instruction->length = index;

   // Sanity check the instruction bytes have sequential addresses
   // which can help to avoid incorrect synchronization to the instruction stream
   if (sample_q[0].addr >= 0 && async_pc_write == 0) {
      for (int i = 1; i < instruction->length; i++) {
         if (sample_q[i].addr != ((sample_q[0].addr + i) & 15)) {
            failflag |= FAIL_ADDR_INSTR;
            break;
         }
      }
   }
   async_pc_write = 0;

   // In indexed mode, calculate the additional postbyte cycles
   int postbyte_cycles = 0;
   if (mode == INDEXED) {
      if (cpu6309) {
         if (NM == 1) {
            postbyte_cycles = postbyte_cycles_6309_nat[pb];
         } else {
            postbyte_cycles = postbyte_cycles_6309_emu[pb];
         }
      } else {
         postbyte_cycles = postbyte_cycles_6809[pb];
      }
      if (postbyte_cycles < 0) {
         postbyte_cycles = -postbyte_cycles;
         num_cycles += postbyte_cycles;
         failflag |= FAIL_BADM;
         if (cpu6309) {
            // 21/23 cycles
            num_cycles = oi + postbyte_cycles;
            sample_ref.num_cycles = num_cycles;
            interrupt_helper(&sample_ref, 5, 1, VEC_IL);
            // TODO: validate actual
            return num_cycles;
         }
      } else {
         num_cycles += postbyte_cycles;
      }
      // Again, if we have fewer samples than num_cycles, then bail early to prevent suprious errors
      if (num_samples < num_cycles) {
         failflag = 0;
         return CYCLES_TRUNCATED;
      }
   }

   // In main, instruction->pc is checked against the emulated PC, so in the case
   // of JSR/BSR/LBSR this provides a means of sanity checking
   if (instr->op->type == JSROP) {
      instruction->pc = ((sample_q[num_cycles - 1].data << 8) + sample_q[num_cycles - 2].data - instruction->length) & 0xffff;
   }

   // Update the PC assuming not change of flow takes place
   if (PC >= 0) {
      PC = (PC + instruction->length) & 0xffff;
   }

   // Calculate the effective address (for additional memory reads)
   // Note: oi is the opcode index (0 = no prefix, 1 = prefix)
   ea_t ea = -1;
   switch (mode) {
   case RELATIVE_8:
      if (PC >= 0) {
         ea = (PC + (int8_t)sample_q[oi + 1].data) & 0xffff;
      }
      break;
   case RELATIVE_16:
      if (PC >= 0) {
         ea = (PC + (sample_q[oi + 1].data << 8) + sample_q[oi + 2].data) & 0xffff;
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
         int *reg = get_index_reg((pb >> 5) & 0x03);
         if (!(pb & 0x80)) {       /* n4,R */
            if (*reg >= 0) {
               if (pb & 0x10) {
                  ea = (*reg - ((pb & 0x0f) ^ 0x0f) - 1) & 0xffff;
               } else {
                  ea = (*reg + (pb & 0x0f)) & 0xffff;
               }
            }
         } else {
            if (cpu6309 && ((pb & 0x1f) == 0x0f || (pb & 0x1f) == 0x10)) {

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
                     unpack(W, &ACCE, &ACCF);
                     break;
                  case 3:           /* ,--W */
                     W = (W - 2) & 0xffff;
                     ea = W;
                     unpack(W, &ACCE, &ACCF);
                     break;
                  }
               } else if (ACCF >= 0) {
                  // If ACCF is defined (but ACCE is undefined) we can still correctly update ACCF
                  switch ((pb >> 5) & 3) {
                  case 2:           /* ,W++ */
                     ACCF = (ACCF + 2) & 0xff;
                     break;
                  case 3:           /* ,--W */
                     ACCF = (ACCF - 2) & 0xff;
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
                     // The accumulator is treated as a 8-bit signed offset (!!!)
                     int offset = ACCB;
                     if (offset & 0x80) {
                        offset -= 0x100;
                     }
                     ea = (*reg + offset) & 0xffff;
                  }
                  break;
               case 6:                 /* A,R */
                  if (*reg >= 0 && ACCA >= 0) {
                     // The accumulator is treated as a 8-bit signed offset (!!!)
                     int offset = ACCA;
                     if (offset & 0x80) {
                        offset -= 0x100;
                     }
                     ea = (*reg + offset) & 0xffff;
                  }
                  break;
               case 7:                 /* E,R */
                  if (cpu6309) {
                     if (*reg >= 0 && ACCE >= 0) {
                        // The accumulator is treated as a 8-bit signed offset (!!!)
                        int offset = ACCE;
                        if (offset & 0x80) {
                           offset -= 0x100;
                        }
                        ea = (*reg + offset) & 0xffff;
                     }
                  } else {
                     // Ref: David Flamand's Undocumented 6809 Paper
                     if (*reg >= 0 && ACCA >= 0) {
                        // The accumulator is treated as a 8-bit signed offset (!!!)
                        int offset = ACCA;
                        if (offset & 0x80) {
                           offset -= 0x100;
                        }
                        ea = (*reg + offset) & 0xffff;
                     }
                  }
                  break;
               case 8:                 /* n7,R */
                  if (*reg >= 0) {
                     ea = (*reg + (int8_t)(sample_q[oi + 2].data)) & 0xffff;
                  }
                  break;
               case 9:                 /* n15,R */
                  if (*reg >= 0) {
                     ea = (*reg + (sample_q[oi + 2].data << 8) + sample_q[oi + 3].data) & 0xffff;
                  }
                  break;
               case 10:                /* F,R */
                  if (cpu6309) {
                     if (*reg >= 0 && ACCF >= 0) {
                        // The accumulator is treated as a 8-bit signed offset (!!!)
                        int offset = ACCF;
                        if (offset & 0x80) {
                           offset -= 0x100;
                        }
                        ea = (*reg + offset) & 0xffff;
                     }
                  } else {
                     // Ref: David Flamand's Undocumented 6809 Paper
                     if (PC >= 0) {
                        ea = ((PC + 1) | 0x00ff) & 0xffff;
                        if (ACCA >= 0) {
                           ACCA &= sample_q[oi + 2].data;
                        }
                     }
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
                     ea = (PC + (sample_q[oi + 2].data << 8) + sample_q[oi + 3].data) & 0xffff;
                  }
                  break;
               case 14:                /* W,R */
                  if (cpu6309) {
                     if (*reg >= 0 && ACCE >= 0 && ACCF >= 0) {
                        ea = (*reg + (ACCE << 8) + ACCF) & 0xffff;
                     }
                  } else {
                     // Ref: David Flamand's Undocumented 6809 Paper
                     ea = 0xffff;
                  }
                  break;
               case 15:                /* [n] */
                  ea = ((sample_q[oi + 2].data << 8) + sample_q[oi + 3].data) & 0xffff;
                  break;
               }
            }
            if (pb & 0x10) {
               // In this mode there is a further level of indirection to find the ea
               // The postbyte_cycles tables contain the number of extra cycles for this indexed mode
               int offset = postbyte_cycles;
               // In long form: offset = oi + 2 + postbyte_cycles - 2;
               // - the oi skips the prefix
               // - the first 2 skips the opcode and postbyte
               // - the final 2 steps back to the effective address read
               offset += oi;
               memory_read(sample_q + offset    , ea, MEM_POINTER);
               if (ea >= 0) {
                  ea = (ea + 1 ) & 0xffff;
               }
               memory_read(sample_q + offset + 1, ea, MEM_POINTER);
               ea = ((sample_q[offset].data << 8) + sample_q[offset + 1].data) & 0xffff;
            }
         }
      }
      break;
   default:
      break;
   }
   // Special Case XSTS/XSTU/XSTX/XSTY
   if (PC >= 0 && (instr->op == &op_XSTS || instr->op == &op_XSTU || instr->op == &op_XSTX || instr->op == &op_XSTY)) {
      // The write happens to second byte of immediate data
      ea = (PC - 1) & 0xffff;
   }

#ifdef WORK_FORWARD_TO_OPERAND

   // WORK FORWARD to find the operand

   // On entry, oi points to the opcode

   switch (mode) {
   case INHERENT:
   case IMMEDIATE_8:
   case IMMEDIATE_16:
   case IMMEDIATE_32:
   case REGISTER:
      // [ <Prefix> ] <Opcode> <Operand>
      oi++;
      break;
   case DIRECT:
      if (instr->mode == DIRECTIM) {
         // [ <Prefix> ] <Opcode> <Immediate> <Direct> <Operand>
         // oi currently points to <Immediate>
         oi += 2;
      } else {
         // [ <Prefix> ] <Opcode> <Direct> <Dummy> <Operand>
         oi += (NM == 1) ? 2 : 3;
      }
      break;
   case DIRECTBIT:
      // [ <Prefix> ] <Opcode> <Postbyte> <Direct> <Dummy> <Operand>
      oi += (NM == 1) ? 3 : 4;
      break;
   case EXTENDED:
      if (instr->mode == EXTENDEDIM) {
         // [ <Prefix> ] <Opcode> <Immediate> <Extended Hi> <Extended Lo> <Operand>
         // oi currently points to <Immediate>
         oi += 3;
      } else {
         // [ <Prefix> ] <Opcode> <Extended Hi> <Extended Lo> <Dummy> <Operand>
         oi += (NM == 1) ? 3 : 4;
      }
      break;
   case INDEXED:
      if (instr->mode == INDEXEDIM) {
         // Test Immediate example in NM=0
         //
         // num cycles = 7 (5 + 2 extra indexed)
         //
         // 142017  0 6b  1 0 00 B <Opcode>
         // 142018  1 08  1 0 00 C <Immediate>
         // 142019  2 e0  1 0 00 D <Postbyte>
         // 14201a  3 27  1 0 00 E
         // 14201b  4 27  1 0 00 F
         // 14201c  5 27  1 0 00 F
         // 14201d  6 49  1 1 00 C <Operand>
         // EC6B : 6B 08 E0       : TIM   #$08 ,S+       :   7 : A=80 B=FF E=?? F=?? X=0049 Y=00ED U=0700 S=???? DP=00 T=???? E=1 F=1 H=0 I=1 N=0 Z=1 V=0 C=0 DZ=? IL=? FM=? NM=0
         //
         // oi currently points to <Immediate>
         oi += postbyte_cycles + 3;
      } else {
         // [ <Prefix> ] <Opcode> <Postbyte> ... <Operand>
         oi += postbyte_cycles + ((NM == 1) ? 3 : 3);
      }
      break;
   }

#else

   // WORK BACKWARD from the end of the instruction to find the operand

   // Pick out the read operand (Fig 17 in datasheet, esp sheet 5)
   if (instr->op == &op_MULD) {
      // There are many dead cycles at the end of MULD
      oi = num_cycles - 26;
   } else if (instr->op == &op_TST) {
      // There are two dead cycles at the end of TST in emul mode, and one in native mode
      oi = num_cycles - ((NM == 1) ? 2 : 3);
   } else if (mode == IMMEDIATE_8 || mode == IMMEDIATE_16 || mode == REGISTER) {
      // operand immediately follows the opcode
      oi++;
   } else if (mode == DIRECTBIT) {
      // There is one dead cycle at the end of directbit
      oi = num_cycles - 2;
   } else if (instr->op->type == RMWOP) {
      // Read-modify-write instruction
      oi = num_cycles - 3;
   } else if (instr->op->size == SIZE_32) {
      // Quad-byte operand
      oi = num_cycles - 4;
   } else if (instr->op->size == SIZE_16) {
      // Double byte operand
      if (instr->op->type == LOADOP || instr->op->type == STOREOP || NM == 1) {
         // No dead cycle at the end with LDD/LDS/LDU/LDX/LDY/STD/STS/STU/STX/STY/JSR
         oi = num_cycles - 2;
      } else {
         // Dead cycle at the end in ADDD/CMPD/CMPS/CMPU/CMPX/CMPY/SUBD
         oi = num_cycles - 3;
      }
   } else {
      // Single byte operand
      oi = num_cycles - 1;
   }
#endif

   // Calculate the read operand
   operand_t operand;
   if (instr->op->size == SIZE_32) {
      operand = (sample_q[oi    ].data << 24) + (sample_q[oi + 1].data << 16) +
                (sample_q[oi + 2].data <<  8) +  sample_q[oi + 3].data;
   } else if (instr->op->size == SIZE_16) {
      operand = (sample_q[oi].data << 8) + sample_q[oi + 1].data;
   } else {
      operand = sample_q[oi].data;
   }

   // Memory modelling of the read operand
   if (instr->op->type == RMWOP || instr->op->type == LOADOP ||  instr->op->type == READOP) {
      if (instr->op->size == SIZE_32) {
         memory_read(sample_q + oi    ,                ea,     MEM_DATA);
         memory_read(sample_q + oi + 1, offset_address(ea, 1), MEM_DATA);
         memory_read(sample_q + oi + 2, offset_address(ea, 2), MEM_DATA);
         memory_read(sample_q + oi + 3, offset_address(ea, 3), MEM_DATA);
      } else if (instr->op->size == SIZE_16) {
         memory_read(sample_q + oi    ,                ea    , MEM_DATA);
         memory_read(sample_q + oi + 1, offset_address(ea, 1), MEM_DATA);
      } else {
         memory_read(sample_q + oi    ,                ea    , MEM_DATA);
      }
   }

   // Operand 2 is the value written back in a store or read-modify-write
   operand_t operand2 = 0;
   if (instr->op->type == RMWOP || instr->op->type == STOREOP) {
      if (instr->op->size == SIZE_32) {
         operand2 = (sample_q[num_cycles - 4].data << 24) + (sample_q[num_cycles - 3].data << 16) + (sample_q[num_cycles - 2].data << 8) + sample_q[num_cycles - 1].data;
         memory_write(sample_q + num_cycles - 4,                ea,     MEM_DATA);
         memory_write(sample_q + num_cycles - 3, offset_address(ea, 1), MEM_DATA);
         memory_write(sample_q + num_cycles - 2, offset_address(ea, 2), MEM_DATA);
         memory_write(sample_q + num_cycles - 1, offset_address(ea, 3), MEM_DATA);
      } else if (instr->op->size == SIZE_16) {
         operand2 = (sample_q[num_cycles - 2].data << 8) + sample_q[num_cycles - 1].data;
         memory_write(sample_q + num_cycles - 2,                ea,     MEM_DATA);
         memory_write(sample_q + num_cycles - 1, offset_address(ea, 1), MEM_DATA);
      } else {
         operand2 = sample_q[num_cycles - 1].data;
         memory_write(sample_q + num_cycles - 1,                ea    , MEM_DATA);
      }
   }

   // Emulate the instruction, and check the result against what was seen on the bus
   if (instr->op->emulate) {
      sample_ref.num_cycles = num_cycles;
      int result = instr->op->emulate(operand, ea, &sample_ref);
      num_cycles = sample_ref.num_cycles;
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
      }
   }

   if (instr->op->type == RMWOP && (instr->mode != DIRECTIM && instr->mode != EXTENDEDIM && instr->mode != INDEXEDIM)) {
      // M register usef for the result of the RMW operation
      M = operand2;
   }

   // At this point num_cycles is the expected number of cycles (ignoring LIC)
   if (num_cycles > num_samples) {
      num_cycles = CYCLES_TRUNCATED;
   }

   // The SYNC bug occurs on the 6309 in native-mode where a single
   // cycle instruction (e.g. CLRA) is followed immediately by a
   // SYNC. It causes the flags to be set incorrectly.
   //
   // If fail_syncbug=0 then we suppress the bug setting the flags to undefined
   if (NM == 1 && !fail_syncbug && num_cycles == sample_ref.oi + 1 && sample_q[num_cycles].data == 0x13) {
      set_NZVC_unknown();
   }

   // If LIC is available, we return the actual number of cycles, and validate the estimate
   if (sample_q->lic >= 0 && instr->op != &op_TFM && instr->op != &op_LDMD && instr->op != &op_SYNC) {
      // Certain instruction need to be excluded, because LIC is an unreliable indication of length
      // - TFM, when interrupted
      // - LDMD, when changing mode
      // - SYNC, because LIC occurs in the middle of the instruction
      int actual_cycles = count_cycles_with_lic(&sample_ref);
      // Validate the estimated number of cycles
      if (actual_cycles >= 0) {
         if (show_cycle_errors && actual_cycles != num_cycles) {
            failflag |= FAIL_CYCLES;
         }
         num_cycles = actual_cycles;
      }
   }

   // TODO: is this needed?
   if (num_cycles == CYCLES_TRUNCATED) {
      failflag = 0;
   }

   // Return a possibly updates estimate of the number of cycles
   return num_cycles;
}

static int em_6809_get_PC() {
   return PC;
}

static int em_6809_get_NM() {
   return NM;
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
      if (M >= 0) {
         write_hex2(bp, M);
      }
      bp += 5;
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
   .emulate = em_6809_emulate,
   .disassemble = dis_6809_disassemble,
   .get_PC = em_6809_get_PC,
   .get_NM = em_6809_get_NM,
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
   // it seems to be unchanged (i.e. no errors). Verified on 6309.
   return val;
}

static int asl16_helper(int val) {
   if (val >= 0) {
      C = (val >> 15) & 1;
      // V is the xor of bits 15,14 of val
      V = ((val >> 14) & 1) ^ C;
      val = (val << 1) & 0xffff;
      set_NZ16(val);
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
   // it seems to be unchanged (i.e. no errors). Verified on 6309.
   return val;
}

static int asr16_helper(int val) {
   if (val >= 0) {
      C = val & 1;
      val = (val & 0x8000) | (val >> 1);
      set_NZ16(val);
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
   // it seems to be unchanged (i.e. no errors). Verified on 6309.
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
      set_NZ16(val);
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
   C = (val != 0x00);
   val = (-val) & 0xff;
   set_NZ(val);
   // The datasheet says the half-carry flag is undefined, but in practice
   // it seems to be unchanged (i.e. no errors). Verified on 6309.
   return val;
}

static int neg16_helper(int val) {
   V = (val == 0x8000);
   C = (val != 0x0000);
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

static void push_helper(sample_q_t *sample_q, int system) {
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
   sample_t *sample = sample_q->sample + sample_q->oi;
   int *us;
   int (*push8)(sample_t *);
   int (*push16)(sample_t *);
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
   int pb = sample[1].data;
   sample_q->num_cycles += count_ones_in_nibble[pb & 0x0f];            // bits 0..3 are 8 bit registers
   sample_q->num_cycles += count_ones_in_nibble[(pb >> 4) & 0x0f] * 2; // bits 4..7 are 16 bit registers
   if (sample_q->num_samples < sample_q->num_cycles) {
      sample_q->num_cycles = CYCLES_TRUNCATED;
      return;
   }
   int tmp;
   int i = (NM == 1) ? 4 : 5;
   if (pb & 0x80) {
      tmp = push16(sample + i);
      i += 2;
      if (PC >= 0 && PC != tmp) {
         failflag |= FAIL_PC;
      }
      PC = tmp;
   }
   if (pb & 0x40) {
      tmp = push16(sample + i);
      i += 2;
      if (*us >= 0 && *us != tmp) {
         failflag |= fail_us;
      }
      *us = tmp;
   }
   if (pb & 0x20) {
      tmp = push16(sample + i);
      i += 2;
      if (Y >= 0 && Y != tmp) {
         failflag |= FAIL_Y;
      }
      Y = tmp;
   }
   if (pb & 0x10) {
      tmp = push16(sample + i);
      i += 2;
      if (X >= 0 && X != tmp) {
         failflag |= FAIL_X;
      }
      X = tmp;
   }
   if (pb & 0x08) {
      tmp = push8(sample + i);
      i++;
      if (DP >= 0 && DP != tmp) {
         failflag |= FAIL_DP;
      }
      DP = tmp;
   }
   if (pb & 0x04) {
      tmp = push8(sample + i);
      i++;
      if (ACCB >= 0 && ACCB != tmp) {
         failflag |= FAIL_ACCB;
      }
      ACCB = tmp;
   }
   if (pb & 0x02) {
      tmp = push8(sample + i);
      i++;
      if (ACCA >= 0 && ACCA != tmp) {
         failflag |= FAIL_ACCA;
      }
      ACCA = tmp;
   }
   if (pb & 0x01) {
      tmp = push8(sample + i);
      i++;
      check_FLAGS(tmp);
      set_FLAGS(tmp);
   }
}

static void pull_helper(sample_q_t *sample_q, int system) {
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
   sample_t *sample = sample_q->sample + sample_q->oi;
   int *us;
   int (*pop8)(sample_t *);
   int (*pop16)(sample_t *);
   if (system) {
      pop8 = pop8s;
      pop16 = pop16s;
      us = &U;
   } else {
      pop8 = pop8u;
      pop16 = pop16u;
      us = &S;
   }

   int pb = sample[1].data;
   sample_q->num_cycles += count_ones_in_nibble[pb & 0x0f];            // bits 0..3 are 8 bit registers
   sample_q->num_cycles += count_ones_in_nibble[(pb >> 4) & 0x0f] * 2; // bits 4..7 are 16 bit registers
   if (sample_q->num_samples < sample_q->num_cycles) {
      sample_q->num_cycles = CYCLES_TRUNCATED;
      return;
   }
   int tmp;
   int i = (NM == 1) ? 3 : 4;
   if (pb & 0x01) {
      tmp = pop8(sample + i);
      i++;
      set_FLAGS(tmp);
   }
   if (pb & 0x02) {
      tmp = pop8(sample + i);
      i++;
      ACCA = tmp;
   }
   if (pb & 0x04) {
      tmp = pop8(sample + i);
      i++;
      ACCB = tmp;
   }
   if (pb & 0x08) {
      tmp = pop8(sample + i);
      i++;
      DP = tmp;
   }
   if (pb & 0x10) {
      tmp = pop16(sample + i);
      i += 2;
      X = tmp;
   }
   if (pb & 0x20) {
      tmp = pop16(sample + i);
      i += 2;
      Y = tmp;
   }
   if (pb & 0x40) {
      tmp = pop16(sample + i);
      i += 2;
      *us = tmp;
   }
   if (pb & 0x80) {
      tmp = pop16(sample + i);
      i += 2;
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
      set_NZ16(val);
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
      set_NZ(val);
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
   // it seems to be unchanged (i.e. no errors). Verified on 6309.
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
   // it seems to be unchanged (i.e. no errors). Verified on 6309.
}

static int xnc_helper(int val) {
   if (C == 0) {
      return neg_helper(val);
   } else if (C == 1) {
      return com_helper(val);
   } else {
      set_NZVC_unknown();
      return -1;
   }
}

static int xdec_helper(int val) {
   C = (val != 0);
   return dec_helper(val);
}

static int xclr_helper() {
   N = 0;
   Z = 1;
   V = 0;
   // Unlike CLR, C is unchanged
   return 0;
}

// ====================================================================
// Common 6809/6309 Instructions
// ====================================================================

static int op_fn_ABX(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   // X = X + B
   if (X >= 0 && ACCB >= 0) {
      // Here ABBC is treated as an 8-bit unsigned value
      X = (X + ACCB) & 0xffff;
   } else {
      X = -1;
   }
   return -1;
}

static int op_fn_ADCA(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   ACCA = add_helper(ACCA, C, operand);
   return -1;
}

static int op_fn_ADCB(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   ACCB = add_helper(ACCB, C, operand);
   return -1;
}

static int op_fn_ADDA(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   ACCA = add_helper(ACCA, 0, operand);
   return -1;
}

static int op_fn_ADDB(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   ACCB = add_helper(ACCB, 0, operand);
   return -1;
}

static int op_fn_ADDD(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   int D = pack(ACCA, ACCB);
   D = add16_helper(D, 0, operand);
   unpack(D, &ACCA, &ACCB);
   return -1;
}

static int op_fn_ANDA(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   ACCA = and_helper(ACCA, operand);
   return -1;
}

static int op_fn_ANDB(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   ACCB = and_helper(ACCB, operand);
   return -1;
}

static int op_fn_ANDC(operand_t operand, ea_t ea, sample_q_t *sample_q) {
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

static int op_fn_ASL(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   return asl_helper(operand);
}

static int op_fn_ASLA(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   ACCA = asl_helper(ACCA);
   return -1;
}

static int op_fn_ASLB(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   ACCB = asl_helper(ACCB);
   return -1;
}

static int op_fn_ASR(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   return asr_helper(operand);
}

static int op_fn_ASRA(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   ACCA = asr_helper(ACCA);
   return -1;
}

static int op_fn_ASRB(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   ACCB = asr_helper(ACCB);
   return -1;
}

// On a 6809 or a 6309 in emulated mode, a taken long branch adds one cycle
static inline void add_branch_taken_penalty(sample_q_t *sample_q) {
   if (NM !=1 && sample_q->sample->data == 0x10) {
      sample_q->num_cycles++;
   }
}

static int op_fn_BCC(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   if (C < 0) {
      PC = -1;
   } else if (C == 0) {
      PC = ea;
      add_branch_taken_penalty(sample_q);
   }
   return -1;
}

static int op_fn_BEQ(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   if (Z < 0) {
      PC = -1;
   } else if (Z == 1) {
      PC = ea;
      add_branch_taken_penalty(sample_q);
   }
   return -1;
}

static int op_fn_BGE(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   if (N < 0 || V < 0) {
      PC = -1;
   } else if (N == V) {
      PC = ea;
      add_branch_taken_penalty(sample_q);
   }
   return -1;
}

static int op_fn_BGT(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   if (Z < 0 || N < 0 || V < 0) {
      PC = -1;
   } else if (Z == 0 && N == V) {
      PC = ea;
      add_branch_taken_penalty(sample_q);
   }
   return -1;
}

static int op_fn_BHI(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   if (Z < 0 || C < 0) {
      PC = -1;
   } else if (Z == 0 && C == 0) {
      PC = ea;
      add_branch_taken_penalty(sample_q);
   }
   return -1;
}

static int op_fn_BITA(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   bit_helper(ACCA, operand);
   return -1;
}

static int op_fn_BITB(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   bit_helper(ACCB, operand);
   return -1;
}

static int op_fn_BLE(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   if (Z < 0 || N < 0 || V < 0) {
      PC = -1;
   } else if (Z == 1 || N != V) {
      PC = ea;
      add_branch_taken_penalty(sample_q);
   }
   return -1;
}

static int op_fn_BLO(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   if (C < 0) {
      PC = -1;
   } else if (C == 1) {
      PC = ea;
      add_branch_taken_penalty(sample_q);
   }
   return -1;
}

static int op_fn_BLS(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   if (Z < 0 || C < 0) {
      PC = -1;
   } else if (Z == 1 || C == 1) {
      PC = ea;
      add_branch_taken_penalty(sample_q);
   }
   return -1;
}

static int op_fn_BLT(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   if (N < 0 || V < 0) {
      PC = -1;
   } else if (N != V) {
      PC = ea;
      add_branch_taken_penalty(sample_q);
   }
   return -1;
}

static int op_fn_BMI(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   if (N < 0) {
      PC = -1;
   } else if (N == 1) {
      PC = ea;
      add_branch_taken_penalty(sample_q);
   }
   return -1;
}

static int op_fn_BNE(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   if (Z < 0) {
      PC = -1;
   } else if (Z == 0) {
      PC = ea;
      add_branch_taken_penalty(sample_q);
   }
   return -1;
}

static int op_fn_BPL(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   if (N < 0) {
      PC = -1;
   } else if (N == 0) {
      PC = ea;
      add_branch_taken_penalty(sample_q);
   }
   return -1;
}

static int op_fn_BRA(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   PC = ea;
   return -1;
}

static int op_fn_BRN(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   return -1;
}

static int op_fn_BSR(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   push16s(sample_q->sample + sample_q->num_cycles - 2);
   PC = ea;
   return -1;
}

static int op_fn_BVC(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   if (V < 0) {
      PC = -1;
   } else if (V == 0) {
      PC = ea;
      add_branch_taken_penalty(sample_q);
   }
   return -1;
}

static int op_fn_BVS(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   if (V < 0) {
      PC = -1;
   } else if (V == 1) {
      PC = ea;
      add_branch_taken_penalty(sample_q);
   }
   return -1;
}

static int op_fn_CLR(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   return clr_helper();
}

static int op_fn_CLRA(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   ACCA = clr_helper();
   return -1;
}

static int op_fn_CLRB(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   ACCB = clr_helper();
   return -1;
}

static int op_fn_CMPA(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   cmp_helper(ACCA, operand);
   return -1;
}

static int op_fn_CMPB(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   cmp_helper(ACCB, operand);
   return -1;
}

static int op_fn_CMPD(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   cmp16_helper(pack(ACCA, ACCB), operand);
   return -1;
}

static int op_fn_CMPS(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   cmp16_helper(S, operand);
   return -1;
}

static int op_fn_CMPU(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   cmp16_helper(U, operand);
   return -1;
}

static int op_fn_CMPX(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   cmp16_helper(X, operand);
   return -1;
}

static int op_fn_CMPY(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   cmp16_helper(Y, operand);
   return -1;
}

static int op_fn_COM(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   return com_helper(operand);
}

static int op_fn_COMA(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   ACCA = com_helper(ACCA);
   return -1;
}

static int op_fn_COMB(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   ACCB = com_helper(ACCB);
   return -1;
}

static int op_fn_CWAI(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   // CC = CC & immediate operand
   op_fn_ANDC(operand, ea, sample_q);

   // Look ahead for the vector fetch
   sample_t *sample = sample_q->sample;
   int num_samples = sample_q->num_samples;
   int num_cycles = CYCLES_UNKNOWN;
   // TODO: Handle running out of samples, e.g. 20220728a/random5-E.bin
   if (sample[0].bs >= 0) {
      for (int i = 1; i < num_samples; i++) {
         if (sample[i].bs == 1) {
            num_cycles = i + 3;
            break;
         }
      }
   }
   sample_q->num_cycles = num_cycles;

   // Look at the vector fetch to determine what event released CWAI
   int vec = sample[num_cycles - 3].addr;
   // Fallback to IRQ if addr is not being captured
   if (vec < 0) {
      vec = VEC_IRQ;
   }
   // The full state is always stacked
   interrupt_helper(sample_q, 4, 1, vec);
   return -1;
}

static int op_fn_DAA(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   if (ACCA >= 0 && H >= 0 && C >= 0) {
      int correction = 0x00;
      if (H == 1 || (ACCA & 0x0f) > 0x09) {
         correction |= 0x06;
      }
      if (C == 1 || (ACCA & 0xf0) > 0x90 || ((ACCA & 0xf0) > 0x80 && (ACCA & 0x0f) > 0x09)) {
         correction |= 0x60;
      }
      if (cpu6309 && C == 1 && ACCA >= 0x80 && ACCA <= 0x99) {

         // This difference from the 6809 is not widely known

         // C=1 H=0 A=80 => DAA => 6809:A=E0  6309:A=00
         // C=1 H=0 A=81 => DAA => 6809:A=E1  6309:A=01
         // C=1 H=0 A=82 => DAA => 6809:A=E2  6309:A=02
         // C=1 H=0 A=83 => DAA => 6809:A=E3  6309:A=03
         // C=1 H=0 A=84 => DAA => 6809:A=E4  6309:A=04
         // C=1 H=0 A=85 => DAA => 6809:A=E5  6309:A=05
         // C=1 H=0 A=86 => DAA => 6809:A=E6  6309:A=06
         // C=1 H=0 A=87 => DAA => 6809:A=E7  6309:A=07
         // C=1 H=0 A=88 => DAA => 6809:A=E8  6309:A=08
         // C=1 H=0 A=89 => DAA => 6809:A=E9  6309:A=09
         // C=1 H=0 A=8A => DAA => 6809:A=F0  6309:A=10
         // C=1 H=0 A=8B => DAA => 6809:A=F1  6309:A=11
         // C=1 H=0 A=8C => DAA => 6809:A=F2  6309:A=12
         // C=1 H=0 A=8D => DAA => 6809:A=F3  6309:A=13
         // C=1 H=0 A=8E => DAA => 6809:A=F4  6309:A=14
         // C=1 H=0 A=8F => DAA => 6809:A=F5  6309:A=15
         // C=1 H=0 A=90 => DAA => 6809:A=F0  6309:A=00
         // C=1 H=0 A=91 => DAA => 6809:A=F1  6309:A=01
         // C=1 H=0 A=92 => DAA => 6809:A=F2  6309:A=02
         // C=1 H=0 A=93 => DAA => 6809:A=F3  6309:A=03
         // C=1 H=0 A=94 => DAA => 6809:A=F4  6309:A=04
         // C=1 H=0 A=95 => DAA => 6809:A=F5  6309:A=05
         // C=1 H=0 A=96 => DAA => 6809:A=F6  6309:A=06
         // C=1 H=0 A=97 => DAA => 6809:A=F7  6309:A=07
         // C=1 H=0 A=98 => DAA => 6809:A=F8  6309:A=00
         // C=1 H=0 A=99 => DAA => 6809:A=F9  6309:A=01

         // C=1 H=1 A=80 => DAA => 6809:A=E6  6309:A=06
         // C=1 H=1 A=81 => DAA => 6809:A=E7  6309:A=07
         // C=1 H=1 A=82 => DAA => 6809:A=E8  6309:A=08
         // C=1 H=1 A=83 => DAA => 6809:A=E9  6309:A=09
         // C=1 H=1 A=84 => DAA => 6809:A=EA  6309:A=0A
         // C=1 H=1 A=85 => DAA => 6809:A=EB  6309:A=0B
         // C=1 H=1 A=86 => DAA => 6809:A=EC  6309:A=0C
         // C=1 H=1 A=87 => DAA => 6809:A=ED  6309:A=0D
         // C=1 H=1 A=88 => DAA => 6809:A=EE  6309:A=0E
         // C=1 H=1 A=89 => DAA => 6809:A=EF  6309:A=0F
         // C=1 H=1 A=8A => DAA => 6809:A=F0  6309:A=10
         // C=1 H=1 A=8B => DAA => 6809:A=F1  6309:A=11
         // C=1 H=1 A=8C => DAA => 6809:A=F2  6309:A=12
         // C=1 H=1 A=8D => DAA => 6809:A=F3  6309:A=13
         // C=1 H=1 A=8E => DAA => 6809:A=F4  6309:A=14
         // C=1 H=1 A=8F => DAA => 6809:A=F5  6309:A=15
         // C=1 H=1 A=90 => DAA => 6809:A=F6  6309:A=06
         // C=1 H=1 A=91 => DAA => 6809:A=F7  6309:A=07
         // C=1 H=1 A=92 => DAA => 6809:A=F8  6309:A=08
         // C=1 H=1 A=93 => DAA => 6809:A=F9  6309:A=09
         // C=1 H=1 A=94 => DAA => 6809:A=FA  6309:A=0A
         // C=1 H=1 A=95 => DAA => 6809:A=FB  6309:A=0B
         // C=1 H=1 A=96 => DAA => 6809:A=FC  6309:A=0C
         // C=1 H=1 A=97 => DAA => 6809:A=FD  6309:A=0D
         // C=1 H=1 A=98 => DAA => 6809:A=FE  6309:A=00
         // C=1 H=1 A=99 => DAA => 6809:A=FF  6309:A=00

         // I haven't worked out a pattern here, but this logic is correct on an exhaustive test

                                    // 6809 correction  6309 correction
         if (ACCA == 0x99) {
            correction = 0x68 - H;  // 0x60/0x66   ->   0x68/0x67
         } else if (ACCA == 0x98) {
            correction = 0x68;      // 0x60/0x66   ->   0x68/0x68
         } else if (ACCA >= 0x90) {
            correction += 0x10;     // 0x60/0x66   ->   0x70/0x76
         } else {
            correction += 0x20;     // 0x60/0x66   ->   0x80/0x86
         }
      }
      int tmp = ACCA + correction;
      // C is apparently only ever set by DAA, never cleared
      C |= (tmp >> 8) & 1;
      // V is is calculated as follows on both the 6809 and the 6309
      V = ((tmp >> 7) & 1) ^ C;
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

   // John's description of V above doesn't yeild the correct results

   return -1;
}

static int op_fn_DEC(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   return dec_helper(operand);
}

static int op_fn_DECA(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   ACCA = dec_helper(ACCA);
   return -1;
}

static int op_fn_DECB(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   ACCB = dec_helper(ACCB);
   return -1;
}

static int op_fn_EORA(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   ACCA = eor_helper(ACCA, operand);
   return -1;
}

static int op_fn_EORB(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   ACCB = eor_helper(ACCB, operand);
   return -1;
}

// Operand is the postbyte
static int op_fn_EXG(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   int reg1 = (operand >> 4) & 15;
   int reg2 = operand & 15;

   if (cpu6309) {
      int tmp1 = get_regp(reg1);
      int tmp2 = get_regp(reg2);
      set_regp(reg1, tmp2);
      set_regp(reg2, tmp1);
   } else {
      // According to Atkinson, page 66, there is a 6809 corner case where:
      //   EXC A,D
      // should behave as:
      //   EXC A,B
      // i.e. do A<==>B

      // According to David Flamand:
      //
      // Exchange is implemented as a triple transfer where:
      //   Source [3-0] -> TEMP
      //   Source [7-4] -> Destination [3-0]
      //           TEMP -> Destination [7-4]
      // The following rules apply to TFR and EXG instruction:
      //
      // 1. Transferring from an undefined register, the value $FF or
      // $FFFF is transferred depending on the destination register
      // size.
      //
      // 2. Transferring to an undefined register is a no operation.
      //
      // 3. Transferring to from a 16-bit to a 8-bit register, only
      // the source LSB is transferred.
      //
      // 4. Transferring from A or B or TEMP to a 16-bit register, the
      // source is transferred into LSB, MSB is set to $FF.
      //
      // 5. Transferring from CC or DP to a 16-bit register, the
      // source is transferred into LSB and MSB.
      //
      // That is EXG X,DP and EXG DP,X give different result. (rule
      // 3,4 and 5,3 respectively)

      // Examples: EXG reg1, reg2
      // (reg1 = [7:4] and reg2 = [3:0])

      // EXG X, DP
      //   DP -> temp (temp = DPDP )
      //    X -> DP      DP =   XL )
      // temp -> X        X = FFDP )

      // EXG DP, X
      //    X -> temp (temp = XHXL )
      //   DP -> X    (X    = DPDP )
      // temp -> DP   (DP   =   XL )

      // EXG A, D
      //    D -> temp (temp = AABB )
      //    A -> D    (   D = FFAA )
      // temp -> A    (   A = BB   )

      int tmp = get_regp(reg2);
      set_regp(reg2, get_regp(reg1));
      // Special case reg2 (8 bits) => reg1 (16 bits)
      if (tmp >= 0 && reg2 >= 8 && reg1 < 8) {
         tmp |= 0xFF00;
      }
      set_regp(reg1, tmp);
   }
   return -1;
}

static int op_fn_INC(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   return inc_helper(operand);
}

static int op_fn_INCA(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   ACCA = inc_helper(ACCA);
   return -1;
}

static int op_fn_INCB(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   ACCB = inc_helper(ACCB);
   return -1;
}

static int op_fn_JMP(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   PC = ea;
   return -1;
}

static int op_fn_JSR(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   push16s(sample_q->sample + sample_q->num_cycles - 2);
   PC = ea;
   return -1;
}

static int op_fn_LDA(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   ACCA = ld_helper(operand);
   return -1;
}

static int op_fn_LDB(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   ACCB = ld_helper(operand);
   return -1;
}

static int op_fn_LDD(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   int tmp = ld16_helper(operand);
   ACCA = (tmp >> 8) & 0xff;
   ACCB = tmp & 0xff;
   return -1;
}

static int op_fn_LDS(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   S = ld16_helper(operand);
   return -1;
}

static int op_fn_LDU(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   U = ld16_helper(operand);
   return -1;
}

static int op_fn_LDX(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   X = ld16_helper(operand);
   return -1;
}

static int op_fn_LDY(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   Y = ld16_helper(operand);
   return -1;
}

static int op_fn_LEAS(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   S = ea;
   return -1;
}

static int op_fn_LEAU(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   U = ea;
   return -1;
}

static int op_fn_LEAX(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   X = ea;
   Z = (X == 0);
   return -1;
}

static int op_fn_LEAY(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   Y = ea;
   Z = (Y == 0);
   return -1;
}

static int op_fn_LSR(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   return lsr_helper(operand);
}

static int op_fn_LSRA(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   ACCA = lsr_helper(ACCA);
   return -1;
}

static int op_fn_LSRB(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   ACCB = lsr_helper(ACCB);
   return -1;
}

static int op_fn_MUL(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   // M register
   M = ACCB;
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

static int op_fn_NEG(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   return neg_helper(operand);
}

static int op_fn_NEGA(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   ACCA = neg_helper(ACCA);
   return -1;
}

static int op_fn_NEGB(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   ACCB = neg_helper(ACCB);
   return -1;
}

static int op_fn_NOP(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   return -1;
}

static int op_fn_ORA(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   ACCA = or_helper(ACCA, operand);
   return -1;
}

static int op_fn_ORB(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   ACCB = or_helper(ACCB, operand);
   return -1;
}

static int op_fn_ORCC(operand_t operand, ea_t ea, sample_q_t *sample_q) {
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

static int op_fn_PSHS(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   push_helper(sample_q, 1); // 1 = PSHS
   return -1;
}

static int op_fn_PSHU(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   push_helper(sample_q, 0); // 0 = PSHU
   return -1;
}
static int op_fn_PULS(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   pull_helper(sample_q, 1); // 1 = PULS
   return -1;
}

static int op_fn_PULU(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   pull_helper(sample_q, 0); // 0 = PULU
   return -1;
}

static int op_fn_ROL(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   return rol_helper(operand);
}

static int op_fn_ROLA(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   ACCA = rol_helper(ACCA);
   return -1;
}

static int op_fn_ROLB(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   ACCB = rol_helper(ACCB);
   return -1;
}

static int op_fn_ROR(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   return ror_helper(operand);
}

static int op_fn_RORA(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   ACCA = ror_helper(ACCA);
   return -1;
}

static int op_fn_RORB(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   ACCB = ror_helper(ACCB);
   return -1;
}

static int op_fn_RTI(operand_t operand, ea_t ea, sample_q_t *sample_q) {
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
   sample_t *sample = sample_q->sample + sample_q->oi;

   int i = 2;

   // Do the flags first, as the stacked E indicates how much to restore
   set_FLAGS(sample[i++].data);

   // Update the register state
   if (E == 1) {
      ACCA = sample[i++].data;
      ACCB = sample[i++].data;
      if (NM == 1) {
         ACCE = sample[i++].data;
         ACCF = sample[i++].data;
      }
      DP = sample[i++].data;
      X  = sample[i++].data << 8;
      X |= sample[i++].data;
      Y  = sample[i++].data << 8;
      Y |= sample[i++].data;
      U  = sample[i++].data << 8;
      U |= sample[i++].data;
      // RTI takes 9 additional cycles if E = 1 (and two more if in native mode)
      sample_q->num_cycles += (NM == 1) ? 11 : 9;
   }
   PC  = sample[i++].data << 8;
   PC |= sample[i++].data;

   // Memory modelling
   for (int j = 2; j < i; j++) {
      pop8s(sample + j);
   }
   return -1;
}

static int op_fn_RTS(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   sample_t *sample = sample_q->sample + sample_q->oi;
   PC = pop16s(sample + 2);
   return -1;
}

static int op_fn_SBCA(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   ACCA = sub_helper(ACCA, C, operand);
   return -1;
}

static int op_fn_SBCB(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   ACCB = sub_helper(ACCB, C, operand);
   return -1;
}

static int op_fn_SEX(operand_t operand, ea_t ea, sample_q_t *sample_q) {
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
   // Tests show V is not cleared (contrary to some documentation)
   return -1;
}

static int op_fn_STA(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   ACCA = st_helper(ACCA, operand, FAIL_ACCA);
   return operand;
}

static int op_fn_STB(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   ACCB = st_helper(ACCB, operand, FAIL_ACCB);
   return operand;
}

static int op_fn_STD(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   // Do a bit more work here to correctly set the failute code
   int fail = 0;
   if (ACCA >= 0 && ACCA != ((operand >> 8) & 0xff)) {
      fail |= FAIL_ACCA;
   }
   if (ACCB >= 0 && ACCB != (operand & 0xff)) {
      fail |= FAIL_ACCB;
   }
   int D = pack(ACCA, ACCB);
   D = st16_helper(D, operand, fail);
   unpack(D, &ACCA, &ACCB);
   return operand;
}

static int op_fn_STS(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   S = st16_helper(S, operand, FAIL_S);
   return operand;
}

static int op_fn_STU(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   U = st16_helper(U, operand, FAIL_U);
   return operand;
}

static int op_fn_STX(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   X = st16_helper(X, operand, FAIL_X);
   return operand;
}

static int op_fn_STY(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   Y = st16_helper(Y, operand, FAIL_Y);
   return operand;
}

static int op_fn_SUBA(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   ACCA = sub_helper(ACCA, 0, operand);
   return -1;
}

static int op_fn_SUBB(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   ACCB = sub_helper(ACCB, 0, operand);
   return -1;
}

static int op_fn_SUBD(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   int D = pack(ACCA, ACCB);
   D = sub16_helper(D, 0, operand);
   unpack(D, &ACCA, &ACCB);
   return -1;
}


static int op_fn_SWI(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   interrupt_helper(sample_q, 3, 1, VEC_SWI);
   return -1;
}

static int op_fn_SWI2(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   interrupt_helper(sample_q, 3, 1, VEC_SWI2);
   return -1;
}

static int op_fn_SWI3(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   interrupt_helper(sample_q, 3, 1, VEC_SWI3);
   return -1;
}

static int op_fn_SYNC(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   // SYNC, look ahead for sync acknowledge
   sample_t *sample = sample_q->sample;
   int num_samples = sample_q->num_samples;
   int num_cycles = CYCLES_UNKNOWN;
   // TODO: Handle running out of samples
   if (sample[0].ba >= 0) {
      int i = sample_q->oi + 2;
      // Look for Sync acknowledge ending (BA = 0) or the address bus changing
      while (i < num_samples && sample[i].ba == 1) {
         i++;
      }
      num_cycles = i + 1;
   }
   sample_q->num_cycles = num_cycles;
   return -1;
}

// Operand is the postbyte
static int op_fn_TFR(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   int reg1 = (operand >> 4) & 15;
   int reg2 = operand  & 15;
   set_regp(reg2, get_regp(reg1));
   return -1;
}

static int op_fn_TST(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   set_NZ(operand);
   V = 0;
   return -1;
}

static int op_fn_TSTA(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   set_NZ(ACCA);
   V = 0;
   return -1;
}

static int op_fn_TSTB(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   set_NZ(ACCB);
   V = 0;
   return -1;
}

static int op_fn_UU(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   return -1;
}

// ====================================================================
// Undocumented Instructions
// ====================================================================

// Much of the information on the undocumented instructions comes from here:
// https://colorcomputerarchive.com/repo/Documents/Books/Motorola%206809%20and%20Hitachi%206309%20Programming%20Reference%20(Darren%20Atkinson).pdf

static int op_fn_XX(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   return -1;
}

// Undefined opcodes in row 2 execute as a NEG instruction when the
// Carry bit in CC is 0, and as a COM instruction when the Carry bit
// is 1

static int op_fn_XNC(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   return xnc_helper(operand);
}

static int op_fn_XNCA(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   ACCA = xnc_helper(ACCA);
   return -1;
}

static int op_fn_XNCB(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   ACCB = xnc_helper(ACCB);
   return -1;
}

// Opcodes $14, $15 and $CD all cause the CPU to stop functioning
// normally. One or more of these may be the HCF (Halt and Catch Fire)
// instruction. The HCF instruction was provided for manufacturing
// test purposes. Its causes the CPU to halt execution and enter a
// mode in which the Address lines are incrementally strobed.

static int op_fn_XHCF(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   sample_t *samples = sample_q->sample;
   // Ignore the opcode
   int i = sample_q->oi + 1;
   if (PC >= 0) {
      // Set back the PC to the start of the instruction, so HCF re-executes
      PC = (PC - i) & 0xffff;
   }
   // Read 64K - i bytes starting at PC + i
   while (i < 0x10000) {
      int addr = (PC >= 0) ? ((PC + i) & 0xffff) : -1;
      memory_read(samples + i, addr, MEM_DATA);
      i++;
   }
   // Force the number of cycles to 64K
   sample_q->num_cycles = 0x10000;
   return -1;
}

// Opcode $18 affects only the Condition Codes register (CC).
//
// This behaviour was discovered by David Flamand
//
// Left Shift And CC (LSACC), take 3 cycles.
// CC.E' = (CC.F & B6)
// CC.F' = (CC.H & B5)
// CC.H' = (CC.I & B4)
// CC.I' = (CC.N & B3)
// CC.N' = (CC.Z & B2)
// CC.Z' = (CC.V & B1)
// CC.V' = (CC.C & B0) | (CC.Z & B2)
// CC.C' = 0
// B0 to B6 are the bits from the byte following this opcode.
//
// The previous understanding was always tested with the following
// instruction being an NOP (0x12), so all bits apart from H and Z
// were cleared.
//
// Execution of this opcode takes 3 MPU cycles.

static int op_fn_X18(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   uint8_t b = (sample_q->sample + sample_q->oi + 1)->data;
   int tmp1 = (b & 0x01) ? C : 0;
   int tmp2 = (b & 0x04) ? Z : 0;
   E = (b & 0x40) ? F : 0; // Bit 7
   F = (b & 0x20) ? H : 0;
   H = (b & 0x10) ? I : 0;
   I = (b & 0x08) ? N : 0;
   N = (b & 0x04) ? Z : 0;
   Z = (b & 0x02) ? V : 0;
   V = (tmp1 == 1 || tmp2 == 1) ? 1 : (tmp1 == 0 && tmp2 == 0) ? 0 : -1;
   C = 0;                  // Bit 0
   return -1;
}

// Opcodes $87 and $C7 read and discard an 8-bit Immediate operand
// which follows the opcode. The value of the immediate byte is
// irrelevant. The Negative bit (N) in the CC register is always set,
// while the Zero (Z) and Overflow (V) bits are always cleared. No
// other bits in the Condition Codes register are affected. Each of
// these opcodes execute in 2 MPU cycles.

static int op_fn_X8C7(operand_t operand, ea_t ea, sample_q_t *sample_q) {
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
//
// DMB: $10 $3E invokes the SWI2 trap (so is the same as SWI2)
//      $11 $3e invokes the FIQ trap

static int op_fn_XRES(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   interrupt_helper(sample_q, 3, 1, VEC_XRST);
   return -1;
}

static int op_fn_XFIQ(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   interrupt_helper(sample_q, 3, 1, VEC_FIQ);
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

static int op_fn_XSTS(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   N = 1;
   Z = 0;
   V = 0;
   return S & 0xff;
}

static int op_fn_XSTU(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   N = 1;
   Z = 0;
   V = 0;
   return U & 0xff;
}

static int op_fn_XSTX(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   N = 1;
   Z = 0;
   V = 0;
   return X & 0xff;
}

static int op_fn_XSTY(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   N = 1;
   Z = 0;
   V = 0;
   return Y & 0xff;
}

static int op_fn_XCLRA(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   ACCA = xclr_helper(ACCA);
   return -1;
}

static int op_fn_XCLRB(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   ACCB = xclr_helper(ACCB);
   return -1;
}

static int op_fn_XDEC(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   return xdec_helper(operand);
}

static int op_fn_XDECA(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   ACCA = xdec_helper(ACCA);
   return -1;
}

static int op_fn_XDECB(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   ACCB = xdec_helper(ACCB);
   return -1;
}

static int op_fn_XADDD(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   add16_helper(pack(ACCA, ACCB), 0, operand);
   return -1;
}

static int op_fn_XADDU(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   // This one took some figuring out!
   int tmpU = (U >= 0) ? (U | 0xff00) : -1;
   add16_helper(tmpU, 0, operand);
   return -1;
}

// ====================================================================
// 6309 Helpers
// ====================================================================


// The value of the PC seen in the register addressing is one cycle
// ahead due to pipelining (even in emulation mode)
static inline int get_r_pc() {
   return PC < 0 ? -1 : (PC + 1) & 0xFFFF;
}

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
      case  5: ret = get_r_pc() & 0xff;           break;
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
      case  5: ret = get_r_pc();                  break;
      case  6: ret = pack(ACCE, ACCF);            break;
      case  7: ret = TV;                          break;
         // src is 8 bits, promote to 16 bits
      case  8: ret = pack(ACCA, ACCB);            break;
      case  9: ret = pack(ACCA, ACCB);            break;
      case 10: ret = get_FLAGS();                 break;
      case 11: ret = pack(DP, M);                 break;
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
   case  5: ret = get_r_pc();                  break;
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
//
// The register addressing mode instructions cannot safely write to
// the PC. What seem to happen in practice is the PC is updated on the
// 2nd byte of the next instruction.
static void set_r1(int pb, int val) {
   int dst = pb & 0xf;
   switch(dst) {
   case  0: unpack(val, &ACCA, &ACCB);   break;
   case  1: X  = val;                    break;
   case  2: Y  = val;                    break;
   case  3: U  = val;                    break;
   case  4: S  = val;                    break;
   case  5: PC = -1; async_pc_write = 1; break;
   case  6: unpack(val, &ACCE, &ACCF);   break;
   case  7: TV = val;                    break;
   case  8: ACCA = val;                  break;
   case  9: ACCB = val;                  break;
   case 10: set_FLAGS(val);              break;
   case 11: DP = val;                    break;
   case 14: ACCE = val;                  break;
   case 15: ACCF = val;                  break;
   }
}

static void set_Arithmetic_R_result(operand_t operand, int result) {
   // See page 143 of Atkinson
   int tmpC = C;
   int tmpN = N;
   int tmpV = V;
   int tmpZ = Z;
   C = 0;
   N = 0;
   V = 0;
   Z = 0;
   set_r1(operand, result);
   if (tmpC == 1) {
      C = 1;
   }
   if (tmpN == 1) {
      N = 1;
   }
   if (tmpV == 1) {
      V = 1;
   }
   if (tmpZ == 1) {
      Z = 1;
   }
}

static void set_Logical_R_result(operand_t operand, int result) {
   // See page 143 of Atkinson
   int tmpN = N;
   int tmpV = V;
   int tmpZ = Z;
   N = 0;
   V = 0;
   Z = 0;
   set_r1(operand, result);
   if (tmpN == 1) {
      N = 1;
   }
   if (tmpV == 1) {
      V = 1;
   }
   if (tmpZ == 1) {
      Z = 1;
   }
}

static void directbit_helper(operand_t operand, sample_q_t *sample_q) {
   // Pickout the opcode and the postbyte from the samples
   sample_t *sample = sample_q->sample + sample_q->oi;
   int opcode = sample[0].data;
   int postbyte = sample[1].data;

   // Parse the post byte
   int reg_num    = (postbyte >> 6) & 3; // Bits 7..6

   // A destination regnum of 3 is (as far as we know) a NOP
   if (reg_num == 3) {
      failflag |= FAIL_UNDOC;
      return;
   }

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
   N    = (val >> 31) & 1;
   // 6309 bug: Z is set based only on the top 16 bits
   Z    = (ACCA == 0 && ACCB == 0);
}

static void set_q_nz_unknown() {
   ACCA = -1;
   ACCB = -1;
   ACCE = -1;
   ACCF = -1;
   N    = -1;
   Z    = -1;
}

static void pushw_helper(sample_q_t *sample_q, int system) {
   sample_t *sample = sample_q->sample + sample_q->oi;
   int (*push8)(sample_t *) = system ? push8s : push8u;
   int f = push8(sample + 3);
   if (ACCF >= 0 && ACCF != f) {
      failflag |= FAIL_ACCF;
   }
   ACCF = f;
   int e = push8(sample + 4);
   if (ACCE >= 0 && ACCE != e) {
      failflag |= FAIL_ACCE;
   }
   ACCE = e;
}

static void pullw_helper(sample_q_t *sample_q, int system) {
   sample_t *sample = sample_q->sample + sample_q->oi;
   int (*pop8)(sample_t *) = system ? pop8s : pop8u;
   ACCE = pop8(sample + 3);
   ACCF = pop8(sample + 4);
}

// ====================================================================
// 6309 Instructions
// ====================================================================

static int op_fn_ADCD(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   int D = pack(ACCA, ACCB);
   D = add16_helper(D, C, operand);
   unpack(D, &ACCA, &ACCB);
   return -1;
}

static int op_fn_ADCR(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   // r1 := r1 + r0 + C
   int r0 = get_r0(operand);
   int r1 = get_r1(operand);
   int result;
   // Save H, as it's not affected by ADDR
   int tmpH = H;
   if ((operand & 0x0f) < 8) {
      result = add16_helper(r1, C, r0);
   } else {
      result = add_helper(r1, C, r0);
   }
   // Restore H
   H = tmpH;
   set_Arithmetic_R_result(operand, result);
   return -1;
}

static int op_fn_ADDE(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   ACCE = add_helper(ACCE, 0, operand);
   return -1;
}

static int op_fn_ADDF(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   ACCF = add_helper(ACCF, 0, operand);
   return -1;
}

static int op_fn_ADDR(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   // r1 := r1 + r0
   int r0 = get_r0(operand);
   int r1 = get_r1(operand);
   int result;
   // Save H, as it's not affected by ADDR
   int tmpH = H;
   if ((operand & 0x0f) < 8) {
      result = add16_helper(r1, 0, r0);
   } else {
      result = add_helper(r1, 0, r0);
   }
   // Restore H
   H = tmpH;
   set_Arithmetic_R_result(operand, result);
   return -1;
}

static int op_fn_ADDW(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   int W = pack(ACCE, ACCF);
   W = add16_helper(W, 0, operand);
   unpack(W, &ACCE, &ACCF);
   return -1;
}

static int op_fn_AIM(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   // oi points to the immediate byte
   sample_t *sample = sample_q->sample + sample_q->oi;
   return and_helper(operand, sample->data);
}

static int op_fn_ANDD(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   int D = pack(ACCA, ACCB);
   D = and16_helper(D, operand);
   unpack(D, &ACCA, &ACCB);
   return -1;
}

static int op_fn_ANDR(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   // r1 := r1 & r0
   int r0 = get_r0(operand);
   int r1 = get_r1(operand);
   int result;
   if ((operand & 0x0f) < 8) {
      result = and16_helper(r1, r0);
   } else {
      result = and_helper(r1, r0);
   }
   set_Logical_R_result(operand, result);
   return -1;
}

static int op_fn_ASLD(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   int D = pack(ACCA, ACCB);
   D = asl16_helper(D);
   unpack(D, &ACCA, &ACCB);
   return -1;
}

static int op_fn_ASRD(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   int D = pack(ACCA, ACCB);
   D = asr16_helper(D);
   unpack(D, &ACCA, &ACCB);
   return -1;
}

static int op_fn_BAND(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   directbit_helper(operand, sample_q);
   return -1;
}

static int op_fn_BEOR(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   directbit_helper(operand, sample_q);
   return -1;
}

static int op_fn_BIAND(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   directbit_helper(operand, sample_q);
   return -1;
}

static int op_fn_BIEOR(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   directbit_helper(operand, sample_q);
   return -1;
}

static int op_fn_BIOR(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   directbit_helper(operand, sample_q);
   return -1;
}

static int op_fn_BITD(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   int D = pack(ACCA, ACCB);
   bit16_helper(D, operand);
   return -1;
}

static int op_fn_BITMD(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   // M register
   M = operand;
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

static int op_fn_BOR(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   directbit_helper(operand, sample_q);
   return -1;
}

static int op_fn_CLRD(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   ACCA = ACCB = clr_helper();
   return -1;
}

static int op_fn_CLRE(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   ACCE = clr_helper();
   return -1;
}

static int op_fn_CLRF(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   ACCF = clr_helper();
   return -1;
}

static int op_fn_CLRW(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   ACCE = ACCF = clr_helper();
   return -1;
}

static int op_fn_CMPE(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   cmp_helper(ACCE, operand);
   return -1;
}

static int op_fn_CMPF(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   cmp_helper(ACCF, operand);
   return -1;
}

static int op_fn_CMPR(operand_t operand, ea_t ea, sample_q_t *sample_q) {
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

static int op_fn_CMPW(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   cmp16_helper(pack(ACCE, ACCF), operand);
   return -1;
}

static int op_fn_COMD(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   int D = pack(ACCA, ACCB);
   D = com16_helper(D);
   unpack(D, &ACCA, &ACCB);
   return -1;
}

static int op_fn_COME(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   ACCE = com_helper(ACCE);
   return -1;
}

static int op_fn_COMF(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   ACCF = com_helper(ACCF);
   return -1;
}

static int op_fn_COMW(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   int W = pack(ACCE, ACCF);
   W = com16_helper(W);
   unpack(W, &ACCE, &ACCF);
   return -1;
}

static int op_fn_DECD(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   int D = pack(ACCA, ACCB);
   D = dec16_helper(D);
   unpack(D, &ACCA, &ACCB);
   return -1;
}

static int op_fn_DECE(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   ACCE = dec_helper(ACCE);
   return -1;
}

static int op_fn_DECF(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   ACCF = dec_helper(ACCF);
   return -1;
}

static int op_fn_DECW(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   int W = pack(ACCE, ACCF);
   W = dec16_helper(W);
   unpack(W, &ACCE, &ACCF);
   return -1;
}

static int op_fn_DIVD(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   // The cycle times vary depending on the values of the dividend and divisor
   //
   // e.g. DIVD &81 in Native mode, with &81 containing &10
   //
   // D=0000->07FF (no overflow)                  26 cycles
   // D=0800->0FFF (twos-complement overflow)     25 cycles
   // D=1000->7FFF (range overflow)               13 cycles
   // D=8000->F000 (range overflow)               14 cycles
   // D=F001->F800 (twos-complement overflow)     26 cycles
   // D=F801->FFFF (no overflow)                  27 cycles
   //
   // e.g. DIVD &81 in Native mode, with &81 containing &f0
   //
   // D=0000->07FF (no overflow)                  27 cycles
   // D=0800->0FFF (twos-complement overflow)     26 cycles
   // D=1000->7FFF (range overflow)               14 cycles
   // D=8000->F000 (range overflow)               15 cycles
   // D=F001->F800 (twos-complement overflow)     27 cycles
   // D=F801->FFFF (no overflow)                  28 cycles
   //
   // The rule for dynamic part of the cycle count seems to be:
   //
   // +1 if dividend is negative
   // +1 if divisor is negative
   // -1 if twos-complement overflow
   // -13 if range overflow

   int trap = 0;
   int cycle_correction = 0;
   int signq = 0; // Set if the quotient will be negative
   int signr = 0; // Set if the remainder will be negative

   if (operand == 0) {
      // It seems Z is set in this case
      Z = 1;
      N = 0;
      V = 0;
      // Cycle correction
      if (NM != 1) {
         cycle_correction -= 2; // 27 to 25
      }
      trap = 1;
   } else if (ACCA < 0 || ACCB < 0) {
      ACCA = -1;
      ACCB = -1;
      set_NZVC_unknown();
   } else {
      int a = (ACCA << 8) + ACCB; // 0x0000-0xFFFF
      int b = operand;            // 0x00-0xFF
      // Twos-complement a negative dividend
      if (a >= 0x8000) {
         a = 0x10000 - a;
         signq ^= 1;
         signr ^= 1; // Sign of a non-zero remainder matches the sign of the dividend
         // There is a one cycle penatly here
         cycle_correction++;
      }
      // Twos-complement a negative divisor
      if (b >= 0x80) {
         b = 0x100 - b;
         signq ^= 1;
         // There is a one cycle penatly here
         cycle_correction++;
      }
      // Do the division using positive numbers
      int quotient  = a / b;
      int remainder = a % b;
      if (quotient > 255) {
         // A range overflow has occurred
         V = 1;
         C = 0;
         Z = 0;
         // Undocumented: D = dividend magnitude, N = dividend sign
         N = signr;
         ACCA = (a >> 8) & 0xff;
         ACCB =  a       & 0xff;
         cycle_correction -= 13;
      } else {
         // Handle the remainder...
         if (remainder > 0 && signr) {
            // The remainer sign correction happens regarless of two-complement overflow
            remainder = 0x100 - remainder;
         }
         ACCA = remainder & 0xff;
         // Handle the quotient...
         C = quotient & 1;
         if (quotient > 127) {
            // A two-complement overflow has occurred, set overflow
            V = 1;
            cycle_correction -= 1;
         } else {
            // The quotient is valid, clear overflow
            V = 0;
            // The quotient sign correction only happens in this case
            if (quotient > 0 && signq) {
               quotient = 0x100 - quotient;
            }
         }
         ACCB = quotient & 0xff;
         set_NZ(ACCB);
      }
   }
   // Correct the number of cycles
   sample_q->num_cycles += cycle_correction;
   // Throw a trap if division by zero
   if (trap) {
      // Set bit 0 of the vector to differentiate DZ Trap from IL Trap
      interrupt_helper(sample_q, sample_q->num_cycles - ((NM == 1) ? 18 : 16) - sample_q->oi, 1, VEC_DZ);
   }
   // M register
   M = signq ? 0xff : 0x00;
   return -1;
}

static int op_fn_DIVQ(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   // The cycle times vary depending on the values of the dividend and divisor
   //
   // The rule for dynamic part of the cycle count seems to be:
   //
   // +1 if dividend is negative
   // +1 if divisor is negative
   // -21 if range overflow
   // -8 if there is a division by zero

   int cycle_correction = 0;
   int trap = 0;
   int signq = 0; // Set if the quotient will be negative
   int signr = 0; // Set if the remainder will be negative

   if (operand == 0) {
      // It seems Z is set in this case
      Z = 1;
      N = 0;
      V = 0;
      if (NM == 1) {
         cycle_correction -= 8; // 35 to 27
      } else {
         cycle_correction -= 10; // 36 to 26
      }
      trap = 1;
   } else if (ACCA < 0 || ACCB < 0 || ACCE < 0 || ACCF < 0) {
      ACCA = -1;
      ACCB = -1;
      ACCE = -1;
      ACCF = -1;
      set_NZVC_unknown();
   } else {
      uint32_t a = (ACCA << 24) + (ACCB << 16) + (ACCE << 8) + ACCF; // 0x00000000-0xFFFFFFFF
      uint32_t b = operand; // 0x0000-0xFFFF
      // Twos-complement a negative dividend
      if (a >= 0x80000000) {
         a = - a;
         signq ^= 1;
         signr ^= 1; // Sign of a non-zero remainder matches the sign of the dividend
         // There is a one cycle penatly here
         cycle_correction++;
      }
      // Twos-complement a negative divisor
      if (b >= 0x8000) {
         b = 0x10000 - b;
         signq ^= 1;
         // There is a one cycle penatly here
         cycle_correction++;
      }
      // Do the division using positive numbers
      uint32_t quotient  = a / b;
      uint32_t remainder = a % b;
      if (quotient > 65535) {
         // A range overflow has occurred
         V = 1;
         C = 0;
         Z = 0;
         // Undocumented: D = dividend magnitude, N = dividend sign
         N = signr;
         ACCA = (a >> 24) & 0xff;
         ACCB = (a >> 16) & 0xff;
         ACCE = (a >>  8) & 0xff;
         ACCF =  a        & 0xff;
         cycle_correction -= 21;
      } else {
         // Handle the remainder...
         if (remainder > 0 && signr) {
            // The remainer sign correction happens regarless of two-complement overflow
            remainder = 0x10000 - remainder;
         }
         ACCA = (remainder >> 8) & 0xff;
         ACCB =  remainder       & 0xff;
         // Handle the quotient...
         C = quotient & 1;
         if (quotient > 32767) {
            // A two-complement overflow has occurred, set overflow
            V = 1;
            // Note: Unlike DIVD, no cycle is saved in this case
         } else {
            // The quotient is valid, clear overflow
            V = 0;
            // The quotient sign correction only happens in this case
            if (quotient > 0 && signq) {
               quotient = 0x10000 - quotient;
            }
         }
         ACCE = (quotient >> 8) & 0xff;
         ACCF =  quotient       & 0xff;
         set_NZ16(quotient);
      }
   }
   // Correct the number of cycles
   sample_q->num_cycles += cycle_correction;

   // Throw a trap if division by zero
   if (trap) {
      // Set bit 0 of the vector to differentiate DZ Trap from IL Trap
      interrupt_helper(sample_q, sample_q->num_cycles - ((NM == 1) ? 18 : 16) - sample_q->oi, 1, VEC_DZ);
   }
   // M register
   M = signq ? 0xff : 0x00;
   return -1;
}

static int op_fn_EIM(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   // oi points to the immediate byte
   sample_t *sample = sample_q->sample + sample_q->oi;
   return eor_helper(operand, sample->data);
}

static int op_fn_EORD(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   int D = pack(ACCA, ACCB);
   D = eor16_helper(D, operand);
   unpack(D, &ACCA, &ACCB);
   return -1;
}

static int op_fn_EORR(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   // r1 := r1 ^ r0
   int r0 = get_r0(operand);
   int r1 = get_r1(operand);
   int result;
   if ((operand & 0x0f) < 8) {
      result = eor16_helper(r1, r0);
   } else {
      result = eor_helper(r1, r0);
   }
   set_Logical_R_result(operand, result);
   return -1;
}

static int op_fn_INCD(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   int D = pack(ACCA, ACCB);
   D  = inc16_helper(D);
   unpack(D, &ACCA, &ACCB);
   return -1;
}

static int op_fn_INCE(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   ACCE = inc_helper(ACCE);
   return -1;
}

static int op_fn_INCF(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   ACCF = inc_helper(ACCF);
   return -1;
}

static int op_fn_INCW(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   int W = pack(ACCE, ACCF);
   W = inc16_helper(W);
   unpack(W, &ACCE, &ACCF);
   return -1;
}

static int op_fn_LDBT(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   directbit_helper(operand, sample_q);
   return -1;
}

static int op_fn_LDE(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   ACCE = ld_helper(operand);
   return -1;
}

static int op_fn_LDF(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   ACCF = ld_helper(operand);
   return -1;
}

static int op_fn_LDMD(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   // The instruction cycle count of LDMD is always 5 cycles. However,
   // when NM toggles, the position of LIC moves, so the cycle count
   // inferred from LIC needs to be adjusted. Otherwise we would see
   // false cycle count errors. Note: emulate() disables the LIC based
   // cycle count check for LDMD.
   if (sample_q->sample->lic >= 0 && NM >= 0) {
      int correction = NM - (operand & 1);
      if (sample_q->num_cycles != count_cycles_with_lic(sample_q) + correction) {
         failflag |= FAIL_CYCLES;
      }
   }
   FM = (operand >> 1) & 1;
   NM =  operand       & 1;
   return -1;
}

static int op_fn_LDQ(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   set_q_nz((uint32_t) operand);
   // Random testing showed V is not cleared
   // V = 0;
   return -1;
}

static int op_fn_LDW(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   int W = ld16_helper(operand);
   unpack(W, &ACCE, &ACCF);
   return -1;
}

static int op_fn_LSRD(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   int D = pack(ACCA, ACCB);
   D = lsr_helper(D);
   unpack(D, &ACCA, &ACCB);
   return -1;
}

static int op_fn_LSRW(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   int W = pack(ACCE, ACCF);
   W = lsr_helper(W);
   unpack(W, &ACCE, &ACCF);
   return -1;
}

static int op_fn_MULD(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   // Q = D * imm16 (signed)
   int cycle_correction = 0;
   int sign = 0; // Set if the result will be negative
   if (ACCA >= 0 && ACCB >= 0) {
      int a = (ACCA << 8) + ACCB; // 0x0000-0xFFFF
      int b = operand;            // 0x0000-0xFFFF
      // Twos-complement the first operand
      if (a >= 0x8000) {
         a = 0x10000 - a;
         sign ^= 1;
         // There is a one cycle penatly here
         cycle_correction++;
      }
      // Twos-complement the second operand
      if (b >= 0x8000) {
         b = 0x10000 - b;
         sign ^= 1;
         // There is a one cycle penatly here
         cycle_correction++;
      }
      // This cannot overflow because the range of a,b is 0x0000-0x7FFF
      uint32_t result = (uint32_t)(a * b);
      if (sign) {
         result = -result;
         // There is a one cycle penatly here
         cycle_correction++;
      }
      set_q_nz(result);
   } else {
      set_q_nz_unknown();
   }
   // Correct the number of cycles
   sample_q->num_cycles += cycle_correction;
   // M register
   M = sign ? 0xff : 0x00;
   return -1;
}

static int op_fn_NEGD(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   int D = pack(ACCA, ACCB);
   D = neg16_helper(D);
   unpack(D, &ACCA, &ACCB);
   return -1;
}

static int op_fn_OIM(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   // oi points to the immediate byte
   sample_t *sample = sample_q->sample + sample_q->oi;
   return or_helper(operand, sample->data);
}

static int op_fn_ORD(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   int D = pack(ACCA, ACCB);
   D = or16_helper(D, operand);
   unpack(D, &ACCA, &ACCB);
   return -1;
}

static int op_fn_ORR(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   // r1 := r1 | r0
   int r0 = get_r0(operand);
   int r1 = get_r1(operand);
   int result;
   if ((operand & 0x0f) < 8) {
      result = or16_helper(r1, r0);
   } else {
      result = or_helper(r1, r0);
   }
   set_Logical_R_result(operand, result);
   return -1;
}

static int op_fn_PSHSW(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   pushw_helper(sample_q, 1);
   return -1;
}

static int op_fn_PSHUW(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   pushw_helper(sample_q, 0);
   return -1;
}

static int op_fn_PULSW(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   pullw_helper(sample_q, 1);
   return -1;
}

static int op_fn_PULUW(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   pullw_helper(sample_q, 0);
   return -1;
}

static int op_fn_ROLD(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   int D = pack(ACCA, ACCB);
   D = rol16_helper(D);
   unpack(D, &ACCA, &ACCB);
   return -1;
}

static int op_fn_ROLW(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   int W = pack(ACCE, ACCF);
   W = rol16_helper(W);
   unpack(W, &ACCE, &ACCF);
   return -1;
}

static int op_fn_RORD(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   int D = pack(ACCA, ACCB);
   D = ror16_helper(D);
   unpack(D, &ACCA, &ACCB);
   return -1;
}

static int op_fn_RORW(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   int W = pack(ACCE, ACCF);
   W = ror16_helper(W);
   unpack(W, &ACCE, &ACCF);
   return -1;
}

static int op_fn_SBCD(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   int D = pack(ACCA, ACCB);
   D = sub16_helper(D, C, operand);
   unpack(D, &ACCA, &ACCB);
   return -1;
}

static int op_fn_SBCR(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   // r1 := r1 - r0 - C
   int r0 = get_r0(operand);
   int r1 = get_r1(operand);
   int result;
   if ((operand & 0x0f) < 8) {
      result = sub16_helper(r1, C, r0);
   } else {
      result = sub_helper(r1, C, r0);
   }
   set_Arithmetic_R_result(operand, result);
   return -1;
}

static int op_fn_SEXW(operand_t operand, ea_t ea, sample_q_t *sample_q) {
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

static int op_fn_STBT(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   // Pickout the postbyte from the samples
   sample_t *sample = sample_q->sample + sample_q->oi;
   int postbyte = sample[1].data;

   // Parse the post byte
   int reg_num    = (postbyte >> 6) & 3; // Bits 7..6
   int reg_bitnum = (postbyte >> 3) & 7; // Bits 5..3
   int mem_bitnum = (postbyte     ) & 7; // Bits 2..0

   // Extract register bit, which can be 0, 1 or -1
   int reg_bit;
   switch (reg_num) {
   case 0: reg_bit = get_FLAG(reg_bitnum);                       break;
   case 1: reg_bit = (ACCA < 0) ? -1 : (ACCA >> reg_bitnum) & 1; break;
   case 2: reg_bit = (ACCB < 0) ? -1 : (ACCB >> reg_bitnum) & 1; break;
   case 3: reg_bit = 0; failflag |= FAIL_UNDOC;                ; break;
   }

   if (reg_bit >= 0) {
      // Calculate the value that is expected to be written back
      int ret = (operand & (0xff ^ (1 << mem_bitnum))) | (reg_bit << mem_bitnum);
      set_NZ(ret);
      return ret;
   } else {
      // No memory modelling is possible
      set_NZ_unknown();
      return -1;
   }
}

static int op_fn_STE(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   ACCE = st_helper(ACCE, operand, FAIL_ACCE);
   return -1;
}

static int op_fn_STF(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   ACCF = st_helper(ACCF, operand, FAIL_ACCF);
   return -1;
}

static int op_fn_STQ(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   uint32_t result = (uint32_t) operand;
   if (ACCF >= 0 && (uint32_t)ACCF != (result & 0xff)) {
      failflag |= FAIL_ACCF;
   }
   if (ACCE >= 0 && (uint32_t)ACCE != ((result >> 8) & 0xff)) {
      failflag |= FAIL_ACCE;
   }
   if (ACCB >= 0 && (uint32_t)ACCB != ((result >> 16) & 0xff)) {
      failflag |= FAIL_ACCB;
   }
   if (ACCA >= 0 && (uint32_t)ACCA != ((result >> 24) & 0xff)) {
      failflag |= FAIL_ACCA;
   }
   set_q_nz(result);
   return -1;
}

static int op_fn_STW(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   int W = pack(ACCE, ACCF);
   W = st16_helper(W, operand, FAIL_ACCE | FAIL_ACCF);
   unpack(W, &ACCE, &ACCF);
   return -1;
}

static int op_fn_SUBE(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   ACCE = sub_helper(ACCE, 0, operand);
   return -1;
}

static int op_fn_SUBF(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   ACCF = sub_helper(ACCF, 0, operand);
   return -1;
}

static int op_fn_SUBR(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   // r1 := r1 + r0
   int r0 = get_r0(operand);
   int r1 = get_r1(operand);
   int result;
   if ((operand & 0x0f) < 8) {
      result = sub16_helper(r1, 0, r0);
   } else {
      result = sub_helper(r1, 0, r0);
   }
   set_Arithmetic_R_result(operand, result);
   return -1;
}

static int op_fn_SUBW(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   int W = pack(ACCE, ACCF);
   W = sub16_helper(W, 0, operand);
   unpack(W, &ACCE, &ACCF);
   return -1;
}

static int op_fn_TFM(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   // 1138 TFM r0+, r1+
   // 1139 TFM r0-, r1-
   // 113A TFM r0+, r1
   // 113B TFM r0 , r1+
   sample_t *sample = sample_q->sample;
   int opcode = sample[sample_q->oi].data;
   int postbyte = sample[sample_q->oi + 1].data;
   sample_t *rd_sample = &sample[sample_q->oi + 5];
   sample_t *wr_sample = &sample[sample_q->oi + 7];
   int r0 = (postbyte >> 4) & 0xf;
   int r1 = postbyte & 0xf;

   // The number of bytes expected to be transferred is in W
   int W = pack(ACCE, ACCF);

   // Only D, X, Y, U, S are legal, anything else causes an illegal instruction trap
   if (r0 > 4 || r1 > 4) {
      // Random testing showed Z set based on W even in the failed case=0 in the failed case
      Z = (W < 0) ? -1 : (W == 0);
      failflag |= FAIL_BADM;
      // Illegal index register takes 25/23 (without additional prefixes)
      sample_q->num_cycles += (NM == 1) ? 19 : 17;
      interrupt_helper(sample_q, 6, 1, VEC_IL);
      return -1;
   }

   int D = pack(ACCA, ACCB);

   // Get reg0 (the source address)
   int *reg0;
   switch (r0) {
   case 1:  reg0 = &X;  break;
   case 2:  reg0 = &Y;  break;
   case 3:  reg0 = &U;  break;
   case 4:  reg0 = &S;  break;
   default: reg0 = &D;  break;
   }

   // Get reg1 (the destination address) [ which may be the same as the source! ]
   int *reg1;
   switch (r1) {
   case 1:  reg1 = &X;  break;
   case 2:  reg1 = &Y;  break;
   case 3:  reg1 = &U;  break;
   case 4:  reg1 = &S;  break;
   default: reg1 = &D;  break;
   }

   // The number of bytes actually transferred (less if TFR was)

   // NATIVE MODE
   //
   // Interrupt sequence forms part of the TFR instruction (no LIC seperating them)
   //
   // 12191a   0 11  1 1 00 F    PREFIX
   // 12191b   1 3b  1 0 00 0    OPCODE
   // 12191c   2 21  1 0 00 1    POSTBYTE
   // 12191d   3 21  1 0 00 F    -
   // 12191e   4 21  1 0 00 F    -
   // 12191f   5 21  1 0 00 F    -
   // 121920   6 20  1 0 00 8    Rd
   // 121921   7 20  1 0 00 F    -
   // 121922   8 20  0 0 00 0    Wr
   // 121923   9 20  1 0 00 8    Rd
   // 121924  10 20  1 0 00 F    -
   // 121925  11 20  0 0 00 1    Wr
   // 121926  12 20  1 0 00 8    Rd
   // 121927  13 20  1 0 00 F    -
   // 121928  14 20  0 0 00 2    Wr
   // 121929  15 20  1 0 00 8    Rd
   // 12192a  16 20  1 0 00 F    -
   // 12192b  17 20  0 0 00 3    Wr
   // 12192c  18 20  1 0 00 8    Rd
   // 12192d  19 20  1 0 00 F    -
   // 12192e  20 20  0 0 00 4    Wr
   // ...
   // 121b9c 642 20  1 0 00 8    Rd
   // 121b9d 643 20  1 0 00 F    -
   // 121b9e 644 20  0 0 00 4    Wr
   // 121b9f 645 20  1 0 00 8    Rd
   // 121ba0 646 20  1 0 00 F    -
   // 121ba1 647 20  0 0 00 5    Wr
   // 121ba2 648 20  1 0 00 8    Rd
   // 121ba3 649 20  1 0 00 F    -
   // 121ba4 650 20  0 0 00 6    Wr
   // 121ba5 651 20  1 0 00 8    Rd, not sure if PC or IO
   // 121ba6 652 20  1 0 00 F    -
   //                            End of 653 cycle TFM (6 + 215 * 3 + 2)
   //
   //                            Start of 19 cycle interrupt
   // 121ba7 653 20  1 0 00 F    -
   // 121ba8 654 bf  0 0 00 1 <<<< PC at offset 3
   // 121ba9 655 cb  0 0 00 0
   // 121baa 656 08  0 0 00 F
   // 121bab 657 19  0 0 00 E
   // 121bac 658 58  0 0 00 D
   // 121bad 659 03  0 0 00 C
   // 121bae 660 d7  0 0 00 B
   // 121baf 661 7c  0 0 00 A
   // 121bb0 662 00  0 0 00 9
   // 121bb1 663 29  0 0 00 8
   // 121bb2 664 03  0 0 00 7
   // 121bb3 665 04  0 0 00 6
   // 121bb4 666 0d  0 0 00 5
   // 121bb5 667 c0  0 0 00 4 <<<< end of 14 writes
   // 121bb6 668 20  1 0 00 F
   // 121bb7 669 d9  1 0 10 8 <<<< Vector
   // 121bb8 670 04  1 0 10 9
   // 121bb9 671 04  1 0 00 F


   // EMULATED MODE
   //
   // Interrupt sequence after the TFR instruction ()
   //
   // 1716f0    0 11  1 0 00 D    PREFIX
   // 1716f1    1 3b  1 0 00 E    OPCODE
   // 1716f2    2 21  1 0 00 F    POSTBYTE
   // 1716f3    3 21  1 0 00 F    -
   // 1716f4    4 21  1 0 00 F    -
   // 1716f5    5 21  1 0 00 F    -
   // 1716f6    6 20  1 0 00 8    Rd
   // 1716f7    7 20  1 0 00 F    -
   // 1716f8    8 20  0 0 00 0    Wr
   // 1716f9    9 20  1 0 00 8    Rd
   // 1716fa   10 20  1 0 00 F    -
   // 1716fb   11 20  0 0 00 1    Wr
   // 1716fc   12 20  1 0 00 8    Rd
   // 1716fd   13 20  1 0 00 F    -
   // 1716fe   14 20  0 0 00 2    Wr
   // 1716ff   15 20  1 0 00 8    Rd
   // 171700   16 20  1 0 00 F    -
   // 171701   17 20  0 0 00 3    Wr
   // 171702   18 20  1 0 00 8    Rd
   // 171703   19 20  1 0 00 F    -
   // 171704   20 20  0 0 00 4    Wr
   // ...
   // 1722bd 3021 20  1 0 00 8    Rd
   // 1722be 3022 20  1 0 00 F    -
   // 1722bf 3023 20  0 0 00 D    Wr
   // 1722c0 3024 20  1 0 00 8    Rd
   // 1722c1 3025 20  1 0 00 F    -
   // 1722c2 3026 20  0 0 00 E    Wr
   // 1722c3 3027 20  1 0 00 8    Rd
   // 1722c4 3028 20  1 0 00 F    -
   // 1722c5 3029 20  0 0 00 F    Wr
   // 1722c6 3030 20  1 0 00 8    Rd
   // 1722c7 3031 20  1 0 00 F    -
   // 1722c8 3032 20  0 0 00 0    Wr
   // 1722c9 3033 20  1 0 00 8    Rd (not sure of what)
   // 1722ca 3034 20  1 1 00 F    -    <<<<< LIC
   //                             End of 3035 cycles TFM (6 + 1009 * 3 + 2)
   //
   //                             Start of 17 cycle interrupt
   // 1722cb    0 20  1 1 00 F
   // 1722cc    0 bd  0 1 00 1 <<<< PC at offset 1
   // 1722cd    0 cb  0 1 00 0
   // 1722ce    0 08  0 1 00 F
   // 1722cf    0 19  0 1 00 E
   // 1722d0    0 58  0 1 00 D
   // 1722d1    0 03  0 1 00 C
   // 1722d2    0 f1  0 1 00 B
   // 1722d3    0 7f  0 1 00 A
   // 1722d4    0 00  0 1 00 9
   // 1722d5    0 04  0 1 00 8
   // 1722d6    0 0d  0 1 00 7
   // 1722d7    0 c0  0 1 00 6 <<<< end of 12 writes
   // 1722d8    0 20  1 1 00 F
   // 1722d9    0 d8  1 1 10 8 <<<< Vector
   // 1722da    0 ff  1 1 10 9
   // 1722db    0 ff  1 1 00 F

   int interrupted;
   int num_bytes;
   if (sample->rnw >= 0) {
      // If RnW is available, use it to check write cycles in the expected slots
      num_bytes = 0;
      int i = 8;
      while (sample[i].rnw == 0 && i < sample_q->num_samples) {
         num_bytes++;
         i += 3;
      }
      interrupted = (sample[i + 1].rnw == 0);
   } else {
      // If RnW is not available, we can't reliably detect TFM being interrupted
      // TODO: We could fall back to less reliable LIC if available
      num_bytes = W;
      interrupted = 0;
   }

   // TFM - cycle count in instruction tables is 6
   sample_q->num_cycles += 3 * num_bytes;

   // Handle the case where the common actions when TFM is interrupted in either mode
   if (interrupted) {
      // In native mode, the interrupted PC write is expected at offset 4, so we adjust num_cycles so this is true
      if (NM == 1) {
         sample_q->num_cycles--;
      }

      // set the PC back three cycles (the length of TFM), so the TFM re-executes after the RTI
      if (PC >= 0) {
         PC = (PC - 3) & 0xffff;
      }
      // cancel the num_cycles warning
      failflag &= ~FAIL_CYCLES;
   }

   // Memory modelling of reads and writes
   int r0inc = 0;
   int r1inc = 0;
   switch (opcode) {
   case 0x38: r0inc =  1; r1inc =  1; break; // 1138 TFM r0+, r1+
   case 0x39: r0inc = -1; r1inc = -1; break; // 1139 TFM r0-, r1-
   case 0x3A: r0inc =  1; r1inc =  0; break; // 113A TFM r0+, r1
   case 0x3B: r0inc =  0; r1inc =  1; break; // 113B TFM r0 , r1+
   }

   if (num_bytes > 0) {
      for (int i = 0; i < num_bytes; i++) {
         memory_read(rd_sample, *reg0, MEM_DATA);
         rd_sample += 3;
         *reg0 = offset_address(*reg0, r0inc);
         memory_write(wr_sample, *reg1, MEM_DATA);
         wr_sample += 3;
         *reg1 = offset_address(*reg1, r1inc);
      }
      // M Register
      M = (rd_sample - 3)->data;
   } else if (num_bytes < 0) {
      if (r0inc) {
         *reg0 = -1;
      }
      if (r1inc) {
         *reg1 = -1;
      }
      // M Register
      M = -1;
   }

   // In case D has been involved, we push the value back to ACCA/B
   unpack(D, &ACCA, &ACCB);

   // Update the final value of W
   if (W >= 0 && num_bytes >= 0) {
      W -= num_bytes;
      unpack(W, &ACCE, &ACCF);
      Z = (W == 0);
   }

   return -1;
}

static int op_fn_TIM(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   // oi points to the immediate byte
   sample_t *sample = sample_q->sample + sample_q->oi;
   set_NZ(operand & sample->data);
   V = 0;
   return -1;
}

static int op_fn_TRAP(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   interrupt_helper(sample_q, 4, 1, VEC_IL);
   return -1;
}

static int op_fn_TSTD(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   int D = pack(ACCA, ACCB);
   set_NZ16(D);
   V = 0;
   return -1;
}

static int op_fn_TSTE(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   set_NZ(ACCE);
   V = 0;
   return -1;
}

static int op_fn_TSTF(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   set_NZ(ACCF);
   V = 0;
   return -1;
}

static int op_fn_TSTW(operand_t operand, ea_t ea, sample_q_t *sample_q) {
   int W = pack(ACCE, ACCF);
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
static operation_t op_XADDD = { "XADDD", op_fn_XADDD,   READOP , 1 };
static operation_t op_XADDU = { "XADDU", op_fn_XADDU,   READOP , 1 };
static operation_t op_XCLRA = { "XCLRA", op_fn_XCLRA,    REGOP , 0 };
static operation_t op_XCLRB = { "XCLRB", op_fn_XCLRB,    REGOP , 0 };
static operation_t op_XDEC  = { "XDEC",  op_fn_XDEC ,    RMWOP , 0 };
static operation_t op_XDECA = { "XDECA", op_fn_XDECA,    REGOP , 0 };
static operation_t op_XDECB = { "XDECB", op_fn_XDECB,    REGOP , 0 };
static operation_t op_XFIQ  = { "XFIQ",  op_fn_XFIQ,     OTHER , 0 };
static operation_t op_XHCF  = { "XHCF",  op_fn_XHCF,    READOP , 0 };
static operation_t op_XNC   = { "XNC",   op_fn_XNC,      RMWOP , 0 };
static operation_t op_XNCA  = { "XNCA",  op_fn_XNCA,     REGOP , 0 };
static operation_t op_XNCB  = { "XNCB",  op_fn_XNCB,     REGOP , 0 };
static operation_t op_XSTS  = { "XSTS",  op_fn_XSTS,   STOREOP , 0 };
static operation_t op_XSTU  = { "XSTU",  op_fn_XSTU,   STOREOP , 0 };
static operation_t op_XSTX  = { "XSTX",  op_fn_XSTX,   STOREOP , 0 };
static operation_t op_XSTY  = { "XSTY",  op_fn_XSTY,   STOREOP , 0 };
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
   /* 0B */    { &op_XDEC , DIRECT       , 1, 6 },
   /* 0C */    { &op_INC  , DIRECT       , 0, 6 },
   /* 0D */    { &op_TST  , DIRECT       , 0, 6 },
   /* 0E */    { &op_JMP  , DIRECT       , 0, 3 },
   /* 0F */    { &op_CLR  , DIRECT       , 0, 6 },
   /* 10 */    { &op_UU   , INHERENT     , 0, 1 },
   /* 11 */    { &op_UU   , INHERENT     , 0, 1 },
   /* 12 */    { &op_NOP  , INHERENT     , 0, 2 },
   /* 13 */    { &op_SYNC , INHERENT     , 0, 4 },
   /* 14 */    { &op_XHCF , INHERENT     , 1, 0x10000 },
   /* 15 */    { &op_XHCF , INHERENT     , 1, 0x10000 },
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
   /* 42 */    { &op_XNCA , INHERENT     , 1, 2 },
   /* 43 */    { &op_COMA , INHERENT     , 0, 2 },
   /* 44 */    { &op_LSRA , INHERENT     , 0, 2 },
   /* 45 */    { &op_LSRA , INHERENT     , 1, 2 },
   /* 46 */    { &op_RORA , INHERENT     , 0, 2 },
   /* 47 */    { &op_ASRA , INHERENT     , 0, 2 },
   /* 48 */    { &op_ASLA , INHERENT     , 0, 2 },
   /* 49 */    { &op_ROLA , INHERENT     , 0, 2 },
   /* 4A */    { &op_DECA , INHERENT     , 0, 2 },
   /* 4B */    { &op_XDECA, INHERENT     , 1, 2 },
   /* 4C */    { &op_INCA , INHERENT     , 0, 2 },
   /* 4D */    { &op_TSTA , INHERENT     , 0, 2 },
   /* 4E */    { &op_XCLRA, INHERENT     , 1, 2 },
   /* 4F */    { &op_CLRA , INHERENT     , 0, 2 },
   /* 50 */    { &op_NEGB , INHERENT     , 0, 2 },
   /* 51 */    { &op_NEGB , INHERENT     , 1, 2 },
   /* 52 */    { &op_XNCB , INHERENT     , 1, 2 },
   /* 53 */    { &op_COMB , INHERENT     , 0, 2 },
   /* 54 */    { &op_LSRB , INHERENT     , 0, 2 },
   /* 55 */    { &op_LSRB , INHERENT     , 1, 2 },
   /* 56 */    { &op_RORB , INHERENT     , 0, 2 },
   /* 57 */    { &op_ASRB , INHERENT     , 0, 2 },
   /* 58 */    { &op_ASLB , INHERENT     , 0, 2 },
   /* 59 */    { &op_ROLB , INHERENT     , 0, 2 },
   /* 5A */    { &op_DECB , INHERENT     , 0, 2 },
   /* 5B */    { &op_XDECB, INHERENT     , 1, 2 },
   /* 5C */    { &op_INCB , INHERENT     , 0, 2 },
   /* 5D */    { &op_TSTB , INHERENT     , 0, 2 },
   /* 5E */    { &op_XCLRB, INHERENT     , 1, 2 },
   /* 5F */    { &op_CLRB , INHERENT     , 0, 2 },
   /* 60 */    { &op_NEG  , INDEXED      , 0, 6 },
   /* 61 */    { &op_NEG  , INDEXED      , 1, 6 },
   /* 62 */    { &op_XNC  , INDEXED      , 1, 6 },
   /* 63 */    { &op_COM  , INDEXED      , 0, 6 },
   /* 64 */    { &op_LSR  , INDEXED      , 0, 6 },
   /* 65 */    { &op_LSR  , INDEXED      , 1, 6 },
   /* 66 */    { &op_ROR  , INDEXED      , 0, 6 },
   /* 67 */    { &op_ASR  , INDEXED      , 0, 6 },
   /* 68 */    { &op_ASL  , INDEXED      , 0, 6 },
   /* 69 */    { &op_ROL  , INDEXED      , 0, 6 },
   /* 6A */    { &op_DEC  , INDEXED      , 0, 6 },
   /* 6B */    { &op_XDEC , INDEXED      , 1, 6 },
   /* 6C */    { &op_INC  , INDEXED      , 0, 6 },
   /* 6D */    { &op_TST  , INDEXED      , 0, 6 },
   /* 6E */    { &op_JMP  , INDEXED      , 0, 3 },
   /* 6F */    { &op_CLR  , INDEXED      , 0, 6 },
   /* 70 */    { &op_NEG  , EXTENDED     , 0, 7 },
   /* 71 */    { &op_NEG  , EXTENDED     , 1, 7 },
   /* 72 */    { &op_XNC  , EXTENDED     , 1, 7 },
   /* 73 */    { &op_COM  , EXTENDED     , 0, 7 },
   /* 74 */    { &op_LSR  , EXTENDED     , 0, 7 },
   /* 75 */    { &op_LSR  , EXTENDED     , 1, 7 },
   /* 76 */    { &op_ROR  , EXTENDED     , 0, 7 },
   /* 77 */    { &op_ASR  , EXTENDED     , 0, 7 },
   /* 78 */    { &op_ASL  , EXTENDED     , 0, 7 },
   /* 79 */    { &op_ROL  , EXTENDED     , 0, 7 },
   /* 7A */    { &op_DEC  , EXTENDED     , 0, 7 },
   /* 7B */    { &op_XDEC , EXTENDED     , 1, 7 },
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
   /* CD */    { &op_XHCF , INHERENT     , 1, 0x10000 },
   /* CE */    { &op_LDU  , IMMEDIATE_16 , 0, 3 },
   /* CF */    { &op_XSTU , IMMEDIATE_8  , 1, 3 },
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
   /* 1020 */  { &op_LBRA , RELATIVE_16  , 1, 6 },
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
   /* 103E */  { &op_SWI2 , INHERENT     , 1,20 },
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
   /* 108F */  { &op_XSTY , IMMEDIATE_8  , 1, 4 },
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
   /* 10C3 */  { &op_XADDD, IMMEDIATE_16 , 1, 5 },
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
   /* 10CF */  { &op_XSTS , IMMEDIATE_8  , 1, 4 },
   /* 10D0 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10D1 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10D2 */  { &op_XX   , ILLEGAL      , 1, 1 },
   /* 10D3 */  { &op_XADDD, DIRECT       , 1, 7 },
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
   /* 10E3 */  { &op_XADDD, INDEXED      , 1, 7 },
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
   /* 10F3 */  { &op_XADDD, EXTENDED     , 1, 8 },
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
   /* 113E */  { &op_XFIQ , INHERENT     , 1,20 },
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
   /* 11C3 */  { &op_XADDU, IMMEDIATE_16 , 1, 5 },
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
   /* 11D3 */  { &op_XADDU, DIRECT       , 1, 7 },
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
   /* 11E3 */  { &op_XADDU, INDEXED      , 1, 7 },
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
   /* 11F3 */  { &op_XADDU, EXTENDED     , 1, 8 },
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
   /* 01 */    { &op_OIM  , DIRECTIM     , 0, 6, 6 },
   /* 02 */    { &op_AIM  , DIRECTIM     , 0, 6, 6 },
   /* 03 */    { &op_COM  , DIRECT       , 0, 6, 5 },
   /* 04 */    { &op_LSR  , DIRECT       , 0, 6, 5 },
   /* 05 */    { &op_EIM  , DIRECTIM     , 0, 6, 6 },
   /* 06 */    { &op_ROR  , DIRECT       , 0, 6, 5 },
   /* 07 */    { &op_ASR  , DIRECT       , 0, 6, 5 },
   /* 08 */    { &op_ASL  , DIRECT       , 0, 6, 5 },
   /* 09 */    { &op_ROL  , DIRECT       , 0, 6, 5 },
   /* 0A */    { &op_DEC  , DIRECT       , 0, 6, 5 },
   /* 0B */    { &op_TIM  , DIRECTIM     , 0, 4, 4 },
   /* 0C */    { &op_INC  , DIRECT       , 0, 6, 5 },
   /* 0D */    { &op_TST  , DIRECT       , 0, 6, 4 },
   /* 0E */    { &op_JMP  , DIRECT       , 0, 3, 2 },
   /* 0F */    { &op_CLR  , DIRECT       , 0, 6, 5 },
   /* 10 */    { &op_UU   , INHERENT     , 0, 1, 1 },
   /* 11 */    { &op_UU   , INHERENT     , 0, 1, 1 },
   /* 12 */    { &op_NOP  , INHERENT     , 0, 2, 1 },
   /* 13 */    { &op_SYNC , INHERENT     , 0, 4, 3 },
   /* 14 */    { &op_SEXW , INHERENT     , 0, 4, 4 },
   /* 15 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 16 */    { &op_LBRA , RELATIVE_16  , 0, 5, 4 },
   /* 17 */    { &op_LBSR , RELATIVE_16  , 0, 9, 7 },
   /* 18 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 19 */    { &op_DAA  , INHERENT     , 0, 2, 1 },
   /* 1A */    { &op_ORCC , REGISTER     , 0, 3, 3 },
   /* 1B */    { &op_TRAP , ILLEGAL      , 1,20,22 },
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
   /* 38 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 39 */    { &op_RTS  , INHERENT     , 0, 5, 4 },
   /* 3A */    { &op_ABX  , INHERENT     , 0, 3, 1 },
   /* 3B */    { &op_RTI  , INHERENT     , 0, 6, 6 },
   /* 3C */    { &op_CWAI , IMMEDIATE_8  , 0,20,22 },
   /* 3D */    { &op_MUL  , INHERENT     , 0,11,10 },
   /* 3E */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 3F */    { &op_SWI  , INHERENT     , 0,19,21 },
   /* 40 */    { &op_NEGA , INHERENT     , 0, 2, 1 },
   /* 41 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 42 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 43 */    { &op_COMA , INHERENT     , 0, 2, 1 },
   /* 44 */    { &op_LSRA , INHERENT     , 0, 2, 1 },
   /* 45 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 46 */    { &op_RORA , INHERENT     , 0, 2, 1 },
   /* 47 */    { &op_ASRA , INHERENT     , 0, 2, 1 },
   /* 48 */    { &op_ASLA , INHERENT     , 0, 2, 1 },
   /* 49 */    { &op_ROLA , INHERENT     , 0, 2, 1 },
   /* 4A */    { &op_DECA , INHERENT     , 0, 2, 1 },
   /* 4B */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 4C */    { &op_INCA , INHERENT     , 0, 2, 1 },
   /* 4D */    { &op_TSTA , INHERENT     , 0, 2, 1 },
   /* 4E */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 4F */    { &op_CLRA , INHERENT     , 0, 2, 1 },
   /* 50 */    { &op_NEGB , INHERENT     , 0, 2, 1 },
   /* 51 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 52 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 53 */    { &op_COMB , INHERENT     , 0, 2, 1 },
   /* 54 */    { &op_LSRB , INHERENT     , 0, 2, 1 },
   /* 55 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 56 */    { &op_RORB , INHERENT     , 0, 2, 1 },
   /* 57 */    { &op_ASRB , INHERENT     , 0, 2, 1 },
   /* 58 */    { &op_ASLB , INHERENT     , 0, 2, 1 },
   /* 59 */    { &op_ROLB , INHERENT     , 0, 2, 1 },
   /* 5A */    { &op_DECB , INHERENT     , 0, 2, 1 },
   /* 5B */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 5C */    { &op_INCB , INHERENT     , 0, 2, 1 },
   /* 5D */    { &op_TSTB , INHERENT     , 0, 2, 1 },
   /* 5E */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 5F */    { &op_CLRB , INHERENT     , 0, 2, 1 },
   /* 60 */    { &op_NEG  , INDEXED      , 0, 6, 6 },
   /* 61 */    { &op_OIM  , INDEXEDIM    , 0, 7, 7 },
   /* 62 */    { &op_AIM  , INDEXEDIM    , 0, 7, 7 },
   /* 63 */    { &op_COM  , INDEXED      , 0, 6, 6 },
   /* 64 */    { &op_LSR  , INDEXED      , 0, 6, 6 },
   /* 65 */    { &op_EIM  , INDEXEDIM    , 0, 7, 7 },
   /* 66 */    { &op_ROR  , INDEXED      , 0, 6, 6 },
   /* 67 */    { &op_ASR  , INDEXED      , 0, 6, 6 },
   /* 68 */    { &op_ASL  , INDEXED      , 0, 6, 6 },
   /* 69 */    { &op_ROL  , INDEXED      , 0, 6, 6 },
   /* 6A */    { &op_DEC  , INDEXED      , 0, 6, 6 },
   /* 6B */    { &op_TIM  , INDEXEDIM    , 0, 5, 5 },
   /* 6C */    { &op_INC  , INDEXED      , 0, 6, 6 },
   /* 6D */    { &op_TST  , INDEXED      , 0, 6, 5 },
   /* 6E */    { &op_JMP  , INDEXED      , 0, 3, 3 },
   /* 6F */    { &op_CLR  , INDEXED      , 0, 6, 6 },
   /* 70 */    { &op_NEG  , EXTENDED     , 0, 7, 6 },
   /* 71 */    { &op_OIM  , EXTENDEDIM   , 0, 7, 7 },
   /* 72 */    { &op_AIM  , EXTENDEDIM   , 0, 7, 7 },
   /* 73 */    { &op_COM  , EXTENDED     , 0, 7, 6 },
   /* 74 */    { &op_LSR  , EXTENDED     , 0, 7, 6 },
   /* 75 */    { &op_EIM  , EXTENDEDIM   , 0, 7, 7 },
   /* 76 */    { &op_ROR  , EXTENDED     , 0, 7, 6 },
   /* 77 */    { &op_ASR  , EXTENDED     , 0, 7, 6 },
   /* 78 */    { &op_ASL  , EXTENDED     , 0, 7, 6 },
   /* 79 */    { &op_ROL  , EXTENDED     , 0, 7, 6 },
   /* 7A */    { &op_DEC  , EXTENDED     , 0, 7, 6 },
   /* 7B */    { &op_TIM  , EXTENDEDIM   , 0, 5, 5 },
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
   /* 87 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 88 */    { &op_EORA , IMMEDIATE_8  , 0, 2, 2 },
   /* 89 */    { &op_ADCA , IMMEDIATE_8  , 0, 2, 2 },
   /* 8A */    { &op_ORA  , IMMEDIATE_8  , 0, 2, 2 },
   /* 8B */    { &op_ADDA , IMMEDIATE_8  , 0, 2, 2 },
   /* 8C */    { &op_CMPX , IMMEDIATE_16 , 0, 4, 3 },
   /* 8D */    { &op_BSR  , RELATIVE_8   , 0, 7, 6 },
   /* 8E */    { &op_LDX  , IMMEDIATE_16 , 0, 3, 3 },
   /* 8F */    { &op_TRAP , ILLEGAL      , 1,20,22 },
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
   /* C7 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* C8 */    { &op_EORB , IMMEDIATE_8  , 0, 2, 2 },
   /* C9 */    { &op_ADCB , IMMEDIATE_8  , 0, 2, 2 },
   /* CA */    { &op_ORB  , IMMEDIATE_8  , 0, 2, 2 },
   /* CB */    { &op_ADDB , IMMEDIATE_8  , 0, 2, 2 },
   /* CC */    { &op_LDD  , IMMEDIATE_16 , 0, 3, 3 },
   /* CD */    { &op_LDQ  , IMMEDIATE_32 , 0, 5, 5 },
   /* CE */    { &op_LDU  , IMMEDIATE_16 , 0, 3, 3 },
   /* CF */    { &op_TRAP , ILLEGAL      , 1,20,22 },
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

   /* 1000 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1001 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1002 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1003 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1004 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1005 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1006 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1007 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1008 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1009 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 100A */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 100B */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 100C */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 100D */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 100E */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 100F */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1010 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1011 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1012 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1013 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1014 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1015 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1016 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1017 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1018 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1019 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 101A */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 101B */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 101C */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 101D */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 101E */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 101F */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1020 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
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
   /* 103C */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 103D */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 103E */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 103F */  { &op_SWI2 , INHERENT     , 0,20,22 },
   /* 1040 */  { &op_NEGD , INHERENT     , 0, 3, 2 },
   /* 1041 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1042 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1043 */  { &op_COMD , INHERENT     , 0, 3, 2 },
   /* 1044 */  { &op_LSRD , INHERENT     , 0, 3, 2 },
   /* 1045 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1046 */  { &op_RORD , INHERENT     , 0, 3, 2 },
   /* 1047 */  { &op_ASRD , INHERENT     , 0, 3, 2 },
   /* 1048 */  { &op_ASLD , INHERENT     , 0, 3, 2 },
   /* 1049 */  { &op_ROLD , INHERENT     , 0, 3, 2 },
   /* 104A */  { &op_DECD , INHERENT     , 0, 3, 2 },
   /* 104B */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 104C */  { &op_INCD , INHERENT     , 0, 3, 2 },
   /* 104D */  { &op_TSTD , INHERENT     , 0, 3, 2 },
   /* 104E */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 104F */  { &op_CLRD , INHERENT     , 0, 3, 2 },
   /* 1050 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1051 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1052 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1053 */  { &op_COMW , INHERENT     , 0, 3, 2 },
   /* 1054 */  { &op_LSRW , INHERENT     , 0, 3, 2 },
   /* 1055 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1056 */  { &op_RORW , INHERENT     , 0, 3, 2 },
   /* 1057 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1058 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1059 */  { &op_ROLW , INHERENT     , 0, 3, 2 },
   /* 105A */  { &op_DECW , INHERENT     , 0, 3, 2 },
   /* 105B */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 105C */  { &op_INCW , INHERENT     , 0, 3, 2 },
   /* 105D */  { &op_TSTW , INHERENT     , 0, 3, 2 },
   /* 105E */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 105F */  { &op_CLRW , INHERENT     , 0, 3, 2 },
   /* 1060 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1061 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1062 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1063 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1064 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1065 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1066 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1067 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1068 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1069 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 106A */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 106B */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 106C */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 106D */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 106E */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 106F */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1070 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1071 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1072 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1073 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1074 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1075 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1076 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1077 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1078 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1079 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 107A */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 107B */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 107C */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 107D */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 107E */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 107F */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1080 */  { &op_SUBW , IMMEDIATE_16 , 0, 5, 4 },
   /* 1081 */  { &op_CMPW , IMMEDIATE_16 , 0, 5, 4 },
   /* 1082 */  { &op_SBCD , IMMEDIATE_16 , 0, 5, 4 },
   /* 1083 */  { &op_CMPD , IMMEDIATE_16 , 0, 5, 4 },
   /* 1084 */  { &op_ANDD , IMMEDIATE_16 , 0, 5, 4 },
   /* 1085 */  { &op_BITD , IMMEDIATE_16 , 0, 5, 4 },
   /* 1086 */  { &op_LDW  , IMMEDIATE_16 , 0, 4, 4 },
   /* 1087 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1088 */  { &op_EORD , IMMEDIATE_16 , 0, 5, 4 },
   /* 1089 */  { &op_ADCD , IMMEDIATE_16 , 0, 5, 4 },
   /* 108A */  { &op_ORD  , IMMEDIATE_16 , 0, 5, 4 },
   /* 108B */  { &op_ADDW , IMMEDIATE_16 , 0, 5, 4 },
   /* 108C */  { &op_CMPY , IMMEDIATE_16 , 0, 5, 4 },
   /* 108D */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 108E */  { &op_LDY  , IMMEDIATE_16 , 0, 4, 4 },
   /* 108F */  { &op_TRAP , ILLEGAL      , 1,21,23 },
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
   /* 109D */  { &op_TRAP , ILLEGAL      , 1,21,23 },
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
   /* 10AD */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 10AE */  { &op_LDY  , INDEXED      , 0, 6, 6 },
   /* 10AF */  { &op_STY  , INDEXED      , 0, 6, 6 },
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
   /* 10BD */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 10BE */  { &op_LDY  , EXTENDED     , 0, 7, 6 },
   /* 10BF */  { &op_STY  , EXTENDED     , 0, 7, 6 },
   /* 10C0 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 10C1 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 10C2 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 10C3 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 10C4 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 10C5 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 10C6 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 10C7 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 10C8 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 10C9 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 10CA */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 10CB */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 10CC */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 10CD */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 10CE */  { &op_LDS  , IMMEDIATE_16 , 0, 4, 4 },
   /* 10CF */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 10D0 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 10D1 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 10D2 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 10D3 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 10D4 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 10D5 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 10D6 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 10D7 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 10D8 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 10D9 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 10DA */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 10DB */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 10DC */  { &op_LDQ  , DIRECT       , 1, 8, 7 },
   /* 10DD */  { &op_STQ  , DIRECT       , 1, 8, 7 },
   /* 10DE */  { &op_LDS  , DIRECT       , 0, 6, 5 },
   /* 10DF */  { &op_STS  , DIRECT       , 0, 6, 5 },
   /* 10E0 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 10E1 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 10E2 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 10E3 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 10E4 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 10E5 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 10E6 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 10E7 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 10E8 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 10E9 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 10EA */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 10EB */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 10EC */  { &op_LDQ  , INDEXED      , 0, 8, 8 },
   /* 10ED */  { &op_STQ  , INDEXED      , 0, 8, 8 },
   /* 10EE */  { &op_LDS  , INDEXED      , 0, 6, 6 },
   /* 10EF */  { &op_STS  , INDEXED      , 0, 6, 6 },
   /* 10F0 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 10F1 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 10F2 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 10F3 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 10F4 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 10F5 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 10F6 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 10F7 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 10F8 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 10F9 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 10FA */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 10FB */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 10FC */  { &op_LDQ  , EXTENDED     , 0, 9, 8 },
   /* 10FD */  { &op_STQ  , EXTENDED     , 0, 9, 8 },
   /* 10FE */  { &op_LDS  , EXTENDED     , 0, 7, 6 },
   /* 10FF */  { &op_STS  , EXTENDED     , 0, 7, 6 },

   /* 1100 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1101 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1102 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1103 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1104 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1105 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1106 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1107 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1108 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1109 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 110A */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 110B */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 110C */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 110D */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 110E */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 110F */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1110 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1111 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1112 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1113 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1114 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1115 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1116 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1117 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1118 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1119 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 111A */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 111B */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 111C */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 111D */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 111E */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 111F */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1120 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1121 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1122 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1123 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1124 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1125 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1126 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1127 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1128 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1129 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 112A */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 112B */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 112C */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 112D */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 112E */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 112F */  { &op_TRAP , ILLEGAL      , 1,21,23 },
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
   /* 113D */  { &op_LDMD , IMMEDIATE_8  , 0, 5, 5 },
   /* 113E */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 113F */  { &op_SWI3 , INHERENT     , 0,20,22 },
   /* 1140 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1141 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1142 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1143 */  { &op_COME , INHERENT     , 0, 3, 2 },
   /* 1144 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1145 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1146 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1147 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1148 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1149 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 114A */  { &op_DECE , INHERENT     , 0, 3, 2 },
   /* 114B */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 114C */  { &op_INCE , INHERENT     , 0, 3, 2 },
   /* 114D */  { &op_TSTE , INHERENT     , 0, 3, 2 },
   /* 114E */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 114F */  { &op_CLRE , INHERENT     , 0, 3, 2 },
   /* 1150 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1151 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1152 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1153 */  { &op_COMF , INHERENT     , 0, 3, 2 },
   /* 1154 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1155 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1156 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1157 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1158 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1159 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 115A */  { &op_DECF , INHERENT     , 0, 3, 2 },
   /* 115B */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 115C */  { &op_INCF , INHERENT     , 0, 3, 2 },
   /* 115D */  { &op_TSTF , ILLEGAL      , 0, 3, 2 },
   /* 115E */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 115F */  { &op_CLRF , INHERENT     , 0, 3, 2 },
   /* 1160 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1161 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1162 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1163 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1164 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1165 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1166 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1167 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1168 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1169 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 116A */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 116B */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 116C */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 116D */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 116E */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 116F */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1170 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1171 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1172 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1173 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1174 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1175 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1176 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1177 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1178 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1179 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 117A */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 117B */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 117C */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 117D */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 117E */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 117F */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1180 */  { &op_SUBE , IMMEDIATE_8  , 0, 3, 3 },
   /* 1181 */  { &op_CMPE , IMMEDIATE_8  , 0, 3, 3 },
   /* 1182 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1183 */  { &op_CMPU , IMMEDIATE_16 , 0, 5, 4 },
   /* 1184 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1185 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1186 */  { &op_LDE  , IMMEDIATE_8  , 0, 3, 3 },
   /* 1187 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1188 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1189 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 118A */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 118B */  { &op_ADDE , IMMEDIATE_8  , 0, 3, 3 },
   /* 118C */  { &op_CMPS , IMMEDIATE_16 , 0, 5, 4 },
   /* 118D */  { &op_DIVD , IMMEDIATE_8  , 0,25,25 },
   /* 118E */  { &op_DIVQ , IMMEDIATE_16 , 0,34,34 },
   /* 118F */  { &op_MULD , IMMEDIATE_16 , 0,28,28 },
   /* 1190 */  { &op_SUBE , DIRECT       , 0, 5, 4 },
   /* 1191 */  { &op_CMPE , DIRECT       , 0, 5, 4 },
   /* 1192 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1193 */  { &op_CMPU , DIRECT       , 0, 7, 5 },
   /* 1194 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1195 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1196 */  { &op_LDE  , DIRECT       , 0, 5, 4 },
   /* 1197 */  { &op_STE  , DIRECT       , 0, 5, 4 },
   /* 1198 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 1199 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 119A */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 119B */  { &op_ADDE , DIRECT       , 0, 5, 4 },
   /* 119C */  { &op_CMPS , DIRECT       , 0, 7, 5 },
   /* 119D */  { &op_DIVD , DIRECT       , 0,27,26 },
   /* 119E */  { &op_DIVQ , DIRECT       , 0,36,35 },
   /* 119F */  { &op_MULD , DIRECT       , 0,30,29 },
   /* 11A0 */  { &op_SUBE , INDEXED      , 0, 5, 5 },
   /* 11A1 */  { &op_CMPE , INDEXED      , 0, 5, 5 },
   /* 11A2 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 11A3 */  { &op_CMPU , INDEXED      , 0, 7, 6 },
   /* 11A4 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 11A5 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 11A6 */  { &op_LDE  , INDEXED      , 0, 5, 5 },
   /* 11A7 */  { &op_STE  , INDEXED      , 0, 5, 5 },
   /* 11A8 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 11A9 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 11AA */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 11AB */  { &op_ADDE , INDEXED      , 0, 5, 5 },
   /* 11AC */  { &op_CMPS , INDEXED      , 0, 7, 6 },
   /* 11AD */  { &op_DIVD , INDEXED      , 0,27,27 },
   /* 11AE */  { &op_DIVQ , INDEXED      , 0,36,36 },
   /* 11AF */  { &op_MULD , INDEXED      , 0,30,30 },
   /* 11B0 */  { &op_SUBE , EXTENDED     , 0, 6, 5 },
   /* 11B1 */  { &op_CMPE , EXTENDED     , 0, 6, 5 },
   /* 11B2 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 11B3 */  { &op_CMPU , EXTENDED     , 0, 8, 6 },
   /* 11B4 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 11B5 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 11B6 */  { &op_LDE  , EXTENDED     , 0, 6, 5 },
   /* 11B7 */  { &op_STE  , EXTENDED     , 0, 6, 5 },
   /* 11B8 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 11B9 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 11BA */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 11BB */  { &op_ADDE , EXTENDED     , 0, 6, 5 },
   /* 11BC */  { &op_CMPS , EXTENDED     , 0, 8, 6 },
   /* 11BD */  { &op_DIVD , EXTENDED     , 0,28,27 },
   /* 11BE */  { &op_DIVQ , EXTENDED     , 0,37,36 },
   /* 11BF */  { &op_MULD , EXTENDED     , 0,31,30 },
   /* 11C0 */  { &op_SUBF , IMMEDIATE_8  , 0, 3, 3 },
   /* 11C1 */  { &op_CMPF , IMMEDIATE_8  , 0, 3, 3 },
   /* 11C2 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 11C3 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 11C4 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 11C5 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 11C6 */  { &op_LDF  , IMMEDIATE_8  , 0, 3, 3 },
   /* 11C7 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 11C8 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 11C9 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 11CA */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 11CB */  { &op_ADDF , IMMEDIATE_8  , 0, 3, 3 },
   /* 11CC */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 11CD */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 11CE */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 11CF */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 11D0 */  { &op_SUBF , DIRECT       , 0, 5, 4 },
   /* 11D1 */  { &op_CMPF , DIRECT       , 0, 5, 4 },
   /* 11D2 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 11D3 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 11D4 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 11D5 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 11D6 */  { &op_LDF  , DIRECT       , 0, 5, 4 },
   /* 11D7 */  { &op_STF  , DIRECT       , 0, 5, 4 },
   /* 11D8 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 11D9 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 11DA */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 11DB */  { &op_ADDF , DIRECT       , 0, 5, 4 },
   /* 11DC */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 11DD */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 11DE */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 11DF */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 11E0 */  { &op_SUBF , INDEXED      , 0, 5, 5 },
   /* 11E1 */  { &op_CMPF , INDEXED      , 0, 5, 5 },
   /* 11E2 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 11E3 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 11E4 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 11E5 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 11E6 */  { &op_LDF  , INDEXED      , 0, 5, 5 },
   /* 11E7 */  { &op_STF  , INDEXED      , 0, 5, 5 },
   /* 11E8 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 11E9 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 11EA */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 11EB */  { &op_ADDF , INDEXED      , 0, 5, 5 },
   /* 11EC */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 11ED */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 11EE */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 11EF */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 11F0 */  { &op_SUBF , EXTENDED     , 0, 6, 5 },
   /* 11F1 */  { &op_CMPF , EXTENDED     , 0, 6, 5 },
   /* 11F2 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 11F3 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 11F4 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 11F5 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 11F6 */  { &op_LDF  , EXTENDED     , 0, 6, 5 },
   /* 11F7 */  { &op_STF  , EXTENDED     , 0, 6, 5 },
   /* 11F8 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 11F9 */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 11FA */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 11FB */  { &op_ADDF , EXTENDED     , 0, 6, 5 },
   /* 11FC */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 11FD */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 11FE */  { &op_TRAP , ILLEGAL      , 1,21,23 },
   /* 11FF */  { &op_TRAP , ILLEGAL      , 1,21,23 }
};
