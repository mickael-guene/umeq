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

#define float32_two make_float32(0x40000000)
#define float32_three make_float32(0x40400000)
#define float32_one_point_five make_float32(0x3fc00000)
#define float64_two make_float64(0x4000000000000000ULL)
#define float64_three make_float64(0x4008000000000000ULL)
#define float64_one_point_five make_float64(0x3FF8000000000000ULL)
#define float32_maxnorm make_float32(0x7f7fffff)
#define float64_512 make_float64(0x4080000000000000LL)
#define float64_256 make_float64(0x4070000000000000LL)
#define float64_maxnorm make_float64(0x7fefffffffffffffLL)

static inline uint32_t extract32(uint32_t value, int start, int length)
{
    assert(start >= 0 && length > 0 && length <= 32 - start);
    return (value >> start) & (~0U >> (32 - length));
}

static inline uint64_t extract64(uint64_t value, int start, int length)
{
    assert(start >= 0 && length > 0 && length <= 64 - start);
    return (value >> start) & (~0ULL >> (64 - length));
}

static bool round_to_inf(float_status *fpst, bool sign_bit)
{
    switch (fpst->float_rounding_mode) {
    case float_round_nearest_even: /* Round to Nearest */
        return true;
    case float_round_up: /* Round to +Inf */
        return !sign_bit;
    case float_round_down: /* Round to -Inf */
        return sign_bit;
    case float_round_to_zero: /* Round to Zero */
        return false;
    }

    assert(0);
}

static float64 recip_estimate(float64 a, float_status *real_fp_status)
{
    /* These calculations mustn't set any fp exception flags,
     * so we use a local copy of the fp_status.
     */
    float_status dummy_status = *real_fp_status;
    float_status *s = &dummy_status;
    /* q = (int)(a * 512.0) */
    float64 q = float64_mul(float64_512, a, s);
    int64_t q_int = float64_to_int64_round_to_zero(q, s);

    /* r = 1.0 / (((double)q + 0.5) / 512.0) */
    q = int64_to_float64(q_int, s);
    q = float64_add(q, float64_half, s);
    q = float64_div(q, float64_512, s);
    q = float64_div(float64_one, q, s);

    /* s = (int)(256.0 * r + 0.5) */
    q = float64_mul(q, float64_256, s);
    q = float64_add(q, float64_half, s);
    q_int = float64_to_int64_round_to_zero(q, s);

    /* return (double)s / 256.0 */
    return float64_div(int64_to_float64(q_int, s), float64_256, s);
}

static float64 call_recip_estimate(float64 num, int off, float_status *fpst)
{
    uint64_t val64 = float64_val(num);
    uint64_t frac = extract64(val64, 0, 52);
    int64_t exp = extract64(val64, 52, 11);
    uint64_t sbit;
    float64 scaled, estimate;

    /* Generate the scaled number for the estimate function */
    if (exp == 0) {
        if (extract64(frac, 51, 1) == 0) {
            exp = -1;
            frac = extract64(frac, 0, 50) << 2;
        } else {
            frac = extract64(frac, 0, 51) << 1;
        }
    }

    /* scaled = '0' : '01111111110' : fraction<51:44> : Zeros(44); */
    scaled = make_float64((0x3feULL << 52)
                          | extract64(frac, 44, 8) << 44);

    estimate = recip_estimate(scaled, fpst);

    /* Build new result */
    val64 = float64_val(estimate);
    sbit = 0x8000000000000000ULL & val64;
    exp = off - exp;
    frac = extract64(val64, 0, 52);

    if (exp == 0) {
        frac = 1ULL << 51 | extract64(frac, 1, 51);
    } else if (exp == -1) {
        frac = 1ULL << 50 | extract64(frac, 2, 50);
        exp = 0;
    }

    return make_float64(sbit | (exp << 52) | frac);
}

