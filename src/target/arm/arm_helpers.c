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
#include <stddef.h>
#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <sys/mman.h>
#include <signal.h>
#include <math.h>

#include "arm_private.h"
#include "arm_helpers.h"
#include "runtime.h"

#define INSN(msb, lsb) ((insn >> (lsb)) & ((1 << ((msb) - (lsb) + 1))-1))
#define INSN1(msb, lsb) INSN(msb+16, lsb+16)
#define INSN2(msb, lsb) INSN(msb, lsb)
//#define DUMP_STACK 1

/*static double ssat64_d(double a)
{
    if (a > 0x7fffffffffffffffUL)
        return 0x7fffffffffffffffUL;
    else if (a < (int64_t)0x8000000000000000UL)
        return (int64_t)0x8000000000000000UL;
    return a;
}*/

#define DECLARE_SSAT(size,max,min) \
static inline int##size##_t ssat##size(struct arm_registers *regs, int64_t op) \
{ \
    int##size##_t res; \
 \
    if (op > max) { \
        res = max; \
        regs->fpscr |= 1 << 27; \
    } else if (op < min) { \
        res = min; \
        regs->fpscr |= 1 << 27; \
    } else \
        res = op; \
 \
    return res; \
}
DECLARE_SSAT(8,0x7f,-0x80)
DECLARE_SSAT(16,0x7fff,-0x8000)
DECLARE_SSAT(32,0x7fffffff,-0x80000000L)
static inline int64_t ssat64(struct arm_registers *regs, __int128_t op)
{
    int64_t res;
    __int128_t max = 0x7fffffffffffffffUL;
    __int128_t min = -(max+1);

    if (op > max) {
        res = max;
        regs->fpscr |= 1 << 27;
    } else if (op < min) {
        res = min;
        regs->fpscr |= 1 << 27;
    } else
        res = op;

    return res;
}

#define DECLARE_USAT(size,max) \
static inline uint##size##_t usat##size(struct arm_registers *regs, int64_t op) \
{ \
    uint##size##_t res; \
 \
    if (op > max) { \
        res = max; \
        regs->fpscr |= 1 << 27; \
    } else if (op < 0) { \
        res = 0; \
        regs->fpscr |= 1 << 27; \
    } else \
        res = op; \
 \
    return res; \
}
DECLARE_USAT(8,0xff)
DECLARE_USAT(16,0xffff)
DECLARE_USAT(32,0xffffffff)
static inline uint64_t usat64(struct arm_registers *regs, __int128_t op)
{
    uint64_t res;
    __int128_t max = 0xffffffffffffffffUL;

    if (op > max) {
        res = max;
        regs->fpscr |= 1 << 27;
    } else if (op < 0) {
        res = 0;
        regs->fpscr |= 1 << 27;
    } else
        res = op;

    return res;
}

#define DECLARE_USAT_U(size,max) \
static inline uint##size##_t usat##size##_u(struct arm_registers *regs, uint64_t op) \
{ \
    uint##size##_t res; \
 \
    if (op > max) { \
        res = max; \
        regs->fpscr |= 1 << 27; \
    } else \
        res = op; \
 \
    return res; \
}
DECLARE_USAT_U(8,0xff)
DECLARE_USAT_U(16,0xffff)
DECLARE_USAT_U(32,0xffffffff)
static inline uint64_t usat64_u(struct arm_registers *regs, __uint128_t op)
{
    uint64_t res;
    __int128_t max = 0xffffffffffffffffUL;

    if (op > max) {
        res = max;
        regs->fpscr |= 1 << 27;
    } else
        res = op;

    return res;
}

static __uint128_t ushl(__uint128_t val, int8_t shift, int is_round)
{
    __uint128_t res;
    __uint128_t round = 0;

    if (is_round) {
        if (shift < 0)
            round = (__uint128_t)1 << (-shift -1);
    }

    if (shift == -128)
        res = 0;
    else if (shift >= 0) {
        /* to keep stuff for saturation we need to limit shift to 64 bits */
        shift = shift>64?64:shift;
        res = val << shift;
    }
    else
        res = (val + (is_round?round:0)) >> -shift;

    return res;
}

static __int128_t sshl(__int128_t val, int8_t shift, int is_round)
{
    __int128_t res;
    __uint128_t round = 0;

    if (is_round) {
        if (shift < 0)
            round = (__uint128_t)1 << (-shift -1);
    }

    if (shift == -128)
        /* if there is rounding then bit 128 is always 0 and so result is 0.
           if there is no rounding then result is the sign bit of val */
        res = is_round?0:val>>127;
    else if (shift >= 0) {
        /* to keep stuff for saturation we need to limit shift to 64 bits */
        shift = shift>64?64:shift;
        res = val << shift;
    }
    else
        res = (__int128_t)(val + (is_round?round:0)) >> -shift;

    return res;
}

static inline uint8_t umax8(uint8_t op1, uint8_t op2)
{
    return op1>op2?op1:op2;
}
static inline uint16_t umax16(uint16_t op1, uint16_t op2)
{
    return op1>op2?op1:op2;
}
static inline uint32_t umax32(uint32_t op1, uint32_t op2)
{
    return op1>op2?op1:op2;
}
static inline uint8_t smax8(int8_t op1, int8_t op2)
{
    return op1>op2?op1:op2;
}
static inline uint16_t smax16(int16_t op1, int16_t op2)
{
    return op1>op2?op1:op2;
}
static inline uint32_t smax32(int32_t op1, int32_t op2)
{
    return op1>op2?op1:op2;
}
static inline uint8_t umin8(uint8_t op1, uint8_t op2)
{
    return op1<op2?op1:op2;
}
static inline uint16_t umin16(uint16_t op1, uint16_t op2)
{
    return op1<op2?op1:op2;
}
static inline uint32_t umin32(uint32_t op1, uint32_t op2)
{
    return op1<op2?op1:op2;
}
static inline uint8_t smin8(int8_t op1, int8_t op2)
{
    return op1<op2?op1:op2;
}
static inline uint16_t smin16(int16_t op1, int16_t op2)
{
    return op1<op2?op1:op2;
}
static inline uint32_t smin32(int32_t op1, int32_t op2)
{
    return op1<op2?op1:op2;
}
static inline float maxf(float a, float b)
{
    return a>=b?a:b;
}
static inline float minf(float a, float b)
{
    return a<b?a:b;
}

static int cls(uint64_t op, int start_index)
{
    int res = 0;
    int i;
    int sign = (op >> start_index) & 1;

    for(i = start_index - 1; i >= 0; i--) {
        if (((op >> i) & 1) != sign)
            break;
        res++;
    }

    return res;
}

static int clz(uint64_t op, int start_index)
{
    int res = 0;
    int i;

    for(i = start_index; i >= 0; i--) {
        if ((op >> i) & 1)
            break;
        res++;
    }

    return res;
}

static int cnt(uint64_t op, int width)
{
    int res = 0;
    int i;

    for(i = 0; i < width; i++) {
        if ((op >> i) & 1)
            res++;
    }

    return res;
}

static uint16_t polynomial_16(uint16_t op1, uint8_t op2)
{
    uint16_t result = 0;
    uint16_t extended_op2 = op2;
    int i;

    for(i = 0; i < 16; i++)
        if ((op1 >> i) & 1)
            result = result ^ (extended_op2 << i);

    return result;
}

static double ssat32_d(double a)
{
    if (a > 0x7fffffff)
        return 0x7fffffff;
    else if (a < (int32_t)0x80000000)
        return (int32_t)0x80000000;
    return a;
}

/*static double usat64_d(double a)
{
    if (a > 0xffffffffffffffffUL)
        return 0xffffffffffffffffUL;
    else if (a < 0)
        return 0;
    return a;
}*/

static double usat32_d(double a)
{
    if (a > 0xffffffff)
        return 0xffffffff;
    else if (a < 0)
        return 0;
    return a;
}

static int tkill(int pid, int sig)
{
    return syscall(SYS_tkill, (long) pid, (long) sig);
}

void arm_hlp_dump(uint64_t regs)
{
    struct arm_target *context = container_of((void *) regs, struct arm_target, regs);
    int i;

    printf("==============================================================================\n\n");
    for(i = 0; i < 16; i++) {
        printf("r[%2d] = 0x%08x  ", i, context->regs.r[i]);
        if (i % 4 == 3)
            printf("\n");
    }
    printf("cpsr  = 0x%08x\n", context->regs.cpsr);
#ifdef DUMP_STACK
    for(i = 0 ;i < 16; i++) {
        if (context->regs.r[13] + i * 4 < context->sp_init)
            printf("0x%08x [sp + 0x%02x] = 0x%08x\n", context->regs.r[13] + i * 4, i * 4, *((uint32_t *)(uint64_t)(context->regs.r[13] + i * 4)));
    }
#endif
}

void arm_hlp_dump_and_assert(uint64_t regs)
{
    arm_hlp_dump(regs);
    assert(0);
}

static pid_t gettid()
{
    return syscall(SYS_gettid);
}

void arm_gdb_breakpoint_instruction(uint64_t regs)
{
    tkill(gettid(), SIGILL);
}

void arm_hlp_vdso_cmpxchg(uint64_t _regs)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    uint32_t oldval = regs->r[0];
    uint32_t newval = regs->r[1];
    uint32_t *address = (uint32_t *) g_2_h(regs->r[2]);

    if (__sync_bool_compare_and_swap(address, oldval, newval)) {
        regs->r[0] = 0;
        regs->cpsr |= 0x20000000;
    } else {
        regs->r[0] = ~0;
        regs->cpsr &= ~0x20000000;
    }
}

void arm_hlp_vdso_cmpxchg64(uint64_t _regs)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    uint64_t *oldval = (uint64_t *) g_2_h(regs->r[0]);
    uint64_t *newval = (uint64_t *) g_2_h(regs->r[1]);
    uint64_t *address = (uint64_t *) g_2_h(regs->r[2]);

    if (__sync_bool_compare_and_swap(address, *oldval, *newval)) {
        regs->r[0] = 0;
        regs->cpsr |= 0x20000000;
    } else {
        regs->r[0] = ~0;
        regs->cpsr &= ~0x20000000;
    }
}

uint32_t arm_hlp_compute_flags_pred(uint64_t context, uint32_t cond, uint32_t cpsr)
{
    uint32_t pred = 0;

    switch(cond >> 1) {
        case 0://EQ + NE
            pred = (cpsr & 0x40000000)?0:1;
            break;
        case 1://HS + LO
            pred = (cpsr & 0x20000000)?0:1;
            break;
       case 2://MI + PL
            pred = (cpsr & 0x80000000)?0:1;
            break;
       case 3://VS + VC
            pred = (cpsr & 0x10000000)?0:1;
            break;
       case 4://HI + LS
            pred = ((cpsr & 0x20000000) && !(cpsr & 0x40000000))?0:1;
            break;
       case 5://GE + LT
            {
                uint32_t n = (cpsr >> 31) & 1;
                uint32_t v = (cpsr >> 28) & 1;
                pred = (n == v)?0:1;
            }
            break;
       case 6://GT + LE
            {
                uint32_t n = (cpsr >> 31) & 1;
                uint32_t z = (cpsr >> 30) & 1;
                uint32_t v = (cpsr >> 28) & 1;
                pred = ((z == 0) && (n == v))?0:1;
            }
            break;
        default:
            assert(0);
    }
    //invert cond
    if (cond&1)
        pred = 1 - pred;

    return pred;
}

uint32_t arm_hlp_compute_next_flags(uint64_t context, uint32_t opcode_and_shifter_carry_out, uint32_t rn, uint32_t op, uint32_t oldcpsr)
{
    int n, z, c, v;
    uint32_t op1 = rn;
    uint32_t op2 = op;
    uint32_t calc;
    uint32_t shifter_carry_out;
    uint32_t opcode;
    uint32_t c_in = (oldcpsr >> 29) & 1;

    opcode = opcode_and_shifter_carry_out & 0xf;
    shifter_carry_out = opcode_and_shifter_carry_out & 0x20000000;
    c = oldcpsr & 0x20000000;
    v = oldcpsr & 0x10000000;
    switch(opcode) {
        case 0://and
        case 8://tst
            calc = rn & op;
            c = shifter_carry_out;
            break;
        case 1://eor
            calc = rn ^ op;
            c = shifter_carry_out;
            break;
        case 2://sub
        case 10://cmp
            calc = rn - op;
            c = (op1 >= op2)?0x20000000:0;
            v = (((op1 ^ op2) & (op1 ^ calc)) >> 3) & 0x10000000;
            break;
        case 3://rsb
            calc = op - rn;
            c = (op2 >= op1)?0x20000000:0;
            v = (((op2 ^ op1) & (op2 ^ calc)) >> 3) & 0x10000000;
            break;
        case 5://adc
            calc = rn + op + c_in;
            if (c_in)
                c = (calc <= op1)?0x20000000:0;
            else
                c = (calc < op1)?0x20000000:0;
            v = (((calc ^ op1) & (calc ^ op2)) >> 3) & 0x10000000;
            break;
        case 6://sbc
            calc = rn + ~op + c_in;
            if (c_in)
                c = (calc <= op1)?0x20000000:0;
            else
                c = (calc < op1)?0x20000000:0;
            v = (((op1 ^ op2) & (op1 ^ calc)) >> 3) & 0x10000000;
            break;
        case 7://rsc
            calc = op + ~rn + c_in;
            if (c_in)
                c = (calc <= op2)?0x20000000:0;
            else
                c = (calc < op2)?0x20000000:0;
            v = (((op2 ^ op1) & (op2 ^ calc)) >> 3) & 0x10000000;
            break;
        case 9://teq
            calc = rn ^ op;
            c = shifter_carry_out;
            break;
        case 4://add
        case 11://cmn
            calc = rn + op;
            c = (calc < op1)?0x20000000:0;
            v = (((calc ^ op1) & (calc ^ op2)) >> 3) & 0x10000000;
            break;
        case 12://orr
            calc = rn | op;
            c = shifter_carry_out;
            break;
        case 13://mov
            calc = op;
            c = shifter_carry_out;
            break;
        case 14://bic
            calc = rn & (~op);
            c = shifter_carry_out;
            break;
        case 15://mvn
            calc = ~op;
            c = shifter_carry_out;
            break;
        default:
            printf("arm_hlp_compute_next_flags : opcode = %d\n\n", opcode);
            assert(0);
    }
    n = (calc & 0x80000000);
    z = (calc == 0)?0x40000000:0;

    return n|z|c|v|(oldcpsr&0x0fffffff);
}

uint32_t thumb_t2_hlp_compute_sco(uint64_t context, uint32_t insn, uint32_t rm, uint32_t op, uint32_t oldcpsr)
{
    int shift_mode = INSN(5, 4);
    int sco = oldcpsr & 0x20000000;

    op = op & 0xff;
    switch(shift_mode) {
        case 0:
            if (op == 0) {
                ;//nothing to do
            } else {
                sco = ((rm << (op - 1)) >> 2) & 0x20000000;
            }
            break;
        case 1:
            if (op == 0) //op is convert to 32
                sco = ((rm >> (31)) << 29) & 0x20000000;
            else
                sco = ((rm >> (op - 1)) << 29) & 0x20000000;
            break;
        case 2:
            if (op == 0)
                sco = ((rm >> (31)) << 29) & 0x20000000;
            else
                sco = ((rm >> (op - 1)) << 29) & 0x20000000;
            break;
        case 3:
            if (op == 0) {
                sco = (rm << 29) & 0x20000000;
            } else
                sco = ((rm >> (op - 1)) << 29) & 0x20000000;
            break;
        default:
            assert(0);
    }

    return sco;
}

uint32_t arm_hlp_compute_sco(uint64_t context, uint32_t insn, uint32_t rm, uint32_t op, uint32_t oldcpsr)
{
    int shift_mode = INSN(6, 5);
    int sco = oldcpsr & 0x20000000;

    op = op & 0xff;
    switch(shift_mode) {
        case 0:
            if (op > 32) {
                sco = 0;
            } else if (op) {
                sco = ((rm << (op - 1)) >> 2) & 0x20000000;
            }
            break;
        case 1:
            if (INSN(4, 4) == 0 && op == 0)
                op += 32;
            if (op > 32) {
                sco = 0;
            } else if (op) {
                sco = ((rm >> (op - 1)) << 29) & 0x20000000;
            }
            break;
        case 2:
            if (INSN(4, 4) == 0  && op == 0)
                op += 32;
            if (op >= 32) {
                sco = (rm & 0x80000000) >> 2;
            } else if (op) {
                int32_t rm_s = rm;

                sco = ((rm_s >> (op - 1)) << 29) & 0x20000000;
            }
            break;
        case 3:
            if (INSN(4, 4) == 0  && op == 0) {
                //rrx
                sco = (rm & 1) << 29;
            } else if (op) {
                op = op & 0x1f;
                if (op) {
                    sco = ((rm >> (op - 1)) << 29) & 0x20000000;
                } else {
                    sco = (rm >> 2) & 0x20000000;
                }
            }
            break;
        default:
            assert(0);
    }

    return sco;
}

uint32_t thumb_hlp_compute_next_flags_data_processing(uint64_t context, uint32_t opcode, uint32_t rn, uint32_t op, uint32_t oldcpsr)
{
    int n, z, c, v;
    uint32_t op1 = rn;
    uint32_t op2 = op;
    uint32_t calc;
    uint32_t c_in = (oldcpsr >> 29) & 1;

    c = oldcpsr & 0x20000000;
    v = oldcpsr & 0x10000000;
    switch(opcode) {
        case 0://and
            calc = rn & op;
            break;
        case 1://eor
            calc = rn ^ op;
            break;
        case 2://lsl
            {
                op2 = op2 & 0xff;
                if (op2 == 0) {
                    calc = op1;
                } else if (op2 < 32) {
                    calc = op1 << op2;
                    c = ((op1 << (op2 - 1)) >> 2) & 0x20000000;
                } else if (op2 == 32) {
                    calc = 0;
                    c = ((op1 << (op2 - 1)) >> 2) & 0x20000000;
                } else {
                    calc = 0;
                    c = 0;
                }
            }
            break;
        case 3://lsr
            {
                op2 = op2 & 0xff;
                if (op2 == 0) {
                    calc = op1;
                } else if (op2 < 32) {
                    calc = op1 >> op2;
                    c = ((op1 >> (op2 - 1)) << 29) & 0x20000000;
                } else if (op2 == 32) {
                    calc = 0;
                    c = ((op1 >> (op2 - 1)) << 29) & 0x20000000;
                } else {
                    calc = 0;
                    c = 0;
                }
            }
            break;
        case 4://asr
            {
                int32_t ops = (int32_t) op1;

                op2 = op2 & 0xff;
                if (op2 == 0) {
                    calc = op1;
                } else if (op2 < 32) {
                    calc = ops >> op2;
                    c = ((ops >> (op2 - 1)) << 29) & 0x20000000;
                } else {
                    calc = ops >> 31;
                    c = ((ops >> (32 - 1)) << 29) & 0x20000000;
                }
            }
            break;
        case 5://adc
            calc = rn + op + c_in;
            if (c_in)
                c = (calc <= op1)?0x20000000:0;
            else
                c = (calc < op1)?0x20000000:0;
            v = (((calc ^ op1) & (calc ^ op2)) >> 3) & 0x10000000;
            break;
        case 6://sbc
            calc = rn + ~op + c_in;
            if (c_in)
                c = (calc <= op1)?0x20000000:0;
            else
                c = (calc < op1)?0x20000000:0;
            v = (((op1 ^ op2) & (op1 ^ calc)) >> 3) & 0x10000000;
            break;
        case 7://ror
            op2 = op2 & 0xff;
            if (op2 == 0) {
                calc = op1;
            } else if ((op2 & 0x1f) == 0) {
                calc = op1;
                c = (op1 >> 2) & 0x20000000;
            } else {
                op2 = op2 & 0x1f;
                calc = (op1 >> op2) | (op1 << (32 - op2));
                c = ((op1 >> (op2 - 1)) << 29) & 0x20000000;
            }
            break;
        case 8://tst
            calc = rn & op;
            break;
        case 9://rsb
            calc = -op2;
            c = (0 >= op2)?0x20000000:0;
            v = (((0 ^ op2) & (0 ^ calc)) >> 3) & 0x10000000;
            break;
        case 10://cmp
            calc = rn - op;
            c = (op1 >= op2)?0x20000000:0;
            v = (((op1 ^ op2) & (op1 ^ calc)) >> 3) & 0x10000000;
            break;
        case 11://cmn
            calc = rn + op;
            c = (calc < op1)?0x20000000:0;
            v = (((calc ^ op1) & (calc ^ op2)) >> 3) & 0x10000000;
            break;
        case 12://orr
            calc = rn | op;
            break;
        case 13://mul
            calc = op1 * op2;
            break;
        case 14://bic
            calc = rn & (~op);
            break;
        case 15://mvn
            calc = ~op;
            break;
        default:
            fatal("thumb_hlp_compute_next_flags_data_processing : unsupported opcode %d\n", opcode);
    }
    n = (calc & 0x80000000);
    z = (calc == 0)?0x40000000:0;

    return n|z|c|v|(oldcpsr&0x0fffffff);
}

uint32_t thumb_hlp_compute_next_flags(uint64_t context, uint32_t opcode_and_shifter_carry_out, uint32_t rn, uint32_t op, uint32_t oldcpsr)
{
    int n, z, c, v;
    uint32_t op1 = rn;
    uint32_t op2 = op;
    uint32_t calc;
    uint32_t shifter_carry_out;
    uint32_t opcode;

    opcode = opcode_and_shifter_carry_out & 0x1f;
    shifter_carry_out = opcode_and_shifter_carry_out & 0x20000000;
    c = oldcpsr & 0x20000000;
    v = oldcpsr & 0x10000000;
    switch(opcode) {
        case 12://add register
        case 14://add 3 bits immediate
        case 24 ... 27://add 8 bits immediate
            calc = rn + op;
            c = (calc < op1)?0x20000000:0;
            v = (((calc ^ op1) & (calc ^ op2)) >> 3) & 0x10000000;
            break;
        case 0 ... 3://lsl immediate
        case 4 ... 7://lsr immediate
        case 8 ... 11://asr immediate
        case 16 ... 19://move immediate
            calc = op;
            c = shifter_carry_out;
            break;
        case 13://sub register
        case 15://sub 3 bits immediate
        case 20 ... 23://cmp immediate
        case 28 ... 31://sub 8 immediate
            calc = rn - op;
            c = (op1 >= op2)?0x20000000:0;
            v = (((op1 ^ op2) & (op1 ^ calc)) >> 3) & 0x10000000;
            break;
        default:
            fatal("thumb_hlp_compute_next_flags : unsupported opcode %d(0x%x)\n", opcode, opcode);
    }
    n = (calc & 0x80000000);
    z = (calc == 0)?0x40000000:0;

    return n|z|c|v|(oldcpsr&0x0fffffff);
}

uint32_t thumb_hlp_t2_modified_compute_next_flags(uint64_t context, uint32_t opcode_and_shifter_carry_out, uint32_t rn, uint32_t op, uint32_t oldcpsr)
{
    int n, z, c, v;
    uint32_t op1 = rn;
    uint32_t op2 = op;
    uint32_t calc;
    uint32_t shifter_carry_out;
    uint32_t opcode;
    uint32_t c_in = (oldcpsr >> 29) & 1;

    opcode = opcode_and_shifter_carry_out & 0xf;
    shifter_carry_out = opcode_and_shifter_carry_out & 0x20000000;
    c = oldcpsr & 0x20000000;
    v = oldcpsr & 0x10000000;
    switch(opcode) {
        case 0://and / test
            calc = rn & op;
            c = shifter_carry_out;
            break;
        case 1://bic
            calc = rn & (~op);
            c = shifter_carry_out;
            break;
        case 2://orr / mv
            calc = rn | op;
            c = shifter_carry_out;
            break;
        case 3://orn / mvn
            calc = rn | (~op);
            c = shifter_carry_out;
            break;
        case 4://eor / teq
            calc = rn ^ op;
            c = shifter_carry_out;
            break;
        case 8://add / cmn
            calc = rn + op;
            c = (calc < op1)?0x20000000:0;
            v = (((calc ^ op1) & (calc ^ op2)) >> 3) & 0x10000000;
            break;
        case 10://adc
            calc = rn + op + c_in;
            if (c_in)
                c = (calc <= op1)?0x20000000:0;
            else
                c = (calc < op1)?0x20000000:0;
            v = (((calc ^ op1) & (calc ^ op2)) >> 3) & 0x10000000;
            break;
        case 11:
            calc = rn + ~op + c_in;
            if (c_in)
                c = (calc <= op1)?0x20000000:0;
            else
                c = (calc < op1)?0x20000000:0;
            v = (((op1 ^ op2) & (op1 ^ calc)) >> 3) & 0x10000000;
            break;
        case 13://sub / cmp
            calc = rn - op;
            c = (op1 >= op2)?0x20000000:0;
            v = (((op1 ^ op2) & (op1 ^ calc)) >> 3) & 0x10000000;
            break;
        case 14://rsb
            calc = op - rn;
            c = (op2 >= op1)?0x20000000:0;
            v = (((op2 ^ op1) & (op2 ^ calc)) >> 3) & 0x10000000;
            break;
        default:
            printf("thumb_hlp_t2_modified_compute_next_flags : opcode = %d\n\n", opcode);
            assert(0);
    }
    n = (calc & 0x80000000);
    z = (calc == 0)?0x40000000:0;

    return n|z|c|v|(oldcpsr&0x0fffffff);
}

