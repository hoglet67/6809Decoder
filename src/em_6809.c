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
// Type Defs
// ====================================================================

typedef enum {
   INHERENT,
   REGISTER,
   IMMEDIATE_8,
   IMMEDIATE_16,
   IMMEDIATE_32,
   RELATIVE_8,
   RELATIVE_16,
   DIRECT,
   DIRECTBIT,
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

typedef enum {
   SIZE_8    = 0,
   SIZE_16   = 1,
   SIZE_32   = 2
} opsize_t;

typedef struct {
   const char *mnemonic;
   int (*emulate)(operand_t, ea_t, sample_t *);
   optype_t type;
   opsize_t size;
} operation_t;

typedef struct {
   operation_t *op;
   addr_mode_t mode;
   int undocumented;
   int cycles;
   int cycles_native;
} instr_mode_t;


// ====================================================================
// Static variables
// ====================================================================

// For the disassember

static const char regi2[] = { 'X', 'Y', 'U', 'S' };

static const char **regi4;

static const char *regi4_6809[] = { "D",  "X",  "Y",  "U",  "S", "PC", "??", "??",
                                    "A",  "B", "CC", "DP", "??", "??", "??", "??" };

static const char *regi4_6309[] = { "D",  "X",  "Y",  "U",  "S", "PC",  "W", "TV",
                                    "A",  "B", "CC", "DP",  "0",  "0",  "E",  "F" };

static const char *pshsregi[] = { "PC", "U", "Y", "X", "DP", "B", "A", "CC" };

static const char *pshuregi[] = { "PC", "S", "Y", "X", "DP", "B", "A", "CC" };

static const char tfmreg[] = { 'D', 'X', 'Y', 'U', 'S', '?', '?', '?',
                               '?', '?', '?', '?', '?', '?', '?', '?' };

static const char tfmr0inc[] = { '+', '-', '+', ' ' };
static const char tfmr1inc[] = { '+', '-', ' ', '+' };

// For the CPU state display

static const char *cpu_state;
static const char cpu_6809_state[] = "A=?? B=?? X=???? Y=???? U=???? S=???? DP=?? E=? F=? H=? I=? N=? Z=? V=? C=?";
static const char cpu_6309_state[] = "A=?? B=?? E=?? F=?? X=???? Y=???? U=???? S=???? DP=?? T=???? E=? F=? H=? I=? N=? Z=? V=? C=? DZ=? IL=? FM=? NM=?";

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

// Is the CPU the 6309?
static int cpu6309 = 0;

// 6809 registers: -1 means unknown
static int ACCA = -1;
static int ACCB = -1;
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
// Forward declarations
// ====================================================================

static instr_mode_t instr_table_6809_map0[];
static instr_mode_t instr_table_6809_map1[];
static instr_mode_t instr_table_6809_map2[];

static instr_mode_t instr_table_6309_map0[];
static instr_mode_t instr_table_6309_map1[];
static instr_mode_t instr_table_6309_map2[];

static instr_mode_t *instr_table_map0;
static instr_mode_t *instr_table_map1;
static instr_mode_t *instr_table_map2;

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

// Undocumented

static operation_t op_XX   ;
static operation_t op_X18  ;
static operation_t op_X8C7 ;
static operation_t op_XHCF ;
static operation_t op_XNC  ;
static operation_t op_XSTX ;
static operation_t op_XSTU ;
static operation_t op_XRES ;

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

static instr_mode_t *get_instruction(uint8_t b0, uint8_t b1) {
   if (b0 == 0x11) {
      return instr_table_map2 + b1;
   } else if (b0 == 0x10) {
      return instr_table_map1 + b1;
   } else {
      return instr_table_map0 + b0;
   }
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
   cpu6309 = args->cpu_type == CPU_6309 || args->cpu_type == CPU_6309;

   if (cpu6309) {
      cpu_state = cpu_6309_state;
      instr_table_map0 = instr_table_6309_map0;
      instr_table_map1 = instr_table_6309_map1;
      instr_table_map2 = instr_table_6309_map2;
      regi4 = regi4_6309;
   } else {
      cpu_state = cpu_6809_state;
      instr_table_map0 = instr_table_6809_map0;
      instr_table_map1 = instr_table_6809_map1;
      instr_table_map2 = instr_table_6809_map2;
      regi4 = regi4_6809;
   }

   // Validate the cycles in the maps are consistent
   int fail = 0;
   for (int i = 0; i <= 2; i++) {
      instr_mode_t *instr_6309;
      instr_mode_t *instr_6809;
      switch (i) {
      case 1:
         instr_6309 = instr_table_6309_map1;
         instr_6809 = instr_table_6809_map1;
         break;
      case 2:
         instr_6309 = instr_table_6309_map2;
         instr_6809 = instr_table_6809_map2;
         break;
      default:
         instr_6309 = instr_table_6309_map0;
         instr_6809 = instr_table_6809_map0;
         break;
      }
      for (int j = 0; j <= 0xff; j++) {
         if (!instr_6809->undocumented) {
            if (instr_6309->cycles != instr_6809->cycles) {
               printf("cycle mismatch in instruction table: %02x %02x (%d cf %d)\n", i, j, instr_6309->cycles, instr_6809->cycles);
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

// TODO: Update for 6309
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
   ACCA = -1;
   ACCB = -1;
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
   if (cpu6309) {
      ACCE = -1;
      ACCF = -1;
      TV = -1;
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

      // TODO: Handle the case where cpu6309 and NM<0

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

      if (cpu6309 && (NM == 1)) {

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

// TODO: Update for 6309
static void em_6809_interrupt(sample_t *sample_q, int num_cycles, instruction_t *instruction) {
   if (num_cycles == 10) {
      instruction->pc = interrupt_helper(sample_q, 3, 0, 0xfff6);
   } else if (num_cycles == 19 && sample_q[16].addr == 0x8) {
      // IRQ
      instruction->pc = interrupt_helper(sample_q, 3, 1, 0xfff8);
   } else if (num_cycles == 19 && sample_q[16].addr == 0xC) {
      // NMI
      instruction->pc = interrupt_helper(sample_q, 3, 1, 0xfffC);
   } else {
      printf("*** could not determine interrupt type ***\n");
   }
}

static void em_6809_emulate(sample_t *sample_q, int num_cycles, instruction_t *instruction) {

   instr_mode_t *instr;
   int index = 0;
   int oi = 0;

   instruction->prefix = 0;
   instruction->opcode = sample_q[index].data;
   if (PC >= 0) {
      memory_read(instruction->opcode, PC + index, MEM_INSTR);
   }
   index++;
   instr = instr_table_map0;

   // Flag that an instruction marked as undocumented has been encoutered
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
         memory_read(instruction->opcode, PC + index, MEM_INSTR);
      }
      index++;
      instr = instruction->prefix == 0x11 ? instr_table_map2 : instr_table_map1;
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
   case DIRECTBIT:
      instruction->postbyte = sample_q[index].data;
      if (PC >= 0) {
         memory_read(instruction->postbyte, PC + index, MEM_INSTR);
      }
      index++;
      __attribute__((__fallthrough__));
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
   if (instr->op == &op_TST) {
      // There are two dead cycles at the end of TST
      operand = sample_q[num_cycles - 3].data;
   } else if (instr->mode == REGISTER) {
      operand = sample_q[1].data; // This is the postbyte
   } else if (instr->op->type == RMWOP) {
      // Read-modify-wrie instruction (always 8-bit)
      operand = sample_q[num_cycles - 3].data;
   } else if (instr->op->size == SIZE_32) {
      operand = (sample_q[num_cycles - 4].data << 24) + (sample_q[num_cycles - 3].data << 16) + (sample_q[num_cycles - 2].data << 8) + sample_q[num_cycles - 1].data;
   } else if (instr->op->size == SIZE_16) {
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
               if (*reg >= 0 && ACCB >= 0) {
                  ea = (*reg + ACCB) & 0xffff;
               }
               break;
            case 6:                 /* A,R */
               if (*reg >= 0 && ACCA >= 0) {
                  ea = (*reg + ACCA) & 0xffff;
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

static char *strinsert(char *ptr, const char *str) {
   while (*str) {
      *ptr++ = *str++;
   }
   return ptr;
}

static int em_6809_disassemble(char *buffer, instruction_t *instruction) {
   int b0 = instruction->instr[0];
   int b1 = instruction->instr[1];
   int pb = 0;
   instr_mode_t *instr = get_instruction(b0, b1);

   // Work out where in the instruction the operand is
   // [Prefix] Opcode [ Postbyte] Op1 Op2
   int oi;
   int opcode;
   // Extract the prefix/opcode
   if (b0 == 0x10 || b0 == 0x11) {
      opcode = (b0 << 8) | b1;
      oi = 2;
   } else {
      opcode = b0;
      oi = 1;
   }
   if (instr->mode == INDEXED || instr->mode == DIRECTBIT || instr->mode == REGISTER) {
      // Skip over the post byte
      pb = instruction->instr[oi];
      oi++;
   }
   int op8 = instruction->instr[oi];
   int op16 = (instruction->instr[oi] << 8) + instruction->instr[oi + 1];

   /// Output the mnemonic
   char *ptr = buffer;
   int len = strlen(instr->op->mnemonic);
   strcpy(ptr, instr->op->mnemonic);
   ptr += len;
   for (int i = len; i < 6; i++) {
      *ptr++ = ' ';
   }

   // Output the operand
   switch (instr->mode) {
   case INHERENT:
      break;
   case REGISTER:
      {
         switch (opcode) {
         case 0x001a: // ORC
         case 0x001c: // ANDC
            *ptr++ = '#';
            *ptr++ = '$';
            write_hex2(ptr, pb);
            ptr += 2;
            break;
         case 0x001e: // EXG
         case 0x001f: // TFR
         case 0x1030: // ADDR
         case 0x1031: // ADCR
         case 0x1032: // SUBR
         case 0x1033: // SBCR
         case 0x1034: // ANDR
         case 0x1035: // OR
         case 0x1036: // EORR
         case 0x1037: // CMPR
            ptr = strinsert(ptr, regi4[(pb >> 4) & 0x0f]);
            *ptr++ = ',';
            ptr = strinsert(ptr, regi4[pb & 0x0f]);
            break;
         case 0x0034: // PSHS
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
         case 0x0035: // PULS
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
         case 0x0036: // PSHU
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
         case 0x0037: // PULU
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
         case 0x1138: // TFM r0+, r1+
         case 0x1139: // TFM r0-, r1-
         case 0x113a: // TFM r0+, r1
         case 0x113b: // TFM r0 , r1+
            *ptr++ = tfmreg[(pb >> 4) & 0xf];
            *ptr++ = tfmr0inc[opcode & 3];
            *ptr++ = tfmreg[pb & 0xf];
            *ptr++ = tfmr1inc[opcode & 3];
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
   case IMMEDIATE_32:
      *ptr++ = '#';
      *ptr++ = '$';
      for (int i = 0; i < 4; i++) {
         write_hex2(ptr, instruction->instr[oi + i]);
         ptr += 2;
      }
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
   case DIRECTBIT:
      // r,sBit,dBit,addr
      // Reg num is in bits 7..6
      switch ((pb >> 6) & 3) {
      case 0:
         *ptr++ = 'C';
         *ptr++ = 'C';
         break;
      case 1:
         *ptr++ = 'A';
         break;
      case 2:
         *ptr++ = 'B';
         break;
      default:
         *ptr++ = '?';
         break;
      }
      *ptr++ = ',';
      // Src Bit is in bits 5..3
      *ptr++ = '0' + ((pb >> 3) & 7);
      *ptr++ = ',';
      // Dest Bit is in bits 2..0
      *ptr++ = '0' + (pb & 7);
      *ptr++ = ',';
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
         char reg = regi2[(pb >> 5) & 0x03];
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
   if (S >= 0) {
      write_hex4(bp, S);
   }
   bp += 7;
   if (U >= 0) {
      write_hex4(bp, U);
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
   .disassemble = em_6809_disassemble,
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

static int asl16_helper(int val) {
   if (val >= 0) {
      C = (val >> 15) & 1;
      val = (val << 1) & 0xffff;
      V = (val >> 15) & C & 1;
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

static int rol_helper(int val) {
   if (val >= 0 && C >= 0) {
      int tmp = (val << 1) + C;
      // C is bit 7 of val
      C = (val >> 7) & 1;
      // V is the xor of bits 7,6 of val
      V = ((tmp ^ val) >> 7) & 1;
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
      V = ((tmp ^ val) >> 16) & 1;
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

// r0 is the src
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
      case  0: ret = pack(ACCB, ACCB);            break;
      case  1: ret = X;                           break;
      case  2: ret = Y;                           break;
      case  3: ret = U;                           break;
      case  4: ret = S;                           break;
      case  5: ret = PC;                          break;
      case  6: ret = pack(ACCE, ACCF);            break;
      case  7: ret = TV;                          break;
         // src is 8 bits, promote to 16 bits
      case  8: ret = pack(ACCB, ACCB);            break;
      case  9: ret = pack(ACCB, ACCB);            break;
      case 10: ret = get_FLAGS();                 break;
      case 11: ret = (DP < 0) ? -1 : (DP << 8);   break;
      case 14: ret = pack(ACCE, ACCF);            break;
      case 15: ret = pack(ACCE, ACCF);            break;
      default: ret = 0;
      }
   }
   return ret;
}


// r1 is the dst
static int get_r1(int pb) {
   int ret;
   int dst = pb & 0xf;
   switch(dst) {
   case  0: ret = pack(ACCB, ACCB);            break;
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



// r1 is the dst
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
   // TODO: Confirm cycles
   int e = sample_q[5].data;
   int f = sample_q[3].data;
   push8(f);
   if (ACCF >= 0 && ACCE != f) {
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
   // TODO: Confirm cycles
   int e = sample_q[3].data;
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
      result = add16_helper(r0, C, r1);
   } else {
      result = add_helper(r0, C, r1);
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
      result = add16_helper(r0, 0, r1);
   } else {
      result = add_helper(r0, 0, r1);
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
      result = and16_helper(r0, r1);
   } else {
      result = and_helper(r0, r1);
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
      cmp16_helper(r0, r1);
   } else {
      cmp_helper(r0, r1);
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
      // TODO: how to determine the length?
      interrupt_helper(sample_q, 3, 1, 0xfff0);
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
      // TODO: how to determine the length?
      interrupt_helper(sample_q, 3, 1, 0xfff0);
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
      result = eor16_helper(r0, r1);
   } else {
      result = eor_helper(r0, r1);
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
      result = or16_helper(r0, r1);
   } else {
      result = or_helper(r0, r1);
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
      result = sub16_helper(r0, C, r1);
   } else {
      result = sub_helper(r0, C, r1);
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
   // TODO: Confirm V is cleared, documentation is inconsistent
   V = 0;
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
      result = sub16_helper(r0, 0, r1);
   } else {
      result = sub_helper(r0, 0, r1);
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
static operation_t op_LDW   = { "LDW",   op_fn_LDW,     LOADOP , 0 };
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

static instr_mode_t instr_table_6809_map0[] = {
   /* 00 */    { &op_NEG , DIRECT       , 0, 6 },
   /* 01 */    { &op_NEG , DIRECT       , 1, 6 },
   /* 02 */    { &op_XNC , DIRECT       , 1, 6 },
   /* 03 */    { &op_COM , DIRECT       , 0, 6 },
   /* 04 */    { &op_LSR , DIRECT       , 0, 6 },
   /* 05 */    { &op_LSR , DIRECT       , 1, 6 },
   /* 06 */    { &op_ROR , DIRECT       , 0, 6 },
   /* 07 */    { &op_ASR , DIRECT       , 0, 6 },
   /* 08 */    { &op_ASL , DIRECT       , 0, 6 },
   /* 09 */    { &op_ROL , DIRECT       , 0, 6 },
   /* 0A */    { &op_DEC , DIRECT       , 0, 6 },
   /* 0B */    { &op_DEC , DIRECT       , 1, 6 },
   /* 0C */    { &op_INC , DIRECT       , 0, 6 },
   /* 0D */    { &op_TST , DIRECT       , 0, 6 },
   /* 0E */    { &op_JMP , DIRECT       , 0, 3 },
   /* 0F */    { &op_CLR , DIRECT       , 0, 6 },
   /* 10 */    { &op_UU  , INHERENT     , 0, 1 },
   /* 11 */    { &op_UU  , INHERENT     , 0, 1 },
   /* 12 */    { &op_NOP , INHERENT     , 0, 2 },
   /* 13 */    { &op_SYNC, INHERENT     , 0, 4 },
   /* 14 */    { &op_XHCF, INHERENT     , 1, 1 },
   /* 15 */    { &op_XHCF, INHERENT     , 1, 1 },
   /* 16 */    { &op_LBRA, RELATIVE_16  , 0, 5 },
   /* 17 */    { &op_LBSR, RELATIVE_16  , 0, 9 },
   /* 18 */    { &op_X18 , INHERENT     , 1, 3 },
   /* 19 */    { &op_DAA , INHERENT     , 0, 2 },
   /* 1A */    { &op_ORCC, REGISTER     , 0, 3 },
   /* 1B */    { &op_NOP , INHERENT     , 1, 2 },
   /* 1C */    { &op_ANDC, REGISTER     , 0, 3 },
   /* 1D */    { &op_SEX , INHERENT     , 0, 2 },
   /* 1E */    { &op_EXG , REGISTER     , 0, 8 },
   /* 1F */    { &op_TFR , REGISTER     , 0, 6 },
   /* 20 */    { &op_BRA , RELATIVE_8   , 0, 3 },
   /* 21 */    { &op_BRN , RELATIVE_8   , 0, 3 },
   /* 22 */    { &op_BHI , RELATIVE_8   , 0, 3 },
   /* 23 */    { &op_BLS , RELATIVE_8   , 0, 3 },
   /* 24 */    { &op_BCC , RELATIVE_8   , 0, 3 },
   /* 25 */    { &op_BLO , RELATIVE_8   , 0, 3 },
   /* 26 */    { &op_BNE , RELATIVE_8   , 0, 3 },
   /* 27 */    { &op_BEQ , RELATIVE_8   , 0, 3 },
   /* 28 */    { &op_BVC , RELATIVE_8   , 0, 3 },
   /* 29 */    { &op_BVS , RELATIVE_8   , 0, 3 },
   /* 2A */    { &op_BPL , RELATIVE_8   , 0, 3 },
   /* 2B */    { &op_BMI , RELATIVE_8   , 0, 3 },
   /* 2C */    { &op_BGE , RELATIVE_8   , 0, 3 },
   /* 2D */    { &op_BLT , RELATIVE_8   , 0, 3 },
   /* 2E */    { &op_BGT , RELATIVE_8   , 0, 3 },
   /* 2F */    { &op_BLE , RELATIVE_8   , 0, 3 },
   /* 30 */    { &op_LEAX, INDEXED      , 0, 4 },
   /* 31 */    { &op_LEAY, INDEXED      , 0, 4 },
   /* 32 */    { &op_LEAS, INDEXED      , 0, 4 },
   /* 33 */    { &op_LEAU, INDEXED      , 0, 4 },
   /* 34 */    { &op_PSHS, REGISTER     , 0, 5 },
   /* 35 */    { &op_PULS, REGISTER     , 0, 5 },
   /* 36 */    { &op_PSHU, REGISTER     , 0, 5 },
   /* 37 */    { &op_PULU, REGISTER     , 0, 5 },
   /* 38 */    { &op_ANDC, REGISTER     , 1, 4 }, // 4 cycle version of ANCDD
   /* 39 */    { &op_RTS , INHERENT     , 0, 5 },
   /* 3A */    { &op_ABX , INHERENT     , 0, 3 },
   /* 3B */    { &op_RTI , INHERENT     , 0, 6 },
   /* 3C */    { &op_CWAI, IMMEDIATE_8  , 0,20 },
   /* 3D */    { &op_MUL , INHERENT     , 0,11 },
   /* 3E */    { &op_XRES, INHERENT     , 1,19 },
   /* 3F */    { &op_SWI , INHERENT     , 0,19 },
   /* 40 */    { &op_NEGA, INHERENT     , 0, 2 },
   /* 41 */    { &op_NEGA, INHERENT     , 1, 2 },
   /* 42 */    { &op_COMA, INHERENT     , 1, 2 },
   /* 43 */    { &op_COMA, INHERENT     , 0, 2 },
   /* 44 */    { &op_LSRA, INHERENT     , 0, 2 },
   /* 45 */    { &op_LSRA, INHERENT     , 1, 2 },
   /* 46 */    { &op_RORA, INHERENT     , 0, 2 },
   /* 47 */    { &op_ASRA, INHERENT     , 0, 2 },
   /* 48 */    { &op_ASLA, INHERENT     , 0, 2 },
   /* 49 */    { &op_ROLA, INHERENT     , 0, 2 },
   /* 4A */    { &op_DECA, INHERENT     , 0, 2 },
   /* 4B */    { &op_DECA, INHERENT     , 1, 2 },
   /* 4C */    { &op_INCA, INHERENT     , 0, 2 },
   /* 4D */    { &op_TSTA, INHERENT     , 0, 2 },
   /* 4E */    { &op_CLRA, INHERENT     , 1, 2 },
   /* 4F */    { &op_CLRA, INHERENT     , 0, 2 },
   /* 50 */    { &op_NEGB, INHERENT     , 0, 2 },
   /* 51 */    { &op_NEGB, INHERENT     , 1, 2 },
   /* 52 */    { &op_COMB, INHERENT     , 1, 2 },
   /* 53 */    { &op_COMB, INHERENT     , 0, 2 },
   /* 54 */    { &op_LSRB, INHERENT     , 0, 2 },
   /* 55 */    { &op_LSRB, INHERENT     , 1, 2 },
   /* 56 */    { &op_RORB, INHERENT     , 0, 2 },
   /* 57 */    { &op_ASRB, INHERENT     , 0, 2 },
   /* 58 */    { &op_ASLB, INHERENT     , 0, 2 },
   /* 59 */    { &op_ROLB, INHERENT     , 0, 2 },
   /* 5A */    { &op_DECB, INHERENT     , 0, 2 },
   /* 5B */    { &op_DECB, INHERENT     , 1, 2 },
   /* 5C */    { &op_INCB, INHERENT     , 0, 2 },
   /* 5D */    { &op_TSTB, INHERENT     , 0, 2 },
   /* 5E */    { &op_CLRB, INHERENT     , 1, 2 },
   /* 5F */    { &op_CLRB, INHERENT     , 0, 2 },
   /* 60 */    { &op_NEG , INDEXED      , 0, 6 },
   /* 61 */    { &op_NEG , INDEXED      , 1, 6 },
   /* 62 */    { &op_COM , INDEXED      , 1, 6 },
   /* 63 */    { &op_COM , INDEXED      , 0, 6 },
   /* 64 */    { &op_LSR , INDEXED      , 0, 6 },
   /* 65 */    { &op_LSR , INDEXED      , 1, 6 },
   /* 66 */    { &op_ROR , INDEXED      , 0, 6 },
   /* 67 */    { &op_ASR , INDEXED      , 0, 6 },
   /* 68 */    { &op_ASL , INDEXED      , 0, 6 },
   /* 69 */    { &op_ROL , INDEXED      , 0, 6 },
   /* 6A */    { &op_DEC , INDEXED      , 0, 6 },
   /* 6B */    { &op_DEC , INDEXED      , 1, 6 },
   /* 6C */    { &op_INC , INDEXED      , 0, 6 },
   /* 6D */    { &op_TST , INDEXED      , 0, 6 },
   /* 6E */    { &op_JMP , INDEXED      , 0, 3 },
   /* 6F */    { &op_CLR , INDEXED      , 0, 6 },
   /* 70 */    { &op_NEG , EXTENDED     , 0, 7 },
   /* 71 */    { &op_NEG , EXTENDED     , 1, 7 },
   /* 72 */    { &op_COM , EXTENDED     , 1, 7 },
   /* 73 */    { &op_COM , EXTENDED     , 0, 7 },
   /* 74 */    { &op_LSR , EXTENDED     , 0, 7 },
   /* 75 */    { &op_LSR , EXTENDED     , 1, 7 },
   /* 76 */    { &op_ROR , EXTENDED     , 0, 7 },
   /* 77 */    { &op_ASR , EXTENDED     , 0, 7 },
   /* 78 */    { &op_ASL , EXTENDED     , 0, 7 },
   /* 79 */    { &op_ROL , EXTENDED     , 0, 7 },
   /* 7A */    { &op_DEC , EXTENDED     , 0, 7 },
   /* 7B */    { &op_DEC , EXTENDED     , 1, 7 },
   /* 7C */    { &op_INC , EXTENDED     , 0, 7 },
   /* 7D */    { &op_TST , EXTENDED     , 0, 7 },
   /* 7E */    { &op_JMP , EXTENDED     , 0, 4 },
   /* 7F */    { &op_CLR , EXTENDED     , 0, 7 },
   /* 80 */    { &op_SUBA, IMMEDIATE_8  , 0, 2 },
   /* 81 */    { &op_CMPA, IMMEDIATE_8  , 0, 2 },
   /* 82 */    { &op_SBCA, IMMEDIATE_8  , 0, 2 },
   /* 83 */    { &op_SUBD, IMMEDIATE_16 , 0, 4 },
   /* 84 */    { &op_ANDA, IMMEDIATE_8  , 0, 2 },
   /* 85 */    { &op_BITA, IMMEDIATE_8  , 0, 2 },
   /* 86 */    { &op_LDA , IMMEDIATE_8  , 0, 2 },
   /* 87 */    { &op_X8C7, IMMEDIATE_8  , 1, 2 },
   /* 88 */    { &op_EORA, IMMEDIATE_8  , 0, 2 },
   /* 89 */    { &op_ADCA, IMMEDIATE_8  , 0, 2 },
   /* 8A */    { &op_ORA , IMMEDIATE_8  , 0, 2 },
   /* 8B */    { &op_ADDA, IMMEDIATE_8  , 0, 2 },
   /* 8C */    { &op_CMPX, IMMEDIATE_16 , 0, 4 },
   /* 8D */    { &op_BSR , RELATIVE_8   , 0, 7 },
   /* 8E */    { &op_LDX , IMMEDIATE_16 , 0, 3 },
   /* 8F */    { &op_XSTX, IMMEDIATE_8  , 1, 3 },
   /* 90 */    { &op_SUBA, DIRECT       , 0, 4 },
   /* 91 */    { &op_CMPA, DIRECT       , 0, 4 },
   /* 92 */    { &op_SBCA, DIRECT       , 0, 4 },
   /* 93 */    { &op_SUBD, DIRECT       , 0, 6 },
   /* 94 */    { &op_ANDA, DIRECT       , 0, 4 },
   /* 95 */    { &op_BITA, DIRECT       , 0, 4 },
   /* 96 */    { &op_LDA , DIRECT       , 0, 4 },
   /* 97 */    { &op_STA , DIRECT       , 0, 4 },
   /* 98 */    { &op_EORA, DIRECT       , 0, 4 },
   /* 99 */    { &op_ADCA, DIRECT       , 0, 4 },
   /* 9A */    { &op_ORA , DIRECT       , 0, 4 },
   /* 9B */    { &op_ADDA, DIRECT       , 0, 4 },
   /* 9C */    { &op_CMPX, DIRECT       , 0, 6 },
   /* 9D */    { &op_JSR , DIRECT       , 0, 7 },
   /* 9E */    { &op_LDX , DIRECT       , 0, 5 },
   /* 9F */    { &op_STX , DIRECT       , 0, 5 },
   /* A0 */    { &op_SUBA, INDEXED      , 0, 4 },
   /* A1 */    { &op_CMPA, INDEXED      , 0, 4 },
   /* A2 */    { &op_SBCA, INDEXED      , 0, 4 },
   /* A3 */    { &op_SUBD, INDEXED      , 0, 6 },
   /* A4 */    { &op_ANDA, INDEXED      , 0, 4 },
   /* A5 */    { &op_BITA, INDEXED      , 0, 4 },
   /* A6 */    { &op_LDA , INDEXED      , 0, 4 },
   /* A7 */    { &op_STA , INDEXED      , 0, 4 },
   /* A8 */    { &op_EORA, INDEXED      , 0, 4 },
   /* A9 */    { &op_ADCA, INDEXED      , 0, 4 },
   /* AA */    { &op_ORA , INDEXED      , 0, 4 },
   /* AB */    { &op_ADDA, INDEXED      , 0, 4 },
   /* AC */    { &op_CMPX, INDEXED      , 0, 6 },
   /* AD */    { &op_JSR , INDEXED      , 0, 7 },
   /* AE */    { &op_LDX , INDEXED      , 0, 5 },
   /* AF */    { &op_STX , INDEXED      , 0, 5 },
   /* B0 */    { &op_SUBA, EXTENDED     , 0, 5 },
   /* B1 */    { &op_CMPA, EXTENDED     , 0, 5 },
   /* B2 */    { &op_SBCA, EXTENDED     , 0, 5 },
   /* B3 */    { &op_SUBD, EXTENDED     , 0, 7 },
   /* B4 */    { &op_ANDA, EXTENDED     , 0, 5 },
   /* B5 */    { &op_BITA, EXTENDED     , 0, 5 },
   /* B6 */    { &op_LDA , EXTENDED     , 0, 5 },
   /* B7 */    { &op_STA , EXTENDED     , 0, 5 },
   /* B8 */    { &op_EORA, EXTENDED     , 0, 5 },
   /* B9 */    { &op_ADCA, EXTENDED     , 0, 5 },
   /* BA */    { &op_ORA , EXTENDED     , 0, 5 },
   /* BB */    { &op_ADDA, EXTENDED     , 0, 5 },
   /* BC */    { &op_CMPX, EXTENDED     , 0, 7 },
   /* BD */    { &op_JSR , EXTENDED     , 0, 8 },
   /* BE */    { &op_LDX , EXTENDED     , 0, 6 },
   /* BF */    { &op_STX , EXTENDED     , 0, 6 },
   /* C0 */    { &op_SUBB, IMMEDIATE_8  , 0, 2 },
   /* C1 */    { &op_CMPB, IMMEDIATE_8  , 0, 2 },
   /* C2 */    { &op_SBCB, IMMEDIATE_8  , 0, 2 },
   /* C3 */    { &op_ADDD, IMMEDIATE_16 , 0, 4 },
   /* C4 */    { &op_ANDB, IMMEDIATE_8  , 0, 2 },
   /* C5 */    { &op_BITB, IMMEDIATE_8  , 0, 2 },
   /* C6 */    { &op_LDB , IMMEDIATE_8  , 0, 2 },
   /* C7 */    { &op_X8C7, IMMEDIATE_8  , 1, 2 },
   /* C8 */    { &op_EORB, IMMEDIATE_8  , 0, 2 },
   /* C9 */    { &op_ADCB, IMMEDIATE_8  , 0, 2 },
   /* CA */    { &op_ORB , IMMEDIATE_8  , 0, 2 },
   /* CB */    { &op_ADDB, IMMEDIATE_8  , 0, 2 },
   /* CC */    { &op_LDD , IMMEDIATE_16 , 0, 3 },
   /* CD */    { &op_XHCF, INHERENT     , 1, 1 },
   /* CE */    { &op_LDU , IMMEDIATE_16 , 0, 3 },
   /* 8F */    { &op_XSTU, IMMEDIATE_8  , 1, 3 },
   /* D0 */    { &op_SUBB, DIRECT       , 0, 4 },
   /* D1 */    { &op_CMPB, DIRECT       , 0, 4 },
   /* D2 */    { &op_SBCB, DIRECT       , 0, 4 },
   /* D3 */    { &op_ADDD, DIRECT       , 0, 6 },
   /* D4 */    { &op_ANDB, DIRECT       , 0, 4 },
   /* D5 */    { &op_BITB, DIRECT       , 0, 4 },
   /* D6 */    { &op_LDB , DIRECT       , 0, 4 },
   /* D7 */    { &op_STB , DIRECT       , 0, 4 },
   /* D8 */    { &op_EORB, DIRECT       , 0, 4 },
   /* D9 */    { &op_ADCB, DIRECT       , 0, 4 },
   /* DA */    { &op_ORB , DIRECT       , 0, 4 },
   /* DB */    { &op_ADDB, DIRECT       , 0, 4 },
   /* DC */    { &op_LDD , DIRECT       , 0, 5 },
   /* DD */    { &op_STD , DIRECT       , 0, 5 },
   /* DE */    { &op_LDU , DIRECT       , 0, 5 },
   /* DF */    { &op_STU , DIRECT       , 0, 5 },
   /* E0 */    { &op_SUBB, INDEXED      , 0, 4 },
   /* E1 */    { &op_CMPB, INDEXED      , 0, 4 },
   /* E2 */    { &op_SBCB, INDEXED      , 0, 4 },
   /* E3 */    { &op_ADDD, INDEXED      , 0, 6 },
   /* E4 */    { &op_ANDB, INDEXED      , 0, 4 },
   /* E5 */    { &op_BITB, INDEXED      , 0, 4 },
   /* E6 */    { &op_LDB , INDEXED      , 0, 4 },
   /* E7 */    { &op_STB , INDEXED      , 0, 4 },
   /* E8 */    { &op_EORB, INDEXED      , 0, 4 },
   /* E9 */    { &op_ADCB, INDEXED      , 0, 4 },
   /* EA */    { &op_ORB , INDEXED      , 0, 4 },
   /* EB */    { &op_ADDB, INDEXED      , 0, 4 },
   /* EC */    { &op_LDD , INDEXED      , 0, 5 },
   /* ED */    { &op_STD , INDEXED      , 0, 5 },
   /* EE */    { &op_LDU , INDEXED      , 0, 5 },
   /* EF */    { &op_STU , INDEXED      , 0, 5 },
   /* F0 */    { &op_SUBB, EXTENDED     , 0, 5 },
   /* F1 */    { &op_CMPB, EXTENDED     , 0, 5 },
   /* F2 */    { &op_SBCB, EXTENDED     , 0, 5 },
   /* F3 */    { &op_ADDD, EXTENDED     , 0, 7 },
   /* F4 */    { &op_ANDB, EXTENDED     , 0, 5 },
   /* F5 */    { &op_BITB, EXTENDED     , 0, 5 },
   /* F6 */    { &op_LDB , EXTENDED     , 0, 5 },
   /* F7 */    { &op_STB , EXTENDED     , 0, 5 },
   /* F8 */    { &op_EORB, EXTENDED     , 0, 5 },
   /* F9 */    { &op_ADCB, EXTENDED     , 0, 5 },
   /* FA */    { &op_ORB , EXTENDED     , 0, 5 },
   /* FB */    { &op_ADDB, EXTENDED     , 0, 5 },
   /* FC */    { &op_LDD , EXTENDED     , 0, 6 },
   /* FD */    { &op_STD , EXTENDED     , 0, 6 },
   /* FE */    { &op_LDU , EXTENDED     , 0, 6 },
   /* FF */    { &op_STU , EXTENDED     , 0, 6 }
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
   /* 20 */    { &op_LBRA, RELATIVE_16  , 1, 5 },
   /* 21 */    { &op_LBRN, RELATIVE_16  , 0, 5 },
   /* 22 */    { &op_LBHI, RELATIVE_16  , 0, 5 },
   /* 23 */    { &op_LBLS, RELATIVE_16  , 0, 5 },
   /* 24 */    { &op_LBCC, RELATIVE_16  , 0, 5 },
   /* 25 */    { &op_LBLO, RELATIVE_16  , 0, 5 },
   /* 26 */    { &op_LBNE, RELATIVE_16  , 0, 5 },
   /* 27 */    { &op_LBEQ, RELATIVE_16  , 0, 5 },
   /* 28 */    { &op_LBVC, RELATIVE_16  , 0, 5 },
   /* 29 */    { &op_LBVS, RELATIVE_16  , 0, 5 },
   /* 2A */    { &op_LBPL, RELATIVE_16  , 0, 5 },
   /* 2B */    { &op_LBMI, RELATIVE_16  , 0, 5 },
   /* 2C */    { &op_LBGE, RELATIVE_16  , 0, 5 },
   /* 2D */    { &op_LBLT, RELATIVE_16  , 0, 5 },
   /* 2E */    { &op_LBGT, RELATIVE_16  , 0, 5 },
   /* 2F */    { &op_LBLE, RELATIVE_16  , 0, 5 },
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
   /* 3F */    { &op_SWI2, INHERENT     , 0,20 },
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
   /* 83 */    { &op_CMPD, IMMEDIATE_16 , 0, 5 },
   /* 84 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 85 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 86 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 87 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 88 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 89 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 8A */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 8B */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 8C */    { &op_CMPY, IMMEDIATE_16 , 0, 5 },
   /* 8D */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 8E */    { &op_LDY , IMMEDIATE_16 , 0, 4 },
   /* 8F */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 90 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 91 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 92 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 93 */    { &op_CMPD, DIRECT       , 0, 7 },
   /* 94 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 95 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 96 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 97 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 98 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 99 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 9A */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 9B */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 9C */    { &op_CMPY, DIRECT       , 0, 7 },
   /* 9D */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 9E */    { &op_LDY , DIRECT       , 0, 6 },
   /* 9F */    { &op_STY , DIRECT       , 0, 6 },
   /* A0 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* A1 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* A2 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* A3 */    { &op_CMPD, INDEXED      , 0, 7 },
   /* A4 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* A5 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* A6 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* A7 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* A8 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* A9 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* AA */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* AB */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* AC */    { &op_CMPY, INDEXED      , 0, 7 },
   /* AD */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* AE */    { &op_LDY , INDEXED      , 0, 6 },
   /* AF */    { &op_STY , INDEXED      , 0, 6 },
   /* B0 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* B1 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* B2 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* B3 */    { &op_CMPD, EXTENDED     , 0, 8 },
   /* B4 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* B5 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* B6 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* B7 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* B8 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* B9 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* BA */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* BB */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* BC */    { &op_CMPY, EXTENDED     , 0, 8 },
   /* BD */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* BE */    { &op_LDY , EXTENDED     , 0, 7 },
   /* BF */    { &op_STY , EXTENDED     , 0, 7 },
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
   /* CE */    { &op_LDS , IMMEDIATE_16 , 0, 4 },
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
   /* DE */    { &op_LDS , DIRECT       , 0, 6 },
   /* DF */    { &op_STS , DIRECT       , 0, 6 },
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
   /* EE */    { &op_LDS , INDEXED      , 0, 6 },
   /* EF */    { &op_STS , INDEXED      , 0, 6 },
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
   /* FE */    { &op_LDS , EXTENDED     , 0, 7 },
   /* FF */    { &op_STS , EXTENDED     , 0, 7 }
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
   /* 3F */    { &op_SWI3, INHERENT     , 0,20 },
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
   /* 83 */    { &op_CMPU, IMMEDIATE_16 , 0, 5 },
   /* 84 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 85 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 86 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 87 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 88 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 89 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 8A */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 8B */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 8C */    { &op_CMPS, IMMEDIATE_16 , 0, 5 },
   /* 8D */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 8E */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 8F */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 90 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 91 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 92 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 93 */    { &op_CMPU, DIRECT       , 0, 7 },
   /* 94 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 95 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 96 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 97 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 98 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 99 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 9A */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 9B */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 9C */    { &op_CMPS, DIRECT       , 0, 7 },
   /* 9D */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 9E */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* 9F */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* A0 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* A1 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* A2 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* A3 */    { &op_CMPU, INDEXED      , 0, 7 },
   /* A4 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* A5 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* A6 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* A7 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* A8 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* A9 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* AA */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* AB */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* AC */    { &op_CMPS, INDEXED      , 0, 7 },
   /* AD */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* AE */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* AF */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* B0 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* B1 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* B2 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* B3 */    { &op_CMPU, EXTENDED     , 0, 8 },
   /* B4 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* B5 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* B6 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* B7 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* B8 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* B9 */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* BA */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* BB */    { &op_XX  , ILLEGAL      , 1, 1 },
   /* BC */    { &op_CMPS, EXTENDED     , 0, 8 },
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

// ====================================================================
// 6309 Opcode Tables
// ====================================================================

static instr_mode_t instr_table_6309_map0[] = {
   /* 00 */    { &op_NEG , DIRECT       , 0, 6, 5 },
   /* 01 */    { &op_OIM , IMMEDIATE_8  , 0, 6, 6 },
   /* 02 */    { &op_AIM , IMMEDIATE_8  , 0, 6, 6 },
   /* 03 */    { &op_COM , DIRECT       , 0, 6, 5 },
   /* 04 */    { &op_LSR , DIRECT       , 0, 6, 5 },
   /* 05 */    { &op_EIM , IMMEDIATE_8  , 0, 6, 6 },
   /* 06 */    { &op_ROR , DIRECT       , 0, 6, 5 },
   /* 07 */    { &op_ASR , DIRECT       , 0, 6, 5 },
   /* 08 */    { &op_ASL , DIRECT       , 0, 6, 5 },
   /* 09 */    { &op_ROL , DIRECT       , 0, 6, 5 },
   /* 0A */    { &op_DEC , DIRECT       , 0, 6, 5 },
   /* 0B */    { &op_TIM , IMMEDIATE_8  , 0, 6, 6 },
   /* 0C */    { &op_INC , DIRECT       , 0, 6, 5 },
   /* 0D */    { &op_TST , DIRECT       , 0, 6, 4 },
   /* 0E */    { &op_JMP , DIRECT       , 0, 3, 2 },
   /* 0F */    { &op_CLR , DIRECT       , 0, 6, 5 },
   /* 10 */    { &op_UU  , INHERENT     , 0, 1, 1 },
   /* 11 */    { &op_UU  , INHERENT     , 0, 1, 1 },
   /* 12 */    { &op_NOP , INHERENT     , 0, 2, 1 },
   /* 13 */    { &op_SYNC, INHERENT     , 0, 4, 4 },
   /* 14 */    { &op_SEXW, INHERENT     , 0, 2, 1 },
   /* 15 */    { &op_TRAP, ILLEGAL      , 1,19,21 },
   /* 16 */    { &op_LBRA, RELATIVE_16  , 0, 5, 4 },
   /* 17 */    { &op_LBSR, RELATIVE_16  , 0, 9, 7 },
   /* 18 */    { &op_TRAP, ILLEGAL      , 1,19,21 },
   /* 19 */    { &op_DAA , INHERENT     , 0, 2, 1 },
   /* 1A */    { &op_ORCC, REGISTER     , 0, 3, 2 },
   /* 1B */    { &op_TRAP, ILLEGAL      , 1,19,21 },
   /* 1C */    { &op_ANDC, REGISTER     , 0, 3, 3 },
   /* 1D */    { &op_SEX , INHERENT     , 0, 2, 1 },
   /* 1E */    { &op_EXG , REGISTER     , 0, 8, 5 },
   /* 1F */    { &op_TFR , REGISTER     , 0, 6, 4 },
   /* 20 */    { &op_BRA , RELATIVE_8   , 0, 3, 3 },
   /* 21 */    { &op_BRN , RELATIVE_8   , 0, 3, 3 },
   /* 22 */    { &op_BHI , RELATIVE_8   , 0, 3, 3 },
   /* 23 */    { &op_BLS , RELATIVE_8   , 0, 3, 3 },
   /* 24 */    { &op_BCC , RELATIVE_8   , 0, 3, 3 },
   /* 25 */    { &op_BLO , RELATIVE_8   , 0, 3, 3 },
   /* 26 */    { &op_BNE , RELATIVE_8   , 0, 3, 3 },
   /* 27 */    { &op_BEQ , RELATIVE_8   , 0, 3, 3 },
   /* 28 */    { &op_BVC , RELATIVE_8   , 0, 3, 3 },
   /* 29 */    { &op_BVS , RELATIVE_8   , 0, 3, 3 },
   /* 2A */    { &op_BPL , RELATIVE_8   , 0, 3, 3 },
   /* 2B */    { &op_BMI , RELATIVE_8   , 0, 3, 3 },
   /* 2C */    { &op_BGE , RELATIVE_8   , 0, 3, 3 },
   /* 2D */    { &op_BLT , RELATIVE_8   , 0, 3, 3 },
   /* 2E */    { &op_BGT , RELATIVE_8   , 0, 3, 3 },
   /* 2F */    { &op_BLE , RELATIVE_8   , 0, 3, 3 },
   /* 30 */    { &op_LEAX, INDEXED      , 0, 4, 4 },
   /* 31 */    { &op_LEAY, INDEXED      , 0, 4, 4 },
   /* 32 */    { &op_LEAS, INDEXED      , 0, 4, 4 },
   /* 33 */    { &op_LEAU, INDEXED      , 0, 4, 4 },
   /* 34 */    { &op_PSHS, REGISTER     , 0, 5, 4 },
   /* 35 */    { &op_PULS, REGISTER     , 0, 5, 4 },
   /* 36 */    { &op_PSHU, REGISTER     , 0, 5, 4 },
   /* 37 */    { &op_PULU, REGISTER     , 0, 5, 4 },
   /* 38 */    { &op_XX,   ILLEGAL      , 1,19,21 },
   /* 39 */    { &op_RTS , INHERENT     , 0, 5, 4 },
   /* 3A */    { &op_ABX , INHERENT     , 0, 3, 1 },
   /* 3B */    { &op_RTI , INHERENT     , 0, 6, 6 },
   /* 3C */    { &op_CWAI, IMMEDIATE_8  , 0,20,22 },
   /* 3D */    { &op_MUL , INHERENT     , 0,11,10 },
   /* 3E */    { &op_TRAP, ILLEGAL      , 1,19,21 },
   /* 3F */    { &op_SWI , INHERENT     , 0,19,21 },
   /* 40 */    { &op_NEGA, INHERENT     , 0, 2, 1 },
   /* 41 */    { &op_TRAP, ILLEGAL      , 1,19,21 },
   /* 42 */    { &op_TRAP, ILLEGAL      , 1,19,21 },
   /* 43 */    { &op_COMA, INHERENT     , 0, 2, 1 },
   /* 44 */    { &op_LSRA, INHERENT     , 0, 2, 1 },
   /* 45 */    { &op_TRAP, ILLEGAL      , 1,19,21 },
   /* 46 */    { &op_RORA, INHERENT     , 0, 2, 1 },
   /* 47 */    { &op_ASRA, INHERENT     , 0, 2, 1 },
   /* 48 */    { &op_ASLA, INHERENT     , 0, 2, 1 },
   /* 49 */    { &op_ROLA, INHERENT     , 0, 2, 1 },
   /* 4A */    { &op_DECA, INHERENT     , 0, 2, 1 },
   /* 4B */    { &op_TRAP, ILLEGAL      , 1,19,21 },
   /* 4C */    { &op_INCA, INHERENT     , 0, 2, 1 },
   /* 4D */    { &op_TSTA, INHERENT     , 0, 2, 1 },
   /* 4E */    { &op_TRAP, ILLEGAL      , 1,19,21 },
   /* 4F */    { &op_CLRA, INHERENT     , 0, 2, 1 },
   /* 50 */    { &op_NEGB, INHERENT     , 0, 2, 1 },
   /* 51 */    { &op_TRAP, ILLEGAL      , 1,19,21 },
   /* 52 */    { &op_TRAP, ILLEGAL      , 1,19,21 },
   /* 53 */    { &op_COMB, INHERENT     , 0, 2, 1 },
   /* 54 */    { &op_LSRB, INHERENT     , 0, 2, 1 },
   /* 55 */    { &op_TRAP, ILLEGAL      , 1,19,21 },
   /* 56 */    { &op_RORB, INHERENT     , 0, 2, 1 },
   /* 57 */    { &op_ASRB, INHERENT     , 0, 2, 1 },
   /* 58 */    { &op_ASLB, INHERENT     , 0, 2, 1 },
   /* 59 */    { &op_ROLB, INHERENT     , 0, 2, 1 },
   /* 5A */    { &op_DECB, INHERENT     , 0, 2, 1 },
   /* 5B */    { &op_TRAP, ILLEGAL      , 1,19,21 },
   /* 5C */    { &op_INCB, INHERENT     , 0, 2, 1 },
   /* 5D */    { &op_TSTB, INHERENT     , 0, 2, 1 },
   /* 5E */    { &op_TRAP, ILLEGAL      , 1,19,21 },
   /* 5F */    { &op_CLRB, INHERENT     , 0, 2, 1 },
   /* 60 */    { &op_NEG , INDEXED      , 0, 6, 6 },
   /* 61 */    { &op_OIM , INDEXED      , 0, 6, 6 },
   /* 62 */    { &op_AIM , INDEXED      , 0, 6, 6 },
   /* 63 */    { &op_COM , INDEXED      , 0, 6, 6 },
   /* 64 */    { &op_LSR , INDEXED      , 0, 6, 6 },
   /* 65 */    { &op_EIM , INDEXED      , 0, 6, 6 },
   /* 66 */    { &op_ROR , INDEXED      , 0, 6, 6 },
   /* 67 */    { &op_ASR , INDEXED      , 0, 6, 6 },
   /* 68 */    { &op_ASL , INDEXED      , 0, 6, 6 },
   /* 69 */    { &op_ROL , INDEXED      , 0, 6, 6 },
   /* 6A */    { &op_DEC , INDEXED      , 0, 6, 6 },
   /* 6B */    { &op_TIM , INDEXED      , 0, 6, 6 },
   /* 6C */    { &op_INC , INDEXED      , 0, 6, 6 },
   /* 6D */    { &op_TST , INDEXED      , 0, 6, 5 },
   /* 6E */    { &op_JMP , INDEXED      , 0, 3, 3 },
   /* 6F */    { &op_CLR , INDEXED      , 0, 6, 6 },
   /* 70 */    { &op_NEG , EXTENDED     , 0, 7, 6 },
   /* 71 */    { &op_OIM , EXTENDED     , 0, 7, 7 },
   /* 72 */    { &op_AIM , EXTENDED     , 0, 7, 7 },
   /* 73 */    { &op_COM , EXTENDED     , 0, 7, 6 },
   /* 74 */    { &op_LSR , EXTENDED     , 0, 7, 6 },
   /* 75 */    { &op_EIM , EXTENDED     , 0, 7, 7 },
   /* 76 */    { &op_ROR , EXTENDED     , 0, 7, 6 },
   /* 77 */    { &op_ASR , EXTENDED     , 0, 7, 6 },
   /* 78 */    { &op_ASL , EXTENDED     , 0, 7, 6 },
   /* 79 */    { &op_ROL , EXTENDED     , 0, 7, 6 },
   /* 7A */    { &op_DEC , EXTENDED     , 0, 7, 6 },
   /* 7B */    { &op_TIM , EXTENDED     , 0, 7, 7 },
   /* 7C */    { &op_INC , EXTENDED     , 0, 7, 7 },
   /* 7D */    { &op_TST , EXTENDED     , 0, 7, 5 },
   /* 7E */    { &op_JMP , EXTENDED     , 0, 4, 3 },
   /* 7F */    { &op_CLR , EXTENDED     , 0, 7, 6 },
   /* 80 */    { &op_SUBA, IMMEDIATE_8  , 0, 2, 2 },
   /* 81 */    { &op_CMPA, IMMEDIATE_8  , 0, 2, 2 },
   /* 82 */    { &op_SBCA, IMMEDIATE_8  , 0, 2, 2 },
   /* 83 */    { &op_SUBD, IMMEDIATE_16 , 0, 4, 3 },
   /* 84 */    { &op_ANDA, IMMEDIATE_8  , 0, 2, 2 },
   /* 85 */    { &op_BITA, IMMEDIATE_8  , 0, 2, 2 },
   /* 86 */    { &op_LDA , IMMEDIATE_8  , 0, 2, 2 },
   /* 87 */    { &op_TRAP, ILLEGAL      , 1,19,21 },
   /* 88 */    { &op_EORA, IMMEDIATE_8  , 0, 2, 2 },
   /* 89 */    { &op_ADCA, IMMEDIATE_8  , 0, 2, 2 },
   /* 8A */    { &op_ORA , IMMEDIATE_8  , 0, 2, 2 },
   /* 8B */    { &op_ADDA, IMMEDIATE_8  , 0, 2, 2 },
   /* 8C */    { &op_CMPX, IMMEDIATE_16 , 0, 4, 3 },
   /* 8D */    { &op_BSR , RELATIVE_8   , 0, 7, 6 },
   /* 8E */    { &op_LDX , IMMEDIATE_16 , 0, 3, 3 },
   /* 8F */    { &op_TRAP, ILLEGAL      , 1,19,21 },
   /* 90 */    { &op_SUBA, DIRECT       , 0, 4, 3 },
   /* 91 */    { &op_CMPA, DIRECT       , 0, 4, 3 },
   /* 92 */    { &op_SBCA, DIRECT       , 0, 4, 3 },
   /* 93 */    { &op_SUBD, DIRECT       , 0, 6, 4 },
   /* 94 */    { &op_ANDA, DIRECT       , 0, 4, 3 },
   /* 95 */    { &op_BITA, DIRECT       , 0, 4, 3 },
   /* 96 */    { &op_LDA , DIRECT       , 0, 4, 3 },
   /* 97 */    { &op_STA , DIRECT       , 0, 4, 3 },
   /* 98 */    { &op_EORA, DIRECT       , 0, 4, 3 },
   /* 99 */    { &op_ADCA, DIRECT       , 0, 4, 3 },
   /* 9A */    { &op_ORA , DIRECT       , 0, 4, 3 },
   /* 9B */    { &op_ADDA, DIRECT       , 0, 4, 3 },
   /* 9C */    { &op_CMPX, DIRECT       , 0, 6, 4 },
   /* 9D */    { &op_JSR , DIRECT       , 0, 7, 6 },
   /* 9E */    { &op_LDX , DIRECT       , 0, 5, 4 },
   /* 9F */    { &op_STX , DIRECT       , 0, 5, 4 },
   /* A0 */    { &op_SUBA, INDEXED      , 0, 4, 4 },
   /* A1 */    { &op_CMPA, INDEXED      , 0, 4, 4 },
   /* A2 */    { &op_SBCA, INDEXED      , 0, 4, 4 },
   /* A3 */    { &op_SUBD, INDEXED      , 0, 6, 5 },
   /* A4 */    { &op_ANDA, INDEXED      , 0, 4, 4 },
   /* A5 */    { &op_BITA, INDEXED      , 0, 4, 4 },
   /* A6 */    { &op_LDA , INDEXED      , 0, 4, 4 },
   /* A7 */    { &op_STA , INDEXED      , 0, 4, 4 },
   /* A8 */    { &op_EORA, INDEXED      , 0, 4, 4 },
   /* A9 */    { &op_ADCA, INDEXED      , 0, 4, 4 },
   /* AA */    { &op_ORA , INDEXED      , 0, 4, 4 },
   /* AB */    { &op_ADDA, INDEXED      , 0, 4, 4 },
   /* AC */    { &op_CMPX, INDEXED      , 0, 6, 5 },
   /* AD */    { &op_JSR , INDEXED      , 0, 7, 6 },
   /* AE */    { &op_LDX , INDEXED      , 0, 5, 5 },
   /* AF */    { &op_STX , INDEXED      , 0, 5, 5 },
   /* B0 */    { &op_SUBA, EXTENDED     , 0, 5, 4 },
   /* B1 */    { &op_CMPA, EXTENDED     , 0, 5, 4 },
   /* B2 */    { &op_SBCA, EXTENDED     , 0, 5, 4 },
   /* B3 */    { &op_SUBD, EXTENDED     , 0, 7, 5 },
   /* B4 */    { &op_ANDA, EXTENDED     , 0, 5, 4 },
   /* B5 */    { &op_BITA, EXTENDED     , 0, 5, 4 },
   /* B6 */    { &op_LDA , EXTENDED     , 0, 5, 4 },
   /* B7 */    { &op_STA , EXTENDED     , 0, 5, 4 },
   /* B8 */    { &op_EORA, EXTENDED     , 0, 5, 4 },
   /* B9 */    { &op_ADCA, EXTENDED     , 0, 5, 4 },
   /* BA */    { &op_ORA , EXTENDED     , 0, 5, 4 },
   /* BB */    { &op_ADDA, EXTENDED     , 0, 5, 4 },
   /* BC */    { &op_CMPX, EXTENDED     , 0, 7, 5 },
   /* BD */    { &op_JSR , EXTENDED     , 0, 8, 7 },
   /* BE */    { &op_LDX , EXTENDED     , 0, 6, 5 },
   /* BF */    { &op_STX , EXTENDED     , 0, 6, 5 },
   /* C0 */    { &op_SUBB, IMMEDIATE_8  , 0, 2, 2 },
   /* C1 */    { &op_CMPB, IMMEDIATE_8  , 0, 2, 2 },
   /* C2 */    { &op_SBCB, IMMEDIATE_8  , 0, 2, 2 },
   /* C3 */    { &op_ADDD, IMMEDIATE_16 , 0, 4, 3 },
   /* C4 */    { &op_ANDB, IMMEDIATE_8  , 0, 2, 2 },
   /* C5 */    { &op_BITB, IMMEDIATE_8  , 0, 2, 2 },
   /* C6 */    { &op_LDB , IMMEDIATE_8  , 0, 2, 2 },
   /* C7 */    { &op_TRAP, ILLEGAL      , 1,19,21 },
   /* C8 */    { &op_EORB, IMMEDIATE_8  , 0, 2, 2 },
   /* C9 */    { &op_ADCB, IMMEDIATE_8  , 0, 2, 2 },
   /* CA */    { &op_ORB , IMMEDIATE_8  , 0, 2, 2 },
   /* CB */    { &op_ADDB, IMMEDIATE_8  , 0, 2, 2 },
   /* CC */    { &op_LDD , IMMEDIATE_16 , 0, 3, 3 },
   /* CD */    { &op_LDQ , IMMEDIATE_32 , 0, 5, 5 },
   /* CE */    { &op_LDU , IMMEDIATE_16 , 0, 3, 3 },
   /* CF */    { &op_TRAP, ILLEGAL      , 1,19,21 },
   /* D0 */    { &op_SUBB, DIRECT       , 0, 4, 3 },
   /* D1 */    { &op_CMPB, DIRECT       , 0, 4, 3 },
   /* D2 */    { &op_SBCB, DIRECT       , 0, 4, 3 },
   /* D3 */    { &op_ADDD, DIRECT       , 0, 6, 4 },
   /* D4 */    { &op_ANDB, DIRECT       , 0, 4, 3 },
   /* D5 */    { &op_BITB, DIRECT       , 0, 4, 3 },
   /* D6 */    { &op_LDB , DIRECT       , 0, 4, 3 },
   /* D7 */    { &op_STB , DIRECT       , 0, 4, 3 },
   /* D8 */    { &op_EORB, DIRECT       , 0, 4, 3 },
   /* D9 */    { &op_ADCB, DIRECT       , 0, 4, 3 },
   /* DA */    { &op_ORB , DIRECT       , 0, 4, 3 },
   /* DB */    { &op_ADDB, DIRECT       , 0, 4, 3 },
   /* DC */    { &op_LDD , DIRECT       , 0, 5, 4 },
   /* DD */    { &op_STD , DIRECT       , 0, 5, 4 },
   /* DE */    { &op_LDU , DIRECT       , 0, 5, 4 },
   /* DF */    { &op_STU , DIRECT       , 0, 5, 4 },
   /* E0 */    { &op_SUBB, INDEXED      , 0, 4, 4 },
   /* E1 */    { &op_CMPB, INDEXED      , 0, 4, 4 },
   /* E2 */    { &op_SBCB, INDEXED      , 0, 4, 4 },
   /* E3 */    { &op_ADDD, INDEXED      , 0, 6, 5 },
   /* E4 */    { &op_ANDB, INDEXED      , 0, 4, 4 },
   /* E5 */    { &op_BITB, INDEXED      , 0, 4, 4 },
   /* E6 */    { &op_LDB , INDEXED      , 0, 4, 4 },
   /* E7 */    { &op_STB , INDEXED      , 0, 4, 4 },
   /* E8 */    { &op_EORB, INDEXED      , 0, 4, 4 },
   /* E9 */    { &op_ADCB, INDEXED      , 0, 4, 4 },
   /* EA */    { &op_ORB , INDEXED      , 0, 4, 4 },
   /* EB */    { &op_ADDB, INDEXED      , 0, 4, 4 },
   /* EC */    { &op_LDD , INDEXED      , 0, 5, 5 },
   /* ED */    { &op_STD , INDEXED      , 0, 5, 5 },
   /* EE */    { &op_LDU , INDEXED      , 0, 5, 5 },
   /* EF */    { &op_STU , INDEXED      , 0, 5, 5 },
   /* F0 */    { &op_SUBB, EXTENDED     , 0, 5, 4 },
   /* F1 */    { &op_CMPB, EXTENDED     , 0, 5, 4 },
   /* F2 */    { &op_SBCB, EXTENDED     , 0, 5, 4 },
   /* F3 */    { &op_ADDD, EXTENDED     , 0, 7, 5 },
   /* F4 */    { &op_ANDB, EXTENDED     , 0, 5, 4 },
   /* F5 */    { &op_BITB, EXTENDED     , 0, 5, 4 },
   /* F6 */    { &op_LDB , EXTENDED     , 0, 5, 4 },
   /* F7 */    { &op_STB , EXTENDED     , 0, 5, 4 },
   /* F8 */    { &op_EORB, EXTENDED     , 0, 5, 4 },
   /* F9 */    { &op_ADCB, EXTENDED     , 0, 5, 4 },
   /* FA */    { &op_ORB , EXTENDED     , 0, 5, 4 },
   /* FB */    { &op_ADDB, EXTENDED     , 0, 5, 4 },
   /* FC */    { &op_LDD , EXTENDED     , 0, 6, 5 },
   /* FD */    { &op_STD , EXTENDED     , 0, 6, 5 },
   /* FE */    { &op_LDU , EXTENDED     , 0, 6, 5 },
   /* FF */    { &op_STU , EXTENDED     , 0, 6, 5 }
};

static instr_mode_t instr_table_6309_map1[] = {
   /* 00 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 01 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 02 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 03 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 04 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 05 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 06 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 07 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 08 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 09 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 0A */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 0B */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 0C */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 0D */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 0E */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 0F */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 10 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 11 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 12 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 13 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 14 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 15 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 16 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 17 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 18 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 19 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 1A */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 1B */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 1C */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 1D */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 1E */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 1F */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 20 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 21 */    { &op_LBRN , RELATIVE_16  , 0, 5, 5 },
   /* 22 */    { &op_LBHI , RELATIVE_16  , 0, 5, 5 },
   /* 23 */    { &op_LBLS , RELATIVE_16  , 0, 5, 5 },
   /* 24 */    { &op_LBCC , RELATIVE_16  , 0, 5, 5 },
   /* 25 */    { &op_LBLO , RELATIVE_16  , 0, 5, 5 },
   /* 26 */    { &op_LBNE , RELATIVE_16  , 0, 5, 5 },
   /* 27 */    { &op_LBEQ , RELATIVE_16  , 0, 5, 5 },
   /* 28 */    { &op_LBVC , RELATIVE_16  , 0, 5, 5 },
   /* 29 */    { &op_LBVS , RELATIVE_16  , 0, 5, 5 },
   /* 2A */    { &op_LBPL , RELATIVE_16  , 0, 5, 5 },
   /* 2B */    { &op_LBMI , RELATIVE_16  , 0, 5, 5 },
   /* 2C */    { &op_LBGE , RELATIVE_16  , 0, 5, 5 },
   /* 2D */    { &op_LBLT , RELATIVE_16  , 0, 5, 5 },
   /* 2E */    { &op_LBGT , RELATIVE_16  , 0, 5, 5 },
   /* 2F */    { &op_LBLE , RELATIVE_16  , 0, 5, 5 },
   /* 30 */    { &op_ADDR , REGISTER     , 0, 4, 4 },
   /* 31 */    { &op_ADCR , REGISTER     , 0, 4, 4 },
   /* 32 */    { &op_SUBR , REGISTER     , 0, 4, 4 },
   /* 33 */    { &op_SBCR , REGISTER     , 0, 4, 4 },
   /* 34 */    { &op_ANDR , REGISTER     , 0, 4, 4 },
   /* 35 */    { &op_ORR  , REGISTER     , 0, 4, 4 },
   /* 36 */    { &op_EORR , REGISTER     , 0, 4, 4 },
   /* 37 */    { &op_CMPR , REGISTER     , 0, 4, 4 },
   /* 38 */    { &op_PSHSW, INHERENT     , 0, 6, 6 },
   /* 39 */    { &op_PULSW, INHERENT     , 0, 6, 6 },
   /* 3A */    { &op_PSHUW, INHERENT     , 0, 6, 6 },
   /* 3B */    { &op_PULUW, INHERENT     , 0, 6, 6 },
   /* 3C */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 3D */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 3E */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 3F */    { &op_SWI2 , INHERENT     , 0,20,22 },
   /* 40 */    { &op_NEGD , INHERENT     , 0, 2, 1 },
   /* 41 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 42 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 43 */    { &op_COMD , INHERENT     , 0, 2, 1 },
   /* 44 */    { &op_LSRD , INHERENT     , 0, 2, 1 },
   /* 45 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 46 */    { &op_RORD , INHERENT     , 0, 2, 1 },
   /* 47 */    { &op_ASRD , INHERENT     , 0, 2, 1 },
   /* 48 */    { &op_ASLD , INHERENT     , 0, 2, 1 },
   /* 49 */    { &op_ROLD , INHERENT     , 0, 2, 1 },
   /* 4A */    { &op_DECD , INHERENT     , 0, 2, 1 },
   /* 4B */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 4C */    { &op_INCD , INHERENT     , 0, 2, 1 },
   /* 4D */    { &op_TSTD , INHERENT     , 0, 2, 1 },
   /* 4E */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 4F */    { &op_CLRD , INHERENT     , 0, 2, 1 },
   /* 50 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 51 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 52 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 53 */    { &op_COMW , INHERENT     , 0, 3, 2 },
   /* 54 */    { &op_LSRW , INHERENT     , 0, 3, 2 },
   /* 55 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 56 */    { &op_RORW , INHERENT     , 0, 3, 2 },
   /* 57 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 58 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 59 */    { &op_ROLW , INHERENT     , 0, 3, 2 },
   /* 5A */    { &op_DECW , INHERENT     , 0, 3, 2 },
   /* 5B */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 5C */    { &op_INCW , INHERENT     , 0, 3, 2 },
   /* 5D */    { &op_TSTW , INHERENT     , 0, 3, 2 },
   /* 5E */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 5F */    { &op_CLRW , INHERENT     , 0, 3, 2 },
   /* 60 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 61 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 62 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 63 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 64 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 65 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 66 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 67 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 68 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 69 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 6A */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 6B */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 6C */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 6D */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 6E */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 6F */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 70 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 71 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 72 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 73 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 74 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 75 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 76 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 77 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 78 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 79 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 7A */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 7B */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 7C */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 7D */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 7E */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 7F */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 80 */    { &op_SUBW , IMMEDIATE_16 , 0, 5, 4 },
   /* 81 */    { &op_CMPW , IMMEDIATE_16 , 0, 5, 4 },
   /* 82 */    { &op_SBCD , IMMEDIATE_16 , 0, 5, 4 },
   /* 83 */    { &op_CMPD , IMMEDIATE_16 , 0, 5, 4 },
   /* 84 */    { &op_ANDD , IMMEDIATE_16 , 0, 5, 4 },
   /* 85 */    { &op_BITD , IMMEDIATE_16 , 0, 5, 4 },
   /* 86 */    { &op_LDW  , IMMEDIATE_16 , 0, 4, 4 },
   /* 87 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 88 */    { &op_EORD , IMMEDIATE_16 , 0, 5, 4 },
   /* 89 */    { &op_ADCD , IMMEDIATE_16 , 0, 5, 4 },
   /* 8A */    { &op_ORD  , IMMEDIATE_16 , 0, 5, 4 },
   /* 8B */    { &op_ADDW , IMMEDIATE_16 , 0, 5, 4 },
   /* 8C */    { &op_CMPY , IMMEDIATE_16 , 0, 5, 4 },
   /* 8D */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 8E */    { &op_LDY  , IMMEDIATE_16 , 0, 4, 4 },
   /* 8F */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 90 */    { &op_SUBW , DIRECT       , 0, 7, 5 },
   /* 91 */    { &op_CMPW , DIRECT       , 0, 7, 5 },
   /* 92 */    { &op_SBCD , DIRECT       , 0, 7, 5 },
   /* 93 */    { &op_CMPD , DIRECT       , 0, 7, 5 },
   /* 94 */    { &op_ANDD , DIRECT       , 0, 7, 5 },
   /* 95 */    { &op_BITD , DIRECT       , 0, 7, 5 },
   /* 96 */    { &op_LDW  , DIRECT       , 0, 6, 5 },
   /* 97 */    { &op_STW  , DIRECT       , 0, 6, 5 },
   /* 98 */    { &op_EORD , DIRECT       , 0, 7, 5 },
   /* 99 */    { &op_ADCD , DIRECT       , 0, 7, 5 },
   /* 9A */    { &op_ORD  , DIRECT       , 0, 7, 5 },
   /* 9B */    { &op_ADDW , DIRECT       , 0, 7, 5 },
   /* 9C */    { &op_CMPY , DIRECT       , 0, 7, 5 },
   /* 9D */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* 9E */    { &op_LDY  , DIRECT       , 0, 6, 5 },
   /* 9F */    { &op_STY  , DIRECT       , 0, 6, 5 },
   /* A0 */    { &op_SUBW , INDEXED      , 0, 7, 6 },
   /* A1 */    { &op_CMPW , INDEXED      , 0, 7, 6 },
   /* A2 */    { &op_SBCD , INDEXED      , 0, 7, 6 },
   /* A3 */    { &op_CMPD , INDEXED      , 0, 7, 6 },
   /* A4 */    { &op_ANDD , INDEXED      , 0, 7, 6 },
   /* A5 */    { &op_BITD , INDEXED      , 0, 7, 6 },
   /* A6 */    { &op_LDW  , INDEXED      , 0, 6, 6 },
   /* A7 */    { &op_STW  , INDEXED      , 0, 6, 6 },
   /* A8 */    { &op_EORD , INDEXED      , 0, 7, 6 },
   /* A9 */    { &op_ADCD , INDEXED      , 0, 7, 6 },
   /* AA */    { &op_ORD  , INDEXED      , 0, 7, 6 },
   /* AB */    { &op_ADDW , INDEXED      , 0, 7, 6 },
   /* AC */    { &op_CMPY , INDEXED      , 0, 7, 6 },
   /* AD */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* AE */    { &op_LDY  , INDEXED      , 0, 6, 5 },
   /* AF */    { &op_STY  , INDEXED      , 0, 6, 5 },
   /* B0 */    { &op_SUBW , EXTENDED     , 0, 8, 6 },
   /* B1 */    { &op_CMPW , EXTENDED     , 0, 8, 6 },
   /* B2 */    { &op_SBCD , EXTENDED     , 0, 8, 6 },
   /* B3 */    { &op_CMPD , EXTENDED     , 0, 8, 6 },
   /* B4 */    { &op_ANDD , EXTENDED     , 0, 8, 6 },
   /* B5 */    { &op_BITD , EXTENDED     , 0, 8, 6 },
   /* B6 */    { &op_LDW  , EXTENDED     , 0, 7, 6 },
   /* B7 */    { &op_STW  , EXTENDED     , 0, 7, 6 },
   /* B8 */    { &op_EORD , EXTENDED     , 0, 8, 6 },
   /* B9 */    { &op_ADCD , EXTENDED     , 0, 8, 6 },
   /* BA */    { &op_ORD  , EXTENDED     , 0, 8, 6 },
   /* BB */    { &op_ADDW , EXTENDED     , 0, 8, 6 },
   /* BC */    { &op_CMPY , EXTENDED     , 0, 8, 6 },
   /* BD */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* BE */    { &op_LDY  , EXTENDED     , 0, 7, 6 },
   /* BF */    { &op_STY  , EXTENDED     , 0, 7, 6 },
   /* C0 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* C1 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* C2 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* C3 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* C4 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* C5 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* C6 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* C7 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* C8 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* C9 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* CA */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* CB */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* CC */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* CD */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* CE */    { &op_LDS  , IMMEDIATE_16 , 0, 4, 4 },
   /* CF */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* D0 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* D1 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* D2 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* D3 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* D4 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* D5 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* D6 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* D7 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* D8 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* D9 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* DA */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* DB */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* DC */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* DD */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* DE */    { &op_LDS  , DIRECT       , 0, 6, 5 },
   /* DF */    { &op_STS  , DIRECT       , 0, 6, 5 },
   /* E0 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* E1 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* E2 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* E3 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* E4 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* E5 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* E6 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* E7 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* E8 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* E9 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* EA */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* EB */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* EC */    { &op_LDQ  , INDEXED      , 0, 8, 6 },
   /* ED */    { &op_STQ  , INDEXED      , 0, 8, 6 },
   /* EE */    { &op_LDS  , INDEXED      , 0, 6, 6 },
   /* EF */    { &op_STS  , INDEXED      , 0, 6, 6 },
   /* F0 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* F1 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* F2 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* F3 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* F4 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* F5 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* F6 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* F7 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* F8 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* F9 */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* FA */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* FB */    { &op_TRAP , ILLEGAL      , 1,20,22 },
   /* FC */    { &op_LDQ  , EXTENDED     , 0, 9, 8 },
   /* FD */    { &op_STQ  , EXTENDED     , 0, 9, 8 },
   /* FE */    { &op_LDS  , EXTENDED     , 0, 7, 6 },
   /* FF */    { &op_STS  , EXTENDED     , 0, 7, 6 }
};

static instr_mode_t instr_table_6309_map2[] = {
   /* 00 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 01 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 02 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 03 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 04 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 05 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 06 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 07 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 08 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 09 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 0A */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 0B */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 0C */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 0D */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 0E */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 0F */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 10 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 11 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 12 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 13 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 14 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 15 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 16 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 17 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 18 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 19 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1A */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1B */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1C */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1D */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1E */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 1F */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 20 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 21 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 22 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 23 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 24 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 25 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 26 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 27 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 28 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 29 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 2A */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 2B */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 2C */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 2D */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 2E */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 2F */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 30 */    { &op_BAND , DIRECTBIT    , 0, 7, 6 },
   /* 31 */    { &op_BIAND, DIRECTBIT    , 0, 7, 6 },
   /* 32 */    { &op_BOR  , DIRECTBIT    , 0, 7, 6 },
   /* 33 */    { &op_BIOR , DIRECTBIT    , 0, 7, 6 },
   /* 34 */    { &op_BEOR , DIRECTBIT    , 0, 7, 6 },
   /* 35 */    { &op_BIEOR, DIRECTBIT    , 0, 7, 6 },
   /* 36 */    { &op_LDBT , DIRECTBIT    , 0, 7, 6 },
   /* 37 */    { &op_STBT , DIRECTBIT    , 0, 8, 7 },
   /* 38 */    { &op_TFM  , REGISTER     , 0, 6, 6 },
   /* 39 */    { &op_TFM  , REGISTER     , 0, 6, 6 },
   /* 3A */    { &op_TFM  , REGISTER     , 0, 6, 6 },
   /* 3B */    { &op_TFM  , REGISTER     , 0, 6, 6 },
   /* 3C */    { &op_BITMD, IMMEDIATE_8  , 0, 4, 4 },
   /* 3D */    { &op_LDMD , IMMEDIATE_8  , 0, 4, 5 },
   /* 3E */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 3F */    { &op_SWI3 , INHERENT     , 0,20,22 },
   /* 40 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 41 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 42 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 43 */    { &op_COME , INHERENT     , 0, 2, 2 },
   /* 44 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 45 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 46 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 47 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 48 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 49 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 4A */    { &op_DECE , INHERENT     , 0, 2, 2 },
   /* 4B */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 4C */    { &op_INCE , INHERENT     , 0, 2, 2 },
   /* 4D */    { &op_TSTE , INHERENT     , 0, 2, 2 },
   /* 4E */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 4F */    { &op_CLRE , INHERENT     , 0, 2, 2 },
   /* 50 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 51 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 52 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 53 */    { &op_COMF , INHERENT     , 0, 2, 2 },
   /* 54 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 55 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 56 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 57 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 58 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 59 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 5A */    { &op_DECF , INHERENT     , 0, 2, 2 },
   /* 5B */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 5C */    { &op_INCF , INHERENT     , 0, 2, 2 },
   /* 5D */    { &op_TSTF , ILLEGAL      , 0, 2, 2 },
   /* 5E */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 5F */    { &op_CLRF , INHERENT     , 0, 2, 2 },
   /* 60 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 61 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 62 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 63 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 64 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 65 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 66 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 67 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 68 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 69 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 6A */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 6B */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 6C */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 6D */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 6E */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 6F */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 70 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 71 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 72 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 73 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 74 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 75 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 76 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 77 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 78 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 79 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 7A */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 7B */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 7C */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 7D */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 7E */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 7F */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 80 */    { &op_SUBE , IMMEDIATE_8  , 0, 3, 3 },
   /* 81 */    { &op_CMPE , IMMEDIATE_8  , 0, 3, 3 },
   /* 82 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 83 */    { &op_CMPU , IMMEDIATE_16 , 0, 5, 4 },
   /* 84 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 85 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 86 */    { &op_LDE  , IMMEDIATE_8  , 0, 3, 3 },
   /* 87 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 88 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 89 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 8A */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 8B */    { &op_ADDE , IMMEDIATE_8  , 0, 3, 3 },
   /* 8C */    { &op_CMPS , IMMEDIATE_16 , 0, 5, 4 },
   /* 8D */    { &op_DIVD , IMMEDIATE_8  , 0,25,25 },
   /* 8E */    { &op_DIVQ , IMMEDIATE_16 , 0,34,34 },
   /* 8F */    { &op_MULD , IMMEDIATE_16 , 0,28,28 },
   /* 90 */    { &op_SUBE , DIRECT       , 0, 5, 4 },
   /* 91 */    { &op_CMPE , DIRECT       , 0, 5, 4 },
   /* 92 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 93 */    { &op_CMPU , DIRECT       , 0, 7, 5 },
   /* 94 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 95 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 96 */    { &op_LDE  , DIRECT       , 0, 5, 4 },
   /* 97 */    { &op_STE  , DIRECT       , 0, 5, 4 },
   /* 98 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 99 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 9A */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* 9B */    { &op_ADDE , DIRECT       , 0, 5, 4 },
   /* 9C */    { &op_CMPS , DIRECT       , 0, 7, 5 },
   /* 9D */    { &op_DIVD , DIRECT       , 0,27,26 },
   /* 9E */    { &op_DIVQ , DIRECT       , 0,26,35 },
   /* 9F */    { &op_MULD , DIRECT       , 0,30,29 },
   /* A0 */    { &op_SUBE , INDEXED      , 0, 5, 5 },
   /* A1 */    { &op_CMPE , INDEXED      , 0, 5, 5 },
   /* A2 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* A3 */    { &op_CMPU , INDEXED      , 0, 7, 6 },
   /* A4 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* A5 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* A6 */    { &op_LDE  , INDEXED      , 0, 5, 5 },
   /* A7 */    { &op_STE  , INDEXED      , 0, 5, 5 },
   /* A8 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* A9 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* AA */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* AB */    { &op_ADDE , INDEXED      , 0, 5, 5 },
   /* AC */    { &op_CMPS , INDEXED      , 0, 7, 6 },
   /* AD */    { &op_DIVD , INDEXED      , 0,27,27 },
   /* AE */    { &op_DIVQ , INDEXED      , 0,36,36 },
   /* AF */    { &op_MULD , INDEXED      , 0,30,30 },
   /* B0 */    { &op_SUBE , EXTENDED     , 0, 6, 5 },
   /* B1 */    { &op_CMPE , EXTENDED     , 0, 6, 5 },
   /* B2 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* B3 */    { &op_CMPU , EXTENDED     , 0, 8, 6 },
   /* B4 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* B5 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* B6 */    { &op_LDE  , EXTENDED     , 0, 6, 5 },
   /* B7 */    { &op_STE  , EXTENDED     , 0, 6, 5 },
   /* B8 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* B9 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* BA */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* BB */    { &op_ADDE , EXTENDED     , 0, 6, 5 },
   /* BC */    { &op_CMPS , EXTENDED     , 0, 8, 6 },
   /* BD */    { &op_DIVD , EXTENDED     , 0,28,27 },
   /* BE */    { &op_DIVQ , EXTENDED     , 0,37,36 },
   /* BF */    { &op_MULD , EXTENDED     , 0,31,30 },
   /* C0 */    { &op_SUBF , IMMEDIATE_8  , 0, 3, 3 },
   /* C1 */    { &op_CMPF , IMMEDIATE_8  , 0, 3, 3 },
   /* C2 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* C3 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* C4 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* C5 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* C6 */    { &op_LDF  , IMMEDIATE_8  , 0, 3, 3 },
   /* C7 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* C8 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* C9 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* CA */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* CB */    { &op_ADDF , IMMEDIATE_8  , 0, 3, 3 },
   /* CC */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* CD */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* CE */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* CF */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* D0 */    { &op_SUBF , DIRECT       , 0, 5, 4 },
   /* D1 */    { &op_CMPF , DIRECT       , 0, 5, 4 },
   /* D2 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* D3 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* D4 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* D5 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* D6 */    { &op_LDF  , DIRECT       , 0, 5, 4 },
   /* D7 */    { &op_STF  , DIRECT       , 0, 5, 4 },
   /* D8 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* D9 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* DA */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* DB */    { &op_ADDF , DIRECT       , 0, 5, 4 },
   /* DC */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* DD */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* DE */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* DF */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* E0 */    { &op_SUBF , INDEXED      , 0, 5, 5 },
   /* E1 */    { &op_CMPF , INDEXED      , 0, 5, 5 },
   /* E2 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* E3 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* E4 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* E5 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* E6 */    { &op_LDF  , INDEXED      , 0, 5, 5 },
   /* E7 */    { &op_STF  , INDEXED      , 0, 5, 5 },
   /* E8 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* E9 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* EA */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* EB */    { &op_ADDF , INDEXED      , 0, 5, 5 },
   /* EC */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* ED */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* EE */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* EF */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* F0 */    { &op_SUBF , EXTENDED     , 0, 6, 5 },
   /* F1 */    { &op_CMPF , EXTENDED     , 0, 6, 5 },
   /* F2 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* F3 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* F4 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* F5 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* F6 */    { &op_LDF  , EXTENDED     , 0, 6, 5 },
   /* F7 */    { &op_STF  , EXTENDED     , 0, 6, 5 },
   /* F8 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* F9 */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* FA */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* FB */    { &op_ADDF , EXTENDED     , 0, 6, 5 },
   /* FC */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* FD */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* FE */    { &op_TRAP , ILLEGAL      , 0,20,22 },
   /* FF */    { &op_TRAP , ILLEGAL      , 0,20,22 }
};