static float64 recip_sqrt_estimate(float64 a, float_status *real_fp_status)
{
    /* These calculations mustn't set any fp exception flags,
     * so we use a local copy of the fp_status.
     */
    float_status dummy_status = *real_fp_status;
    float_status *s = &dummy_status;
    float64 q;
    int64_t q_int;

    if (float64_lt(a, float64_half, s)) {
        /* range 0.25 <= a < 0.5 */

        /* a in units of 1/512 rounded down */
        /* q0 = (int)(a * 512.0);  */
        q = float64_mul(float64_512, a, s);
        q_int = float64_to_int64_round_to_zero(q, s);

        /* reciprocal root r */
        /* r = 1.0 / sqrt(((double)q0 + 0.5) / 512.0);  */
        q = int64_to_float64(q_int, s);
        q = float64_add(q, float64_half, s);
        q = float64_div(q, float64_512, s);
        q = float64_sqrt(q, s);
        q = float64_div(float64_one, q, s);
    } else {
        /* range 0.5 <= a < 1.0 */

        /* a in units of 1/256 rounded down */
        /* q1 = (int)(a * 256.0); */
        q = float64_mul(float64_256, a, s);
        int64_t q_int = float64_to_int64_round_to_zero(q, s);

        /* reciprocal root r */
        /* r = 1.0 /sqrt(((double)q1 + 0.5) / 256); */
        q = int64_to_float64(q_int, s);
        q = float64_add(q, float64_half, s);
        q = float64_div(q, float64_256, s);
        q = float64_sqrt(q, s);
        q = float64_div(float64_one, q, s);
    }
    /* r in units of 1/256 rounded to nearest */
    /* s = (int)(256.0 * r + 0.5); */

    q = float64_mul(q, float64_256,s );
    q = float64_add(q, float64_half, s);
    q_int = float64_to_int64_round_to_zero(q, s);

    /* return (double)s / 256.0;*/
    return float64_div(int64_to_float64(q_int, s), float64_256, s);
}

static inline uint32_t fneg32(struct arm64_registers *regs, uint32_t a)
{
    return a ^ 0x80000000;
}

static inline uint64_t fneg64(struct arm64_registers *regs, uint64_t a)
{
    return a ^ 0x8000000000000000UL;
}

static inline uint32_t fabs32(struct arm64_registers *regs, uint32_t a)
{
    return a & ~0x80000000;
}

static inline uint64_t fabs64(struct arm64_registers *regs, uint64_t a)
{
    return a & ~0x8000000000000000UL;
}

static inline uint32_t fsub32(struct arm64_registers *regs, uint32_t a, uint32_t b)
{
    return float32_val(float32_sub(make_float32(a), make_float32(b), &regs->fp_status));
}

static inline uint64_t fsub64(struct arm64_registers *regs, uint64_t a, uint64_t b)
{
    return float64_val(float64_sub(make_float64(a), make_float64(b), &regs->fp_status));
}

static inline uint32_t fadd32(struct arm64_registers *regs, uint32_t a, uint32_t b)
{
    return float32_val(float32_add(make_float32(a), make_float32(b), &regs->fp_status));
}

static inline uint64_t fadd64(struct arm64_registers *regs, uint64_t a, uint64_t b)
{
    return float64_val(float64_add(make_float64(a), make_float64(b), &regs->fp_status));
}

static inline uint32_t fmul32(struct arm64_registers *regs, uint32_t a, uint32_t b)
{
    return float32_val(float32_mul(make_float32(a), make_float32(b), &regs->fp_status));
}

static inline uint64_t fmul64(struct arm64_registers *regs, uint64_t a, uint64_t b)
{
    return float64_val(float64_mul(make_float64(a), make_float64(b), &regs->fp_status));
}

static inline uint32_t fmulx32(struct arm64_registers *regs, uint32_t a, uint32_t b)
{
    float_status *fp_status = &regs->fp_status;

    float32 a_f32 = float32_squash_input_denormal(make_float32(a), fp_status);
    float32 b_f32 = float32_squash_input_denormal(make_float32(b), fp_status);

    if ((float32_is_zero(a_f32) && float32_is_infinity(b_f32)) ||
        (float32_is_infinity(a_f32) && float32_is_zero(b_f32))) {
        /* return 2.0 with xor sign */
        return (1U << 30) | ((a ^ b) & (1U << 31));
    }
    else
        return float32_val(float32_mul(make_float32(a), make_float32(b), &regs->fp_status));
}

static inline uint64_t fmulx64(struct arm64_registers *regs, uint64_t a, uint64_t b)
{
    float_status *fp_status = &regs->fp_status;

    float64 a_f64 = float64_squash_input_denormal(make_float64(a), fp_status);
    float64 b_f64 = float64_squash_input_denormal(make_float64(b), fp_status);

    if ((float64_is_zero(a_f64) && float64_is_infinity(b_f64)) ||
        (float64_is_infinity(a_f64) && float64_is_zero(b_f64))) {
        /* return 2.0 with xor sign */
        return (1ULL << 62) | ((a ^ b) & (1ULL << 63));
    }
    else
        return float64_val(float64_mul(make_float64(a), make_float64(b), &regs->fp_status));
}

static inline uint32_t fdiv32(struct arm64_registers *regs, uint32_t a, uint32_t b)
{
    return float32_val(float32_div(make_float32(a), make_float32(b), &regs->fp_status));
}

