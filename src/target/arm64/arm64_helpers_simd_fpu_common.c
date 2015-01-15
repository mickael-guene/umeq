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
    if (isnanf(a))
        return b;
    else if (isnanf(b))
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
    if (isnanf(a))
        return b;
    else if (isnanf(b))
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

static void set_arm64_exception(uint64_t _regs, int excepts)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;

    if (excepts & FE_INVALID)
        regs->fpsr |= ARM64_FPSR_IOC;
    if (excepts & FE_DIVBYZERO)
        regs->fpsr |= ARM64_FPSR_DZC;
    if (excepts & FE_OVERFLOW)
        regs->fpsr |= ARM64_FPSR_OFC;
    if (excepts & FE_UNDERFLOW)
        regs->fpsr |= ARM64_FPSR_UFC;
    if (excepts & FE_INEXACT)
        regs->fpsr |= ARM64_FPSR_IXC;
}

static void set_host_rounding_mode_from_arm64(uint64_t _regs)
{
    const int arm64_rounding_mode_to_host[4] = {FE_TONEAREST, FE_UPWARD, FE_DOWNWARD, FE_TOWARDZERO};
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int arm64_rm = (regs->fpcr >> 22) & 3;

    fesetround(arm64_rounding_mode_to_host[arm64_rm]);
}
