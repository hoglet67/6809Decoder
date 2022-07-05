#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <argp.h>
#include <string.h>
#include <math.h>

#include "defs.h"
#include "em_6809.h"
#include "memory.h"

// Small skew buffer to allow the data bus samples to be taken early or late

#define SKEW_BUFFER_SIZE  32 // Must be a power of 2

#define MAX_SKEW_VALUE ((SKEW_BUFFER_SIZE / 2) - 1)

// Value use to indicate a pin (or register) has not been assigned by
// the user, so should take the default value.
#define UNSPECIFIED -2

// Value used to indicate a pin (or register) is undefined. For a pin,
// this means unconnected. For a register this means it will default
// to a value of undefined (?).
#define UNDEFINED -1

#define BUFSIZE 8192

uint8_t buffer8[BUFSIZE];

uint16_t buffer[BUFSIZE];

const char *machine_names[] = {
   "default",
   "dragon32",
   "beeb",
   "positron9000",
   0
};


static char disbuf[1024];

static cpu_emulator_t *em;


// This is a global, so it's visible to the emulator functions
arguments_t arguments;

// This is a global, so it's visible to the emulator functions
int triggered = 0;

// indicate state prediction failed
uint32_t failflag = 0;

// ====================================================================
// Argp processing
// ====================================================================

const char *argp_program_version = "decode6809 0.1";

const char *argp_program_bug_address = "<dave@hoglet.com>";

static char doc[] = "\n\
Decoder for 6809 logic analyzer capture files.\n\
\n\
FILENAME must be a binary capture file containing:\n\
- 16 bit samples (of the data bus and control signals), or\n\
-  8-bit samples (of the data bus), if the --byte option is present.\n\
\n\
If FILENAME is omitted, stdin is read instead.\n\
\n\
The default sample bit assignments for the 6809 signals are:\n\
 - data: bit  0 (assumes 8 consecutive bits)\n\
 -  rnw: bit  8\n\
 -  lic: bit  9\n\
 -   bs: bit 10\n\
 -   ba: bit 11\n\
 - addr: bit 12 (assumes 4 consecutive bits)\n\
\n\
To specify that an input is unconnected, include the option with an empty\n\
BITNUM. e.g. --lic=\n\
\n\
The capture file should contain one sample per falling edge of E.\n\
\n\
If lic is not available a heuristic based decoder is used.\n\
This works well, but can take several instructions to lock onto the\n\
instruction stream.\n\
\n\
If --debug=1 is specified, each instruction is preceeded by it\'s sample values.\n\
\n\
The --mem= option controls the memory access logging and modelling. The value\n\
is three hex nibbles: WRM, where W controls write logging, R controls read\n\
logging, and M controls modelling.\n\
Each of the three nibbles has the same semantics:\n\
 - bit 3 applies to stack accesses\n\
 - bit 2 applies to data accesses\n\
 - bit 1 applies to pointer accesses\n\
 - bit 0 applies to instruction accesses\n\
Examples:\n\
 --mem=00F models (and verifies) all accesses, but with minimal extra logging\n\
 --mem=F0F would additional log all writes\n\
\n";

static char args_doc[] = "[FILENAME]";

enum {
   GROUP_GENERAL = 1,
   GROUP_OUTPUT  = 2,
   GROUP_REGISTER = 3,
   GROUP_SIGDEFS = 4
};


enum {
   KEY_ADDRESS = 'a',
   KEY_BYTE = 'b',
   KEY_CPU = 'c',
   KEY_DEBUG = 'd',
   KEY_HEX = 'h',
   KEY_INSTR = 'i',
   KEY_MACHINE = 'm',
   KEY_QUIET = 'q',
   KEY_STATE = 's',
   KEY_TRIGGER = 't',
   KEY_CYCLES = 'y',
   KEY_SAMPLES = 'Y',
   KEY_VECRST = 1,
   KEY_MEM,
   KEY_REG_S,
   KEY_REG_U,
   KEY_REG_PC,
   KEY_REG_DP,
   KEY_REG_NM,
   KEY_REG_FM,
   KEY_SKIP,
   KEY_SKEW,
   KEY_DATA,
   KEY_RNW,
   KEY_LIC,
   KEY_BS,
   KEY_BA,
   KEY_ADDR,
   KEY_CLKE
};


typedef struct {
   char *cpu_name;
   cpu_t cpu_type;
} cpu_name_t;

static cpu_name_t cpu_names[] = {
   // 6809
   {"6809",       CPU_6809},
   {"6809E",      CPU_6809E},
   {"HD6809",     CPU_6809},
   {"HD6809E",    CPU_6809E},
   {"MC6809",     CPU_6809},
   {"MC6809E",    CPU_6809E},

   {"6309",       CPU_6309},
   {"6309E",      CPU_6309E},
   {"HD6309",     CPU_6309},
   {"HD6309E",    CPU_6309E},

   // Terminator
   {NULL, 0}
};

static struct argp_option options[] = {
   { 0, 0, 0, 0, "General options:", GROUP_GENERAL},