static inline uint64_t fdiv64(struct arm64_registers *regs, uint64_t a, uint64_t b)
{
    return float64_val(float64_div(make_float64(a), make_float64(b), &regs->fp_status));
}

static inline uint32_t fmadd32(struct arm64_registers *regs, uint32_t a, uint32_t b, uint32_t acc)
{
    return float32_val(float32_muladd(make_float32(a), make_float32(b), make_float32(acc), 0, &regs->fp_status));
}

static inline uint64_t fmadd64(struct arm64_registers *regs, uint64_t a, uint64_t b, uint64_t acc)
{
    return float64_val(float64_muladd(make_float64(a), make_float64(b), make_float64(acc), 0, &regs->fp_status));
}

static inline uint32_t fmax32(struct arm64_registers *regs, uint32_t a, uint32_t b)
{
    return float32_val(float32_max(make_float32(a), make_float32(b), &regs->fp_status));
}

static inline uint64_t fmax64(struct arm64_registers *regs, uint64_t a, uint64_t b)
{
    return float64_val(float64_max(make_float64(a), make_float64(b), &regs->fp_status));
}

static inline uint32_t fmaxnm32(struct arm64_registers *regs, uint32_t a, uint32_t b)
{
    return float32_val(float32_maxnum(make_float32(a), make_float32(b), &regs->fp_status));
}

static inline uint64_t fmaxnm64(struct arm64_registers *regs, uint64_t a, uint64_t b)
{
    return float64_val(float64_maxnum(make_float64(a), make_float64(b), &regs->fp_status));
}

static inline uint32_t fmin32(struct arm64_registers *regs, uint32_t a, uint32_t b)
{
    return float32_val(float32_min(make_float32(a), make_float32(b), &regs->fp_status));
}

static inline uint64_t fmin64(struct arm64_registers *regs, uint64_t a, uint64_t b)
{
    return float64_val(float64_min(make_float64(a), make_float64(b), &regs->fp_status));
}

static inline uint32_t fminnm32(struct arm64_registers *regs, uint32_t a, uint32_t b)
{
    return float32_val(float32_minnum(make_float32(a), make_float32(b), &regs->fp_status));
}

static inline uint64_t fminnm64(struct arm64_registers *regs, uint64_t a, uint64_t b)
{
    return float64_val(float64_minnum(make_float64(a), make_float64(b), &regs->fp_status));
}

static inline int fcmpeq32(struct arm64_registers *regs, uint32_t a, uint32_t b)
{
    return float32_eq_quiet(make_float32(b), make_float32(a), &regs->fp_status);
}

static inline int fcmpeq64(struct arm64_registers *regs, uint64_t a, uint64_t b)
{
    return float64_eq_quiet(make_float64(b), make_float64(a), &regs->fp_status);
}

static inline int fcmpge32(struct arm64_registers *regs, uint32_t a, uint32_t b)
{
    return float32_le(make_float32(b), make_float32(a), &regs->fp_status);
}

static inline int fcmpge64(struct arm64_registers *regs, uint64_t a, uint64_t b)
{
    return float64_le(make_float64(b), make_float64(a), &regs->fp_status);
}

static inline int fcmpgt32(struct arm64_registers *regs, uint32_t a, uint32_t b)
{
    return float32_lt(make_float32(b), make_float32(a), &regs->fp_status);
}

static inline int fcmpgt64(struct arm64_registers *regs, uint64_t a, uint64_t b)
{
    return float64_lt(make_float64(b), make_float64(a), &regs->fp_status);
}

static inline int fcmple32(struct arm64_registers *regs, uint32_t a, uint32_t b)
{
    return float32_le(make_float32(a), make_float32(b), &regs->fp_status);
}

static inline int fcmple64(struct arm64_registers *regs, uint64_t a, uint64_t b)
{
    return float64_le(make_float64(a), make_float64(b), &regs->fp_status);
}

static inline int fcmplt32(struct arm64_registers *regs, uint32_t a, uint32_t b)
{
    return float32_lt(make_float32(a), make_float32(b), &regs->fp_status);
}

static inline int fcmplt64(struct arm64_registers *regs, uint64_t a, uint64_t b)
{
    return float64_lt(make_float64(a), make_float64(b), &regs->fp_status);
}

static inline int fcmp32(struct arm64_registers *regs, uint32_t a, uint32_t b, int is_quiet)
{
    int float_relation;

    if (is_quiet)
        float_relation = float32_compare_quiet(make_float32(a), make_float32(b), &regs->fp_status);
    else
        float_relation = float32_compare(make_float32(a), make_float32(b), &regs->fp_status);

    switch(float_relation) {
        case float_relation_less:
            return 8;
            break;
        case float_relation_equal:
            return 6;
            break;
        case float_relation_greater:
            return 2;
            break;
        case float_relation_unordered:
        default:
            return 3;
    }
}

