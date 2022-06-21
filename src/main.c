#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <argp.h>
#include <string.h>
#include <math.h>

#include "defs.h"
#include "em_6809.h"
#include "memory.h"

// Value use to indicate a pin (or register) has not been assigned by
// the user, so should take the default value.
#define UNSPECIFIED -2

// Value used to indicate a pin (or register) is undefined. For a pin,
// this means unconnected. For a register this means it will default
// to a value of undefined (?).
#define UNDEFINED -1

int sample_count = 0;

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


static char disbuf[256];

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
   KEY_SKIP,
   KEY_DATA,
   KEY_RNW,
   KEY_LIC,
   KEY_BS,
   KEY_BA,
   KEY_ADDR
};


typedef struct {
   char *cpu_name;
   cpu_t cpu_type;
} cpu_name_t;

static cpu_name_t cpu_names[] = {
   // 6809
   {"6809",       CPU_6809},
   {"6809E",      CPU_6809E},

   // Terminator
   {NULL, 0}
};

static struct argp_option options[] = {
   { 0, 0, 0, 0, "General options:", GROUP_GENERAL},

   { "vecrst",      KEY_VECRST,    "HEX",  OPTION_ARG_OPTIONAL, "Reset vector, optionally preceeded by the first opcode (e.g. A9D9CD)",
                                                                                                                     GROUP_GENERAL},
   { "cpu",            KEY_CPU,     "CPU",                   0, "Sets CPU type (6809, 6809e)",                       GROUP_GENERAL},
   { "machine",    KEY_MACHINE, "MACHINE",                   0, "Enable machine (beeb,elk,master) defaults",         GROUP_GENERAL},
   { "byte",          KEY_BYTE,         0,                   0, "Enable byte-wide sample mode",                      GROUP_GENERAL},
   { "debug",        KEY_DEBUG,   "LEVEL",                   0, "Sets the debug level (0 or 1)",                     GROUP_GENERAL},
   { "trigger",    KEY_TRIGGER, "ADDRESS",                   0, "Trigger on address",                                GROUP_GENERAL},
   { "mem",            KEY_MEM,     "HEX", OPTION_ARG_OPTIONAL, "Memory modelling (see above)",                      GROUP_GENERAL},
   { "skip",          KEY_SKIP,     "HEX", OPTION_ARG_OPTIONAL, "Skip the first n samples",                          GROUP_GENERAL},

