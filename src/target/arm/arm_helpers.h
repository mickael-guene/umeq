#include <stdlib.h>

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ARM_HELPERS__
#define __ARM_HELPERS__ 1

extern void arm_hlp_dump(uint64_t regs);
extern void arm_hlp_vdso_cmpxchg(uint64_t _regs);
extern uint32_t arm_hlp_compute_flags_pred(uint64_t context, uint32_t cond, uint32_t cpsr);
extern uint32_t arm_hlp_compute_next_flags(uint64_t context, uint32_t opcode_and_shifter_carry_out, uint32_t rn, uint32_t op, uint32_t oldcpsr);
extern uint32_t arm_hlp_compute_sco(uint64_t context, uint32_t insn, uint32_t rm, uint32_t op, uint32_t oldcpsr);
extern void arm_hlp_syscall(uint64_t regs);

#endif

#ifdef __cplusplus
}
#endif