static inline int fcmp64(struct arm64_registers *regs, uint64_t a, uint64_t b, int is_quiet)
{
    int float_relation;

    if (is_quiet)
        float_relation = float64_compare_quiet(make_float64(a), make_float64(b), &regs->fp_status);
    else
        float_relation = float64_compare(make_float64(a), make_float64(b), &regs->fp_status);

    switch(float_relation) {
        case float_relation_less:
            return 8;
            break;
        case float_relation_equal:
            return 6;
            break;
        case float_relation_greater:
            return 2;
            break;
        case float_relation_unordered:
        default:
            return 3;
    }
}

static inline uint32_t frecpe32(struct arm64_registers *regs, uint32_t a)
{
    float_status *fpst = &regs->fp_status;
    float32 f32 = float32_squash_input_denormal(a, fpst);
    uint32_t f32_val = float32_val(f32);
    uint32_t f32_sbit = 0x80000000ULL & f32_val;
    int32_t f32_exp = extract32(f32_val, 23, 8);
    uint32_t f32_frac = extract32(f32_val, 0, 23);
    float64 f64, r64;
    uint64_t r64_val;
    int64_t r64_exp;
    uint64_t r64_frac;

    if (float32_is_any_nan(f32)) {
        float32 nan = f32;
        if (float32_is_signaling_nan(f32)) {
            float_raise(float_flag_invalid, fpst);
            nan = float32_maybe_silence_nan(f32);
        }
        if (fpst->default_nan_mode) {
            nan =  float32_default_nan;
        }
        return nan;
    } else if (float32_is_infinity(f32)) {
        return float32_set_sign(float32_zero, float32_is_neg(f32));
    } else if (float32_is_zero(f32)) {
        float_raise(float_flag_divbyzero, fpst);
        return float32_set_sign(float32_infinity, float32_is_neg(f32));
    } else if ((f32_val & ~(1ULL << 31)) < (1ULL << 21)) {
        /* Abs(value) < 2.0^-128 */
        float_raise(float_flag_overflow | float_flag_inexact, fpst);
        if (round_to_inf(fpst, f32_sbit)) {
            return float32_set_sign(float32_infinity, float32_is_neg(f32));
        } else {
            return float32_set_sign(float32_maxnorm, float32_is_neg(f32));
        }
    } else if (f32_exp >= 253 && fpst->flush_to_zero) {
        float_raise(float_flag_underflow, fpst);
        return float32_set_sign(float32_zero, float32_is_neg(f32));
    }


    f64 = make_float64(((int64_t)(f32_exp) << 52) | (int64_t)(f32_frac) << 29);
    r64 = call_recip_estimate(f64, 253, fpst);
    r64_val = float64_val(r64);
    r64_exp = extract64(r64_val, 52, 11);
    r64_frac = extract64(r64_val, 0, 52);

    /* result = sign : result_exp<7:0> : fraction<51:29>; */
    return make_float32(f32_sbit |
                        (r64_exp & 0xff) << 23 |
                        extract64(r64_frac, 29, 24));
}

static inline uint64_t frecpe64(struct arm64_registers *regs, uint64_t a)
{
    float_status *fpst = &regs->fp_status;
    float64 f64 = float64_squash_input_denormal(a, fpst);
    uint64_t f64_val = float64_val(f64);
    uint64_t f64_sbit = 0x8000000000000000ULL & f64_val;
    int64_t f64_exp = extract64(f64_val, 52, 11);
    float64 r64;
    uint64_t r64_val;
    int64_t r64_exp;
    uint64_t r64_frac;

    /* Deal with any special cases */
    if (float64_is_any_nan(f64)) {
        float64 nan = f64;
        if (float64_is_signaling_nan(f64)) {
            float_raise(float_flag_invalid, fpst);
            nan = float64_maybe_silence_nan(f64);
        }
        if (fpst->default_nan_mode) {
            nan =  float64_default_nan;
        }
        return nan;
    } else if (float64_is_infinity(f64)) {
        return float64_set_sign(float64_zero, float64_is_neg(f64));
    } else if (float64_is_zero(f64)) {
        float_raise(float_flag_divbyzero, fpst);
        return float64_set_sign(float64_infinity, float64_is_neg(f64));
    } else if ((f64_val & ~(1ULL << 63)) < (1ULL << 50)) {
        /* Abs(value) < 2.0^-1024 */
        float_raise(float_flag_overflow | float_flag_inexact, fpst);
        if (round_to_inf(fpst, f64_sbit)) {
            return float64_set_sign(float64_infinity, float64_is_neg(f64));
        } else {
            return float64_set_sign(float64_maxnorm, float64_is_neg(f64));
        }
    } else if (f64_exp >= 2045 && fpst->flush_to_zero) {
        float_raise(float_flag_underflow, fpst);
        return float64_set_sign(float64_zero, float64_is_neg(f64));
    }

    r64 = call_recip_estimate(f64, 2045, fpst);
    r64_val = float64_val(r64);
    r64_exp = extract64(r64_val, 52, 11);
    r64_frac = extract64(r64_val, 0, 52);

    /* result = sign : result_exp<10:0> : fraction<51:0> */
    return make_float64(f64_sbit |
                        ((r64_exp & 0x7ff) << 52) |
                        r64_frac);
}

