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

#include <stdint.h>
#include <math.h>
#include <fenv.h>
/* FIXME: remove assert code */
#include <assert.h>

union double_unpack {
    double df;
    uint64_t d;
};

union float_unpack {
    float sf;
    uint32_t s;
};

double floor(double x)
{
#if 1
    assert(0 && "implement me");
#else
    double res;

    asm volatile("roundsd $1, %[x], %[res]"
                 : [res] "=x" (res)
                 : [x] "xm" (x)
                 :);

    return res;
#endif
}

double sqrt(double x)
{
#if 1
    assert(0 && "implement me");
#else
    double res;

    asm volatile("sqrtsd %[x], %[res]"
                 : [res] "=x" (res)
                 : [x] "xm" (x)
                 :);

    return res;
#endif
}

float sqrtf(float x)
{
#if 1
    assert(0 && "implement me");
#else
    float res;

    asm volatile("sqrtss %[x], %[res]"
                 : [res] "=x" (res)
                 : [x] "xm" (x)
                 :);

    return res;
#endif
}

int __isnan(double x)
{
    union double_unpack data = {x};
    uint64_t d = data.d;
    uint64_t exponent = d & 0x7ff0000000000000UL;
    uint64_t mantissa = d & 0x000fffffffffffffUL;

    return (exponent == 0x7ff0000000000000UL) && (mantissa != 0);
}

int __isnanf(float x)
{
    union float_unpack data = {x};
    uint32_t s = data.s;
    uint32_t exponent = s & 0x7f800000;
    uint32_t mantissa = s & 0x007fffff;

    return (exponent == 0x7f800000) && (mantissa != 0);
}

int __fpclassify(double x)
{
    union double_unpack data = {x};
    uint64_t d = data.d;
    uint64_t exponent = d & 0x7ff0000000000000UL;
    uint64_t mantissa = d & 0x000fffffffffffffUL;

    if (exponent == 0x7ff0000000000000UL && mantissa)
        return FP_NAN;
    else if (exponent == 0x7ff0000000000000UL && !mantissa)
        return FP_INFINITE;
    else if (!exponent && mantissa)
        return FP_SUBNORMAL;
    else if (!exponent && !mantissa)
        return FP_ZERO;
    else
        return FP_NORMAL;
}

int __fpclassifyf(float x)
{
    union float_unpack data = {x};
    uint32_t s = data.s;
    uint32_t exponent = s & 0x7f800000;
    uint32_t mantissa = s & 0x007fffff;

    if (exponent == 0x7f800000 && mantissa)
        return FP_NAN;
    else if (exponent == 0x7f800000 && !mantissa)
        return FP_INFINITE;
    else if (!exponent && mantissa)
        return FP_SUBNORMAL;
    else if (!exponent && !mantissa)
        return FP_ZERO;
    else
        return FP_NORMAL;
}

int feraiseexcept(int excepts)
{
    unsigned int mxcsr;

    excepts &= FE_ALL_EXCEPT;
    asm volatile("stmxcsr %[mxcsr]\n\t"
                 : [mxcsr] "=m" (*&mxcsr)
                 :
                 :);
    mxcsr |= excepts;
    asm volatile("ldmxcsr %[mxcsr]\n\t"
                 :
                 : [mxcsr] "m" (*&mxcsr)
                 :);
    return 0;
}

int feclearexcept(int excepts)
{
    fenv_t fenv;
    unsigned int mxcsr;

    excepts &= FE_ALL_EXCEPT;
    asm volatile("fnstenv %[fenv]"
                 : [fenv] "=m" (*&fenv)
                 :
                 :);
    fenv.__status_word &= excepts ^ FE_ALL_EXCEPT;
    asm volatile("fldenv %[fenv]\n\t"
                 "stmxcsr %[mxcsr]\n\t"
                 : [mxcsr] "=m" (*&mxcsr)
                 : [fenv] "m" (*&fenv)
                 :);
    mxcsr &= ~excepts;
    asm volatile("ldmxcsr %[mxcsr]\n\t"
                 :
                 : [mxcsr] "m" (*&mxcsr)
                 :);
    return 0;
}

int fetestexcept(int excepts)
{
    int status;
    unsigned int mxcsr;

    asm volatile("fnstsw %[status]\n\t"
                 "stmxcsr %[mxcsr]\n\t"
                 : [status] "=m" (*&status), [mxcsr] "=m" (*&mxcsr)
                 :
                 :);
    return (status | mxcsr) & excepts & FE_ALL_EXCEPT;
}

int fesetround(int round)
{
    unsigned short int cw;
    unsigned int mxcsr;

    if (round & ~0xc00)
        return 1;
    asm volatile("fnstcw %[cw]\n\t"
                 : [cw] "=m" (*&cw)
                 :
                 :);
    cw &= (cw & ~0xc00) | round;
    asm volatile("fldcw %[cw]\n\t"
                 :
                 : [cw] "m" (*&cw)
                 :);
    asm volatile("stmxcsr %[mxcsr]\n\t"
                 : [mxcsr] "=m" (*&mxcsr)
                 :
                 :);
    mxcsr = (mxcsr & ~0x6000) | (round << 3);
    asm volatile("ldmxcsr %[mxcsr]\n\t"
                 :
                 : [mxcsr] "m" (*&mxcsr)
                 :);

    return 0;
}

int fegetround (void)
{
    int cw;

    asm volatile("fnstcw %[cw]\n\t"
                 : [cw] "=m" (*&cw)
                 :
                 :);

    return cw & 0xc00;
}

int abs(int j)
{
    return j>=0?j:-j;
}

long labs(long j)
{
    return j>=0?j:-j;
}

void __udivdi3()
{
    assert(0);
}