uint32_t arm_hlp_clz(uint64_t context, uint32_t rm)
{
    uint32_t res = 0;
    int i;

    for(i = 31; i >= 0; i--) {
        if ((rm >> i) & 1)
            break;
        res++;
    }

    return res;
}

uint32_t arm_hlp_multiply_unsigned_lsb(uint64_t context, uint32_t op1, uint32_t op2)
{
    return op1 * op2;
}

uint32_t arm_hlp_multiply_flag_update(uint64_t context, uint32_t res, uint32_t old_cpsr)
{
    uint32_t cpsr = old_cpsr & 0x3fffffff;

    if (res == 0)
        cpsr |= 0x40000000;
    if (res >> 31)
        cpsr |= 0x80000000;

    return cpsr;
}

/* FIXME: ldrex / strex implementation below is not sematically correct. It's subject
          to the ABBA problem which is not the case of ldrex/strex hardware implementation
 */
uint32_t arm_hlp_ldrexx(uint64_t regs, uint32_t address, uint32_t size_access)
{
    struct arm_target *context = container_of((void *) regs, struct arm_target, regs);

    switch(size_access) {
        case 6://8 bits
            context->exclusive_value = (uint64_t) *((uint8_t *)g_2_h(address));
            break;
        case 7://16 bits
            context->exclusive_value = (uint64_t) *((uint16_t *)g_2_h(address));
            break;
        case 4://32 bits
            context->exclusive_value = (uint64_t) *((uint32_t *)g_2_h(address));
            break;
        default:
            fatal("size_access %d unsupported\n", size_access);
    }

    return (uint32_t) context->exclusive_value;
}

uint64_t arm_hlp_ldrexd(uint64_t regs, uint32_t address)
{
    struct arm_target *context = container_of((void *) regs, struct arm_target, regs);

    context->exclusive_value = (uint64_t) *((uint64_t *)g_2_h(address));

    return context->exclusive_value;
}

uint32_t arm_hlp_strexx(uint64_t regs, uint32_t address, uint32_t size_access, uint32_t value)
{
    struct arm_target *context = container_of((void *) regs, struct arm_target, regs);
    uint32_t res = 0;

    switch(size_access) {
        case 6://8 bits
            if (__sync_bool_compare_and_swap((uint8_t *) g_2_h(address), (uint8_t)context->exclusive_value, value))
                res = 0;
            else
                res = 1;
            break;
        case 7://16 bits
            if (__sync_bool_compare_and_swap((uint16_t *) g_2_h(address), (uint16_t)context->exclusive_value, value))
                res = 0;
            else
                res = 1;
            break;
        case 4://32 bits
            if (__sync_bool_compare_and_swap((uint32_t *) g_2_h(address), (uint32_t)context->exclusive_value, value))
                res = 0;
            else
                res = 1;
            break;
        default:
            fatal("size_access %d unsupported\n", size_access);
    }

    return res;
}

uint32_t arm_hlp_strexd(uint64_t regs, uint32_t address, uint32_t lsb, uint32_t msb)
{
    struct arm_target *context = container_of((void *) regs, struct arm_target, regs);
    uint32_t res = 0;
    uint64_t value = ((uint64_t)msb << 32) | lsb;

    if (__sync_bool_compare_and_swap((uint64_t *) g_2_h(address), context->exclusive_value, value))
        res = 0;
    else
        res = 1;

    return res;
}

void arm_hlp_memory_barrier(uint64_t regs)
{
    __sync_synchronize();
}

static int64_t unsignedSatQ(int64_t i, int n, int *isSat)
{
    int64_t sat_pos = (1UL << n) - 1;
    int64_t sat_neg = 0;
    int64_t res;
    *isSat = 0;

    if (i > sat_pos) {
        res = sat_pos;
        *isSat = 1;
    } else if (i < sat_neg) {
        res  = sat_neg;
        *isSat = 1;
    } else
        res = i;

    return res;
}

static int64_t unsignedSat(int64_t i, int n)
{
    int isSatDummy;

    return unsignedSatQ(i, n, &isSatDummy);
}

static int32_t unsignedSat8(int32_t i)
{
    return unsignedSat(i, 8);
}

static int32_t unsignedSat16(int32_t i)
{
    return unsignedSat(i, 16);
}

static uint32_t signedSat16(int32_t i)
{
    if (i > 32767)
        return 0x7fff;
    else if (i < -32768)
        return 0x8000;
    else
        return i & 0xffff;
}

static uint32_t signedSat8(int32_t i)
{
    if (i > 127)
        return 0x7f;
    else if (i < -128)
        return 0x80;
    else
        return i & 0xff;
}

static uint32_t signedSat32Q(int64_t i, int *isSat)
{
    uint32_t res;
    *isSat = 0;

    if (i > 2147483647L) {
        res = 0x7fffffff;
        *isSat = 1;
    } else if (i < -2147483648L) {
        res = 0x80000000;
        *isSat = 1;
    } else
        res = (uint32_t) i;

    return res;
}

static int64_t signedSatQ(int64_t i, int n, int *isSat)
{
    int64_t sat_pos = (1UL << (n - 1)) - 1;
    int64_t sat_neg = -(1UL << (n - 1));
    int64_t res;
    *isSat = 0;

    if (i > sat_pos) {
        res = sat_pos;
        *isSat = 1;
    } else if (i < sat_neg) {
        res  = sat_neg;
        *isSat = 1;
    } else
        res = i;

    return res;
}

static void uadd16_usub16(uint64_t _regs, int is_sub, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int32_t res[2];
    int i;

    //clear ge
    regs->cpsr &= 0xfff0ffff;
    for(i = 0; i < 2; i++) {
        if (is_sub) {
            res[i] = ((regs->r[rn] >> (i * 16)) & 0xffff) - ((regs->r[rm] >> (i * 16)) & 0xffff);
            regs->cpsr |= (res[i] >= 0)?(0x30000 << (i * 2)):0;
        } else {
            res[i] = ((regs->r[rn] >> (i * 16)) & 0xffff) + ((regs->r[rm] >> (i * 16)) & 0xffff);
            regs->cpsr |= (res[i] >= 0x10000)?(0x30000 << (i * 2)):0;
        }
    }
    regs->r[rd] = ((res[1] & 0xffff) << 16) + (res[0] & 0xffff);
}

static void uadd16_usub16_a1(uint64_t _regs, uint32_t insn)
{
    uadd16_usub16(_regs, INSN(5, 5), INSN(15, 12), INSN(19, 16), INSN(3, 0));
}