   { "vecrst",      KEY_VECRST,     "HEX", OPTION_ARG_OPTIONAL, "Reset vector, optionally preceeded by the first opcode (e.g. A9D9CD)",
                                                                                                                     GROUP_GENERAL},
   { "cpu",            KEY_CPU,     "CPU",                   0, "Sets CPU type (6809, 6809e)",                       GROUP_GENERAL},
   { "machine",    KEY_MACHINE, "MACHINE",                   0, "Enable machine (beeb,elk,master) defaults",         GROUP_GENERAL},
   { "byte",          KEY_BYTE,         0,                   0, "Enable byte-wide sample mode",                      GROUP_GENERAL},
   { "debug",        KEY_DEBUG,   "LEVEL",                   0, "Sets the debug level (0 or 1)",                     GROUP_GENERAL},
   { "trigger",    KEY_TRIGGER, "ADDRESS",                   0, "Trigger on address",                                GROUP_GENERAL},
   { "mem",            KEY_MEM,     "HEX", OPTION_ARG_OPTIONAL, "Memory modelling (see above)",                      GROUP_GENERAL},
   { "skip",          KEY_SKIP,     "HEX", OPTION_ARG_OPTIONAL, "Skip the first n samples",                          GROUP_GENERAL},
   { "skew",          KEY_SKEW,    "SKEW", OPTION_ARG_OPTIONAL, "Skew the data bus by +/- n samples",                GROUP_GENERAL},

   { 0, 0, 0, 0, "Register options:", GROUP_REGISTER},
   { "reg_s",        KEY_REG_S,     "HEX", OPTION_ARG_OPTIONAL, "Initial value of the S register",                   GROUP_REGISTER},
   { "reg_u",        KEY_REG_U,     "HEX", OPTION_ARG_OPTIONAL, "Initial value of the U register",                   GROUP_REGISTER},
   { "reg_pc",      KEY_REG_PC,     "HEX", OPTION_ARG_OPTIONAL, "Initial value of the PC register",                  GROUP_REGISTER},
   { "reg_dp",      KEY_REG_DP,     "HEX", OPTION_ARG_OPTIONAL, "Initial value of the DP register",                  GROUP_REGISTER},
   { "reg_nm",      KEY_REG_NM,     "HEX", OPTION_ARG_OPTIONAL, "Initial value of the NM flag (6309)",               GROUP_REGISTER},
   { "reg_fm",      KEY_REG_FM,     "HEX", OPTION_ARG_OPTIONAL, "Initial value of the FM flag (6309)",               GROUP_REGISTER},

   { 0, 0, 0, 0, "Output options:", GROUP_OUTPUT},

   { "quiet",        KEY_QUIET,         0,                   0, "Set all the output options to off",                 GROUP_OUTPUT},
   { "address",    KEY_ADDRESS,         0,                   0, "Show address of instruction",                       GROUP_OUTPUT},
   { "hex",            KEY_HEX,         0,                   0, "Show hex bytes of instruction",                     GROUP_OUTPUT},
   { "instruction",  KEY_INSTR,         0,                   0, "Show instruction disassembly",                      GROUP_OUTPUT},
   { "state",        KEY_STATE,         0,                   0, "Show register/flag state",                          GROUP_OUTPUT},
   { "cycles",      KEY_CYCLES,         0,                   0, "Show instruction cycles",                           GROUP_OUTPUT},
   { "samplenum",  KEY_SAMPLES,         0,                   0, "Show bus cycle numbers",                            GROUP_OUTPUT},

   { 0, 0, 0, 0, "Signal defintion options:", GROUP_SIGDEFS},

   { "data",          KEY_DATA, "BITNUM",                   0, "Bit number for data (default  0)",                   GROUP_SIGDEFS},
   { "rnw",            KEY_RNW, "BITNUM", OPTION_ARG_OPTIONAL, "Bit number for rnw  (default  8)",                   GROUP_SIGDEFS},
   { "lic",            KEY_LIC, "BITNUM", OPTION_ARG_OPTIONAL, "Bit number for lic  (default  9)",                   GROUP_SIGDEFS},
   { "bs",              KEY_BS, "BITNUM", OPTION_ARG_OPTIONAL, "Bit number for bs   (default 10)",                   GROUP_SIGDEFS},
   { "ba",              KEY_BA, "BITNUM", OPTION_ARG_OPTIONAL, "Bit number for ba   (default 11)",                   GROUP_SIGDEFS},
   { "addr",          KEY_ADDR, "BITNUM", OPTION_ARG_OPTIONAL, "Bit number for addr (default 12)",                   GROUP_SIGDEFS},
   { "clke",          KEY_CLKE, "BITNUM", OPTION_ARG_OPTIONAL, "Bit number for clke (no default)",                   GROUP_SIGDEFS},

   { 0 }
};

