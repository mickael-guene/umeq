#include <stdio.h>
#include <assert.h>
#include <math.h>

#include "arm64_private.h"
#include "arm64_helpers.h"
#include "runtime.h"
#include "softfloat.h"

/* FIXME: rounding */
/* FIXME: support signaling */

#define INSN(msb, lsb) ((insn >> (lsb)) & ((1 << ((msb) - (lsb) + 1))-1))

#include "arm64_helpers_simd_fpu_common.c"

/* slolen from glibc */
int feclearexcept (int excepts)
{
  fenv_t temp;

  /* Mask out unsupported bits/exceptions.  */
  excepts &= FE_ALL_EXCEPT;

  /* Bah, we have to clear selected exceptions.  Since there is no
     `fldsw' instruction we have to do it the hard way.  */
  __asm__ ("fnstenv %0" : "=m" (*&temp));

  /* Clear the relevant bits.  */
  temp.__status_word &= excepts ^ FE_ALL_EXCEPT;

  /* Put the new data in effect.  */
  __asm__ ("fldenv %0" : : "m" (*&temp));

  /* If the CPU supports SSE, we clear the MXCSR as well.  */
  if (1)
    {
      unsigned int xnew_exc;

      /* Get the current MXCSR.  */
      __asm__ ("stmxcsr %0" : "=m" (*&xnew_exc));

      /* Clear the relevant bits.  */
      xnew_exc &= ~excepts;

      /* Put the new data in effect.  */
      __asm__ ("ldmxcsr %0" : : "m" (*&xnew_exc));
    }

  /* Success.  */
  return 0;
}

/* slolen from glibc */
int fetestexcept (int excepts)
{
  int temp;
  unsigned int mxscr;

  /* Get current exceptions.  */
  __asm__ ("fnstsw %0\n"
       "stmxcsr %1" : "=m" (*&temp), "=m" (*&mxscr));

  return (temp | mxscr) & excepts & FE_ALL_EXCEPT;
}

/* slolen from glibc */
int fesetround (int round)
{
  unsigned short int cw;
  int mxcsr;

  if ((round & ~0xc00) != 0)
    /* ROUND is no valid rounding mode.  */
    return 1;

  /* First set the x87 FPU.  */
  asm ("fnstcw %0" : "=m" (*&cw));
  cw &= ~0xc00;
  cw |= round;
  asm ("fldcw %0" : : "m" (*&cw));

  /* And now the MSCSR register for SSE, the precision is at different bit
     positions in the different units, we need to shift it 3 bits.  */
  asm ("stmxcsr %0" : "=m" (*&mxcsr));
  mxcsr &= ~ 0x6000;
  mxcsr |= round << 3;
  asm ("ldmxcsr %0" : : "m" (*&mxcsr));

  return 0;
}

/* slolen from glibc */
int fegetround (void)
{
  int cw;
  /* We only check the x87 FPU unit.  The SSE unit should be the same
     - and if it's not the same there's no way to signal it.  */

  __asm__ ("fnstcw %0" : "=m" (*&cw));

  return cw & 0xc00;
}

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
        feclearexcept(FE_ALL_EXCEPT);
        res.sf[0] = regs->v[rn].df[0];
        set_arm64_exception(_regs, fetestexcept(FE_ALL_EXCEPT));
    } else if (type == 0 && opc == 1) {
        //fcvt Dd, Sn
        feclearexcept(FE_ALL_EXCEPT);
        res.df[0] = regs->v[rn].sf[0];
        set_arm64_exception(_regs, fetestexcept(FE_ALL_EXCEPT));
    } else if (type == 0 && opc ==3) {
        //fcvt Hd, Sn
        float_status dummy = {0};
        float16 r = float32_to_float16(regs->v[rn].s[0], 1, &dummy);
        res.h[0] = float16_val(r);
    } else if (type == 1 && opc ==3) {
        //fcvt Hd, Dn
        float_status dummy = {0};
        float16 r = float64_to_float16(regs->v[rn].d[0], 1, &dummy);
        res.h[0] = float16_val(r);
    } else if (type == 3 && opc == 0) {
        //fcvt Sd, Hn
        float_status dummy = {0};
        float32 r = float16_to_float32(regs->v[rn].h[0], 1, &dummy);
        res.s[0] = float32_val(r);
    }else if (type == 3 && opc == 1) {
        //fcvt Dd, Hn
        float_status dummy = {0};
        float64 r = float16_to_float64(regs->v[rn].h[0], 1, &dummy);
        res.d[0] = float64_val(r);
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

    if (is_double) {
        res.df[0] = fcvt_rm(regs->v[rn].df[0], rmode);
        if (res.df[0] == 0.0 && regs->v[rn].df[0] < 0)
            res.d[0] = 0x8000000000000000UL;
    } else {
        res.sf[0] = fcvt_rm(regs->v[rn].sf[0], rmode);
        /* handle -0 case */
        if (res.sf[0] == 0.0 && regs->v[rn].sf[0] < 0)
            res.s[0] = 0x80000000;
    }
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
    enum rm rm = (regs->fpcr >> 22) & 3;

    dis_frint(_regs, insn, rm);
}

