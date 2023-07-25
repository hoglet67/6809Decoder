#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "defs.h"
#include "memory.h"

// Main Memory

static int *memory        = NULL;
static int mem_model      = 0;
static int mem_rd_logging = 0;
static int mem_wr_logging = 0;

// Standard sideways ROM (upto 16 banks)
#define SWROM_SIZE          0x4000
#define SWROM_NUM_BANKS     16
static int *swrom         = NULL;
static int rom_latch      = -1;

static char buffer[256];

// Machine specific memory rd/wr handlers
static void (*memory_read_fn)(int data, int ea);
static int (*memory_write_fn)(int data, int ea);

// Machine specific address display handler (to allow SW Rom bank on the Beeb to be shown)
static int (*addr_display_fn)(char *bp, int ea);

#define TO_HEX(value) ((value) + ((value) < 10 ? '0' : 'A' - 10))

int write_bankid(char *bp, int ea) {
   if (ea < 0x8000 || ea >= 0xc000) {
      *bp++ = ' ';
      *bp++ = ' ';
   } else if (rom_latch < 0) {
      *bp++ = '?';
      *bp++ = '-';
   } else {
      *bp++ = TO_HEX(rom_latch);
      *bp++ = '-';
   }
  return 2;
}

static inline void log_memory_access(char *msg, int data, int ea, int ignored) {
   char *bp = buffer;
   bp += write_s(bp, msg);
   bp += addr_display_fn(bp, ea);
   bp += write_s(bp, " = ");
   write_hex2(bp, data);
   bp += 2;
   *bp++ = ' ';
   *bp++ = (data >= 32 && data <= 126) ? data : '.';
   if (ignored) {
      bp += write_s(bp, " (ignored)");
   }
   *bp++ = 0;
   puts(buffer);
}


static inline void log_memory_fail(int ea, int expected, int actual) {
   char *bp = buffer;
   bp += write_s(bp, "memory modelling failed at ");
   bp += addr_display_fn(bp, ea);
   bp += write_s(bp, ": expected ");
   write_hex2(bp, expected);
   bp += 2;
   bp += write_s(bp, " actual ");
   write_hex2(bp, actual);
   bp += 2;
   *bp++ = 0;
   puts(buffer);
}

static int *init_ram(int size) {
   int *ram =  malloc(size * sizeof(int));
   for (int i = 0; i < size; i++) {
      ram[i] = -1;
   }
   return ram;
}

// ==================================================
// Default Memory Handlers
// ==================================================

static int addr_display_default(char *bp, int ea) {
   write_hex4(bp, ea);
   return 4;
}

static void memory_read_default(int data, int ea) {
   if (memory[ea] >= 0 && memory[ea] != data) {
      log_memory_fail(ea, memory[ea], data);
      failflag |= FAIL_MEMORY;
   }
   memory[ea] = data;
}

static int memory_write_default(int data, int ea) {
   memory[ea] = data;
   return 0;
}

static void init_default(arguments_t *args) {
   memory = init_ram(0x10000);
   memory_read_fn  = memory_read_default;
   memory_write_fn = memory_write_default;
   addr_display_fn = addr_display_default;
}

// ==================================================
// Dragon Memory Handlers
// ==================================================

static void memory_read_dragon(int data, int ea) {
   if (memory[ea] >= 0 && memory[ea] != data && (ea < 0xff00 || ea >= 0xfff0)) {
      log_memory_fail(ea, memory[ea], data);
      failflag |= FAIL_MEMORY;
   }
   memory[ea] = data;
}

static void init_dragon(arguments_t *args) {
   memory = init_ram(0x10000);
   memory_read_fn  = memory_read_dragon;
   memory_write_fn = memory_write_default;
   addr_display_fn = addr_display_default;
}

// ==================================================
// SBC09 Memory Handlers
// ==================================================

static int mmu_disabled[4] = { 0x80, 0x81, 0x00, 0x01 };

static int mmu0[4] = { 0xff, 0xff, 0xff, 0xff };

static int mmu_enabled = 0;

static int get_block(int ea) {
   if (ea < 0) {
      fprintf(stderr, "EA should never be undefined; exiting\n");
      exit(1);
   } else if (ea < 0xfc00 || ea >= 0xff00) {
      if (mmu_enabled) {
         return mmu0[(ea >> 14) & 3];
      } else {
         return mmu_disabled[(ea >> 14) & 3];
      }
   } else {
      return -1; // IO
   }
}

static int addr_display_sbc09(char *bp, int ea) {
   int block = get_block(ea);
   if (block >= 0) {
      write_hex2(bp, block);
      bp += 2;
      *bp++ = ':';
      write_hex4(bp, ea);
      return 7;
   } else {
      write_hex4(bp, ea);
      return 4;
   }
}

static inline int *get_memptr_sbc09(int ea) {
   int block = get_block(ea);
   return memory + (block << 14) + (ea & 0x3FFF);
}


static void memory_read_sbc09(int data, int ea) {
   if (ea < 0xfc00 || ea >= 0xff00) {
      int *memptr = get_memptr_sbc09(ea);
      if (memptr) {
         if (*memptr >= 0 && *memptr != data) {
            log_memory_fail(ea, *memptr, data);
            failflag |= FAIL_MEMORY;
         }
         *memptr = data;
      }
   }
}