static void uadd16_usub16_t32(uint64_t _regs, uint32_t insn)
{
    uadd16_usub16(_regs, INSN1(6, 6), INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void uasx_usax(uint64_t _regs, int is_diff_first, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int32_t op1, op2;

    //clear ge
    regs->cpsr &= 0xfff0ffff;
    if (is_diff_first) {
        op1 = ((regs->r[rn] >> (0 * 16)) & 0xffff) - ((regs->r[rm] >> (1 * 16)) & 0xffff);
        op2 = ((regs->r[rn] >> (1 * 16)) & 0xffff) + ((regs->r[rm] >> (0 * 16)) & 0xffff);
    } else {
        op1 = ((regs->r[rn] >> (0 * 16)) & 0xffff) + ((regs->r[rm] >> (1 * 16)) & 0xffff);
        op2 = ((regs->r[rn] >> (1 * 16)) & 0xffff) - ((regs->r[rm] >> (0 * 16)) & 0xffff);
    }
    if (is_diff_first) {
        if (op1 >= 0)
            regs->cpsr |= 3 << 16;
        if (op2 >= 0x10000)
            regs->cpsr |= 3 << 18;
    } else {
        if (op1 >= 0x10000)
            regs->cpsr |= 3 << 16;
        if (op2 >= 0)
            regs->cpsr |= 3 << 18;
    }

    regs->r[rd] = (((uint16_t)op2) << 16) | ((uint16_t)op1);
}

static void uasx_usax_a1(uint64_t _regs, uint32_t insn)
{
    uasx_usax(_regs, INSN(5, 5), INSN(15, 12), INSN(19, 16), INSN(3, 0));
}

static void uasx_usax_t32(uint64_t _regs, uint32_t insn)
{
    uasx_usax(_regs, 1 - INSN1(6, 6), INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void uadd8_usub8(uint64_t _regs, int rd, int rn, int rm, int is_sub)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int32_t res[4];
    int i;

    //clear ge
    regs->cpsr &= 0xfff0ffff;
    for(i = 0; i < 4; i++) {
        if (is_sub) {
            res[i] = ((regs->r[rn] >> (i * 8)) & 0xff) - ((regs->r[rm] >> (i * 8)) & 0xff);
            regs->cpsr |= (res[i] >= 0)?(0x10000 << i):0;
        } else {
            res[i] = ((regs->r[rn] >> (i * 8)) & 0xff) + ((regs->r[rm] >> (i * 8)) & 0xff);
            regs->cpsr |= (res[i] >= 0x100)?(0x10000 << i):0;
        }
    }
    regs->r[rd] = ((res[3] & 0xff) << 24) + ((res[2] & 0xff) << 16) + ((res[1] & 0xff) << 8) + (res[0] & 0xff);
}

static void uadd8_usub8_a1(uint64_t _regs, uint32_t insn)
{
    uadd8_usub8(_regs, INSN(15, 12), INSN(19, 16), INSN(3, 0), INSN(5, 5));
}

static void uadd8_usub8_t32(uint64_t _regs, uint32_t insn)
{
    uadd8_usub8(_regs, INSN2(11, 8), INSN1(3, 0), INSN2(3, 0), INSN1(6, 6));
}

static void uqadd16_uqsub16(uint64_t _regs, int is_sub, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    uint32_t res[2];
    int i;

    for(i = 0; i < 2; i++) {
        if (is_sub)
            res[i] = ((regs->r[rn] >> (i * 16)) & 0xffff) - ((regs->r[rm] >> (i * 16)) & 0xffff);
        else
            res[i] = ((regs->r[rn] >> (i * 16)) & 0xffff) + ((regs->r[rm] >> (i * 16)) & 0xffff);
    }

    regs->r[rd] =  (unsignedSat16(res[1]) << 16) | unsignedSat16(res[0]);
}

static void uqadd16_uqsub16_a1(uint64_t _regs, uint32_t insn)
{
    uqadd16_uqsub16(_regs, INSN(5, 5), INSN(15, 12), INSN(19, 16), INSN(3, 0));
}

static void uqadd16_uqsub16_t32(uint64_t _regs, uint32_t insn)
{
    uqadd16_uqsub16(_regs, INSN1(6, 6), INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void uqasx_uqsax(uint64_t _regs, int is_diff_first, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    uint32_t res[2];

    if (is_diff_first) {
        res[0] = ((regs->r[rn] >> (0 * 16)) & 0xffff) - ((regs->r[rm] >> (1 * 16)) & 0xffff);
        res[1] = ((regs->r[rn] >> (1 * 16)) & 0xffff) + ((regs->r[rm] >> (0 * 16)) & 0xffff);
    } else {
        res[0] = ((regs->r[rn] >> (0 * 16)) & 0xffff) + ((regs->r[rm] >> (1 * 16)) & 0xffff);
        res[1] = ((regs->r[rn] >> (1 * 16)) & 0xffff) - ((regs->r[rm] >> (0 * 16)) & 0xffff); 
    }

    regs->r[rd] =  (unsignedSat16(res[1]) << 16) | unsignedSat16(res[0]);
}

static void uqasx_uqsax_a1(uint64_t _regs, uint32_t insn)
{
    uqasx_uqsax(_regs, INSN(5, 5), INSN(15, 12), INSN(19, 16), INSN(3, 0));
}

static void uqasx_uqsax_t32(uint64_t _regs, uint32_t insn)
{
    uqasx_uqsax(_regs, 1 - INSN1(6, 6), INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void uqadd8(uint64_t _regs, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    uint32_t res[4];
    int i;

    for(i = 0; i < 4; i++) {
        res[i] = ((regs->r[rn] >> (i * 8)) & 0xff) + ((regs->r[rm] >> (i * 8)) & 0xff);
    }

    regs->r[rd] =  (unsignedSat8(res[3]) << 24) | (unsignedSat8(res[2]) << 16) | (unsignedSat8(res[1]) << 8) | unsignedSat8(res[0]);
}

static void uqadd8_a1(uint64_t _regs, uint32_t insn)
{
    uqadd8(_regs, INSN(15, 12), INSN(19, 16), INSN(3, 0));
}

static void uqadd8_t32(uint64_t _regs, uint32_t insn)
{
    uqadd8(_regs, INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void uqsub8(uint64_t _regs, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int32_t diff[4];
    int i;

    for(i = 0; i < 4; i++) {
        diff[i] = ((regs->r[rn] >> (i * 8)) & 0xff) - ((regs->r[rm] >> (i * 8)) & 0xff);
    }
    regs->r[rd] = (unsignedSat8(diff[3]) << 24) | (unsignedSat8(diff[2]) << 16) | (unsignedSat8(diff[1]) << 8) | unsignedSat8(diff[0]);
}

static void uqsub8_a1(uint64_t _regs, uint32_t insn)
{
    uqsub8(_regs, INSN(15, 12), INSN(19, 16), INSN(3, 0));
}

static void uqsub8_t32(uint64_t _regs, uint32_t insn)
{
    uqsub8(_regs, INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void uhadd16_uhsub16(uint64_t _regs, int is_sub, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    uint32_t sum[2];
    int i;

    for(i = 0; i < 2; i++) {
        if (is_sub)
            sum[i] = ((regs->r[rn] >> (i * 16)) & 0xffff) - ((regs->r[rm] >> (i * 16)) & 0xffff);
        else
            sum[i] = ((regs->r[rn] >> (i * 16)) & 0xffff) + ((regs->r[rm] >> (i * 16)) & 0xffff);
        sum[i] >>= 1;
    }

    regs->r[rd] = ((uint16_t)(sum[1]) << 16) | (uint16_t)(sum[0]);
}

static void uhadd16_uhsub16_a1(uint64_t _regs, uint32_t insn)
{
    uhadd16_uhsub16(_regs, INSN(5, 5), INSN(15, 12), INSN(19, 16), INSN(3, 0));
}

static void uhadd16_uhsub16_t32(uint64_t _regs, uint32_t insn)
{
    uhadd16_uhsub16(_regs, INSN1(6, 6), INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void uhasx_uhsax(uint64_t _regs, int is_diff_first, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    uint32_t res[2];

    if (is_diff_first) {
        res[0] = ((regs->r[rn] >> (0 * 16)) & 0xffff) - ((regs->r[rm] >> (1 * 16)) & 0xffff);
        res[1] = ((regs->r[rn] >> (1 * 16)) & 0xffff) + ((regs->r[rm] >> (0 * 16)) & 0xffff);
    } else {
        res[0] = ((regs->r[rn] >> (0 * 16)) & 0xffff) + ((regs->r[rm] >> (1 * 16)) & 0xffff);
        res[1] = ((regs->r[rn] >> (1 * 16)) & 0xffff) - ((regs->r[rm] >> (0 * 16)) & 0xffff);
    }
    res[0] >>= 1;
    res[1] >>= 1;

    regs->r[rd] = ((uint16_t)(res[1]) << 16) | (uint16_t)(res[0]);
}

static void uhasx_uhsax_a1(uint64_t _regs, uint32_t insn)
{
    uhasx_uhsax(_regs, INSN(5, 5), INSN(15, 12), INSN(19, 16), INSN(3, 0));
}

static void uhasx_uhsax_t32(uint64_t _regs, uint32_t insn)
{
    uhasx_uhsax(_regs, 1 - INSN1(6, 6), INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void uhsub8_uhadd8(uint64_t _regs, int is_sub, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    uint32_t sum[4];
    int i;

    for(i = 0; i < 4; i++) {
        if (is_sub)
            sum[i] = ((regs->r[rn] >> (i * 8)) & 0xff) - ((regs->r[rm] >> (i * 8)) & 0xff);
        else
            sum[i] = ((regs->r[rn] >> (i * 8)) & 0xff) + ((regs->r[rm] >> (i * 8)) & 0xff);
        sum[i] >>= 1;
    }

    regs->r[rd] = ((uint8_t)(sum[3]) << 24) | ((uint8_t)(sum[2]) << 16) | ((uint8_t)(sum[1]) << 8) | (uint8_t)(sum[0]);
}

static void uhsub8_uhadd8_a1(uint64_t _regs, uint32_t insn)
{
    uhsub8_uhadd8(_regs, INSN(5, 5), INSN(15, 12), INSN(19, 16), INSN(3, 0));
}

static void uhsub8_uhadd8_t32(uint64_t _regs, uint32_t insn)
{
    uhsub8_uhadd8(_regs, INSN1(6, 6), INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void sadd16_ssub16(uint64_t _regs, int is_sub, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int32_t res[2];
    int i;

    //clear ge
    regs->cpsr &= 0xfff0ffff;
    for(i = 0; i < 2; i++) {
        if (is_sub)
            res[i] = (int32_t)(int16_t)((regs->r[rn] >> (i * 16)) & 0xffff) - (int32_t)(int16_t)((regs->r[rm] >> (i * 16)) & 0xffff);
        else
            res[i] = (int32_t)(int16_t)((regs->r[rn] >> (i * 16)) & 0xffff) + (int32_t)(int16_t)((regs->r[rm] >> (i * 16)) & 0xffff);
        if (res[i] >= 0)
            regs->cpsr |= 3 << (16 + i * 2);
    }

    regs->r[rd] = (((uint16_t)res[1]) << 16) | ((uint16_t)res[0]);
}

static void sadd16_ssub16_a1(uint64_t _regs, uint32_t insn)
{
    sadd16_ssub16(_regs, INSN(5, 5), INSN(15, 12), INSN(19, 16), INSN(3, 0));
}

static void sadd16_ssub16_t32(uint64_t _regs, uint32_t insn)
{
    sadd16_ssub16(_regs, INSN1(6, 6), INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void sasx_ssax(uint64_t _regs, int is_diff_first, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int32_t op1, op2;

    //clear ge
    regs->cpsr &= 0xfff0ffff;
    if (is_diff_first) {
        op1 = (int32_t)(int16_t)((regs->r[rn] >> (0 * 16)) & 0xffff) - (int32_t)(int16_t)((regs->r[rm] >> (1 * 16)) & 0xffff);
        op2  = (int32_t)(int16_t)((regs->r[rn] >> (1 * 16)) & 0xffff) + (int32_t)(int16_t)((regs->r[rm] >> (0 * 16)) & 0xffff);
    } else {
        op1 = (int32_t)(int16_t)((regs->r[rn] >> (0 * 16)) & 0xffff) + (int32_t)(int16_t)((regs->r[rm] >> (1 * 16)) & 0xffff);
        op2  = (int32_t)(int16_t)((regs->r[rn] >> (1 * 16)) & 0xffff) - (int32_t)(int16_t)((regs->r[rm] >> (0 * 16)) & 0xffff);
    }
    if (op1 >= 0)
        regs->cpsr |= 3 << 16;
    if (op2 >= 0)
        regs->cpsr |= 3 << 18;

    regs->r[rd] = (((uint16_t)op2) << 16) | ((uint16_t)op1);
}

static void sasx_ssax_a1(uint64_t _regs, uint32_t insn)
{
    sasx_ssax(_regs, INSN(5, 5), INSN(15, 12), INSN(19, 16), INSN(3, 0));
}

static void sasx_ssax_t32(uint64_t _regs, uint32_t insn)
{
    sasx_ssax(_regs, 1 - INSN1(6, 6), INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void sadd8_ssub8(uint64_t _regs, int is_sub, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int32_t res[4];
    int i;

    //clear ge
    regs->cpsr &= 0xfff0ffff;
    for(i = 0; i < 4; i++) {
        if (is_sub)
            res[i] = (int32_t)(int8_t)((regs->r[rn] >> (i * 8)) & 0xff) - (int32_t)(int8_t)((regs->r[rm] >> (i * 8)) & 0xff);
        else
            res[i] = (int32_t)(int8_t)((regs->r[rn] >> (i * 8)) & 0xff) + (int32_t)(int8_t)((regs->r[rm] >> (i * 8)) & 0xff);
        if (res[i] >= 0)
            regs->cpsr |= 1 << (16 + i);
    }

    regs->r[rd] = (((uint8_t)res[3]) << 24) |(((uint8_t)res[2]) << 16) | (((uint8_t)res[1]) << 8) | ((uint8_t)res[0]);
}

static void sadd8_ssub8_a1(uint64_t _regs, uint32_t insn)
{
    sadd8_ssub8(_regs, INSN(5, 5), INSN(15, 12), INSN(19, 16), INSN(3, 0));
}

static void sadd8_ssub8_t32(uint64_t _regs, uint32_t insn)
{
    sadd8_ssub8(_regs, INSN1(6, 6), INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void qadd16(uint64_t _regs, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int32_t sum[2];
    int i;

    for(i = 0; i < 2; i++) {
        sum[i] = (int32_t)(int16_t)((regs->r[rn] >> (i * 16)) & 0xffff) + (int32_t)(int16_t)((regs->r[rm] >> (i * 16)) & 0xffff);
    }
    regs->r[rd] = (signedSat16(sum[1]) << 16) | signedSat16(sum[0]);
}

static void qadd16_a1(uint64_t _regs, uint32_t insn)
{
    qadd16(_regs, INSN(15, 12), INSN(19, 16), INSN(3, 0));
}

static void qadd16_t32(uint64_t _regs, uint32_t insn)
{
    qadd16(_regs, INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void qasx(uint64_t _regs, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int32_t res[2];


    res[0] = (int32_t)(int16_t)((regs->r[rn] >> (0 * 16)) & 0xffff) - (int32_t)(int16_t)((regs->r[rm] >> (1 * 16)) & 0xffff);
    res[1] = (int32_t)(int16_t)((regs->r[rn] >> (1 * 16)) & 0xffff) + (int32_t)(int16_t)((regs->r[rm] >> (0 * 16)) & 0xffff);

    regs->r[rd] = (signedSat16(res[1]) << 16) | signedSat16(res[0]);
}

static void qasx_a1(uint64_t _regs, uint32_t insn)
{
    qasx(_regs, INSN(15, 12), INSN(19, 16), INSN(3, 0));
}

static void qasx_t32(uint64_t _regs, uint32_t insn)
{
    qasx(_regs, INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void qsax(uint64_t _regs, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int32_t res[2];


    res[0] = (int32_t)(int16_t)((regs->r[rn] >> (0 * 16)) & 0xffff) + (int32_t)(int16_t)((regs->r[rm] >> (1 * 16)) & 0xffff);
    res[1] = (int32_t)(int16_t)((regs->r[rn] >> (1 * 16)) & 0xffff) - (int32_t)(int16_t)((regs->r[rm] >> (0 * 16)) & 0xffff);

    regs->r[rd] = (signedSat16(res[1]) << 16) | signedSat16(res[0]);
}

static void qsax_a1(uint64_t _regs, uint32_t insn)
{
    qsax(_regs, INSN(15, 12), INSN(19, 16), INSN(3, 0));
}

static void qsax_t32(uint64_t _regs, uint32_t insn)
{
    qsax(_regs, INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void qsub16(uint64_t _regs, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int32_t sum[2];
    int i;

    for(i = 0; i < 2; i++) {
        sum[i] = (int32_t)(int16_t)((regs->r[rn] >> (i * 16)) & 0xffff) - (int32_t)(int16_t)((regs->r[rm] >> (i * 16)) & 0xffff);
    }
    regs->r[rd] = (signedSat16(sum[1]) << 16) | signedSat16(sum[0]);
}

static void qsub16_a1(uint64_t _regs, uint32_t insn)
{
    qsub16(_regs, INSN(15, 12), INSN(19, 16), INSN(3, 0));
}

static void qsub16_t32(uint64_t _regs, uint32_t insn)
{
    qsub16(_regs, INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void qadd8(uint64_t _regs, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int32_t sum[4];
    int i;

    for(i = 0; i < 4; i++) {
        sum[i] = (int32_t)(int8_t)((regs->r[rn] >> (i * 8)) & 0xff) + (int32_t)(int8_t)((regs->r[rm] >> (i * 8)) & 0xff);
    }
    regs->r[rd] = (signedSat8(sum[3]) << 24) | (signedSat8(sum[2]) << 16) | (signedSat8(sum[1]) << 8) | signedSat8(sum[0]);
}

static void qadd8_a1(uint64_t _regs, uint32_t insn)
{
    qadd8(_regs, INSN(15, 12), INSN(19, 16), INSN(3, 0));
}

static void qadd8_t32(uint64_t _regs, uint32_t insn)
{
    qadd8(_regs, INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void qsub8(uint64_t _regs, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int32_t sum[4];
    int i;

    for(i = 0; i < 4; i++) {
        sum[i] = (int32_t)(int8_t)((regs->r[rn] >> (i * 8)) & 0xff) - (int32_t)(int8_t)((regs->r[rm] >> (i * 8)) & 0xff);
    }
    regs->r[rd] = (signedSat8(sum[3]) << 24) | (signedSat8(sum[2]) << 16) | (signedSat8(sum[1]) << 8) | signedSat8(sum[0]);
}

static void qsub8_a1(uint64_t _regs, uint32_t insn)
{
    qsub8(_regs, INSN(15, 12), INSN(19, 16), INSN(3, 0));
}

static void qsub8_t32(uint64_t _regs, uint32_t insn)
{
    qsub8(_regs, INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void shadd16(uint64_t _regs, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int32_t sum[2];
    int i;

    for(i = 0; i < 2; i++) {
        sum[i] = (int32_t)(int16_t)((regs->r[rn] >> (i * 16)) & 0xffff) + (int32_t)(int16_t)((regs->r[rm] >> (i * 16)) & 0xffff);
        sum[i] >>= 1;
    }

    regs->r[rd] = ((uint16_t)(sum[1]) << 16) | (uint16_t)(sum[0]);
}

static void shadd16_a1(uint64_t _regs, uint32_t insn)
{
    shadd16(_regs, INSN(15, 12), INSN(19, 16), INSN(3, 0));
}

static void shadd16_t32(uint64_t _regs, uint32_t insn)
{
    shadd16(_regs, INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void shasx(uint64_t _regs, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int32_t diff, sum;

    diff = (int32_t)(int16_t)((regs->r[rn] >> (0 * 16)) & 0xffff) - (int32_t)(int16_t)((regs->r[rm] >> (1 * 16)) & 0xffff);
    sum  = (int32_t)(int16_t)((regs->r[rn] >> (1 * 16)) & 0xffff) + (int32_t)(int16_t)((regs->r[rm] >> (0 * 16)) & 0xffff);
    diff >>= 1;
    sum >>= 1;

    regs->r[rd] = ((uint16_t)(sum) << 16) | (uint16_t)(diff);
}

static void shasx_a1(uint64_t _regs, uint32_t insn)
{
    shasx(_regs, INSN(15, 12), INSN(19, 16), INSN(3, 0));
}

static void shasx_t32(uint64_t _regs, uint32_t insn)
{
    shasx(_regs, INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void shsax(uint64_t _regs, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int32_t diff, sum;

    sum = (int32_t)(int16_t)((regs->r[rn] >> (0 * 16)) & 0xffff) + (int32_t)(int16_t)((regs->r[rm] >> (1 * 16)) & 0xffff);
    diff  = (int32_t)(int16_t)((regs->r[rn] >> (1 * 16)) & 0xffff) - (int32_t)(int16_t)((regs->r[rm] >> (0 * 16)) & 0xffff);
    diff >>= 1;
    sum >>= 1;

    regs->r[rd] = ((uint16_t)(diff) << 16) | (uint16_t)(sum);
}

static void shsax_a1(uint64_t _regs, uint32_t insn)
{
    shsax(_regs, INSN(15, 12), INSN(19, 16), INSN(3, 0));
}

static void shsax_t32(uint64_t _regs, uint32_t insn)
{
    shsax(_regs, INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void shsub16(uint64_t _regs, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int32_t diff[2];
    int i;

    for(i = 0; i < 2; i++) {
        diff[i] = (int32_t)(int16_t)((regs->r[rn] >> (i * 16)) & 0xffff) - (int32_t)(int16_t)((regs->r[rm] >> (i * 16)) & 0xffff);
        diff[i] >>= 1;
    }

    regs->r[rd] = ((uint16_t)(diff[1]) << 16) | (uint16_t)(diff[0]);
}

static void shsub16_a1(uint64_t _regs, uint32_t insn)
{
    shsub16(_regs, INSN(15, 12), INSN(19, 16), INSN(3, 0));
}

static void shsub16_t32(uint64_t _regs, uint32_t insn)
{
    shsub16(_regs, INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void shadd8(uint64_t _regs, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int32_t sum[4];
    int i;

    for(i = 0; i < 4; i++) {
        sum[i] = (int32_t)(int8_t)((regs->r[rn] >> (i * 8)) & 0xff) + (int32_t)(int8_t)((regs->r[rm] >> (i * 8)) & 0xff);
        sum[i] >>= 1;
    }

    regs->r[rd] = ((uint8_t)(sum[3]) << 24) | ((uint8_t)(sum[2]) << 16) | ((uint8_t)(sum[1]) << 8) | (uint8_t)(sum[0]);
}

static void shadd8_a1(uint64_t _regs, uint32_t insn)
{
    shadd8(_regs, INSN(15, 12), INSN(19, 16), INSN(3, 0));
}

static void shadd8_t32(uint64_t _regs, uint32_t insn)
{
    shadd8(_regs, INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void shsub8(uint64_t _regs, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int32_t diff[4];
    int i;

    for(i = 0; i < 4; i++) {
        diff[i] = (int32_t)(int8_t)((regs->r[rn] >> (i * 8)) & 0xff) - (int32_t)(int8_t)((regs->r[rm] >> (i * 8)) & 0xff);
        diff[i] >>= 1;
    }

    regs->r[rd] = ((uint8_t)(diff[3]) << 24) | ((uint8_t)(diff[2]) << 16) | ((uint8_t)(diff[1]) << 8) | (uint8_t)(diff[0]);
}

static void shsub8_a1(uint64_t _regs, uint32_t insn)
{
    shsub8(_regs, INSN(15, 12), INSN(19, 16), INSN(3, 0));
}

static void shsub8_t32(uint64_t _regs, uint32_t insn)
{
    shsub8(_regs, INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

void thumb_hlp_t2_unsigned_parallel(uint64_t regs, uint32_t insn)
{
    int op1 = INSN1(6, 4);
    int op2 = INSN2(5, 4);

    if (op2 == 0) {
        switch(op1) {
            case 0: case 4:
                uadd8_usub8_t32(regs, insn);
                break;
            case 1: case 5:
                uadd16_usub16_t32(regs, insn);
                break;
            case 2: case 6:
                uasx_usax_t32(regs, insn);
                break;
            default:
                fatal("op1 = %d(0x%x)\n", op1, op1);
        }
    } else if (op2 == 1) {
        switch(op1) {
            case 0:
                uqadd8_t32(regs, insn);
                break;
            case 1: case 5:
                uqadd16_uqsub16_t32(regs, insn);
                break;
            case 2: case 6:
                uqasx_uqsax_t32(regs, insn);
                break;
            case 4:
                uqsub8_t32(regs, insn);
                break;
            default:
                fatal("op1 = %d(0x%x)\n", op1, op1);
        }
    } else if (op2 == 2) {
        switch(op1) {
            case 0: case 4:
                uhsub8_uhadd8_t32(regs, insn);
                break;
            case 1: case 5:
                uhadd16_uhsub16_t32(regs, insn);
                break;
            case 2: case 6:
                uhasx_uhsax_t32(regs, insn);
                break;
            default:
                fatal("op1 = %d(0x%x)\n", op1, op1);
        }
    } else {
        assert(0);
    }
}

void arm_hlp_unsigned_parallel(uint64_t regs, uint32_t insn)
{
    int op1 = INSN(21, 20);
    int op2 = INSN(7, 5);

    switch(op1) {
        case 1:
            switch(op2) {
                case 0:
                case 3:
                    uadd16_usub16_a1(regs, insn);
                    break;
                case 1:
                case 2:
                    uasx_usax_a1(regs, insn);
                    break;
                case 4:
                case 7:
                    uadd8_usub8_a1(regs, insn);
                    break;
                default:
                    fatal("op1 = %d / op2 = %d(0x%x)\n", op1, op2, op2);
            }
            break;
        case 2:
            switch(op2) {
                case 0:
                    uqadd16_uqsub16_a1(regs, insn);
                    break;
                case 1:
                case 2:
                    uqasx_uqsax_a1(regs, insn);
                    break;
                case 3:
                    uqadd16_uqsub16_a1(regs, insn);
                    break;
                case 4:
                    uqadd8_a1(regs, insn);
                    break;
                case 7:
                    uqsub8_a1(regs, insn);
                    break;
                default:
                    fatal("op1 = %d / op2 = %d(0x%x)\n", op1, op2, op2);
            }
            break;
        case 3:
            switch(op2) {
                case 0:
                case 3:
                    uhadd16_uhsub16_a1(regs, insn);
                    break;
                case 1:
                case 2:
                    uhasx_uhsax_a1(regs, insn);
                    break;
                case 4:
                case 7:
                    uhsub8_uhadd8_a1(regs, insn);
                    break;
                default:
                    fatal("op1 = %d / op2 = %d(0x%x)\n", op1, op2, op2);
            }
            break;
        default:
            fatal("op1 = %d / op2 = %d(0x%x)\n", op1, op2, op2);
    }
}

void arm_hlp_signed_parallel(uint64_t regs, uint32_t insn)
{
    int op1 = INSN(21, 20);
    int op2 = INSN(7, 5);

    switch(op1) {
        case 1:
            switch(op2) {
                case 0:
                case 3:
                    sadd16_ssub16_a1(regs, insn);
                    break;
                case 1:
                    sasx_ssax_a1(regs, insn);
                    break;
                case 2:
                    sasx_ssax_a1(regs, insn);
                    break;
                case 4:
                case 7:
                    sadd8_ssub8_a1(regs, insn);
                    break;
                default:
                    fatal("op1 = %d / op2 = %d(0x%x)\n", op1, op2, op2);
            }
            break;
        case 2:
            //saturating
            switch(op2) {
                case 0:
                    qadd16_a1(regs, insn);
                    break;
                case 1:
                    qasx_a1(regs, insn);
                    break;
                case 2:
                    qsax_a1(regs, insn);
                    break;
                case 3:
                    qsub16_a1(regs, insn);
                    break;
                case 4:
                    qadd8_a1(regs, insn);
                    break;
                case 7:
                    qsub8_a1(regs, insn);
                    break;
                default:
                    fatal("op1 = %d / op2 = %d(0x%x)\n", op1, op2, op2);
            }
            break;
        case 3:
            switch(op2) {
                case 0:
                    shadd16_a1(regs, insn);
                    break;
                case 1:
                    shasx_a1(regs, insn);
                    break;
                case 2:
                    shsax_a1(regs, insn);
                    break;
                case 3:
                    shsub16_a1(regs, insn);
                    break;
                case 4:
                    shadd8_a1(regs, insn);
                    break;
                case 7:
                    shsub8_a1(regs, insn);
                    break;
                default:
                    fatal("op1 = %d / op2 = %d(0x%x)\n", op1, op2, op2);
            }
            break;
        default:
            fatal("op1 = %d(0x%x)\n", op1, op1);
    }
}

uint32_t arm_hlp_sel(uint64_t context, uint32_t cpsr, uint32_t rn, uint32_t rm)
{
    uint32_t res = 0;
    int i;

    for(i = 0; i < 4; i++) {
        if (cpsr & (0x10000 << i))
            res |= rn & (0xff << (i * 8));
        else
            res |= rm & (0xff << (i * 8));
    }

    return res;
}

uint32_t arm_hlp_multiply_unsigned_msb(uint64_t context, uint32_t op1, uint32_t op2)
{
    uint64_t res = (uint64_t)op1 * (uint64_t)op2;

    return res >> 32;
}

uint32_t arm_hlp_multiply_signed_lsb(uint64_t context, int32_t op1, int32_t op2)
{
    int64_t res = (int64_t)op1 * (int64_t)op2;

    return (uint32_t) res;
}

uint32_t arm_hlp_multiply_signed_msb(uint64_t context, int32_t op1, int32_t op2)
{
    int64_t res = (int64_t)op1 * (int64_t)op2;

    return (uint32_t) (res >> 32);
}

uint32_t arm_hlp_multiply_accumulate_unsigned_lsb(uint64_t context, uint32_t op1, uint32_t op2, uint32_t rdhi, uint32_t rdlo)
{
    return op1 * op2 + rdlo;
}

uint32_t arm_hlp_multiply_accumulate_signed_lsb(uint64_t context, int32_t op1, int32_t op2, uint32_t rdhi, uint32_t rdlo)
{
    uint64_t acc = (((uint64_t)rdhi) << 32) + rdlo;
    int64_t res = (int64_t)op1 * (int64_t)op2 + (int64_t)acc;

    return (uint32_t) res;
}

uint32_t arm_hlp_multiply_accumulate_unsigned_msb(uint64_t context, uint32_t op1, uint32_t op2, uint32_t rdhi, uint32_t rdlo)
{
    uint64_t acc = (((uint64_t)rdhi) << 32) + rdlo;
    uint64_t res = (uint64_t)op1 * (uint64_t)op2 + acc;

    return res >> 32;
}

uint32_t arm_hlp_multiply_accumulate_signed_msb(uint64_t context, int32_t op1, int32_t op2, uint32_t rdhi, uint32_t rdlo)
{
    uint64_t acc = (((uint64_t)rdhi) << 32) + rdlo;
    int64_t res = (int64_t)op1 * (int64_t)op2 + (int64_t)acc;

    return (uint32_t) (res >> 32);
}

void vstm(uint64_t _regs, uint32_t insn, int is_thumb)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    //int p = INSN(24, 24);
    int add = INSN(23, 23);
    int D = INSN(22, 22);
    int w = INSN(21, 21);
    int rn = INSN(19, 16);
    int vd = INSN(15, 12);
    int is_64 = INSN(8, 8);
    uint32_t imm8 = INSN(7, 0);
    uint32_t imm32 = imm8 << 2;
    int r;
    int d;

    assert(rn != 15);

    if (is_64) {
        int regs_nb = imm8 >> 1;
        uint32_t address = regs->r[rn];

        d = (D << 4) + vd;
        if (!add)
            address -= imm32;
        if (w)
            regs->r[rn] += add?imm32:-imm32;
        for(r = 0; r < regs_nb; r++) {
            uint64_t *addr = (uint64_t *) g_2_h(address);
            *addr = regs->e.d[d + r];
            address += 8;
        }
    } else {
        int regs_nb = imm8;
        uint32_t address = regs->r[rn];

        d = (vd << 1) + D;
        if (!add)
            address -= imm32;
        if (w)
            regs->r[rn] += add?imm32:-imm32;
        for(r = 0; r < regs_nb; r++) {
            uint32_t *addr = (uint32_t *) g_2_h(address);
            *addr = regs->e.s[d + r];
            address += 4;
        }
    }
}

void vldm(uint64_t _regs, uint32_t insn, int is_thumb)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    //int p = INSN(24, 24);
    int add = INSN(23, 23);
    int D = INSN(22, 22);
    int w = INSN(21, 21);
    int rn = INSN(19, 16);
    int vd = INSN(15, 12);
    int is_64 = INSN(8, 8);
    uint32_t imm8 = INSN(7, 0);
    uint32_t imm32 = imm8 << 2;
    int r;
    int d;

    assert(rn != 15);
    if (is_64) {
        int regs_nb = imm8 >> 1;
        uint32_t address = regs->r[rn];

        d = (D << 4) + vd;
        if (!add)
            address -= imm32;
        if (w)
            regs->r[rn] += add?imm32:-imm32;
        for(r = 0; r < regs_nb; r++) {
            uint64_t *addr = (uint64_t *) g_2_h(address);
            regs->e.d[d + r] = *addr;
            address += 8;
        }
    } else {
        int regs_nb = imm8;
        uint32_t address = regs->r[rn];

        d = (vd << 1) + D;
        if (!add)
            address -= imm32;
        if (w)
            regs->r[rn] += add?imm32:-imm32;
        for(r = 0; r < regs_nb; r++) {
            uint32_t *addr = (uint32_t *) g_2_h(address);
            regs->e.s[d + r] = *addr;
            address += 4;
        }
    }
}

static void qadd(uint64_t _regs, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int64_t unsat = (int64_t)(int32_t) regs->r[rm] + (int64_t)(int32_t) regs->r[rn];
    uint32_t res;
    int sat;

    res = signedSat32Q(unsat, &sat);
    if (sat)
        regs->cpsr |= 1 << 27;

    regs->r[rd] = res;
}

static void qadd_a1(uint64_t _regs, uint32_t insn)
{
    qadd(_regs, INSN(15, 12), INSN(19, 16), INSN(3, 0));
}

static void qadd_t32(uint64_t _regs, uint32_t insn)
{
    qadd(_regs, INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void qsub(uint64_t _regs, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int64_t unsat = (int64_t)(int32_t) regs->r[rm] - (int64_t)(int32_t) regs->r[rn];
    uint32_t res;
    int sat;

    res = signedSat32Q(unsat, &sat);
    if (sat)
        regs->cpsr |= 1 << 27;

    regs->r[rd] = res;
}

static void qsub_a1(uint64_t _regs, uint32_t insn)
{
    qsub(_regs, INSN(15, 12), INSN(19, 16), INSN(3, 0));
}

static void qsub_t32(uint64_t _regs, uint32_t insn)
{
    qsub(_regs, INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void qdadd(uint64_t _regs, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    uint32_t doubled;
    uint32_t res;
    int sat1, sat2;

    doubled = signedSat32Q(2 * (int64_t)(int32_t) regs->r[rn], &sat1);
    res = signedSat32Q((int64_t)(int32_t) regs->r[rm] + (int64_t)(int32_t) doubled, &sat2);
    if (sat1 || sat2)
        regs->cpsr |= 1 << 27;

    regs->r[rd] = res;
}

static void qdadd_a1(uint64_t _regs, uint32_t insn)
{
    qdadd(_regs, INSN(15, 12), INSN(19, 16), INSN(3, 0));
}

static void qdadd_t32(uint64_t _regs, uint32_t insn)
{
    qdadd(_regs, INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void qdsub(uint64_t _regs, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    uint32_t doubled;
    uint32_t res;
    int sat1, sat2;

    doubled = signedSat32Q(2 * (int64_t)(int32_t) regs->r[rn], &sat1);
    res = signedSat32Q((int64_t)(int32_t) regs->r[rm] - (int64_t)(int32_t) doubled, &sat2);
    if (sat1 || sat2)
        regs->cpsr |= 1 << 27;

    regs->r[rd] = res;
}

static void qdsub_a1(uint64_t _regs, uint32_t insn)
{
    qdsub(_regs, INSN(15, 12), INSN(19, 16), INSN(3, 0));
}

static void qdsub_t32(uint64_t _regs, uint32_t insn)
{
    qdsub(_regs, INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void smlaxy(uint64_t _regs, int n, int m, int rd, int rn, int rm, int ra)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int64_t operand1, operand2;
    int64_t res;

    operand1 = (int16_t)(n?regs->r[rn]>>16:regs->r[rn]);
    operand2 = (int16_t)(m?regs->r[rm]>>16:regs->r[rm]);

    res = operand1 * operand2 + (int32_t)regs->r[ra];
    if (res != (int32_t) res)
        regs->cpsr |= 1 << 27;

    regs->r[rd] = res;
}

static void smlaxy_a1(uint64_t _regs, uint32_t insn)
{
    smlaxy(_regs, INSN(5, 5), INSN(6, 6), INSN(19, 16), INSN(3, 0), INSN(11, 8), INSN(15, 12));
}

static void smlaxy_t32(uint64_t _regs, uint32_t insn)
{
    smlaxy(_regs, INSN2(5, 5), INSN2(4, 4), INSN2(11, 8), INSN1(3, 0), INSN2(3, 0), INSN2(15, 12));
}

static void smulwy(uint64_t _regs, int m, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int64_t operand2;
    int64_t res;

    operand2 = (int16_t)(m?regs->r[rm]>>16:regs->r[rm]);

    res = (int32_t)regs->r[rn] * operand2;

    regs->r[rd] = res >> 16;
}

static void smulwy_a1(uint64_t _regs, uint32_t insn)
{
    smulwy(_regs, INSN(6, 6), INSN(19, 16), INSN(3, 0), INSN(11, 8));
}

static void smulwy_t32(uint64_t _regs, uint32_t insn)
{
    smulwy(_regs, INSN2(4, 4), INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void smlawy(uint64_t _regs, int m, int rd, int rn, int rm, int ra)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int64_t operand2;
    int64_t res;

    operand2 = (int16_t)(m?regs->r[rm]>>16:regs->r[rm]);

    res = (int32_t)regs->r[rn] * operand2 + (((int64_t)(int32_t)regs->r[ra]) << 16);
    if ((res >> 16) != (int32_t)(res >> 16))
        regs->cpsr |= 1 << 27;

    regs->r[rd] = res >> 16;
}

static void smlawy_a1(uint64_t _regs, uint32_t insn)
{
    smlawy(_regs, INSN(6, 6), INSN(19, 16), INSN(3, 0), INSN(11, 8), INSN(15, 12));
}

static void smlawy_t32(uint64_t _regs, uint32_t insn)
{
    smlawy(_regs, INSN2(4, 4), INSN2(11, 8), INSN1(3, 0), INSN2(3, 0), INSN2(15, 12));
}

static void sdiv(uint64_t _regs, uint32_t insn)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int rn = INSN1(3, 0);
    int rd = INSN2(11, 8);
    int rm = INSN2(3, 0);
    int32_t res = 0;

    if (regs->r[rm])
        res = (int32_t)regs->r[rn] / (int32_t)regs->r[rm];

    regs->r[rd] = res;
}

static void udiv(uint64_t _regs, uint32_t insn)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int rn = INSN1(3, 0);
    int rd = INSN2(11, 8);
    int rm = INSN2(3, 0);
    uint32_t res = 0;

    if (regs->r[rm])
        res = regs->r[rn] / regs->r[rm];

    regs->r[rd] = res;
}

static void smlalxy(uint64_t _regs, int n, int m, int rdlo, int rdhi, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int64_t operand1, operand2;
    int64_t acc = ((uint64_t) regs->r[rdhi] << 32) + regs->r[rdlo];
    int64_t res;

    operand1 = (int16_t)(n?regs->r[rn]>>16:regs->r[rn]);
    operand2 = (int16_t)(m?regs->r[rm]>>16:regs->r[rm]);

    res = operand1 * operand2 + acc;

    regs->r[rdhi] = res >> 32;
    regs->r[rdlo] = res;
}

static void smlalxy_a1(uint64_t _regs, uint32_t insn)
{
    smlalxy(_regs, INSN(5, 5), INSN(6, 6), INSN(15, 12), INSN(19, 16), INSN(3, 0), INSN(11, 8));
}

static void smlalxy_t32(uint64_t _regs, uint32_t insn)
{
    smlalxy(_regs, INSN2(5, 5), INSN2(4, 4), INSN2(15, 12), INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void smulxy(uint64_t _regs, int n, int m, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int64_t operand1, operand2;
    int64_t res;

    operand1 = (int16_t)(n?regs->r[rn]>>16:regs->r[rn]);
    operand2 = (int16_t)(m?regs->r[rm]>>16:regs->r[rm]);

    res = operand1 * operand2;

    regs->r[rd] = res;
}

static void smulxy_a1(uint64_t _regs, uint32_t insn)
{
    smulxy(_regs, INSN(5, 5), INSN(6, 6), INSN(19, 16), INSN(3, 0), INSN(11, 8));
}

static void smulxy_t32(uint64_t _regs, uint32_t insn)
{
    smulxy(_regs, INSN2(5, 5), INSN2(4, 4), INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void smuad_smusd(uint64_t _regs, int is_sub, int m, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    uint32_t operand2 = regs->r[rm];
    int64_t product1, product2;
    int64_t res;

    if (m)
        operand2 = (operand2 >> 16) | (operand2 << 16);
    product1 = (int16_t)regs->r[rn] * (int16_t)operand2;
    product2 = (int16_t)(regs->r[rn] >> 16) * (int16_t)(operand2 >> 16);
    if (is_sub)
        res = product1 - product2;
    else
        res = product1 + product2;
    if (res != (int32_t) res)
        regs->cpsr |= 1 << 27;

    regs->r[rd] = res;
}

static void smuad_smusd_a1(uint64_t _regs, uint32_t insn)
{
    smuad_smusd(_regs, INSN(6, 6), INSN(5, 5), INSN(19, 16), INSN(3, 0), INSN(11, 8));
}

static void smuad_smusd_t32(uint64_t _regs, uint32_t insn)
{
    smuad_smusd(_regs, INSN1(6, 6), INSN2(4, 4), INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void smlad_smlsd(uint64_t _regs, int is_sub, int m, int rd, int rn, int rm, int ra)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    uint32_t operand2 = regs->r[rm];
    int64_t product1, product2;
    int64_t res;

    if (m)
        operand2 = (operand2 >> 16) | (operand2 << 16);
    product1 = (int16_t)regs->r[rn] * (int16_t)operand2;
    product2 = (int16_t)(regs->r[rn] >> 16) * (int16_t)(operand2 >> 16);

    if (is_sub)
        res = product1 - product2 + (int32_t)regs->r[ra];
    else
        res = product1 + product2 + (int32_t)regs->r[ra];
    if (res != (int32_t) res)
        regs->cpsr |= 1 << 27;

    regs->r[rd] = res;
}

static void smlad_smlsd_a1(uint64_t _regs, uint32_t insn)
{
    smlad_smlsd(_regs, INSN(6,6), INSN(5, 5), INSN(19, 16), INSN(3, 0), INSN(11, 8), INSN(15, 12));
}

static void smlad_smlsd_t32(uint64_t _regs, uint32_t insn)
{
    smlad_smlsd(_regs, INSN1(6,6), INSN2(4, 4), INSN2(11, 8), INSN1(3, 0), INSN2(3, 0), INSN2(15, 12));
}

static void smlald_smlsld(uint64_t _regs, int is_sub, int m, int rdlo, int rdhi, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    uint32_t operand2 = regs->r[rm];
    int64_t product1, product2;
    int64_t acc = ((uint64_t) regs->r[rdhi] << 32) + regs->r[rdlo];
    int64_t res;

    if (m)
        operand2 = (operand2 >> 16) | (operand2 << 16);
    product1 = (int16_t)regs->r[rn] * (int16_t)operand2;
    product2 = (int16_t)(regs->r[rn] >> 16) * (int16_t)(operand2 >> 16);

    if (is_sub)
        res = product1 - product2 + acc;
    else
        res = product1 + product2 + acc;
    if (res != (int32_t) res)
        regs->cpsr |= 1 << 27;

    regs->r[rdhi] = res >> 32;
    regs->r[rdlo] = res;
}

static void smlald_smlsld_a1(uint64_t _regs, uint32_t insn)
{
    smlald_smlsld(_regs, INSN(6, 6), INSN(5, 5), INSN(15, 12), INSN(19, 16), INSN(3, 0), INSN(11, 8));
}

static void smlald_smlsld_t32(uint64_t _regs, uint32_t insn)
{
    smlald_smlsld(_regs, INSN1(4, 4), INSN2(4, 4), INSN2(15, 12), INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void smmul(uint64_t _regs, int r, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int64_t product1 = (int32_t)regs->r[rn];
    int64_t product2 = (int32_t)regs->r[rm];
    int64_t res;

    res = product1 * product2;
    if (r)
        res += 0x80000000;

    regs->r[rd] = res >> 32;
}

static void smmul_a1(uint64_t _regs, uint32_t insn)
{
    smmul(_regs, INSN(5, 5), INSN(19, 16), INSN(3, 0), INSN(11, 8));
}

static void smmul_t32(uint64_t _regs, uint32_t insn)
{
    smmul(_regs, INSN2(4, 4), INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void smmla_smmls(uint64_t _regs, int is_sub, int r, int rd, int rn , int rm, int ra)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int64_t acc = (int64_t)regs->r[ra] << 32;
    int64_t product1 = (int32_t)regs->r[rn];
    int64_t product2 = (int32_t)regs->r[rm];
    int64_t res;

    if (is_sub)
        res = acc - product1 * product2;
    else
        res = acc + product1 * product2;
    if (r)
        res += 0x80000000;

    regs->r[rd] = res >> 32;
}

static void smmla_smmls_a1(uint64_t _regs, uint32_t insn)
{
    smmla_smmls(_regs, INSN(6, 6), INSN(5, 5), INSN(19, 16), INSN(3, 0), INSN(11, 8), INSN(15, 12));
}

static void smmla_smmls_t32(uint64_t _regs, uint32_t insn)
{
    smmla_smmls(_regs, INSN1(5, 5), INSN2(4, 4), INSN2(11, 8), INSN1(3, 0), INSN2(3, 0), INSN2(15, 12));
}

static void dis_ssat(uint64_t _regs, int saturate_to, int shift_mode, int shift_value, int rd, int rn)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int64_t operand;
    int64_t result;
    int isSat;

    switch(shift_mode) {
        case 0:
            operand = (int64_t)(int32_t)(regs->r[rn] << shift_value);
            break;
        case 2:
            operand = (int64_t)(int32_t)regs->r[rn] >> (shift_value?shift_value:32);
            break;
        default:
            fatal("Invalid shift mode %d\n", shift_mode);
    }

    result = signedSatQ(operand, saturate_to, &isSat);
    if (isSat)
        regs->cpsr |= 1 << 27;

    regs->r[rd] = result;
}

static void dis_ssat_a1(uint64_t _regs, uint32_t insn)
{
    dis_ssat(_regs, INSN(20, 16) + 1, INSN(6, 5), INSN(11, 7), INSN(15, 12), INSN(3, 0));
}

static void dis_ssat_t32(uint64_t _regs, uint32_t insn)
{
    dis_ssat(_regs, INSN2(4, 0) + 1, INSN1(5, 4), (INSN2(14, 12) << 2) | INSN2(7, 6), INSN2(11, 8), INSN1(3, 0));
}

static void dis_usat(uint64_t _regs, int saturate_to, int shift_mode, int shift_value, int rd, int rn)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int64_t operand;
    int64_t result;
    int isSat;

    switch(shift_mode) {
        case 0:
            operand = (int64_t)(int32_t)(regs->r[rn] << shift_value);
            break;
        case 2:
            operand = (int64_t)(int32_t)regs->r[rn] >> (shift_value?shift_value:32);
            break;
        default:
            fatal("Invalid shift mode %d\n", shift_mode);
    }

    result = unsignedSatQ(operand, saturate_to, &isSat);
    if (isSat)
        regs->cpsr |= 1 << 27;

    regs->r[rd] = result;
}

static void dis_usat_a1(uint64_t _regs, uint32_t insn)
{
    dis_usat(_regs, INSN(20, 16), INSN(6, 5), INSN(11, 7), INSN(15, 12), INSN(3, 0));
}

static void dis_usat_t32(uint64_t _regs, uint32_t insn)
{
    dis_usat(_regs, INSN2(4, 0), INSN1(5, 4), (INSN2(14, 12) << 2) | INSN2(7, 6), INSN2(11, 8), INSN1(3, 0));
}

static void dis_ssat16(uint64_t _regs, int saturate_to, int rd, int rn)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int64_t result1, result2;
    int isSat1, isSat2;

    result1 = signedSatQ((int64_t)(int16_t)regs->r[rn], saturate_to, &isSat1);
    result2 = signedSatQ((int64_t)(int16_t)(regs->r[rn] >> 16), saturate_to, &isSat2);
    if (isSat1 || isSat2)
        regs->cpsr |= 1 << 27;

    regs->r[rd] = (result2 << 16) | (result1 & 0xffff);
}

static void dis_ssat16_a1(uint64_t _regs, uint32_t insn)
{
    dis_ssat16(_regs, INSN(19, 16) + 1, INSN(15, 12), INSN(3, 0));
}

static void dis_ssat16_t32(uint64_t _regs, uint32_t insn)
{
    dis_ssat16(_regs, INSN2(3, 0) + 1, INSN2(11, 8), INSN1(3, 0));
}

static void dis_usat16(uint64_t _regs, int saturate_to, int rd, int rn)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int64_t result1, result2;
    int isSat1, isSat2;

    result1 = unsignedSatQ((int64_t)(int16_t)regs->r[rn], saturate_to, &isSat1);
    result2 = unsignedSatQ((int64_t)(int16_t)(regs->r[rn] >> 16), saturate_to, &isSat2);
    if (isSat1 || isSat2)
        regs->cpsr |= 1 << 27;

    regs->r[rd] = (result2 << 16) | (result1 & 0xffff);
}

static void dis_usat16_a1(uint64_t _regs, uint32_t insn)
{
    dis_usat16(_regs, INSN(19, 16), INSN(15, 12), INSN(3, 0));
}

static void dis_usat16_t32(uint64_t _regs, uint32_t insn)
{
    dis_usat16(_regs, INSN2(3, 0), INSN2(11, 8), INSN1(3, 0));
}

static void usad8_usada8(uint64_t _regs, int rd, int rn, int rm, int ra)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    uint32_t result = 0;
    int i;

    for(i = 0; i < 4; i++) {
        result += abs(((regs->r[rn] >> (i * 8)) & 0xff) - ((regs->r[rm] >> (i * 8)) & 0xff));
    }
    if (ra != 15)
        result += regs->r[ra];

    regs->r[rd] = result;
}

static void usad8_usada8_t32(uint64_t _regs, uint32_t insn)
{
    usad8_usada8(_regs, INSN2(11, 8), INSN1(3, 0), INSN2(3, 0), INSN2(15, 12));
}

static void dis_common_vcmp_vcmpe_vfp(uint64_t _regs, uint32_t insn)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int D = INSN(22, 22);
    int is_with_zero = INSN(16, 16);
    int vd = INSN(15, 12);
    int is_double = INSN(8, 8);
    //int E = INSN(7, 7);
    int M = INSN(5, 5);
    int vm = INSN(3, 0);
    int d, m;
    uint32_t fpscr_nzcv = 0;

    if (is_double) {
        d = (D << 4) + vd;
        m = (M << 4) + vm;
    } else {
        d = (vd << 1) + D;
        m = (vm << 1) + M;
    }
    if (is_double) {
        double op1 = regs->e.df[d];
        double op2 = is_with_zero?0.0:regs->e.df[m];

        if (isnan(op1) || isnan(op2))
            fpscr_nzcv = 0x30000000;
        else if (op1 == op2)
            fpscr_nzcv = 0x60000000;
        else if (op1 > op2)
            fpscr_nzcv = 0x20000000;
        else
            fpscr_nzcv = 0x80000000;
    } else {
        float op1 = regs->e.sf[d];
        float op2 = is_with_zero?0.0:regs->e.sf[m];

        if (__isnanf(op1) || __isnanf(op2))
            fpscr_nzcv = 0x30000000;
        else if (op1 == op2)
            fpscr_nzcv = 0x60000000;
        else if (op1 > op2)
            fpscr_nzcv = 0x20000000;
        else
            fpscr_nzcv = 0x80000000;
    }

    regs->fpscr = (regs->fpscr & 0x0fffffff) + fpscr_nzcv;
}

static void dis_common_vcvt_double_single_vfp(uint64_t _regs, uint32_t insn)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int D = INSN(22, 22);
    int vd = INSN(15, 12);
    int is_double_to_single = INSN(8, 8);
    int M = INSN(5, 5);
    int vm = INSN(3, 0);
    int d, m;

    if (is_double_to_single) {
        d = (vd << 1) + D;
        m = (M << 4) + vm;

        regs->e.sf[d] = regs->e.df[m];
    } else {
        d = (D << 4) + vd;
        m = (vm << 1) + M;

        regs->e.df[d] = regs->e.sf[m];
    }
}

static void dis_common_vcvt_vcvtr_floating_integer_vfp(uint64_t _regs, uint32_t insn)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int D = INSN(22, 22);
    int is_to_integer = INSN(18, 18);
    int vd = INSN(15, 12);
    int is_double = INSN(8, 8);
    int op = INSN(7, 7);
    int M = INSN(5, 5);
    int vm = INSN(3, 0);
    int is_signed;
    int d;
    int m;

    if (is_to_integer) {
        is_signed = INSN(16, 16);
        d = (vd << 1) + D;
        m = is_double?((M << 4) + vm):((vm << 1) + M);

        assert(op == 1);
        if (is_double) {
            if (is_signed)
                regs->e.s[d] = (int32_t) ssat32_d(regs->e.df[m]);
            else
                regs->e.s[d] = (uint32_t) usat32_d(regs->e.df[m]);
        } else {
            if (is_signed)
                regs->e.s[d] = (int32_t) ssat32_d(regs->e.sf[m]);
            else
                regs->e.s[d] = (uint32_t) usat32_d(regs->e.sf[m]);
        }
    } else {
        is_signed = op;
        m = (vm << 1) + M;
        d = is_double?((D << 4) + vd):((vd << 1) + D);

        if (is_double) {
            if (is_signed)
                regs->e.df[d] = (int32_t) regs->e.s[m];
            else
                regs->e.df[d] = (uint32_t) regs->e.s[m];
        } else {
            if (is_signed)
                regs->e.sf[d] = (int32_t) regs->e.s[m];
            else
                regs->e.sf[d] = (uint32_t) regs->e.s[m];
        }
    }
}

static void dis_common_vqadd_simd(uint64_t _regs, uint32_t insn, uint32_t is_unsigned)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int d = (INSN(22, 22) << 4) | INSN(15, 12);
    int n = (INSN(7, 7) << 4) | INSN(19, 16);
    int m = (INSN(5, 5) << 4) | INSN(3, 0);
    int size = INSN(21, 20);
    int reg_nb = INSN(6, 6) + 1;
    int i;
    int r;
    union simd_d_register res[2];

    switch(size) {
        case 0:
            for(r = 0; r < reg_nb; r++) {
                for(i = 0; i < 8; i++) {
                    if (is_unsigned)
                        res[r].u8[i] = usat8_u(regs, regs->e.simd[n + r].u8[i] + regs->e.simd[m + r].u8[i]);
                    else
                        res[r].s8[i] = ssat8(regs, regs->e.simd[n + r].s8[i] + regs->e.simd[m + r].s8[i]);
                }
            }
            break;
        case 1:
            for(r = 0; r < reg_nb; r++) {
                for(i = 0; i < 4; i++) {
                    if (is_unsigned)
                        res[r].u16[i] = usat16_u(regs, regs->e.simd[n + r].u16[i] + regs->e.simd[m + r].u16[i]);
                    else
                        res[r].s16[i] = ssat16(regs, regs->e.simd[n + r].s16[i] + regs->e.simd[m + r].s16[i]);
                }
            }
            break;
        case 2:
            for(r = 0; r < reg_nb; r++) {
                for(i = 0; i < 4; i++) {
                    if (is_unsigned)
                        res[r].u32[i] = usat32_u(regs, (uint64_t)regs->e.simd[n + r].u32[i] + (uint64_t)regs->e.simd[m + r].u32[i]);
                    else
                        res[r].s32[i] = ssat32(regs, (int64_t)regs->e.simd[n + r].s32[i] + (int64_t)regs->e.simd[m + r].s32[i]);
                }
            }
            break;
        case 3:
            for(r = 0; r < reg_nb; r++) {
                for(i = 0; i < 2; i++) {
                    if (is_unsigned)
                        res[r].u64[i] = usat64_u(regs, (__uint128_t)regs->e.simd[n + r].u64[i] + (__uint128_t)regs->e.simd[m + r].u64[i]);
                    else
                        res[r].s64[i] = ssat64(regs, (__int128_t)regs->e.simd[n + r].s64[i] + (__int128_t)regs->e.simd[m + r].s64[i]);
                }
            }
            break;
        default:
            fatal("size = %d\n", size);
    }

    for(r = 0; r < reg_nb; r++)
        regs->e.simd[d + r] = res[r];
}

static void dis_common_vhadd_vhsub_simd(uint64_t _regs, uint32_t insn, uint32_t is_unsigned)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int d = (INSN(22, 22) << 4) | INSN(15, 12);
    int n = (INSN(7, 7) << 4) | INSN(19, 16);
    int m = (INSN(5, 5) << 4) | INSN(3, 0);
    int size = INSN(21, 20);
    int reg_nb = INSN(6, 6) + 1;
    int is_sub = INSN(9, 9);
    int i;
    int r;
    union simd_d_register res[2];

    switch(size) {
        case 0:
            for(r = 0; r < reg_nb; r++) {
                for(i = 0; i < 8; i++) {
                    if (is_unsigned)
                        res[r].u8[i] = (regs->e.simd[n + r].u8[i] + (is_sub?-regs->e.simd[m + r].u8[i]:regs->e.simd[m + r].u8[i])) >> 1;
                    else
                        res[r].s8[i] = (regs->e.simd[n + r].s8[i] + (is_sub?-regs->e.simd[m + r].s8[i]:regs->e.simd[m + r].s8[i])) >> 1;
                }
            }
            break;
        case 1:
            for(r = 0; r < reg_nb; r++) {
                for(i = 0; i < 4; i++) {
                    if (is_unsigned)
                        res[r].u16[i] = (regs->e.simd[n + r].u16[i] + (is_sub?-regs->e.simd[m + r].u16[i]:regs->e.simd[m + r].u16[i])) >> 1;
                    else
                        res[r].s16[i] = (regs->e.simd[n + r].s16[i] + (is_sub?-regs->e.simd[m + r].s16[i]:regs->e.simd[m + r].s16[i])) >> 1;
                }
            }
            break;
        case 2:
            for(r = 0; r < reg_nb; r++) {
                for(i = 0; i < 2; i++) {
                    if (is_unsigned)
                        res[r].u32[i] = ((uint64_t)regs->e.simd[n + r].u32[i] + (uint64_t)(is_sub?-regs->e.simd[m + r].u32[i]:regs->e.simd[m + r].u32[i])) >> 1;
                    else
                        res[r].s32[i] = ((int64_t)regs->e.simd[n + r].s32[i] + (int64_t)(is_sub?-regs->e.simd[m + r].s32[i]:regs->e.simd[m + r].s32[i])) >> 1;
                }
            }
            break;
        default:
            fatal("size = %d\n", size);
    }

    for(r = 0; r < reg_nb; r++)
        regs->e.simd[d + r] = res[r];
}

static void dis_common_vqrshl_simd(uint64_t _regs, uint32_t insn, uint32_t is_unsigned)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int d = (INSN(22, 22) << 4) | INSN(15, 12);
    int n = (INSN(7, 7) << 4) | INSN(19, 16);
    int m = (INSN(5, 5) << 4) | INSN(3, 0);
    int size = INSN(21, 20);
    int reg_nb = INSN(6, 6) + 1;
    int i;
    int r;
    union simd_d_register res[2];
    __uint128_t u_res_before_sat;
    __int128_t s_res_before_sat;

    switch(size) {
        case 0:
            for(r = 0; r < reg_nb; r++) {
                for(i = 0; i < 8; i++) {
                    int8_t shift = regs->e.simd[n + r].s8[i];
                    if (is_unsigned) {
                        u_res_before_sat = ushl((__uint128_t) regs->e.simd[m + r].u8[i], shift, 1);
                        res[r].u8[i] = usat8_u(regs, usat64_u(regs,u_res_before_sat));
                    } else {
                        s_res_before_sat = sshl((__int128_t) regs->e.simd[m + r].s8[i], shift, 1);
                        res[r].s8[i] = ssat8(regs, ssat64(regs,s_res_before_sat));
                    }
                }
            }
            break;
        case 1:
            for(r = 0; r < reg_nb; r++) {
                for(i = 0; i < 4; i++) {
                    int8_t shift = regs->e.simd[n + r].s16[i];
                    if (is_unsigned) {
                        u_res_before_sat = ushl((__uint128_t) regs->e.simd[m + r].u16[i], shift, 1);
                        res[r].u16[i] = usat16_u(regs, usat64_u(regs,u_res_before_sat));
                    } else {
                        s_res_before_sat = sshl((__int128_t) regs->e.simd[m + r].s16[i], shift, 1);
                        res[r].s16[i] = ssat16(regs, ssat64(regs,s_res_before_sat));
                    }
                }
            }
            break;
        case 2:
            for(r = 0; r < reg_nb; r++) {
                for(i = 0; i < 2; i++) {
                    int8_t shift = regs->e.simd[n + r].s32[i];
                    if (is_unsigned) {
                        u_res_before_sat = ushl((__uint128_t) regs->e.simd[m + r].u32[i], shift, 1);
                        res[r].u32[i] = usat32_u(regs, usat64_u(regs,u_res_before_sat));
                    } else {
                        s_res_before_sat = sshl((__int128_t) regs->e.simd[m + r].s32[i], shift, 1);
                        res[r].s32[i] = ssat32(regs, ssat64(regs,s_res_before_sat));
                    }
                }
            }
            break;
        case 3:
            for(r = 0; r < reg_nb; r++) {
                for(i = 0; i < 1; i++) {
                    int8_t shift = regs->e.simd[n + r].s64[i];
                    if (is_unsigned) {
                        u_res_before_sat = ushl((__uint128_t) regs->e.simd[m + r].u64[i], shift, 1);
                        res[r].u64[i] = usat64_u(regs, usat64_u(regs,u_res_before_sat));
                    } else {
                        s_res_before_sat = sshl((__int128_t) regs->e.simd[m + r].s64[i], shift, 1);
                        res[r].s64[i] = ssat64(regs,s_res_before_sat);
                    }
                }
            }
            break;
        default:
            fatal("size = %d\n", size);
    }

    for(r = 0; r < reg_nb; r++)
        regs->e.simd[d + r] = res[r];
}

static void dis_common_vmax_vmin_simd(uint64_t _regs, uint32_t insn, uint32_t is_unsigned)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int d = (INSN(22, 22) << 4) | INSN(15, 12);
    int n = (INSN(7, 7) << 4) | INSN(19, 16);
    int m = (INSN(5, 5) << 4) | INSN(3, 0);
    int size = INSN(21, 20);
    int reg_nb = INSN(6, 6) + 1;
    int is_min = INSN(4, 4);
    int i;
    int r;
    union simd_d_register res[2];

    switch(size) {
        case 0:
            for(r = 0; r < reg_nb; r++) {
                for(i = 0; i < 8; i++) {
                    if (is_unsigned) {
                        uint8_t (*minmax)(uint8_t,uint8_t) = is_min?umin8:umax8;
                        res[r].u8[i] = (*minmax)(regs->e.simd[n + r].u8[i],regs->e.simd[m + r].u8[i]);
                    } else {
                        uint8_t (*minmax)(int8_t,int8_t) = is_min?smin8:smax8;
                        res[r].s8[i] = (*minmax)(regs->e.simd[n + r].s8[i],regs->e.simd[m + r].s8[i]);
                    }
                }
            }
            break;
        case 1:
            for(r = 0; r < reg_nb; r++) {
                for(i = 0; i < 4; i++) {
                    if (is_unsigned) {
                        uint16_t (*minmax)(uint16_t,uint16_t) = is_min?umin16:umax16;
                        res[r].u16[i] = (*minmax)(regs->e.simd[n + r].u16[i],regs->e.simd[m + r].u16[i]);
                    } else {
                        uint16_t (*minmax)(int16_t,int16_t) = is_min?smin16:smax16;
                        res[r].s16[i] = (*minmax)(regs->e.simd[n + r].s16[i],regs->e.simd[m + r].s16[i]);
                    }
                }
            }
            break;
        case 2:
            for(r = 0; r < reg_nb; r++) {
                for(i = 0; i < 2; i++) {
                    if (is_unsigned) {
                        uint32_t (*minmax)(uint32_t,uint32_t) = is_min?umin32:umax32;
                        res[r].u32[i] = (*minmax)(regs->e.simd[n + r].u32[i],regs->e.simd[m + r].u32[i]);
                    } else {
                        uint32_t (*minmax)(int32_t,int32_t) = is_min?smin32:smax32;
                        res[r].s32[i] = (*minmax)(regs->e.simd[n + r].s32[i],regs->e.simd[m + r].s32[i]);
                    }
                }
            }
            break;
        default:
            fatal("size = %d\n", size);
    }

    for(r = 0; r < reg_nb; r++)
        regs->e.simd[d + r] = res[r];
}

static void dis_common_vpmax_vpmin_fpu_simd(uint64_t _regs, uint32_t insn)
{
    /* FIXME: need to properly handle exception cases */
#if 0
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int d = (INSN(22, 22) << 4) | INSN(15, 12);
    int n = (INSN(7, 7) << 4) | INSN(19, 16);
    int m = (INSN(5, 5) << 4) | INSN(3, 0);
    int reg_nb = INSN(6, 6) + 1;
    int is_min = INSN(21, 21);
    int i;
    int r;
    union simd_d_register res[2];

    for(r = 0; r < reg_nb; r++) {
        for(i = 0; i < 1; i++) {
            if (is_min) {
                res[r].sf[i + 0] = minf(regs->e.simd[n + r].sf[2 * i], regs->e.simd[n + r].sf[2 * i + 1]);
                res[r].sf[i + 1] = minf(regs->e.simd[m + r].sf[2 * i], regs->e.simd[m + r].sf[2 * i + 1]);
            } else {
                res[r].sf[i + 0] = maxf(regs->e.simd[n + r].sf[2 * i], regs->e.simd[n + r].sf[2 * i + 1]);
                res[r].sf[i + 1] = maxf(regs->e.simd[m + r].sf[2 * i], regs->e.simd[m + r].sf[2 * i + 1]);
            }
        }
    }

    for(r = 0; r < reg_nb; r++)
        regs->e.simd[d + r] = res[r];
#else
    assert(0);
#endif
}

static void dis_common_vmax_vmin_fpu_simd(uint64_t _regs, uint32_t insn)
{
    /* FIXME: need to properly handle exception cases */
#if 0
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int d = (INSN(22, 22) << 4) | INSN(15, 12);
    int n = (INSN(7, 7) << 4) | INSN(19, 16);
    int m = (INSN(5, 5) << 4) | INSN(3, 0);
    int reg_nb = INSN(6, 6) + 1;
    int is_min = INSN(21, 21);
    int i;
    int r;
    union simd_d_register res[2];

    for(r = 0; r < reg_nb; r++) {
        for(i = 0; i < 2; i++) {
            if (is_min)
                res[r].sf[i] = minf(regs->e.simd[n + r].sf[i], regs->e.simd[m + r].sf[i]);
            else
                res[r].sf[i] = maxf(regs->e.simd[n + r].sf[i], regs->e.simd[m + r].sf[i]);
        }
    }

    for(r = 0; r < reg_nb; r++)
        regs->e.simd[d + r] = res[r];
#else
    assert(0);
#endif
}

static void dis_common_vaba_vabd_simd(uint64_t _regs, uint32_t insn, uint32_t is_thumb)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int d = (INSN(22, 22) << 4) | INSN(15, 12);
    int n = (INSN(7, 7) << 4) | INSN(19, 16);
    int m = (INSN(5, 5) << 4) | INSN(3, 0);
    int size = INSN(21, 20);
    int U = is_thumb?INSN(28, 28):INSN(24, 24);
    int reg_nb = INSN(6, 6) + 1;
    int is_acc = INSN(4, 4);
    int i;
    int r;
    union simd_d_register res[2];

    switch(size) {
        case 0:
            for(r = 0; r < reg_nb; r++) {
                for(i = 0; i < 8; i++) {
                    uint32_t absdiff;
                    if (U)
                        absdiff = abs(regs->e.simd[n + r].u8[i] - regs->e.simd[m + r].u8[i]);
                    else
                        absdiff = abs(regs->e.simd[n + r].s8[i] - regs->e.simd[m + r].s8[i]);
                    res[r].u8[i] = (is_acc?regs->e.simd[d + r].u8[i]:0) + absdiff;
                }
            }
            break;
        case 1:
            for(r = 0; r < reg_nb; r++) {
                for(i = 0; i < 4; i++) {
                    uint32_t absdiff;
                    if (U)
                        absdiff = abs(regs->e.simd[n + r].u16[i] - regs->e.simd[m + r].u16[i]);
                    else
                        absdiff = abs(regs->e.simd[n + r].s16[i] - regs->e.simd[m + r].s16[i]);
                    res[r].u16[i] = (is_acc?regs->e.simd[d + r].u16[i]:0) + absdiff;
                }
            }
            break;
        case 2:
            for(r = 0; r < reg_nb; r++) {
                for(i = 0; i < 2; i++) {
                    uint64_t absdiff;
                    if (U)
                        absdiff = labs((uint64_t) regs->e.simd[n + r].u32[i] - (uint64_t) regs->e.simd[m + r].u32[i]);
                    else
                        absdiff = labs((int64_t) regs->e.simd[n + r].s32[i] - (int64_t) regs->e.simd[m + r].s32[i]);
                    res[r].u32[i] = (is_acc?regs->e.simd[d + r].u32[i]:0) + absdiff;
                }
            }
            break;
        default:
            assert(0);
    }

    for(r = 0; r < reg_nb; r++)
        regs->e.simd[d + r] = res[r];
}

/* This macro will define 4 functions use to implement vcxx opcodes
*/
#define VCXX_SIMD(name, op) \
static void dis_common_ ## name ## _all_simd(uint64_t _regs, uint32_t insn, int is_zero, int is_unsigned) \
{ \
    struct arm_registers *regs = (struct arm_registers *) _regs; \
    int d = (INSN(22, 22) << 4) | INSN(15, 12); \
    int n = (INSN(7, 7) << 4) | INSN(19, 16); \
    int m = (INSN(5, 5) << 4) | INSN(3, 0); \
    int size = is_zero?INSN(19, 18):INSN(21, 20); \
    int f = INSN(10, 10); \
    int reg_nb = INSN(6, 6) + 1; \
    int i; \
    int r; \
    union simd_d_register res[2]; \
 \
    /* for immediate 0 case. n is not use and m is op1. Just use n to avoid swapping logic */ \
    if (is_zero) \
        n = m; \
 \
    if (f) { \
        for(r = 0; r < reg_nb; r++) \
            for(i = 0; i < 2; i++) \
                res[r].u32[i] = regs->e.simd[n + r].sf[i] op (is_zero?0:regs->e.simd[m + r].sf[i])?~0:0; \
    } else { \
        switch(size) { \
            case 0: \
                for(r = 0; r < reg_nb; r++) \
                    for(i = 0; i < 8; i++) \
                        if (is_unsigned) \
                            res[r].u8[i] = regs->e.simd[n + r].u8[i] op (is_zero?0:regs->e.simd[m + r].u8[i])?~0:0; \
                        else \
                            res[r].s8[i] = regs->e.simd[n + r].s8[i] op (is_zero?0:regs->e.simd[m + r].s8[i])?~0:0; \
                break; \
            case 1: \
                for(r = 0; r < reg_nb; r++) \
                    for(i = 0; i < 4; i++) \
                        if (is_unsigned) \
                            res[r].u16[i] = regs->e.simd[n + r].u16[i] op (is_zero?0:regs->e.simd[m + r].u16[i])?~0:0; \
                        else \
                            res[r].s16[i] = regs->e.simd[n + r].s16[i] op (is_zero?0:regs->e.simd[m + r].s16[i])?~0:0; \
                break; \
            case 2: \
                for(r = 0; r < reg_nb; r++) \
                    for(i = 0; i < 2; i++) \
                        if (is_unsigned) \
                            res[r].u32[i] = regs->e.simd[n + r].u32[i] op (is_zero?0:regs->e.simd[m + r].u32[i])?~0:0; \
                        else \
                            res[r].s32[i] = regs->e.simd[n + r].s32[i] op (is_zero?0:regs->e.simd[m + r].s32[i])?~0:0; \
                break; \
            default: \
                assert(0); \
        } \
    } \
 \
    for(r = 0; r < reg_nb; r++) \
        regs->e.simd[d + r] = res[r]; \
} \
 \
static void dis_common_ ## name ## _simd(uint64_t _regs, uint32_t insn, int is_unsigned) __attribute__ ((unused)); \
static void dis_common_ ## name ## _simd(uint64_t _regs, uint32_t insn, int is_unsigned) \
{ \
    dis_common_ ## name ## _all_simd(_regs, insn, 0, is_unsigned); \
} \
 \
static void dis_common_ ## name ## _immediate_simd(uint64_t _regs, uint32_t insn) \
{ \
    dis_common_ ## name ## _all_simd(_regs, insn, 1, 0); \
} \
 \
static void dis_common_ ## name ## _fpu_simd(uint64_t _regs, uint32_t insn) __attribute__ ((unused)); \
static void dis_common_ ## name ## _fpu_simd(uint64_t _regs, uint32_t insn) \
{ \
    dis_common_ ## name ## _all_simd(_regs, insn, 0, 0/*is_unsigned*/); \
}

VCXX_SIMD(vceq, ==)
VCXX_SIMD(vcge, >=)
VCXX_SIMD(vcgt, >)
VCXX_SIMD(vcle, <=)
VCXX_SIMD(vclt, <)

static void dis_common_vadd_simd(uint64_t _regs, uint32_t insn)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int d = (INSN(22, 22) << 4) | INSN(15, 12);
    int n = (INSN(7, 7) << 4) | INSN(19, 16);
    int m = (INSN(5, 5) << 4) | INSN(3, 0);
    int size = INSN(21, 20);
    int reg_nb = INSN(6, 6) + 1;
    int i;
    int r;
    union simd_d_register res[2];

    switch(size) {
        case 0:
            for(r = 0; r < reg_nb; r++)
                for(i = 0; i < 8; i++)
                    res[r].u8[i] = regs->e.simd[n + r].u8[i] + regs->e.simd[m + r].u8[i];
            break;
        case 1:
            for(r = 0; r < reg_nb; r++)
                for(i = 0; i < 4; i++)
                    res[r].u16[i] = regs->e.simd[n + r].u16[i] + regs->e.simd[m + r].u16[i];
            break;
        case 2:
            for(r = 0; r < reg_nb; r++)
                for(i = 0; i < 2; i++)
                    res[r].u32[i] = regs->e.simd[n + r].u32[i] + regs->e.simd[m + r].u32[i];
            break;
        case 3:
            for(r = 0; r < reg_nb; r++)
                for(i = 0; i < 1; i++)
                    res[r].u64[i] = regs->e.simd[n + r].u64[i] + regs->e.simd[m + r].u64[i];
            break;
        default:
            assert(0);
    }

    for(r = 0; r < reg_nb; r++)
        regs->e.simd[d + r] = res[r];
}

static void dis_common_vmul_polynomial_simd(uint64_t _regs, uint32_t insn)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int d = (INSN(22, 22) << 4) | INSN(15, 12);
    int n = (INSN(7, 7) << 4) | INSN(19, 16);
    int m = (INSN(5, 5) << 4) | INSN(3, 0);
    int size = INSN(21, 20);
    int reg_nb = INSN(6, 6) + 1;
    int i;
    int r;
    union simd_d_register res[2];

    switch(size) {
        case 0:
            for(r = 0; r < reg_nb; r++)
                for(i = 0; i < 8; i++)
                    res[r].u8[i] = polynomial_16(regs->e.simd[n + r].u8[i], regs->e.simd[m + r].u8[i]);
            break;
        default:
            assert(0);
    }

    for(r = 0; r < reg_nb; r++)
        regs->e.simd[d + r] = res[r];
}

static void dis_common_vmul_integer_simd(uint64_t _regs, uint32_t insn)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int d = (INSN(22, 22) << 4) | INSN(15, 12);
    int n = (INSN(7, 7) << 4) | INSN(19, 16);
    int m = (INSN(5, 5) << 4) | INSN(3, 0);
    int size = INSN(21, 20);
    int reg_nb = INSN(6, 6) + 1;
    int i;
    int r;
    union simd_d_register res[2];

    switch(size) {
        case 0:
            for(r = 0; r < reg_nb; r++)
                for(i = 0; i < 8; i++)
                    res[r].u8[i] = regs->e.simd[n + r].u8[i] * regs->e.simd[m + r].u8[i];
            break;
        case 1:
            for(r = 0; r < reg_nb; r++)
                for(i = 0; i < 4; i++)
                    res[r].u16[i] = regs->e.simd[n + r].u16[i] * regs->e.simd[m + r].u16[i];
            break;
        case 2:
            for(r = 0; r < reg_nb; r++)
                for(i = 0; i < 2; i++)
                    res[r].u32[i] = regs->e.simd[n + r].u32[i] * regs->e.simd[m + r].u32[i];
            break;
        default:
            assert(0);
    }

    for(r = 0; r < reg_nb; r++)
        regs->e.simd[d + r] = res[r];
}

static void dis_common_vmla_vmls_simd(uint64_t _regs, uint32_t insn, int is_sub)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int d = (INSN(22, 22) << 4) | INSN(15, 12);
    int n = (INSN(7, 7) << 4) | INSN(19, 16);
    int m = (INSN(5, 5) << 4) | INSN(3, 0);
    int size = INSN(21, 20);
    int reg_nb = INSN(6, 6) + 1;
    int i;
    int r;
    union simd_d_register res[2];

    switch(size) {
        case 0:
            for(r = 0; r < reg_nb; r++)
                for(i = 0; i < 8; i++) {
                    uint32_t product = regs->e.simd[n + r].u8[i] * regs->e.simd[m + r].u8[i];
                    res[r].u8[i] = regs->e.simd[d + r].u8[i] + (is_sub?-product:product);
                }
            break;
        case 1:
            for(r = 0; r < reg_nb; r++)
                for(i = 0; i < 4; i++) {
                    uint32_t product = regs->e.simd[n + r].u16[i] * regs->e.simd[m + r].u16[i];
                    res[r].u16[i] = regs->e.simd[d + r].u16[i] + (is_sub?-product:product);
                }
            break;
        case 2:
            for(r = 0; r < reg_nb; r++)
                for(i = 0; i < 2; i++) {
                    uint64_t product = regs->e.simd[n + r].u32[i] * regs->e.simd[m + r].u32[i];
                    res[r].u32[i] = regs->e.simd[d + r].u32[i] + (is_sub?-product:product);
                }
            break;
        default:
            assert(0);
    }

    for(r = 0; r < reg_nb; r++)
        regs->e.simd[d + r] = res[r];
}

static void dis_common_vpmin_simd(uint64_t _regs, uint32_t insn, int is_unsigned)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int d = (INSN(22, 22) << 4) | INSN(15, 12);
    int n = (INSN(7, 7) << 4) | INSN(19, 16);
    int m = (INSN(5, 5) << 4) | INSN(3, 0);
    int size = INSN(21, 20);
    int i;
    union simd_d_register res;

    switch(size) {
        case 0:
            for(i = 0; i < 4; i++) {
                if (is_unsigned) {
                    res.u8[i + 0] = umin8(regs->e.simd[n].u8[2 * i], regs->e.simd[n].u8[2 * i + 1]);
                    res.u8[i + 4] = umin8(regs->e.simd[m].u8[2 * i], regs->e.simd[m].u8[2 * i + 1]);
                } else {
                    res.u8[i + 0] = smin8(regs->e.simd[n].u8[2 * i], regs->e.simd[n].u8[2 * i + 1]);
                    res.u8[i + 4] = smin8(regs->e.simd[m].u8[2 * i], regs->e.simd[m].u8[2 * i + 1]);
                }
            }
            break;
        case 1:
            for(i = 0; i < 2; i++) {
                if (is_unsigned) {
                    res.u16[i + 0] = umin16(regs->e.simd[n].u16[2 * i], regs->e.simd[n].u16[2 * i + 1]);
                    res.u16[i + 2] = umin16(regs->e.simd[m].u16[2 * i], regs->e.simd[m].u16[2 * i + 1]);
                } else {
                    res.u16[i + 0] = smin16(regs->e.simd[n].u16[2 * i], regs->e.simd[n].u16[2 * i + 1]);
                    res.u16[i + 2] = smin16(regs->e.simd[m].u16[2 * i], regs->e.simd[m].u16[2 * i + 1]);
                }
            }
            break;
        case 2:
            for(i = 0; i < 1; i++) {
                if (is_unsigned) {
                    res.u32[i + 0] = umin32(regs->e.simd[n].u32[2 * i], regs->e.simd[n].u32[2 * i + 1]);
                    res.u32[i + 1] = umin32(regs->e.simd[m].u32[2 * i], regs->e.simd[m].u32[2 * i + 1]);
                } else {
                    res.u32[i + 0] = smin32(regs->e.simd[n].u32[2 * i], regs->e.simd[n].u32[2 * i + 1]);
                    res.u32[i + 1] = smin32(regs->e.simd[m].u32[2 * i], regs->e.simd[m].u32[2 * i + 1]);
                }
            }
            break;
        default:
            assert(0);
    }

    regs->e.simd[d] = res;
}

static void dis_common_vpmax_simd(uint64_t _regs, uint32_t insn, int is_unsigned)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int d = (INSN(22, 22) << 4) | INSN(15, 12);
    int n = (INSN(7, 7) << 4) | INSN(19, 16);
    int m = (INSN(5, 5) << 4) | INSN(3, 0);
    int size = INSN(21, 20);
    int i;
    union simd_d_register res;

    switch(size) {
        case 0:
            for(i = 0; i < 4; i++) {
                if (is_unsigned) {
                    res.u8[i + 0] = umax8(regs->e.simd[n].u8[2 * i], regs->e.simd[n].u8[2 * i + 1]);
                    res.u8[i + 4] = umax8(regs->e.simd[m].u8[2 * i], regs->e.simd[m].u8[2 * i + 1]);
                } else {
                    res.u8[i + 0] = smax8(regs->e.simd[n].u8[2 * i], regs->e.simd[n].u8[2 * i + 1]);
                    res.u8[i + 4] = smax8(regs->e.simd[m].u8[2 * i], regs->e.simd[m].u8[2 * i + 1]);
                }
            }
            break;
        case 1:
            for(i = 0; i < 2; i++) {
                if (is_unsigned) {
                    res.u16[i + 0] = umax16(regs->e.simd[n].u16[2 * i], regs->e.simd[n].u16[2 * i + 1]);
                    res.u16[i + 2] = umax16(regs->e.simd[m].u16[2 * i], regs->e.simd[m].u16[2 * i + 1]);
                } else {
                    res.u16[i + 0] = smax16(regs->e.simd[n].u16[2 * i], regs->e.simd[n].u16[2 * i + 1]);
                    res.u16[i + 2] = smax16(regs->e.simd[m].u16[2 * i], regs->e.simd[m].u16[2 * i + 1]);
                }
            }
            break;
        case 2:
            for(i = 0; i < 1; i++) {
                if (is_unsigned) {
                    res.u32[i + 0] = umax32(regs->e.simd[n].u32[2 * i], regs->e.simd[n].u32[2 * i + 1]);
                    res.u32[i + 1] = umax32(regs->e.simd[m].u32[2 * i], regs->e.simd[m].u32[2 * i + 1]);
                } else {
                    res.u32[i + 0] = smax32(regs->e.simd[n].u32[2 * i], regs->e.simd[n].u32[2 * i + 1]);
                    res.u32[i + 1] = smax32(regs->e.simd[m].u32[2 * i], regs->e.simd[m].u32[2 * i + 1]);
                }
            }
            break;
        default:
            assert(0);
    }

    regs->e.simd[d] = res;
}

static void dis_common_vpadd_simd(uint64_t _regs, uint32_t insn)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int d = (INSN(22, 22) << 4) | INSN(15, 12);
    int n = (INSN(7, 7) << 4) | INSN(19, 16);
    int m = (INSN(5, 5) << 4) | INSN(3, 0);
    int size = INSN(21, 20);
    int i;
    union simd_d_register res;

    switch(size) {
        case 0:
            for(i = 0; i < 4; i++) {
                res.u8[i + 0] = regs->e.simd[n].u8[2 * i] + regs->e.simd[n].u8[2 * i + 1];
                res.u8[i + 4] = regs->e.simd[m].u8[2 * i] + regs->e.simd[m].u8[2 * i + 1];
            }
            break;
        case 1:
            for(i = 0; i < 2; i++) {
                res.u16[i + 0] = regs->e.simd[n].u16[2 * i] + regs->e.simd[n].u16[2 * i + 1];
                res.u16[i + 2] = regs->e.simd[m].u16[2 * i] + regs->e.simd[m].u16[2 * i + 1];
            }
            break;
        case 2:
            for(i = 0; i < 1; i++) {
                res.u32[i + 0] = regs->e.simd[n].u32[2 * i] + regs->e.simd[n].u32[2 * i + 1];
                res.u32[i + 1] = regs->e.simd[m].u32[2 * i] + regs->e.simd[m].u32[2 * i + 1];
            }
            break;
        default:
            assert(0);
    }

    regs->e.simd[d] = res;
}

static void dis_common_vqdmulh_vqrdmulh_simd(uint64_t _regs, uint32_t insn, int is_round)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int d = (INSN(22, 22) << 4) | INSN(15, 12);
    int n = (INSN(7, 7) << 4) | INSN(19, 16);
    int m = (INSN(5, 5) << 4) | INSN(3, 0);
    int size = INSN(21, 20);
    int reg_nb = INSN(6, 6) + 1;
    int i;
    int r;
    union simd_d_register res[2];

    switch(size) {
        case 1:
            for(r = 0; r < reg_nb; r++)
                for(i = 0; i < 4; i++) {
                    int32_t product = 2 * regs->e.simd[n + r].s16[i] * regs->e.simd[m + r].s16[i] + (is_round?1 << 15:0);
                    res[r].s16[i] = ssat16(regs, product >> 16);
                }
            break;
        case 2:
            for(r = 0; r < reg_nb; r++)
                for(i = 0; i < 2; i++) {
                    int64_t product = 2 * (int64_t)regs->e.simd[n + r].s32[i] * (int64_t)regs->e.simd[m + r].s32[i] + (is_round?1UL << 31:0);
                    res[r].s32[i] = ssat32(regs, product >> 32);
                }
            break;
        default:
            assert(0);
    }

    for(r = 0; r < reg_nb; r++)
        regs->e.simd[d + r] = res[r];
}

static void dis_common_vmla_vmls_scalar_simd(uint64_t _regs, uint32_t insn, int is_thumb)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int d = (INSN(22, 22) << 4) | INSN(15, 12);
    int n = (INSN(7, 7) << 4) | INSN(19, 16);
    int size = INSN(21, 20);
    int reg_nb = (is_thumb?INSN(28,28):INSN(24,24)) + 1;
    int is_sub = INSN(10, 10);
    int is_f = INSN(8, 8);
    int m;
    int index;
    int i;
    int r;
    union simd_d_register res[2];

    switch(size) {
        case 1:
            m = INSN(2, 0);
            index = (INSN(5, 5) << 1) | INSN(3, 3);
            for(r = 0; r < reg_nb; r++)
                for(i = 0; i < 4; i++) {
                    uint32_t product = regs->e.simd[n + r].u16[i] * regs->e.simd[m].u16[index];
                    res[r].u16[i] = regs->e.simd[d + r].u16[i] + (is_sub?-product:product);
                }
            break;
        case 2:
            m = INSN(3, 0);
            index = INSN(5, 5);
            for(r = 0; r < reg_nb; r++)
                for(i = 0; i < 2; i++) {
                    if (is_f) {
#if 1
                        /* code below not always correct */
                        assert(0);
#else
                        float product = regs->e.simd[n + r].sf[i] * regs->e.simd[m].sf[index];
                        res[r].sf[i] = regs->e.simd[d + r].sf[i] + (is_sub?-product:product);
#endif
                    } else {
                        uint64_t product = regs->e.simd[n + r].u32[i] * regs->e.simd[m].u32[index];
                        res[r].u32[i] = regs->e.simd[d + r].u32[i] + (is_sub?-product:product);
                    }
                }
            break;
        default:
            assert(0);
    }

    for(r = 0; r < reg_nb; r++)
        regs->e.simd[d + r] = res[r];
}

static void dis_common_vmul_f32_simd(uint64_t _regs, uint32_t insn)
{
    /* FIXME: result is not correct */
#if 0
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int d = (INSN(22, 22) << 4) | INSN(15, 12);
    int n = (INSN(7, 7) << 4) | INSN(19, 16);
    int m = (INSN(5, 5) << 4) | INSN(3, 0);
    int reg_nb = INSN(6, 6) + 1;
    int i;
    int r;
    union simd_d_register res[2];

    for(r = 0; r < reg_nb; r++) {
        for(i = 0; i < 2; i++) {
            res[r].sf[i] = regs->e.simd[n + r].sf[i] * regs->e.simd[m + r].sf[i];
        }
    }

    for(r = 0; r < reg_nb; r++)
        regs->e.simd[d + r] = res[r];
#else
    assert(0);
#endif
}

static void dis_common_vmla_vmls_f32_simd(uint64_t _regs, uint32_t insn)
{
    /* FIXME: result is not correct */
#if 0
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int d = (INSN(22, 22) << 4) | INSN(15, 12);
    int n = (INSN(7, 7) << 4) | INSN(19, 16);
    int m = (INSN(5, 5) << 4) | INSN(3, 0);
    int reg_nb = INSN(6, 6) + 1;
    int is_sub = INSN(21, 21);
    int i;
    int r;
    union simd_d_register res[2];

    for(r = 0; r < reg_nb; r++) {
        for(i = 0; i < 2; i++) {
            if (is_sub)
                res[r].sf[i] = regs->e.simd[d + r].sf[i] - regs->e.simd[n + r].sf[i] * regs->e.simd[m + r].sf[i];
            else
                res[r].sf[i] = regs->e.simd[d + r].sf[i] + regs->e.simd[n + r].sf[i] * regs->e.simd[m + r].sf[i];
        }
    }

    for(r = 0; r < reg_nb; r++)
        regs->e.simd[d + r] = res[r];
#else
    assert(0);
#endif
}

static void dis_common_vpadd_fpu_simd(uint64_t _regs, uint32_t insn)
{
    /* FIXME: fpu operation not always correct */
#if 1
    assert(0);
#else
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int d = (INSN(22, 22) << 4) | INSN(15, 12);
    int n = (INSN(7, 7) << 4) | INSN(19, 16);
    int m = (INSN(5, 5) << 4) | INSN(3, 0);
    union simd_d_register res;

    res.sf[0] = regs->e.simd[n].sf[0] + regs->e.simd[n].sf[1];
    res.sf[1] = regs->e.simd[m].sf[0] + regs->e.simd[m].sf[1];

    regs->e.simd[d] = res;
#endif
}

static void dis_common_vadd_fpu_simd(uint64_t _regs, uint32_t insn)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int d = (INSN(22, 22) << 4) | INSN(15, 12);
    int n = (INSN(7, 7) << 4) | INSN(19, 16);
    int m = (INSN(5, 5) << 4) | INSN(3, 0);
    int reg_nb = INSN(6, 6) + 1;
    int i;
    int r;
    union simd_d_register res[2];

    for(r = 0; r < reg_nb; r++)
        for(i = 0; i < 2; i++)
            res[r].sf[i] = regs->e.simd[n + r].sf[i] + regs->e.simd[m + r].sf[i];

    for(r = 0; r < reg_nb; r++)
        regs->e.simd[d + r] = res[r];
}

static void dis_common_vacge_vacgt_simd(uint64_t _regs, uint32_t insn)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int d = (INSN(22, 22) << 4) | INSN(15, 12);
    int n = (INSN(7, 7) << 4) | INSN(19, 16);
    int m = (INSN(5, 5) << 4) | INSN(3, 0);
    int reg_nb = INSN(6, 6) + 1;
    int or_equal = (INSN(21, 21) == 0);
    int i;
    int r;
    union simd_d_register op1[2];
    union simd_d_register op2[2];
    union simd_d_register res[2];

    for(r = 0; r < reg_nb; r++) {
        for(i = 0; i < 2; i++) {
            op1[r].s32[i] = regs->e.simd[n + r].s32[i]&0x80000000?regs->e.simd[n + r].s32[i]^0x80000000:regs->e.simd[n + r].s32[i];
            op2[r].s32[i] = regs->e.simd[m + r].s32[i]&0x80000000?regs->e.simd[m + r].s32[i]^0x80000000:regs->e.simd[m + r].s32[i];
        }
    }
    for(r = 0; r < reg_nb; r++) {
        for(i = 0; i < 2; i++) {
            if (or_equal)
                res[r].u32[i] = (op1[r].sf[i]>=op2[r].sf[i])?~0:0;
            else
                res[r].u32[i] = (op1[r].sf[i]>op2[r].sf[i])?~0:0;
        }
    }

    for(r = 0; r < reg_nb; r++)
        regs->e.simd[d + r] = res[r];
}

static void dis_common_vaddl_vaddw_simd(uint64_t _regs, uint32_t insn, uint32_t is_thumb)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int d = (INSN(22, 22) << 4) | INSN(15, 12);
    int n = (INSN(7, 7) << 4) | INSN(19, 16);
    int m = (INSN(5, 5) << 4) | INSN(3, 0);
    int size = INSN(21, 20);
    int u = is_thumb?INSN(28, 28):INSN(24, 24);
    int is_vaddw = INSN(8, 8);
    int i;
    int r;
    union simd_d_register res[2];

    switch(size) {
        case 0:
            for(r = 0; r < 2; r++) {
                for(i = 0; i < 4; i++) {
                    if (u)
                        res[r].u16[i] = (is_vaddw?regs->e.simd[n + r].u16[i]:regs->e.simd[n].u8[i + 4 * r]) + regs->e.simd[m].u8[i + 4 * r];
                    else
                        res[r].s16[i] = (is_vaddw?regs->e.simd[n + r].s16[i]:regs->e.simd[n].s8[i + 4 * r]) + regs->e.simd[m].s8[i + 4 * r];
                }
            }
            break;
        case 1:
            for(r = 0; r < 2; r++) {
                for(i = 0; i < 2; i++) {
                    if (u)
                        res[r].u32[i] = (is_vaddw?regs->e.simd[n + r].u32[i]:regs->e.simd[n].u16[i + 2 * r]) + regs->e.simd[m].u16[i + 2 * r];
                    else
                        res[r].s32[i] = (is_vaddw?regs->e.simd[n + r].s32[i]:regs->e.simd[n].s16[i + 2 * r]) + regs->e.simd[m].s16[i + 2 * r];
                }
            }
            break;
        case 2:
            for(r = 0; r < 2; r++) {
                for(i = 0; i < 1; i++) {
                    if (u)
                        res[r].u64[i] = (uint64_t)(is_vaddw?regs->e.simd[n + r].u64[i]:regs->e.simd[n].u32[i + r]) + (uint64_t)regs->e.simd[m].u32[i + r];
                    else
                        res[r].s64[i] = (int64_t)(is_vaddw?regs->e.simd[n + r].s64[i]:regs->e.simd[n].s32[i + r]) + (int64_t)regs->e.simd[m].s32[i + r];
                }
            }
            break;
        default:
            assert(0);
    }

    for(r = 0; r < 2; r++)
        regs->e.simd[d + r] = res[r];
}

static void dis_common_vaddhn_simd(uint64_t _regs, uint32_t insn)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int d = (INSN(22, 22) << 4) | INSN(15, 12);
    int n = (INSN(7, 7) << 4) | INSN(19, 16);
    int m = (INSN(5, 5) << 4) | INSN(3, 0);
    int size = INSN(21, 20);
    int i;
    union simd_d_register res;

    switch(size) {
        case 0:
            for(i = 0; i < 8; i++)
                res.u8[i] = (regs->e.simq[n>>1].u16[i] + regs->e.simq[m>>1].u16[i]) >> 8;
            break;
        case 1:
            for(i = 0; i < 4; i++)
                res.u16[i] = (regs->e.simq[n>>1].u32[i] + regs->e.simq[m>>1].u32[i]) >> 16;
            break;
        case 2:
            for(i = 0; i < 2; i++)
                res.u32[i] = (regs->e.simq[n>>1].u64[i] + regs->e.simq[m>>1].u64[i]) >> 32;
            break;
        default:
            assert(0);
    }

    regs->e.simd[d] = res;
}

static void dis_common_vabal_vabdl_simd(uint64_t _regs, uint32_t insn, uint32_t is_thumb)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int d = (INSN(22, 22) << 4) | INSN(15, 12);
    int n = (INSN(7, 7) << 4) | INSN(19, 16);
    int m = (INSN(5, 5) << 4) | INSN(3, 0);
    int size = INSN(21, 20);
    int U = is_thumb?INSN(28, 28):INSN(24, 24);
    int is_acc = INSN(9, 9)?0:1;
    int i;
    union simd_q_register res;

    switch(size) {
        case 0:
            for(i = 0; i < 8; i++) {
                uint32_t absdiff;
                if (U)
                    absdiff = abs(regs->e.simd[n].u8[i] - regs->e.simd[m].u8[i]);
                else
                    absdiff = abs(regs->e.simd[n].s8[i] - regs->e.simd[m].s8[i]);
                res.u16[i] = (is_acc?regs->e.simq[d >> 1].u16[i]:0) + absdiff;
            }
            break;
        case 1:
            for(i = 0; i < 4; i++) {
                uint32_t absdiff;
                if (U)
                    absdiff = abs(regs->e.simd[n].u16[i] - regs->e.simd[m].u16[i]);
                else
                    absdiff = abs(regs->e.simd[n].s16[i] - regs->e.simd[m].s16[i]);
                res.u32[i] = (is_acc?regs->e.simq[d >> 1].u32[i]:0) + absdiff;
            }
            break;
        case 2:
            for(i = 0; i < 2; i++) {
                uint64_t absdiff;
                if (U)
                    absdiff = labs((uint64_t) regs->e.simd[n].u32[i] - (uint64_t) regs->e.simd[m].u32[i]);
                else
                    absdiff = labs((int64_t) regs->e.simd[n].s32[i] - (int64_t) regs->e.simd[m].s32[i]);
                res.u64[i] = (is_acc?regs->e.simq[d >> 1].u64[i]:0) + absdiff;
            }
            break;
        default:
            assert(0);
    }
    regs->e.simq[d >> 1] = res;
}

static void dis_common_vqdmlal_vqdmlsl_simd(uint64_t _regs, uint32_t insn)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int d = (INSN(22, 22) << 4) | INSN(15, 12);
    int n = (INSN(7, 7) << 4) | INSN(19, 16);
    int m = (INSN(5, 5) << 4) | INSN(3, 0);
    int size = INSN(21, 20);
    int is_sub = INSN(9, 9);
    int i;
    union simd_q_register res = regs->e.simq[d >> 1];

    switch(size) {
        case 1:
            for(i = 0; i < 4; i++) {
                int32_t product = ssat32(regs, 2 * regs->e.simd[n].s16[i] * regs->e.simd[m].s16[i]);

                res.s32[i] = ssat32(regs, (int64_t)res.s32[i] + (is_sub?-(int64_t)product:(int64_t)product));
            }
            break;
        case 2:
            for(i = 0; i < 2; i++) {
                int64_t product = ssat64(regs, 2 * (int64_t)regs->e.simd[n].s32[i] * (int64_t)regs->e.simd[m].s32[i]);

                res.s64[i] = ssat64(regs, (__int128_t)res.s64[i] + (is_sub?-(__int128_t)product:(__int128_t)product));
            }
            break;
        default:
            assert(0);
    }

    regs->e.simq[d >> 1] = res;
}

static void dis_common_vmlal_vmlsl_simd(uint64_t _regs, uint32_t insn, uint32_t is_thumb)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int d = (INSN(22, 22) << 4) | INSN(15, 12);
    int n = (INSN(7, 7) << 4) | INSN(19, 16);
    int m = (INSN(5, 5) << 4) | INSN(3, 0);
    int size = INSN(21, 20);
    int U = is_thumb?INSN(28, 28):INSN(24, 24);
    int is_sub = INSN(9, 9);
    int i;
    union simd_q_register res = regs->e.simq[d >> 1];

    switch(size) {
        case 0:
            for(i = 0; i < 8; i++) {
                if (U) {
                    uint32_t product = regs->e.simd[n].u8[i] * regs->e.simd[m].u8[i];
                    res.u16[i] = res.u16[i] + (is_sub?-product:product);
                } else {
                    int32_t product = regs->e.simd[n].s8[i] * regs->e.simd[m].s8[i];
                    res.s16[i] = res.s16[i] + (is_sub?-product:product);
                }
            }
            break;
        case 1:
            for(i = 0; i < 4; i++) {
                if (U) {
                    uint32_t product = regs->e.simd[n].u16[i] * regs->e.simd[m].u16[i];
                    res.u32[i] = res.u32[i] + (is_sub?-product:product);
                } else {
                    int32_t product = regs->e.simd[n].s16[i] * regs->e.simd[m].s16[i];
                    res.s32[i] = res.s32[i] + (is_sub?-product:product);
                }
            }
            break;
        case 2:
            for(i = 0; i < 2; i++) {
                if (U) {
                    uint64_t product = (uint64_t)regs->e.simd[n].u32[i] * (uint64_t)regs->e.simd[m].u32[i];
                    res.u64[i] = res.u64[i] + (is_sub?-product:product);
                } else {
                    int64_t product = (int64_t)regs->e.simd[n].s32[i] * (int64_t)regs->e.simd[m].s32[i];
                    res.s64[i] = res.s64[i] + (is_sub?-product:product);
                }
            }
            break;
        default:
            assert(0);
    }

    regs->e.simq[d >> 1] = res;
}

static void dis_common_vmull_integer_simd(uint64_t _regs, uint32_t insn, uint32_t is_thumb)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int d = (INSN(22, 22) << 4) | INSN(15, 12);
    int n = (INSN(7, 7) << 4) | INSN(19, 16);
    int m = (INSN(5, 5) << 4) | INSN(3, 0);
    int size = INSN(21, 20);
    int U = is_thumb?INSN(28, 28):INSN(24, 24);
    int i;
    union simd_q_register res;

    switch(size) {
        case 0:
            for(i = 0; i < 8; i++) {
                if (U)
                    res.u16[i] = regs->e.simd[n].u8[i] * regs->e.simd[m].u8[i];
                else
                    res.s16[i] = regs->e.simd[n].s8[i] * regs->e.simd[m].s8[i];
            }
            break;
        case 1:
            for(i = 0; i < 4; i++) {
                if (U)
                    res.u32[i] = regs->e.simd[n].u16[i] * regs->e.simd[m].u16[i];
                else
                    res.s32[i] = regs->e.simd[n].s16[i] * regs->e.simd[m].s16[i];
            }
            break;
        case 2:
            for(i = 0; i < 2; i++) {
                if (U)
                    res.u64[i] = (uint64_t)regs->e.simd[n].u32[i] * (uint64_t)regs->e.simd[m].u32[i];
                else
                    res.s64[i] = (int64_t)regs->e.simd[n].s32[i] * (int64_t)regs->e.simd[m].s32[i];
            }
            break;
        default:
            assert(0);
    }

    regs->e.simq[d >> 1] = res;
}

static void dis_common_vqdmull_simd(uint64_t _regs, uint32_t insn)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int d = (INSN(22, 22) << 4) | INSN(15, 12);
    int n = (INSN(7, 7) << 4) | INSN(19, 16);
    int m = (INSN(5, 5) << 4) | INSN(3, 0);
    int size = INSN(21, 20);
    int i;
    union simd_q_register res;

    switch(size) {
        case 1:
            for(i = 0; i < 4; i++) {
                int32_t product = 2 * regs->e.simd[n].s16[i] * regs->e.simd[m].s16[i];
                res.s32[i] = ssat32(regs, product);
            }
            break;
        case 2:
            for(i = 0; i < 2; i++) {
                int64_t product = 2 * (int64_t)regs->e.simd[n].s32[i] * (int64_t)regs->e.simd[m].s32[i];
                res.s64[i] = ssat64(regs, product);
            }
            break;
        default:
            assert(0);
    }

    regs->e.simq[d >> 1] = res;
}

static void dis_common_vmull_polynomial_simd(uint64_t _regs, uint32_t insn, uint32_t is_thumb)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int d = (INSN(22, 22) << 4) | INSN(15, 12);
    int n = (INSN(7, 7) << 4) | INSN(19, 16);
    int m = (INSN(5, 5) << 4) | INSN(3, 0);
    int size = INSN(21, 20);
    int i;
    union simd_q_register res;

    switch(size) {
        case 0:
            for(i = 0; i < 8; i++)
                res.u16[i] = polynomial_16(regs->e.simd[n].u8[i], regs->e.simd[m].u8[i]);
            break;
        default:
            assert(0);
    }

    regs->e.simq[d >> 1] = res;
}

static void dis_common_vmlal_vmlsl_scalar_simd(uint64_t _regs, uint32_t insn, uint32_t is_thumb)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int d = (INSN(22, 22) << 4) | INSN(15, 12);
    int n = (INSN(7, 7) << 4) | INSN(19, 16);
    int size = INSN(21, 20);
    int U = is_thumb?INSN(28, 28):INSN(24, 24);
    int is_sub = INSN(10, 10);
    int m;
    int index;
    int i;
    union simd_q_register res = regs->e.simq[d >> 1];

    switch(size) {
        case 1:
            m = INSN(2, 0);
            index = (INSN(5, 5) << 1) + INSN(3, 3);
            for(i = 0; i < 4; i++) {
                if (U) {
                    uint32_t product = regs->e.simd[n].u16[i] * regs->e.simd[m].u16[index];
                    res.u32[i] = res.u32[i] + (is_sub?-product:product);
                } else {
                    int32_t product = regs->e.simd[n].s16[i] * regs->e.simd[m].s16[index];
                    res.s32[i] = res.s32[i] + (is_sub?-product:product);
                }
            }
            break;
        case 2:
            m = INSN(3, 0);
            index = INSN(5, 5);
            for(i = 0; i < 2; i++) {
                if (U) {
                    uint64_t product = (uint64_t)regs->e.simd[n].u32[i] * (uint64_t)regs->e.simd[m].u32[index];
                    res.u64[i] = res.u64[i] + (is_sub?-product:product);
                } else {
                    int64_t product = (int64_t)regs->e.simd[n].s32[i] * (int64_t)regs->e.simd[m].s32[index];
                    res.s64[i] = res.s64[i] + (is_sub?-product:product);
                }
            }
            break;
        default:
            assert(0);
    }

    regs->e.simq[d >> 1] = res;
}

static void dis_common_vmul_scalar_simd(uint64_t _regs, uint32_t insn, uint32_t is_thumb)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int d = (INSN(22, 22) << 4) | INSN(15, 12);
    int n = (INSN(7, 7) << 4) | INSN(19, 16);
    int size = INSN(21, 20);
    int reg_nb = (is_thumb?INSN(28, 28):INSN(24, 24)) + 1;
    int f = INSN(8, 8);
    int m;
    int index;
    int i;
    int r;
    union simd_d_register res[2];

    /* FIXME: fpcu result not ok */
    assert(f == 0);
    switch(size) {
        case 1:
            m = INSN(2, 0);
            index = (INSN(5, 5) << 1) + INSN(3, 3);
            for(r = 0; r < reg_nb; r++)
                for(i = 0; i < 4; i++)
                    res[r].u16[i] = regs->e.simd[n + r].u16[i] * regs->e.simd[m].u16[index];
            break;
        case 2:
            m = INSN(3, 0);
            index = INSN(5, 5);
            for(r = 0; r < reg_nb; r++)
                for(i = 0; i < 2; i++)
                    if (f)
                        res[r].sf[i] = regs->e.simd[n + r].sf[i] * regs->e.simd[m].sf[index];
                    else
                        res[r].u32[i] = regs->e.simd[n + r].u32[i] * regs->e.simd[m].u32[index];
            break;
        default:
            assert(0);
    }

    for(r = 0; r < reg_nb; r++)
        regs->e.simd[d + r] = res[r];
}

static void dis_common_vmull_scalar_simd(uint64_t _regs, uint32_t insn, uint32_t is_thumb)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int d = (INSN(22, 22) << 4) | INSN(15, 12);
    int n = (INSN(7, 7) << 4) | INSN(19, 16);
    int size = INSN(21, 20);
    int U = is_thumb?INSN(28, 28):INSN(24, 24);
    int m;
    int index;
    int i;
    union simd_q_register res = regs->e.simq[d >> 1];

    switch(size) {
        case 1:
            m = INSN(2, 0);
            index = (INSN(5, 5) << 1) + INSN(3, 3);
            for(i = 0; i < 4; i++) {
                if (U)
                    res.u32[i] = regs->e.simd[n].u16[i] * regs->e.simd[m].u16[index];
                else
                    res.s32[i] = regs->e.simd[n].s16[i] * regs->e.simd[m].s16[index];
            }
            break;
        case 2:
            m = INSN(3, 0);
            index = INSN(5, 5);
            for(i = 0; i < 2; i++) {
                if (U)
                    res.u64[i] = (uint64_t)regs->e.simd[n].u32[i] * (uint64_t)regs->e.simd[m].u32[index];
                else
                    res.s64[i] = (int64_t)regs->e.simd[n].s32[i] * (int64_t)regs->e.simd[m].s32[index];
            }
            break;
        default:
            assert(0);
    }

    regs->e.simq[d >> 1] = res;
}

static void dis_common_vqdmlal_vqdmlsl_scalar_simd(uint64_t _regs, uint32_t insn)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int d = (INSN(22, 22) << 4) | INSN(15, 12);
    int n = (INSN(7, 7) << 4) | INSN(19, 16);
    int size = INSN(21, 20);
    int is_sub = INSN(10, 10);
    int m;
    int index;
    int i;
    union simd_q_register res = regs->e.simq[d >> 1];

    switch(size) {
        case 1:
            m = INSN(2, 0);
            index = (INSN(5, 5) << 1) + INSN(3, 3);
            for(i = 0; i < 4; i++) {
                int32_t product = ssat32(regs, 2 * regs->e.simd[n].s16[i] * regs->e.simd[m].s16[index]);

                res.s32[i] = ssat32(regs, (int64_t)res.s32[i] + (is_sub?-(int64_t)product:(int64_t)product));
            }
            break;
        case 2:
            m = INSN(3, 0);
            index = INSN(5, 5);
            for(i = 0; i < 2; i++) {
                int64_t product = ssat64(regs, 2 * (int64_t)regs->e.simd[n].s32[i] * (int64_t)regs->e.simd[m].s32[index]);

                res.s64[i] = ssat64(regs, (__int128_t)res.s64[i] + (is_sub?-(__int128_t)product:(__int128_t)product));
            }
            break;
        default:
            assert(0);
    }

    regs->e.simq[d >> 1] = res;
}

static void dis_common_vqdmull_scalar_simd(uint64_t _regs, uint32_t insn, uint32_t is_thumb)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int d = (INSN(22, 22) << 4) | INSN(15, 12);
    int n = (INSN(7, 7) << 4) | INSN(19, 16);
    int size = INSN(21, 20);
    int m;
    int index;
    int i;
    union simd_q_register res;

    switch(size) {
        case 1:
            m = INSN(2, 0);
            index = (INSN(5, 5) << 1) + INSN(3, 3);
            for(i = 0; i < 4; i++) {
                int32_t product = 2 * regs->e.simd[n].s16[i] * regs->e.simd[m].s16[index];
                res.s32[i] = ssat32(regs, product);
            }
            break;
        case 2:
            m = INSN(3, 0);
            index = INSN(5, 5);
            for(i = 0; i < 2; i++) {
                int64_t product = 2 * (int64_t)regs->e.simd[n].s32[i] * (int64_t)regs->e.simd[m].s32[index];
                res.s64[i] = ssat64(regs, product);
            }
            break;
        default:
            assert(0);
    }

    regs->e.simq[d >> 1] = res;
}

static void dis_common_vqdmulh_vqrdmulh_scalar_simd(uint64_t _regs, uint32_t insn, uint32_t is_thumb, int is_round)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int d = (INSN(22, 22) << 4) | INSN(15, 12);
    int n = (INSN(7, 7) << 4) | INSN(19, 16);
    int size = INSN(21, 20);
    int reg_nb = (is_thumb?INSN(28, 28):INSN(24, 24)) + 1;
    int m;
    int index;
    int i;
    int r;
    union simd_d_register res[2];

    switch(size) {
        case 1:
            m = INSN(2, 0);
            index = (INSN(5, 5) << 1) + INSN(3, 3);
            for(r = 0; r < reg_nb; r++)
                for(i = 0; i < 4; i++) {
                    int32_t product = 2 * regs->e.simd[n + r].s16[i] * regs->e.simd[m].s16[index] + (is_round?1<<15:0);
                    res[r].s16[i] = ssat16(regs, product >> 16);
                }
            break;
        case 2:
            m = INSN(3, 0);
            index = INSN(5, 5);
            for(r = 0; r < reg_nb; r++)
                for(i = 0; i < 2; i++) {
                    int64_t product = 2 * (int64_t)regs->e.simd[n + r].s32[i] * (int64_t)regs->e.simd[m].s32[index] + (is_round?1UL<<31:0);
                    res[r].s32[i] = ssat32(regs, product >> 32);
                }
            break;
        default:
            assert(0);
    }

    for(r = 0; r < reg_nb; r++)
        regs->e.simd[d + r] = res[r];
}

static void dis_common_vpaddl_simd(uint64_t _regs, uint32_t insn)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int d = (INSN(22, 22) << 4) | INSN(15, 12);
    int m = (INSN(5, 5) << 4) | INSN(3, 0);
    int size = INSN(19, 18);
    int reg_nb = INSN(6, 6) + 1;
    int is_unsigned = INSN(7, 7);
    int i;
    int r;
    union simd_d_register res[2];

    switch(size) {
        case 0:
            for(r = 0; r < reg_nb; r++)
                for(i = 0; i < 4; i++)
                    if (is_unsigned)
                        res[r].u16[i] = regs->e.simd[m + r].u8[2 * i] + regs->e.simd[m + r].u8[2 * i + 1];
                    else
                        res[r].s16[i] = regs->e.simd[m + r].s8[2 * i] + regs->e.simd[m + r].s8[2 * i + 1];
            break;
        case 1:
            for(r = 0; r < reg_nb; r++)
                for(i = 0; i < 2; i++)
                    if (is_unsigned)
                        res[r].u32[i] = regs->e.simd[m + r].u16[2 * i] + regs->e.simd[m + r].u16[2 * i + 1];
                    else
                        res[r].s32[i] = regs->e.simd[m + r].s16[2 * i] + regs->e.simd[m + r].s16[2 * i + 1];
            break;
        case 2:
            for(r = 0; r < reg_nb; r++)
                for(i = 0; i < 1; i++)
                    if (is_unsigned)
                        res[r].u64[i] = (uint64_t)regs->e.simd[m + r].u32[2 * i] + (uint64_t)regs->e.simd[m + r].u32[2 * i + 1];
                    else
                        res[r].s64[i] = (int64_t)regs->e.simd[m + r].s32[2 * i] + (int64_t)regs->e.simd[m + r].s32[2 * i + 1];
            break;
        default:
            fatal("size = %d\n", size);
    }

    for(r = 0; r < reg_nb; r++)
        regs->e.simd[d + r] = res[r];
}

static void dis_common_vcls_simd(uint64_t _regs, uint32_t insn)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int d = (INSN(22, 22) << 4) | INSN(15, 12);
    int m = (INSN(5, 5) << 4) | INSN(3, 0);
    int size = INSN(19, 18);
    int reg_nb = INSN(6, 6) + 1;
    int i;
    int r;
    union simd_d_register res[2];

    switch(size) {
        case 0:
            for(r = 0; r < reg_nb; r++)
                for(i = 0; i < 8; i++)
                    res[r].u8[i] = cls(regs->e.simd[m + r].u8[i], 7);
            break;
        case 1:
            for(r = 0; r < reg_nb; r++)
                for(i = 0; i < 4; i++)
                    res[r].u16[i] = cls(regs->e.simd[m + r].u16[i], 15);
            break;
        case 2:
            for(r = 0; r < reg_nb; r++)
                for(i = 0; i < 2; i++)
                    res[r].u32[i] = cls(regs->e.simd[m + r].u32[i], 31);
            break;
        default:
            fatal("size = %d\n", size);
    }

    for(r = 0; r < reg_nb; r++)
        regs->e.simd[d + r] = res[r];
}

static void dis_common_vclz_simd(uint64_t _regs, uint32_t insn)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int d = (INSN(22, 22) << 4) | INSN(15, 12);
    int m = (INSN(5, 5) << 4) | INSN(3, 0);
    int size = INSN(19, 18);
    int reg_nb = INSN(6, 6) + 1;
    int i;
    int r;
    union simd_d_register res[2];

    switch(size) {
        case 0:
            for(r = 0; r < reg_nb; r++)
                for(i = 0; i < 8; i++)
                    res[r].u8[i] = clz(regs->e.simd[m + r].u8[i], 7);
            break;
        case 1:
            for(r = 0; r < reg_nb; r++)
                for(i = 0; i < 4; i++)
                    res[r].u16[i] = clz(regs->e.simd[m + r].u16[i], 15);
            break;
        case 2:
            for(r = 0; r < reg_nb; r++)
                for(i = 0; i < 2; i++)
                    res[r].u32[i] = clz(regs->e.simd[m + r].u32[i], 31);
            break;
        default:
            fatal("size = %d\n", size);
    }

    for(r = 0; r < reg_nb; r++)
        regs->e.simd[d + r] = res[r];
}

static void dis_common_vcnt_simd(uint64_t _regs, uint32_t insn)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int d = (INSN(22, 22) << 4) | INSN(15, 12);
    int m = (INSN(5, 5) << 4) | INSN(3, 0);
    int size = INSN(19, 18);
    int reg_nb = INSN(6, 6) + 1;
    int i;
    int r;
    union simd_d_register res[2];

    assert(size == 0);
    for(r = 0; r < reg_nb; r++)
        for(i = 0; i < 8; i++)
            res[r].u8[i] = cnt(regs->e.simd[m + r].u8[i], 8);

    for(r = 0; r < reg_nb; r++)
        regs->e.simd[d + r] = res[r];
}

static void dis_common_vpadal_simd(uint64_t _regs, uint32_t insn)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int d = (INSN(22, 22) << 4) | INSN(15, 12);
    int m = (INSN(5, 5) << 4) | INSN(3, 0);
    int size = INSN(19, 18);
    int reg_nb = INSN(6, 6) + 1;
    int is_unsigned = INSN(7, 7);
    int i;
    int r;
    union simd_d_register res[2];

    for(r = 0; r < reg_nb; r++)
        res[r] = regs->e.simd[d + r];
    switch(size) {
        case 0:
            for(r = 0; r < reg_nb; r++)
                for(i = 0; i < 4; i++)
                    if (is_unsigned)
                        res[r].u16[i] += regs->e.simd[m + r].u8[i * 2] + regs->e.simd[m + r].u8[i * 2 + 1];
                    else
                        res[r].s16[i] += regs->e.simd[m + r].s8[i * 2] + regs->e.simd[m + r].s8[i * 2 + 1];
            break;
        case 1:
            for(r = 0; r < reg_nb; r++)
                for(i = 0; i < 2; i++)
                    if (is_unsigned)
                        res[r].u32[i] += regs->e.simd[m + r].u16[i * 2] + regs->e.simd[m + r].u16[i * 2 + 1];
                    else
                        res[r].s32[i] += regs->e.simd[m + r].s16[i * 2] + regs->e.simd[m + r].s16[i * 2 + 1];
            break;
        case 2:
            for(r = 0; r < reg_nb; r++)
                for(i = 0; i < 1; i++)
                    if (is_unsigned)
                        res[r].u64[i] += (uint64_t)regs->e.simd[m + r].u32[i * 2] + (uint64_t)regs->e.simd[m + r].u32[i * 2 + 1];
                    else
                        res[r].s64[i] += (int64_t)regs->e.simd[m + r].s32[i * 2] + (int64_t)regs->e.simd[m + r].s32[i * 2 + 1];
            break;
        default:
            fatal("size = %d\n", size);
    }

    for(r = 0; r < reg_nb; r++)
        regs->e.simd[d + r] = res[r];
}

static void dis_common_vqabs_simd(uint64_t _regs, uint32_t insn)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int d = (INSN(22, 22) << 4) | INSN(15, 12);
    int m = (INSN(5, 5) << 4) | INSN(3, 0);
    int size = INSN(19, 18);
    int reg_nb = INSN(6, 6) + 1;
    int i;
    int r;
    union simd_d_register res[2];

    switch(size) {
        case 0:
            for(r = 0; r < reg_nb; r++)
                for(i = 0; i < 8; i++)
                    res[r].s8[i] = ssat8(regs, abs(regs->e.simd[m + r].s8[i]));
            break;
        case 1:
            for(r = 0; r < reg_nb; r++)
                for(i = 0; i < 4; i++)
                    res[r].s16[i] = ssat16(regs, abs(regs->e.simd[m + r].s16[i]));
            break;
        case 2:
            for(r = 0; r < reg_nb; r++)
                for(i = 0; i < 2; i++)
                    res[r].s32[i] = ssat32(regs, labs(regs->e.simd[m + r].s32[i]));
            break;
        default:
            fatal("size = %d\n", size);
    }

    for(r = 0; r < reg_nb; r++)
        regs->e.simd[d + r] = res[r];
}

static void dis_common_vqneg_simd(uint64_t _regs, uint32_t insn)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int d = (INSN(22, 22) << 4) | INSN(15, 12);
    int m = (INSN(5, 5) << 4) | INSN(3, 0);
    int size = INSN(19, 18);
    int reg_nb = INSN(6, 6) + 1;
    int i;
    int r;
    union simd_d_register res[2];

    switch(size) {
        case 0:
            for(r = 0; r < reg_nb; r++)
                for(i = 0; i < 8; i++)
                    res[r].s8[i] = ssat8(regs, -regs->e.simd[m + r].s8[i]);
            break;
        case 1:
            for(r = 0; r < reg_nb; r++)
                for(i = 0; i < 4; i++)
                    res[r].s16[i] = ssat16(regs, -regs->e.simd[m + r].s16[i]);
            break;
        case 2:
            for(r = 0; r < reg_nb; r++)
                for(i = 0; i < 2; i++)
                    res[r].s32[i] = ssat32(regs, -(int64_t)regs->e.simd[m + r].s32[i]);
            break;
        default:
            fatal("size = %d\n", size);
    }

    for(r = 0; r < reg_nb; r++)
        regs->e.simd[d + r] = res[r];
}

static void dis_common_vabs_simd(uint64_t _regs, uint32_t insn)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int d = (INSN(22, 22) << 4) | INSN(15, 12);
    int m = (INSN(5, 5) << 4) | INSN(3, 0);
    int size = INSN(19, 18);
    int reg_nb = INSN(6, 6) + 1;
    int f = INSN(10, 10);
    int i;
    int r;
    union simd_d_register res[2];

    switch(size) {
        case 0:
            for(r = 0; r < reg_nb; r++)
                for(i = 0; i < 8; i++)
                    res[r].s8[i] = abs(regs->e.simd[m + r].s8[i]);
            break;
        case 1:
            for(r = 0; r < reg_nb; r++)
                for(i = 0; i < 4; i++)
                    res[r].s16[i] = abs(regs->e.simd[m + r].s16[i]);
            break;
        case 2:
            for(r = 0; r < reg_nb; r++)
                for(i = 0; i < 2; i++)
                    if (f)
                        res[r].s32[i] = regs->e.simd[m + r].s32[i]&0x80000000?regs->e.simd[m + r].s32[i]^0x80000000:regs->e.simd[m + r].s32[i];
                    else
                        res[r].s32[i] = abs(regs->e.simd[m + r].s32[i]);
            break;
        default:
            fatal("size = %d\n", size);
    }
    for(r = 0; r < reg_nb; r++)
        regs->e.simd[d + r] = res[r];
}

static void dis_common_vneg_simd(uint64_t _regs, uint32_t insn)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int d = (INSN(22, 22) << 4) | INSN(15, 12);
    int m = (INSN(5, 5) << 4) | INSN(3, 0);
    int size = INSN(19, 18);
    int reg_nb = INSN(6, 6) + 1;
    int f = INSN(10, 10);
    int i;
    int r;
    union simd_d_register res[2];

    switch(size) {
        case 0:
            for(r = 0; r < reg_nb; r++)
                for(i = 0; i < 8; i++)
                    res[r].s8[i] = -regs->e.simd[m + r].s8[i];
            break;
        case 1:
            for(r = 0; r < reg_nb; r++)
                for(i = 0; i < 4; i++)
                    res[r].s16[i] = -regs->e.simd[m + r].s16[i];
            break;
        case 2:
            for(r = 0; r < reg_nb; r++)
                for(i = 0; i < 2; i++)
                    if (f)
                        res[r].s32[i] = regs->e.simd[m + r].s32[i]^0x80000000;
                    else
                        res[r].s32[i] = -regs->e.simd[m + r].s32[i];
            break;
        default:
            fatal("size = %d\n", size);
    }
    for(r = 0; r < reg_nb; r++)
        regs->e.simd[d + r] = res[r];
}

static void dis_common_vmovn_simd(uint64_t _regs, uint32_t insn)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int d = (INSN(22, 22) << 4) | INSN(15, 12);
    int m = (INSN(5, 5) << 4) | INSN(3, 0);
    int size = INSN(19, 18);
    int i;
    union simd_d_register res;

    switch(size) {
        case 0:
            for(i = 0; i < 8; i++)
                res.u8[i] = regs->e.simq[(m >> 1)].u16[i];
            break;
        case 1:
            for(i = 0; i < 4; i++)
                res.u16[i] = regs->e.simq[(m >> 1)].u32[i];
            break;
        case 2:
            for(i = 0; i < 2; i++)
                res.u32[i] = regs->e.simq[(m >> 1)].u64[i];
            break;
        default:
            fatal("size = %d\n", size);
    }

    regs->e.simd[d] = res;
}

static void dis_common_vqmovun_simd(uint64_t _regs, uint32_t insn)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int d = (INSN(22, 22) << 4) | INSN(15, 12);
    int m = (INSN(5, 5) << 4) | INSN(3, 0);
    int size = INSN(19, 18);
    int i;
    union simd_d_register res;

    switch(size) {
        case 0:
            for(i = 0; i < 8; i++)
                res.u8[i] = usat8(regs, regs->e.simq[m >> 1].s16[i]);
            break;
        case 1:
            for(i = 0; i < 4; i++)
                res.u16[i] = usat16(regs, regs->e.simq[m >> 1].s32[i]);
            break;
        case 2:
            for(i = 0; i < 2; i++)
                res.u32[i] = usat32(regs, regs->e.simq[m >> 1].s64[i]);
            break;
        default:
            fatal("size = %d\n", size);
    }

    regs->e.simd[d] = res;
}

static void dis_common_vqmovn_simd(uint64_t _regs, uint32_t insn)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int d = (INSN(22, 22) << 4) | INSN(15, 12);
    int m = (INSN(5, 5) << 4) | INSN(3, 0);
    int size = INSN(19, 18);
    int source_unsigned = (INSN(7, 6) == 3);
    int i;
    union simd_d_register res;

    switch(size) {
        case 0:
            for(i = 0; i < 8; i++) {
                if (source_unsigned)
                    res.s8[i] = usat8_u(regs, regs->e.simq[m >> 1].u16[i]);
                else
                    res.s8[i] = ssat8(regs, regs->e.simq[m >> 1].s16[i]);
            }
            break;
        case 1:
            for(i = 0; i < 4; i++) {
                if (source_unsigned)
                    res.s16[i] = usat16_u(regs, regs->e.simq[m >> 1].u32[i]);
                else
                    res.s16[i] = ssat16(regs, regs->e.simq[m >> 1].s32[i]);
            }
            break;
        case 2:
            for(i = 0; i < 2; i++) {
                if (source_unsigned)
                    res.s32[i] = usat32_u(regs, regs->e.simq[m >> 1].u64[i]);
                else
                    res.s32[i] = ssat32(regs, regs->e.simq[m >> 1].s64[i]);
            }
            break;
        default:
            fatal("size = %d\n", size);
    }

    regs->e.simd[d] = res;
}

static void dis_common_vcvt_floating_integer_simd(uint64_t _regs, uint32_t insn)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int d = (INSN(22, 22) << 4) | INSN(15, 12);
    int m = (INSN(5, 5) << 4) | INSN(3, 0);
    int size = INSN(19, 18);
    int reg_nb = INSN(6, 6) + 1;
    int to_integer = INSN(8, 8);
    int is_unsigned = INSN(7, 7);
    int i;
    int r;
    union simd_d_register res[2];

    assert(size == 2);
    for(r = 0; r < reg_nb; r++)
        for(i = 0; i < 2; i++) {
            if (to_integer) {
                if (is_unsigned)
                    res[r].u32[i] = (uint32_t) usat32_d(regs->e.simd[m + r].sf[i]);
                else
                    res[r].s32[i] = (int32_t) ssat32_d(regs->e.simd[m + r].sf[i]);
            } else {
                if (is_unsigned)
                    res[r].sf[i] = regs->e.simd[m + r].u32[i];
                else
                    res[r].sf[i] = regs->e.simd[m + r].s32[i];
            }
        }

    for(r = 0; r < reg_nb; r++)
        regs->e.simd[d + r] = res[r];
}

static void dis_common_vmla_vmls_vfp(uint64_t _regs, uint32_t insn)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int D = INSN(22, 22);
    int vn = INSN(19, 16);
    int vd = INSN(15, 12);
    int is_double = INSN(8, 8);
    int N = INSN(7, 7);
    int is_sub = INSN(6, 6);
    int M = INSN(5, 5);
    int vm = INSN(3, 0);
    int d, n, m;

    if (is_double) {
        d = (D << 4) + vd;
        n = (N << 4) + vn;
        m = (M << 4) + vm;
        if (is_sub)
            regs->e.df[d] -= regs->e.df[n] * regs->e.df[m];
        else
            regs->e.df[d] += regs->e.df[n] * regs->e.df[m];
    } else {
        d = (vd << 1) + D;
        n = (vn << 1) + N;
        m = (vm << 1) + M;
        if (is_sub)
            regs->e.sf[d] -= regs->e.sf[n] * regs->e.sf[m];
        else
            regs->e.sf[d] += regs->e.sf[n] * regs->e.sf[m];
    }
}

static void dis_common_vnmla_vnmls_vfp(uint64_t _regs, uint32_t insn)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int D = INSN(22, 22);
    int vn = INSN(19, 16);
    int vd = INSN(15, 12);
    int is_double = INSN(8, 8);
    int N = INSN(7, 7);
    int is_sub = INSN(6, 6);
    int M = INSN(5, 5);
    int vm = INSN(3, 0);
    int d, n, m;

    if (is_double) {
        d = (D << 4) + vd;
        n = (N << 4) + vn;
        m = (M << 4) + vm;
        if (is_sub)
            regs->e.df[d] = -regs->e.df[d] - regs->e.df[n] * regs->e.df[m];
        else
            regs->e.df[d] = -regs->e.df[d] + regs->e.df[n] * regs->e.df[m];
    } else {
        d = (vd << 1) + D;
        n = (vn << 1) + N;
        m = (vm << 1) + M;
        if (is_sub)
            regs->e.sf[d] = -regs->e.sf[d] - regs->e.sf[n] * regs->e.sf[m];
        else
            regs->e.sf[d] = -regs->e.sf[d] + regs->e.sf[n] * regs->e.sf[m];
    }
}

static void dis_common_vnmul_vfp(uint64_t _regs, uint32_t insn)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int D = INSN(22, 22);
    int vn = INSN(19, 16);
    int vd = INSN(15, 12);
    int is_double = INSN(8, 8);
    int N = INSN(7, 7);
    int M = INSN(5, 5);
    int vm = INSN(3, 0);
    int d, n, m;

    if (is_double) {
        d = (D << 4) + vd;
        n = (N << 4) + vn;
        m = (M << 4) + vm;
        regs->e.df[d] = -regs->e.df[n] * regs->e.df[m];
    } else {
        d = (vd << 1) + D;
        n = (vn << 1) + N;
        m = (vm << 1) + M;
        regs->e.sf[d] = -regs->e.sf[n] * regs->e.sf[m];
    }
}

static void dis_common_vmul_vfp(uint64_t _regs, uint32_t insn)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int D = INSN(22, 22);
    int vn = INSN(19, 16);
    int vd = INSN(15, 12);
    int is_double = INSN(8, 8);
    int N = INSN(7, 7);
    int M = INSN(5, 5);
    int vm = INSN(3, 0);
    int d, n, m;

    if (is_double) {
        d = (D << 4) + vd;
        n = (N << 4) + vn;
        m = (M << 4) + vm;
        regs->e.df[d] = regs->e.df[n] * regs->e.df[m];
    } else {
        d = (vd << 1) + D;
        n = (vn << 1) + N;
        m = (vm << 1) + M;
        regs->e.sf[d] = regs->e.sf[n] * regs->e.sf[m];
    }
}

static void dis_common_vadd_vsub_vfp(uint64_t _regs, uint32_t insn)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int D = INSN(22, 22);
    int vn = INSN(19, 16);
    int vd = INSN(15, 12);
    int is_double = INSN(8, 8);
    int N = INSN(7, 7);
    int is_sub = INSN(6, 6);
    int M = INSN(5, 5);
    int vm = INSN(3, 0);
    int d, n, m;

    if (is_double) {
        d = (D << 4) + vd;
        n = (N << 4) + vn;
        m = (M << 4) + vm;
        if (is_sub)
            regs->e.df[d] = regs->e.df[n] - regs->e.df[m];
        else
            regs->e.df[d] = regs->e.df[n] + regs->e.df[m];
    } else {
        d = (vd << 1) + D;
        n = (vn << 1) + N;
        m = (vm << 1) + M;
        if (is_sub)
            regs->e.sf[d] = regs->e.sf[n] - regs->e.sf[m];
        else
            regs->e.sf[d] = regs->e.sf[n] + regs->e.sf[m];
    }
}

static void dis_common_vdiv_vfp(uint64_t _regs, uint32_t insn)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int D = INSN(22, 22);
    int vn = INSN(19, 16);
    int vd = INSN(15, 12);
    int is_double = INSN(8, 8);
    int N = INSN(7, 7);
    int M = INSN(5, 5);
    int vm = INSN(3, 0);
    int d, n, m;

    if (is_double) {
        d = (D << 4) + vd;
        n = (N << 4) + vn;
        m = (M << 4) + vm;
        regs->e.df[d] = regs->e.df[n] / regs->e.df[m];
    } else {
        d = (vd << 1) + D;
        n = (vn << 1) + N;
        m = (vm << 1) + M;
        regs->e.sf[d] = regs->e.sf[n] / regs->e.sf[m];
    }
}

static void dis_common_vabs_vfp(uint64_t _regs, uint32_t insn)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int D = INSN(22, 22);
    int vd = INSN(15, 12);
    int is_double = INSN(8, 8);
    int M = INSN(5, 5);
    int vm = INSN(3, 0);
    int d, m;

    if (is_double) {
        d = (D << 4) + vd;
        m = (M << 4) + vm;
        regs->e.d[d] = regs->e.d[m]&0x8000000000000000UL?regs->e.d[m]^0x8000000000000000UL:regs->e.d[m];

    } else {
        d = (vd << 1) + D;
        m = (vm << 1) + M;
        regs->e.s[d] = regs->e.s[m]&0x80000000?regs->e.s[m]^0x80000000:regs->e.s[m];
    }
}

static void dis_common_vsqrt_vfp(uint64_t _regs, uint32_t insn)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int D = INSN(22, 22);
    int vd = INSN(15, 12);
    int is_double = INSN(8, 8);
    int M = INSN(5, 5);
    int vm = INSN(3, 0);
    int d, m;

    if (is_double) {
        d = (D << 4) + vd;
        m = (M << 4) + vm;
        if (regs->e.d[m]&0x8000000000000000UL)
            regs->e.df[d] = NAN;
        else
            regs->e.df[d] = sqrt(regs->e.df[m]);
    } else {
        d = (vd << 1) + D;
        m = (vm << 1) + M;
        if (regs->e.s[m]&0x80000000)
            regs->e.sf[d] = NAN;
        else
            regs->e.sf[d] = sqrtf(regs->e.sf[m]);
    }
}