static void dis_frinti(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    enum rm rm = (regs->fpcr >> 22) & 3;

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

    if (arm64_hlp_compute_flags_pred(_regs, cond, regs->nzcv)) {
        if (is_double) {
            double op1 = regs->v[rn].df[0];
            double op2 = regs->v[rm].df[0];

            if (op1 == op2)
                nzcv = 0x6;
            else if (op1 < op2)
                nzcv = 0x8;
            else
                nzcv = 0x2;
        } else {
            float op1 = regs->v[rn].sf[0];
            float op2 = regs->v[rm].sf[0];

            if (op1 == op2)
                nzcv = 0x6;
            else if (op1 < op2)
                nzcv = 0x8;
            else
                nzcv = 0x2;
        }
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
    if (is_double) {
        for(i = 0; i < (is_scalar?1:2); i++)
            if (regs->v[rn].d[i]&0x8000000000000000UL) {
                set_arm64_exception(_regs, FE_INVALID);
                res.df[i] = NAN;
            }
            else
                res.df[i] = sqrt(regs->v[rn].df[i]);
    } else {
        for(i = 0; i < (is_scalar?1:(q?4:2)); i++)
            if (regs->v[rn].s[i]&0x80000000) {
                set_arm64_exception(_regs, FE_INVALID);
                res.sf[i] = NAN;
            }
            else
                res.sf[i] = sqrtf(regs->v[rn].sf[i]);
    }
    regs->v[rd] = res;
}

/* deasm */
void arm64_hlp_dirty_scvtf_scalar_integer_simd(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int sf_type0 = (INSN(31,31) << 1) | INSN(22,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);

    switch(sf_type0) {
        case 0:
            regs->v[rd].d[1] = 0;
            regs->v[rd].sf[0] = (float)(int32_t) regs->r[rn];
            regs->v[rd].sf[1] = 0;
            break;
        case 1:
            regs->v[rd].d[1] = 0;
            regs->v[rd].df[0] = (double)(int32_t) regs->r[rn];
            break;
        case 2:
            regs->v[rd].d[1] = 0;
            regs->v[rd].sf[0] = (float)(int64_t) regs->r[rn];
            regs->v[rd].sf[1] = 0;
            break;
        case 3:
            regs->v[rd].d[1] = 0;
            regs->v[rd].df[0] = (double)(int64_t) regs->r[rn];
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
    //int is_signaling = INSN(4,4);
    int rm = INSN(20,16);
    int rn = INSN(9,5);

    if (is_double) {
        double op1 = regs->v[rn].df[0];
        double op2 = is_compare_zero?0.0:regs->v[rm].df[0];

        if (isnan(op1) || isnan(op2))
            regs->nzcv = 0x30000000;
        else if (op1 == op2)
            regs->nzcv = 0x60000000;
        else if (op1 > op2)
            regs->nzcv = 0x20000000;
        else
            regs->nzcv = 0x80000000;
    } else {
        float op1 = regs->v[rn].sf[0];
        float op2 = is_compare_zero?0.0:regs->v[rm].sf[0];

        if (isnan(op1) || isnan(op2))
            regs->nzcv = 0x30000000;
        else if (op1 == op2)
            regs->nzcv = 0x60000000;
        else if (op1 > op2)
            regs->nzcv = 0x20000000;
        else
            regs->nzcv = 0x80000000;
    }
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
    int original_rounding_mode = fegetround();

    feclearexcept(FE_ALL_EXCEPT);
    set_host_rounding_mode_from_arm64(_regs);
    if (is_double) {
        switch(opcode) {
            case 0://fmul
                res.df[0] = regs->v[rn].df[0] * regs->v[rm].df[0];
                break;
            case 1://fdiv
                res.df[0] = regs->v[rn].df[0] / regs->v[rm].df[0];
                break;
            case 2://fadd
                res.df[0] = regs->v[rn].df[0] + regs->v[rm].df[0];
                break;
            case 3://fsub
                res.df[0] = regs->v[rn].df[0] - regs->v[rm].df[0];
                break;
            case 4://fmax
                res.df[0] = maxd(regs->v[rn].df[0],regs->v[rm].df[0]);
                break;
            case 6://fmaxnm
                res.df[0] = maxnmd(regs->v[rn].df[0],regs->v[rm].df[0]);
                break;
            case 5://fmin
                res.df[0] = mind(regs->v[rn].df[0],regs->v[rm].df[0]);
                break;
            case 7://fminnm
                res.df[0] = minnmd(regs->v[rn].df[0],regs->v[rm].df[0]);
                break;
            case 8://fnmul
                res.df[0] = -(regs->v[rn].df[0] * regs->v[rm].df[0]);
                break;
            default:
                fatal("opcode = %d(0x%x)\n", opcode, opcode);
        }
    } else {
        switch(opcode) {
            case 0://fmul
                res.sf[0] = regs->v[rn].sf[0] * regs->v[rm].sf[0];
                break;
            case 1://fdiv
                res.sf[0] = regs->v[rn].sf[0] / regs->v[rm].sf[0];
                break;
            case 2://fadd
                res.sf[0] = regs->v[rn].sf[0] + regs->v[rm].sf[0];
                break;
            case 3://fsub
                res.sf[0] = regs->v[rn].sf[0] - regs->v[rm].sf[0];
                break;
            case 4://fmax
                res.sf[0] = maxf(regs->v[rn].sf[0],regs->v[rm].sf[0]);
                break;
            case 6://fmaxnm
                res.sf[0] = maxnmf(regs->v[rn].sf[0],regs->v[rm].sf[0]);
                break;
            case 5://fmin
                res.sf[0] = minf(regs->v[rn].sf[0],regs->v[rm].sf[0]);
                break;
            case 7://fminnm
                res.sf[0] = minnmf(regs->v[rn].sf[0],regs->v[rm].sf[0]);
                break;
            case 8://fnmul
                res.sf[0] = -(regs->v[rn].sf[0] * regs->v[rm].sf[0]);
                break;
            default:
                fatal("opcode = %d(0x%x)\n", opcode, opcode);
        }
    }
    regs->v[rd] = res;
    fesetround(original_rounding_mode);
    set_arm64_exception(_regs, fetestexcept(FE_ALL_EXCEPT));
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
                res.df[0] = regs->v[ra].df[0] + regs->v[rn].df[0] * regs->v[rm].df[0];
                break;
            case 1://fmsub
                res.df[0] = regs->v[ra].df[0] - regs->v[rn].df[0] * regs->v[rm].df[0];
                break;
            case 2://fnmadd
                res.df[0] = -regs->v[ra].df[0] - regs->v[rn].df[0] * regs->v[rm].df[0];
                break;
            case 3://fnmsub
                res.df[0] = -regs->v[ra].df[0] + regs->v[rn].df[0] * regs->v[rm].df[0];
                break;
            default:
                fatal("o1_o0 = %d(0x%x)\n", o1_o0, o1_o0);
        }
    } else {
        switch(o1_o0) {
            case 0://fmadd
                res.sf[0] = (double)regs->v[ra].sf[0] + (double)regs->v[rn].sf[0] * (double)regs->v[rm].sf[0];
                break;
            case 1://fmsub
                res.sf[0] = (double)regs->v[ra].sf[0] - (double)regs->v[rn].sf[0] * (double)regs->v[rm].sf[0];
                break;
            case 2://fnmadd
                res.sf[0] = -(double)regs->v[ra].sf[0] - (double)regs->v[rn].sf[0] * (double)regs->v[rm].sf[0];
                break;
            case 3://fnmsub
                res.sf[0] = -(double)regs->v[ra].sf[0] + (double)regs->v[rn].sf[0] * (double)regs->v[rm].sf[0];
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
            regs->v[rd].sf[0] = (float)(uint32_t) regs->r[rn];
            regs->v[rd].sf[1] = 0;
            break;
        case 1:
            regs->v[rd].df[0] = (double)(uint32_t) regs->r[rn];
            break;
        case 2:
            regs->v[rd].sf[0] = (float) regs->r[rn];
            regs->v[rd].sf[1] = 0;
            break;
        case 3:
            regs->v[rd].df[0] = (double) regs->r[rn];
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

    regs->v[rd].d[1] = 0;
    switch(sf_type0) {
        case 0:
            regs->r[rd] = (uint32_t) usat32_d(fcvt_rm(regs->v[rn].sf[0], rmode));
            break;
        case 1:
            regs->r[rd] = (uint32_t) usat32_d(fcvt_rm(regs->v[rn].df[0], rmode));
            break;
        case 2:
            regs->r[rd] = (uint64_t) usat64_d(fcvt_rm(regs->v[rn].sf[0], rmode));
            break;
        case 3:
            regs->r[rd] = (uint64_t) usat64_d(fcvt_rm(regs->v[rn].df[0], rmode));
            break;
        default:
            fatal("sf_type0 = %d\n", sf_type0);
    }
}
static void dis_fcvts_scalar_integer(uint64_t _regs, uint32_t insn, enum rm rmode)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int sf_type0 = (INSN(31,31) << 1) | INSN(22,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);

    regs->v[rd].d[1] = 0;
    switch(sf_type0) {
        case 0:
            regs->r[rd] = (uint32_t)(int32_t) ssat32_d(fcvt_rm(regs->v[rn].sf[0], rmode));
            break;
        case 1:
            regs->r[rd] = (uint32_t)(int32_t) ssat32_d(fcvt_rm(regs->v[rn].df[0], rmode));
            break;
        case 2:
            regs->r[rd] = (int64_t) ssat64_d(fcvt_rm(regs->v[rn].sf[0], rmode));
            break;
        case 3:
            regs->r[rd] = (int64_t) ssat64_d(fcvt_rm(regs->v[rn].df[0], rmode));
            break;
        default:
            fatal("sf_type0 = %d\n", sf_type0);
    }
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
            regs->r[rd] = (uint32_t)(int32_t) ssat32_d(fcvt_fract(regs->v[rn].sf[0], fracbits));
            break;
        case 1:
            //Wd, Dn
            regs->r[rd] = (uint32_t)(int32_t) ssat32_d(fcvt_fract(regs->v[rn].df[0], fracbits));
            break;
        case 2:
            //Xd, Sn
            regs->r[rd] = (int64_t) ssat64_d(fcvt_fract(regs->v[rn].sf[0], fracbits));
            break;
        case 3:
            //Xd, Dn
            regs->r[rd] = (int64_t) ssat64_d(fcvt_fract(regs->v[rn].df[0], fracbits));
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
            regs->r[rd] = (uint32_t) usat32_d(fcvt_fract(regs->v[rn].sf[0], fracbits));
            break;
        case 1:
            //Wd, Dn
            regs->r[rd] = (uint32_t) usat32_d(fcvt_fract(regs->v[rn].df[0], fracbits));
            break;
        case 2:
            //Xd, Sn
            regs->r[rd] = (uint64_t) usat64_d(fcvt_fract(regs->v[rn].sf[0], fracbits));
            break;
        case 3:
            //Xd, Dn
            regs->r[rd] = (uint64_t) usat64_d(fcvt_fract(regs->v[rn].df[0], fracbits));
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
            res.sf[0] = (double)(int32_t)regs->r[rn] / (1UL << fracbits);
            break;
        case 1:
            //Dd, Wn
            res.df[0] = (double)(int32_t)regs->r[rn] / (1UL << fracbits);
            break;
        case 2:
            //Sd, Xn
            res.sf[0] = (double)(int64_t)regs->r[rn] / (1UL << fracbits);
            break;
        case 3:
            //Dd, Xn
            res.df[0] = (double)(int64_t)regs->r[rn] / (1UL << fracbits);
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
            res.sf[0] = (double)(uint32_t)regs->r[rn] / (1UL << fracbits);
            break;
        case 1:
            //Dd, Wn
            res.df[0] = (double)(uint32_t)regs->r[rn] / (1UL << fracbits);
            break;
        case 2:
            //Sd, Xn
            res.sf[0] = (double)(uint64_t)regs->r[rn] / (1UL << fracbits);
            break;
        case 3:
            //Dd, Xn
            res.df[0] = (double)(uint64_t)regs->r[rn] / (1UL << fracbits);
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
