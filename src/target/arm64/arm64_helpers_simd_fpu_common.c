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

static double floor(double d)
{
  double res;
# if defined __AVX__ || defined SSE2AVX
  asm ("vroundsd $1, %1, %0, %0" : "=x" (res) : "xm" (d));
# else
  asm ("roundsd $1, %1, %0" : "=x" (res) : "xm" (d));
# endif
  return res;
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
                res.sf[i] = regs->v[rn].sf[i]<0?-regs->v[rn].sf[i]:regs->v[rn].sf[i];
            break;
        case 1: case 3:
            for(i = 0; i < (is_scalar?1:(q?2:1)); i++)
                res.df[i] = regs->v[rn].df[i]<0?-regs->v[rn].df[i]:regs->v[rn].df[i];
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