static void dis_common_vneg_vfp(uint64_t _regs, uint32_t insn)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int D = INSN(22, 22);
    int vd = INSN(15, 12);
    int is_double = INSN(8, 8);
    int M = INSN(5, 5);
    int vm = INSN(3, 0);
    int d, m;

    if (is_double) {
        d = (D << 4) + vd;
        m = (M << 4) + vm;
        regs->e.df[d] = -regs->e.df[m];
    } else {
        d = (vd << 1) + D;
        m = (vm << 1) + M;
        regs->e.sf[d] = -regs->e.sf[m];
    }
}

static void dis_common_vdup_arm(uint64_t _regs, uint32_t insn)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int d = (INSN(7, 7) << 4) | INSN(19, 16);
    int rt = INSN(15, 12);
    int reg_nb = INSN(21, 21) + 1;
    int be = (INSN(22, 22) << 1) | INSN(5, 5);
    int i;
    int r;
    union simd_d_register res[2];

    assert(rt != 15);

    /* !!!!! doc is wrong is case (b, e) stuff but correct in <size>
     * desciption stuff.
     */
    switch(be) {
        case 0:
            for(r = 0; r < reg_nb; r++)
                for(i = 0; i < 2; i++)
                    res[r].u32[i] = regs->r[rt];
            break;
        case 1:
            for(r = 0; r < reg_nb; r++)
                for(i = 0; i < 4; i++)
                    res[r].u16[i] = regs->r[rt];
            break;
        case 2:
            for(r = 0; r < reg_nb; r++)
                for(i = 0; i < 8; i++)
                    res[r].u8[i] = regs->r[rt];
            break;
        default:
            assert(0);
    }

    for(r = 0; r < reg_nb; r++)
        regs->e.simd[d + r] = res[r];
}

