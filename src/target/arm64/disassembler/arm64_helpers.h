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

#ifndef __ARM64_HELPERS__
#define __ARM64_HELPERS__ 1

#include "target64.h"

#define ARM64_FPSR_IOC      1
#define ARM64_FPSR_DZC      2
#define ARM64_FPSR_OFC      4
#define ARM64_FPSR_UFC      8
#define ARM64_FPSR_IXC      16
#define ARM64_FPSR_IDC      128

enum ops {
    OPS_ADD = 0,
    OPS_SUB,
    OPS_LOGICAL,
    OPS_ADC
};

enum rm {
    RM_TIEEVEN = 0,
    RM_POSINF,
    RM_NEGINF,
    RM_ZERO,
    RM_TIEAWAY,
    RM_ODD
};

extern void arm64_hlp_dump(uint64_t regs);
extern void arm64_gdb_breakpoint_instruction(uint64_t regs);
extern void arm64_gdb_stepin_instruction(uint64_t regs);
extern void arm64_clean_caches(uint64_t regs);
extern uint32_t arm64_hlp_compute_next_nzcv_32(uint64_t context, uint32_t opcode, uint32_t op1, uint32_t op2, uint32_t oldnzcv);
extern uint32_t arm64_hlp_compute_next_nzcv_64(uint64_t context, uint32_t opcode, uint64_t op1, uint64_t op2, uint32_t oldnzcv);
extern uint32_t arm64_hlp_compute_flags_pred(uint64_t context, uint32_t cond, uint32_t nzcv);
extern uint64_t arm64_hlp_compute_bitfield(uint64_t context, uint32_t insn, uint64_t rn, uint64_t rd);
extern uint64_t arm64_hlp_udiv_64(uint64_t context, uint64_t op1, uint64_t op2);
extern uint32_t arm64_hlp_udiv_32(uint64_t context, uint32_t op1, uint32_t op2);
extern int64_t arm64_hlp_sdiv_64(uint64_t context, int64_t op1, int64_t op2);
extern int32_t arm64_hlp_sdiv_32(uint64_t context, int32_t op1, int32_t op2);
extern uint64_t arm64_hlp_umul_lsb_64(uint64_t context, uint64_t op1, uint64_t op2);
extern uint32_t arm64_hlp_umul_lsb_32(uint64_t context, uint32_t op1, uint32_t op2);
extern int64_t arm64_hlp_smul_lsb_64(uint64_t context, int64_t op1, int64_t op2);
extern uint64_t arm64_hlp_ldxr(uint64_t regs, uint64_t address, uint32_t size_access);
extern uint64_t arm64_hlp_ldaxr(uint64_t regs, uint64_t address, uint32_t size_access);
extern uint32_t arm64_hlp_stxr(uint64_t regs, uint64_t address, uint32_t size_access, uint64_t value);
extern uint64_t arm64_hlp_clz(uint64_t context, uint64_t rn, uint32_t start_index);
extern uint64_t arm64_hlp_umul_msb_64(uint64_t context, uint64_t op1, uint64_t op2);
extern int64_t arm64_hlp_smul_msb_64(uint64_t context, int64_t op1, int64_t op2);
extern void arm64_hlp_memory_barrier(uint64_t regs);
extern void arm64_hlp_clrex(uint64_t regs);
extern uint64_t arm64_hlp_cls(uint64_t context, uint64_t rn, uint32_t start_index);
extern void arm64_hlp_ldxp_dirty(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_ldaxp_dirty(uint64_t _regs, uint32_t insn);
extern uint32_t arm64_hlp_stlxr(uint64_t regs, uint64_t address, uint32_t size_access, uint64_t value);
extern void arm64_hlp_stxp_dirty(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_stlxp_dirty(uint64_t _regs, uint32_t insn);
extern void arm64_hlp_dirty_crc32(uint64_t regs, uint32_t insn);

#endif

#ifdef __cplusplus
}
#endif