   { 0, 0, 0, 0, "Register options:", GROUP_REGISTER},
   { "reg_s",        KEY_REG_S,     "HEX", OPTION_ARG_OPTIONAL, "Initial value of the S register",                   GROUP_REGISTER},
   { "reg_u",        KEY_REG_U,     "HEX", OPTION_ARG_OPTIONAL, "Initial value of the U register",                   GROUP_REGISTER},
   { "reg_pc",      KEY_REG_PC,     "HEX", OPTION_ARG_OPTIONAL, "Initial value of the PC register",                  GROUP_REGISTER},

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
      static int ctr = 0;
      for (int i = 0; i < n; i++) {
         sample_t *sample = sample_q + i;
         printf("%8d %8x %d %02x ", ctr, ctr, i, sample->data);
         putchar(' ');
         putchar(sample->rnw >= 0 ? '0' + sample->rnw : '?');
         putchar(' ');
         putchar(sample->lic >= 0 ? '0' + sample->lic : '?');
         putchar(' ');
         putchar(sample->ba >= 0 ? '0' + sample->ba : '?');
         putchar(sample->bs >= 0 ? '0' + sample->bs : '?');
         putchar(' ');
         putchar(sample->addr >= 0 ? sample->addr + (sample->addr < 10 ? '0' : 'A' - 10) : '?');
         putchar('\n');
         ctr++;
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

static int analyze_instruction(sample_t *sample_q, int num_samples) {
   static int total_cycles = 0;
   static int interrupt_depth = 0;
   static int skipping_interrupted = 0;

   int num_cycles = 0;
   int intr_seen = 0;
   int rst_seen = em->match_reset(sample_q, num_samples);
   if (rst_seen) {
      num_cycles = rst_seen;
   } else {
      intr_seen = em->match_interrupt(sample_q, num_samples);
      if (intr_seen) {
         num_cycles = intr_seen;
      } else {
         num_cycles = em->count_cycles(sample_q);
      }
   }

   // Deal with partial final instruction
   if (num_samples <= num_cycles || num_cycles == 0) {
      return num_samples;
   }

   if (triggered && arguments.debug & 1) {
      dump_samples(sample_q, num_cycles);
   }

   instruction_t instruction;

   int oldpc = em->get_PC();

   if (rst_seen) {
      // Handle a reset
      em->reset(sample_q, num_cycles, &instruction);
   } else if (intr_seen) {
      // Handle an interrupt
      em->interrupt(sample_q, num_cycles, &instruction);
   } else {
      // Handle a normal instruction
      em->emulate(sample_q, num_cycles, &instruction);
   }

   // Sanity check the pc prediction has not gone awry
   // (e.g. in JSR the emulation can use the stacked PC)

   int opcode = instruction.opcode;
   int pc = instruction.pc;

   if (pc >= 0) {
      if (oldpc >= 0 && oldpc != pc) {
         printf("pc: prediction failed at %04X old pc was %04X\n", pc, oldpc);
      }
   }

   if (pc >= 0 && pc == arguments.trigger_start) {
      triggered = 1;
      printf("start trigger hit at cycle %d\n", total_cycles);
      memory_set_rd_logging((arguments.mem_model >> 4) & 0x0f);
      memory_set_wr_logging((arguments.mem_model >> 8) & 0x0f);
   } else if (pc >= 0 && pc == arguments.trigger_stop) {
      triggered = 0;
      printf("stop trigger hit at cycle %d\n", total_cycles);
      memory_set_rd_logging(0);
      memory_set_wr_logging(0);
   }

   // Exclude interrupts from profiling
   if (arguments.trigger_skipint && pc >= 0) {
      if (interrupt_depth == 0) {
         skipping_interrupted = 0;
      }
      if (intr_seen) {
         interrupt_depth++;
         skipping_interrupted = 1;
      } else if (interrupt_depth > 0 && opcode == 0x40) {
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
         if (rst_seen) {
            numchars = write_s(bp, "RESET !!");
         } else if (intr_seen) {
            numchars = write_s(bp, "INTERRUPT !!");
         } else {
            numchars = em->disassemble(bp, &instruction);
         }
         bp += numchars;
      }

      // Pad if there is more to come
      if (fail || arguments.show_cycles || arguments.show_state) {
         // Pad opcode to 14 characters, to match python
         while (numchars++ < 14) {
            *bp++ = ' ';
         }
      }
      // Show cycles (don't include with fail as it is inconsistent depending on whether rdy is present)
      if (arguments.show_cycles) {
         *bp++ = ' ';
         *bp++ = ':';
         *bp++ = ' ';
         *bp++ = (num_cycles < 10) ? ' ' : ('0' + num_cycles / 10);
         *bp++ = '0' + (num_cycles % 10);
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

   total_cycles += num_cycles;
   return num_cycles;
}

// ====================================================================
// Generic instruction decoder
// ====================================================================

// This stage is mostly about cleaning coming out of reset in all cases
//
//        Rst Sync
//
// Case 1:  ?   ?  : search for heuristic at n, n+1, n+2 - consume n+2 cycles
// Case 2:  ?  01  : search for heuristic at 5, 6, 7 - consume 7 ctcles
// Case 3: 01   ?  : dead reconning; 8 or 9 depending on the cpu type
// Case 4: 01  01  : mark first instruction after rst stable
//

int decode_instruction(sample_t *sample_q, int num_samples) {

   // Decode the instruction
   int num_cycles = analyze_instruction(sample_q, num_samples);

   return num_cycles;
}

// ====================================================================
// Queue a small number of samples so the decoders can lookahead
// ====================================================================

void queue_sample(sample_t *sample) {
   static sample_t sample_q[DEPTH];
   static int index = 0;

   // This helped when clock noise affected Arlet's core
   // (a better fix was to add 100pF cap to the clock)
   //
   // if (index > 0 && sample_q[index - 1].type == OPCODE && sample->type == OPCODE) {
   //    printf("Skipping duplicate SYNC\n");
   //    return;
   // }

   sample_q[index++] = *sample;

   if (sample->type == LAST) {
      // To prevent edge condition, don't advertise the LAST marker
      index--;
      // Drain the queue when the LAST marker is seen
      while (index > 1) {
         int consumed = decode_instruction(sample_q, index);
         for (int i = 0; i < DEPTH - consumed; i++) {
            sample_q[i] = sample_q[i + consumed];
         }
         index -= consumed;
      }
   } else {
      // Else queue the samples
      // If the queue is full, then pass on to the decoder
      if (index == DEPTH) {
         int consumed = decode_instruction(sample_q, index);
         for (int i = 0; i < DEPTH - consumed; i++) {
            sample_q[i] = sample_q[i + consumed];
         }
         index -= consumed;
      }
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

   // Default Pin values
   int bus_data  =  0;
   int pin_rnw   =  1;
   int pin_lic   =  0;
   int pin_bs    =  0;
   int pin_ba    =  0;
   int pin_addr  =  0;

   int num;

   // The previous sample of the 16-bit capture (async sampling only)
   uint16_t sample       = -1;

   sample_t s;

   // Skip the start of the file, if required
   if (arguments.skip) {
      fseek(stream, arguments.skip * (arguments.byte ? 1 : 2), SEEK_SET);
   }

   if (arguments.byte) {
      s.rnw = -1;
      s.lic = -1;
      s.bs = -1;
      s.ba = -1;
      s.addr = -1;

      // In byte mode we have only data bus samples, nothing else so we must
      // use the sync-less decoder. The values of pin_rnw and pin_rst are set
      // to 1, but the decoder should never actually use them.

      while ((num = fread(buffer8, sizeof(uint8_t), BUFSIZE, stream)) > 0) {
         uint8_t *sampleptr = &buffer8[0];
         while (num-- > 0) {
            s.data = *sampleptr++;
            queue_sample(&s);
         }
      }

   } else {

      // In word mode (the default) we have data bus samples, plus optionally
      // rnw, sync, rdy, phy2 and rst.

      while ((num = fread(buffer, sizeof(uint16_t), BUFSIZE, stream)) > 0) {

         uint16_t *sampleptr = &buffer[0];

         while (num-- > 0) {

            // The current 16-bit capture sample, and the previous two
            sample       = *sampleptr++;

            // TODO: fix the hard coded values!!!
            //if (arguments.debug & 4) {
            //   printf("%d %02x %x %x %x %x\n", sample_count, sample&255, (sample >> 8)&1,  (sample >> 9)&1,  (sample >> 10)&1,  (sample >> 11)&1  );
            //}
            sample_count++;

            // Phi2 is optional
            // - if asynchronous capture is used, it must be connected
            // - if synchronous capture is used, it must not connected

            // If Phi2 is not present, use the pins directly
            bus_data = (sample >> idx_data) & 255;
            if (idx_rnw >= 0) {
               pin_rnw = (sample >> idx_rnw ) & 1;
            }
            if (idx_lic >= 0) {
               pin_lic = (sample >> idx_lic) & 1;
            }
            if (idx_bs >= 0) {
               pin_bs = (sample >> idx_bs) & 1;
            }
            if (idx_ba >= 0) {
               pin_ba = (sample >> idx_ba) & 1;
            }
            if (idx_addr >= 0) {
               pin_addr = (sample >> idx_addr) & 15;
            }

            // Build the sample
            s.type = NORMAL;
            s.sample_count = sample_count;
            s.data = bus_data;
            if (idx_rnw < 0) {
               s.rnw = -1;
            } else {
               s.rnw = pin_rnw;
            }
            if (idx_lic < 0) {
               s.lic = -1;
            } else {
               s.lic = pin_lic;
            }
            if (idx_bs < 0) {
               s.bs = -1;
            } else {
               s.bs = pin_bs;
            }
            if (idx_ba < 0) {
               s.ba = -1;
            } else {
               s.ba = pin_ba;
            }
            if (idx_addr < 0) {
               s.addr = -1;
            } else {
               s.addr = pin_addr;
            }
            queue_sample(&s);
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
   arguments.trigger_start    = UNSPECIFIED;
   arguments.trigger_stop     = UNSPECIFIED;
   arguments.trigger_skipint  = 0;
   arguments.filename         = NULL;

   // Register options
   arguments.reg_s            = UNSPECIFIED;
   arguments.reg_u            = UNSPECIFIED;
   arguments.reg_pc           = UNSPECIFIED;

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

   if (arguments.cpu_type == CPU_6809) {
      if (arguments.idx_lic != UNSPECIFIED) {
         fprintf(stderr, "--lic= is incompatible with the 6809 CPU as it doesn't have this pin\n");
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
   if (arguments.idx_lic == UNSPECIFIED && arguments.cpu_type != CPU_6809) {
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