static void dis_common_vmov_from_arm_simd(uint64_t _regs, uint32_t insn)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int d = (INSN(7, 7) << 4) | INSN(19, 16);
    int rt = INSN(15, 12);
    int opc1 = INSN(22, 21);
    int opc2 = INSN(6, 5);
    int index;

    if (opc1 & 2) {
        index = ((opc1 & 1) << 2) + opc2;
        regs->e.simd[d].u8[index] = regs->r[rt];
    } else if (opc2 & 1) {
        index = ((opc1 & 1) << 1) + (opc2 >> 1);
        regs->e.simd[d].u16[index] = regs->r[rt];
    } else {
        index = opc1 & 1;
        regs->e.simd[d].u32[index] = regs->r[rt];
    }
}

static void dis_common_vqrshrun_simd(uint64_t _regs, uint32_t insn)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int d = (INSN(22, 22) << 4) | INSN(15, 12);
    int m = (INSN(5, 5) << 4) | INSN(3, 0);
    int imm6 = INSN(21, 16);
    int i;
    union simd_d_register res;
    int shift_value;

    if (imm6 >> 5) {
        shift_value = 64 - imm6;
        for(i = 0; i < 2; i++)
            res.u32[i] = usat32(regs, ((__int128_t)regs->e.simq[m >> 1].s64[i] + (1 << (shift_value - 1))) >> shift_value);
    } else if (imm6 >> 4) {
        shift_value = 32 - imm6;
        for(i = 0; i < 4; i++)
            res.u16[i] = usat16(regs, ((int64_t)regs->e.simq[m >> 1].s32[i] + (1 << (shift_value - 1))) >> shift_value);
    } else if (imm6 >> 3) {
        shift_value = 16 - imm6;
        for(i = 0; i < 8; i++)
            res.u8[i] = usat8(regs, (regs->e.simq[m >> 1].s16[i] + (1 << (shift_value - 1))) >> shift_value);
    } else
        assert(0);


    regs->e.simd[d] = res;
}