static error_t parse_opt(int key, char *arg, struct argp_state *state) {
   int i;
   arguments_t *arguments = state->input;

   switch (key) {
   case KEY_DATA:
      arguments->idx_data = atoi(arg);
      break;
   case KEY_RNW:
      if (arg && strlen(arg) > 0) {
         arguments->idx_rnw = atoi(arg);
      } else {
         arguments->idx_rnw = UNDEFINED;
      }
      break;
   case KEY_LIC:
      if (arg && strlen(arg) > 0) {
         arguments->idx_lic = atoi(arg);
      } else {
         arguments->idx_lic = UNDEFINED;
      }
      break;
   case KEY_BS:
      if (arg && strlen(arg) > 0) {
         arguments->idx_bs = atoi(arg);
      } else {
         arguments->idx_bs = UNDEFINED;
      }
      break;
   case KEY_BA:
      if (arg && strlen(arg) > 0) {
         arguments->idx_ba = atoi(arg);
      } else {
         arguments->idx_ba = UNDEFINED;
      }
      break;
   case KEY_ADDR:
      if (arg && strlen(arg) > 0) {
         arguments->idx_addr = atoi(arg);
      } else {
         arguments->idx_addr = UNDEFINED;
      }
      break;
   case KEY_CLKE:
      if (arg && strlen(arg) > 0) {
         arguments->idx_clke = atoi(arg);
      } else {
         arguments->idx_clke = UNDEFINED;
      }
      break;
   case KEY_VECRST:
      if (arg && strlen(arg) > 0) {
         arguments->vec_rst = strtol(arg, (char **)NULL, 16);
      } else {
         arguments->vec_rst = UNDEFINED;
      }
      break;
   case KEY_SKIP:
      if (arg && strlen(arg) > 0) {
         arguments->skip = strtol(arg, (char **)NULL, 16);
      } else {
         arguments->skip = 0;
      }
      break;
   case KEY_SKEW:
      if (arg && strlen(arg) > 0) {
         arguments->skew = strtol(arg, (char **)NULL, 10);
         if (arguments->skew < -MAX_SKEW_VALUE || arguments->skew > MAX_SKEW_VALUE) {
            argp_error(state, "specified skew exceeds skew buffer size");
         }
      } else {
         arguments->skew = 0;
      }
      break;
   case KEY_MEM:
      if (arg && strlen(arg) > 0) {
         arguments->mem_model = strtol(arg, (char **)NULL, 16);
      } else {
         arguments->mem_model = 0;
      }
      break;
   case KEY_REG_S:
      if (arg && strlen(arg) > 0) {
         arguments->reg_s = strtol(arg, (char **)NULL, 16);
      } else {
         arguments->reg_s = UNDEFINED;
      }
      break;
   case KEY_REG_U:
      if (arg && strlen(arg) > 0) {
         arguments->reg_u = strtol(arg, (char **)NULL, 16);
      } else {
         arguments->reg_u = UNDEFINED;
      }
      break;
   case KEY_REG_PC:
      if (arg && strlen(arg) > 0) {
         arguments->reg_pc = strtol(arg, (char **)NULL, 16);
      } else {
         arguments->reg_pc = UNDEFINED;
      }
      break;
   case KEY_REG_DP:
      if (arg && strlen(arg) > 0) {
         arguments->reg_dp = strtol(arg, (char **)NULL, 16);
      } else {
         arguments->reg_dp = UNDEFINED;
      }
      break;
   case KEY_REG_NM:
      if (arg && strlen(arg) > 0) {
         arguments->reg_nm = strtol(arg, (char **)NULL, 16);
      } else {
         arguments->reg_nm = UNDEFINED;
      }
      break;
   case KEY_REG_FM:
      if (arg && strlen(arg) > 0) {
         arguments->reg_fm = strtol(arg, (char **)NULL, 16);
      } else {
         arguments->reg_fm = UNDEFINED;
      }
      break;
   case KEY_CPU:
      if (arg && strlen(arg) > 0) {
         i = 0;
         while (cpu_names[i].cpu_name) {
            if (strcasecmp(arg, cpu_names[i].cpu_name) == 0) {
               arguments->cpu_type = cpu_names[i].cpu_type;
               return 0;
            }
            i++;
         }
      }
      argp_error(state, "unsupported cpu type: %s", arg);
      break;
   case KEY_MACHINE:
      i = 0;
      while (machine_names[i]) {
         if (strcasecmp(arg, machine_names[i]) == 0) {
            arguments->machine = i;
            return 0;
         }
         i++;
      }
      argp_error(state, "unsupported machine type");
      break;
   case KEY_DEBUG:
      arguments->debug = atoi(arg);
      break;
   case KEY_BYTE:
      arguments->byte = 1;
      break;
   case KEY_QUIET:
      arguments->show_address = 0;
      arguments->show_hex = 0;
      arguments->show_instruction = 0;
      arguments->show_state = 0;
      arguments->show_cycles = 0;
      arguments->show_samplenums = 0;
      break;
   case KEY_ADDRESS:
      arguments->show_address = 1;
      break;
   case KEY_HEX:
      arguments->show_hex = 1;
      break;
   case KEY_INSTR:
      arguments->show_instruction = 1;
      break;
   case KEY_STATE:
      arguments->show_state = 1;
      break;
   case KEY_CYCLES:
      arguments->show_cycles = 1;
      break;
   case KEY_SAMPLES:
      arguments->show_samplenums = 1;
      break;
   case KEY_TRIGGER:
      if (arg && strlen(arg) > 0) {
         char *start   = strtok(arg, ",");
         char *stop    = strtok(NULL, ",");
         char *skipint = strtok(NULL, ",");
         if (start && strlen(start) > 0) {
            arguments->trigger_start = strtol(start, (char **)NULL, 16);
         }
         if (stop && strlen(stop) > 0) {
            arguments->trigger_stop = strtol(stop, (char **)NULL, 16);
         }
         if (skipint && strlen(skipint) > 0) {
            arguments->trigger_skipint = atoi(skipint);
         }
      }
      break;
   case ARGP_KEY_ARG:
      arguments->filename = arg;
      break;
   case ARGP_KEY_END:
      if (state->arg_num > 1) {
         argp_error(state, "multiple capture file arguments");
      }
      break;
   default:
      return ARGP_ERR_UNKNOWN;
   }
   return 0;
}

