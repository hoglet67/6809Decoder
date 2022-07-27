#ifndef DEFS
#define DEFS

#include <inttypes.h>

enum {
   FAIL_ADDR_INSTR = 0x00000001,
   FAIL_ADDR_PTR   = 0x00000002,
   FAIL_ADDR_DATA  = 0x00000004,
   FAIL_ADDR_STACK = 0x00000008,
   FAIL_RNW        = 0x00000010,
   FAIL_MEMORY     = 0x00000020,
   FAIL_PC         = 0x00000040,
   FAIL_ACCA       = 0x00000100,
   FAIL_ACCB       = 0x00000200,
   FAIL_ACCE       = 0x00000400,
   FAIL_ACCF       = 0x00000800,
   FAIL_X          = 0x00001000,
   FAIL_Y          = 0x00002000,
   FAIL_U          = 0x00004000,
   FAIL_S          = 0x00008000,
   FAIL_DP         = 0x00010000,
   FAIL_E          = 0x00020000,
   FAIL_F          = 0x00040000,
   FAIL_H          = 0x00080000,
   FAIL_I          = 0x00100000,
   FAIL_N          = 0x00200000,
   FAIL_Z          = 0x00400000,
   FAIL_V          = 0x00800000,
   FAIL_C          = 0x01000000,
   FAIL_RESULT     = 0x02000000,
   FAIL_VECTOR     = 0x04000000,
   FAIL_CYCLES     = 0x08000000,
   FAIL_UNDOC      = 0x10000000,
   FAIL_BADM       = 0x20000000
};

typedef enum {
   MACHINE_DEFAULT,
   MACHINE_DRAGON32,
   MACHINE_BEEB,
   MACHINE_POSITRON9000
} machine_t;

typedef enum {
   CPU_UNKNOWN,
   CPU_6809,
   CPU_6809E,
   CPU_6309,
   CPU_6309E,
} cpu_t;

// DIVQ is the longest instructions, at 34 cycles
#define LONGEST_INSTRUCTION 34

// Sample_type_t is an abstraction of both the 6502 SYNC and the 65816 VDA/VPA

typedef enum {
   NORMAL,
   LAST
} sample_type_t;

typedef struct {
   sample_type_t   type;
   uint32_t      sample_count;
   uint8_t       data;
   int8_t         rnw; // -1 indicates unknown
   int8_t         lic; // -1 indicates unknown
   int8_t          bs; // -1 indicates unknown
   int8_t          ba; // -1 indicates unknown
   int8_t        addr; // -1 indicates unknown
} sample_t;

// This is used to pa
typedef struct {
   int           pc;
   uint8_t       instr[8];
   uint8_t       length;
   int           rst_seen;
   int           intr_seen;
} instruction_t;

void write_hex1(char *buffer, int value);
void write_hex2(char *buffer, int value);
void write_hex4(char *buffer, int value);
void write_hex6(char *buffer, int value);
void write_hex8(char *buffer, uint32_t value);
int  write_s   (char *buffer, const char *s);

typedef struct {
   cpu_t cpu_type;
   machine_t machine;
   int idx_data;
   int idx_rnw;
   int idx_lic;
   int idx_ba;
   int idx_bs;
   int idx_addr;
   int idx_clke;
   int vec_rst;
   int show_address;
   int show_hex;
   int show_instruction;
   int show_state;
   int show_cycles;
   int show_samplenums;
   int show_something;
   uint32_t fail_mask;
   int fail_syncbug;
   int reg_s;
   int reg_u;
   int reg_pc;
   int reg_dp;
   int reg_nm;
   int reg_fm;
   int rom_latch;
   int byte;
   int debug;
   int skip;
   int skew;
   int mem_model;
   int trigger_start;
   int trigger_stop;
   int trigger_skipint;
   char *filename;
   int show_romno;
} arguments_t;

// Error return valyes from count_cycles

#define CYCLES_UNKNOWN   -1   // The cycle count could not be determined
#define CYCLES_TRUNCATED -2   // The final instruction was truncated

typedef struct {
   void (*init)(arguments_t *args);
   int (*emulate)(sample_t *sample_q, int num_samples, instruction_t *instruction);
   int (*disassemble)(char *bp, instruction_t *instruction);
   int (*get_PC)();
   int (*get_NM)();
   int (*read_memory)(int address);
   char *(*get_state)();
   uint32_t (*get_and_clear_fail)();
   int (*write_fail)(char *bp, uint32_t fail);
} cpu_emulator_t;

extern uint32_t failflag;

static inline void validate_address(sample_t *sample, int ea, int fail) {
   if (sample->addr >= 0 && sample->addr != (ea & 15)) {
      failflag |= fail;
   }
}

#endif