static void dis_common_vqrshrn_simd(uint64_t _regs, uint32_t insn, int is_unsigned)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int d = (INSN(22, 22) << 4) | INSN(15, 12);
    int m = (INSN(5, 5) << 4) | INSN(3, 0);
    int imm6 = INSN(21, 16);
    int i;
    union simd_d_register res;
    int shift_value;

    if (imm6 >> 5) {
        shift_value = 64 - imm6;
        for(i = 0; i < 2; i++) {
            if (is_unsigned)
                res.u32[i] = usat32_u(regs, ((__uint128_t)regs->e.simq[m >> 1].u64[i] + (1 << (shift_value - 1))) >> shift_value);
            else
                res.s32[i] = ssat32(regs, ((__int128_t)regs->e.simq[m >> 1].s64[i] + (1 << (shift_value - 1))) >> shift_value);
        }
    } else if (imm6 >> 4) {
        shift_value = 32 - imm6;
        for(i = 0; i < 4; i++) {
            if (is_unsigned)
                res.u16[i] = usat16_u(regs, ((uint64_t)regs->e.simq[m >> 1].u32[i] + (1 << (shift_value - 1))) >> shift_value);
            else
                res.s16[i] = ssat16(regs, ((int64_t)regs->e.simq[m >> 1].s32[i] + (1 << (shift_value - 1))) >> shift_value);
        }
    } else if (imm6 >> 3) {
        shift_value = 16 - imm6;
        for(i = 0; i < 8; i++) {
            if (is_unsigned)
                res.u8[i] = usat8_u(regs, (regs->e.simq[m >> 1].u16[i] + (1 << (shift_value - 1))) >> shift_value);
            else
                res.s8[i] = ssat8(regs, (regs->e.simq[m >> 1].s16[i] + (1 << (shift_value - 1))) >> shift_value);
        }
    } else
        assert(0);


    regs->e.simd[d] = res;
}

