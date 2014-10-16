#include <stdlib.h>

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ARM64_HELPERS__
#define __ARM64_HELPERS__ 1

#include "target64.h"

enum ops {
	OPS_ADD = 0,
	OPS_SUB,
	OPS_LOGICAL
};

extern void arm64_hlp_dump(uint64_t regs);
extern uint32_t arm64_hlp_compute_next_nzcv_32(uint64_t context, uint32_t opcode, uint32_t op1, uint32_t op2, uint32_t oldnzcv);
extern uint32_t arm64_hlp_compute_next_nzcv_64(uint64_t context, uint32_t opcode, uint64_t op1, uint64_t op2, uint32_t oldnzcv);
extern uint32_t arm64_hlp_compute_flags_pred(uint64_t context, uint32_t cond, uint32_t nzcv);
extern uint64_t arm64_hlp_compute_bitfield(uint64_t context, uint32_t insn, uint64_t rn, uint64_t rd);
extern uint64_t arm64_hlp_udiv_64(uint64_t context, uint64_t op1, uint64_t op2);
extern uint32_t arm64_hlp_udiv_32(uint64_t context, uint32_t op1, uint32_t op2);
extern uint64_t arm64_hlp_umul_lsb_64(uint64_t context, uint64_t op1, uint64_t op2);
extern uint32_t arm64_hlp_umul_lsb_32(uint64_t context, uint32_t op1, uint32_t op2);
extern uint64_t arm64_hlp_ldaxr(uint64_t regs, uint64_t address, uint32_t size_access);
extern uint32_t arm64_hlp_stxr(uint64_t regs, uint64_t address, uint32_t size_access, uint64_t value);
extern uint64_t arm64_hlp_clz(uint64_t context, uint64_t rn, uint32_t start_index);
extern uint64_t arm64_hlp_umul_msb_64(uint64_t context, uint64_t op1, uint64_t op2);
extern void arm64_hlp_memory_barrier(uint64_t regs);

#endif

#ifdef __cplusplus
}
#endif
