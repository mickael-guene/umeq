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

#include <stdio.h>
#include <assert.h>
#include <math.h>

#include "arm64_private.h"
#include "arm64_helpers.h"
#include "runtime.h"
#include "softfloat.h"
#include "arm64_softfloat.h"

#define INSN(msb, lsb) ((insn >> (lsb)) & ((1 << ((msb) - (lsb) + 1))-1))
#define IEEE_H16        ((regs->fpcr_others & 0x04000000) == 0)

#include "arm64_helpers_simd_fpu_common.c"

static void dis_fcvt(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int type = INSN(23,22);
    int opc = INSN(16,15);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    union simd_register res = {0};

    if (type == 1 && opc == 0) {
        //fcvt Sd, Dn
        res.s[0] = float32_maybe_silence_nan(float64_to_float32(regs->v[rn].d[0], &regs->fp_status));
    } else if (type == 0 && opc == 1) {
        //fcvt Dd, Sn
        res.d[0] = float64_maybe_silence_nan(float32_to_float64(regs->v[rn].s[0], &regs->fp_status));
    } else if (type == 0 && opc ==3) {
        //fcvt Hd, Sn
        res.h[0] = float32_to_float16(regs->v[rn].s[0], IEEE_H16, &regs->fp_status);
        if (IEEE_H16)
            res.h[0] = float16_maybe_silence_nan(res.h[0]);
    } else if (type == 1 && opc ==3) {
        //fcvt Hd, Dn
        res.h[0] = float64_to_float16(regs->v[rn].d[0], IEEE_H16, &regs->fp_status);
        if (IEEE_H16)
            res.h[0] = float16_maybe_silence_nan(res.h[0]);
    } else if (type == 3 && opc == 0) {
        //fcvt Sd, Hn
        res.s[0] = float16_to_float32(regs->v[rn].h[0], IEEE_H16, &regs->fp_status);
        if (IEEE_H16)
            res.s[0] = float32_maybe_silence_nan(res.s[0]);
    }else if (type == 3 && opc == 1) {
        //fcvt Dd, Hn
        res.d[0] = float16_to_float64(regs->v[rn].h[0], IEEE_H16, &regs->fp_status);
        if (IEEE_H16)
            res.d[0] = float64_maybe_silence_nan(res.d[0]);
    } else
        fatal("type=%d / opc=%d\n", type, opc);
    regs->v[rd] = res;
}

static void dis_frint(uint64_t _regs, uint32_t insn, enum rm rmode)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int is_double = INSN(22,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    union simd_register res = {0};
    signed char float_rounding_mode_save = regs->fp_status.float_rounding_mode;
    int old_flags = get_float_exception_flags(&regs->fp_status);
    int new_flags;

    /* save rounding mode */
    regs->fp_status.float_rounding_mode = arm64_rm_to_softfloat_rm(rmode);
    /* do the convertion */
    if (is_double) {
        res.d[0] = float64_round_to_int(regs->v[rn].d[0], &regs->fp_status);
    } else {
        res.s[0] = float32_round_to_int(regs->v[rn].s[0], &regs->fp_status);
    }
    /* Suppress any inexact exceptions the conversion produced */
    if (!(old_flags & float_flag_inexact)) {
        new_flags = get_float_exception_flags(&regs->fp_status);
        set_float_exception_flags(new_flags & ~float_flag_inexact, &regs->fp_status);
    }
    /* restore rounding mode */
    regs->fp_status.float_rounding_mode = float_rounding_mode_save;

    regs->v[rd] = res;
}

static void dis_frintp(uint64_t _regs, uint32_t insn)
{
    dis_frint(_regs, insn, RM_POSINF);
}

static void dis_frintn(uint64_t _regs, uint32_t insn)
{
    dis_frint(_regs, insn, RM_TIEEVEN);
}

static void dis_frintm(uint64_t _regs, uint32_t insn)
{
    dis_frint(_regs, insn, RM_NEGINF);
}

static void dis_frintz(uint64_t _regs, uint32_t insn)
{
    dis_frint(_regs, insn, RM_ZERO);
}

static void dis_frinta(uint64_t _regs, uint32_t insn)
{
    dis_frint(_regs, insn, RM_TIEAWAY);
}

