#include <stdlib.h>

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ARM64_HELPERS_SIMD__
#define __ARM64_HELPERS_SIMD__ 1

#include "target64.h"

extern void arm64_hlp_dirty_simd_dup_general(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_add_simd_vector(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_cmpeq_simd_vector(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_cmpeq_register_simd_vector(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_orr_register_simd_vector(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_addp_simd_vector(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_and_simd_vector(uint64_t _regs, uint32_t insn);

#endif

#ifdef __cplusplus
}
#endif
