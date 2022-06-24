

// ====================================================================
// Undocumented Instructions
// ====================================================================

// Much of the information on the undocumented instructions comes from here:
// https://colorcomputerarchive.com/repo/Documents/Books/Motorola%206809%20and%20Hitachi%206309%20Programming%20Reference%20(Darren%20Atkinson).pdf

static int op_fn_XX(operand_t operand, ea_t ea, sample_t *sample_q) {
   return -1;
}

// Undefined opcodes in row 2 execute as a NEG instruction when the
// Carry bit in CC is 0, and as a COM instruction when the Carry bit
// is 1

static int op_fn_XNC(operand_t operand, ea_t ea, sample_t *sample_q) {
   if (C == 0) {
      neg_helper(operand);
   } else if (C == 1) {
      com_helper(operand);
   } else {
      set_NZVC_unknown();
   }
   return -1;
}

// Opcodes $14, $15 and $CD all cause the CPU to stop functioning
// normally. One or more of these may be the HCF (Halt and Catch Fire)
// instruction. The HCF instruction was provided for manufacturing
// test purposes. Its causes the CPU to halt execution and enter a
// mode in which the Address lines are incrementally strobed.

static int op_fn_XHCF(operand_t operand, ea_t ea, sample_t *sample_q) {
   // TODO
   return -1;
}

// Opcode $18 affects only the Condition Codes register (CC). The
// value in the Overflow bit (V) is shifted into the Zero bit (Z)
// while the value in the IRQ Mask bit (I) is shifted into the Half
// Carry bit (H). All other bits in the CC register are
// cleared. Execution of this opcode takes 3 MPU cycles.

static int op_fn_X18(operand_t operand, ea_t ea, sample_t *sample_q) {
   E = 0; // Bit 7
   F = 0;
   H = I;
   I = 0;
   N = 0;
   Z = V;
   V = 0;
   C = 0; // Bit 0
   return -1;
}

// Opcodes $87 and $C7 read and discard an 8-bit Immediate operand
// which follows the opcode. The value of the immediate byte is
// irrelevant. The Negative bit (N) in the CC register is always set,
// while the Zero (Z) and Overflow (V) bits are always cleared. No
// other bits in the Condition Codes register are affected. Each of
// these opcodes execute in 2 MPU cycles.

static int op_fn_X8C7(operand_t operand, ea_t ea, sample_t *sample_q) {
   N = 1;
   Z = 0;
   V = 0;
   return -1;
}


// Opcode $3E is similar to the SWI instruction. It stacks the Entire
// register state, sets the I and F bits in the Condition Codes
// register and then loads the PC register with an address obtained
// from the RESET vector ($FFFE:F). This could potentially be used as
// a fourth Software Interrupt instruction, so long as the code
// invoked by the Reset vector is able to differentiate between a
// software reset and a hardware reset. It does NOT set the Entire bit
// (E) in the CC register prior to stacking the register state. This
// could cause an RTI instruction for a Reset handler to fail to
// operate as expected. This opcode uses the same number of MPU cycles
// as SWI (15).

static int op_fn_XRES(operand_t operand, ea_t ea, sample_t *sample_q) {
   interrupt_helper(sample_q, 3, 1, 0xfffe);
   return -1;
}

// Opcodes $8F and $CF are STX Immediate and STU Immediate
// respectively. These instructions are partially functional. Two
// bytes of immediate data follow the opcode. The first immediate byte
// is read and discarded by the instruction. The lower half (LSB) of
// the X or U register is then written into the second immediate
// byte. The Negative bit (N) in the CC register is always set, while
// the Zero (Z) and Overflow (V) bits are always cleared. No other
// bits in the Condition Codes register are affected. Each of these
// opcodes execute in 3 MPU cycles.

static int op_fn_XSTX(operand_t operand, ea_t ea, sample_t *sample_q) {
   N = 1;
   Z = 0;
   V = 0;
   // TODO, need to force the EA to be PC + 2
   return X & 0xff;
}

static int op_fn_XSTU(operand_t operand, ea_t ea, sample_t *sample_q) {
   N = 1;
   Z = 0;
   V = 0;
   // TODO, need to force the EA to be PC + 2
   return U & 0xff;
}

// Undocumented

static operation_t op_XX   = { "??? ", op_fn_XX,       OTHER , 0 };
static operation_t op_X18  = { "X18 ", op_fn_X18,      REGOP , 0 };
static operation_t op_X8C7 = { "X8C7", op_fn_X8C7,    READOP , 0 };
static operation_t op_XHCF = { "XXHCF",op_fn_XHCF,    READOP , 0 };
static operation_t op_XNC  = { "XNC ", op_fn_XNC,     READOP , 0 };
static operation_t op_XSTX = { "XSTX", op_fn_XSTX,   STOREOP , 0 };
static operation_t op_XSTU = { "XSTU", op_fn_XSTU,   STOREOP , 0 };
static operation_t op_XRES = { "XRES", op_fn_XRES,     OTHER , 0 };
