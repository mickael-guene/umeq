#include <stdio.h>
#include <assert.h>

#include "arm64_private.h"
#include "arm64_helpers.h"
#include "runtime.h"

//#define DUMP_STACK 1
#define INSN(msb, lsb) ((insn >> (lsb)) & ((1 << ((msb) - (lsb) + 1))-1))

void arm64_hlp_dump_simd(uint64_t regs)
{
    struct arm64_target *context = container_of((void *) regs, struct arm64_target, regs);
    int i;

    printf("==============================================================================\n\n");
    for(i = 0; i < 32; i++) {
        printf("r[%2d] = 0x%016lx  ", i, context->regs.r[i]);
        if (i % 4 == 3)
            printf("\n");
    }
    for(i = 0; i < 32; i++) {
        printf("v[%2d] = 0x%016lx%016lx  ", i, context->regs.v[i].v.msb, context->regs.v[i].v.lsb);
        if (i % 2 == 1)
            printf("\n");
    }
    printf("pc    = 0x%016lx\n", context->regs.pc);
}

void arm64_hlp_dirty_simd_dup_general(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int imm5 = INSN(20,16);
    int rn = INSN(9,5);
    int rd = INSN(4,0);
    uint64_t rn_r = regs->r[rn];
    uint64_t lsb = 0;
    uint64_t msb = 0;

    if (imm5 & 1) {
        rn_r = rn_r & 0xff;
        lsb = (rn_r << 56) | (rn_r << 48) | (rn_r << 40) | (rn_r << 32) | (rn_r << 24) | (rn_r << 16) | (rn_r << 8) | (rn_r << 0);
    } else if (imm5 & 2) {
        rn_r = rn_r & 0xffff;
        lsb = (rn_r << 48) | (rn_r << 32) | (rn_r << 16) | (rn_r << 0);
    } else if (imm5 & 4) {
        rn_r = rn_r & 0xffffffff;
        lsb = (rn_r << 32) | (rn_r << 0);
    } else {
        assert(q == 1);
        lsb = rn_r;
    }
    if (q)
        msb = lsb;

    regs->v[rd].v.lsb = lsb;
    regs->v[rd].v.msb = msb;
}

void arm64_hlp_dirty_shl_simd(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int is_scalar = INSN(28,28);
    int imm = INSN(22,16);
    int rd = INSN(4,0);
    int rn = INSN(9,5);

    if (is_scalar) {
        int shift = imm - 64;

        regs->v[rd].d[1] = 0;
        regs->v[rd].d[0] = regs->v[rn].d[0] << shift;
    } else {
        assert(0);
    }
}

/* FIXME: rounding */
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
        case 3:
            regs->v[rd].d[1] = 0;
            regs->v[rd].df[0] = (double)(int64_t) regs->r[rn];
            break;
        default:
            fatal("sf_type0 = %d\n", sf_type0);
    }
}

void arm64_hlp_dirty_ucvtf_scalar_integer_simd(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int sf_type0 = (INSN(31,31) << 1) | INSN(22,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);

    switch(sf_type0) {
        case 2:
            regs->v[rd].d[1] = 0;
            regs->v[rd].sf[0] = (float) regs->r[rn];
            regs->v[rd].sf[1] = 0;
            break;
        default:
            fatal("sf_type0 = %d\n", sf_type0);
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
        default:
            fatal("sf_type0 = %d\n", sf_type0);
    }
}