static inline uint32_t frecps32(struct arm64_registers *regs, uint32_t a, uint32_t b)
{
    float_status *fpst = &regs->fp_status;

    a = float32_squash_input_denormal(a, fpst);
    b = float32_squash_input_denormal(b, fpst);

    a = float32_chs(a);
    if ((float32_is_infinity(a) && float32_is_zero(b)) || (float32_is_infinity(b) && float32_is_zero(a)))
        return float32_two;

    return float32_muladd(a, b, float32_two, 0, fpst);
}

static inline uint64_t frecps64(struct arm64_registers *regs, uint64_t a, uint64_t b)
{
    float_status *fpst = &regs->fp_status;

    a = float64_squash_input_denormal(a, fpst);
    b = float64_squash_input_denormal(b, fpst);

    a = float64_chs(a);
    if ((float64_is_infinity(a) && float64_is_zero(b)) || (float64_is_infinity(b) && float64_is_zero(a)))
        return float64_two;

    return float64_muladd(a, b, float64_two, 0, fpst);
}

static inline uint32_t frecpx32(struct arm64_registers *regs, uint32_t a)
{
    float_status *fpst = &regs->fp_status;
    uint32_t val32, sbit;
    int32_t exp;

    if (float32_is_any_nan(a)) {
        float32 nan = a;
        if (float32_is_signaling_nan(a)) {
            float_raise(float_flag_invalid, fpst);
            nan = float32_maybe_silence_nan(a);
        }
        if (fpst->default_nan_mode) {
            nan = float32_default_nan;
        }
        return nan;
    }

    val32 = float32_val(a);
    sbit = 0x80000000ULL & val32;
    exp = extract32(val32, 23, 8);

    if (exp == 0) {
        return make_float32(sbit | (0xfe << 23));
    } else {
        return make_float32(sbit | (~exp & 0xff) << 23);
    }
}

static inline uint64_t frecpx64(struct arm64_registers *regs, uint64_t a)
{
    float_status *fpst = &regs->fp_status;
    uint64_t val64, sbit;
    int64_t exp;

    if (float64_is_any_nan(a)) {
        float64 nan = a;
        if (float64_is_signaling_nan(a)) {
            float_raise(float_flag_invalid, fpst);
            nan = float64_maybe_silence_nan(a);
        }
        if (fpst->default_nan_mode) {
            nan = float64_default_nan;
        }
        return nan;
    }

    val64 = float64_val(a);
    sbit = 0x8000000000000000ULL & val64;
    exp = extract64(float64_val(a), 52, 11);

    if (exp == 0) {
        return make_float64(sbit | (0x7feULL << 52));
    } else {
        return make_float64(sbit | (~exp & 0x7ffULL) << 52);
    }
}