static void dis_frintx(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int is_double = INSN(22,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    union simd_register res = {0};

    /* do the convertion */
    if (is_double) {
        res.d[0] = float64_round_to_int(regs->v[rn].d[0], &regs->fp_status);
    } else {
        res.s[0] = float32_round_to_int(regs->v[rn].s[0], &regs->fp_status);
    }

    regs->v[rd] = res;
}

static void dis_frinti(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    enum rm rm = softfloat_rm_to_arm64_rm(get_float_rounding_mode(&regs->fp_status));

    dis_frint(_regs, insn, rm);
}

static void dis_fccmp_fccmpe(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int is_double = INSN(22,22);
    int rn = INSN(9,5);
    int rm = INSN(20,16);
    int cond = INSN(15,12);
    int nzcv = INSN(3,0);
    int is_quiet = (INSN(4, 4) == 0);

    if (arm64_hlp_compute_flags_pred(_regs, cond, regs->nzcv)) {
        if (is_double)
            nzcv = fcmp64(regs, regs->v[rn].d[0], regs->v[rm].d[0], is_quiet);
        else
            nzcv = fcmp32(regs, regs->v[rn].s[0], regs->v[rm].s[0], is_quiet);
    }
    regs->nzcv = (nzcv << 28) | (regs->nzcv & 0x0fffffff);
}

static void dis_fsqrt(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int is_scalar = INSN(28,28);
    int is_double = INSN(22,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    union simd_register res = {0};
    int i;

    assert(is_scalar);
    if (is_double)
        for(i = 0; i < (is_scalar?1:2); i++)
            res.d[i] = fsqrt64(regs, regs->v[rn].d[i]);
    else
        for(i = 0; i < (is_scalar?1:(q?4:2)); i++)
            res.s[i] = fsqrt32(regs, regs->v[rn].s[i]);

    regs->v[rd] = res;
}

/* deasm */
void arm64_hlp_dirty_scvtf_scalar_integer_simd(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int sf_type0 = (INSN(31,31) << 1) | INSN(22,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);

    regs->v[rd].d[1] = 0;
    switch(sf_type0) {
        case 0:
            regs->v[rd].s[0] = float32_val(int32_to_float32(regs->r[rn], &regs->fp_status));
            regs->v[rd].s[1] = 0;
            break;
        case 1:
            regs->v[rd].d[0] = float64_val(int32_to_float64(regs->r[rn], &regs->fp_status));
            break;
        case 2:
            regs->v[rd].s[0] = float32_val(int64_to_float32(regs->r[rn], &regs->fp_status));
            regs->v[rd].s[1] = 0;
            break;
        case 3:
            regs->v[rd].d[0] = float64_val(int64_to_float64(regs->r[rn], &regs->fp_status));
            break;
        default:
            fatal("sf_type0 = %d\n", sf_type0);
    }
}

void arm64_hlp_dirty_floating_point_compare_simd(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int is_double = INSN(22,22);
    int is_compare_zero = INSN(3,3);
    int is_quiet = (INSN(4,4) == 0);
    int rm = INSN(20,16);
    int rn = INSN(9,5);

    if (is_double)
        regs->nzcv = fcmp64(regs, regs->v[rn].d[0], is_compare_zero?0:regs->v[rm].d[0], is_quiet) << 28;
    else
        regs->nzcv = fcmp32(regs, regs->v[rn].s[0], is_compare_zero?0:regs->v[rm].s[0], is_quiet) << 28;
}

void arm64_hlp_dirty_floating_point_data_processing_2_source_simd(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int is_double = INSN(22,22);
    int opcode = INSN(15,12);
    int rm = INSN(20,16);
    int rn = INSN(9,5);
    int rd = INSN(4,0);
    union simd_register res = {0};

    if (is_double) {
        switch(opcode) {
            case 0://fmul
                res.d[0] = fmul64(regs, regs->v[rn].d[0], regs->v[rm].d[0]);
                break;
            case 1://fdiv
                res.d[0] = fdiv64(regs, regs->v[rn].d[0], regs->v[rm].d[0]);
                break;
            case 2://fadd
                res.d[0] = fadd64(regs, regs->v[rn].d[0], regs->v[rm].d[0]);
                break;
            case 3://fsub
                res.d[0] = fsub64(regs, regs->v[rn].d[0], regs->v[rm].d[0]);
                break;
            case 4://fmax
                res.d[0] = fmax64(regs, regs->v[rn].d[0], regs->v[rm].d[0]);
                break;
            case 6://fmaxnm
                res.d[0] = fmaxnm64(regs, regs->v[rn].d[0],regs->v[rm].d[0]);
                break;
            case 5://fmin
                res.d[0] = fmin64(regs, regs->v[rn].d[0], regs->v[rm].d[0]);
                break;
            case 7://fminnm
                res.d[0] = fminnm64(regs, regs->v[rn].d[0],regs->v[rm].d[0]);
                break;
            case 8://fnmul
                res.d[0] = fneg64(regs, fmul64(regs, regs->v[rn].d[0], regs->v[rm].d[0]));
                break;
            default:
                fatal("opcode = %d(0x%x)\n", opcode, opcode);
        }
    } else {
        switch(opcode) {
            case 0://fmul
                res.s[0] = fmul32(regs, regs->v[rn].s[0], regs->v[rm].s[0]);
                break;
            case 1://fdiv
                res.s[0] = fdiv32(regs, regs->v[rn].s[0], regs->v[rm].s[0]);
                break;
            case 2://fadd
                res.s[0] = fadd32(regs, regs->v[rn].s[0], regs->v[rm].s[0]);
                break;
            case 3://fsub
                res.s[0] = fsub32(regs, regs->v[rn].s[0], regs->v[rm].s[0]);
                break;
            case 4://fmax
                res.s[0] = fmax32(regs, regs->v[rn].s[0], regs->v[rm].s[0]);
                break;
            case 6://fmaxnm
                res.s[0] = fmaxnm32(regs, regs->v[rn].s[0],regs->v[rm].s[0]);
                break;
            case 5://fmin
                res.s[0] = fmin32(regs, regs->v[rn].s[0], regs->v[rm].s[0]);
                break;
            case 7://fminnm
                res.s[0] = fminnm32(regs, regs->v[rn].s[0],regs->v[rm].s[0]);
                break;
            case 8://fnmul
                res.s[0] = fneg32(regs, fmul32(regs, regs->v[rn].s[0], regs->v[rm].s[0]));
                break;
            default:
                fatal("opcode = %d(0x%x)\n", opcode, opcode);
        }
    }
    regs->v[rd] = res;
}

void arm64_hlp_dirty_floating_point_immediate_simd(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int is_double = INSN(22,22);
    int rd = INSN(4,0);
    int imm8 = INSN(20,13);

    if (is_double) {
        uint64_t res;
        uint64_t imm = imm8;
        uint64_t s = imm >> 7;
        uint64_t i6 = (imm >> 6) & 1;
        uint64_t not_i6 = 1 - i6;
        uint64_t i6_0 = imm & 0x7f;

        res = (s << 63) | (not_i6 << 62) | (i6 << 61) | (i6 << 60) | (i6 << 59) | (i6 << 58) |
              (i6 << 57) | (i6 << 56) | (i6 << 55) | (i6_0 << 48);
        regs->v[rd].d[0] = res;
        regs->v[rd].d[1] = 0;
    } else {
        uint32_t res;
        uint32_t imm = imm8;
        uint32_t s = imm >> 7;
        uint32_t i6 = (imm >> 6) & 1;
        uint32_t not_i6 = 1 - i6;
        uint32_t i6_0 = imm & 0x7f;

        res = (s << 31) | (not_i6 << 30) | (i6 << 29) | (i6 << 28) | (i6 << 27) | (i6 << 26) | (i6_0 << 19);
        regs->v[rd].s[0] = res;
        regs->v[rd].s[1] = 0;
        regs->v[rd].d[1] = 0;
    }
}

void arm64_hlp_dirty_floating_point_conditional_select_simd(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int is_double = INSN(22,22);
    int cond = INSN(15,12);
    int rm = INSN(20,16);
    int rn = INSN(9,5);
    int rd = INSN(4,0);

    if (arm64_hlp_compute_flags_pred(_regs, cond, regs->nzcv)) {
        //d <= n
        regs->v[rd].d[1] = 0;
        if (is_double)
            regs->v[rd].d[0] = regs->v[rn].d[0];
        else {
            regs->v[rd].s[0] = regs->v[rn].s[0];
            regs->v[rd].s[1] = 0;
        }
    } else {
        //d <= m
        regs->v[rd].d[1] = 0;
        if (is_double)
            regs->v[rd].d[0] = regs->v[rm].d[0];
        else {
            regs->v[rd].s[0] = regs->v[rm].s[0];
            regs->v[rd].s[1] = 0;
        }
    }
}

void arm64_hlp_dirty_floating_point_data_processing_3_source_simd(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int is_double = INSN(22,22);
    int o1_o0 = (INSN(21,21) << 1) | INSN(15,15);
    int rm = INSN(20,16);
    int ra = INSN(14,10);
    int rn = INSN(9,5);
    int rd = INSN(4,0);
    union simd_register res = {0};

    if (is_double) {
        switch(o1_o0) {
            case 0://fmadd
                res.d[0] = fmadd64(regs, regs->v[rn].d[0], regs->v[rm].d[0], regs->v[ra].d[0]);
                break;
            case 1://fmsub
                res.d[0] = fmadd64(regs, fneg64(regs, regs->v[rn].d[0]), regs->v[rm].d[0], regs->v[ra].d[0]);
                break;
            case 2://fnmadd
                res.d[0] = fmadd64(regs, fneg64(regs, regs->v[rn].d[0]), regs->v[rm].d[0], fneg64(regs, regs->v[ra].d[0]));
                break;
            case 3://fnmsub
                res.d[0] = fmadd64(regs, regs->v[rn].d[0], regs->v[rm].d[0], fneg64(regs, regs->v[ra].d[0]));
                break;
            default:
                fatal("o1_o0 = %d(0x%x)\n", o1_o0, o1_o0);
        }
    } else {
        switch(o1_o0) {
            case 0://fmadd
                res.s[0] = fmadd32(regs, regs->v[rn].s[0], regs->v[rm].s[0], regs->v[ra].s[0]);
                break;
            case 1://fmsub
                res.s[0] = fmadd32(regs, fneg32(regs, regs->v[rn].s[0]), regs->v[rm].s[0], regs->v[ra].s[0]);
                break;
            case 2://fnmadd
                res.s[0] = fmadd32(regs, fneg32(regs, regs->v[rn].s[0]), regs->v[rm].s[0], fneg32(regs, regs->v[ra].s[0]));
                break;
            case 3://fnmsub
                res.s[0] = fmadd32(regs, regs->v[rn].s[0], regs->v[rm].s[0], fneg32(regs, regs->v[ra].s[0]));
                break;
            default:
                fatal("o1_o0 = %d(0x%x)\n", o1_o0, o1_o0);
        }
    }
    regs->v[rd] = res;
}

void arm64_hlp_dirty_ucvtf_scalar_integer_simd(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int sf_type0 = (INSN(31,31) << 1) | INSN(22,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);

    regs->v[rd].d[1] = 0;
    switch(sf_type0) {
        case 0:
            regs->v[rd].s[0] = float32_val(uint32_to_float32(regs->r[rn], &regs->fp_status));
            regs->v[rd].s[1] = 0;
            break;
        case 1:
            regs->v[rd].d[0] = float64_val(uint32_to_float64(regs->r[rn], &regs->fp_status));
            break;
        case 2:
            regs->v[rd].s[0] = float32_val(uint64_to_float32(regs->r[rn], &regs->fp_status));
            regs->v[rd].s[1] = 0;
            break;
        case 3:
            regs->v[rd].d[0] = float64_val(uint64_to_float64(regs->r[rn], &regs->fp_status));
            break;
        default:
            fatal("sf_type0 = %d\n", sf_type0);
    }
}

static void dis_fcvtu_scalar_integer(uint64_t _regs, uint32_t insn, enum rm rmode)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int sf_type0 = (INSN(31,31) << 1) | INSN(22,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    signed char float_rounding_mode_save = regs->fp_status.float_rounding_mode;

    regs->fp_status.float_rounding_mode = arm64_rm_to_softfloat_rm(rmode);
    regs->v[rd].d[1] = 0;
    switch(sf_type0) {
        case 0:
            if (float32_is_any_nan(regs->v[rn].s[0])) {
                float_raise(float_flag_invalid, &regs->fp_status);
                regs->r[rd] = 0;
            } else
                regs->r[rd] = (uint32_t)float32_to_uint32(regs->v[rn].s[0], &regs->fp_status);
            break;
        case 1:
            if (float64_is_any_nan(regs->v[rn].d[0])) {
                float_raise(float_flag_invalid, &regs->fp_status);
                regs->r[rd] = 0;
            } else
                regs->r[rd] = (uint32_t)float64_to_uint32(regs->v[rn].d[0], &regs->fp_status);
            break;
        case 2:
            if (float32_is_any_nan(regs->v[rn].s[0])) {
                float_raise(float_flag_invalid, &regs->fp_status);
                regs->r[rd] = 0;
            } else
                regs->r[rd] = float32_to_uint64(regs->v[rn].s[0], &regs->fp_status);
            break;
        case 3:
            if (float64_is_any_nan(regs->v[rn].d[0])) {
                float_raise(float_flag_invalid, &regs->fp_status);
                regs->r[rd] = 0;
            } else
                regs->r[rd] = float64_to_uint64(regs->v[rn].d[0], &regs->fp_status);
            break;
        default:
            fatal("sf_type0 = %d\n", sf_type0);
    }
    regs->fp_status.float_rounding_mode = float_rounding_mode_save;
}
static void dis_fcvts_scalar_integer(uint64_t _regs, uint32_t insn, enum rm rmode)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int sf_type0 = (INSN(31,31) << 1) | INSN(22,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    signed char float_rounding_mode_save = regs->fp_status.float_rounding_mode;

    regs->fp_status.float_rounding_mode = arm64_rm_to_softfloat_rm(rmode);
    regs->v[rd].d[1] = 0;
    switch(sf_type0) {
        case 0:
            if (float32_is_any_nan(regs->v[rn].s[0])) {
                float_raise(float_flag_invalid, &regs->fp_status);
                regs->r[rd] = 0;
            } else
                regs->r[rd] = (uint32_t)float32_to_int32(regs->v[rn].s[0], &regs->fp_status);
            break;
        case 1:
            if (float64_is_any_nan(regs->v[rn].d[0])) {
                float_raise(float_flag_invalid, &regs->fp_status);
                regs->r[rd] = 0;
            } else
                regs->r[rd] = (uint32_t)float64_to_int32(regs->v[rn].d[0], &regs->fp_status);
            break;
        case 2:
            if (float32_is_any_nan(regs->v[rn].s[0])) {
                float_raise(float_flag_invalid, &regs->fp_status);
                regs->r[rd] = 0;
            } else
                regs->r[rd] = float32_to_int64(regs->v[rn].s[0], &regs->fp_status);
            break;
        case 3:
            if (float64_is_any_nan(regs->v[rn].d[0])) {
                float_raise(float_flag_invalid, &regs->fp_status);
                regs->r[rd] = 0;
            } else
                regs->r[rd] = float64_to_int64(regs->v[rn].d[0], &regs->fp_status);
            break;
        default:
            fatal("sf_type0 = %d\n", sf_type0);
    }
    regs->fp_status.float_rounding_mode = float_rounding_mode_save;
}

static void dis_fcvtau_scalar_integer(uint64_t _regs, uint32_t insn)
{
    dis_fcvtu_scalar_integer(_regs, insn, RM_TIEAWAY);
}

static void dis_fcvtas_scalar_integer(uint64_t _regs, uint32_t insn)
{
    dis_fcvts_scalar_integer(_regs, insn, RM_TIEAWAY);
}

static void dis_fcvtmu_scalar_integer(uint64_t _regs, uint32_t insn)
{
    dis_fcvtu_scalar_integer(_regs, insn, RM_NEGINF);
}

static void dis_fcvtms_scalar_integer(uint64_t _regs, uint32_t insn)
{
    dis_fcvts_scalar_integer(_regs, insn, RM_NEGINF);
}

static void dis_fcvtnu_scalar_integer(uint64_t _regs, uint32_t insn)
{
    dis_fcvtu_scalar_integer(_regs, insn, RM_TIEEVEN);
}

static void dis_fcvtns_scalar_integer(uint64_t _regs, uint32_t insn)
{
    dis_fcvts_scalar_integer(_regs, insn, RM_TIEEVEN);
}

static void dis_fcvtpu_scalar_integer(uint64_t _regs, uint32_t insn)
{
    dis_fcvtu_scalar_integer(_regs, insn, RM_POSINF);
}

static void dis_fcvtps_scalar_integer(uint64_t _regs, uint32_t insn)
{
    dis_fcvts_scalar_integer(_regs, insn, RM_POSINF);
}

static void dis_fcvtzu_scalar_integer(uint64_t _regs, uint32_t insn)
{
    dis_fcvtu_scalar_integer(_regs, insn, RM_ZERO);
}

static void dis_fcvtzs_scalar_integer(uint64_t _regs, uint32_t insn)
{
    dis_fcvts_scalar_integer(_regs, insn, RM_ZERO);
}

static void dis_fcvtzs_fixed(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int sf_type0 = (INSN(31,31) << 1) | INSN(22,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int fracbits = 64 - INSN(15,10);

    switch(sf_type0) {
        case 0:
            //Wd, Sn
            regs->r[rd] = fcvtzs32_fixed_32(regs, regs->v[rn].s[0], fracbits);
            break;
        case 1:
            //Wd, Dn
            regs->r[rd] = fcvtzs64_fixed_32(regs, regs->v[rn].d[0], fracbits);
            break;
        case 2:
            //Xd, Sn
            regs->r[rd] = fcvtzs32_fixed_64(regs, regs->v[rn].s[0], fracbits);
            break;
        case 3:
            //Xd, Dn
            regs->r[rd] = fcvtzs64_fixed_64(regs, regs->v[rn].d[0], fracbits);
            break;
        default:
            fatal("sf_type0=%d\n", sf_type0);
    }
}

static void dis_fcvtzu_fixed(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int sf_type0 = (INSN(31,31) << 1) | INSN(22,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int fracbits = 64 - INSN(15,10);

    switch(sf_type0) {
        case 0:
            //Wd, Sn
            regs->r[rd] = fcvtzu32_fixed_32(regs, regs->v[rn].s[0], fracbits);
            break;
        case 1:
            //Wd, Dn
            regs->r[rd] = fcvtzu64_fixed_32(regs, regs->v[rn].d[0], fracbits);
            break;
        case 2:
            //Xd, Sn
            regs->r[rd] = fcvtzu32_fixed_64(regs, regs->v[rn].s[0], fracbits);
            break;
        case 3:
            //Xd, Dn
            regs->r[rd] = fcvtzu64_fixed_64(regs, regs->v[rn].d[0], fracbits);
            break;
    }
}

static void dis_scvtf_fixed(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int sf_type0 = (INSN(31,31) << 1) | INSN(22,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int fracbits = 64 - INSN(15,10);
    union simd_register res = {0};

    switch(sf_type0) {
        case 0:
            //Sd, Wn
            res.s[0] = float32_val(int32_to_float32(regs->r[rn], &regs->fp_status));
            res.s[0] = float32_scalbn(res.s[0], -fracbits, &regs->fp_status);
            break;
        case 1:
            //Dd, Wn
            res.d[0] = float64_val(int32_to_float64(regs->r[rn], &regs->fp_status));
            res.d[0] = float64_scalbn(res.d[0], -fracbits, &regs->fp_status);
            break;
        case 2:
            //Sd, Xn
            res.s[0] = float32_val(int64_to_float32(regs->r[rn], &regs->fp_status));
            res.s[0] = float32_scalbn(res.s[0], -fracbits, &regs->fp_status);
            break;
        case 3:
            //Dd, Xn
            res.d[0] = float64_val(int64_to_float64(regs->r[rn], &regs->fp_status));
            res.d[0] = float64_scalbn(res.d[0], -fracbits, &regs->fp_status);
            break;
        default:
            fatal("sf_type0=%d\n", sf_type0);
    }
    regs->v[rd] = res;
}

static void dis_ucvtf_fixed(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int sf_type0 = (INSN(31,31) << 1) | INSN(22,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int fracbits = 64 - INSN(15,10);
    union simd_register res = {0};

    switch(sf_type0) {
        case 0:
            //Sd, Wn
            res.s[0] = float32_val(uint32_to_float32(regs->r[rn], &regs->fp_status));
            res.s[0] = float32_scalbn(res.s[0], -fracbits, &regs->fp_status);
            break;
        case 1:
            //Dd, Wn
            res.d[0] = float64_val(uint32_to_float64(regs->r[rn], &regs->fp_status));
            res.d[0] = float64_scalbn(res.d[0], -fracbits, &regs->fp_status);
            break;
        case 2:
            //Sd, Xn
            res.s[0] = float32_val(uint64_to_float32(regs->r[rn], &regs->fp_status));
            res.s[0] = float32_scalbn(res.s[0], -fracbits, &regs->fp_status);
            break;
        case 3:
            //Dd, Xn
            res.d[0] = float64_val(uint64_to_float64(regs->r[rn], &regs->fp_status));
            res.d[0] = float64_scalbn(res.d[0], -fracbits, &regs->fp_status);
            break;
        default:
            fatal("sf_type0=%d\n", sf_type0);
    }
    regs->v[rd] = res;
}

void arm64_hlp_dirty_fcvtxx_scalar_integer_simd(uint64_t _regs, uint32_t insn)
{
    int sf = INSN(31,31);
    int type = INSN(23,22);
    int rmode = INSN(20,19);
    int opcode = INSN(18,16);

    if ((type & 2) == 0 && rmode == 0 && opcode == 5)
        dis_fcvtau_scalar_integer(_regs, insn);
    else if ((type & 2) == 0 && rmode == 2 && opcode == 1)
        dis_fcvtmu_scalar_integer(_regs, insn);
    else if ((type & 2) == 0 && rmode == 0 && opcode == 0)
        dis_fcvtns_scalar_integer(_regs, insn);
    else if ((type & 2) == 0 && rmode == 0 && opcode == 1)
        dis_fcvtnu_scalar_integer(_regs, insn);
    else if ((type & 2) == 0 && rmode == 1 && opcode == 1)
        dis_fcvtpu_scalar_integer(_regs, insn);
    else if ((type & 2) == 0 && rmode == 3 && opcode == 0)
        dis_fcvtzs_scalar_integer(_regs, insn);
    else if ((type & 2) == 0 && rmode == 3 && opcode == 1)
        dis_fcvtzu_scalar_integer(_regs, insn);
    else if ((type & 2) == 0 && rmode == 2 && opcode == 0)
        dis_fcvtms_scalar_integer(_regs, insn);
    else if ((type & 2) == 0 && rmode == 1 && opcode == 0)
        dis_fcvtps_scalar_integer(_regs, insn);
    else if ((type & 2) == 0 && rmode == 0 && opcode == 4)
        dis_fcvtas_scalar_integer(_regs, insn);
    else
        fatal("sf=%d / type=%d / rmode=%d / opcode=%d\n", sf, type, rmode, opcode);
}

void arm64_hlp_dirty_floating_point_data_processing_1_source(uint64_t _regs, uint32_t insn)
{
    int opcode = INSN(20,15);

    switch(opcode) {
        case 1:
            dis_fabs(_regs, insn);
            break;
        case 2:
            dis_fneg(_regs, insn);
            break;
        case 3:
            dis_fsqrt(_regs, insn);
            break;
        case 4: case 5: case 7:
            dis_fcvt(_regs, insn);
            break;
        case 8:
            dis_frintn(_regs, insn);
            break;
        case 9:
            dis_frintp(_regs, insn);
            break;
        case 10:
            dis_frintm(_regs, insn);
            break;
        case 11:
            dis_frintz(_regs, insn);
            break;
        case 12:
            dis_frinta(_regs, insn);
            break;
        case 14:
            dis_frintx(_regs, insn);
            break;
        case 15:
            dis_frinti(_regs, insn);
            break;
        default:
            fatal("opcode = %d/0x%x\n", opcode, opcode);
    }
}

void arm64_hlp_dirty_floating_point_conditional_compare(uint64_t _regs, uint32_t insn)
{
    dis_fccmp_fccmpe(_regs, insn);
}

void arm64_hlp_dirty_conversion_between_floating_point_and_fixed_point(uint64_t _regs, uint32_t insn)
{
    int type0 = INSN(22,22);
    int rmode = INSN(20,19);
    int opcode = INSN(18,16);

    switch(opcode) {
        case 0:
            dis_fcvtzs_fixed(_regs, insn);
            break;
        case 1:
            dis_fcvtzu_fixed(_regs, insn);
            break;
        case 2:
            dis_scvtf_fixed(_regs, insn);
            break;
        case 3:
            dis_ucvtf_fixed(_regs, insn);
            break;
        default:
            fatal("type0=%d / rmode=%d / opcode=%d\n", type0, rmode, opcode);
    }
}

/* Convert between softfloat and arm fpcr and fpsr registers
*/
void arm64_hlp_write_fpsr(uint64_t _regs, uint32_t value)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int exceptions = 0;

    /* qc are handle out of softfloat */
    regs->qc = value & 0x08000000;
    /* now handle exceptions bits */
    if (value & ARM64_FPSR_IDC)
        exceptions |= float_flag_input_denormal;
    if (value & ARM64_FPSR_IXC)
        exceptions |= float_flag_inexact;
    if (value & ARM64_FPSR_UFC)
        exceptions |= float_flag_underflow;
    if (value & ARM64_FPSR_OFC)
        exceptions |= float_flag_overflow;
    if (value & ARM64_FPSR_DZC)
        exceptions |= float_flag_divbyzero;
    if (value & ARM64_FPSR_IOC)
        exceptions |= float_flag_invalid;
    /* setup softfloat state */
    set_float_exception_flags(exceptions, &regs->fp_status);
    /* clear host state */
    feclearexcept(FE_ALL_EXCEPT);
}

uint32_t arm64_hlp_read_fpsr(uint64_t _regs)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int host_exceptions = fetestexcept(FE_ALL_EXCEPT);
    uint32_t arm64_exceptions_from_host = 0;

    /* get host state */
    arm64_exceptions_from_host |= (host_exceptions&FE_DIVBYZERO)?ARM64_FPSR_DZC:0;
    arm64_exceptions_from_host |= (host_exceptions&FE_INEXACT)?ARM64_FPSR_IXC:0;
    arm64_exceptions_from_host |= (host_exceptions&FE_INVALID)?ARM64_FPSR_IOC:0;
    arm64_exceptions_from_host |= (host_exceptions&FE_OVERFLOW)?ARM64_FPSR_OFC:0;
    arm64_exceptions_from_host |= (host_exceptions&FE_UNDERFLOW)?ARM64_FPSR_UFC:0;

    /* and add it to softfloat state */
    return softfloat_to_arm64_fpsr(&regs->fp_status, regs->qc) | arm64_exceptions_from_host;
}

void arm64_hlp_write_fpcr(uint64_t _regs, uint32_t value)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;

    set_default_nan_mode((value>>25)&1, &regs->fp_status);
    set_flush_to_zero((value>>24)&1, &regs->fp_status);
    set_flush_inputs_to_zero((value>>24)&1, &regs->fp_status);
    set_float_rounding_mode(arm64_rm_to_softfloat_rm((value>>22)&3), &regs->fp_status);
    /* update fast_math_is_allow */
    if (get_default_nan_mode(&regs->fp_status) ||
        get_flush_to_zero(&regs->fp_status) ||
        get_float_rounding_mode(&regs->fp_status) != float_round_nearest_even)
        regs->fast_math_is_allow = 0;
    else
        regs->fast_math_is_allow = 1;
    /* save AHP, IDE, IXE, UFE, OFE, DZE, IOE in fpcr_others */
    regs->fpcr_others = value & 0x04009f00;
}

uint32_t arm64_hlp_read_fpcr(uint64_t _regs)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;

    return softfloat_to_arm64_cpsr(&regs->fp_status, regs->fpcr_others);
}
