#include <stdlib.h>

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ARM64_HELPERS_SIMD__
#define __ARM64_HELPERS_SIMD__ 1

#include "target64.h"

extern void arm64_hlp_dirty_simd_dup_element(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_simd_dup_general(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_shl_simd(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_scvtf_scalar_integer_simd(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_floating_point_compare_simd(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_floating_point_data_processing_2_source_simd(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_floating_point_immediate_simd(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_floating_point_conditional_select_simd(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_fcvtzs_scalar_integer_simd(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_floating_point_data_processing_3_source_simd(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_ucvtf_scalar_integer_simd(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_advanced_simd_shift_by_immediate_simd(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_advanced_simd_scalar_two_reg_misc_simd(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_advanced_simd_two_reg_misc_simd(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_advanced_simd_scalar_three_same_simd(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_advanced_simd_three_same_simd(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_advanced_simd_simd_three_different_simd(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_advanced_simd_scalar_pair_wise_simd(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_advanced_simd_accross_lanes_simd(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_advanced_simd_ext_simd(uint64_t _regs, uint32_t insn);

#endif

#ifdef __cplusplus
}
#endif