static void dis_common_vmovl_simd(uint64_t _regs, uint32_t insn, int is_unsigned)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int d = (INSN(22, 22) << 4) | INSN(15, 12);
    int m = (INSN(5, 5) << 4) | INSN(3, 0);
    int size = INSN(21, 19);
    int i;
    union simd_q_register res;

    switch(size) {
        case 1:
            for(i = 0; i < 8; i++) {
                if (is_unsigned)
                    res.u16[i] = regs->e.simd[m].u8[i];
                else
                    res.s16[i] = regs->e.simd[m].s8[i];
            }
            break;
        case 2:
            for(i = 0; i < 4; i++) {
                if (is_unsigned)
                    res.u32[i] = regs->e.simd[m].u16[i];
                else
                    res.s32[i] = regs->e.simd[m].s16[i];
            }
            break;
        case 4:
            for(i = 0; i < 2; i++) {
                if (is_unsigned)
                    res.u64[i] = regs->e.simd[m].u32[i];
                else
                    res.s64[i] = regs->e.simd[m].s32[i];
            }
            break;
        default:
            assert(0);
    }

    regs->e.simq[d >> 1] = res;
}

static void dis_common_vext_simd(uint64_t _regs, uint32_t insn)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int d = (INSN(22, 22) << 4) | INSN(15, 12);
    int n = (INSN(7, 7) << 4) | INSN(19, 16);
    int m = (INSN(5, 5) << 4) | INSN(3, 0);
    int reg_nb = INSN(6, 6) + 1;
    int imm4 = INSN(11, 8);
    int pos = 0;
    int i;
    int r;
    union simd_d_register res[2];

    for(i = imm4; i < 8 * reg_nb; i++) {
        res[pos / 8].u8[pos % 8] = regs->e.simd[n + i / 8].u8[i % 8];
        pos++;
    }
    for(i = 0; i < imm4; i++) {
        res[pos / 8].u8[pos % 8] = regs->e.simd[m + i / 8].u8[i % 8];
        pos++;
    }

    for(r = 0; r < reg_nb; r++)
        regs->e.simd[d + r] = res[r];
}

