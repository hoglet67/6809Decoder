#ifndef MEMORY_H
#define MEMORY_H

#include "defs.h"

typedef enum {
   MEM_INSTR    = 0,
   MEM_POINTER  = 1,
   MEM_DATA     = 2,
   MEM_STACK    = 3,
} mem_access_t;

void memory_init(int size, machine_t machine);

void memory_set_modelling(int bitmask);

void memory_set_rd_logging(int bitmask);

void memory_set_wr_logging(int bitmask);

int memory_get_modelling();

int memory_get_rd_logging();

int memory_get_wr_logging();

void memory_read(sample_t *sample, int ea, mem_access_t type);

void memory_write(sample_t *sample, int ea, mem_access_t type);

int memory_read_raw(int ea);

void memory_destroy();

#endif