static struct argp argp = { options, parse_opt, args_doc, doc, 0, 0, 0 };



// ====================================================================
// Analyze a complete instruction
// ====================================================================


static void dump_samples(sample_t *sample_q, int n) {
      for (int i = 0; i < n; i++) {
         sample_t *sample = sample_q + i;
         printf("%8x %2d %02x ", sample->sample_count, i, sample->data);
         putchar(' ');
         putchar(sample->rnw >= 0 ? '0' + sample->rnw : '?');
         putchar(' ');
         putchar(sample->lic >= 0 ? '0' + sample->lic : '?');
         putchar(' ');
         putchar(sample->bs >= 0 ? '0' + sample->bs : '?');
         putchar(sample->ba >= 0 ? '0' + sample->ba : '?');
         putchar(' ');
         putchar(sample->addr >= 0 ? sample->addr + (sample->addr < 10 ? '0' : 'A' - 10) : '?');
         putchar('\n');
      }
}

void write_hex1(char *buffer, int value) {
   *buffer = value + (value < 10 ? '0' : 'A' - 10);
}

void write_hex2(char *buffer, int value) {
   write_hex1(buffer++, (value >> 4) & 15);
   write_hex1(buffer++, (value >> 0) & 15);
}

void write_hex4(char *buffer, int value) {
   write_hex1(buffer++, (value >> 12) & 15);
   write_hex1(buffer++, (value >> 8) & 15);
   write_hex1(buffer++, (value >> 4) & 15);
   write_hex1(buffer++, (value >> 0) & 15);
}

void write_hex6(char *buffer, int value) {
   write_hex1(buffer++, (value >> 20) & 15);
   write_hex1(buffer++, (value >> 16) & 15);
   write_hex1(buffer++, (value >> 12) & 15);
   write_hex1(buffer++, (value >> 8) & 15);
   write_hex1(buffer++, (value >> 4) & 15);
   write_hex1(buffer++, (value >> 0) & 15);
}

void write_hex8(char *buffer, uint32_t value) {
   write_hex1(buffer++, (value >> 28) & 15);
   write_hex1(buffer++, (value >> 24) & 15);
   write_hex1(buffer++, (value >> 20) & 15);
   write_hex1(buffer++, (value >> 16) & 15);
   write_hex1(buffer++, (value >> 12) & 15);
   write_hex1(buffer++, (value >> 8) & 15);
   write_hex1(buffer++, (value >> 4) & 15);
   write_hex1(buffer++, (value >> 0) & 15);
}

int write_s(char *buffer, const char *s) {
   int i = 0;
   while (*s) {
      *buffer++ = *s++;
      i++;
   }
   return i;
}


static int exec_instruction(sample_t *sample_q, int num_samples, instruction_t *instruction) {
   int num_cycles;
   int window = LONGEST_INSTRUCTION;
   instruction->intr_seen = 0;
   instruction->rst_seen = em->match_reset(sample_q, window);
   if (instruction->rst_seen) {
      num_cycles = instruction->rst_seen;
   } else {
      instruction->intr_seen = em->match_interrupt(sample_q, window);
      if (instruction->intr_seen) {
         num_cycles = instruction->intr_seen;
      } else {
         num_cycles = em->count_cycles(sample_q, num_samples);
      }
   }

   // Two cases where num_cycles < 0:
   //    in non-LIC mode, where the instruction length can't be determined from the know CPU state
   //    if the final instruction is truncated

   if (num_cycles >= 0) {
      if (instruction->rst_seen) {
         // Handle a reset
         em->reset(sample_q, num_cycles, instruction);
      } else if (instruction->intr_seen) {
         // Handle an interrupt
         em->interrupt(sample_q, num_cycles, instruction);
      } else {
         // Handle a normal instruction
         num_cycles = em->emulate(sample_q, num_cycles, instruction);
      }
   }

   return num_cycles;
}


