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
   int oi;
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


#endif