static inline uint32_t frsqrte32(struct arm64_registers *regs, uint32_t a)
{
    float_status *s = &regs->fp_status;
    float32 f32 = float32_squash_input_denormal(a, s);
    uint32_t val = float32_val(f32);
    uint32_t f32_sbit = 0x80000000 & val;
    int32_t f32_exp = extract32(val, 23, 8);
    uint32_t f32_frac = extract32(val, 0, 23);
    uint64_t f64_frac;
    uint64_t val64;
    int result_exp;
    float64 f64;

    if (float32_is_any_nan(f32)) {
        float32 nan = f32;
        if (float32_is_signaling_nan(f32)) {
            float_raise(float_flag_invalid, s);
            nan = float32_maybe_silence_nan(f32);
        }
        if (s->default_nan_mode) {
            nan =  float32_default_nan;
        }
        return nan;
    } else if (float32_is_zero(f32)) {
        float_raise(float_flag_divbyzero, s);
        return float32_set_sign(float32_infinity, float32_is_neg(f32));
    } else if (float32_is_neg(f32)) {
        float_raise(float_flag_invalid, s);
        return float32_default_nan;
    } else if (float32_is_infinity(f32)) {
        return float32_zero;
    }

    /* Scale and normalize to a double-precision value between 0.25 and 1.0,
     * preserving the parity of the exponent.  */

    f64_frac = ((uint64_t) f32_frac) << 29;
    if (f32_exp == 0) {
        while (extract64(f64_frac, 51, 1) == 0) {
            f64_frac = f64_frac << 1;
            f32_exp = f32_exp-1;
        }
        f64_frac = extract64(f64_frac, 0, 51) << 1;
    }

    if (extract64(f32_exp, 0, 1) == 0) {
        f64 = make_float64(((uint64_t) f32_sbit) << 32
                           | (0x3feULL << 52)
                           | f64_frac);
    } else {
        f64 = make_float64(((uint64_t) f32_sbit) << 32
                           | (0x3fdULL << 52)
                           | f64_frac);
    }

    result_exp = (380 - f32_exp) / 2;

    f64 = recip_sqrt_estimate(f64, s);

    val64 = float64_val(f64);

    val = ((result_exp & 0xff) << 23)
        | ((val64 >> 29)  & 0x7fffff);
    return make_float32(val);
}

static inline uint64_t frsqrte64(struct arm64_registers *regs, uint64_t a)
{
    float_status *s = &regs->fp_status;
    float64 f64 = float64_squash_input_denormal(a, s);
    uint64_t val = float64_val(f64);
    uint64_t f64_sbit = 0x8000000000000000ULL & val;
    int64_t f64_exp = extract64(val, 52, 11);
    uint64_t f64_frac = extract64(val, 0, 52);
    int64_t result_exp;
    uint64_t result_frac;

    if (float64_is_any_nan(f64)) {
        float64 nan = f64;
        if (float64_is_signaling_nan(f64)) {
            float_raise(float_flag_invalid, s);
            nan = float64_maybe_silence_nan(f64);
        }
        if (s->default_nan_mode) {
            nan =  float64_default_nan;
        }
        return nan;
    } else if (float64_is_zero(f64)) {
        float_raise(float_flag_divbyzero, s);
        return float64_set_sign(float64_infinity, float64_is_neg(f64));
    } else if (float64_is_neg(f64)) {
        float_raise(float_flag_invalid, s);
        return float64_default_nan;
    } else if (float64_is_infinity(f64)) {
        return float64_zero;
    }

    /* Scale and normalize to a double-precision value between 0.25 and 1.0,
     * preserving the parity of the exponent.  */

    if (f64_exp == 0) {
        while (extract64(f64_frac, 51, 1) == 0) {
            f64_frac = f64_frac << 1;
            f64_exp = f64_exp - 1;
        }
        f64_frac = extract64(f64_frac, 0, 51) << 1;
    }

    if (extract64(f64_exp, 0, 1) == 0) {
        f64 = make_float64(f64_sbit
                           | (0x3feULL << 52)
                           | f64_frac);
    } else {
        f64 = make_float64(f64_sbit
                           | (0x3fdULL << 52)
                           | f64_frac);
    }

    result_exp = (3068 - f64_exp) / 2;

    f64 = recip_sqrt_estimate(f64, s);

    result_frac = extract64(float64_val(f64), 0, 52);

    return make_float64(f64_sbit |
                        ((result_exp & 0x7ff) << 52) |
                        result_frac);
}

static inline uint32_t frsqrts32(struct arm64_registers *regs, uint32_t a, uint32_t b)
{
    float_status *fpst = &regs->fp_status;

    a = float32_squash_input_denormal(a, fpst);
    b = float32_squash_input_denormal(b, fpst);

    a = float32_chs(a);
    if ((float32_is_infinity(a) && float32_is_zero(b)) || (float32_is_infinity(b) && float32_is_zero(a)))
        return float32_one_point_five;

    return float32_muladd(a, b, float32_three, float_muladd_halve_result, fpst);
}

static inline uint64_t frsqrts64(struct arm64_registers *regs, uint64_t a, uint64_t b)
{
    float_status *fpst = &regs->fp_status;

    a = float64_squash_input_denormal(a, fpst);
    b = float64_squash_input_denormal(b, fpst);

    a = float64_chs(a);
    if ((float64_is_infinity(a) && float64_is_zero(b)) || (float64_is_infinity(b) && float64_is_zero(a)))
        return float64_one_point_five;

    return float64_muladd(a, b, float64_three, float_muladd_halve_result, fpst);
}

