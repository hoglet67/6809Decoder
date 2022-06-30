#include <stdio.h>
#include <string.h>

#include "defs.h"
#include "dis_6809.h"
#include "types_6809.h"

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

static opcode_t *instr_table;

int cpu6309 = 0;

static char *strinsert(char *ptr, const char *str) {
   while (*str) {
      *ptr++ = *str++;
   }
   return ptr;
}


void dis_6809_init(cpu_t cpu_type, opcode_t *cpu_instr_table) {
   if (cpu_type == CPU_6309 || cpu_type == CPU_6309E) {
      regi4 = regi4_6309;
      cpu6309 = 1;
   } else {
      regi4 = regi4_6809;
      cpu6309 = 0;
   }
   instr_table = cpu_instr_table;
}


int dis_6809_disassemble(char *buffer, instruction_t *instruction) {
   int b0 = instruction->instr[0];
   int b1 = instruction->instr[1];
   int pb = 0;
   opcode_t *instr = get_instruction(instr_table, b0, b1);
   int mode = instr->mode;

   /// Output the mnemonic
   char *ptr = buffer;
   int len = strlen(instr->op->mnemonic);
   strcpy(ptr, instr->op->mnemonic);
   ptr += len;
   for (int i = len; i < 6; i++) {
      *ptr++ = ' ';
   }

   // Work out where in the instruction the operand is
   // [Prefix] Opcode [Imbyte] [ Postbyte] Op1 Op2
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
   // Handle the immediate operand AIM/EIM/OIM/TIM in as simple way as possible
   if (mode == DIRECTIM || mode == EXTENDEDIM || mode == INDEXEDIM) {
      *ptr++ = '#';
      *ptr++ = '$';
      write_hex2(ptr, instruction->instr[oi++]);
      ptr += 2;
      *ptr++ = ' ';
      // The changed the mode back to the base mode
      mode--;
   }

   if (mode == INDEXED || mode == DIRECTBIT || mode == REGISTER) {
      // Skip over the post byte
      pb = instruction->instr[oi++];
   }
   int op8 = instruction->instr[oi];
   int op16 = (instruction->instr[oi] << 8) + instruction->instr[oi + 1];

   // Output the operand
   switch (mode) {
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
         if (mode == RELATIVE_8) {
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
            if (cpu6309 && ((pb & 0x1f) == 0x0f || (pb & 0x1f) == 0x10)) {

               // Extra 6309 W indexed modes
               switch ((pb >> 5) & 3) {
               case 0:           /* ,W */
                  *ptr++ = ',';
                  *ptr++ = 'W';
                  *ptr++ = '+';
                  break;
               case 1:           /* n15,W */
                  write_hex4(ptr, op16);
                  ptr += 4;
                  *ptr++ = ',';
                  *ptr++ = 'W';
                  break;
               case 2:           /* ,W++ */
                  *ptr++ = ',';
                  *ptr++ = 'W';
                  *ptr++ = '+';
                  *ptr++ = '+';
                  break;
               case 3:           /* ,--W */
                  *ptr++ = ',';
                  *ptr++ = '-';
                  *ptr++ = '-';
                  *ptr++ = 'W';
                  break;
               }

            } else {

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
               case 7:                 /* E,R */
                  if (cpu6309) {
                     *ptr++ = 'E';
                     *ptr++ = ',';
                     *ptr++ = reg;
                  } else {
                     *ptr++ = '?';
                  }
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
               case 10:                /* F,R */
                  if (cpu6309) {
                     *ptr++ = 'F';
                     *ptr++ = ',';
                     *ptr++ = reg;
                  } else {
                     *ptr++ = '?';
                  }
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
               case 14:                /* W,R */
                  if (cpu6309) {
                     *ptr++ = 'W';
                     *ptr++ = ',';
                     *ptr++ = reg;
                  } else {
                     *ptr++ = '?';
                  }
                  break;
               case 15:                /* [n] */
                  *ptr++ = '$';
                  write_hex4(ptr, op16);
                  ptr += 4;
                  break;
               }
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
