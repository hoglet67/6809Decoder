#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "memory.h"
#include "em_6809.h"

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
} AddrMode ;

typedef enum {
   READOP,
   WRITEOP,
   RMWOP,
   BRANCHOP,
   OTHER
} OpType;

typedef int operand_t;

typedef int ea_t;

typedef struct {
   const char *mnemonic;
   AddrMode mode;
   int cycles;
   int (*emulate)(operand_t, ea_t);
   int len;
   const char *fmt;
} InstrType;

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

static const char statusString[] = "EFHINZVC";

static const char regi[] = { 'X', 'Y', 'U', 'S' };

static const char *exgi[] = { "D", "X", "Y", "U", "S", "PC", "??", "??", "A",
                              "B", "CC", "DP", "??", "??", "??", "??" };

static const char *pshsregi[] = { "PC", "U", "Y", "X", "DP", "B", "A", "CC" };

static const char *pshuregi[] = { "PC", "S", "Y", "X", "DP", "B", "A", "CC" };

static const char default_state[] = "A=?? B=?? X=???? Y=???? U=???? S=???? DP=?? E=? F=? H=? I=? N=? Z=? V=? C=?";

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

// ====================================================================
// Forward declarations
// ====================================================================

static InstrType instr_table_6809_map0[];
static InstrType instr_table_6809_map1[];
static InstrType instr_table_6809_map2[];

static int op_STA(operand_t operand, ea_t ea);
static int op_STX(operand_t operand, ea_t ea);
static int op_STY(operand_t operand, ea_t ea);

// ====================================================================
// Helper Methods
// ====================================================================

static int compare_FLAGS(int operand) {
   if (E >= 0) {
      if (E != ((operand >> 7) & 1)) {
         return 1;
      }
   }
   if (F >= 0) {
      if (F != ((operand >> 6) & 1)) {
         return 1;
      }
   }
   if (H >= 0) {
      if (H != ((operand >> 5) & 1)) {
         return 1;
      }
   }
   if (I >= 0) {
      if (I != ((operand >> 4) & 1)) {
         return 1;
      }
   }
   if (N >= 0) {
      if (N != ((operand >> 3) & 1)) {
         return 1;
      }
   }
   if (Z >= 0) {
      if (Z != ((operand >> 2) & 1)) {
         return 1;
      }
   }
   if (V >= 0) {
      if (V != ((operand >> 1) & 1)) {
         return 1;
      }
   }
   if (C >= 0) {
      if (C != ((operand >> 0) & 1)) {
         return 1;
      }
   }
   return 0;
}

static void check_FLAGS(int operand) {
   failflag |= compare_FLAGS(operand);
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

static void set_NVZC_unknown() {
   N = -1;
   V = -1;
   Z = -1;
   C = -1;
}

static void set_NZ(int value) {
   N = (value & 128) > 0;
   Z = value == 0;
}


static void pop8(int value) {
   if (S >= 0) {
      S = (S + 1) & 0xff;
      memory_read(value & 0xff, 0x100 + S, MEM_STACK);
   }
}

static void push8(int value) {
   if (S >= 0) {
      memory_write(value & 0xff, 0x100 + S, MEM_STACK);
      S = (S - 1) & 0xff;
   }
}

static void push16(int value) {
   push8(value >> 8);
   push8(value);
}

static uint8_t get_opcode(uint8_t b0, uint8_t b1) {
   return (b0 == 0x10 || b0 == 0x11) ? b1 : b0;
}

static InstrType *get_instruction(uint8_t b0, uint8_t b1) {
   if (b0 == 0x11) {
      return instr_table_6809_map2 + b1;
   } else if (b0 == 0x10) {
      return instr_table_6809_map1 + b1;
   } else {
      return instr_table_6809_map0 + b0;
   }
}


// ====================================================================
// Public Methods
// ====================================================================

static void em_6809_init(arguments_t *args) {
}

static int em_6809_match_interrupt(sample_t *sample_q, int num_samples) {
   return 0;
}

static int postbyte_cycles[] = { 2, 3, 2, 3, 0, 1, 1, 0, 1, 4, 0, 4, 1, 5, 0, 5 };

static int count_bits[] =    { 0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4};

static int em_6809_count_cycles(sample_t *sample_q) {
   uint8_t b0 = sample_q[0].data;
   uint8_t b1 = sample_q[1].data;
   InstrType *instr = get_instruction(b0, b1);
   int cycle_count = instr->cycles;
   // TODO: Long Branch, one extra if branch talen
   // TODO: RTI 6/15 based on E flag
   if (b0 >= 0x34 && b0 <= 0x37) {
      // PSHS/PULS/PSHU/PULU
      cycle_count += count_bits[b1 & 0x0f];            // bits 0..3 are 8 bit registers
      cycle_count += count_bits[(b1 >> 4) & 0x0f] * 2; // bits 4..7 are 16 bit registers
   } else if (instr->mode == INDEXED) {
      int postindex = (b0 == 0x10 || b0 == 0x11) ? 2 : 1;
      int postbyte = sample_q[postindex].data;
      if (postbyte & 0x80) {
         cycle_count += postbyte_cycles[postbyte & 0x0F];
      } else {
         cycle_count += 1;
      }
   }
   return cycle_count;
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
   PC = (sample_q[num_cycles - 1].data << 8) + sample_q[num_cycles - 2].data;
}

static void em_6809_interrupt(sample_t *sample_q, int num_cycles, instruction_t *instruction) {
}

static void em_6809_emulate(sample_t *sample_q, int num_cycles, instruction_t *instruction) {

   InstrType *instr;
   int index = 0;

   instruction->prefix = 0;
   instruction->opcode = sample_q[index].data;
   if (PC >= 0) {
      memory_read(instruction->opcode, PC + index, MEM_FETCH);
   }
   index++;
   instr = instr_table_6809_map0;

   // Handle the tw prefixes
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
   }

   // Move down to the instruction metadata
   instr += instruction->opcode;

   switch (instr->mode) {
   case REGISTER:
      // TODO
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
            instruction->op1 = sample_q[index].data;
            if (PC >= 0) {
               memory_read(instruction->op1, PC + index, MEM_INSTR);
            }
            index++;
         }
         if (type == 9 || type == 13 || type == 15) {
            instruction->op2 = sample_q[index].data;
            if (PC >= 0) {
               memory_read(instruction->op2, PC + index, MEM_INSTR);
            }
            index++;
         }
      }
      break;
   case IMMEDIATE_8:
   case RELATIVE_8:
   case DIRECT:
      instruction->op1 = sample_q[index].data;
      if (PC >= 0) {
         memory_read(instruction->op1, PC + index, MEM_INSTR);
      }
      index++;
      break;
   case IMMEDIATE_16:
   case RELATIVE_16:
   case EXTENDED:
      instruction->op1 = sample_q[index].data;
      instruction->op2 = sample_q[index + 1].data;
      if (PC >= 0) {
         memory_read(instruction->op1, PC + index, MEM_INSTR);
         memory_read(instruction->op2, PC + index + 1, MEM_INSTR);
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

   instruction->pc = PC;

   // TODO: All emulation!
   if (instr->emulate) {

   }

   // TODO: Update PC

   int opcode = (instruction->prefix << 8) + instruction->opcode;

   // Look for control flow changes and update the PC
   if (opcode == 0x7e) {
      // JMP
      PC = (instruction->op1 << 8) + instruction->op2;
   } else {
      // Otherwise, increment pc by length of instuction
      PC = (PC + instruction->length) & 0xffff;
   }

}

