/* This file is part of Umeq, an equivalent of qemu user mode emulation with improved robustness.
 *
 * Copyright (C) 2016 STMicroelectronics
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
#include "arm_softfloat.h"

static int softfloat_rm_to_arm_rm(int softfloat_rm)
{
    switch(softfloat_rm) {
        case float_round_nearest_even:
            return 0;
            break;
        case float_round_down:
            return 2;
            break;
        case float_round_up:
            return 1;
            break;
        case float_round_to_zero:
            return 3;
            break;
        default:
            assert(0);
    }
}

static int arm_rm_to_softfloat_rm(int arm_rm)
{
    switch(arm_rm) {
        case 0:
            return float_round_nearest_even;
            break;
        case 1:
            return float_round_up;
            break;
        case 2:
            return float_round_down;
            break;
        case 3:
            return float_round_to_zero;
            break;
        default:
            assert(0);
    }
}

void arm_fpscr_to_softfloat(uint32_t value, float_status *fp_status, float_status *fp_status_simd)
{
    int exceptions = 0;

    set_default_nan_mode((value>>25)&1, fp_status);
    set_flush_to_zero((value>>24)&1, fp_status);
    set_flush_inputs_to_zero((value>>24)&1, fp_status);
    set_float_rounding_mode(arm_rm_to_softfloat_rm((value>>22)&3), fp_status);
    if (value & (1 << 7))
        exceptions |= float_flag_input_denormal;
    if (value & (1 << 4))
        exceptions |= float_flag_inexact;
    if (value & (1 << 3))
        exceptions |= float_flag_underflow;
    if (value & (1 << 2))
        exceptions |= float_flag_overflow;
    if (value & (1 << 1))
        exceptions |= float_flag_divbyzero;
    if (value & (1 << 0))
        exceptions |= float_flag_invalid;
    set_float_exception_flags(exceptions, fp_status);

    /* regs->fp_status_simd update */
    set_float_exception_flags(0, fp_status_simd);
}

/* FIXME: masking fp_status with exceptions flag enables ? */
uint32_t softfloat_to_arm_fpscr(uint32_t fpscr, float_status *fp_status, float_status *fp_status_simd)
{
    int exceptions = get_float_exception_flags(fp_status) |
                     get_float_exception_flags(fp_status_simd);

    /* DN */
    if (get_default_nan_mode(fp_status))
        fpscr |= 1 << 25;
    /* FZ */
    if (get_flush_to_zero(fp_status))
        fpscr |= 1 << 24;
    /* Rmode */
    fpscr |= softfloat_rm_to_arm_rm(get_float_rounding_mode(fp_status)) << 22;

    /* set up exceptions bits */
    if (exceptions & float_flag_input_denormal)
        fpscr |= 1 << 7;
    if (exceptions & float_flag_inexact)
        fpscr |= 1 << 4;
    if (exceptions & (float_flag_underflow | float_flag_output_denormal))
        fpscr |= 1 << 3;
    if (exceptions & float_flag_overflow)
        fpscr |= 1 << 2;
    if (exceptions & float_flag_divbyzero)
        fpscr |= 1 << 1;
    if (exceptions & float_flag_invalid)
        fpscr |= 1 << 0;

    return fpscr;
}
