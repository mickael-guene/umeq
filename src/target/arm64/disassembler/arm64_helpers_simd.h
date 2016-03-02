/* This file is part of Umeq, an equivalent of qemu user mode emulation with improved robustness.
 *
 * Copyright (C) 2015 STMicroelectronics
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA.
 */

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
