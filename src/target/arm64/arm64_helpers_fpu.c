#include <stdio.h>
#include <assert.h>

#include "arm64_private.h"
#include "arm64_helpers.h"
#include "runtime.h"

/* FIXME: rounding */
/* FIXME: support signaling */

#define INSN(msb, lsb) ((insn >> (lsb)) & ((1 << ((msb) - (lsb) + 1))-1))

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
        res.sf[0] = regs->v[rn].df[0];
    } else if (type == 0 && opc == 1) {
        //fcvt Dd, Sn
        res.df[0] = regs->v[rn].sf[0];
    } else
        fatal("type=%d / opc=%d\n", type, opc);
    regs->v[rd] = res;
}

/* FIXME: certainly not exact ..... */
static void dis_frintm(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int is_double = INSN(22,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    union simd_register res = {0};

    if (is_double) {
        res.df[0] = (double)(int64_t)regs->v[rn].df[0];
    } else {
        res.sf[0] = (float)(int64_t)regs->v[rn].sf[0];
    }
    regs->v[rd] = res;
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

        if (op1 == op2)
            regs->nzcv = 0x60000000;
        else if (op1 > op2)
            regs->nzcv = 0x20000000;
        else
            regs->nzcv = 0x80000000;
    } else {
        float op1 = regs->v[rn].sf[0];
        float op2 = is_compare_zero?0.0:regs->v[rm].sf[0];

        if (op1 == op2)
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

    regs->v[rd].d[1] = 0;
    if (is_double) {
        switch(opcode) {
            case 0://fmul
                regs->v[rd].df[0] = regs->v[rn].df[0] * regs->v[rm].df[0];
                break;
            case 1://fdiv
                regs->v[rd].df[0] = regs->v[rn].df[0] / regs->v[rm].df[0];
                break;
            case 2://fadd
                regs->v[rd].df[0] = regs->v[rn].df[0] + regs->v[rm].df[0];
                break;
            case 3://fsub
                regs->v[rd].df[0] = regs->v[rn].df[0] - regs->v[rm].df[0];
                break;
            case 4://fmax
            case 6://fmaxnm
                regs->v[rd].df[0] = maxd(regs->v[rn].df[0],regs->v[rm].df[0]);
                break;
            case 5://fmin
            case 7://fminnm
                regs->v[rd].df[0] = mind(regs->v[rn].df[0],regs->v[rm].df[0]);
                break;
            case 8://fnmul
                regs->v[rd].df[0] = -(regs->v[rn].df[0] * regs->v[rm].df[0]);
                break;
            default:
                fatal("opcode = %d(0x%x)\n", opcode, opcode);
        }
    } else {
        switch(opcode) {
            case 0://fmul
                regs->v[rd].sf[0] = regs->v[rn].sf[0] * regs->v[rm].sf[0];
                break;
            case 1://fdiv
                regs->v[rd].sf[0] = regs->v[rn].sf[0] / regs->v[rm].sf[0];
                break;
            case 2://fadd
                regs->v[rd].sf[0] = regs->v[rn].sf[0] + regs->v[rm].sf[0];
                break;
            case 3://fsub
                regs->v[rd].sf[0] = regs->v[rn].sf[0] - regs->v[rm].sf[0];
                break;
            case 4://fmax
            case 6://fmaxnm
                regs->v[rd].sf[0] = maxf(regs->v[rn].sf[0],regs->v[rm].sf[0]);
                break;
            case 5://fmin
            case 7://fminnm
                regs->v[rd].sf[0] = minf(regs->v[rn].sf[0],regs->v[rm].sf[0]);
                break;
            case 8://fnmul
                regs->v[rd].sf[0] = -(regs->v[rn].sf[0] * regs->v[rm].sf[0]);
                break;
            default:
                fatal("opcode = %d(0x%x)\n", opcode, opcode);
        }
        regs->v[rd].sf[1] = 0;
    }
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

void arm64_hlp_dirty_fcvtzs_scalar_integer_simd(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int sf_type0 = (INSN(31,31) << 1) | INSN(22,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);

    switch(sf_type0) {
        case 1:
            regs->r[rd] = (int32_t) regs->v[rn].df[0];
            break;
        case 3:
            regs->r[rd] = (int64_t) regs->v[rn].df[0];
            break;
        default:
            fatal("sf_type0 = %d\n", sf_type0);
    }
}

void arm64_hlp_dirty_fcvtzu_scalar_integer_simd(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int sf_type0 = (INSN(31,31) << 1) | INSN(22,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);

    switch(sf_type0) {
        case 1:
            regs->r[rd] = (uint32_t) regs->v[rn].df[0];
            break;
        case 2:
            regs->r[rd] = (uint64_t) regs->v[rn].sf[0];
            break;
        case 3:
            regs->r[rd] = (uint64_t) regs->v[rn].df[0];
            break;
        default:
            fatal("sf_type0 = %d\n", sf_type0);
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
        case 4: case 5:
            dis_fcvt(_regs, insn);
            break;
        case 10:
            dis_frintm(_regs, insn);
            break;
        default:
            fatal("opcode = %d/0x%x\n", opcode, opcode);
    }
}

void arm64_hlp_dirty_floating_point_conditional_compare(uint64_t _regs, uint32_t insn)
{
    dis_fccmp_fccmpe(_regs, insn);
}
