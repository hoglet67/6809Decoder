#ifndef _INCLUDE_TYPES_6809_H
#define _INCLUDE_TYPES_6809_H

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
   DIRECTBIT,
   DIRECT,
   DIRECTIM,   // Must follow DIRECT
   EXTENDED,
   EXTENDEDIM, // Must follow EXTENDED
   INDEXED,
   INDEXEDIM,  // Must follow INDEXED
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
   sample_t *sample;
   int num_cycles;
} sample_q_t;

typedef struct {
   const char *mnemonic;
   int (*emulate)(operand_t, ea_t, sample_q_t *);
   optype_t type;
   opsize_t size;
} operation_t;

typedef struct {
   operation_t *op;
   addr_mode_t mode;
   int undocumented;
   int cycles;
   int cycles_native;
} opcode_t;

static inline opcode_t *get_instruction(opcode_t *instr_table, uint8_t b0, uint8_t b1) {
   if (b0 == 0x10 || b0 == 0x11) {
      return instr_table + 0x100 * (1 + (b0 & 1)) + b1;
   } else {
      return instr_table + b0;
   }
}

#endif