static inline uint32_t fsqrt32(struct arm64_registers *regs, uint32_t a)
{
    return float32_val(float32_sqrt(make_float32(a), &regs->fp_status));
}

static inline uint64_t fsqrt64(struct arm64_registers *regs, uint64_t a)
{
    return float64_val(float64_sqrt(make_float64(a), &regs->fp_status));
}

static inline uint32_t fcvtxn64(struct arm64_registers *regs, uint64_t a)
{
    float32 r;
    float_status *fpst = &regs->fp_status;
    float_status tstat = *fpst;
    int exflags;

    set_float_rounding_mode(float_round_to_zero, &tstat);
    set_float_exception_flags(0, &tstat);
    r = float64_to_float32(make_float64(a), &tstat);
    r = float32_maybe_silence_nan(r);
    exflags = get_float_exception_flags(&tstat);
    if (exflags & float_flag_inexact) {
        r = make_float32(float32_val(r) | 1);
    }
    exflags |= get_float_exception_flags(fpst);
    set_float_exception_flags(exflags, fpst);

    return float32_val(r);
}

/* FIXME: not sure overflow is handled correctly .... Code was taken from qemu and not sure
it's correct for exceptions results */
static inline uint32_t fcvtzs32_fixed_32(struct arm64_registers *regs, uint32_t a, int fracbits)
{
    uint32_t res;
    float_status *fpst = &regs->fp_status;
    float32 a32 = make_float32(a);

    if (float32_is_any_nan(a32)) {
        float_raise(float_flag_invalid, fpst);
        res = 0;
    } else {
        int old_exc_flags = get_float_exception_flags(fpst);

        /* here we do not handle overflow correctly .... */
        a32 = float32_scalbn(a32, fracbits, fpst);
        old_exc_flags |= get_float_exception_flags(fpst) & float_flag_input_denormal;
        set_float_exception_flags(old_exc_flags, fpst);

        res = float32_val(float32_to_int32_round_to_zero(a32, fpst));
    }

    return res;
}

/* FIXME: not sure overflow is handled correctly .... Code was taken from qemu and not sure
it's correct for exceptions results */
static inline uint64_t fcvtzs32_fixed_64(struct arm64_registers *regs, uint32_t a, int fracbits)
{
    uint64_t res;
    float_status *fpst = &regs->fp_status;
    float32 a32 = make_float32(a);

    if (float32_is_any_nan(a32)) {
        float_raise(float_flag_invalid, fpst);
        res = 0;
    } else {
        int old_exc_flags = get_float_exception_flags(fpst);

        /* here we do not handle overflow correctly .... */
        a32 = float32_scalbn(a32, fracbits, fpst);
        old_exc_flags |= get_float_exception_flags(fpst) & float_flag_input_denormal;
        set_float_exception_flags(old_exc_flags, fpst);

        res = float64_val(float32_to_int64_round_to_zero(a32, fpst));
    }

    return res;
}

/* FIXME: not sure overflow is handled correctly .... Code was taken from qemu and not sure
it's correct for exceptions results */
static inline uint64_t fcvtzs64_fixed_64(struct arm64_registers *regs, uint64_t a, int fracbits)
{
    uint64_t res;
    float_status *fpst = &regs->fp_status;
    float64 a64 = make_float64(a);

    if (float64_is_any_nan(a64)) {
        float_raise(float_flag_invalid, fpst);
        res = 0;
    } else {
        int old_exc_flags = get_float_exception_flags(fpst);

        /* here we do not handle overflow correctly .... */
        a64 = float64_scalbn(a64, fracbits, fpst);
        old_exc_flags |= get_float_exception_flags(fpst) & float_flag_input_denormal;
        set_float_exception_flags(old_exc_flags, fpst);

        res = float64_val(float64_to_int64_round_to_zero(a64, fpst));
    }

    return res;
}

/* FIXME: not sure overflow is handled correctly .... Code was taken from qemu and not sure
it's correct for exceptions results */
static inline uint32_t fcvtzs64_fixed_32(struct arm64_registers *regs, uint64_t a, int fracbits)
{
    uint32_t res;
    float_status *fpst = &regs->fp_status;
    float64 a64 = make_float64(a);

    if (float64_is_any_nan(a64)) {
        float_raise(float_flag_invalid, fpst);
        res = 0;
    } else {
        int old_exc_flags = get_float_exception_flags(fpst);

        /* here we do not handle overflow correctly .... */
        a64 = float64_scalbn(a64, fracbits, fpst);
        old_exc_flags |= get_float_exception_flags(fpst) & float_flag_input_denormal;
        set_float_exception_flags(old_exc_flags, fpst);

        res = float32_val(float64_to_int32_round_to_zero(a64, fpst));
    }

    return res;
}

