#ifndef DEFS
#define DEFS

#include <inttypes.h>

typedef enum {
   MACHINE_DEFAULT,
   MACHINE_DRAGON32,
   MACHINE_POSITRON9000
} machine_t;

typedef enum {
   CPU_UNKNOWN,
   CPU_6809,
   CPU_6809E,
} cpu_t;

// Sample Queue Depth - needs to fit the longest instruction
#define DEPTH 32

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


typedef struct {
   int           pc;
   uint8_t       instr[8];
   uint8_t       prefix;
   uint8_t       opcode;
   uint8_t       postbyte;
   uint8_t       length;
} instruction_t;

void write_hex1(char *buffer, int value);
void write_hex2(char *buffer, int value);
void write_hex4(char *buffer, int value);
void write_hex6(char *buffer, int value);
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
   int vec_rst;
   int show_address;
   int show_hex;
   int show_instruction;
   int show_state;
   int show_cycles;
   int show_samplenums;
   int show_something;
   int byte;
   int debug;
   int skip;
   int mem_model;
   int trigger_start;
   int trigger_stop;
   int trigger_skipint;
   char *filename;
   int show_romno;
} arguments_t;

typedef struct {
   void (*init)(arguments_t *args);
   int (*match_reset)(sample_t *sample_q, int num_samples);
   int (*match_interrupt)(sample_t *sample_q, int num_samples);
   int (*count_cycles)(sample_t *sample_q);
   void (*reset)(sample_t *sample_q, int num_cycles, instruction_t *instruction);
   void (*interrupt)(sample_t *sample_q, int num_cycles, instruction_t *instruction);
   void (*emulate)(sample_t *sample_q, int num_cycles, instruction_t *instruction);
   int (*disassemble)(char *bp, instruction_t *instruction);
   int (*get_PC)();
   int (*read_memory)(int address);
   char *(*get_state)();
   int (*get_and_clear_fail)();
} cpu_emulator_t;

extern int failflag;

#endif
