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

#include "runtime.h"

double sqrt(double x)
{
    double res;

    asm volatile("sqrtsd %[x], %[res]"
                 : [res] "=x" (res)
                 : [x] "xm" (x)
                 :);

    return res;
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

int abs(int j)
{
    return j>=0?j:-j;
}

long long llabs(long long j)
{
    return j>=0?j:-j;
}
