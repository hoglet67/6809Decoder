#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "memory.h"
#include "em_6809.h"

// ====================================================================
// Fail flags
// ====================================================================

enum {
   FAIL_A      = 0x00000010,
   FAIL_B      = 0x00000020,
   FAIL_X      = 0x00000040,
   FAIL_Y      = 0x00000080,
   FAIL_U      = 0x00000100,
   FAIL_S      = 0x00000200,
   FAIL_DP     = 0x00000400,
   FAIL_E      = 0x00000800,
   FAIL_F      = 0x00001000,
   FAIL_H      = 0x00002000,
   FAIL_I      = 0x00004000,
   FAIL_N      = 0x00008000,
   FAIL_Z      = 0x00010000,
   FAIL_V      = 0x00020000,
   FAIL_C      = 0x00040000,
   FAIL_RESULT = 0x00080000,
   FAIL_VECTOR = 0x00100000,
   FAIL_CYCLES = 0x00200000,
   FAIL_UNDOC  = 0x00400000,
   FAIL_BADM   = 0x00800000,
};

static const char * fail_hints[32] = {
   "PC",
   "Memory",
   "?",
   "?",
   "A",
   "B",
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
   "?",
   "?",
   "?"
};

// ====================================================================
// Type Defs
// ====================================================================

typedef enum {
   INHERENT,
   REGISTER,
   IMMEDIATE_8,
   IMMEDIATE_16,
   RELATIVE_8,
   RELATIVE_16,
   DIRECT,
   EXTENDED,
   INDEXED,
   ILLEGAL
} addr_mode_t ;

typedef enum {
   READOP,
   REGOP,     // operates on rehisters onlt; no EA
   LEAOP,     // LEA or
   JSROP,     // JSR
   LOADOP,    // LDx
   STOREOP,   // STx
   RMWOP,
   BRANCHOP,
   OTHER
} optype_t;

typedef int operand_t;

typedef int ea_t;

typedef struct {
   const char *mnemonic;
   int (*emulate)(operand_t, ea_t, sample_t *);
   optype_t type;
   int size16;
} operation_t;

typedef struct {
   operation_t *op;
   addr_mode_t mode;
   int cycles;
   int undocumented;
} instr_mode_t;


// ====================================================================
// Static variables
// ====================================================================

#define OFFSET_A    2
#define OFFSET_B    7
#define OFFSET_X   12
#define OFFSET_Y   19
#define OFFSET_U   26
#define OFFSET_S   33
#define OFFSET_DP  41
#define OFFSET_E   46
#define OFFSET_F   50
#define OFFSET_H   54
#define OFFSET_I   58
#define OFFSET_N   62
#define OFFSET_Z   66
#define OFFSET_V   70
#define OFFSET_C   74
#define OFFSET_END 75

static const char regi[] = { 'X', 'Y', 'U', 'S' };

static const char *exgi[] = { "D", "X", "Y", "U", "S", "PC", "??", "??", "A",
                              "B", "CC", "DP", "??", "??", "??", "??" };

static const char *pshsregi[] = { "PC", "U", "Y", "X", "DP", "B", "A", "CC" };

static const char *pshuregi[] = { "PC", "S", "Y", "X", "DP", "B", "A", "CC" };

static const char default_state[] = "A=?? B=?? X=???? Y=???? U=???? S=???? DP=?? E=? F=? H=? I=? N=? Z=? V=? C=?";

// Indexed indirect modes, an extra level of indirection occurs
//
// Cycle numbers from http://atjs.mbnet.fi/mc6809/Information/6809cyc.txt
// and cross-checked with Figure 17 in the 6809E datasheet.

static const int indirect_offset[] = {
   0, //  0 : [,R+]       illegal
   6, //  1 : [,R++]
   0, //  2 : [,-R]       illegal
   6, //  3 : [,--R]
   3, //  4 : [,R]
   4, //  5 : [B,R]
   4, //  6 : [A,R]
   0, //  7 :             undefined
   4, //  8 : [n7,R]
   7, //  9 : [n15,R]
   0, // 10 :             undefined
   7, // 11 : [D,R]
   4, // 12 : [n7,PCR]
   8, // 13 : [n15,PCR]
   0, // 14 :             undefined
   5  // 15 : [n]
};

// 6809 registers: -1 means unknown
static int A = -1;
static int B = -1;
static int X = -1;
static int Y = -1;
static int S = -1;
static int U = -1;
static int DP = -1;
static int PC = -1;

// 6809 flags: -1 means unknown
static int E = -1;
static int F = -1;
static int H = -1;
static int I = -1;
static int N = -1;
static int Z = -1;
static int V = -1;
static int C = -1;

// Misc
static int show_cycle_errors = 0;

// ====================================================================
// Forward declarations
// ====================================================================

static instr_mode_t instr_table_6809_map0[];
static instr_mode_t instr_table_6809_map1[];
static instr_mode_t instr_table_6809_map2[];


static operation_t op_ABX  ;
static operation_t op_ADCA ;
static operation_t op_ADCB ;
static operation_t op_ADDA ;
static operation_t op_ADDB ;
static operation_t op_ADDD ;
static operation_t op_ANDA ;
static operation_t op_ANDB ;
static operation_t op_ANDC ;
static operation_t op_ASL  ;
static operation_t op_ASLA ;
static operation_t op_ASLB ;
static operation_t op_ASR  ;
static operation_t op_ASRA ;
static operation_t op_ASRB ;
static operation_t op_BCC  ;
static operation_t op_BEQ  ;
static operation_t op_BGE  ;
static operation_t op_BGT  ;
static operation_t op_BHI  ;
static operation_t op_BITA ;
static operation_t op_BITB ;
static operation_t op_BLE  ;
static operation_t op_BLO  ;
static operation_t op_BLS  ;
static operation_t op_BLT  ;
static operation_t op_BMI  ;
static operation_t op_BNE  ;
static operation_t op_BPL  ;
static operation_t op_BRA  ;
static operation_t op_BRN  ;
static operation_t op_BSR  ;
static operation_t op_BVC  ;
static operation_t op_BVS  ;
static operation_t op_CLR  ;
static operation_t op_CLRA ;
static operation_t op_CLRB ;
static operation_t op_CMPA ;
static operation_t op_CMPB ;
static operation_t op_CMPD ;
static operation_t op_CMPS ;
static operation_t op_CMPU ;
static operation_t op_CMPX ;
static operation_t op_CMPY ;
static operation_t op_COM  ;
static operation_t op_COMA ;
static operation_t op_COMB ;
static operation_t op_CWAI ;
static operation_t op_DAA  ;
static operation_t op_DEC  ;
static operation_t op_DECA ;
static operation_t op_DECB ;
static operation_t op_EORA ;
static operation_t op_EORB ;
static operation_t op_EXG  ;
static operation_t op_INC  ;
static operation_t op_INCA ;
static operation_t op_INCB ;
static operation_t op_JMP  ;
static operation_t op_JSR  ;
static operation_t op_LBCC ;
static operation_t op_LBEQ ;
static operation_t op_LBGE ;
static operation_t op_LBGT ;
static operation_t op_LBHI ;
static operation_t op_LBLE ;
static operation_t op_LBLO ;
static operation_t op_LBLS ;
static operation_t op_LBLT ;
static operation_t op_LBMI ;
static operation_t op_LBNE ;
static operation_t op_LBPL ;
static operation_t op_LBRA ;
static operation_t op_LBRN ;
static operation_t op_LBSR ;
static operation_t op_LBVC ;
static operation_t op_LBVS ;
static operation_t op_LDA  ;
static operation_t op_LDB  ;
static operation_t op_LDD  ;
static operation_t op_LDS  ;
static operation_t op_LDU  ;
static operation_t op_LDX  ;
static operation_t op_LDY  ;
static operation_t op_LEAS ;
static operation_t op_LEAU ;
static operation_t op_LEAX ;
static operation_t op_LEAY ;
static operation_t op_LSR  ;
static operation_t op_LSRA ;
static operation_t op_LSRB ;
static operation_t op_MUL  ;
static operation_t op_NEG  ;
static operation_t op_NEGA ;
static operation_t op_NEGB ;
static operation_t op_NOP  ;
static operation_t op_ORA  ;
static operation_t op_ORB  ;
static operation_t op_ORCC ;
static operation_t op_PSHS ;
static operation_t op_PSHU ;
static operation_t op_PULS ;
static operation_t op_PULU ;
static operation_t op_ROL  ;
static operation_t op_ROLA ;
static operation_t op_ROLB ;
static operation_t op_ROR  ;
static operation_t op_RORA ;
static operation_t op_RORB ;
static operation_t op_RTS  ;
static operation_t op_SBCA ;
static operation_t op_SBCB ;
static operation_t op_SEX  ;
static operation_t op_STA  ;
static operation_t op_STB  ;
static operation_t op_STD  ;
static operation_t op_STS  ;
static operation_t op_STU  ;
static operation_t op_STX  ;
static operation_t op_STY  ;
static operation_t op_SUBA ;
static operation_t op_SUBB ;
static operation_t op_SUBD ;
static operation_t op_SWI  ;
static operation_t op_SWI2 ;
static operation_t op_SWI3 ;
static operation_t op_SYNC ;
static operation_t op_TFR  ;
static operation_t op_TST  ;
static operation_t op_TSTA ;
static operation_t op_TSTB ;
static operation_t op_UU   ;
static operation_t op_XX   ;

