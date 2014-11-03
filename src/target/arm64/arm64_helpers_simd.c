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

/* FIXME: merge code with others threee same */
void arm64_hlp_dirty_add_simd_vector(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int size = INSN(23,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int rm = INSN(20,16);
    int i;

    regs->v[rd].v.msb = 0;
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
            for(i = 0; i < (q?2:1); i++)
                regs->v[rd].d[i] = regs->v[rn].d[i] + regs->v[rm].d[i];
            break;
    }
}

void arm64_hlp_dirty_cmpeq_simd_vector(uint64_t _regs, uint32_t insn)
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

/* FIXME: merge code with others threee same */
void arm64_hlp_dirty_cmpeq_register_simd_vector(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int size = INSN(23,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int rm = INSN(20,16);
    int i;

    regs->v[rd].v.msb = 0;
    switch(size) {
        case 0:
            for(i = 0; i < (q?16:8); i++)
                regs->v[rd].b[i] = regs->v[rn].b[i]==regs->v[rm].b[i]?0xff:0;
            break;
        case 1:
            for(i = 0; i < (q?8:4); i++)
                regs->v[rd].h[i] = regs->v[rn].h[i]==regs->v[rm].h[i]?0xffff:0;
            break;
        case 2:
            for(i = 0; i < (q?4:2); i++) {
                regs->v[rd].s[i] = regs->v[rn].s[i]==regs->v[rm].s[i]?0xffffffff:0;
            }
            break;
        case 3:
            for(i = 0; i < (q?2:1); i++)
                regs->v[rd].d[i] = regs->v[rn].d[i]==regs->v[rm].d[i]?0xffffffffffffffffUL:0;
            break;
    }
}

void arm64_hlp_dirty_orr_register_simd_vector(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int rm = INSN(20,16);

    regs->v[rd].v.lsb = regs->v[rn].v.lsb | regs->v[rm].v.lsb;
    regs->v[rd].v.msb = q?regs->v[rn].v.msb | regs->v[rm].v.msb:0;
}

void arm64_hlp_dirty_addp_simd_vector(uint64_t _regs, uint32_t insn)
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
            for(i = 0; i < (q?8:4); i++) {
                res.b[i] = regs->v[rn].b[i] + regs->v[rn].b[i + 1];
                res.b[(q?8:4) + i] = regs->v[rm].b[i] + regs->v[rm].b[i + 1];
            }
            break;
        case 1:
            for(i = 0; i < (q?4:2); i++) {
                res.h[i] = regs->v[rn].h[i] + regs->v[rn].h[i + 1];
                res.h[(q?4:2) + i] = regs->v[rm].h[i] + regs->v[rm].h[i + 1];
            }
            break;
        case 2:
            for(i = 0; i < (q?2:1); i++) {
                res.s[i] = regs->v[rn].s[i] + regs->v[rn].s[i + 1];
                res.s[(q?2:1) + i] = regs->v[rm].s[i] + regs->v[rm].s[i + 1];
            }
            break;
        case 3:
            assert(q == 1);
            for(i = 0; i < 1; i++) {
                res.d[i] = regs->v[rn].d[i] + regs->v[rn].d[i + 1];
                res.d[i+1] = regs->v[rm].d[i] + regs->v[rm].d[i + 1];
            }
            break;
    }

    regs->v[rd] = res;
}

void arm64_hlp_dirty_and_simd_vector(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int rm = INSN(20,16);

    regs->v[rd].v.lsb = regs->v[rn].v.lsb & regs->v[rm].v.lsb;
    regs->v[rd].v.msb = q?regs->v[rn].v.msb & regs->v[rm].v.msb:0;
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
    //struct arm64_registers *regs = (struct arm64_registers *) _regs;
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
