#include <stdlib.h>

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ARM_HELPERS__
#define __ARM_HELPERS__ 1

#define g_2_h(ptr)  ((uint64_t)(ptr))
#define h_2_g(ptr)  ((ptr))

extern void arm_hlp_dump(uint64_t regs);
extern void arm_hlp_dump_and_assert(uint64_t regs);
extern void arm_hlp_gdb_handle_breakpoint(uint64_t regs);
extern void arm_hlp_vdso_cmpxchg(uint64_t _regs);
extern uint32_t arm_hlp_compute_flags_pred(uint64_t context, uint32_t cond, uint32_t cpsr);
extern uint32_t arm_hlp_compute_next_flags(uint64_t context, uint32_t opcode_and_shifter_carry_out, uint32_t rn, uint32_t op, uint32_t oldcpsr);
extern uint32_t arm_hlp_compute_sco(uint64_t context, uint32_t insn, uint32_t rm, uint32_t op, uint32_t oldcpsr);
extern void arm_hlp_syscall(uint64_t regs);
extern uint32_t thumb_hlp_compute_next_flags(uint64_t context, uint32_t opcode, uint32_t rn, uint32_t op, uint32_t oldcpsr);
extern uint32_t arm_hlp_clz(uint64_t context, uint32_t rm);
extern uint32_t arm_hlp_multiply_unsigned_lsb(uint64_t context, uint32_t op1, uint32_t op2);
extern uint32_t arm_hlp_multiply_flag_update(uint64_t context, uint32_t res, uint32_t old_cpsr);
extern uint32_t arm_hlp_ldrex(uint64_t context, uint32_t address, uint32_t size_access);
extern uint32_t arm_hlp_strex(uint64_t regs, uint32_t address, uint32_t size_access, uint32_t value);
extern void arm_hlp_memory_barrier(uint64_t regs);
extern uint32_t thumb_hlp_t2_modified_compute_next_flags(uint64_t context, uint32_t opcode_and_shifter_carry_out, uint32_t rn, uint32_t op, uint32_t oldcpsr);
extern void thumb_hlp_t2_unsigned_parallel(uint64_t regs, uint32_t insn);
extern void arm_hlp_unsigned_parallel(uint64_t regs, uint32_t insn);
extern uint32_t arm_hlp_sel(uint64_t context, uint32_t cpsr, uint32_t rn, uint32_t rm);
extern uint32_t arm_hlp_multiply_unsigned_msb(uint64_t context, uint32_t op1, uint32_t op2);
extern uint32_t arm_hlp_multiply_signed_lsb(uint64_t context, int32_t op1, int32_t op2);
extern uint32_t arm_hlp_multiply_signed_msb(uint64_t context, int32_t op1, int32_t op2);
extern uint32_t arm_hlp_multiply_accumulate_unsigned_lsb(uint64_t context, uint32_t op1, uint32_t op2, uint32_t rdhi, uint32_t rdlo);
extern uint32_t arm_hlp_multiply_accumulate_signed_lsb(uint64_t context, int32_t op1, int32_t op2, uint32_t rdhi, uint32_t rdlo);
extern uint32_t arm_hlp_multiply_accumulate_unsigned_msb(uint64_t context, uint32_t op1, uint32_t op2, uint32_t rdhi, uint32_t rdlo);
extern uint32_t arm_hlp_multiply_accumulate_signed_msb(uint64_t context, int32_t op1, int32_t op2, uint32_t rdhi, uint32_t rdlo);
extern uint32_t thumb_hlp_compute_next_flags_data_processing(uint64_t context, uint32_t opcode, uint32_t rn, uint32_t op, uint32_t oldcpsr);
extern uint32_t thumb_t2_hlp_compute_sco(uint64_t context, uint32_t insn, uint32_t rm, uint32_t op, uint32_t oldcpsr);
extern void hlp_dirty_vpush(uint64_t regs, uint32_t insn);

#endif

#ifdef __cplusplus
}
#endif