// Undocumented

static operation_t op_XNC  ; // Neg/Com depending on the C flag
static operation_t op_XRES ; // Reset

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

static instr_mode_t *get_instruction(uint8_t b0, uint8_t b1) {
   if (b0 == 0x11) {
      return instr_table_6809_map2 + b1;
   } else if (b0 == 0x10) {
      return instr_table_6809_map1 + b1;
   } else {
      return instr_table_6809_map0 + b0;
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

static int get_regp(int i) {
   i &= 15;
   switch(i) {
   case  0: return (A >= 0 && B >= 0) ? ((A << 8) + B) : -1;
   case  1: return X;
   case  2: return Y;
   case  3: return S;
   case  4: return U;
   case  5: return PC;
   case  8: return A;
   case  9: return B;
   case 10: return get_FLAGS();
   case 11: return DP;
   default: return -1;
   }
}

static void set_regp(int i, int val) {
   i &= 15;
   switch(i) {
   case  0: A  = (val >> 8) & 0xff; B = val & 0xff; break;
   case  1: X  = val; break;
   case  2: Y  = val; break;
   case  3: S  = val; break;
   case  4: U  = val; break;
   case  5: PC = val; break;
   case  8: A  = val; break;
   case  9: B  = val; break;
   case 10: set_FLAGS(val); break;
   case 11: DP = val; break;
   }
}


// ====================================================================
// Public Methods
// ====================================================================

static void em_6809_init(arguments_t *args) {
   show_cycle_errors = args->show_cycles;
}

static int em_6809_match_interrupt(sample_t *sample_q, int num_samples) {
   // FIQ:
   //    m +  7   addr=6 ba=0 bs=1
   //    m +  8   addr=7 ba=0 bs=1
   //    m +  9   addr=X ba=0 bs=0
   //    m + 10   <Start of first instruction>
   //
   if (sample_q[7].ba == 0 && sample_q[7].bs == 1 && sample_q[7].addr == 0x6) {
      return 10;
   }
   // IRQ:
   //    m + 16    addr=8 ba=0 bs=1
   //    m + 17    addr=9 ba=0 bs=1
   //    m + 18    addr=X ba=0 bs=0
   //    m + 19    <Start of first instruction>
   //
   if (sample_q[16].ba == 0 && sample_q[16].bs == 1 && sample_q[16].addr == 0x8) {
      return 19;
   }
   // NMI:
   //    m + 16    addr=C ba=0 bs=1
   //    m + 17    addr=D ba=0 bs=1
   //    m + 18    addr=X ba=0 bs=0
   //    m + 19    <Start of first instruction>
   //
   if (sample_q[16].ba == 0 && sample_q[16].bs == 1 && sample_q[16].addr == 0xC) {
      return 19;
   }
   return 0;
}

static int em_6809_match_reset(sample_t *sample_q, int num_samples) {
   // i        addr=E ba=0 bs=1
   // i + 1    addr=F ba=0 bs=1
   // i + 2    addr=X ba=0 bs=0
   // <Start of first instruction>
   for (int i = 0; i < num_samples - 3; i++) {
      if (sample_q[i].ba == 0 && sample_q[i].bs == 1 && sample_q[i].addr == 0x0E) {
         return i + 3;
      }
   }
   return 0;
}

static int postbyte_cycles[] = { 2, 3, 2, 3, 0, 1, 1, 0, 1, 4, 0, 4, 1, 5, 0, 2 };

static int count_bits[] =    { 0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4};

static int get_num_cycles(sample_t *sample_q) {
   uint8_t b0 = sample_q[0].data;
   uint8_t b1 = sample_q[1].data;
   instr_mode_t *instr = get_instruction(b0, b1);
   int cycle_count = instr->cycles;
   // Long Branch, one additional cycle if branch taken
   if (b0 == 0x10) {
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
      // RTI takes 9 addition cycles if E = 1
      cycle_count += 9;
   } else if (b0 >= 0x34 && b0 <= 0x37) {
      // PSHS/PULS/PSHU/PULU
      cycle_count += count_bits[b1 & 0x0f];            // bits 0..3 are 8 bit registers
      cycle_count += count_bits[(b1 >> 4) & 0x0f] * 2; // bits 4..7 are 16 bit registers
   } else if (instr->mode == INDEXED) {
      // For INDEXED address, the instruction table cycles
      // are the minimum the instruction will execute in
      int postindex = (b0 == 0x10 || b0 == 0x11) ? 2 : 1;
      int postbyte = sample_q[postindex].data;
      if (postbyte & 0x80) {
         cycle_count += postbyte_cycles[postbyte & 0x0F];
         if (postbyte & 0x10) {
            cycle_count += 3;
         }
      } else {
         cycle_count += 1;
      }
   }
   return cycle_count;
}

static int count_cycles_with_lic(sample_t *sample_q) {
   for (int i = 0; i < LONGEST_INSTRUCTION; i++) {
      if (sample_q[i].type == LAST) {
         return 0;
      }
      if (sample_q[i].lic == 1) {
         i++;
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
   A = -1;
   B = -1;
   X = -1;
   Y = -1;
   S = -1;
   U = -1;
   DP = 0;
   E = -1;
   F = 1;
   H = -1;
   I = 1;
   Z = -1;
   N = -1;
   V = -1;
   C = -1;
   PC = (sample_q[num_cycles - 3].data << 8) + sample_q[num_cycles - 2].data;
}

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
      if (B >= 0 && b != B) {
         failflag |= FAIL_B;
      }
      B = b;

      int a  = sample_q[i++].data;
      push8s(a);
      if (A >= 0 && a != A) {
         failflag |= FAIL_A;
      }
      A = a;
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

   // Return the old PC value that was pushed to the stack
   return pc;
}

static void em_6809_interrupt(sample_t *sample_q, int num_cycles, instruction_t *instruction) {
   // Try to establish the type of interrupt
   if (num_cycles == 10) {
      // FIQ
      instruction->pc = interrupt_helper(sample_q, 3, 0, 0xfff6);
      F = 1; // Mask FIQ
   } else if (num_cycles == 19 && sample_q[16].addr == 0x8) {
      // IRQ
      instruction->pc = interrupt_helper(sample_q, 3, 1, 0xfff8);
   } else if (num_cycles == 19 && sample_q[16].addr == 0xC) {
      // NMI
      instruction->pc = interrupt_helper(sample_q, 3, 1, 0xfffC);
      F = 1; // Mask FIQ
   } else {
      printf("*** could not determine interrupt type ***\n");
   }
   I = 1; // Mask IRQ
}

static void em_6809_emulate(sample_t *sample_q, int num_cycles, instruction_t *instruction) {

   instr_mode_t *instr;
   int index = 0;
   int oi = 0;

   instruction->prefix = 0;
   instruction->opcode = sample_q[index].data;
   if (PC >= 0) {
      memory_read(instruction->opcode, PC + index, MEM_FETCH);
   }
   index++;
   instr = instr_table_6809_map0;

   if (instr->undocumented) {
      failflag |= FAIL_UNDOC;
   }

   // Handle the 0x10/0x11 prefixes
   if (instruction->opcode == 0x10 || instruction->opcode == 0x11) {
      // The first byte is the prefix
      instruction->prefix = instruction->opcode;
      // The second byte is the opcode
      instruction->opcode = sample_q[index].data;
      if (PC >= 0) {
         memory_read(instruction->opcode, PC + index, MEM_FETCH);
      }
      index++;
      instr = instruction->prefix == 0x11 ? instr_table_6809_map2 : instr_table_6809_map1;
      oi = 1;
   }

   // Move down to the instruction metadata
   instr += instruction->opcode;

   switch (instr->mode) {
   case REGISTER:
      instruction->postbyte = sample_q[index].data;
      if (PC >= 0) {
         memory_read(instruction->postbyte, PC + index, MEM_INSTR);
      }
      index++;
      break;
   case INDEXED:
      instruction->postbyte = sample_q[index].data;
      if (PC >= 0) {
         memory_read(instruction->postbyte, PC + index, MEM_INSTR);
      }
      index++;
      if (instruction->postbyte & 0x80) {
         int type = instruction->postbyte & 0x0f;
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
   case IMMEDIATE_8:
   case RELATIVE_8:
   case DIRECT:
      if (PC >= 0) {
         memory_read(sample_q[index].data, PC + index, MEM_INSTR);
      }
      index++;
      break;
   case IMMEDIATE_16:
   case RELATIVE_16:
   case EXTENDED:
      if (PC >= 0) {
         memory_read(sample_q[index].data, PC + index, MEM_INSTR);
         memory_read(sample_q[index + 1].data, PC + index + 1, MEM_INSTR);
      }
      index += 2;
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
   if (instr->op == &op_TST) {
      // There are two dead cycles at the end of TST
      operand = sample_q[num_cycles - 3].data;
   } else if (instr->mode == REGISTER) {
      operand = sample_q[1].data; // This is the postbyte
   } else if (instr->op->type == RMWOP) {
      // Read-modify-wrie instruction (always 8-bit)
      operand = sample_q[num_cycles - 3].data;
   } else if (instr->op->size16) {
      if (instr->op->type == LOADOP || instr->op->type == STOREOP || instr->op->type == JSROP) {
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
      if (instr->op->size16) {
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
   case EXTENDED:
      ea = (sample_q[oi + 1].data << 8) + sample_q[oi + 2].data;
      break;
   case INDEXED:
      {
         int pb = instruction->postbyte;
         int *reg = get_regi((pb >> 5) & 0x03);
         if (!(pb & 0x80)) {       /* n4,R */
            if (*reg >= 0) {
               if (pb & 0x10) {
                  ea = (*reg - ((pb & 0x0f) ^ 0x0f) - 1) & 0xffff;
               } else {
                  ea = (*reg + (pb & 0x0f)) & 0xffff;
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
               if (*reg >= 0 && B >= 0) {
                  ea = (*reg + B) & 0xffff;
               }
               break;
            case 6:                 /* A,R */
               if (*reg >= 0 && A >= 0) {
                  ea = (*reg + A) & 0xffff;
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
            case 11:                /* D,R */
               if (*reg >= 0 && A >= 0 && B >= 0) {
                  ea = (*reg + (A << 8) + B) & 0xffff;
               }
               break;
            case 12:                /* n7,PCR */
               if (PC >= 0) {
                  ea = (PC + (int8_t)(sample_q[oi + 2].data)) & 0xffff;
               }
               break;
            case 13:                /* n15,PCR */
               if (PC >= 0) {
                  ea = (PC + (int16_t)((sample_q[oi + 2].data << 8) + sample_q[oi + 2].data)) & 0xffff;
               }
               break;
            case 15:                /* [n] */
               ea = ((sample_q[oi + 2].data << 8) + sample_q[oi + 3].data) & 0xffff;
               break;
            }
            if (pb & 0x10) {
               // In this mode there is a further level of indirection to find the ea
               int offset = oi + indirect_offset[pb & 0x0F];
               if (offset) {
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
   default:
      break;
   }

   // Model memory reads
   if (ea >= 0 && (instr->op->type == LOADOP || instr->op->type == READOP || instr->op->type == RMWOP)) {
      if (instr->op->size16) {
         memory_read((operand  >> 8) & 0xff,  ea,     MEM_DATA);
         memory_read( operand        & 0xff,  ea + 1, MEM_DATA);
      } else {
         memory_read( operand        & 0xff,  ea,     MEM_DATA);
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
            if (instr->op->size16) {
               memory_write((operand2 >> 8) & 0xff,  ea,     MEM_DATA);
               memory_write( operand2       & 0xff,  ea + 1, MEM_DATA);
            } else {
               memory_write( operand2       & 0xff,  ea,     MEM_DATA);
            }
         }
      }
   }
}

static char *strinsert(char *ptr, const char *str) {
   while (*str) {
      *ptr++ = *str++;
   }
   return ptr;
}

static int em_6809_disassemble(char *buffer, instruction_t *instruction) {
   uint8_t b0 = instruction->instr[0];
   uint8_t b1 = instruction->instr[1];
   instr_mode_t *instr = get_instruction(b0, b1);

   // Work out where in the instruction the operand is
   // [Prefix] Opcode [ Postbyte] Op1 Op2
   int oi = 1;
   if (b0 == 0x10 || b0 == 0x11) {
      // Skip over the prefix
      oi++;
   }
   if (instr->mode == INDEXED) {
      // Skip over the post byte
      oi++;
   }
   int op8 = instruction->instr[oi];
   int op16 = (instruction->instr[oi] << 8) + instruction->instr[oi + 1];

   /// Output the mnemonic
   char *ptr = buffer;
   strcpy(ptr, instr->op->mnemonic);
   ptr += 4;
   *ptr++ = ' ';

   // Output the operand
   switch (instr->mode) {
   case INHERENT:
      break;
   case REGISTER:
      {
         int pb = instruction->postbyte;
         switch (instruction->opcode) {
         case 0x1a:
         case 0x1c:
            /* orr, andc */
            *ptr++ = '#';
            *ptr++ = '$';
            write_hex2(ptr, pb);
            ptr += 2;
            break;
         case 0x1e:
         case 0x1f:
            /* exg tfr */
            ptr = strinsert(ptr, exgi[(pb >> 4) & 0x0f]);
            *ptr++ = ',';
            ptr = strinsert(ptr, exgi[pb & 0x0f]);
            break;
         case 0x34:                         /* pshs */
            {
               int p = 0;
               for (int i = 0; i < 8; i++) {
                  if (pb & 0x80) {
                     if (p) {
                        *ptr++ = ',';
                     }
                     ptr = strinsert(ptr, pshsregi[i]);
                     p = 1;
                  }
                  pb <<= 1;
               }
            }
            break;
         case 0x35:                         /* puls */
            {
               int p = 0;
               for (int i = 7; i >= 0; i--) {
                  if (pb & 0x01) {
                     if (p) {
                        *ptr++ = ',';
                     }
                     ptr = strinsert(ptr, pshsregi[i]);
                     p = 1;
                  }
                  pb >>= 1;
               }
            }
            break;
         case 0x36:                         /* pshu */
            {
               int p = 0;
               for (int i = 0; i < 8; i++) {
                  if (pb & 0x80) {
                     if (p) {
                        *ptr++ = ',';
                     }
                     ptr = strinsert(ptr, pshuregi[i]);
                     p = 1;
                  }
                  pb <<= 1;
               }
            }
            break;
         case 0x37:                         /* pulu */
            {
               int p = 0;
               for (int i = 7; i >= 0; i--) {
                  if (pb & 0x01) {
                     if (p) {
                        *ptr++ = ',';
                     }
                     ptr = strinsert(ptr, pshuregi[i]);
                     p = 1;
                  }
                  pb >>= 1;
               }
            }
            break;
         }
      }
      break;
   case IMMEDIATE_8:
      *ptr++ = '#';
      *ptr++ = '$';
      write_hex2(ptr, op8);
      ptr += 2;
      break;
   case IMMEDIATE_16:
      *ptr++ = '#';
      *ptr++ = '$';
      write_hex4(ptr, op16);
      ptr += 4;
      break;
   case RELATIVE_8:
   case RELATIVE_16:
      {
         int16_t offset;
         if (instr->mode == RELATIVE_8) {
            offset = (int16_t)((int8_t)op8);
         } else {
            offset = (int16_t)(op16);
         }
         if (instruction->pc < 0) {
            if (offset < 0) {
               ptr += sprintf(ptr, "pc-%d", -offset);
            } else {
               ptr += sprintf(ptr, "pc+%d", offset);
            }
         } else {
            *ptr++ = '$';
            write_hex4(ptr, (instruction->pc + instruction->length + offset) & 0xffff);
            ptr += 4;
         }
      }
      break;
   case DIRECT:
      *ptr++ = '$';
      write_hex2(ptr, op8);
      ptr += 2;
      break;
   case EXTENDED:
      *ptr++ = '$';
      write_hex4(ptr, op16);
      ptr += 4;
      break;
   case INDEXED:
      {
         int pb = instruction->postbyte;
         char reg = regi[(pb >> 5) & 0x03];
         if (!(pb & 0x80)) {       /* n4,R */
            if (pb & 0x10) {
               *ptr++ = '-';
               *ptr++ = '$';
               write_hex2(ptr, ((pb & 0x0f) ^ 0x0f) + 1);
            } else {
               *ptr++ = '$';
               write_hex2(ptr, pb & 0x0f);
            }
            ptr += 2;
            *ptr++ = ',';
            *ptr++ = reg;
         } else {
            if (pb & 0x10) {
               *ptr++ = '[';
            }
            switch (pb & 0x0f) {
            case 0:                 /* ,R+ */
               *ptr++ = ',';
               *ptr++ = reg;
               *ptr++ = '+';
               break;
            case 1:                 /* ,R++ */
               *ptr++ = ',';
               *ptr++ = reg;
               *ptr++ = '+';
               *ptr++ = '+';
               break;
            case 2:                 /* ,-R */
               *ptr++ = ',';
               *ptr++ = '-';
               *ptr++ = reg;
               break;
            case 3:                 /* ,--R */
               *ptr++ = ',';
               *ptr++ = '-';
               *ptr++ = '-';
               *ptr++ = reg;
               break;
            case 4:                 /* ,R */
               *ptr++ = ',';
               *ptr++ = reg;
               break;
            case 5:                 /* B,R */
               *ptr++ = 'B';
               *ptr++ = ',';
               *ptr++ = reg;
               break;
            case 6:                 /* A,R */
               *ptr++ = 'A';
               *ptr++ = ',';
               *ptr++ = reg;
               break;
            case 8:                 /* n7,R */
               *ptr++ = '$';
               write_hex2(ptr, op8);
               ptr += 2;
               *ptr++ = ',';
               *ptr++ = reg;
               break;
            case 9:                 /* n15,R */
               *ptr++ = '$';
               write_hex4(ptr, op16);
               ptr += 4;
               *ptr++ = ',';
               *ptr++ = reg;
               break;
            case 11:                /* D,R */
               *ptr++ = 'D';
               *ptr++ = ',';
               *ptr++ = reg;
               break;
            case 12:                /* n7,PCR */
               *ptr++ = '$';
               write_hex2(ptr, op8);
               ptr += 2;
               *ptr++ = ',';
               *ptr++ = 'P';
               *ptr++ = 'C';
               *ptr++ = 'R';
               break;
            case 13:                /* n15,PCR */
               *ptr++ = '$';
               write_hex4(ptr, op16);
               ptr += 4;
               *ptr++ = ',';
               *ptr++ = 'P';
               *ptr++ = 'C';
               *ptr++ = 'R';
               break;
            case 15:                /* [n] */
               *ptr++ = '$';
               write_hex4(ptr, op16);
               ptr += 4;
               break;
            default:
               *ptr++ = '?';
               *ptr++ = '?';
               break;
            }
            if (pb & 0x10) {
               *ptr++ = ']';
            }
         }
      }
      break;
   default:
      break;
   }
   return ptr - buffer;
}

static int em_6809_get_PC() {
   return PC;
}


static int em_6809_read_memory(int address) {
   return memory_read_raw(address);
}

static char *em_6809_get_state(char *buffer) {
   strcpy(buffer, default_state);
   if (A >= 0) {
      write_hex2(buffer + OFFSET_A, A);
   }
   if (B >= 0) {
      write_hex2(buffer + OFFSET_B, B);
   }
   if (X >= 0) {
      write_hex4(buffer + OFFSET_X, X);
   }
   if (Y >= 0) {
      write_hex4(buffer + OFFSET_Y, Y);
   }
   if (S >= 0) {
      write_hex4(buffer + OFFSET_S, S);
   }
   if (U >= 0) {
      write_hex4(buffer + OFFSET_U, U);
   }
   if (DP >= 0) {
      write_hex2(buffer + OFFSET_DP, DP);
   }
   if (E >= 0) {
      buffer[OFFSET_E] = '0' + E;
   }
   if (F >= 0) {
      buffer[OFFSET_F] = '0' + F;
   }
   if (H >= 0) {
      buffer[OFFSET_H] = '0' + H;
   }
   if (I >= 0) {
      buffer[OFFSET_I] = '0' + I;
   }
   if (N >= 0) {
      buffer[OFFSET_N] = '0' + N;
   }
   if (Z >= 0) {
      buffer[OFFSET_Z] = '0' + Z;
   }
   if (V >= 0) {
      buffer[OFFSET_V] = '0' + V;
   }
   if (C >= 0) {
      buffer[OFFSET_C] = '0' + C;
   }
   return buffer + OFFSET_END;
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
   .disassemble = em_6809_disassemble,
   .get_PC = em_6809_get_PC,
   .read_memory = em_6809_read_memory,
   .get_state = em_6809_get_state,
   .get_and_clear_fail = em_6809_get_and_clear_fail,
   .write_fail = em_6809_write_fail,
};

// ====================================================================
// Individual Instructions
// ====================================================================

static int op_fn_ABX(operand_t operand, ea_t ea, sample_t *sample_q) {
   // X = X + B
   if (X >= 0 && B >= 0) {
      X = (X + B) & 0xFFFF;
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
      // TODO: Check this
      V = (((val ^ operand ^ tmp) >> 7) & 1) ^ C;
      // TODO: Check this
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
   A = add_helper(A, C, operand);
   return -1;
}

static int op_fn_ADCB(operand_t operand, ea_t ea, sample_t *sample_q) {
   B = add_helper(B, C, operand);
   return -1;
}

static int op_fn_ADDA(operand_t operand, ea_t ea, sample_t *sample_q) {
   A = add_helper(A, 0, operand);
   return -1;
}

static int op_fn_ADDB(operand_t operand, ea_t ea, sample_t *sample_q) {
   B = add_helper(B, 0, operand);
   return -1;
}

static int op_fn_ADDD(operand_t operand, ea_t ea, sample_t *sample_q) {
   if (A >= 0 && B >= 0) {
      int d = (A << 8) + B;
      // Perform the addition (there is no carry in)
      int tmp = d + operand;
      // The carry flag is bit 16 of the result
      C = (tmp >> 16) & 1;
      // TODO: Check this
      V = (((d ^ operand ^ tmp) >> 15) & 1) ^ C;
      // Truncate the result to 16 bits
      tmp &= 0xFFFF;
      // Set the flags
      set_NZ16(tmp);
      // Unpack back into A and B
      A = (tmp >> 8) & 0xff;
      B = tmp & 0xff;
   } else {
      A = -1;
      B = -1;
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
   A = and_helper(A, operand);
   return -1;
}

static int op_fn_ANDB(operand_t operand, ea_t ea, sample_t *sample_q) {
   B = and_helper(B, operand);
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
      H = -1;
      set_NZ(val);
   } else {
      set_HNZVC_unknown();
   }
   return val;
}

static int op_fn_ASL(operand_t operand, ea_t ea, sample_t *sample_q) {
   return asl_helper(operand);
}

static int op_fn_ASLA(operand_t operand, ea_t ea, sample_t *sample_q) {
   A = asl_helper(A);
   return -1;
}

static int op_fn_ASLB(operand_t operand, ea_t ea, sample_t *sample_q) {
   B = asl_helper(B);
   return -1;
}

static int asr_helper(int val) {
   if (val >= 0) {
      C = val & 1;
      val = (val & 0x80) | (val >> 1);
      H = -1;
      set_NZ(val);
   } else {
      set_HNZVC_unknown();
   }
   return val;
}

static int op_fn_ASR(operand_t operand, ea_t ea, sample_t *sample_q) {
   return asr_helper(operand);
}

static int op_fn_ASRA(operand_t operand, ea_t ea, sample_t *sample_q) {
   A = asr_helper(A);
   return -1;
}

static int op_fn_ASRB(operand_t operand, ea_t ea, sample_t *sample_q) {
   B = asr_helper(B);
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
   bit_helper(A, operand);
   return -1;
}

static int op_fn_BITB(operand_t operand, ea_t ea, sample_t *sample_q) {
   bit_helper(B, operand);
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
   A = clr_helper();
   return -1;
}

static int op_fn_CLRB(operand_t operand, ea_t ea, sample_t *sample_q) {
   B = clr_helper();
   return -1;
}

static void cmp_helper8(int val, operand_t operand) {
   if (val >= 0) {
      int tmp = val - operand;
      C = (tmp >> 8) & 1;
      // TODO: Check this
      V = ((val ^ tmp) & (val ^ operand)) >> 7;
      tmp &= 0xff;
      set_NZ(tmp);
      H = -1;
   } else {
      set_HNZVC_unknown();
   }
}

static void cmp_helper16(int val, operand_t operand) {
   if (val >= 0) {
      int tmp = val - operand;
      C = (tmp >> 16) & 1;
      // TODO: Check this
      V = ((val ^ tmp) & (val ^ operand)) >> 15;
      tmp &= 0xffff;
      set_NZ16(tmp);
   } else {
      set_NZVC_unknown();
   }
}

static int op_fn_CMPA(operand_t operand, ea_t ea, sample_t *sample_q) {
   cmp_helper8(A, operand);
   return -1;
}

static int op_fn_CMPB(operand_t operand, ea_t ea, sample_t *sample_q) {
   cmp_helper8(B, operand);
   return -1;
}

static int op_fn_CMPD(operand_t operand, ea_t ea, sample_t *sample_q) {
   int D = (A << 8) + B;
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
   A = com_helper(A);
   return -1;
}

static int op_fn_COMB(operand_t operand, ea_t ea, sample_t *sample_q) {
   B = com_helper(B);
   return -1;
}

static int op_fn_CWAI(operand_t operand, ea_t ea, sample_t *sample_q) {
   // TODO
   return -1;
}

static int op_fn_DAA(operand_t operand, ea_t ea, sample_t *sample_q) {
   if (A >= 0 && H >= 0 && C >= 0) {
      int correction = 0x00;
      if (H == 1 && (A & 0x0F) > 0x09) {
         correction |= 0x06;
      }
      if (C == 1 || (A & 0xF0) > 0x90 || ((A & 0xF0) > 0x80 && (A & 0x0F) > 0x09)) {
         correction |= 0x60;
      }
      int tmp = A + correction;
      C = (tmp >> 8) & 1;
      tmp &= 0xff;
      set_NZ(tmp);
      A = tmp;
   } else {
      A = -1;
      set_NZC_unknown();
   }
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
   A = dec_helper(A);
   return -1;
}

static int op_fn_DECB(operand_t operand, ea_t ea, sample_t *sample_q) {
   B = dec_helper(B);
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
   A = eor_helper(A, operand);
   return -1;
}

static int op_fn_EORB(operand_t operand, ea_t ea, sample_t *sample_q) {
   B = eor_helper(B, operand);
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
   A = inc_helper(A);
   return -1;
}

static int op_fn_INCB(operand_t operand, ea_t ea, sample_t *sample_q) {
   B = inc_helper(B);
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
   A = ld_helper8(operand);
   return -1;
}

static int op_fn_LDB(operand_t operand, ea_t ea, sample_t *sample_q) {
   B = ld_helper8(operand);
   return -1;
}

static int op_fn_LDD(operand_t operand, ea_t ea, sample_t *sample_q) {
   int tmp = ld_helper16(operand);
   A = (tmp >> 8) & 0xff;
   B = tmp & 0xff;
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
   A = lsr_helper(A);
   return -1;
}

static int op_fn_LSRB(operand_t operand, ea_t ea, sample_t *sample_q) {
   B = lsr_helper(B);
   return -1;
}

static int op_fn_MUL(operand_t operand, ea_t ea, sample_t *sample_q) {
   // D = A * B
   if (A >= 0 && B >= 0) {
      uint16_t tmp = A * B;
      A = (tmp >> 8) & 0xff;
      B = tmp & 0xff;
      Z = (tmp == 0);
      C = (B >> 7) & 1;
   } else {
      A = -1;
      B = -1;
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
   return val;
}

static int op_fn_NEG(operand_t operand, ea_t ea, sample_t *sample_q) {
   return neg_helper(operand);
}

static int op_fn_NEGA(operand_t operand, ea_t ea, sample_t *sample_q) {
   A = neg_helper(A);
   return -1;
}

static int op_fn_NEGB(operand_t operand, ea_t ea, sample_t *sample_q) {
   B = neg_helper(B);
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
   A = or_helper(A, operand);
   return -1;
}

static int op_fn_ORB(operand_t operand, ea_t ea, sample_t *sample_q) {
   B = or_helper(B, operand);
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
      if (B >= 0 && B != tmp) {
         failflag |= FAIL_B;
      }
      B = tmp;
   }
   if (pb & 0x02) {
      tmp = sample_q[i++].data;
      push8(tmp);
      if (A >= 0 && A != tmp) {
         failflag |= FAIL_A;
      }
      A = tmp;
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
      A = tmp;
   }
   if (pb & 0x04) {
      tmp = sample_q[i++].data;
      pop8(tmp);
      B = tmp;
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
   A = rol_helper(A);
   return -1;
}

static int op_fn_ROLB(operand_t operand, ea_t ea, sample_t *sample_q) {
   B = rol_helper(B);
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
      set_NZVC_unknown();
   }
   return val;
}

static int op_fn_ROR(operand_t operand, ea_t ea, sample_t *sample_q) {
   return ror_helper(operand);
}

static int op_fn_RORA(operand_t operand, ea_t ea, sample_t *sample_q) {
   A = ror_helper(A);
   return -1;
}

static int op_fn_RORB(operand_t operand, ea_t ea, sample_t *sample_q) {
   B = ror_helper(B);
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

   // Number of 8-bit items unstacked
   int n = (E == 1) ? 12 : 3;
   for (int i = 2; i < 2 + n; i++) {
      pop8s(sample_q[i].data);
   }

   // Update the register state
   if (E == 1) {
      A  = sample_q[3].data;
      B  = sample_q[4].data;
      DP = sample_q[5].data;
      X  = (sample_q[6].data << 8) + sample_q[7].data;
      Y  = (sample_q[8].data << 8) + sample_q[9].data;
      U  = (sample_q[10].data << 8) + sample_q[11].data;
      PC = (sample_q[12].data << 8) + sample_q[13].data;
   } else {
      PC = (sample_q[3].data << 8) + sample_q[4].data;
   }

   // Do the flags last, so as not to clobber the E test above
   set_FLAGS(sample_q[2].data);

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
      // TODO: Check this
      V = (((val ^ operand ^ tmp) >> 7) & 1) ^ C;
      // TODO: Check this
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

static int op_fn_SBCA(operand_t operand, ea_t ea, sample_t *sample_q) {
   A = sub_helper(A, C, operand);
   return -1;
}

static int op_fn_SBCB(operand_t operand, ea_t ea, sample_t *sample_q) {
   B = sub_helper(B, C, operand);
   return -1;
}

static int op_fn_SEX(operand_t operand, ea_t ea, sample_t *sample_q) {
   if (B >= 0) {
      if (B & 0x80) {
         A = 0xFF;
      } else {
         A = 0x00;
      }
      set_NZ(B);
   } else {
      A = -1;
      set_NZ_unknown();
   }
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
   A = st_helper8(A, operand, FAIL_A);
   return operand;
}

static int op_fn_STB(operand_t operand, ea_t ea, sample_t *sample_q) {
   B = st_helper8(B, operand, FAIL_B);
   return operand;
}

static int op_fn_STD(operand_t operand, ea_t ea, sample_t *sample_q) {
   int D = (A >= 0 && B >= 0) ? (A << 8) + B : -1;
   D = st_helper16(D, operand, FAIL_A | FAIL_B);
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
   A = sub_helper(A, 0, operand);
   return -1;
}

static int op_fn_SUBB(operand_t operand, ea_t ea, sample_t *sample_q) {
   B = sub_helper(B, 0, operand);
   return -1;
}

static int op_fn_SUBD(operand_t operand, ea_t ea, sample_t *sample_q) {
   if (A >= 0 && B >= 0) {
      int d = (A << 8) + B;
      // Perform the addition (there is no carry in)
      int tmp = d - operand;
      // The carry flag is bit 16 of the result
      C = (tmp >> 16) & 1;
      // TODO: Check this
      V = (((d ^ operand ^ tmp) >> 15) & 1) ^ C;
      // Truncate the result to 16 bits
      tmp &= 0xFFFF;
      // Set the flags
      set_NZ16(tmp);
      // Unpack back into A and B
      A = (tmp >> 8) & 0xff;
      B = tmp & 0xff;
   } else {
      A = -1;
      B = -1;
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
   return -1;
}

static int op_fn_TSTA(operand_t operand, ea_t ea, sample_t *sample_q) {
   set_NZ(A);
   return -1;
}

static int op_fn_TSTB(operand_t operand, ea_t ea, sample_t *sample_q) {
   set_NZ(B);
   return -1;
}

static int op_fn_UU(operand_t operand, ea_t ea, sample_t *sample_q) {
   return -1;
}

static int op_fn_XX(operand_t operand, ea_t ea, sample_t *sample_q) {
   return -1;
}


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

static int op_fn_XRES(operand_t operand, ea_t ea, sample_t *sample_q) {
   interrupt_helper(sample_q, 3, 1, 0xfffe);
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
static operation_t op_XX   = { "??? ", op_fn_XX,       OTHER , 0 };
static operation_t op_XNC  = { "XNC ", op_fn_XNC,     READOP , 0 };
static operation_t op_XRES = { "XRES", op_fn_XRES,     OTHER , 0 };


static instr_mode_t instr_table_6809_map0[] = {
   /* 00 */    { &op_NEG , DIRECT       , 6, 0 },
   /* 01 */    { &op_NEG , DIRECT       , 6, 1 },
   /* 02 */    { &op_XNC , DIRECT       , 6, 1 },
   /* 03 */    { &op_COM , DIRECT       , 6, 0 },
   /* 04 */    { &op_LSR , DIRECT       , 6, 0 },
   /* 05 */    { &op_LSR , DIRECT       , 6, 1 },
   /* 06 */    { &op_ROR , DIRECT       , 6, 0 },
   /* 07 */    { &op_ASR , DIRECT       , 6, 0 },
   /* 08 */    { &op_ASL , DIRECT       , 6, 0 },
   /* 09 */    { &op_ROL , DIRECT       , 6, 0 },
   /* 0A */    { &op_DEC , DIRECT       , 6, 0 },
   /* 0B */    { &op_DEC , DIRECT       , 6, 1 },
   /* 0C */    { &op_INC , DIRECT       , 6, 0 },
   /* 0D */    { &op_TST , DIRECT       , 6, 0 },
   /* 0E */    { &op_JMP , DIRECT       , 3, 0 },
   /* 0F */    { &op_CLR , DIRECT       , 6, 0 },
   /* 10 */    { &op_UU  , INHERENT     , 1, 0 },
   /* 11 */    { &op_UU  , INHERENT     , 1, 0 },
   /* 12 */    { &op_NOP , INHERENT     , 2, 0 },
   /* 13 */    { &op_SYNC, INHERENT     , 2, 0 },
   /* 14 */    { &op_XX  , ILLEGAL      , 1, 1 },  // Seems to be interrupt relatde
   /* 15 */    { &op_XX  , ILLEGAL      , 1, 1 },  // Seems to be interrupt related
   /* 16 */    { &op_LBRA, RELATIVE_16  , 5, 0 },
   /* 17 */    { &op_LBSR, RELATIVE_16  , 9, 0 },
   /* 18 */    { &op_XX  , ILLEGAL      , 1, 1 },  // Seems to be interrupt related
   /* 19 */    { &op_DAA , INHERENT     , 2, 0 },
   /* 1A */    { &op_ORCC, REGISTER     , 3, 0 },
   /* 1B */    { &op_XX  , ILLEGAL      , 1, 1 },  // Seems to be interrupt related
   /* 1C */    { &op_ANDC, REGISTER     , 3, 0 },
   /* 1D */    { &op_SEX , INHERENT     , 2, 0 },
   /* 1E */    { &op_EXG , REGISTER     , 8, 0 },
   /* 1F */    { &op_TFR , REGISTER     , 6, 0 },
   /* 20 */    { &op_BRA , RELATIVE_8   , 3, 0 },
   /* 21 */    { &op_BRN , RELATIVE_8   , 3, 0 },
   /* 22 */    { &op_BHI , RELATIVE_8   , 3, 0 },
   /* 23 */    { &op_BLS , RELATIVE_8   , 3, 0 },
   /* 24 */    { &op_BCC , RELATIVE_8   , 3, 0 },
   /* 25 */    { &op_BLO , RELATIVE_8   , 3, 0 },
   /* 26 */    { &op_BNE , RELATIVE_8   , 3, 0 },
   /* 27 */    { &op_BEQ , RELATIVE_8   , 3, 0 },
   /* 28 */    { &op_BVC , RELATIVE_8   , 3, 0 },
   /* 29 */    { &op_BVS , RELATIVE_8   , 3, 0 },
   /* 2A */    { &op_BPL , RELATIVE_8   , 3, 0 },
   /* 2B */    { &op_BMI , RELATIVE_8   , 3, 0 },
   /* 2C */    { &op_BGE , RELATIVE_8   , 3, 0 },
   /* 2D */    { &op_BLT , RELATIVE_8   , 3, 0 },
   /* 2E */    { &op_BGT , RELATIVE_8   , 3, 0 },
   /* 2F */    { &op_BLE , RELATIVE_8   , 3, 0 },
   /* 30 */    { &op_LEAX, INDEXED      , 4, 0 },
   /* 31 */    { &op_LEAY, INDEXED      , 4, 0 },
   /* 32 */    { &op_LEAS, INDEXED      , 4, 0 },
   /* 33 */    { &op_LEAU, INDEXED      , 4, 0 },
   /* 34 */    { &op_PSHS, REGISTER     , 5, 0 },
   /* 35 */    { &op_PULS, REGISTER     , 5, 0 },
   /* 36 */    { &op_PSHU, REGISTER     , 5, 0 },
   /* 37 */    { &op_PULU, REGISTER     , 5, 0 },
   /* 38 */    { &op_XX  , INHERENT     , 2, 1 },  // CWAIT or something similar
   /* 39 */    { &op_RTS , INHERENT     , 5, 0 },
   /* 3A */    { &op_ABX , INHERENT     , 3, 0 },
   /* 3B */    { &op_RTI , INHERENT     , 6, 0 },
   /* 3C */    { &op_CWAI, IMMEDIATE_8  , 1, 0 },
   /* 3D */    { &op_MUL , INHERENT     ,11, 0 },
   /* 3E */    { &op_XRES, INHERENT     ,19, 1 },
   /* 3F */    { &op_SWI , INHERENT     ,19, 0 },
   /* 40 */    { &op_NEGA, INHERENT     , 2, 0 },
   /* 41 */    { &op_NEGA, INHERENT     , 2, 1 },
   /* 42 */    { &op_COMA, INHERENT     , 2, 1 },
   /* 43 */    { &op_COMA, INHERENT     , 2, 0 },
   /* 44 */    { &op_LSRA, INHERENT     , 2, 0 },
   /* 45 */    { &op_LSRA, INHERENT     , 2, 1 },
   /* 46 */    { &op_RORA, INHERENT     , 2, 0 },
   /* 47 */    { &op_ASRA, INHERENT     , 2, 0 },
   /* 48 */    { &op_ASLA, INHERENT     , 2, 0 },
   /* 49 */    { &op_ROLA, INHERENT     , 2, 0 },
   /* 4A */    { &op_DECA, INHERENT     , 2, 0 },
   /* 4B */    { &op_DECA, INHERENT     , 2, 1 },
   /* 4C */    { &op_INCA, INHERENT     , 2, 0 },
   /* 4D */    { &op_TSTA, INHERENT     , 2, 0 },
   /* 4E */    { &op_CLRA, INHERENT     , 2, 1 },
   /* 4F */    { &op_CLRA, INHERENT     , 2, 0 },
   /* 50 */    { &op_NEGB, INHERENT     , 2, 0 },
   /* 51 */    { &op_NEGB, INHERENT     , 2, 1 },
   /* 52 */    { &op_COMB, INHERENT     , 2, 1 },
   /* 53 */    { &op_COMB, INHERENT     , 2, 0 },
   /* 54 */    { &op_LSRB, INHERENT     , 2, 0 },
   /* 55 */    { &op_LSRB, INHERENT     , 2, 1 },
   /* 56 */    { &op_RORB, INHERENT     , 2, 0 },
   /* 57 */    { &op_ASRB, INHERENT     , 2, 0 },
   /* 58 */    { &op_ASLB, INHERENT     , 2, 0 },
   /* 59 */    { &op_ROLB, INHERENT     , 2, 0 },
   /* 5A */    { &op_DECB, INHERENT     , 2, 0 },
   /* 5B */    { &op_DECB, INHERENT     , 2, 1 },
   /* 5C */    { &op_INCB, INHERENT     , 2, 0 },
   /* 5D */    { &op_TSTB, INHERENT     , 2, 0 },
   /* 5E */    { &op_CLRB, INHERENT     , 2, 1 },
   /* 5F */    { &op_CLRB, INHERENT     , 2, 0 },
   /* 60 */    { &op_NEG , INDEXED      , 6, 0 },
   /* 61 */    { &op_NEG , INDEXED      , 6, 1 },
   /* 62 */    { &op_COM , INDEXED      , 6, 1 },
   /* 63 */    { &op_COM , INDEXED      , 6, 0 },
   /* 64 */    { &op_LSR , INDEXED      , 6, 0 },
   /* 65 */    { &op_LSR , INDEXED      , 6, 1 },
   /* 66 */    { &op_ROR , INDEXED      , 6, 0 },
   /* 67 */    { &op_ASR , INDEXED      , 6, 0 },
   /* 68 */    { &op_ASL , INDEXED      , 6, 0 },
   /* 69 */    { &op_ROL , INDEXED      , 6, 0 },
   /* 6A */    { &op_DEC , INDEXED      , 6, 0 },
   /* 6B */    { &op_DEC , INDEXED      , 6, 1 },
   /* 6C */    { &op_INC , INDEXED      , 6, 0 },
   /* 6D */    { &op_TST , INDEXED      , 6, 0 },
   /* 6E */    { &op_JMP , INDEXED      , 3, 0 },
   /* 6F */    { &op_CLR , INDEXED      , 6, 0 },
   /* 70 */    { &op_NEG , EXTENDED     , 7, 0 },
   /* 71 */    { &op_NEG , EXTENDED     , 7, 1 },
   /* 72 */    { &op_COM , EXTENDED     , 7, 1 },
   /* 73 */    { &op_COM , EXTENDED     , 7, 0 },
   /* 74 */    { &op_LSR , EXTENDED     , 7, 0 },
   /* 75 */    { &op_LSR , EXTENDED     , 7, 1 },
   /* 76 */    { &op_ROR , EXTENDED     , 7, 0 },
   /* 77 */    { &op_ASR , EXTENDED     , 7, 0 },
   /* 78 */    { &op_ASL , EXTENDED     , 7, 0 },
   /* 79 */    { &op_ROL , EXTENDED     , 7, 0 },
   /* 7A */    { &op_DEC , EXTENDED     , 7, 0 },
   /* 7B */    { &op_DEC , EXTENDED     , 7, 1 },
   /* 7C */    { &op_INC , EXTENDED     , 7, 0 },
   /* 7D */    { &op_TST , EXTENDED     , 7, 0 },
   /* 7E */    { &op_JMP , EXTENDED     , 4, 0 },
   /* 7F */    { &op_CLR , EXTENDED     , 7, 0 },
   /* 80 */    { &op_SUBA, IMMEDIATE_8  , 2, 0 },
   /* 81 */    { &op_CMPA, IMMEDIATE_8  , 2, 0 },
   /* 82 */    { &op_SBCA, IMMEDIATE_8  , 2, 0 },
   /* 83 */    { &op_SUBD, IMMEDIATE_16 , 4, 0 },
   /* 84 */    { &op_ANDA, IMMEDIATE_8  , 2, 0 },
   /* 85 */    { &op_BITA, IMMEDIATE_8  , 2, 0 },
   /* 86 */    { &op_LDA , IMMEDIATE_8  , 2, 0 },
   /* 87 */    { &op_XX  , ILLEGAL      , 2, 1 },  // Function unknown
   /* 88 */    { &op_EORA, IMMEDIATE_8  , 2, 0 },
   /* 89 */    { &op_ADCA, IMMEDIATE_8  , 2, 0 },
   /* 8A */    { &op_ORA , IMMEDIATE_8  , 2, 0 },
   /* 8B */    { &op_ADDA, IMMEDIATE_8  , 2, 0 },
   /* 8C */    { &op_CMPX, IMMEDIATE_16 , 4, 0 },
   /* 8D */    { &op_BSR , RELATIVE_8   , 7, 0 },
   /* 8E */    { &op_LDX , IMMEDIATE_16 , 3, 0 },
   /* 8F */    { &op_XX  , ILLEGAL      , 1, 1 },  // Function unknown
   /* 90 */    { &op_SUBA, DIRECT       , 4, 0 },
   /* 91 */    { &op_CMPA, DIRECT       , 4, 0 },
   /* 92 */    { &op_SBCA, DIRECT       , 4, 0 },
   /* 93 */    { &op_SUBD, DIRECT       , 6, 0 },
   /* 94 */    { &op_ANDA, DIRECT       , 4, 0 },
   /* 95 */    { &op_BITA, DIRECT       , 4, 0 },
   /* 96 */    { &op_LDA , DIRECT       , 4, 0 },
   /* 97 */    { &op_STA , DIRECT       , 4, 0 },
   /* 98 */    { &op_EORA, DIRECT       , 4, 0 },
   /* 99 */    { &op_ADCA, DIRECT       , 4, 0 },
   /* 9A */    { &op_ORA , DIRECT       , 4, 0 },
   /* 9B */    { &op_ADDA, DIRECT       , 4, 0 },
   /* 9C */    { &op_CMPX, DIRECT       , 6, 0 },
   /* 9D */    { &op_JSR , DIRECT       , 7, 0 },
   /* 9E */    { &op_LDX , DIRECT       , 5, 0 },
   /* 9F */    { &op_STX , DIRECT       , 5, 0 },
   /* A0 */    { &op_SUBA, INDEXED      , 4, 0 },
   /* A1 */    { &op_CMPA, INDEXED      , 4, 0 },
   /* A2 */    { &op_SBCA, INDEXED      , 4, 0 },
   /* A3 */    { &op_SUBD, INDEXED      , 6, 0 },
   /* A4 */    { &op_ANDA, INDEXED      , 4, 0 },
   /* A5 */    { &op_BITA, INDEXED      , 4, 0 },
   /* A6 */    { &op_LDA , INDEXED      , 4, 0 },
   /* A7 */    { &op_STA , INDEXED      , 4, 0 },
   /* A8 */    { &op_EORA, INDEXED      , 4, 0 },
   /* A9 */    { &op_ADCA, INDEXED      , 4, 0 },
   /* AA */    { &op_ORA , INDEXED      , 4, 0 },
   /* AB */    { &op_ADDA, INDEXED      , 4, 0 },
   /* AC */    { &op_CMPX, INDEXED      , 6, 0 },
   /* AD */    { &op_JSR , INDEXED      , 7, 0 },
   /* AE */    { &op_LDX , INDEXED      , 5, 0 },
   /* AF */    { &op_STX , INDEXED      , 5, 0 },
   /* B0 */    { &op_SUBA, EXTENDED     , 5, 0 },
   /* B1 */    { &op_CMPA, EXTENDED     , 5, 0 },
   /* B2 */    { &op_SBCA, EXTENDED     , 5, 0 },
   /* B3 */    { &op_SUBD, EXTENDED     , 7, 0 },
   /* B4 */    { &op_ANDA, EXTENDED     , 5, 0 },
   /* B5 */    { &op_BITA, EXTENDED     , 5, 0 },
   /* B6 */    { &op_LDA , EXTENDED     , 5, 0 },
   /* B7 */    { &op_STA , EXTENDED     , 5, 0 },
   /* B8 */    { &op_EORA, EXTENDED     , 5, 0 },
   /* B9 */    { &op_ADCA, EXTENDED     , 5, 0 },
   /* BA */    { &op_ORA , EXTENDED     , 5, 0 },
   /* BB */    { &op_ADDA, EXTENDED     , 5, 0 },
   /* BC */    { &op_CMPX, EXTENDED     , 7, 0 },
   /* BD */    { &op_JSR , EXTENDED     , 8, 0 },
   /* BE */    { &op_LDX , EXTENDED     , 6, 0 },
   /* BF */    { &op_STX , EXTENDED     , 6, 0 },
   /* C0 */    { &op_SUBB, IMMEDIATE_8  , 2, 0 },
   /* C1 */    { &op_CMPB, IMMEDIATE_8  , 2, 0 },
   /* C2 */    { &op_SBCB, IMMEDIATE_8  , 2, 0 },
   /* C3 */    { &op_ADDD, IMMEDIATE_16 , 4, 0 },
   /* C4 */    { &op_ANDB, IMMEDIATE_8  , 2, 0 },
   /* C5 */    { &op_BITB, IMMEDIATE_8  , 2, 0 },
   /* C6 */    { &op_LDB , IMMEDIATE_8  , 2, 0 },
   /* C7 */    { &op_XX  , ILLEGAL      , 2, 1 },  // Function unknown
   /* C8 */    { &op_EORB, IMMEDIATE_8  , 2, 0 },
   /* C9 */    { &op_ADCB, IMMEDIATE_8  , 2, 0 },
   /* CA */    { &op_ORB , IMMEDIATE_8  , 2, 0 },
   /* CB */    { &op_ADDB, IMMEDIATE_8  , 2, 0 },
   /* CC */    { &op_LDD , IMMEDIATE_16 , 3, 0 },
   /* CD */    { &op_XX  , ILLEGAL      , 3, 1 },  // Function unknown
   /* CE */    { &op_LDU , IMMEDIATE_16 , 3, 0 },
   /* CF */    { &op_XX  , ILLEGAL      , 3, 1 },  // Function unknown
   /* D0 */    { &op_SUBB, DIRECT       , 4, 0 },
   /* D1 */    { &op_CMPB, DIRECT       , 4, 0 },
   /* D2 */    { &op_SBCB, DIRECT       , 4, 0 },
   /* D3 */    { &op_ADDD, DIRECT       , 6, 0 },
   /* D4 */    { &op_ANDB, DIRECT       , 4, 0 },
   /* D5 */    { &op_BITB, DIRECT       , 4, 0 },
   /* D6 */    { &op_LDB , DIRECT       , 4, 0 },
   /* D7 */    { &op_STB , DIRECT       , 4, 0 },
   /* D8 */    { &op_EORB, DIRECT       , 4, 0 },
   /* D9 */    { &op_ADCB, DIRECT       , 4, 0 },
   /* DA */    { &op_ORB , DIRECT       , 4, 0 },
   /* DB */    { &op_ADDB, DIRECT       , 4, 0 },
   /* DC */    { &op_LDD , DIRECT       , 5, 0 },
   /* DD */    { &op_STD , DIRECT       , 5, 0 },
   /* DE */    { &op_LDU , DIRECT       , 5, 0 },
   /* DF */    { &op_STU , DIRECT       , 5, 0 },
   /* E0 */    { &op_SUBB, INDEXED      , 4, 0 },
   /* E1 */    { &op_CMPB, INDEXED      , 4, 0 },
   /* E2 */    { &op_SBCB, INDEXED      , 4, 0 },
   /* E3 */    { &op_ADDD, INDEXED      , 6, 0 },
   /* E4 */    { &op_ANDB, INDEXED      , 4, 0 },
   /* E5 */    { &op_BITB, INDEXED      , 4, 0 },
   /* E6 */    { &op_LDB , INDEXED      , 4, 0 },
   /* E7 */    { &op_STB , INDEXED      , 4, 0 },
   /* E8 */    { &op_EORB, INDEXED      , 4, 0 },
   /* E9 */    { &op_ADCB, INDEXED      , 4, 0 },
   /* EA */    { &op_ORB , INDEXED      , 4, 0 },
   /* EB */    { &op_ADDB, INDEXED      , 4, 0 },
   /* EC */    { &op_LDD , INDEXED      , 5, 0 },
   /* ED */    { &op_STD , INDEXED      , 5, 0 },
   /* EE */    { &op_LDU , INDEXED      , 5, 0 },
   /* EF */    { &op_STU , INDEXED      , 5, 0 },
   /* F0 */    { &op_SUBB, EXTENDED     , 5, 0 },
   /* F1 */    { &op_CMPB, EXTENDED     , 5, 0 },
   /* F2 */    { &op_SBCB, EXTENDED     , 5, 0 },
   /* F3 */    { &op_ADDD, EXTENDED     , 7, 0 },
   /* F4 */    { &op_ANDB, EXTENDED     , 5, 0 },
   /* F5 */    { &op_BITB, EXTENDED     , 5, 0 },
   /* F6 */    { &op_LDB , EXTENDED     , 5, 0 },
   /* F7 */    { &op_STB , EXTENDED     , 5, 0 },
   /* F8 */    { &op_EORB, EXTENDED     , 5, 0 },
   /* F9 */    { &op_ADCB, EXTENDED     , 5, 0 },
   /* FA */    { &op_ORB , EXTENDED     , 5, 0 },
   /* FB */    { &op_ADDB, EXTENDED     , 5, 0 },
   /* FC */    { &op_LDD , EXTENDED     , 6, 0 },
   /* FD */    { &op_STD , EXTENDED     , 6, 0 },
   /* FE */    { &op_LDU , EXTENDED     , 6, 0 },
   /* FF */    { &op_STU , EXTENDED     , 6, 0 }
};

static instr_mode_t instr_table_6809_map1[] = {
   /* 00 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 01 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 02 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 03 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 04 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 05 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 06 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 07 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 08 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 09 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 0A */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 0B */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 0C */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 0D */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 0E */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 0F */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 10 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 11 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 12 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 13 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 14 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 15 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 16 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 17 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 18 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 19 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 1A */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 1B */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 1C */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 1D */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 1E */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 1F */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 20 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 21 */    { &op_LBRN, RELATIVE_16  , 5, 0 },
   /* 22 */    { &op_LBHI, RELATIVE_16  , 5, 0 },
   /* 23 */    { &op_LBLS, RELATIVE_16  , 5, 0 },
   /* 24 */    { &op_LBCC, RELATIVE_16  , 5, 0 },
   /* 25 */    { &op_LBLO, RELATIVE_16  , 5, 0 },
   /* 26 */    { &op_LBNE, RELATIVE_16  , 5, 0 },
   /* 27 */    { &op_LBEQ, RELATIVE_16  , 5, 0 },
   /* 28 */    { &op_LBVC, RELATIVE_16  , 5, 0 },
   /* 29 */    { &op_LBVS, RELATIVE_16  , 5, 0 },
   /* 2A */    { &op_LBPL, RELATIVE_16  , 5, 0 },
   /* 2B */    { &op_LBMI, RELATIVE_16  , 5, 0 },
   /* 2C */    { &op_LBGE, RELATIVE_16  , 5, 0 },
   /* 2D */    { &op_LBLT, RELATIVE_16  , 5, 0 },
   /* 2E */    { &op_LBGT, RELATIVE_16  , 5, 0 },
   /* 2F */    { &op_LBLE, RELATIVE_16  , 5, 0 },
   /* 30 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 31 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 32 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 33 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 34 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 35 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 36 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 37 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 38 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 39 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 3A */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 3B */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 3C */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 3D */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 3E */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 3F */    { &op_SWI2, INHERENT     ,20, 0 },
   /* 40 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 41 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 42 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 43 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 44 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 45 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 46 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 47 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 48 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 49 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 4A */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 4B */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 4C */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 4D */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 4E */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 4F */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 50 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 51 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 52 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 53 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 54 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 55 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 56 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 57 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 58 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 59 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 5A */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 5B */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 5C */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 5D */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 5E */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 5F */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 60 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 61 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 62 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 63 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 64 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 65 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 66 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 67 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 68 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 69 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 6A */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 6B */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 6C */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 6D */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 6E */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 6F */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 70 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 71 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 72 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 73 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 74 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 75 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 76 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 77 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 78 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 79 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 7A */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 7B */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 7C */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 7D */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 7E */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 7F */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 80 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 81 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 82 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 83 */    { &op_CMPD, IMMEDIATE_16 , 5, 0 },
   /* 84 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 85 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 86 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 87 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 88 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 89 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 8A */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 8B */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 8C */    { &op_CMPY, IMMEDIATE_16 , 5, 0 },
   /* 8D */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 8E */    { &op_LDY , IMMEDIATE_16 , 4, 0 },
   /* 8F */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 90 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 91 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 92 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 93 */    { &op_CMPD, DIRECT       , 7, 0 },
   /* 94 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 95 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 96 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 97 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 98 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 99 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 9A */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 9B */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 9C */    { &op_CMPY, DIRECT       , 7, 0 },
   /* 9D */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 9E */    { &op_LDY , DIRECT       , 6, 0 },
   /* 9F */    { &op_STY , DIRECT       , 6, 0 },
   /* A0 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* A1 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* A2 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* A3 */    { &op_CMPD, INDEXED      , 7, 0 },
   /* A4 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* A5 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* A6 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* A7 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* A8 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* A9 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* AA */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* AB */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* AC */    { &op_CMPY, INDEXED      , 7, 0 },
   /* AD */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* AE */    { &op_LDY , INDEXED      , 6, 0 },
   /* AF */    { &op_STY , INDEXED      , 6, 0 },
   /* B0 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* B1 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* B2 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* B3 */    { &op_CMPD, EXTENDED     , 8, 0 },
   /* B4 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* B5 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* B6 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* B7 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* B8 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* B9 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* BA */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* BB */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* BC */    { &op_CMPY, EXTENDED     , 8, 0 },
   /* BD */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* BE */    { &op_LDY , EXTENDED     , 7, 0 },
   /* BF */    { &op_STY , EXTENDED     , 7, 0 },
   /* C0 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* C1 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* C2 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* C3 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* C4 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* C5 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* C6 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* C7 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* C8 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* C9 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* CA */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* CB */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* CC */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* CD */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* CE */    { &op_LDS , IMMEDIATE_16 , 4, 0 },
   /* CF */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* D0 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* D1 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* D2 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* D3 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* D4 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* D5 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* D6 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* D7 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* D8 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* D9 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* DA */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* DB */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* DC */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* DD */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* DE */    { &op_LDS , DIRECT       , 6, 0 },
   /* DF */    { &op_STS , DIRECT       , 6, 0 },
   /* E0 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* E1 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* E2 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* E3 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* E4 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* E5 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* E6 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* E7 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* E8 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* E9 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* EA */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* EB */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* EC */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* ED */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* EE */    { &op_LDS , INDEXED      , 6, 0 },
   /* EF */    { &op_STS , INDEXED      , 6, 0 },
   /* F0 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* F1 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* F2 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* F3 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* F4 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* F5 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* F6 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* F7 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* F8 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* F9 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* FA */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* FB */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* FC */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* FD */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* FE */    { &op_LDS , EXTENDED     , 7, 0 },
   /* FF */    { &op_STS , EXTENDED     , 7, 0 }
};

static instr_mode_t instr_table_6809_map2[] = {
   /* 00 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 01 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 02 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 03 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 04 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 05 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 06 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 07 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 08 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 09 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 0A */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 0B */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 0C */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 0D */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 0E */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 0F */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 10 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 11 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 12 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 13 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 14 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 15 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 16 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 17 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 18 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 19 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 1A */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 1B */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 1C */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 1D */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 1E */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 1F */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 20 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 21 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 22 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 23 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 24 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 25 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 26 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 27 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 28 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 29 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 2A */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 2B */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 2C */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 2D */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 2E */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 2F */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 30 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 31 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 32 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 33 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 34 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 35 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 36 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 37 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 38 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 39 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 3A */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 3B */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 3C */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 3D */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 3E */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 3F */    { &op_SWI3, INHERENT     ,20, 0 },
   /* 40 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 41 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 42 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 43 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 44 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 45 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 46 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 47 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 48 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 49 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 4A */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 4B */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 4C */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 4D */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 4E */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 4F */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 50 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 51 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 52 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 53 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 54 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 55 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 56 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 57 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 58 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 59 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 5A */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 5B */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 5C */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 5D */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 5E */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 5F */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 60 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 61 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 62 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 63 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 64 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 65 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 66 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 67 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 68 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 69 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 6A */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 6B */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 6C */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 6D */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 6E */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 6F */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 70 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 71 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 72 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 73 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 74 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 75 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 76 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 77 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 78 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 79 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 7A */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 7B */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 7C */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 7D */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 7E */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 7F */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 80 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 81 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 82 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 83 */    { &op_CMPU, IMMEDIATE_16 , 5, 0 },
   /* 84 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 85 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 86 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 87 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 88 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 89 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 8A */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 8B */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 8C */    { &op_CMPS, IMMEDIATE_16 , 5, 0 },
   /* 8D */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 8E */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 8F */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 90 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 91 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 92 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 93 */    { &op_CMPU, DIRECT       , 7, 0 },
   /* 94 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 95 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 96 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 97 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 98 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 99 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 9A */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 9B */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 9C */    { &op_CMPS, DIRECT       , 7, 0 },
   /* 9D */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 9E */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 9F */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* A0 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* A1 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* A2 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* A3 */    { &op_CMPU, INDEXED      , 7, 0 },
   /* A4 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* A5 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* A6 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* A7 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* A8 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* A9 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* AA */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* AB */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* AC */    { &op_CMPS, INDEXED      , 7, 0 },
   /* AD */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* AE */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* AF */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* B0 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* B1 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* B2 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* B3 */    { &op_CMPU, EXTENDED     , 8, 0 },
   /* B4 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* B5 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* B6 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* B7 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* B8 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* B9 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* BA */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* BB */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* BC */    { &op_CMPS, EXTENDED     , 8, 0 },
   /* BD */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* BE */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* BF */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* C0 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* C1 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* C2 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* C3 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* C4 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* C5 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* C6 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* C7 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* C8 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* C9 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* CA */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* CB */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* CC */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* CD */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* CE */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* CF */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* D0 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* D1 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* D2 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* D3 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* D4 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* D5 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* D6 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* D7 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* D8 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* D9 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* DA */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* DB */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* DC */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* DD */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* DE */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* DF */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* E0 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* E1 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* E2 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* E3 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* E4 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* E5 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* E6 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* E7 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* E8 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* E9 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* EA */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* EB */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* EC */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* ED */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* EE */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* EF */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* F0 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* F1 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* F2 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* F3 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* F4 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* F5 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* F6 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* F7 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* F8 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* F9 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* FA */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* FB */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* FC */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* FD */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* FE */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* FF */    { &op_XX  , ILLEGAL      , 1, 1 }
};