static void dis_common_vdup_scalar(uint64_t _regs, uint32_t insn)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int d = (INSN(22, 22) << 4) | INSN(15, 12);
    int m = (INSN(5, 5) << 4) | INSN(3, 0);
    int reg_nb = INSN(6, 6) + 1;
    int imm4 = INSN(19, 16);
    int size;
    int i;
    int r;
    union simd_d_register res[2];

    if (imm4 & 1)
        size = 0;
    else if (imm4 & 2)
        size = 1;
    else if (imm4 & 4)
        size = 2;
    else
        assert(0);

    switch(size) {
        case 0:
            for(r = 0; r < reg_nb; r++)
                for(i = 0; i < 8; i++)
                    res[r].u8[i] = regs->e.simd[m].u8[imm4 >> 1];
            break;
        case 1:
            for(r = 0; r < reg_nb; r++)
                for(i = 0; i < 4; i++)
                    res[r].u16[i] = regs->e.simd[m].u16[imm4 >> 2];
            break;
        case 2:
            for(r = 0; r < reg_nb; r++)
                for(i = 0; i < 2; i++)
                    res[r].u32[i] = regs->e.simd[m].u32[imm4 >> 3];
            break;
        default:
            assert(0);
    }

    for(r = 0; r < reg_nb; r++)
        regs->e.simd[d + r] = res[r];
}


void arm_hlp_dirty_saturating(uint64_t regs, uint32_t insn)
{
    int op = INSN(22, 21);

    switch(op) {
        case 0:
            qadd_a1(regs, insn);
            break;
        case 1:
            qsub_a1(regs, insn);
            break;
        case 2:
            qdadd_a1(regs, insn);
            break;
        case 3:
            qdsub_a1(regs, insn);
            break;
        default:
            fatal("op = %d(0x%x)\n", op, op);
    }
}

void arm_hlp_halfword_mult_and_mult_acc(uint64_t regs, uint32_t insn)
{
    int op1 = INSN(22, 21);
    int op = INSN(5,5);

    switch(op1) {
        case 0:
            smlaxy_a1(regs, insn);
            break;
        case 1:
            if (op)
                smulwy_a1(regs, insn);
            else
                smlawy_a1(regs, insn);
            break;
        case 2:
            smlalxy_a1(regs, insn);
            break;
        case 3:
            smulxy_a1(regs, insn);
            break;
        default:
            fatal("op1 = %d(0x%x)\n", op1, op1);
    }
}

void arm_hlp_signed_multiplies(uint64_t regs, uint32_t insn)
{
    int op1 = INSN(22, 20);
    int a = INSN(15, 12);

    switch(op1) {
        case 0:
            if (a == 15)
                smuad_smusd_a1(regs, insn);
            else
                smlad_smlsd_a1(regs, insn);
            break;
        case 4:
            smlald_smlsld_a1(regs, insn);
            break;
        case 5:
            if (a == 15)
                smmul_a1(regs, insn);
            else
                smmla_smmls_a1(regs, insn);
            break;
        default:
            fatal("op1 = %d(0x%x)\n", op1, op1);
    }
}

void arm_hlp_saturation(uint64_t regs, uint32_t insn)
{
    int op1 = INSN(22, 20);
    int op2 = INSN(7, 5);

    switch(op1) {
        case 2:
            if ((op2 & 1) == 0) {
                dis_ssat_a1(regs, insn);
            } else if (op2 == 1) {
                dis_ssat16_a1(regs, insn);
            } else
                assert(0);
            break;
        case 3:
            if ((op2 & 1) == 0) {
                dis_ssat_a1(regs, insn);
            } else
                assert(0);
            break;
        case 6:
            if ((op2 & 1) == 0) {
                dis_usat_a1(regs, insn);
            } else if (op2 == 1) {
                dis_usat16_a1(regs, insn);
            } else
                assert(0);
            break;
        case 7:
            if ((op2 & 1) == 0) {
                dis_usat_a1(regs, insn);
            } else
                assert(0);
            break;
        default:
            fatal("op1 = %d(0x%x)\n", op1, op1);
    }
}

void t32_hlp_saturation(uint64_t regs, uint32_t insn)
{
    int op = INSN1(8, 4);

    switch(op) {
        case 16:
            dis_ssat_t32(regs, insn);
            break;
        case 18:
            if (INSN2(14, 12) || INSN2(7, 6))
                dis_ssat_t32(regs, insn);
            else
                dis_ssat16_t32(regs, insn);
            break;
        case 24:
            dis_usat_t32(regs, insn);
            break;
        case 26:
            if (INSN2(14, 12) || INSN2(7, 6))
                dis_usat_t32(regs, insn);
            else
                dis_usat16_t32(regs, insn);
            break;
        default:
            fatal("op = %d(0x%x)\n", op, op);
    }
}

void arm_hlp_umaal(uint64_t _regs, uint32_t rdhi_nb, uint32_t rdlo_nb, uint32_t rn_nb, uint32_t rm_nb)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    uint64_t rdhi = regs->r[rdhi_nb];
    uint64_t rdlo = regs->r[rdlo_nb];
    uint64_t rn = regs->r[rn_nb];
    uint64_t rm = regs->r[rm_nb];
    uint64_t result = rn * rm + rdhi + rdlo;

    regs->r[rdhi_nb] = result >> 32;
    regs->r[rdlo_nb] = result;
}

void arm_hlp_sum_absolute_difference(uint64_t _regs, uint32_t insn)
{
    usad8_usada8(_regs, INSN(19, 16), INSN(3, 0), INSN(11, 8), INSN(15, 12));
}

void thumb_hlp_t2_misc_saturating(uint64_t regs, uint32_t insn)
{
    int op2 = INSN2(5, 4);

    switch(op2) {
        case 0:
            qadd_t32(regs, insn);
            break;
        case 1:
            qdadd_t32(regs, insn);
            break;
        case 2:
            qsub_t32(regs, insn);
            break;
        case 3:
            qdsub_t32(regs, insn);
            break;
        default:
            fatal("op2 = %d(0x%x)\n", op2, op2);
    }
}

void thumb_hlp_t2_signed_parallel(uint64_t regs, uint32_t insn)
{
    int op1 = INSN1(6, 4);
    int op2 = INSN2(5, 4);

    switch(op2) {
        case 0:
            switch(op1) {
                case 0: case 4:
                    sadd8_ssub8_t32(regs, insn);
                    break;
                case 1: case 5:
                    sadd16_ssub16_t32(regs, insn);
                    break;
                case 2:
                    sasx_ssax_t32(regs, insn);
                    break;
                case 6:
                    sasx_ssax_t32(regs, insn);
                    break;
                default:
                    fatal("op1 = %d(0x%x)\n", op1, op1);
            }
            break;
        case 1:
            switch(op1) {
                case 0:
                    qadd8_t32(regs, insn);
                    break;
                case 1:
                    qadd16_t32(regs, insn);
                    break;
                case 2:
                    qasx_t32(regs, insn);
                    break;
                case 4:
                    qsub8_t32(regs, insn);
                    break;
                case 5:
                    qsub16_t32(regs, insn);
                    break;
                case 6:
                    qsax_t32(regs, insn);
                    break;
                default:
                    fatal("op1 = %d(0x%x)\n", op1, op1);
            }
            break;
        case 2:
            switch(op1) {
                case 0:
                    shadd8_t32(regs, insn);
                    break;
                case 1:
                    shadd16_t32(regs, insn);
                    break;
                case 2:
                    shasx_t32(regs, insn);
                    break;
                case 4:
                    shsub8_t32(regs, insn);
                    break;
                case 5:
                    shsub16_t32(regs, insn);
                    break;
                case 6:
                    shsax_t32(regs, insn);
                    break;
                default:
                    fatal("op1 = %d(0x%x)\n", op1, op1);
            }
            break;
        default:
            fatal("op2 = %d(0x%x)\n", op2, op2);
    }
}

void thumb_hlp_t2_mult_A_mult_acc_A_abs_diff(uint64_t regs, uint32_t insn)
{
    int op1 = INSN1(6, 4);
    //int op2 = INSN2(5, 4);
    int ra = INSN2(15, 12);

    switch(op1) {
        case 1:
            if (ra == 15)
                smulxy_t32(regs, insn);
            else
                smlaxy_t32(regs, insn);
            break;
        case 2:
            if (ra == 15)
                smuad_smusd_t32(regs, insn);
            else
                smlad_smlsd_t32(regs, insn);
            break;
        case 3:
            if (ra == 15)
                smulwy_t32(regs, insn);
            else
                smlawy_t32(regs, insn);
            break;
        case 4:
            if (ra == 15)
                smuad_smusd_t32(regs, insn);
            else
                smlad_smlsd_t32(regs, insn);
            break;
        case 5:
            if (ra == 15)
                smmul_t32(regs, insn);
            else
                smmla_smmls_t32(regs, insn);
            break;
        case 6:
            if (ra == 15)
                assert(0);
            else
                smmla_smmls_t32(regs, insn);
            break;
        case 7:
            usad8_usada8_t32(regs, insn);
            break;
        default:
           fatal("op1 = %d(0x%x)\n", op1, op1);
    }
}

void thumb_hlp_t2_long_mult_A_long_mult_acc_A_div(uint64_t regs, uint32_t insn)
{
    int op1 = INSN1(6, 4);
    int op2 = INSN2(7, 4);

    switch(op1) {
        case 1:
            sdiv(regs, insn);
            break;
        case 3:
            udiv(regs, insn);
            break;
        case 4:
            if ((op2 & 0xc) == 0x8)
                smlalxy_t32(regs, insn);
            else if ((op2 & 0xe) == 0xc)
                smlald_smlsld_t32(regs, insn);
            else
                assert(0);
            break;
        case 5:
            smlald_smlsld_t32(regs, insn);
            break;
        case 6:
            if (op2)
                arm_hlp_umaal(regs, INSN2(11, 8), INSN2(15, 12), INSN1(3, 0), INSN2(3, 0));
            else
                assert(0);
            break;
        default:
           fatal("op1 = %d(0x%x)\n", op1, op1);
    }
}

void hlp_arm_vldm(uint64_t regs, uint32_t insn, uint32_t is_thumb)
{
    vldm(regs, insn, is_thumb);
}

void hlp_arm_vstm(uint64_t regs, uint32_t insn, uint32_t is_thumb)
{
    vstm(regs, insn, is_thumb);
}

void hlp_common_vfp_data_processing_insn(uint64_t regs, uint32_t insn)
{
    int opc1 = INSN(23, 20);
    int opc2 = INSN(19, 16);
    int opc3_1 = INSN(7, 7);
    int opc3_0 = INSN(6, 6);

    switch(opc1) {
        case 0: case 4:
            dis_common_vmla_vmls_vfp(regs, insn);
            break;
        case 1: case 5:
            dis_common_vnmla_vnmls_vfp(regs, insn);
            break;
        case 2: case 6:
            if (opc3_0)
                dis_common_vnmul_vfp(regs, insn);
            else
                dis_common_vmul_vfp(regs, insn);
            break;
        case 3: case 7:
            dis_common_vadd_vsub_vfp(regs, insn);
            break;
        case 8: case 12:
            dis_common_vdiv_vfp(regs, insn);
            break;
        case 11: case 15:
            switch(opc2) {
                case 0:
                    dis_common_vabs_vfp(regs, insn);
                    break;
                case 1:
                    if (opc3_1)
                        dis_common_vsqrt_vfp(regs, insn);
                    else
                        dis_common_vneg_vfp(regs, insn);
                    break;
                case 4: case 5:
                    dis_common_vcmp_vcmpe_vfp(regs, insn);
                    break;
                case 7:
                    dis_common_vcvt_double_single_vfp(regs, insn);
                    break;
                case 8: case 12: case 13:
                    dis_common_vcvt_vcvtr_floating_integer_vfp(regs, insn);
                    break;
                default:
                    fatal("opc2 = %d(0x%x)\n", opc2, opc2);
            }
            break;
        default:
            fatal("opc1 = %d(0x%x)\n", opc1, opc1);
    }
}

void hlp_common_adv_simd_three_same_length(uint64_t regs, uint32_t insn, uint32_t is_thumb)
{
    int a = INSN(11, 8);
    int b = INSN(4, 4);
    int u = is_thumb?INSN(28, 28):INSN(24, 24);
    int c = INSN(21, 20);

    switch(a) {
        case 0:
            b?dis_common_vqadd_simd(regs, insn, u):dis_common_vhadd_vhsub_simd(regs, insn, u);
            break;
        case 2:
            b?assert(0):dis_common_vhadd_vhsub_simd(regs, insn, u);
            break;
        case 3:
            b?dis_common_vcge_simd(regs, insn, u):dis_common_vcgt_simd(regs, insn, u);
            break;
        case 5:
            b?dis_common_vqrshl_simd(regs, insn, u):assert(0);
            break;
        case 6:
            dis_common_vmax_vmin_simd(regs, insn, u);
            break;
        case 7:
            dis_common_vaba_vabd_simd(regs, insn, is_thumb);
            break;
        case 8:
            if (b)
                u?dis_common_vceq_simd(regs, insn, u):assert(0);//vtst
            else
                u?assert(0):dis_common_vadd_simd(regs, insn);
            break;
        case 9:
            if (b)
                u?dis_common_vmul_polynomial_simd(regs, insn):dis_common_vmul_integer_simd(regs, insn);
            else
                dis_common_vmla_vmls_simd(regs, insn, u);
            break;
        case 10:
            b?dis_common_vpmin_simd(regs, insn, u):dis_common_vpmax_simd(regs, insn, u);
            break;
        case 11:
            if (b)
                dis_common_vpadd_simd(regs, insn);
            else
                dis_common_vqdmulh_vqrdmulh_simd(regs, insn, u);
            break;
        case 13:
            if (b)
                u?dis_common_vmul_f32_simd(regs, insn):dis_common_vmla_vmls_f32_simd(regs, insn);
            else {
                if (u) {
                    if (c&2)
                        assert(0);//vabd fpu
                    else
                        dis_common_vpadd_fpu_simd(regs, insn);
                } else {
                    if (c&2)
                        assert(0);//vsub fpu
                    else
                        dis_common_vadd_fpu_simd(regs, insn);
                }
            }
            break;
        case 14:
            if (b) {
                dis_common_vacge_vacgt_simd(regs, insn);
            } else {
                if (u)
                    (c&2)?dis_common_vcgt_fpu_simd(regs, insn):dis_common_vcge_fpu_simd(regs, insn);
                else
                    dis_common_vceq_fpu_simd(regs, insn);
            }
            break;
        case 15:
            if (b) {
                assert(0);
            } else {
                u?dis_common_vpmax_vpmin_fpu_simd(regs, insn):dis_common_vmax_vmin_fpu_simd(regs, insn);
            }
            break;
        default:
            fatal("a = %d(0x%x)\n", a, a);
    }
}

void hlp_common_adv_simd_three_different_length(uint64_t regs, uint32_t insn, uint32_t is_thumb)
{
    int a = INSN(11, 8);
    int u = is_thumb?INSN(28, 28):INSN(24, 24);

    switch(a) {
        case 0:
        case 1:
            dis_common_vaddl_vaddw_simd(regs, insn, is_thumb);
            break;
        case 4:
            if (u)
                assert(0);//vraddhn
            else
                dis_common_vaddhn_simd(regs, insn);
            break;
        case 5:
        case 7:
            dis_common_vabal_vabdl_simd(regs, insn, is_thumb);
            break;
        case 9: case 11:
            dis_common_vqdmlal_vqdmlsl_simd(regs, insn);
            break;
        case 8:
        case 10:
            dis_common_vmlal_vmlsl_simd(regs, insn, is_thumb);
            break;
        case 12:
            dis_common_vmull_integer_simd(regs, insn, is_thumb);
            break;
        case 13:
            dis_common_vqdmull_simd(regs, insn);
            break;
        case 14:
            dis_common_vmull_polynomial_simd(regs, insn, is_thumb);
            break;
        default:
            fatal("a = %d(0x%x)\n", a, a);
    }
}

void hlp_common_adv_simd_two_regs_misc(uint64_t regs, uint32_t insn)
{
    int a = INSN(17, 16);
    int b = INSN(10, 6);

    if (a == 0) {
        switch(b&0x1e) {
            case 8 ... 11:
                dis_common_vpaddl_simd(regs, insn);
                break;
            case 16:
                dis_common_vcls_simd(regs, insn);
                break;
            case 18:
                dis_common_vclz_simd(regs, insn);
                break;
            case 20:
                dis_common_vcnt_simd(regs, insn);
                break;
            case 24: case 26:
                dis_common_vpadal_simd(regs, insn);
                break;
            case 28:
                dis_common_vqabs_simd(regs, insn);
                break;
            case 30:
                dis_common_vqneg_simd(regs, insn);
                break;
            default:
                fatal("a = %d b = 0x%x\n", a, b);
        }
    } else if (a == 1) {
        switch(b&0xe) {
            case 0:
                dis_common_vcgt_immediate_simd(regs, insn);
                break;
            case 2:
                dis_common_vcge_immediate_simd(regs, insn);
                break;
            case 4:
                dis_common_vceq_immediate_simd(regs, insn);
                break;
            case 6:
                dis_common_vcle_immediate_simd(regs, insn);
                break;
            case 8:
                dis_common_vclt_immediate_simd(regs, insn);
                break;
            case 12:
                dis_common_vabs_simd(regs, insn);
                break;
            case 14:
                dis_common_vneg_simd(regs, insn);
                break;
            default:
                fatal("a = %d b = 0x%x\n", a, b);
        }
    } else if (a == 2) {
        switch(b) {
            case 8:
                dis_common_vmovn_simd(regs, insn);
                break;
            case 9:
                dis_common_vqmovun_simd(regs, insn);
                break;
            case 10:
            case 11:
                dis_common_vqmovn_simd(regs, insn);
                break;
            default:
                fatal("a = %d b = 0x%x\n", a, b);
        }
    } else if (a == 3) {
        switch(b&0x1a) {
            case 24: case 26:
                dis_common_vcvt_floating_integer_simd(regs, insn);
                break;
            default:
                fatal("a = %d b = 0x%x\n", a, b);
        }
    } else
        fatal("a = %d b = 0x%x\n", a, b);
}

void hlp_common_adv_simd_vdup_scalar(uint64_t regs, uint32_t insn)
{
    dis_common_vdup_scalar(regs, insn);
}

void hlp_common_adv_simd_vdup_arm(uint64_t regs, uint32_t insn)
{
    dis_common_vdup_arm(regs, insn);
}

void hlp_common_adv_simd_vext(uint64_t regs, uint32_t insn)
{
    dis_common_vext_simd(regs, insn);
}

void hlp_common_adv_simd_two_regs_and_scalar(uint64_t regs, uint32_t insn, uint32_t is_thumb)
{
    int a = INSN(11, 8);

    if ((a&0xa) == 0) {
        dis_common_vmla_vmls_scalar_simd(regs, insn, is_thumb);
    } else if ((a&0xb) == 0x2) {
        dis_common_vmlal_vmlsl_scalar_simd(regs, insn, is_thumb);
    } else if ((a&0xe) == 0x8) {
        dis_common_vmul_scalar_simd(regs, insn, is_thumb);
    } else if (a == 0xa) {
        dis_common_vmull_scalar_simd(regs, insn, is_thumb);
    } else if (a == 3 || a == 7) {
        dis_common_vqdmlal_vqdmlsl_scalar_simd(regs, insn);
    } else if (a == 11) {
        dis_common_vqdmull_scalar_simd(regs, insn, is_thumb);
    } else if (a == 12 || a == 13) {
        dis_common_vqdmulh_vqrdmulh_scalar_simd(regs, insn, is_thumb, a&1);
    } else
        fatal("a = %d(0x%x)\n", a, a);
}

void hlp_common_adv_simd_vmov_from_arm(uint64_t regs, uint32_t insn)
{
    dis_common_vmov_from_arm_simd(regs, insn);
}

void hlp_common_adv_simd_two_regs_and_shift(uint64_t regs, uint32_t insn, uint32_t is_thumb)
{
    int a = INSN(11, 8);
    int b = INSN(6, 6);
    int u = is_thumb?INSN(28, 28):INSN(24, 24);

    switch(a) {
        case 8:
            if (u)
                dis_common_vqrshrun_simd(regs, insn);
            else
                assert(0);
            break;
        case 9:
            if (b) {
                dis_common_vqrshrn_simd(regs, insn, u);
            } else
                assert(0);
            break;
        case 10:
            {
                int imm6 = INSN(21, 16);

                if (imm6 == 8 || imm6 == 16 || imm6 == 32)
                    dis_common_vmovl_simd(regs, insn, u);
                else
                    assert(0);//vshll
            }
            break;
        default:
            fatal("a = %d\n", a);
    }
}