static char *strcc(char *ptr, uint8_t val) {
  uint8_t i;
  for (i = 0; i < 8; i++) {
    *ptr++ = (val & 0x80) ? statusString[i] : '.';
    val <<= 1;
  }
  return ptr;
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
   InstrType *instr = get_instruction(b0, b1);

   /// Output the mnemonic
   char *ptr = buffer;
   strcpy(ptr, instr->mnemonic);
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
      write_hex2(ptr, instruction->op1);
      ptr += 2;
      break;
   case IMMEDIATE_16:
      *ptr++ = '#';
      *ptr++ = '$';
      write_hex4(ptr, (instruction->op1 << 8) + instruction->op2);
      ptr += 4;
      break;
   case RELATIVE_8:
   case RELATIVE_16:
      {
         int16_t offset;
         if (instr->mode == RELATIVE_8) {
            offset = (int16_t)((int8_t)instruction->op1);
         } else {
            offset = (int16_t)((instruction->op1 << 8) + instruction->op2);
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
      write_hex2(ptr, instruction->op1);
      ptr += 2;
      break;
   case EXTENDED:
      *ptr++ = '$';
      write_hex4(ptr, (instruction->op1 << 8) + instruction->op2);
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
               ptr += 2;
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
               write_hex2(ptr, instruction->op1);
               ptr += 2;
               *ptr++ = ',';
               *ptr++ = reg;
               break;
            case 9:                 /* n15,R */
               *ptr++ = '$';
               write_hex2(ptr, instruction->op1);
               ptr += 2;
               write_hex2(ptr, instruction->op2);
               ptr += 2;
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
               write_hex2(ptr, instruction->op1);
               ptr += 2;
               *ptr++ = ',';
               *ptr++ = 'P';
               *ptr++ = 'C';
               *ptr++ = 'R';
               break;
            case 13:                /* n15,PCR */
               *ptr++ = '$';
               write_hex4(ptr, (instruction->op1 << 8) + instruction->op2);
               ptr += 4;
               *ptr++ = ',';
               *ptr++ = 'P';
               *ptr++ = 'C';
               *ptr++ = 'R';
               break;
            case 15:                /* [n] */
               *ptr++ = '$';
               write_hex4(ptr, (instruction->op1 << 8) + instruction->op2);
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

static int em_6809_get_and_clear_fail() {
   int ret = failflag;
   failflag = 0;
   return ret;
}

cpu_emulator_t em_6809 = {
   .init = em_6809_init,
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
};

// ====================================================================
// Individual Instructions
// ====================================================================

 static int op_ABX(operand_t operand, ea_t ea) {
   return -1;
}

static int op_ADCA(operand_t operand, ea_t ea) {
   return -1;
}

static int op_ADCB(operand_t operand, ea_t ea) {
   return -1;
}

static int op_ADDA(operand_t operand, ea_t ea) {
   return -1;
}

static int op_ADDB(operand_t operand, ea_t ea) {
   return -1;
}

static int op_ADDD(operand_t operand, ea_t ea) {
   return -1;
}

static int op_ANDA(operand_t operand, ea_t ea) {
   return -1;
}

static int op_ANDB(operand_t operand, ea_t ea) {
   return -1;
}

static int op_ANDC(operand_t operand, ea_t ea) {
   return -1;
}

static int op_ASL(operand_t operand, ea_t ea) {
   return -1;
}

static int op_ASLA(operand_t operand, ea_t ea) {
   return -1;
}

static int op_ASR(operand_t operand, ea_t ea) {
   return -1;
}

static int op_ASRA(operand_t operand, ea_t ea) {
   return -1;
}

static int op_ASRB(operand_t operand, ea_t ea) {
   return -1;
}

static int op_BCC(operand_t operand, ea_t ea) {
   return -1;
}

static int op_BEQ(operand_t operand, ea_t ea) {
   return -1;
}

static int op_BGE(operand_t operand, ea_t ea) {
   return -1;
}

static int op_BGT(operand_t operand, ea_t ea) {
   return -1;
}

static int op_BHI(operand_t operand, ea_t ea) {
   return -1;
}

static int op_BITA(operand_t operand, ea_t ea) {
   return -1;
}

static int op_BITB(operand_t operand, ea_t ea) {
   return -1;
}

static int op_BLE(operand_t operand, ea_t ea) {
   return -1;
}

static int op_BLO(operand_t operand, ea_t ea) {
   return -1;
}

static int op_BLS(operand_t operand, ea_t ea) {
   return -1;
}

static int op_BLT(operand_t operand, ea_t ea) {
   return -1;
}

static int op_BMI(operand_t operand, ea_t ea) {
   return -1;
}

static int op_BNE(operand_t operand, ea_t ea) {
   return -1;
}

static int op_BPL(operand_t operand, ea_t ea) {
   return -1;
}

static int op_BRA(operand_t operand, ea_t ea) {
   return -1;
}

static int op_BRN(operand_t operand, ea_t ea) {
   return -1;
}

static int op_BSR(operand_t operand, ea_t ea) {
   return -1;
}

static int op_BVC(operand_t operand, ea_t ea) {
   return -1;
}

static int op_BVS(operand_t operand, ea_t ea) {
   return -1;
}

static int op_CLR(operand_t operand, ea_t ea) {
   return -1;
}

static int op_CLRA(operand_t operand, ea_t ea) {
   return -1;
}

static int op_CLRB(operand_t operand, ea_t ea) {
   return -1;
}

static int op_CMPA(operand_t operand, ea_t ea) {
   return -1;
}

static int op_CMPB(operand_t operand, ea_t ea) {
   return -1;
}

static int op_CMPD(operand_t operand, ea_t ea) {
   return -1;
}

static int op_CMPS(operand_t operand, ea_t ea) {
   return -1;
}

static int op_CMPU(operand_t operand, ea_t ea) {
   return -1;
}

static int op_CMPX(operand_t operand, ea_t ea) {
   return -1;
}

static int op_CMPY(operand_t operand, ea_t ea) {
   return -1;
}

static int op_COM(operand_t operand, ea_t ea) {
   return -1;
}

static int op_COMA(operand_t operand, ea_t ea) {
   return -1;
}

static int op_COMB(operand_t operand, ea_t ea) {
   return -1;
}

static int op_CWAI(operand_t operand, ea_t ea) {
   return -1;
}

static int op_DAA(operand_t operand, ea_t ea) {
   return -1;
}
static int op_DEC(operand_t operand, ea_t ea) {
   return -1;
}

static int op_DECA(operand_t operand, ea_t ea) {
   return -1;
}

static int op_DECB(operand_t operand, ea_t ea) {
   return -1;
}

static int op_EORA(operand_t operand, ea_t ea) {
   return -1;
}

static int op_EORB(operand_t operand, ea_t ea) {
   return -1;
}

static int op_EXG(operand_t operand, ea_t ea) {
   return -1;
}

static int op_INC(operand_t operand, ea_t ea) {
   return -1;
}
static int op_INCA(operand_t operand, ea_t ea) {
   return -1;
}

static int op_INCB(operand_t operand, ea_t ea) {
   return -1;
}

static int op_JMP(operand_t operand, ea_t ea) {
   return -1;
}

static int op_JSR(operand_t operand, ea_t ea) {
   return -1;
}

static int op_LBCC(operand_t operand, ea_t ea) {
   return -1;
}

static int op_LBEQ(operand_t operand, ea_t ea) {
   return -1;
}

static int op_LBGE(operand_t operand, ea_t ea) {
   return -1;
}

static int op_LBGT(operand_t operand, ea_t ea) {
   return -1;
}

static int op_LBHI(operand_t operand, ea_t ea) {
   return -1;
}

static int op_LBLE(operand_t operand, ea_t ea) {
   return -1;
}

static int op_LBLO(operand_t operand, ea_t ea) {
   return -1;
}

static int op_LBLS(operand_t operand, ea_t ea) {
   return -1;
}

static int op_LBLT(operand_t operand, ea_t ea) {
   return -1;
}

static int op_LBMI(operand_t operand, ea_t ea) {
   return -1;
}

static int op_LBNE(operand_t operand, ea_t ea) {
   return -1;
}

static int op_LBPL(operand_t operand, ea_t ea) {
   return -1;
}

static int op_LBRA(operand_t operand, ea_t ea) {
   return -1;
}

static int op_LBRN(operand_t operand, ea_t ea) {
   return -1;
}

static int op_LBSR(operand_t operand, ea_t ea) {
   return -1;
}

static int op_LBVC(operand_t operand, ea_t ea) {
   return -1;
}

static int op_LBVS(operand_t operand, ea_t ea) {
   return -1;
}

static int op_LDA(operand_t operand, ea_t ea) {
   return -1;
}

static int op_LDB(operand_t operand, ea_t ea) {
   return -1;
}

static int op_LDD(operand_t operand, ea_t ea) {
   return -1;
}

static int op_LDS(operand_t operand, ea_t ea) {
   return -1;
}

static int op_LDU(operand_t operand, ea_t ea) {
   return -1;
}

static int op_LDX(operand_t operand, ea_t ea) {
   return -1;
}

static int op_LDY(operand_t operand, ea_t ea) {
   return -1;
}

static int op_LEAS(operand_t operand, ea_t ea) {
   return -1;
}

static int op_LEAU(operand_t operand, ea_t ea) {
   return -1;
}

static int op_LEAX(operand_t operand, ea_t ea) {
   return -1;
}

static int op_LEAY(operand_t operand, ea_t ea) {
   return -1;
}

static int op_LSLB(operand_t operand, ea_t ea) {
   return -1;
}

static int op_LSR(operand_t operand, ea_t ea) {
   return -1;
}

static int op_LSRA(operand_t operand, ea_t ea) {
   return -1;
}

static int op_LSRB(operand_t operand, ea_t ea) {
   return -1;
}

static int op_MUL(operand_t operand, ea_t ea) {
   return -1;
}

static int op_NEG(operand_t operand, ea_t ea) {
   return -1;
}

static int op_NEGA(operand_t operand, ea_t ea) {
   return -1;
}

static int op_NEGB(operand_t operand, ea_t ea) {
   return -1;
}

static int op_NOP(operand_t operand, ea_t ea) {
   return -1;
}

static int op_ORA(operand_t operand, ea_t ea) {
   return -1;
}

static int op_ORB(operand_t operand, ea_t ea) {
   return -1;
}

static int op_ORCC(operand_t operand, ea_t ea) {
   return -1;
}

static int op_PSHS(operand_t operand, ea_t ea) {
   return -1;
}

static int op_PSHU(operand_t operand, ea_t ea) {
   return -1;
}

static int op_PULS(operand_t operand, ea_t ea) {
   return -1;
}

static int op_PULU(operand_t operand, ea_t ea) {
   return -1;
}

static int op_ROL(operand_t operand, ea_t ea) {
   return -1;
}

static int op_ROLA(operand_t operand, ea_t ea) {
   return -1;
}

static int op_ROLB(operand_t operand, ea_t ea) {
   return -1;
}

static int op_ROR(operand_t operand, ea_t ea) {
   return -1;
}

static int op_RORA(operand_t operand, ea_t ea) {
   return -1;
}

static int op_RORB(operand_t operand, ea_t ea) {
   return -1;
}

static int op_RTI(operand_t operand, ea_t ea) {
   return -1;
}

static int op_RTS(operand_t operand, ea_t ea) {
   return -1;
}

static int op_SBCA(operand_t operand, ea_t ea) {
   return -1;
}

static int op_SBCB(operand_t operand, ea_t ea) {
   return -1;
}

static int op_SEX(operand_t operand, ea_t ea) {
   return -1;
}

static int op_STA(operand_t operand, ea_t ea) {
   return -1;
}

static int op_STB(operand_t operand, ea_t ea) {
   return -1;
}

static int op_STD(operand_t operand, ea_t ea) {
   return -1;
}

static int op_STS(operand_t operand, ea_t ea) {
   return -1;
}

static int op_STU(operand_t operand, ea_t ea) {
   return -1;
}

static int op_STX(operand_t operand, ea_t ea) {
   return -1;
}

static int op_STY(operand_t operand, ea_t ea) {
   return -1;
}

static int op_SUBA(operand_t operand, ea_t ea) {
   return -1;
}

static int op_SUBB(operand_t operand, ea_t ea) {
   return -1;
}

static int op_SUBD(operand_t operand, ea_t ea) {
   return -1;
}

static int op_SWI(operand_t operand, ea_t ea) {
   return -1;
}

static int op_SWI2(operand_t operand, ea_t ea) {
   return -1;
}

static int op_SWI30(operand_t operand, ea_t ea) {
   return -1;
}

static int op_SYNC(operand_t operand, ea_t ea) {
   return -1;
}

static int op_TFR(operand_t operand, ea_t ea) {
   return -1;
}

static int op_TST(operand_t operand, ea_t ea) {
   return -1;
}

static int op_TSTA(operand_t operand, ea_t ea) {
   return -1;
}

static int op_TSTB(operand_t operand, ea_t ea) {
   return -1;
}

static int op_UU(operand_t operand, ea_t ea) {
   return -1;
}

static int op_XX(operand_t operand, ea_t ea) {
   return -1;
}


// ====================================================================
// Opcode Tables
// ====================================================================

static InstrType instr_table_6809_map0[] = {
   /* 00 */    { "NEG ", DIRECT       , 6, op_NEG  },
   /* 01 */    { "??? ", DIRECT       , 6, op_XX   },
   /* 02 */    { "??? ", DIRECT       , 6, op_XX   },
   /* 03 */    { "COM ", DIRECT       , 6, op_COM  },
   /* 04 */    { "LSR ", DIRECT       , 6, op_LSR  },
   /* 05 */    { "??? ", DIRECT       , 6, op_XX   },
   /* 06 */    { "ROR ", DIRECT       , 6, op_ROR  },
   /* 07 */    { "ASR ", DIRECT       , 6, op_ASR  },
   /* 08 */    { "ASL ", DIRECT       , 6, op_ASL  },
   /* 09 */    { "ROL ", DIRECT       , 6, op_ROL  },
   /* 0A */    { "DEC ", DIRECT       , 6, op_DEC  },
   /* 0B */    { "??? ", DIRECT       , 6, op_XX   },
   /* 0C */    { "INC ", DIRECT       , 6, op_INC  },
   /* 0D */    { "TST ", DIRECT       , 6, op_TST  },
   /* 0E */    { "JMP ", DIRECT       , 3, op_JMP  },
   /* 0F */    { "CLR ", DIRECT       , 6, op_CLR  },
   /* 10 */    { "UU  ", INHERENT     , 1, op_UU   },
   /* 11 */    { "UU  ", INHERENT     , 1, op_UU   },
   /* 12 */    { "NOP ", INHERENT     , 2, op_NOP  },
   /* 13 */    { "SYNC", INHERENT     , 2, op_SYNC },
   /* 14 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 15 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 16 */    { "LBRA", RELATIVE_16  , 5, op_LBRA },
   /* 17 */    { "LBSR", RELATIVE_16  , 9, op_LBSR },
   /* 18 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 19 */    { "DAA ", INHERENT     , 2, op_DAA  },
   /* 1A */    { "ORCC", IMMEDIATE_8  , 3, op_ORCC },
   /* 1B */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 1C */    { "ANDC", IMMEDIATE_8  , 3, op_ANDC },
   /* 1D */    { "SEX ", INHERENT     , 2, op_SEX  },
   /* 1E */    { "EXG ", REGISTER     , 8, op_EXG  },
   /* 1F */    { "TFR ", REGISTER     , 6, op_TFR  },
   /* 20 */    { "BRA ", RELATIVE_8   , 3, op_BRA  },
   /* 21 */    { "BRN ", RELATIVE_8   , 3, op_BRN  },
   /* 22 */    { "BHI ", RELATIVE_8   , 3, op_BHI  },
   /* 23 */    { "BLS ", RELATIVE_8   , 3, op_BLS  },
   /* 24 */    { "BCC ", RELATIVE_8   , 3, op_BCC  },
   /* 25 */    { "BLO ", RELATIVE_8   , 3, op_BLO  },
   /* 26 */    { "BNE ", RELATIVE_8   , 3, op_BNE  },
   /* 27 */    { "BEQ ", RELATIVE_8   , 3, op_BEQ  },
   /* 28 */    { "BVC ", RELATIVE_8   , 3, op_BVC  },
   /* 29 */    { "BVS ", RELATIVE_8   , 3, op_BVS  },
   /* 2A */    { "BPL ", RELATIVE_8   , 3, op_BPL  },
   /* 2B */    { "BMI ", RELATIVE_8   , 3, op_BMI  },
   /* 2C */    { "BGE ", RELATIVE_8   , 3, op_BGE  },
   /* 2D */    { "BLT ", RELATIVE_8   , 3, op_BLT  },
   /* 2E */    { "BGT ", RELATIVE_8   , 3, op_BGT  },
   /* 2F */    { "BLE ", RELATIVE_8   , 3, op_BLE  },
   /* 30 */    { "LEAX", INDEXED      , 4, op_LEAX },
   /* 31 */    { "LEAY", INDEXED      , 4, op_LEAY },
   /* 32 */    { "LEAS", INDEXED      , 4, op_LEAS },
   /* 33 */    { "LEAU", INDEXED      , 4, op_LEAU },
   /* 34 */    { "PSHS", REGISTER     , 5, op_PSHS },
   /* 35 */    { "PULS", REGISTER     , 5, op_PULS },
   /* 36 */    { "PSHU", REGISTER     , 5, op_PSHU },
   /* 37 */    { "PULU", REGISTER     , 5, op_PULU },
   /* 38 */    { "??? ", INHERENT     , 1, op_XX   },
   /* 39 */    { "RTS ", INHERENT     , 5, op_RTS  },
   /* 3A */    { "ABX ", INHERENT     , 3, op_ABX  },
   /* 3B */    { "RTI ", INHERENT     , 6, op_RTI  },
   /* 3C */    { "CWAI", IMMEDIATE_8  , 1, op_CWAI },
   /* 3D */    { "MUL ", INHERENT     ,11, op_MUL  },
   /* 3E */    { "??? ", INHERENT     , 1, op_XX   },
   /* 3F */    { "SWI ", INHERENT     ,19, op_SWI  },
   /* 40 */    { "NEGA", INHERENT     , 2, op_NEGA },
   /* 41 */    { "??? ", INHERENT     , 1, op_XX   },
   /* 42 */    { "??? ", INHERENT     , 1, op_XX   },
   /* 43 */    { "COMA", INHERENT     , 2, op_COMA },
   /* 44 */    { "LSRA", INHERENT     , 2, op_LSRA },
   /* 45 */    { "??? ", INHERENT     , 1, op_XX   },
   /* 46 */    { "RORA", INHERENT     , 2, op_RORA },
   /* 47 */    { "ASRA", INHERENT     , 2, op_ASRA },
   /* 48 */    { "ASLA", INHERENT     , 2, op_ASLA },
   /* 49 */    { "ROLA", INHERENT     , 2, op_ROLA },
   /* 4A */    { "DECA", INHERENT     , 2, op_DECA },
   /* 4B */    { "??? ", INHERENT     , 2, op_XX   },
   /* 4C */    { "INCA", INHERENT     , 2, op_INCA },
   /* 4D */    { "TSTA", INHERENT     , 2, op_TSTA },
   /* 4E */    { "??? ", INHERENT     , 2, op_XX   },
   /* 4F */    { "CLRA", INHERENT     , 2, op_CLRA },
   /* 50 */    { "NEGB", INHERENT     , 2, op_NEGB },
   /* 51 */    { "??? ", INHERENT     , 2, op_XX   },
   /* 52 */    { "??? ", INHERENT     , 2, op_XX   },
   /* 53 */    { "COMB", INHERENT     , 2, op_COMB },
   /* 54 */    { "LSRB", INHERENT     , 2, op_LSRB },
   /* 55 */    { "??? ", INHERENT     , 2, op_XX   },
   /* 56 */    { "RORB", INHERENT     , 2, op_RORB },
   /* 57 */    { "ASRB", INHERENT     , 2, op_ASRB },
   /* 58 */    { "LSLB", INHERENT     , 2, op_LSLB },
   /* 59 */    { "ROLB", INHERENT     , 2, op_ROLB },
   /* 5A */    { "DECB", INHERENT     , 2, op_DECB },
   /* 5B */    { "??? ", INHERENT     , 2, op_XX   },
   /* 5C */    { "INCB", INHERENT     , 2, op_INCB },
   /* 5D */    { "TSTB", INHERENT     , 2, op_TSTB },
   /* 5E */    { "??? ", INHERENT     , 2, op_XX   },
   /* 5F */    { "CLRB", INHERENT     , 2, op_CLRB },
   /* 60 */    { "NEG ", INDEXED      , 6, op_NEG  },
   /* 61 */    { "??? ", ILLEGAL      , 6, op_XX   },
   /* 62 */    { "??? ", ILLEGAL      , 6, op_XX   },
   /* 63 */    { "COM ", INDEXED      , 6, op_COM  },
   /* 64 */    { "LSR ", INDEXED      , 6, op_LSR  },
   /* 65 */    { "??? ", ILLEGAL      , 6, op_XX   },
   /* 66 */    { "ROR ", INDEXED      , 6, op_ROR  },
   /* 67 */    { "ASR ", INDEXED      , 6, op_ASR  },
   /* 68 */    { "ASL ", INDEXED      , 6, op_ASL  },
   /* 69 */    { "ROL ", INDEXED      , 6, op_ROL  },
   /* 6A */    { "DEC ", INDEXED      , 6, op_DEC  },
   /* 6B */    { "??? ", ILLEGAL      , 6, op_XX   },
   /* 6C */    { "INC ", INDEXED      , 6, op_INC  },
   /* 6D */    { "TST ", INDEXED      , 6, op_TST  },
   /* 6E */    { "JMP ", INDEXED      , 3, op_JMP  },
   /* 6F */    { "CLR ", INDEXED      , 6, op_CLR  },
   /* 70 */    { "NEG ", EXTENDED     , 7, op_NEG  },
   /* 71 */    { "??? ", ILLEGAL      , 7, op_XX   },
   /* 72 */    { "??? ", ILLEGAL      , 7, op_XX   },
   /* 73 */    { "COM ", EXTENDED     , 7, op_COM  },
   /* 74 */    { "LSR ", EXTENDED     , 7, op_LSR  },
   /* 75 */    { "??? ", ILLEGAL      , 7, op_XX   },
   /* 76 */    { "ROR ", EXTENDED     , 7, op_ROR  },
   /* 77 */    { "ASR ", EXTENDED     , 7, op_ASR  },
   /* 78 */    { "ASL ", EXTENDED     , 7, op_ASL  },
   /* 79 */    { "ROL ", EXTENDED     , 7, op_ROL  },
   /* 7A */    { "DEC ", EXTENDED     , 7, op_DEC  },
   /* 7B */    { "??? ", ILLEGAL      , 7, op_XX   },
   /* 7C */    { "INC ", EXTENDED     , 7, op_INC  },
   /* 7D */    { "TST ", EXTENDED     , 7, op_TST  },
   /* 7E */    { "JMP ", EXTENDED     , 4, op_JMP  },
   /* 7F */    { "CLR ", EXTENDED     , 7, op_CLR  },
   /* 80 */    { "SUBA", IMMEDIATE_8  , 2, op_SUBA },
   /* 81 */    { "CMPA", IMMEDIATE_8  , 2, op_CMPA },
   /* 82 */    { "SBCA", IMMEDIATE_8  , 2, op_SBCA },
   /* 83 */    { "SUBD", IMMEDIATE_16 , 4, op_SUBD },
   /* 84 */    { "ANDA", IMMEDIATE_8  , 2, op_ANDA },
   /* 85 */    { "BITA", IMMEDIATE_8  , 2, op_BITA },
   /* 86 */    { "LDA ", IMMEDIATE_8  , 2, op_LDA  },
   /* 87 */    { "??? ", ILLEGAL      , 2, op_XX   },
   /* 88 */    { "EORA", IMMEDIATE_8  , 2, op_EORA },
   /* 89 */    { "ADCA", IMMEDIATE_8  , 2, op_ADCA },
   /* 8A */    { "ORA ", IMMEDIATE_8  , 2, op_ORA  },
   /* 8B */    { "ADDA", IMMEDIATE_8  , 2, op_ADDA },
   /* 8C */    { "CMPX", IMMEDIATE_16 , 4, op_CMPX },
   /* 8D */    { "BSR ", RELATIVE_8   , 7, op_BSR  },
   /* 8E */    { "LDX ", IMMEDIATE_16 , 3, op_LDX  },
   /* 8F */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 90 */    { "SUBA", DIRECT       , 4, op_SUBA },
   /* 91 */    { "CMPA", DIRECT       , 4, op_CMPA },
   /* 92 */    { "SBCA", DIRECT       , 4, op_SBCA },
   /* 93 */    { "SUBD", DIRECT       , 6, op_SUBD },
   /* 94 */    { "ANDA", DIRECT       , 4, op_ANDA },
   /* 95 */    { "BITA", DIRECT       , 4, op_BITA },
   /* 96 */    { "LDA ", DIRECT       , 4, op_LDA  },
   /* 97 */    { "STA ", DIRECT       , 4, op_STA  },
   /* 98 */    { "EORA", DIRECT       , 4, op_EORA },
   /* 99 */    { "ADCA", DIRECT       , 4, op_ADCA },
   /* 9A */    { "ORA ", DIRECT       , 4, op_ORA  },
   /* 9B */    { "ADDA", DIRECT       , 4, op_ADDA },
   /* 9C */    { "CMPX", DIRECT       , 6, op_CMPX },
   /* 9D */    { "JSR ", DIRECT       , 7, op_JSR  },
   /* 9E */    { "LDX ", DIRECT       , 5, op_LDX  },
   /* 9F */    { "STX ", DIRECT       , 5, op_STX  },
   /* A0 */    { "SUBA", INDEXED      , 4, op_SUBA },
   /* A1 */    { "CMPA", INDEXED      , 4, op_CMPA },
   /* A2 */    { "SBCA", INDEXED      , 4, op_SBCA },
   /* A3 */    { "SUBD", INDEXED      , 6, op_SUBD },
   /* A4 */    { "ANDA", INDEXED      , 4, op_ANDA },
   /* A5 */    { "BITA", INDEXED      , 4, op_BITA },
   /* A6 */    { "LDA ", INDEXED      , 4, op_LDA  },
   /* A7 */    { "STA ", INDEXED      , 4, op_STA  },
   /* A8 */    { "EORA", INDEXED      , 4, op_EORA },
   /* A9 */    { "ADCA", INDEXED      , 4, op_ADCA },
   /* AA */    { "ORA ", INDEXED      , 4, op_ORA  },
   /* AB */    { "ADDA", INDEXED      , 4, op_ADDA },
   /* AC */    { "CMPX", INDEXED      , 6, op_CMPX },
   /* AD */    { "JSR ", INDEXED      , 7, op_JSR  },
   /* AE */    { "LDX ", INDEXED      , 5, op_LDX  },
   /* AF */    { "STX ", INDEXED      , 5, op_STX  },
   /* B0 */    { "SUBA", EXTENDED     , 5, op_SUBA },
   /* B1 */    { "CMPA", EXTENDED     , 5, op_CMPA },
   /* B2 */    { "SBCA", EXTENDED     , 5, op_SBCA },
   /* B3 */    { "SUBD", EXTENDED     , 7, op_SUBD },
   /* B4 */    { "ANDA", EXTENDED     , 5, op_ANDA },
   /* B5 */    { "BITA", EXTENDED     , 5, op_BITA },
   /* B6 */    { "LDA ", EXTENDED     , 5, op_LDA  },
   /* B7 */    { "STA ", EXTENDED     , 5, op_STA  },
   /* B8 */    { "EORA", EXTENDED     , 5, op_EORA },
   /* B9 */    { "ADCA", EXTENDED     , 5, op_ADCA },
   /* BA */    { "ORA ", EXTENDED     , 5, op_ORA  },
   /* BB */    { "ADDA", EXTENDED     , 5, op_ADDA },
   /* BC */    { "CMPX", EXTENDED     , 7, op_CMPX },
   /* BD */    { "JSR ", EXTENDED     , 8, op_JSR  },
   /* BE */    { "LDX ", EXTENDED     , 6, op_LDX  },
   /* BF */    { "STX ", EXTENDED     , 6, op_STX  },
   /* C0 */    { "SUBB", IMMEDIATE_8  , 2, op_SUBB },
   /* C1 */    { "CMPB", IMMEDIATE_8  , 2, op_CMPB },
   /* C2 */    { "SBCB", IMMEDIATE_8  , 2, op_SBCB },
   /* C3 */    { "ADDD", IMMEDIATE_16 , 4, op_ADDD },
   /* C4 */    { "ANDB", IMMEDIATE_8  , 2, op_ANDB },
   /* C5 */    { "BITB", IMMEDIATE_8  , 2, op_BITB },
   /* C6 */    { "LDB ", IMMEDIATE_8  , 2, op_LDB  },
   /* C7 */    { "??? ", ILLEGAL      , 2, op_XX   },
   /* C8 */    { "EORB", IMMEDIATE_8  , 2, op_EORB },
   /* C9 */    { "ADCB", IMMEDIATE_8  , 2, op_ADCB },
   /* CA */    { "ORB ", IMMEDIATE_8  , 2, op_ORB  },
   /* CB */    { "ADDB", IMMEDIATE_8  , 2, op_ADDB },
   /* CC */    { "LDD ", IMMEDIATE_16 , 3, op_LDD  },
   /* CD */    { "??? ", ILLEGAL      , 3, op_XX   },
   /* CE */    { "LDU ", IMMEDIATE_16 , 3, op_LDU  },
   /* CF */    { "??? ", ILLEGAL      , 3, op_XX   },
   /* D0 */    { "SUBB", DIRECT       , 4, op_SUBB },
   /* D1 */    { "CMPB", DIRECT       , 4, op_CMPB },
   /* D2 */    { "SBCB", DIRECT       , 4, op_SBCB },
   /* D3 */    { "ADDD", DIRECT       , 6, op_ADDD },
   /* D4 */    { "ANDB", DIRECT       , 4, op_ANDB },
   /* D5 */    { "BITB", DIRECT       , 4, op_BITB },
   /* D6 */    { "LDB ", DIRECT       , 4, op_LDB  },
   /* D7 */    { "STB ", DIRECT       , 4, op_STB  },
   /* D8 */    { "EORB", DIRECT       , 4, op_EORB },
   /* D9 */    { "ADCB", DIRECT       , 4, op_ADCB },
   /* DA */    { "ORB ", DIRECT       , 4, op_ORB  },
   /* DB */    { "ADDB", DIRECT       , 4, op_ADDB },
   /* DC */    { "LDD ", DIRECT       , 5, op_LDD  },
   /* DD */    { "STD ", DIRECT       , 5, op_STD  },
   /* DE */    { "LDU ", DIRECT       , 5, op_LDU  },
   /* DF */    { "STU ", DIRECT       , 5, op_STU  },
   /* E0 */    { "SUBB", INDEXED      , 4, op_SUBB },
   /* E1 */    { "CMPB", INDEXED      , 4, op_CMPB },
   /* E2 */    { "SBCB", INDEXED      , 4, op_SBCB },
   /* E3 */    { "ADDD", INDEXED      , 6, op_ADDD },
   /* E4 */    { "ANDB", INDEXED      , 4, op_ANDB },
   /* E5 */    { "BITB", INDEXED      , 4, op_BITB },
   /* E6 */    { "LDB ", INDEXED      , 4, op_LDB  },
   /* E7 */    { "STB ", INDEXED      , 4, op_STB  },
   /* E8 */    { "EORB", INDEXED      , 4, op_EORB },
   /* E9 */    { "ADCB", INDEXED      , 4, op_ADCB },
   /* EA */    { "ORB ", INDEXED      , 4, op_ORB  },
   /* EB */    { "ADDB", INDEXED      , 4, op_ADDB },
   /* EC */    { "LDD ", INDEXED      , 5, op_LDD  },
   /* ED */    { "STD ", INDEXED      , 5, op_STD  },
   /* EE */    { "LDU ", INDEXED      , 5, op_LDU  },
   /* EF */    { "STU ", INDEXED      , 5, op_STU  },
   /* F0 */    { "SUBB", EXTENDED     , 5, op_SUBB },
   /* F1 */    { "CMPB", EXTENDED     , 5, op_CMPB },
   /* F2 */    { "SBCB", EXTENDED     , 5, op_SBCB },
   /* F3 */    { "ADDD", EXTENDED     , 7, op_ADDD },
   /* F4 */    { "ANDB", EXTENDED     , 5, op_ANDB },
   /* F5 */    { "BITB", EXTENDED     , 5, op_BITB },
   /* F6 */    { "LDB ", EXTENDED     , 5, op_LDB  },
   /* F7 */    { "STB ", EXTENDED     , 5, op_STB  },
   /* F8 */    { "EORB", EXTENDED     , 5, op_EORB },
   /* F9 */    { "ADCB", EXTENDED     , 5, op_ADCB },
   /* FA */    { "ORB ", EXTENDED     , 5, op_ORB  },
   /* FB */    { "ADDB", EXTENDED     , 5, op_ADDB },
   /* FC */    { "LDD ", EXTENDED     , 6, op_LDD  },
   /* FD */    { "STD ", EXTENDED     , 6, op_STD  },
   /* FE */    { "LDU ", EXTENDED     , 6, op_LDU  },
   /* FF */    { "STU ", EXTENDED     , 6, op_STU  }
};

static InstrType instr_table_6809_map1[] = {
   /* 00 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 01 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 02 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 03 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 04 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 05 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 06 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 07 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 08 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 09 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 0A */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 0B */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 0C */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 0D */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 0E */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 0F */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 10 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 11 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 12 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 13 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 14 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 15 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 16 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 17 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 18 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 19 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 1A */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 1B */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 1C */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 1D */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 1E */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 1F */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 20 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 21 */    { "LBRN", RELATIVE_16  , 5, op_LBRN },
   /* 22 */    { "LBHI", RELATIVE_16  , 5, op_LBHI },
   /* 23 */    { "LBLS", RELATIVE_16  , 5, op_LBLS },
   /* 24 */    { "LBCC", RELATIVE_16  , 5, op_LBCC },
   /* 25 */    { "LBLO", RELATIVE_16  , 5, op_LBLO },
   /* 26 */    { "LBNE", RELATIVE_16  , 5, op_LBNE },
   /* 27 */    { "LBEQ", RELATIVE_16  , 5, op_LBEQ },
   /* 28 */    { "LBVC", RELATIVE_16  , 5, op_LBVC },
   /* 29 */    { "LBVS", RELATIVE_16  , 5, op_LBVS },
   /* 2A */    { "LBPL", RELATIVE_16  , 5, op_LBPL },
   /* 2B */    { "LBMI", RELATIVE_16  , 5, op_LBMI },
   /* 2C */    { "LBGE", RELATIVE_16  , 5, op_LBGE },
   /* 2D */    { "LBLT", RELATIVE_16  , 5, op_LBLT },
   /* 2E */    { "LBGT", RELATIVE_16  , 5, op_LBGT },
   /* 2F */    { "LBLE", RELATIVE_16  , 5, op_LBLE },
   /* 30 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 31 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 32 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 33 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 34 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 35 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 36 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 37 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 38 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 39 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 3A */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 3B */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 3C */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 3D */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 3E */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 3F */    { "SWI2", INHERENT     ,20, op_SWI2 },
   /* 40 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 41 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 42 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 43 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 44 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 45 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 46 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 47 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 48 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 49 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 4A */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 4B */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 4C */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 4D */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 4E */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 4F */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 50 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 51 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 52 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 53 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 54 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 55 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 56 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 57 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 58 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 59 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 5A */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 5B */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 5C */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 5D */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 5E */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 5F */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 60 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 61 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 62 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 63 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 64 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 65 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 66 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 67 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 68 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 69 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 6A */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 6B */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 6C */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 6D */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 6E */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 6F */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 70 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 71 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 72 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 73 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 74 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 75 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 76 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 77 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 78 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 79 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 7A */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 7B */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 7C */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 7D */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 7E */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 7F */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 80 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 81 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 82 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 83 */    { "CMPD", IMMEDIATE_16 , 5, op_CMPD },
   /* 84 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 85 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 86 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 87 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 88 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 89 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 8A */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 8B */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 8C */    { "CMPY", IMMEDIATE_16 , 5, op_CMPY },
   /* 8D */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 8E */    { "LDY ", IMMEDIATE_16 , 4, op_LDY  },
   /* 8F */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 90 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 91 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 92 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 93 */    { "CMPD", DIRECT       , 7, op_CMPD },
   /* 94 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 95 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 96 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 97 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 98 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 99 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 9A */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 9B */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 9C */    { "CMPY", DIRECT       , 7, op_CMPY },
   /* 9D */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 9E */    { "LDY ", DIRECT       , 6, op_LDY  },
   /* 9F */    { "STY ", DIRECT       , 6, op_STY  },
   /* A0 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* A1 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* A2 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* A3 */    { "CMPD", INDEXED      , 7, op_CMPD },
   /* A4 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* A5 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* A6 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* A7 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* A8 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* A9 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* AA */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* AB */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* AC */    { "CMPY", INDEXED      , 7, op_CMPY },
   /* AD */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* AE */    { "LDY ", INDEXED      , 6, op_LDY  },
   /* AF */    { "STY ", INDEXED      , 6, op_STY  },
   /* B0 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* B1 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* B2 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* B3 */    { "CMPD", EXTENDED     , 8, op_CMPD },
   /* B4 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* B5 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* B6 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* B7 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* B8 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* B9 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* BA */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* BB */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* BC */    { "CMPY", EXTENDED     , 8, op_CMPY },
   /* BD */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* BE */    { "LDY ", EXTENDED     , 7, op_LDY  },
   /* BF */    { "STY ", EXTENDED     , 7, op_STY  },
   /* C0 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* C1 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* C2 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* C3 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* C4 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* C5 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* C6 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* C7 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* C8 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* C9 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* CA */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* CB */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* CC */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* CD */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* CE */    { "LDS ", IMMEDIATE_16 , 4, op_LDS  },
   /* CF */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* D0 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* D1 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* D2 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* D3 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* D4 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* D5 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* D6 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* D7 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* D8 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* D9 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* DA */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* DB */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* DC */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* DD */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* DE */    { "LDS ", DIRECT       , 6, op_LDS  },
   /* DF */    { "STS ", DIRECT       , 6, op_STS  },
   /* E0 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* E1 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* E2 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* E3 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* E4 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* E5 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* E6 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* E7 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* E8 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* E9 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* EA */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* EB */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* EC */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* ED */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* EE */    { "LDS ", INDEXED      , 6, op_LDS  },
   /* EF */    { "STS ", INDEXED      , 6, op_STS  },
   /* F0 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* F1 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* F2 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* F3 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* F4 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* F5 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* F6 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* F7 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* F8 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* F9 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* FA */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* FB */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* FC */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* FD */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* FE */    { "LDS ", EXTENDED     , 7, op_LDS  },
   /* FF */    { "STS ", EXTENDED     , 7, op_STS  }
};

static InstrType instr_table_6809_map2[] = {
   /* 00 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 01 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 02 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 03 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 04 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 05 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 06 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 07 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 08 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 09 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 0A */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 0B */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 0C */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 0D */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 0E */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 0F */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 10 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 11 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 12 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 13 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 14 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 15 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 16 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 17 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 18 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 19 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 1A */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 1B */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 1C */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 1D */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 1E */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 1F */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 20 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 21 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 22 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 23 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 24 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 25 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 26 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 27 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 28 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 29 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 2A */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 2B */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 2C */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 2D */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 2E */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 2F */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 30 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 31 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 32 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 33 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 34 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 35 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 36 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 37 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 38 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 39 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 3A */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 3B */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 3C */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 3D */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 3E */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 3F */    { "SWI3", INHERENT     , 2, op_SWI30 },
   /* 40 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 41 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 42 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 43 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 44 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 45 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 46 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 47 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 48 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 49 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 4A */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 4B */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 4C */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 4D */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 4E */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 4F */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 50 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 51 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 52 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 53 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 54 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 55 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 56 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 57 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 58 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 59 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 5A */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 5B */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 5C */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 5D */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 5E */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 5F */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 60 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 61 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 62 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 63 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 64 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 65 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 66 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 67 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 68 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 69 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 6A */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 6B */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 6C */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 6D */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 6E */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 6F */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 70 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 71 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 72 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 73 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 74 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 75 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 76 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 77 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 78 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 79 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 7A */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 7B */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 7C */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 7D */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 7E */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 7F */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 80 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 81 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 82 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 83 */    { "CMPU", IMMEDIATE_16 , 5, op_CMPU },
   /* 84 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 85 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 86 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 87 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 88 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 89 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 8A */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 8B */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 8C */    { "CMPS", IMMEDIATE_16 , 5, op_CMPS },
   /* 8D */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 8E */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 8F */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 90 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 91 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 92 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 93 */    { "CMPU", DIRECT       , 7, op_CMPU },
   /* 94 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 95 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 96 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 97 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 98 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 99 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 9A */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 9B */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 9C */    { "CMPS", DIRECT       , 7, op_CMPS },
   /* 9D */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 9E */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* 9F */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* A0 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* A1 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* A2 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* A3 */    { "CMPU", INDEXED      , 7, op_CMPU },
   /* A4 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* A5 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* A6 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* A7 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* A8 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* A9 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* AA */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* AB */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* AC */    { "CMPS", INDEXED      , 7, op_CMPS },
   /* AD */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* AE */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* AF */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* B0 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* B1 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* B2 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* B3 */    { "CMPU", EXTENDED     , 8, op_CMPU },
   /* B4 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* B5 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* B6 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* B7 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* B8 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* B9 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* BA */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* BB */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* BC */    { "CMPS", EXTENDED     , 8, op_CMPS },
   /* BD */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* BE */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* BF */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* C0 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* C1 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* C2 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* C3 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* C4 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* C5 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* C6 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* C7 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* C8 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* C9 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* CA */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* CB */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* CC */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* CD */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* CE */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* CF */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* D0 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* D1 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* D2 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* D3 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* D4 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* D5 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* D6 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* D7 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* D8 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* D9 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* DA */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* DB */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* DC */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* DD */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* DE */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* DF */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* E0 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* E1 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* E2 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* E3 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* E4 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* E5 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* E6 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* E7 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* E8 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* E9 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* EA */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* EB */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* EC */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* ED */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* EE */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* EF */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* F0 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* F1 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* F2 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* F3 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* F4 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* F5 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* F6 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* F7 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* F8 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* F9 */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* FA */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* FB */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* FC */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* FD */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* FE */    { "??? ", ILLEGAL      , 1, op_XX   },
   /* FF */    { "??? ", ILLEGAL      , 1, op_XX   }
};
