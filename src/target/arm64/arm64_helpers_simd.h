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
extern void arm64_hlp_dirty_advanced_simd_shift_by_immediate_simd(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_advanced_simd_scalar_two_reg_misc_simd(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_advanced_simd_two_reg_misc_simd(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_advanced_simd_scalar_three_same_simd(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_advanced_simd_three_same_simd(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_advanced_simd_simd_three_different_simd(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_advanced_simd_scalar_pair_wise_simd(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_advanced_simd_accross_lanes_simd(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_advanced_simd_ext_simd(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_advanced_simd_vector_x_indexed_element_simd(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_advanced_simd_scalar_shift_by_immediate_simd(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_advanced_simd_scalar_x_indexed_element_simd(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_advanced_simd_scalar_three_different_simd(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_advanced_simd_load_store_multiple_structure_simd(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_advanced_simd_load_store_multiple_structure_post_index_simd(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_advanced_simd_load_store_single_structure_simd(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_advanced_simd_load_store_single_structure_post_index_simd(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_advanced_simd_table_lookup_simd(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_advanced_simd_permute_simd(uint64_t _regs, uint32_t insn);

#endif

#ifdef __cplusplus
}
#endif