/* FIXME: not sure overflow is handled correctly .... Code was taken from qemu and not sure
it's correct for exceptions results */
static inline uint32_t fcvtzu32_fixed_32(struct arm64_registers *regs, uint32_t a, int fracbits)
{
    uint32_t res;
    float_status *fpst = &regs->fp_status;
    float32 a32 = make_float32(a);

    if (float32_is_any_nan(a32)) {
        float_raise(float_flag_invalid, fpst);
        res = 0;
    } else {
        int old_exc_flags = get_float_exception_flags(fpst);

        /* here we do not handle overflow correctly .... */
        a32 = float32_scalbn(a32, fracbits, fpst);
        old_exc_flags |= get_float_exception_flags(fpst) & float_flag_input_denormal;
        set_float_exception_flags(old_exc_flags, fpst);

        res = float32_val(float32_to_uint32_round_to_zero(a32, fpst));
    }

    return res;
}

/* FIXME: not sure overflow is handled correctly .... Code was taken from qemu and not sure
it's correct for exceptions results */
static inline uint64_t fcvtzu32_fixed_64(struct arm64_registers *regs, uint32_t a, int fracbits)
{
    uint64_t res;
    float_status *fpst = &regs->fp_status;
    float32 a32 = make_float32(a);

    if (float32_is_any_nan(a32)) {
        float_raise(float_flag_invalid, fpst);
        res = 0;
    } else {
        int old_exc_flags = get_float_exception_flags(fpst);

        /* here we do not handle overflow correctly .... */
        a32 = float32_scalbn(a32, fracbits, fpst);
        old_exc_flags |= get_float_exception_flags(fpst) & float_flag_input_denormal;
        set_float_exception_flags(old_exc_flags, fpst);

        res = float64_val(float32_to_uint64_round_to_zero(a32, fpst));
    }

    return res;
}

/* FIXME: not sure overflow is handled correctly .... Code was taken from qemu and not sure
it's correct for exceptions results */
static inline uint64_t fcvtzu64_fixed_64(struct arm64_registers *regs, uint64_t a, int fracbits)
{
    uint64_t res;
    float_status *fpst = &regs->fp_status;
    float64 a64 = make_float64(a);

    if (float64_is_any_nan(a64)) {
        float_raise(float_flag_invalid, fpst);
        res = 0;
    } else {
        int old_exc_flags = get_float_exception_flags(fpst);

        /* here we do not handle overflow correctly .... */
        a64 = float64_scalbn(a64, fracbits, fpst);
        old_exc_flags |= get_float_exception_flags(fpst) & float_flag_input_denormal;
        set_float_exception_flags(old_exc_flags, fpst);

        res = float64_val(float64_to_uint64_round_to_zero(a64, fpst));
    }

    return res;
}

/* FIXME: not sure overflow is handled correctly .... Code was taken from qemu and not sure
it's correct for exceptions results */
static inline uint32_t fcvtzu64_fixed_32(struct arm64_registers *regs, uint64_t a, int fracbits)
{
    uint32_t res;
    float_status *fpst = &regs->fp_status;
    float64 a64 = make_float64(a);

    if (float64_is_any_nan(a64)) {
        float_raise(float_flag_invalid, fpst);
        res = 0;
    } else {
        int old_exc_flags = get_float_exception_flags(fpst);

        /* here we do not handle overflow correctly .... */
        a64 = float64_scalbn(a64, fracbits, fpst);
        old_exc_flags |= get_float_exception_flags(fpst) & float_flag_input_denormal;
        set_float_exception_flags(old_exc_flags, fpst);

        res = float32_val(float64_to_uint32_round_to_zero(a64, fpst));
    }

    return res;
}

static inline void dis_fabs(uint64_t _regs, uint32_t insn)
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
                res.s[i] = fabs32(regs, regs->v[rn].s[i]);
            break;
        case 1: case 3:
            for(i = 0; i < (is_scalar?1:(q?2:1)); i++)
                res.d[i] = fabs64(regs, regs->v[rn].d[i]);
            break;
        default:
            assert(0);
    }

    regs->v[rd] = res;
}

static inline void dis_fneg(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int is_scalar = INSN(28,28);
    int is_double = INSN(22,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    union simd_register res = {0};
    int i;

    if (is_double)
        for(i = 0; i < (is_scalar?1:2); i++)
            res.d[i] = fneg64(regs, regs->v[rn].d[i]);
    else
        for(i = 0; i < (is_scalar?1:(q?4:2)); i++)
            res.s[i] = fneg32(regs, regs->v[rn].s[i]);
    regs->v[rd] = res;
}
