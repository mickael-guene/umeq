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

#include <assert.h>
#include "arm64_softfloat.h"

enum rm softfloat_rm_to_arm64_rm(int softfloat_rm)
{
    switch(softfloat_rm) {
        case float_round_nearest_even:
            return RM_TIEEVEN;
            break;
        case float_round_down:
            return RM_NEGINF;
            break;
        case float_round_up:
            return RM_POSINF;
            break;
        case float_round_to_zero:
            return RM_ZERO;
            break;
        case float_round_ties_away:
            return RM_TIEAWAY;
            break;
        default:
            assert(0);
    }
}

int arm64_rm_to_softfloat_rm(enum rm arm_rm)
{
    switch(arm_rm) {
        case RM_TIEEVEN:
            return float_round_nearest_even;
            break;
        case RM_POSINF:
            return float_round_up;
            break;
        case RM_NEGINF:
            return float_round_down;
            break;
        case RM_ZERO:
            return float_round_to_zero;
            break;
        case RM_TIEAWAY:
            return float_round_ties_away;
            break;
        case RM_ODD:
            /* this one is not supported by softfloat */
        default:
            assert(0);
    }
}

uint32_t softfloat_to_arm64_fpsr(float_status *fp_status, uint32_t qc)
{
    uint32_t fpsr = qc;
    int exceptions = get_float_exception_flags(fp_status);

    /* set up exceptions bits */
    if (exceptions & float_flag_input_denormal)
        fpsr |= ARM64_FPSR_IDC;
    if (exceptions & float_flag_inexact)
        fpsr |= ARM64_FPSR_IXC;
    if (exceptions & float_flag_underflow)
        fpsr |= ARM64_FPSR_UFC;
    if (exceptions & float_flag_overflow)
        fpsr |= ARM64_FPSR_OFC;
    if (exceptions & float_flag_divbyzero)
        fpsr |= ARM64_FPSR_DZC;
    if (exceptions & float_flag_invalid)
        fpsr |= ARM64_FPSR_IOC;

    return fpsr;
}

uint32_t softfloat_to_arm64_cpsr(float_status *fp_status, uint32_t fpcr_others)
{
    uint32_t fpcr = 0;

    if (get_default_nan_mode(fp_status))
        fpcr |= 1 << 25;
    if (get_flush_to_zero(fp_status))
        fpcr |= 1 << 24;
    fpcr |= (softfloat_rm_to_arm64_rm(get_float_rounding_mode(fp_status)) << 22) & 3;

    return fpcr | fpcr_others;
}
