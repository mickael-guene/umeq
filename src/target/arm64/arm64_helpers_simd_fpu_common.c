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

#include <fenv.h>

static inline float maxf(float a, float b)
{
    return a>=b?a:b;
}
static inline double maxd(double a, double b)
{
    return a>=b?a:b;
}
static inline float minf(float a, float b)
{
    return a<b?a:b;
}
static inline double mind(double a, double b)
{
    return a<b?a:b;
}
static inline float maxnmf(float a, float b)
{
    if (isnan(a))
        return b;
    else if (isnan(b))
        return a;
    else
        return a>=b?a:b;
}
static inline double maxnmd(double a, double b)
{
    if (isnan(a))
        return b;
    else if (isnan(b))
        return a;
    else
        return a>=b?a:b;
}
static inline float minnmf(float a, float b)
{
    if (isnan(a))
        return b;
    else if (isnan(b))
        return a;
    else
        return a<b?a:b;
}
static inline double minnmd(double a, double b)
{
    if (isnan(a))
        return b;
    else if (isnan(b))
        return a;
    else
        return a<b?a:b;
}

static double fcvt_rm(double a, enum rm rmode)
{
    double int_result = floor(a);
    double error = a - int_result;

    switch(rmode) {
        case RM_TIEEVEN:
            if (error > 0.5 || (error == 0.5 && (((uint64_t)int_result) % 2 == 1)))
                int_result += 1;
                break;
        case RM_POSINF:
            if (error != 0.0)
                int_result += 1;
                break;
        case RM_NEGINF:
            break;
        case RM_ZERO:
            if (error != 0.0 && int_result < 0)
                int_result += 1;
                break;
        case RM_TIEAWAY:
            if (error > 0.5 || (error == 0.5 && int_result >= 0))
                int_result += 1;
                break;
        default:
            fatal("rmode = %d\n", rmode);
    }

    return int_result;
}

static double fcvt_fract(double a, int fracbits)
{
    double value = a * (1UL << fracbits);

    return fcvt_rm(value, RM_ZERO);
}

static double ssat64_d(double a)
{
    if (a > 0x7fffffffffffffffUL)
        return 0x7fffffffffffffffUL;
    else if (a < (int64_t)0x8000000000000000UL)
        return (int64_t)0x8000000000000000UL;
    return a;
}

static double ssat32_d(double a)
{
    if (a > 0x7fffffff)
        return 0x7fffffff;
    else if (a < (int32_t)0x80000000)
        return (int32_t)0x80000000;
    return a;
}

static double usat64_d(double a)
{
    if (a > 0xffffffffffffffffUL)
        return 0xffffffffffffffffUL;
    else if (a < 0)
        return 0;
    return a;
}

static double usat32_d(double a)
{
    if (a > 0xffffffff)
        return 0xffffffff;
    else if (a < 0)
        return 0;
    return a;
}

static void dis_fabs(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int is_scalar = INSN(28,28);
    int size = INSN(23,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    union simd_register res = {0};
    int i;

    switch(size) {
        case 0: case 2:
            for(i = 0; i < (is_scalar?1:(q?4:2)); i++)
                res.s[i] = regs->v[rn].s[i]&0x80000000?regs->v[rn].s[i]^0x80000000:regs->v[rn].s[i];
            break;
        case 1: case 3:
            for(i = 0; i < (is_scalar?1:(q?2:1)); i++)
                res.d[i] = regs->v[rn].d[i]&0x8000000000000000UL?regs->v[rn].d[i]^0x8000000000000000UL:regs->v[rn].d[i];
            break;   
        default:
            assert(0);
    }

    regs->v[rd] = res;
}

static void dis_fneg(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int is_scalar = INSN(28,28);
    int is_double = INSN(22,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    union simd_register res = {0};
    int i;

    if (is_double) {
        for(i = 0; i < (is_scalar?1:2); i++)
            res.df[i] = -regs->v[rn].df[i];
    } else {
        for(i = 0; i < (is_scalar?1:(q?4:2)); i++)
            res.sf[i] = -regs->v[rn].sf[i];
    }
    regs->v[rd] = res;
}
