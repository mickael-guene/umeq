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

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ARM64_SOFTFLOAT__
#define __ARM64_SOFTFLOAT__ 1

#include "softfloat.h"
#include "arm64_helpers.h"

enum rm softfloat_rm_to_arm64_rm(int softfloat_rm);
int arm64_rm_to_softfloat_rm(enum rm arm_rm);
uint32_t softfloat_to_arm64_fpsr(float_status *fp_status, uint32_t qc);
uint32_t softfloat_to_arm64_cpsr(float_status *fp_status, uint32_t fpcr_others);

#endif

#ifdef __cplusplus
}
#endif
