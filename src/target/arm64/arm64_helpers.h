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
	OPS_SUB
};

extern void arm64_hlp_dump(uint64_t regs);
extern uint32_t arm64_hlp_compute_next_nzcv_32(uint64_t context, uint32_t opcode, uint32_t op1, uint32_t op2, uint32_t oldnzcv);
extern uint32_t arm64_hlp_compute_next_nzcv_64(uint64_t context, uint32_t opcode, uint64_t op1, uint64_t op2, uint32_t oldnzcv);
extern uint32_t arm64_hlp_compute_flags_pred(uint64_t context, uint32_t cond, uint32_t nzcv);

#endif

#ifdef __cplusplus
}
#endif