/* FIXME: support signaling */
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
            default:
                fatal("opcode = %d(0x%x)\n", opcode, opcode);
        }
        regs->v[rd].sf[1] = 0;
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

    if (is_double) {
        regs->v[rd].d[1] = 0;
        switch(o1_o0) {
            case 0://fmadd
                regs->v[rd].df[0] = regs->v[ra].df[0] + regs->v[rn].df[0] * regs->v[rm].df[0];
                break;
            default:
                fatal("o1_o0 = %d(0x%x)\n", o1_o0, o1_o0);
        }
    } else {
        assert(0);
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

static void dis_ushr_vector(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int immh = INSN(22,19);
    int imm = INSN(22,16);
    int i;
    int shift;

    if (immh == 1) {
        //8B/16B
        shift = 16 - imm;
        assert(0);
    } else if (immh < 4) {
        //4H/8H
        shift = 32 - imm;
         assert(0);
    } else if (immh < 8) {
        //2S/4S
        shift = 64 - imm;
        for(i = 0; i < (q?4:2); i++) {
            regs->v[rd].s[i] = regs->v[rn].s[i] >> shift;
        }
        if (!q)
            regs->v[rd].d[1] = 0;
    } else {
        //2D/1D
        shift = 128 - imm;
         assert(0);
    }
}

void arm64_hlp_dirty_advanced_simd_shift_by_immediate_simd(uint64_t _regs, uint32_t insn)
{
    int opcode = INSN(15,11);
    int U = INSN(29,29);

    switch(opcode) {
        case 0:
            if (U)
                dis_ushr_vector(_regs, insn);
            break;
        default:
            fatal("opcode = %d(0x%x) / U=%d\n", opcode, opcode, U);
    }
}

static void dis_abs(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int is_scalar = INSN(28,28);
    int q = INSN(30,30);
    int size = INSN(23,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int i;

    switch(size) {
        case 0:
            for(i = 0; i < (q?16:8); i++)
                regs->v[rd].b[i] = (int8_t)regs->v[rn].b[i]<0?-regs->v[rn].b[i]:regs->v[rn].b[i];
            break;
        case 1:
            for(i = 0; i < (q?8:4); i++)
                regs->v[rd].h[i] = (int16_t)regs->v[rn].h[i]<0?-regs->v[rn].h[i]:regs->v[rn].h[i];
            break;
        case 2:
            for(i = 0; i < (q?4:2); i++) {
                regs->v[rd].s[i] = (int32_t)regs->v[rn].s[i]<0?-regs->v[rn].s[i]:regs->v[rn].s[i];
            }
            break;
        case 3:
            for(i = 0; i < (q && !is_scalar?2:1); i++)
                regs->v[rd].d[i] = (int64_t)regs->v[rn].d[i]<0?-regs->v[rn].d[i]:regs->v[rn].d[i];
            break;
    }
    if (!q || is_scalar)
        regs->v[rd].v.msb = 0;
}

static void dis_cmpeq_vector(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int size = INSN(23,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int i;

    regs->v[rd].v.msb = 0;
    switch(size) {
        case 0:
            for(i = 0; i < (q?16:8); i++)
                regs->v[rd].b[i] = regs->v[rn].b[i]==0?0xff:0;
            break;
        case 1:
            for(i = 0; i < (q?8:4); i++)
                regs->v[rd].h[i] = regs->v[rn].h[i]==0?0xffff:0;
            break;
        case 2:
            for(i = 0; i < (q?4:2); i++) {
                regs->v[rd].s[i] = regs->v[rn].s[i]==0?0xffffffff:0;
            }
            break;
        case 3:
            for(i = 0; i < (q?2:1); i++)
                regs->v[rd].d[i] = regs->v[rn].d[i]==0?0xffffffffffffffffUL:0;
            break;
    }
}

static void dis_add(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int is_scalar = INSN(28,28);
    int q = INSN(30,30);
    int size = INSN(23,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int rm = INSN(20,16);
    int i;

    switch(size) {
        case 0:
            for(i = 0; i < (q?16:8); i++)
                regs->v[rd].b[i] = regs->v[rn].b[i] + regs->v[rm].b[i];
            break;
        case 1:
            for(i = 0; i < (q?8:4); i++)
                regs->v[rd].h[i] = regs->v[rn].h[i] + regs->v[rm].h[i];
            break;
        case 2:
            for(i = 0; i < (q?4:2); i++) {
                regs->v[rd].s[i] = regs->v[rn].s[i] + regs->v[rm].s[i];
            }
            break;
        case 3:
            for(i = 0; i < (q && !is_scalar?2:1); i++)
                regs->v[rd].d[i] = regs->v[rn].d[i] + regs->v[rm].d[i];
            break;
    }
    if (!q || is_scalar)
        regs->v[rd].v.msb = 0;
}

static void dis_orr_register(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int rm = INSN(20,16);

    regs->v[rd].v.lsb = regs->v[rn].v.lsb | regs->v[rm].v.lsb;
    regs->v[rd].v.msb = q?regs->v[rn].v.msb | regs->v[rm].v.msb:0;
}

static void dis_and(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int rm = INSN(20,16);

    regs->v[rd].v.lsb = regs->v[rn].v.lsb & regs->v[rm].v.lsb;
    regs->v[rd].v.msb = q?regs->v[rn].v.msb & regs->v[rm].v.msb:0;
}

static void dis_bic_register(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int rm = INSN(20,16);

    regs->v[rd].v.lsb = regs->v[rn].v.lsb & ~regs->v[rm].v.lsb;
    regs->v[rd].v.msb = q?regs->v[rn].v.msb & ~regs->v[rm].v.msb:0;
}

static void dis_addp(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int is_scalar = INSN(28,28);
    int q = INSN(30,30);
    int size = INSN(23,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int rm = INSN(20,16);
    int i;
    union simd_register res;

    res.v.msb = 0;
    switch(size) {
        case 0:
            for(i = 0; i < (q?8:4); i++) {
                res.b[i] = regs->v[rn].b[2*i] + regs->v[rn].b[2*i + 1];
                res.b[(q?8:4) + i] = regs->v[rm].b[2*i] + regs->v[rm].b[2*i + 1];
            }
            break;
        case 1:
            for(i = 0; i < (q?4:2); i++) {
                res.h[i] = regs->v[rn].h[2*i] + regs->v[rn].h[2*i + 1];
                res.h[(q?4:2) + i] = regs->v[rm].h[2*i] + regs->v[rm].h[2*i + 1];
            }
            break;
        case 2:
            for(i = 0; i < (q?2:1); i++) {
                res.s[i] = regs->v[rn].s[2*i] + regs->v[rn].s[2*i + 1];
                res.s[(q?2:1) + i] = regs->v[rm].s[2*i] + regs->v[rm].s[2*i + 1];
            }
            break;
        case 3:
            assert(q == 1);
            for(i = 0; i < 1; i++) {
                res.d[i] = regs->v[rn].d[2*i] + regs->v[rn].d[2*i + 1];
                if (!is_scalar)
                    res.d[i+1] = regs->v[rm].d[2*i] + regs->v[rm].d[2*i + 1];
            }
            break;
    }

    regs->v[rd] = res;
}

static void dis_addhn(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int size = INSN(23,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int rm = INSN(20,16);
    int i;
    union simd_register res;

    res.v.msb = 0;
    switch(size) {
        case 0:
            for(i = 0; i < 8; i++) {
                res.b[i] = (regs->v[rn].h[i] + regs->v[rm].h[i]) >> 8;
            }
            break;
        case 1:
            for(i = 0; i < 4; i++) {
                res.h[i] = (regs->v[rn].s[i] + regs->v[rm].s[i]) >> 16;
            }
            break;
        case 2:
            for(i = 0; i < 2; i++) {
                res.s[i] = (regs->v[rn].d[i] + regs->v[rm].d[i]) >> 32;
            }
            break;
        case 3:
            assert(0);
            break;
    }
    if (q)
        regs->v[rd].v.msb = res.v.lsb;
    else
        regs->v[rd].v = res.v;
}

static void dis_addv(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int size = INSN(23,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int i;
    union simd_register res;

    res.v.msb = 0;
    res.v.lsb = 0;
    switch(size) {
        case 0:
            for(i = 0; i < (q?16:8); i++)
                res.b[0] += regs->v[rn].b[i];
            break;
        case 1:
            for(i = 0; i < (q?8:4); i++)
                res.h[0] += regs->v[rn].h[i];
            break;
        case 2:
            assert(q);
            for(i = 0; i < 4; i++)
                res.s[0] += regs->v[rn].s[i];
            break;
        default:
            assert(0);
    }
    regs->v[rd].v = res.v;
}

static void dis_bitop(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int rm = INSN(20,16);
    int opc2 = INSN(23,22);
    union simd_register operand1;
    union simd_register operand2;
    union simd_register operand3;
    uint64_t lsb;
    uint64_t msb = 0;

    switch(opc2) {
        case 0:
            operand1 = regs->v[rm];
            operand2.v.lsb = operand2.v.msb = 0;
            operand3.v.lsb = operand3.v.msb = ~0;
            break;
        case 1:
            operand1 = regs->v[rm];
            operand2 = regs->v[rm];
            operand3 = regs->v[rd];
            break;
        case 2:
            operand1 = regs->v[rd];
            operand2 = regs->v[rd];
            operand3 = regs->v[rm];
            break;
        case 3:
            operand1 = regs->v[rd];
            operand2 = regs->v[rd];
            operand3.v.lsb = ~regs->v[rm].v.lsb;
            operand3.v.msb = ~regs->v[rm].v.msb;
            break;
    }

    lsb = operand1.v.lsb ^ ((operand2.v.lsb ^ regs->v[rn].v.lsb) & (operand3.v.lsb));
    if (q)
        msb = operand1.v.msb ^ ((operand2.v.msb ^ regs->v[rn].v.msb) & (operand3.v.msb));
    regs->v[rd].v.lsb = lsb;
    regs->v[rd].v.msb = msb;
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

static void dis_cls_clz(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int is_clz = INSN(29,29);
    int q = INSN(30,30);
    int size = INSN(23,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int i;
    union simd_register res;
    int (*fct)(uint64_t, int) = is_clz?clz:cls;

    res.v.msb = 0;
    res.v.lsb = 0;
    switch(size) {
        case 0:
            for(i = 0; i < (q?16:8); i++) {
                res.b[i] = (*fct)(regs->v[rn].b[i], 7);
            }
            break;
        case 1:
            for(i = 0; i < (q?8:4); i++) {
                res.h[i] = (*fct)(regs->v[rn].h[i], 15);
            }
            break;
        case 2:
            for(i = 0; i < (q?4:2); i++) {
                res.s[i] = (*fct)(regs->v[rn].s[i], 31);
            }
            break;
        case 3:
            assert(0);
            break;
    }
    regs->v[rd].v = res.v;
}

/* macro to implement cmxx family */
#define DIS_CM(_cast_,_op_,_is_zero_) \
do { \
struct arm64_registers *regs = (struct arm64_registers *) _regs; \
int q = INSN(30,30); \
int is_scalar = INSN(28,28); \
int size = INSN(23,22); \
int rd = INSN(4,0); \
int rn = INSN(9,5); \
int rm = INSN(20,16); \
union simd_register res; \
int i; \
 \
res.v.msb = 0; \
res.v.msb = 0; \
switch(size) { \
    case 0: \
        for(i = 0; i < (q?16:8); i++) \
            res.b[i] = (_cast_##8_t)regs->v[rn].b[i] _op_ (_is_zero_?0:(_cast_##8_t)regs->v[rm].b[i])?0xff:0; \
        break; \
    case 1: \
        for(i = 0; i < (q?8:4); i++) \
            res.h[i] = (_cast_##16_t)regs->v[rn].h[i] _op_ (_is_zero_?0:(_cast_##16_t)regs->v[rm].h[i])?0xffff:0; \
        break; \
    case 2: \
        for(i = 0; i < (q?4:2); i++) \
            res.s[i] = (_cast_##32_t)regs->v[rn].s[i] _op_ (_is_zero_?0:(_cast_##32_t)regs->v[rm].s[i])?0xffffffff:0; \
        break; \
    case 3: \
        for(i = 0; i < ((q && !is_scalar)?2:1); i++) \
            res.d[i] = (_cast_##64_t)regs->v[rn].d[i] _op_ (_is_zero_?0:(_cast_##64_t)regs->v[rm].d[i])?0xffffffffffffffffUL:0; \
        break; \
} \
regs->v[rd] = res; \
 \
} while(0)

static void dis_cmeq(uint64_t _regs, uint32_t insn)
{
DIS_CM(int,==,0);
}

static void dis_cmeq_zero(uint64_t _regs, uint32_t insn)
{
DIS_CM(int,==,1);
}

static void dis_cmge(uint64_t _regs, uint32_t insn)
{
DIS_CM(int,>=,0);
}

static void dis_cmge_zero(uint64_t _regs, uint32_t insn)
{
DIS_CM(int,>=,1);
}

static void dis_cmgt(uint64_t _regs, uint32_t insn)
{
DIS_CM(int,>,0);
}

static void dis_cmgt_zero(uint64_t _regs, uint32_t insn)
{
DIS_CM(int,>,1);
}

static void dis_cmhi(uint64_t _regs, uint32_t insn)
{
DIS_CM(uint,>,0);
}

static void dis_cmhs(uint64_t _regs, uint32_t insn)
{
DIS_CM(uint,>=,0);
}

static void dis_cmle_zero(uint64_t _regs, uint32_t insn)
{
DIS_CM(int,<=,1);
}

static void dis_cmlt_zero(uint64_t _regs, uint32_t insn)
{
DIS_CM(int,<,1);
}

static void dis_cmtst(uint64_t _regs, uint32_t insn)
{
DIS_CM(int,&,0);
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

static void dis_cnt(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int size = INSN(23,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int i;
    union simd_register res;

    res.v.msb = 0;
    res.v.lsb = 0;
    switch(size) {
        case 0:
            for(i = 0; i < (q?16:8); i++)
                res.b[i] = cnt(regs->v[rn].b[i], 8);
            break;
        default:
            break;
    }
    regs->v[rd].v = res.v;
}

void arm64_hlp_dirty_advanced_simd_ext_simd(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int rm = INSN(20,16);
    int index = INSN(14,11);
    int i;
    int pos = 0;
    union simd_register res;

    res.v.msb = 0;
    res.v.lsb = 0;
    for(i = index; i < (q?16:8); i++) {
        res.b[pos++] = regs->v[rn].b[i];
    }
    for(i = 0; i < index; i++) {
        res.b[pos++] = regs->v[rm].b[i];
    }
    regs->v[rd].v = res.v;
}

/* simd deasm */
void arm64_hlp_dirty_advanced_simd_two_reg_misc_simd(uint64_t _regs, uint32_t insn)
{
    int U = INSN(29,29);
    int opcode = INSN(16,12);

    switch(opcode) {
        case 4:
            dis_cls_clz(_regs, insn);
            break;
        case 5:
            if (U)
                assert(0);
            else
                dis_cnt(_regs, insn);
            break;
        case 8:
            if (U)
                dis_cmge_zero(_regs, insn);
            else
                dis_cmgt_zero(_regs, insn);
            break;
        case 9:
            if (U)
                dis_cmle_zero(_regs, insn);
            else
                dis_cmpeq_vector(_regs, insn);
            break;
        case 10:
            if (U)
                assert(0);
            else
                dis_cmlt_zero(_regs, insn);
            break;
        case 11:
            if (U)
                assert(0);
            else
                dis_abs(_regs, insn);
            break;
        default:
            fatal("opcode = %d(0x%x) / U=%d\n", opcode, opcode, U);
    }
}

void arm64_hlp_dirty_advanced_simd_scalar_two_reg_misc_simd(uint64_t _regs, uint32_t insn)
{
    int U = INSN(29,29);
    int opcode = INSN(16,12);

    switch(opcode) {
        case 8:
            if (U)
                dis_cmge_zero(_regs, insn);
            else
                dis_cmgt_zero(_regs, insn);
            break;
        case 9:
            if (U)
                dis_cmle_zero(_regs, insn);
            else
                dis_cmeq_zero(_regs, insn);
            break;
        case 10:
            if (U)
                assert(0);
            else
                dis_cmlt_zero(_regs, insn);
            break;
        case 11:
            if (U)
                assert(0);
            else
                dis_abs(_regs, insn);
            break;
        default:
            fatal("opcode = %d(0x%x) / U=%d\n", opcode, opcode, U);
    }
}

void arm64_hlp_dirty_advanced_simd_scalar_three_same_simd(uint64_t _regs, uint32_t insn)
{
    int U = INSN(29,29);
    int opcode = INSN(15,11);

    switch(opcode) {
        case 6:
            if (U)
                dis_cmhi(_regs, insn);
            else
                dis_cmgt(_regs, insn);
            break;
        case 7:
            if (U)
                dis_cmhs(_regs, insn);
            else
                dis_cmge(_regs, insn);
            break;
        case 16:
            if (U)
                assert(0);
            else
                dis_add(_regs, insn);
            break;
        case 17:
            if (U)
                dis_cmeq(_regs, insn);
            else
                dis_cmtst(_regs, insn);
            break;
        default:
            fatal("opcode = %d(0x%x) / U=%d\n", opcode, opcode, U);
    }
}

void arm64_hlp_dirty_advanced_simd_three_same_simd(uint64_t _regs, uint32_t insn)
{
    int U = INSN(29,29);
    int opcode = INSN(15,11);
    int size = INSN(23,22);

    switch(opcode) {
        case 3:
            if (U) {
                dis_bitop(_regs, insn);
            } else {
                if (size == 2)
                    dis_orr_register(_regs, insn);
                else if (size == 0)
                    dis_and(_regs, insn);
                else if (size == 1)
                    dis_bic_register(_regs, insn);
                else
                    fatal("size = %d\n", size);
            }
            break;
        case 6:
            if (U)
                dis_cmhi(_regs, insn);
            else
                dis_cmgt(_regs, insn);
            break;
        case 7:
            if (U)
                dis_cmhs(_regs, insn);
            else
                dis_cmge(_regs, insn);
            break;
        case 16:
            if (U)
                assert(0);
            else
                dis_add(_regs, insn);
            break;
        case 17:
            if (U)
                dis_cmeq(_regs, insn);
            else
                dis_cmtst(_regs, insn);
            break;
        case 23:
            if (U)
                assert(0);
            else
                dis_addp(_regs, insn);
            break;
        default:
            fatal("opcode = %d(0x%x) / U=%d\n", opcode, opcode, U);
    }
}

void arm64_hlp_dirty_advanced_simd_simd_three_different_simd(uint64_t _regs, uint32_t insn)
{
    int U = INSN(29,29);
    int opcode = INSN(15,12);

    switch(opcode) {
        case 4:
            if (U)
                assert(0);
            else
                dis_addhn(_regs, insn);
            break;
        default:
            fatal("opcode = %d(0x%x) / U=%d\n", opcode, opcode, U);
    }
}

void arm64_hlp_dirty_advanced_simd_scalar_pair_wise_simd(uint64_t _regs, uint32_t insn)
{
    int U = INSN(29,29);
    int opcode = INSN(16,12);

    switch(opcode) {
        case 27:
            assert(U==0);
            dis_addp(_regs, insn);
            break;
        default:
            fatal("opcode = %d(0x%x) / U=%d\n", opcode, opcode, U);
    }
}

void arm64_hlp_dirty_advanced_simd_accross_lanes_simd(uint64_t _regs, uint32_t insn)
{
    int U = INSN(29,29);
    int opcode = INSN(16,12);

    switch(opcode) {
        case 27:
            assert(U==0);
            dis_addv(_regs, insn);
            break;
        default:
            fatal("opcode = %d(0x%x) / U=%d\n", opcode, opcode, U);
    }
}
