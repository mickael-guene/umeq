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
