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

// If the following instruction is store immediate
// then the flags are set as follows:
typedef enum {
   GRP_DEFAULT,    // N=1 Z=0
   GRP_N0_Z1,      // N=0 Z=1
   GRP_A_00,       // NZ based on A==00 test
   GRP_A_FF,       // NZ based on A==FF test
   GRP_A_01,       // NZ based on A==01 test
   GRP_B_01,       // NZ based on B==01 test
   GRP_R16_01,     // NZ based on <last 16 bit result>=01 test
   GRP_LEAU,       // NZ based on U
   GRP_LEAS,       // NZ based on S
   GRP_LEAX,       // NZ based on X
   GRP_LEAY,       // NZ based on Y
} storeimm_t;

typedef int operand_t;

typedef int ea_t;

typedef enum {
   SIZE_8    = 0,
   SIZE_16   = 1,
   SIZE_32   = 2
} opsize_t;

typedef struct {
   sample_t *sample;
   int num_samples;
   int oi;
   int num_cycles;
} sample_q_t;

typedef struct {
   const char *mnemonic;
   int (*emulate)(operand_t, ea_t, sample_q_t *);
   optype_t type;
   opsize_t size;
   storeimm_t storeimm;
} operation_t;

typedef struct {
   operation_t *op;
   addr_mode_t mode;
   int undocumented;
   int cycles;
   int cycles_native;
} opcode_t;


#endif