static int analyze_instruction(sample_t *sample_q, int num_samples) {
   static int total_cycles = 0;
   static int interrupt_depth = 0;
   static int skipping_interrupted = 0;

   int oldpc = em->get_PC();

   instruction_t instruction;

   int num_cycles = exec_instruction(sample_q, num_samples, &instruction);

   if (num_cycles == CYCLES_TRUNCATED) {
      // Silently consume all remaining samples
      return num_samples;
   } else if (num_cycles == CYCLES_UNKNOWN) {
      // Silently slip one cycle
      return 1;
   }

   // Sanity check the pc prediction has not gone awry
   // (e.g. in JSR the emulation can use the stacked PC)

   int pc = instruction.pc;

   if (pc >= 0) {
      if (oldpc >= 0 && oldpc != pc) {
         failflag |= FAIL_PC;
         printf("pc: prediction failed at %04X old pc was %04X\n", pc, oldpc);
      }
   }

   if (pc >= 0 && pc == arguments.trigger_start) {
      triggered = 1;
      printf("start trigger hit at cycle %d\n", total_cycles);
      memory_set_rd_logging((arguments.mem_model >> 4) & 0x0f);
      memory_set_wr_logging((arguments.mem_model >> 8) & 0x0f);
   }
   if (triggered && (arguments.debug & 1)) {
      dump_samples(sample_q, num_cycles);
   }

   // Exclude interrupts from profiling
   if (arguments.trigger_skipint && pc >= 0) {
      if (interrupt_depth == 0) {
         skipping_interrupted = 0;
      }
      if (instruction.intr_seen) {
         interrupt_depth++;
         skipping_interrupted = 1;
      } else if (interrupt_depth > 0 && instruction.instr[0] == 0x3b) {
         // RTI seen
         interrupt_depth--;
      }
   }

   int fail = em->get_and_clear_fail();

   // Try to minimise the calls to printf as these are quite expensive

   char *bp = disbuf;

   if ((fail | arguments.show_something) && triggered && !skipping_interrupted) {
      int numchars = 0;
      // Show cumulative sample number
      if (arguments.show_samplenums) {
         bp += sprintf(bp, "%8d", sample_q->sample_count);
         *bp++ = ' ';
         *bp++ = ':';
         *bp++ = ' ';
      }
      // Show address
      if (fail || arguments.show_address) {
         if (pc < 0) {
            *bp++ = '?';
            *bp++ = '?';
            *bp++ = '?';
            *bp++ = '?';
         } else {
            write_hex4(bp, pc);
            bp += 4;
         }
         *bp++ = ' ';
         *bp++ = ':';
         *bp++ = ' ';
      }
      // Show hex bytes
      if (fail || arguments.show_hex) {
         for (int i = 0; i < instruction.length; i++) {
            write_hex2(bp, instruction.instr[i]);
            bp += 2;
            *bp++ = ' ';
         }
         for (int i = 0; i < 3 * (5 - instruction.length); i++) {
            *bp++ = ' ';
         }
         *bp++ = ':';
         *bp++ = ' ';
      }

      // Show instruction disassembly
      if (fail || arguments.show_something) {
         if (instruction.rst_seen) {
            numchars = write_s(bp, "RESET !!");
         } else if (instruction.intr_seen) {
            numchars = write_s(bp, "INTERRUPT !!");
         } else {
            numchars = em->disassemble(bp, &instruction);
         }
         bp += numchars;
      }

      // Pad if there is more to come
      if (fail || arguments.show_cycles || arguments.show_state) {
         while (numchars++ < 20) {
            *bp++ = ' ';
         }
      }
      // Show cycles (don't include with fail as it is inconsistent depending on whether rdy is present)
      if (arguments.show_cycles) {
         *bp++ = ' ';
         *bp++ = ':';
         bp += sprintf(bp, "%4d", num_cycles);
      }
      // Show register state
      if (fail || arguments.show_state) {
         *bp++ = ' ';
         *bp++ = ':';
         *bp++ = ' ';
         bp = em->get_state(bp);
      }
      // Show any errors
      if (fail) {
         bp += em->write_fail(bp, fail);
      }
      // End the line
      *bp++ = 0;
      puts(disbuf);

   }

   if (pc >= 0 && pc == arguments.trigger_stop) {
      triggered = 0;
      printf("stop trigger hit at cycle %d\n", total_cycles);
      memory_set_rd_logging(0);
      memory_set_wr_logging(0);
   }

   total_cycles += num_cycles;
   return num_cycles;
}

// ====================================================================
// Queue a large number of samples so the decoders can lookahead
// ====================================================================

// BLOCK is guaranteed to be larger than the longest running instuction
// TODO: apart from SYNC or CWAI

#define BLOCK (256*1024)

#define SYNC_RANGE 100
#define SYNC_WINDOW 1000

// Queue three blocks
static sample_t sample_q[BLOCK * 3];

void queue_sample(sample_t *sample) {
   static int synced = 0;
   static sample_t *sample_wr = sample_q;
   static sample_t *sample_rd = sample_q;


   // At the end of the stream, allow the buffered samples to drain
   if (sample->type == LAST) {
      // Drain the queue when the LAST marker is seen
      while (sample_rd < sample_wr) {
         sample_rd += analyze_instruction(sample_rd, sample_wr - sample_rd);
      }
      return;
   } else {
      // Make a copy of the sample structure
      *sample_wr++ = *sample;
   }


   // TODO: Handle RESET

   // Try to synchronize to the instruction
   if (!synced) {
      if (sample_wr < sample_q + BLOCK) {
         return;
      }
      if (sample_rd->lic >= 0) {
         // LIC is available, so step forward to the first sample with LIC == 1
         while (!sample_rd->lic) {
            sample_rd++;
         }
         // In emulation mode, LIC is on the last intruction cycle, so skip forward one more
         if (em->get_NM() != 1) {
            sample_rd++;
         }
      } else {
         // LIC is not available, so use heuristics

         // TODO: Move into a separe function
         // TODO: Also detect native mode

         // Preserve the modelling state
         int mem_modelling  = memory_get_modelling();
         int mem_rd_logging = memory_get_rd_logging();
         int mem_wr_logging = memory_get_wr_logging();
         memory_destroy();
         sample_t *sample_best = NULL;
         int error_best = INT_MAX;
         // This should make the LIC and non-LIC trace start at the same instruction
         if (em->get_NM() != 1) {
            sample_rd++;
         }
         for (int i = 0; i < SYNC_RANGE && error_best > 0; i++) {
            sample_t *sample_tmp = sample_rd;
            int error_count = 0;

            // Re-initialize the emulator
            em->init(&arguments);
            memory_init(0x10000, arguments.machine);
            memory_set_modelling(0);
            memory_set_rd_logging(0);
            memory_set_wr_logging(0);
            while (sample_tmp < sample_q + SYNC_WINDOW ) {
               instruction_t instruction;
               failflag = 0;
               int num_cycles = exec_instruction(sample_tmp, BLOCK - (sample_tmp - sample_q), &instruction);
               if (num_cycles == CYCLES_TRUNCATED) {
                  break;
               } else if (num_cycles == CYCLES_UNKNOWN) {
                  num_cycles = 1;
               }
               sample_tmp += num_cycles;
               if (failflag) {
                  error_count++;
               }
            }
            memory_destroy();

            fprintf(stderr, "Offset %3d had %d errors\n", i, error_count);
            if (error_count < error_best) {
               sample_best = sample_rd;
               error_best = error_count;
            }
            sample_rd++;
         }
         fprintf(stderr, "Best is %ld\n", sample_best - sample_q);
         sample_rd = sample_best;
         // Initialize for real
         em->init(&arguments);
         memory_init(0x10000, arguments.machine);
         memory_set_modelling(mem_modelling);
         memory_set_rd_logging(mem_rd_logging);
         memory_set_wr_logging(mem_wr_logging);
      }
      synced = 1;
   }


   // Sample_q is NOT a circular buffer!
   //
   // When we have two full blocks, we can start to consume the first. Once the first is
   // consumed, we can move everything back in the block.
   if (sample_wr > sample_q + 2 * BLOCK) {
      while (sample_rd < sample_q + BLOCK) {
         sample_rd += analyze_instruction(sample_rd, sample_wr - sample_rd);
      }
      // The first block has been processed, so move everything down a block
      //printf("Block processed\n");
      //printf("  sample_wr = %ld\n", sample_wr - sample_q);
      //printf("  sample_rd = %ld\n", sample_rd - sample_q);
      memmove(sample_q, sample_q + BLOCK, sizeof(sample_t) * (sample_wr - sample_q - BLOCK));
      sample_rd -= BLOCK;
      sample_wr -= BLOCK;
      //printf("Block consumed\n");
      //printf("  sample_wr = %ld\n", sample_wr - sample_q);
      //printf("  sample_rd = %ld\n", sample_rd - sample_q);
   }
}

