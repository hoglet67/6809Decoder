#ifndef _INCLUDE_DIS_6809_H
#define _INCLUDE_DIS_6809_H

#include "defs.h"
#include "types_6809.h"

void dis_6809_init(cpu_t cpu_type, opcode_t *cpu_instr_table);

int dis_6809_disassemble(char *buffer, instruction_t *instruction);

#endif
