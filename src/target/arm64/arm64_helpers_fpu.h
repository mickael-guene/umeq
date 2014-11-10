#include <stdlib.h>

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ARM64_HELPERS_FPU__
#define __ARM64_HELPERS_FPU__ 1

#include "target64.h"

extern void arm64_hlp_dirty_scvtf_scalar_integer_simd(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_floating_point_compare_simd(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_floating_point_data_processing_2_source_simd(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_floating_point_immediate_simd(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_floating_point_conditional_select_simd(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_fcvtzs_scalar_integer_simd(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_floating_point_data_processing_3_source_simd(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_ucvtf_scalar_integer_simd(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_floating_point_data_processing_1_source(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_floating_point_conditional_compare(uint64_t _regs, uint32_t insn);

#endif

#ifdef __cplusplus
}
#endif