static int memory_write_sbc09(int data, int ea) {
   if (ea < 0xfc00 || ea >= 0xff00) {
      int *memptr = get_memptr_sbc09(ea);
      if (memptr) {
         *memptr = data;
      }
   } else if (ea == 0xfe0e) {
      if (data & 0x10) {
         printf("*** MMU Enabled ***\n");
         mmu_enabled = 1;
      }
   } else if (ea == 0xfe0f) {
      if (data & 0x10) {
         printf("*** MMU Disabled ***\n");
         mmu_enabled = 0;
      }
   } else if (ea >= 0xfe10 && ea <= 0xfe13) {
      mmu0[ea & 3] = data;
   }
   return 0;
}

static void init_sbc09(arguments_t *args) {
   // For now, lets just model 256 x 16K blocks
   memory = init_ram(0x100 * 0x4000);
   memory_read_fn  = memory_read_sbc09;
   memory_write_fn = memory_write_sbc09;
   addr_display_fn = addr_display_sbc09;
}

// ==================================================
// Beeb Memory Handlers
// ==================================================

static int addr_display_beeb(char *bp, int ea) {
   write_bankid(bp, ea);
   write_hex4(bp + 2, ea);
   return 6;
}

static inline void set_rom_latch(int data) {
   rom_latch = data;
}

static inline int *get_memptr_beeb(int ea) {
   if (ea < 0) {
      fprintf(stderr, "EA should never be undefined; exiting\n");
      exit(1);
   }
   if (ea >= 0x8000 && ea < 0xC000) {
      if (rom_latch >= 0) {
         return swrom + (rom_latch << 14) + (ea & 0x3FFF);
      } else {
         return NULL;
      }
   } else {
      return memory + ea;
   }
}

static void memory_read_beeb(int data, int ea) {
   if (ea < 0xfc00 || ea >= 0xff00) {
      int *memptr = get_memptr_beeb(ea);
      if (memptr) {
         if (*memptr >= 0 && *memptr != data) {
            log_memory_fail(ea, *memptr, data);
            failflag |= FAIL_MEMORY;
         }
         *memptr = data;
      }
   }
}

static int memory_write_beeb(int data, int ea) {
   if (ea == 0xfe30) {
      set_rom_latch(data & 0xf);
   }
   if ((ea >= 0x8000 && ea < 0xfc00) || (ea >= 0xff00)) {
      return 1;
   }
   int *memptr = get_memptr_beeb(ea);
   if (memptr) {
      *memptr = data;
   }
   return 0;
}

static void init_beeb(arguments_t *args) {
   memory = init_ram(0x10000);
   swrom = init_ram(SWROM_NUM_BANKS * SWROM_SIZE);
   memory_read_fn  = memory_read_beeb;
   memory_write_fn = memory_write_beeb;
   addr_display_fn = addr_display_beeb;
   if (args->rom_latch >= 0 && args->rom_latch <= 15) {
      rom_latch = args->rom_latch;
   }
}

// ==================================================
// Public Methods
// ==================================================

void memory_init(arguments_t *args) {
   // Setup the machine specific memory read/write handler
   switch (args->machine) {
   case MACHINE_DRAGON32:
      init_dragon(args);
      break;
   case MACHINE_BEEB:
      init_beeb(args);
      break;
   case MACHINE_SBC09:
      init_sbc09(args);
      break;
   default:
      init_default(args);
      break;
   }
}

void memory_destroy() {
   if (swrom) {
      free(swrom);
   }
   if (memory) {
      free(memory);
   }
}

void memory_set_modelling(int bitmask) {
   mem_model = bitmask;
}

void memory_set_rd_logging(int bitmask) {
   mem_rd_logging = bitmask;
}

void memory_set_wr_logging(int bitmask) {
   mem_wr_logging = bitmask;
}


int memory_get_modelling() {
   return mem_model;
}

int memory_get_rd_logging() {
   return mem_rd_logging;
}

int memory_get_wr_logging() {
   return mem_wr_logging;
}

void memory_read(sample_t *sample, int ea, mem_access_t type) {
   if (sample->rnw == 0) {
      failflag |= FAIL_RNW;
   }
   // If the effective address is unknown, we can't do any modelling
   if (ea < 0) {
      return;
   }
   int data = sample->data;
   validate_address(sample, ea, 1 << type);
   // Log memory read
   if (mem_rd_logging & (1 << type)) {
      log_memory_access("Rd: ", data, ea, 0);
   }
   // Delegate memory read to machine specific handler
   if (mem_model & (1 << type)) {
      (*memory_read_fn)(data, ea);
   }
}

void memory_write(sample_t *sample, int ea, mem_access_t type) {
   if (sample->rnw == 1) {
      failflag |= FAIL_RNW;
   }
   // If the effective address is unknown, we can't do any modelling
   if (ea < 0) {
      return;
   }
   int data = sample->data;
   validate_address(sample, ea, 1 << type);
   // Delegate memory write to machine specific handler
   int ignored = 0;
   if (mem_model & (1 << type)) {
      ignored = (*memory_write_fn)(data, ea);
   }
   // Log memory write
   if (mem_wr_logging & (1 << type)) {
      log_memory_access("Wr: ", data, ea, ignored);
   }
}

int memory_read_raw(int ea) {
   return memory[ea];
}