// ====================================================================
// Input file processing and bus cycle extraction
// ====================================================================

void decode(FILE *stream) {

   // Pin mappings into the 16 bit words
   int idx_data  = arguments.idx_data;
   int idx_rnw   = arguments.idx_rnw;
   int idx_lic   = arguments.idx_lic;
   int idx_bs    = arguments.idx_bs;
   int idx_ba    = arguments.idx_ba;
   int idx_addr  = arguments.idx_addr;
   int idx_clke  = arguments.idx_clke;

   // The structured bus sample we will pass on to the next level of processing
   sample_t s;

   // Skip the start of the file, if required
   if (arguments.skip) {
      fseek(stream, arguments.skip * (arguments.byte ? 1 : 2), SEEK_SET);
   }

   // Common to all sampling modes
   s.type = NORMAL;
   s.sample_count = 0;
   s.rnw  = -1;
   s.lic  = -1;
   s.bs   = -1;
   s.ba   = -1;
   s.addr = -1;

   if (arguments.byte) {

      // ------------------------------------------------------------
      // Synchronous byte sampling mode
      // ------------------------------------------------------------

      // In byte mode we have only data bus samples, nothing else so we must
      // use the lic-less decoder. All the control signals should be marked
      // as disconnected, by being set to -1.

      // Read the capture file, and queue structured sampled for the decoder
      int num;
      while ((num = fread(buffer8, sizeof(uint8_t), BUFSIZE, stream)) > 0) {
         uint8_t *sampleptr = &buffer8[0];
         while (num-- > 0) {
            s.data = *sampleptr++;
            queue_sample(&s);
            s.sample_count++;
         }
      }

   } else if (idx_clke < 0 ) {

      // ------------------------------------------------------------
      // Synchronous word sampling mode
      // ------------------------------------------------------------

      // In word sampling mode we have data bus samples, plus
      // optionally rnw, lic, bs, ba and addr.

      // In synchronous word sampling mode clke is not connected, and
      // it's assumed that each sample represents a seperate bus
      // cycle.

      // Read the capture file, and queue structured sampled for the decoder
      int num;
      while ((num = fread(buffer, sizeof(uint16_t), BUFSIZE, stream)) > 0) {
         uint16_t *sampleptr = &buffer[0];
         while (num-- > 0) {
            uint16_t sample = *sampleptr++;
            if (idx_rnw >= 0) {
               s.rnw = (sample >> idx_rnw ) & 1;
            }
            if (idx_lic >= 0) {
               s.lic = (sample >> idx_lic) & 1;
            }
            if (idx_bs >= 0) {
               s.bs = (sample >> idx_bs) & 1;
            }
            if (idx_ba >= 0) {
               s.ba = (sample >> idx_ba) & 1;
            }
            if (idx_addr >= 0) {
               s.addr = (sample >> idx_addr) & 15;
            }
            s.data = (sample >> idx_data) & 255;
            queue_sample(&s);
            s.sample_count++;
         }
      }

   } else {

      // ------------------------------------------------------------
      // Asynchronous word sampling mode
      // ------------------------------------------------------------

      // In word sampling mode we have data bus samples, plus
      // optionally rnw, lic, bs, ba and addr.

      // In asynchronous word sampling mode clke is connected, and
      // the capture file contans multple samples per bus cycle.

      // The previous value of clke, to detect the rising/falling edge
      int last_clke = -1;

      // A small circular buffer for skewing the sampling of the data bus
      uint16_t skew_buffer  [SKEW_BUFFER_SIZE];
      int wr_index;
      int rd_index;
      int data_rd_index;
      // Minimize the amount of buffering to avoid unnecessary garbage
      if (arguments.skew < 0) {
         // Data sample taken before clock edge
         wr_index = -arguments.skew;
         rd_index = -arguments.skew;
         data_rd_index = 0;
      } else {
         // Data sample taken after clock edge
         wr_index = arguments.skew;
         rd_index = 0;
         data_rd_index = arguments.skew;
      }

      // Clear the buffer, so the first few samples are ignored
      for (int i = 0; i < SKEW_BUFFER_SIZE; i++) {
         skew_buffer[i] = 0;
      }

      // Read the capture file, and queue structured sampled for the decoder
      int num;
      while ((num = fread(buffer, sizeof(uint16_t), BUFSIZE, stream)) > 0) {
         uint16_t *sampleptr = &buffer[0];
         while (num-- > 0) {

            skew_buffer[wr_index] = *sampleptr++;
            uint16_t sample       = skew_buffer[rd_index];
            uint16_t data_sample  = skew_buffer[data_rd_index];

            // Only act on edges of clke
            int pin_clke = (sample >> idx_clke) & 1;
            if (pin_clke != last_clke) {
               last_clke = pin_clke;
               if (pin_clke) {
                  // Sample control signals after rising edge of CLKE
                  if (idx_rnw >= 0) {
                     s.rnw = (sample >> idx_rnw ) & 1;
                  }
                  if (idx_lic >= 0) {
                     s.lic = (sample >> idx_lic) & 1;
                  }
                  if (idx_bs >= 0) {
                     s.bs = (sample >> idx_bs) & 1;
                  }
                  if (idx_ba >= 0) {
                     s.ba = (sample >> idx_ba) & 1;
                  }
                  if (idx_addr >= 0) {
                     s.addr = (sample >> idx_addr) & 15;
                  }
               } else {
                  // Sample the data skewed (--skew=) relative to the falling edge of CLKE
                  s.data = (data_sample >> idx_data) & 255;
                  queue_sample(&s);
                  s.sample_count++;
               }
            }
            // Increment the circular buffer pointers in lock-step to keey the skew constant
            wr_index      = (wr_index      + 1) & (SKEW_BUFFER_SIZE - 1);
            rd_index      = (rd_index      + 1) & (SKEW_BUFFER_SIZE - 1);
            data_rd_index = (data_rd_index + 1) & (SKEW_BUFFER_SIZE - 1);
         }
      }
   }

   // Flush the sample queue
   s.type = LAST;
   queue_sample(&s);
}


