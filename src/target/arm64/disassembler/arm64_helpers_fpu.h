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

#ifndef __ARM64_HELPERS_FPU__
#define __ARM64_HELPERS_FPU__ 1

#include "target64.h"

extern void arm64_hlp_dirty_floating_point_compare_simd(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_floating_point_data_processing_2_source_simd(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_floating_point_immediate_simd(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_floating_point_conditional_select_simd(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_floating_point_data_processing_3_source_simd(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_floating_point_data_processing_1_source(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_floating_point_conditional_compare(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_scvtf_scalar_integer_simd(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_ucvtf_scalar_integer_simd(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_fcvtxx_scalar_integer_simd(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_conversion_between_floating_point_and_fixed_point(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_write_fpsr(uint64_t _regs, uint32_t value);
extern uint32_t arm64_hlp_read_fpsr(uint64_t _regs);
extern void arm64_hlp_write_fpcr(uint64_t _regs, uint32_t value);
extern uint32_t arm64_hlp_read_fpcr(uint64_t _regs);

#endif

#ifdef __cplusplus
}
#endif
