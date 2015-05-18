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

#ifndef __ARM_HELPERS__
#define __ARM_HELPERS__ 1

#include "target32.h"

extern void arm_hlp_dump(uint64_t regs);
extern void arm_hlp_dump_and_assert(uint64_t regs);
extern void arm_hlp_vdso_cmpxchg(uint64_t _regs);
extern void arm_hlp_vdso_cmpxchg64(uint64_t _regs);
extern uint32_t arm_hlp_compute_flags_pred(uint64_t context, uint32_t cond, uint32_t cpsr);
extern uint32_t arm_hlp_compute_next_flags(uint64_t context, uint32_t opcode_and_shifter_carry_out, uint32_t rn, uint32_t op, uint32_t oldcpsr);
extern uint32_t arm_hlp_compute_sco(uint64_t context, uint32_t insn, uint32_t rm, uint32_t op, uint32_t oldcpsr);
extern void arm_hlp_syscall(uint64_t regs);
extern uint32_t thumb_hlp_compute_next_flags(uint64_t context, uint32_t opcode, uint32_t rn, uint32_t op, uint32_t oldcpsr);
extern uint32_t arm_hlp_clz(uint64_t context, uint32_t rm);
extern uint32_t arm_hlp_multiply_unsigned_lsb(uint64_t context, uint32_t op1, uint32_t op2);
extern uint32_t arm_hlp_multiply_flag_update(uint64_t context, uint32_t res, uint32_t old_cpsr);
extern uint32_t arm_hlp_ldrexx(uint64_t context, uint32_t address, uint32_t size_access);
extern uint32_t arm_hlp_strexx(uint64_t regs, uint32_t address, uint32_t size_access, uint32_t value);
extern uint64_t arm_hlp_ldrexd(uint64_t context, uint32_t address);
extern uint32_t arm_hlp_strexd(uint64_t regs, uint32_t address, uint32_t value, uint32_t value2);
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
extern void arm_gdb_breakpoint_instruction(uint64_t regs);
extern void arm_hlp_dirty_saturating(uint64_t regs, uint32_t insn);
extern void arm_hlp_signed_parallel(uint64_t regs, uint32_t insn);
extern void arm_hlp_halfword_mult_and_mult_acc(uint64_t regs, uint32_t insn);
extern void arm_hlp_signed_multiplies(uint64_t regs, uint32_t insn);
extern void arm_hlp_saturation(uint64_t regs, uint32_t insn);
extern void arm_hlp_umaal(uint64_t regs, uint32_t rdhi, uint32_t rdlow, uint32_t rn, uint32_t rm);
extern void arm_hlp_sum_absolute_difference(uint64_t regs, uint32_t insn);
extern void thumb_hlp_t2_misc_saturating(uint64_t regs, uint32_t insn);
extern void thumb_hlp_t2_signed_parallel(uint64_t regs, uint32_t insn);
extern void thumb_hlp_t2_mult_A_mult_acc_A_abs_diff(uint64_t regs, uint32_t insn);
extern void thumb_hlp_t2_long_mult_A_long_mult_acc_A_div(uint64_t regs, uint32_t insn);
extern void t32_hlp_saturation(uint64_t regs, uint32_t insn);
extern void hlp_arm_vldm(uint64_t regs, uint32_t insn, uint32_t is_thumb);
extern void hlp_arm_vstm(uint64_t regs, uint32_t insn, uint32_t is_thumb);
extern void arm_hlp_common_vcvt_vcvtr_floating_integer(uint64_t regs, uint32_t insn);
extern void hlp_common_vfp_data_processing_insn(uint64_t regs, uint32_t insn);
extern void hlp_common_adv_simd_three_same_length(uint64_t regs, uint32_t insn, uint32_t is_thumb);
extern void hlp_common_adv_simd_three_different_length(uint64_t regs, uint32_t insn, uint32_t is_thumb);

#endif

#ifdef __cplusplus
}
#endif