// ====================================================================
// Main program entry point
// ====================================================================

int main(int argc, char *argv[]) {
   // General options
   arguments.cpu_type         = CPU_UNKNOWN;
   arguments.machine          = MACHINE_DEFAULT;
   arguments.vec_rst          = UNSPECIFIED;
   arguments.byte             = 0;
   arguments.debug            = 0;
   arguments.mem_model        = 0;
   arguments.skip             = 0;
   arguments.skew             = 0;
   arguments.trigger_start    = UNSPECIFIED;
   arguments.trigger_stop     = UNSPECIFIED;
   arguments.trigger_skipint  = 0;
   arguments.filename         = NULL;

   // Register options
   arguments.reg_s            = UNSPECIFIED;
   arguments.reg_u            = UNSPECIFIED;
   arguments.reg_pc           = UNSPECIFIED;
   arguments.reg_dp           = UNSPECIFIED;
   arguments.reg_nm           = UNSPECIFIED;
   arguments.reg_fm           = UNSPECIFIED;

   // Output options
   arguments.show_address     = 1;
   arguments.show_hex         = 0;
   arguments.show_instruction = 1;
   arguments.show_state       = 0;
   arguments.show_cycles      = 0;
   arguments.show_samplenums  = 0;

   // Signal definition options
   arguments.idx_data         = UNSPECIFIED;
   arguments.idx_rnw          = UNSPECIFIED;
   arguments.idx_lic          = UNSPECIFIED;
   arguments.idx_bs           = UNSPECIFIED;
   arguments.idx_ba           = UNSPECIFIED;
   arguments.idx_addr         = UNSPECIFIED;
   arguments.idx_clke         = UNSPECIFIED;

   argp_parse(&argp, argc, argv, 0, 0, &arguments);

   if (arguments.trigger_start < 0) {
      triggered = 1;
   }

   arguments.show_something = arguments.show_samplenums | arguments.show_address | arguments.show_hex | arguments.show_instruction | arguments.show_state | arguments.show_cycles ;

   // Normally the data file should be 16 bit samples. In byte mode
   // the data file is 8 bit samples, and all the control signals are
   // assumed to be don't care.
   if (arguments.byte) {
      if (arguments.idx_rnw != UNSPECIFIED) {
         fprintf(stderr, "--rnw is incompatible with byte mode\n");
         return 1;
      }
      if (arguments.idx_lic != UNSPECIFIED) {
         fprintf(stderr, "--lic is incompatible with byte mode\n");
         return 1;
      }
      if (arguments.idx_bs != UNSPECIFIED) {
         fprintf(stderr, "--bs is incompatible with byte mode\n");
         return 1;
      }
      if (arguments.idx_ba != UNSPECIFIED) {
         fprintf(stderr, "--ba is incompatible with byte mode\n");
         return 1;
      }
      if (arguments.idx_addr != UNSPECIFIED) {
         fprintf(stderr, "--addr is incompatible with byte mode\n");
         return 1;
      }
   }

   // Apply Machine specific defaults
   if (arguments.vec_rst == UNSPECIFIED) {
      switch (arguments.machine) {
      case MACHINE_DRAGON32:
         arguments.vec_rst = 0xB4B3;
         break;
      default:
         arguments.vec_rst = 0xFFFF;
      }
   }

   // Default CPU from machine type
   if (arguments.cpu_type == CPU_UNKNOWN) {
      switch (arguments.machine) {
      case MACHINE_POSITRON9000:
         arguments.cpu_type = CPU_6809;
         break;
      case MACHINE_BEEB:
      case MACHINE_DRAGON32:
         arguments.cpu_type = CPU_6809E;
         break;
      default:
         arguments.cpu_type = CPU_6809E;
         break;
      }
   }

   if (arguments.cpu_type == CPU_6809 || arguments.cpu_type == CPU_6309) {
      if (arguments.idx_lic != UNSPECIFIED) {
         fprintf(stderr, "--lic= is incompatible with the 6809 CPU as it doesn't have this pin\n");
         return 1;
      }
   }

   if (arguments.cpu_type != CPU_6309 && arguments.cpu_type != CPU_6309E) {
      if (arguments.reg_nm != UNSPECIFIED) {
         fprintf(stderr, "--reg_nm= can only be used when the CPU is a 6309/6309E\n");
         return 1;
      }
      if (arguments.reg_fm != UNSPECIFIED) {
         fprintf(stderr, "--reg_fm= can only be used when the CPU is a 6309/6309E\n");
         return 1;
      }
   }


   // Initialize memory modelling
   // (em->init actually mallocs the memory)
   memory_init(0x10000, arguments.machine);

   // Turn on memory write logging if show rom bank option (-r) is selected
   if (arguments.show_romno) {
      arguments.mem_model |= (1 << MEM_DATA) | (1 << MEM_STACK);
   }

   memory_set_modelling(  arguments.mem_model       & 0x0f);
   if (triggered) {
      memory_set_rd_logging  ((arguments.mem_model >> 4) & 0x0f);
      memory_set_wr_logging  ((arguments.mem_model >> 8) & 0x0f);
   }

   // Implement default pins mapping for unspecified pins
   if (arguments.idx_data == UNSPECIFIED) {
      arguments.idx_data = 0;
   }
   if (arguments.idx_rnw == UNSPECIFIED) {
      arguments.idx_rnw = 8;
   }
   if (arguments.idx_lic == UNSPECIFIED && arguments.cpu_type != CPU_6809 && arguments.cpu_type != CPU_6309) {
      arguments.idx_lic = 9;
   }
   if (arguments.idx_bs == UNSPECIFIED) {
      arguments.idx_bs = 10;
   }
   if (arguments.idx_ba == UNSPECIFIED) {
      arguments.idx_ba = 11;
   }
   if (arguments.idx_addr == UNSPECIFIED) {
      arguments.idx_addr = 12;
   }

   // Check for conflicting signal assignments
   int conflicts = 0;
   for (int i = 0; i < 16; i++) {
      int count = 0;
      for (int pass = 0; pass < 2; pass++) {
         // Count the number of things assigned to this bit i
         if (i >= arguments.idx_data && i <= arguments.idx_data + 7) {
            if (pass == 0) {
               count++;
            } else if (count > 1) {
               fprintf(stderr, " data%d", i - arguments.idx_data);
            }
         }
         if (arguments.idx_rnw == i) {
            if (pass == 0) {
               count++;
            } else if (count > 1) {
               fprintf(stderr, " rnw");
            }
         }
         if (arguments.idx_lic == i) {
            if (pass == 0) {
               count++;
            } else if (count > 1) {
               fprintf(stderr, " lic");
            }
         }
         if (arguments.idx_bs == i) {
            if (pass == 0) {
               count++;
            } else if (count > 1) {
               fprintf(stderr, " bs");
            }
         }
         if (arguments.idx_ba == i) {
            if (pass == 0) {
               count++;
            } else if (count > 1) {
               fprintf(stderr, " ba");
            }
         }
         if (i >= arguments.idx_addr && i <= arguments.idx_addr + 3) {
            if (pass == 0) {
               count++;
            } else if (count > 1) {
               fprintf(stderr, " addr%d", i - arguments.idx_addr);
            }
         }
         if (arguments.idx_clke == i) {
            if (pass == 0) {
               count++;
            } else if (count > 1) {
               fprintf(stderr, " clke");
            }
         }
         if (count > 1) {
            if (pass == 0) {
               fprintf(stderr, "Conflicting assignments to bit %d:", i);
            } else {
               fprintf(stderr, "\n");
            }
            conflicts = 1;
         }
      }
   }
   if (conflicts) {
      return 1;
   }

   em = &em_6809;

   em->init(&arguments);

   FILE *stream;
   if (!arguments.filename || !strcmp(arguments.filename, "-")) {
      stream = stdin;
   } else {
      stream = fopen(arguments.filename, "r");
      if (stream == NULL) {
         perror("failed to open capture file");
         return 2;
      }
   }

   decode(stream);
   fclose(stream);

   return 0;
}
