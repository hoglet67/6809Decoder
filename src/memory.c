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
static int addr_digits    = 0;

static char buffer[256];

// Machine specific memory rd/wr handlers
static void (*memory_read_fn)(int data, int ea);
static int (*memory_write_fn)(int data, int ea);


#define TO_HEX(value) ((value) + ((value) < 10 ? '0' : 'A' - 10))

static inline int write_addr(char *bp, int ea) {
   int shift = (addr_digits - 1) << 2; // 6 => 20
   for (int i = 0; i < addr_digits; i++) {
      int value = (ea >> shift) & 0xf;
      *bp++ = TO_HEX(value);
      shift -= 4;
   }
   return addr_digits + 2;
}


static inline void log_memory_access(char *msg, int data, int ea, int ignored) {
   char *bp = buffer;
   bp += write_s(bp, msg);
   bp += write_addr(bp, ea);
   bp += write_s(bp, " = ");
   write_hex2(bp, data);
   bp += 2;
   if (ignored) {
   bp += write_s(bp, " (ignored)");
   }
   *bp++ = 0;
   puts(buffer);
}


static inline void log_memory_fail(int ea, int expected, int actual) {
   char *bp = buffer;
   bp += write_s(bp, "memory modelling failed at ");
   bp += write_addr(bp, ea);
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

static void memory_read_default(int data, int ea) {
   if (memory[ea] >= 0 && memory[ea] != data) {
      log_memory_fail(ea,memory[ea], data);
      failflag |= 1;
   }
   memory[ea] = data;
}

static int memory_write_default(int data, int ea) {
   memory[ea] = data;
   return 0;
}

static void init_default() {
   memory_read_fn  = memory_read_default;
   memory_write_fn = memory_write_default;
}

// ==================================================
// Public Methods
// ==================================================

void memory_init(int size, machine_t machine) {
   memory = init_ram(size);
   // Setup the machine specific memory read/write handler
   switch (machine) {
   default:
      init_default();
      break;
   }
   // Calculate the number of digits to represent an address
   addr_digits = 0;
   size--;
   while (size) {
      size >>= 1;
      addr_digits++;
   }
   addr_digits = (addr_digits + 3) >> 2;
}

void memory_destroy() {
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

void memory_read(int data, int ea, mem_access_t type) {
   assert(ea >= 0);
   assert(data >= 0);
   if (type == MEM_FETCH) {
      type = MEM_INSTR;
   }
   // Log memory read
   if (mem_rd_logging & (1 << type)) {
      log_memory_access("Rd: ", data, ea, 0);
   }
   // Delegate memory read to machine specific handler
   if (mem_model & (1 << type)) {
      (*memory_read_fn)(data, ea);
   }
}

void memory_write(int data, int ea, mem_access_t type) {
   assert(ea >= 0);
   assert(data >= 0);
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
