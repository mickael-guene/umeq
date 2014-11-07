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

static inline uint8_t uabs8(uint8_t op1, uint8_t op2)
{
    return op1>op2?op1-op2:op2-op1;
}
static inline uint16_t uabs16(uint16_t op1, uint16_t op2)
{
    return op1>op2?op1-op2:op2-op1;
}
static inline uint32_t uabs32(uint32_t op1, uint32_t op2)
{
    return op1>op2?op1-op2:op2-op1;
}
static inline uint64_t uabs64(uint64_t op1, uint64_t op2)
{
    return op1>op2?op1-op2:op2-op1;
}
static inline uint8_t sabs8(int8_t op1, int8_t op2)
{
    return op1>op2?op1-op2:op2-op1;
}
static inline uint16_t sabs16(int16_t op1, int16_t op2)
{
    return op1>op2?op1-op2:op2-op1;
}
static inline uint32_t sabs32(int32_t op1, int32_t op2)
{
    return op1>op2?op1-op2:op2-op1;
}
static inline uint64_t sabs64(int64_t op1, int64_t op2)
{
    return op1>op2?op1-op2:op2-op1;
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
static inline uint64_t umax64(uint64_t op1, uint64_t op2)
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
static inline uint64_t smax64(int64_t op1, int64_t op2)
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
static inline uint64_t umin64(uint64_t op1, uint64_t op2)
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
static inline uint64_t smin64(int64_t op1, int64_t op2)
{
    return op1<op2?op1:op2;
}
static inline void set_qc(struct arm64_registers *regs)
{
    regs->fpsr |= 1 << 27;
}
#define DECLARE_USAT(size,max) \
static inline uint##size##_t usat##size(struct arm64_registers *regs, int64_t op) \
{ \
    uint##size##_t res; \
 \
    if (op > max) { \
        res = max; \
        regs->fpsr |= 1 << 27; \
    } else if (op < 0) { \
        res = 0; \
        regs->fpsr |= 1 << 27; \
    } else \
        res = op; \
 \
    return res; \
}
DECLARE_USAT(8,0xff)
DECLARE_USAT(16,0xffff)
DECLARE_USAT(32,0xffffffff)
static inline uint64_t usat64(struct arm64_registers *regs, __int128_t op)
{
    uint64_t res;
    __int128_t max = 0xffffffffffffffffUL;

    if (op > max) {
        res = max;
        regs->fpsr |= 1 << 27;
    } else if (op < 0) {
        res = 0;
        regs->fpsr |= 1 << 27;
    } else
        res = op;

    return res;
}

#define DECLARE_SSAT(size,max,min) \
static inline int##size##_t ssat##size(struct arm64_registers *regs, int64_t op) \
{ \
    int##size##_t res; \
 \
    if (op > max) { \
        res = max; \
        regs->fpsr |= 1 << 27; \
    } else if (op < min) { \
        res = min; \
        regs->fpsr |= 1 << 27; \
    } else \
        res = op; \
 \
    return res; \
}
DECLARE_SSAT(8,0x7f,-0x80)
DECLARE_SSAT(16,0x7fff,-0x8000)
DECLARE_SSAT(32,0x7fffffff,-0x80000000L)
static inline int64_t ssat64(struct arm64_registers *regs, __int128_t op)
{
    int64_t res;
    __int128_t max = 0x7fffffffffffffffUL;
    __int128_t min = -(max+1);

    if (op > max) {
        res = max;
        regs->fpsr |= 1 << 27;
    } else if (op < min) {
        res = min;
        regs->fpsr |= 1 << 27;
    } else
        res = op;

    return res;
}

void arm64_hlp_dirty_simd_dup_element(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int imm5 = INSN(20,16);
    int rn = INSN(9,5);
    int rd = INSN(4,0);
    uint64_t rn_r;
    uint64_t lsb = 0;
    uint64_t msb = 0;

    if (imm5 & 1) {
        rn_r = regs->v[rn].b[imm5 >> 1];
        lsb = (rn_r << 56) | (rn_r << 48) | (rn_r << 40) | (rn_r << 32) | (rn_r << 24) | (rn_r << 16) | (rn_r << 8) | (rn_r << 0);
    } else if (imm5 & 2) {
        rn_r = regs->v[rn].h[imm5 >> 2];
        lsb = (rn_r << 48) | (rn_r << 32) | (rn_r << 16) | (rn_r << 0);
    } else if (imm5 & 4) {
        rn_r = regs->v[rn].s[imm5 >> 3];
        lsb = (rn_r << 32) | (rn_r << 0);
    } else {
        assert(q == 1);
        lsb = regs->v[rn].d[imm5 >> 4];
    }
    if (q)
        msb = lsb;

    regs->v[rd].v.lsb = lsb;
    regs->v[rd].v.msb = msb;
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

static void dis_shl(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int immh = INSN(22,19);
    int imm = INSN(22,16);
    int is_scalar = INSN(28,28);
    int i;
    int shift;
    union simd_register res = {0};

    if (immh == 1) {
        shift = imm - 8;
        for(i = 0; i < (q?16:8); i++)
            res.b[i] = regs->v[rn].b[i] << shift;
    } else if (immh < 4) {
        shift = imm - 16;
        for(i = 0; i < (q?8:4); i++)
            res.h[i] = regs->v[rn].h[i] << shift;
    } else if (immh < 8) {
        shift = imm - 32;
        for(i = 0; i < (q?4:2); i++)
            res.s[i] = regs->v[rn].s[i] << shift;
    } else {
        shift = imm - 64;
        for(i = 0; i < (is_scalar?1:2); i++)
            res.d[i] = regs->v[rn].d[i] << shift;
    }
    regs->v[rd] = res;
}

static void dis_sqshlu(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int immh = INSN(22,19);
    int imm = INSN(22,16);
    int is_scalar = INSN(28,28);
    int i;
    int shift;
    union simd_register res = {0};

    if (immh == 1) {
        shift = imm - 8;
        for(i = 0; i < (is_scalar?1:(q?16:8)); i++)
            //fprintf(stderr, "%d << %d\n", regs->v[rn].b[i]);
            res.b[i] = usat8(regs, (int8_t)regs->v[rn].b[i] << shift);
    } else if (immh < 4) {
        shift = imm - 16;
        for(i = 0; i < (is_scalar?1:(q?8:4)); i++)
            res.h[i] = usat16(regs, (int16_t)regs->v[rn].h[i] << shift);
    } else if (immh < 8) {
        shift = imm - 32;
        for(i = 0; i < (is_scalar?1:(q?4:2)); i++)
            res.s[i] = usat32(regs, (int64_t)(int32_t)regs->v[rn].s[i] << shift);
    } else {
        shift = imm - 64;
        for(i = 0; i < (is_scalar?1:2); i++)
            res.d[i] = usat64(regs, (__int128_t)(int64_t)regs->v[rn].d[i] << shift);
    }
    regs->v[rd] = res;
}

static void dis_sqrshrun_sqshrun(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int immh = INSN(22,19);
    int imm = INSN(22,16);
    int is_scalar = INSN(28,28);
    int is_round = INSN(11,11);
    int i;
    int shift;
    union simd_register res = {0};

    if (q && !is_scalar)
        res.v.lsb = regs->v[rd].v.lsb;
    if (immh == 1) {
        shift = 16 - imm;
        for(i = 0; i < (is_scalar?1:8); i++)
            res.b[is_scalar?i:(q?i+8:i)] = usat8(regs, ((int16_t)regs->v[rn].h[i] + (is_round?(1L<<(shift-1)):0)) >> shift);
    } else if (immh < 4) {
        shift = 32 - imm;
        for(i = 0; i < (is_scalar?1:4); i++)
            res.h[is_scalar?i:(q?i+4:i)] = usat16(regs, ((int64_t)(int32_t)regs->v[rn].s[i] + (is_round?(1L<<(shift-1)):0)) >> shift);
    } else if (immh < 8) {
        shift = 64 -imm;
        for(i = 0; i < (is_scalar?1:2); i++)
            res.s[is_scalar?i:(q?i+2:i)] = usat32(regs, ((__int128_t)(int64_t)regs->v[rn].d[i] + (is_round?(1L<<(shift-1)):0)) >> shift);
    } else
        assert(0);
    regs->v[rd] = res;
}

static void dis_sqrshrn_sqshrn(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int immh = INSN(22,19);
    int imm = INSN(22,16);
    int is_scalar = INSN(28,28);
    int is_round = INSN(11,11);
    int i;
    int shift;
    union simd_register res = {0};

    if (q && !is_scalar)
        res.v.lsb = regs->v[rd].v.lsb;
    if (immh == 1) {
        shift = 16 - imm;
        for(i = 0; i < (is_scalar?1:8); i++)
            res.b[is_scalar?i:(q?i+8:i)] = ssat8(regs, ((int16_t)regs->v[rn].h[i] + (is_round?(1L<<(shift-1)):0)) >> shift);
    } else if (immh < 4) {
        shift = 32 - imm;
        for(i = 0; i < (is_scalar?1:4); i++)
            res.h[is_scalar?i:(q?i+4:i)] = ssat16(regs, ((int64_t)(int32_t)regs->v[rn].s[i] + (is_round?(1L<<(shift-1)):0)) >> shift);
    } else if (immh < 8) {
        shift = 64 -imm;
        for(i = 0; i < (is_scalar?1:2); i++)
            res.s[is_scalar?i:(q?i+2:i)] = ssat32(regs, ((__int128_t)(int64_t)regs->v[rn].d[i] + (is_round?(1L<<(shift-1)):0)) >> shift);
    } else
        assert(0);
    regs->v[rd] = res;
}

static void dis_sshll(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int immh = INSN(22,19);
    int imm = INSN(22,16);
    int i;
    int shift;
    union simd_register res = {0};

    if (immh == 1) {
        shift = imm - 8;
        for(i = 0; i < 8; i++)
            res.h[i] = (int8_t)regs->v[rn].b[q?i+8:i] << shift;
    } else if (immh < 4) {
        shift = imm - 16;
        for(i = 0; i < 4; i++)
            res.s[i] = (int16_t)regs->v[rn].h[q?i+4:i] << shift;
    } else if (immh < 8) {
        shift = imm - 32;
        for(i = 0; i < 2; i++)
            res.d[i] = (int64_t)(int32_t)regs->v[rn].s[q?i+2:i] << shift;
    } else
        assert(0);

    regs->v[rd] = res;
}

static void dis_srshr_srsra_sshr_ssra(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int immh = INSN(22,19);
    int imm = INSN(22,16);
    int is_scalar = INSN(28,28);
    int is_round = INSN(13,13);
    int is_acc = INSN(12,12);
    int i;
    int shift;
    union simd_register res = {0};

    if (immh == 1) {
        shift = 16 - imm;
        for(i = 0; i < (q?16:8); i++)
            res.b[i] = (((int8_t)regs->v[rn].b[i] + (is_round?(1L<<(shift-1)):0)) >> shift) + (is_acc?regs->v[rd].b[i]:0);
    } else if (immh < 4) {
        shift = 32 - imm;
        for(i = 0; i < (q?8:4); i++)
            res.h[i] = (((int16_t)regs->v[rn].h[i] + (is_round?(1L<<(shift-1)):0)) >> shift) + (is_acc?regs->v[rd].h[i]:0);
    } else if (immh < 8) {
        shift = 64 - imm;
        for(i = 0; i < (q?4:2); i++)
            res.s[i] = (((int32_t)regs->v[rn].s[i] + (is_round?(1L<<(shift-1)):0)) >> shift) + (is_acc?regs->v[rd].s[i]:0);
    } else {
        shift = 128 - imm;
        for(i = 0; i < (is_scalar?1:2); i++)
            res.d[i] = (((int64_t)regs->v[rn].d[i] + (is_round?(1L<<(shift-1)):0)) >> shift) + (is_acc?regs->v[rd].d[i]:0);
    }

    regs->v[rd] = res;
}

static void dis_sri(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int immh = INSN(22,19);
    int imm = INSN(22,16);
    int is_scalar = INSN(28,28);
    int i;
    int shift;
    union simd_register res = {0};

    if (immh == 1) {
        shift = 16 - imm;
        for(i = 0; i < (q?16:8); i++)
            res.b[i] = (regs->v[rn].b[i] >> shift) | (regs->v[rd].b[i] & ~(0xff >> shift));
    } else if (immh < 4) {
        shift = 32 - imm;
        for(i = 0; i < (q?8:4); i++)
            res.h[i] = (regs->v[rn].h[i] >> shift) | (regs->v[rd].h[i] & ~(0xffff >> shift));
    } else if (immh < 8) {
        shift = 64 - imm;
        for(i = 0; i < (q?4:2); i++)
            res.s[i] = (regs->v[rn].s[i] >> shift) | (regs->v[rd].s[i] & ~(0xffffffff >> shift));
    } else {
        shift = 128 - imm;
        for(i = 0; i < (is_scalar?1:2); i++)
            res.d[i] = (regs->v[rn].d[i] >> shift) | (regs->v[rd].d[i] & ~(~0UL >> shift));
    }

    regs->v[rd] = res;
}

static void dis_sli(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int immh = INSN(22,19);
    int imm = INSN(22,16);
    int is_scalar = INSN(28,28);
    int i;
    int shift;
    union simd_register res = {0};

    if (immh == 1) {
        shift = imm - 8;
        for(i = 0; i < (q?16:8); i++)
            res.b[i] = (regs->v[rn].b[i] << shift) | (regs->v[rd].b[i] & ((1UL << shift) - 1));
    } else if (immh < 4) {
        shift = imm - 16;
        for(i = 0; i < (q?8:4); i++)
            res.h[i] = (regs->v[rn].h[i] << shift) | (regs->v[rd].h[i] & ((1UL << shift) - 1));
    } else if (immh < 8) {
        shift = imm - 32;
        for(i = 0; i < (q?4:2); i++)
            res.s[i] = (regs->v[rn].s[i] << shift) | (regs->v[rd].s[i] & ((1UL << shift) - 1));
    } else {
        shift = imm - 64;
        for(i = 0; i < (is_scalar?1:2); i++)
            res.d[i] = (regs->v[rn].d[i] << shift) | (regs->v[rd].d[i] & ((1UL << shift) - 1));
    }
    regs->v[rd] = res;
}

static void dis_shrn_rshrn(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int immh = INSN(22,19);
    int imm = INSN(22,16);
    int is_round = INSN(11,11);
    int i;
    int shift;
    union simd_register res;

    if (immh == 1) {
        shift = 16 - imm;
        for(i = 0; i < 8; i++)
            res.b[i] = (regs->v[rn].h[i] + (is_round?(1UL << (shift - 1)):0)) >> shift;
    } else if (immh < 4) {
        shift = 32 - imm;
        for(i = 0; i < 4; i++)
            res.h[i] = (regs->v[rn].s[i] + (is_round?(1UL << (shift - 1)):0)) >> shift;
    } else if (immh < 8) {
        shift = 64 - imm;
        for(i = 0; i < 2; i++)
            res.s[i] = (regs->v[rn].d[i] + (is_round?(1UL << (shift - 1)):0)) >> shift;
    } else
        assert(0);

    if (q)
        regs->v[rd].v.msb = res.v.lsb;
    else {
        regs->v[rd].v.lsb = res.v.lsb;
        regs->v[rd].v.msb = 0;
    }
}

void arm64_hlp_dirty_advanced_simd_shift_by_immediate_simd(uint64_t _regs, uint32_t insn)
{
    int opcode = INSN(15,11);
    int U = INSN(29,29);

    switch(opcode) {
        case 0:
            U?dis_ushr_vector(_regs, insn):dis_srshr_srsra_sshr_ssra(_regs, insn);
            break;
        case 2:
            U?assert(0):dis_srshr_srsra_sshr_ssra(_regs, insn);
            break;
        case 4:
            U?assert(0):dis_srshr_srsra_sshr_ssra(_regs, insn);
            break;
        case 6:
            U?assert(0):dis_srshr_srsra_sshr_ssra(_regs, insn);
            break;
        case 8:
            U?dis_sri(_regs, insn):assert(0);
            break;
        case 10:
            if (U)
                dis_sli(_regs, insn);
            else
                dis_shl(_regs, insn);
            break;
        case 12:
            U?dis_sqshlu(_regs, insn):assert(0);
            break;
        case 16:
            U?dis_sqrshrun_sqshrun(_regs, insn):dis_shrn_rshrn(_regs, insn);
            break;
        case 17:
            U?dis_sqrshrun_sqshrun(_regs, insn):dis_shrn_rshrn(_regs, insn);
            break;
        case 18:
            U?assert(0):dis_sqrshrn_sqshrn(_regs, insn);
        case 19:
            U?assert(0):dis_sqrshrn_sqshrn(_regs, insn);
            break;
        case 20:
            U?assert(0):dis_sshll(_regs, insn);
            break;
        default:
            fatal("opcode = %d(0x%x) / U=%d\n", opcode, opcode, U);
    }
}

static void dis_neg(uint64_t _regs, uint32_t insn)
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
                regs->v[rd].b[i] = -regs->v[rn].b[i];
            break;
        case 1:
            for(i = 0; i < (q?8:4); i++)
                regs->v[rd].h[i] = -regs->v[rn].h[i];
            break;
        case 2:
            for(i = 0; i < (q?4:2); i++) {
                regs->v[rd].s[i] = -regs->v[rn].s[i];
            }
            break;
        case 3:
            for(i = 0; i < (q && !is_scalar?2:1); i++)
                regs->v[rd].d[i] = -regs->v[rn].d[i];
            break;
    }
    if (!q || is_scalar)
        regs->v[rd].v.msb = 0;
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
                regs->v[rd].b[i] = sabs8(regs->v[rn].b[i],0);
            break;
        case 1:
            for(i = 0; i < (q?8:4); i++)
                regs->v[rd].h[i] = sabs16(regs->v[rn].h[i],0);
            break;
        case 2:
            for(i = 0; i < (q?4:2); i++) {
                regs->v[rd].s[i] = sabs32(regs->v[rn].s[i],0);
            }
            break;
        case 3:
            for(i = 0; i < (q && !is_scalar?2:1); i++)
                regs->v[rd].d[i] = sabs64(regs->v[rn].d[i],0);
            break;
    }
    if (!q || is_scalar)
        regs->v[rd].v.msb = 0;
}

static void dis_sqxtun(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int is_scalar = INSN(28,28);
    int q = INSN(30,30);
    int size = INSN(23,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int i;
    union simd_register res = {0};

    if (q && !is_scalar)
        res.v.lsb = regs->v[rd].v.lsb;
    switch(size) {
        case 0:
            for(i = 0; i < (is_scalar?1:8); i++)
                res.b[is_scalar?i:q?i+8:i] = usat8(regs, (int16_t)regs->v[rn].h[i]);
            break;
        case 1:
            for(i = 0; i < (is_scalar?1:4); i++)
                res.h[is_scalar?i:q?i+4:i] = usat16(regs, (int32_t)regs->v[rn].s[i]);
            break;
        case 2:
            for(i = 0; i < (is_scalar?1:2); i++)
                res.s[is_scalar?i:q?i+2:i] = usat32(regs, (int64_t)regs->v[rn].d[i]);
            break;
        default:
            assert(0);
    }

    regs->v[rd] = res;
}

static void dis_sqxtn(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int is_scalar = INSN(28,28);
    int q = INSN(30,30);
    int size = INSN(23,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int i;
    union simd_register res = {0};

    if (q && !is_scalar)
        res.v.lsb = regs->v[rd].v.lsb;
    switch(size) {
        case 0:
            for(i = 0; i < (is_scalar?1:8); i++)
                res.b[is_scalar?i:q?i+8:i] = ssat8(regs, (int16_t)regs->v[rn].h[i]);
            break;
        case 1:
            for(i = 0; i < (is_scalar?1:4); i++)
                res.h[is_scalar?i:q?i+4:i] = ssat16(regs, (int32_t)regs->v[rn].s[i]);
            break;
        case 2:
            for(i = 0; i < (is_scalar?1:2); i++)
                res.s[is_scalar?i:q?i+2:i] = ssat32(regs, (int64_t)regs->v[rn].d[i]);
            break;
        default:
            assert(0);
    }

    regs->v[rd] = res;
}

static void dis_suqadd(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int is_scalar = INSN(28,28);
    int q = INSN(30,30);
    int size = INSN(23,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int i;
    union simd_register res = {0};

    switch(size) {
        case 0:
            for(i = 0; i < (is_scalar?1:(q?16:8)); i++)
                res.b[i] = ssat8(regs, regs->v[rn].b[i] + (int64_t)(int8_t)regs->v[rd].b[i]);
            break;
        case 1:
            for(i = 0; i < (is_scalar?1:(q?8:4)); i++)
                res.h[i] = ssat16(regs, regs->v[rn].h[i] + (int64_t)(int16_t)regs->v[rd].h[i]);
            break;
        case 2:
            for(i = 0; i < (is_scalar?1:(q?4:2)); i++)
                res.s[i] = ssat32(regs, regs->v[rn].s[i] + (int64_t)(int32_t)regs->v[rd].s[i]);
            break;
        case 3:
            for(i = 0; i < (is_scalar?1:(q?2:1)); i++)
                res.d[i] = ssat64(regs, regs->v[rn].d[i] + (__int128_t)(int64_t)regs->v[rd].d[i]);
            break;
        default:
            assert(0);
    }

    regs->v[rd] = res;
}

static void dis_sqneg(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int is_scalar = INSN(28,28);
    int q = INSN(30,30);
    int size = INSN(23,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int i;
    union simd_register res = {0};

    switch(size) {
        case 0:
            for(i = 0; i < (is_scalar?1:(q?16:8)); i++)
                res.b[i] = ssat8(regs, -(int8_t)regs->v[rn].b[i]);
            break;
        case 1:
            for(i = 0; i < (is_scalar?1:(q?8:4)); i++)
                res.h[i] = ssat16(regs, -(int16_t)regs->v[rn].h[i]);
            break;
        case 2:
            for(i = 0; i < (is_scalar?1:(q?4:2)); i++)
                res.s[i] = ssat32(regs, -(int32_t)regs->v[rn].s[i]);
            break;
        case 3:
            for(i = 0; i < (is_scalar?1:(q?2:1)); i++)
                res.d[i] = ssat64(regs, -(int64_t)regs->v[rn].d[i]);
            break;
        default:
            assert(0);
    }
    regs->v[rd] = res;
}

static void dis_sqabs(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int is_scalar = INSN(28,28);
    int q = INSN(30,30);
    int size = INSN(23,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int i;
    union simd_register res = {0};

    switch(size) {
        case 0:
            for(i = 0; i < (is_scalar?1:(q?16:8)); i++) {
                if (regs->v[rn].b[i] == 0x80) {
                    res.b[i] = 0x7f;
                    set_qc(regs);
                } else
                    res.b[i] = sabs8(regs->v[rn].b[i],0);
            }
            break;
        case 1:
            for(i = 0; i < (is_scalar?1:(q?8:4)); i++) {
                if (regs->v[rn].h[i] == 0x8000) {
                    res.h[i] = 0x7fff;
                    set_qc(regs);
                } else
                    res.h[i] = sabs16(regs->v[rn].h[i],0);
            }
            break;
        case 2:
            for(i = 0; i < (is_scalar?1:(q?4:2)); i++) {
                if (regs->v[rn].s[i] == 0x80000000) {
                    res.s[i] = 0x7fffffff;
                    set_qc(regs);
                } else
                    res.s[i] = sabs32(regs->v[rn].s[i],0);
            }
            break;
        case 3:
            for(i = 0; i < (is_scalar?1:(q?2:1)); i++) {
                if (regs->v[rn].d[i] == 0x8000000000000000UL) {
                    res.d[i] = 0x7fffffffffffffffUL;
                    set_qc(regs);
                } else
                    res.d[i] = sabs64(regs->v[rn].d[i],0);
            }
            break;
        default:
            assert(0);
    }
    regs->v[rd] = res;
}

static void dis_shll(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int size = INSN(23,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int i;
    union simd_register res;

    switch(size) {
        case 0:
             for(i = 0; i < 8; i++)
                res.h[i] = regs->v[rn].b[q?i+8:i] << 8;
            break;
        case 1:
             for(i = 0; i < 4; i++)
                res.s[i] = regs->v[rn].h[q?i+4:i] << 16;
            break;
        case 2:
             for(i = 0; i < 2; i++)
                res.d[i] = (uint64_t)regs->v[rn].s[q?i+2:i] << 32;
            break;
        default:
            assert(0);
    }
    regs->v[rd] = res;
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

static void dis_mal_mls(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int size = INSN(23,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int rm = INSN(20,16);
    int is_sub = INSN(29,29);
    int i;

    switch(size) {
        case 0:
            for(i = 0; i < (q?16:8); i++)
                if (is_sub)
                    regs->v[rd].b[i] -= regs->v[rn].b[i] * regs->v[rm].b[i];
                else
                    regs->v[rd].b[i] += regs->v[rn].b[i] * regs->v[rm].b[i];
            break;
        case 1:
            for(i = 0; i < (q?8:4); i++)
                if (is_sub)
                    regs->v[rd].h[i] -= regs->v[rn].h[i] * regs->v[rm].h[i];
                else
                    regs->v[rd].h[i] += regs->v[rn].h[i] * regs->v[rm].h[i];
            break;
        case 2:
            for(i = 0; i < (q?4:2); i++) {
                if (is_sub)
                    regs->v[rd].s[i] -= regs->v[rn].s[i] * regs->v[rm].s[i];
                else
                    regs->v[rd].s[i] += regs->v[rn].s[i] * regs->v[rm].s[i];
            }
            break;
        case 3:
            for(i = 0; i < (q?2:1); i++)
                if (is_sub)
                    regs->v[rd].d[i] -= regs->v[rn].d[i] * regs->v[rm].d[i];
                else
                    regs->v[rd].d[i] += regs->v[rn].d[i] * regs->v[rm].d[i];
            break;
    }
    if (!q)
        regs->v[rd].v.msb = 0;
}

static void dis_pmul(uint64_t _regs, uint32_t insn)
{
    /* FIXME: just do it */
    assert(0);
}

static void dis_mul(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int size = INSN(23,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int rm = INSN(20,16);
    int i;

    switch(size) {
        case 0:
            for(i = 0; i < (q?16:8); i++)
                regs->v[rd].b[i] = regs->v[rn].b[i] * regs->v[rm].b[i];
            break;
        case 1:
            for(i = 0; i < (q?8:4); i++)
                regs->v[rd].h[i] = regs->v[rn].h[i] * regs->v[rm].h[i];
            break;
        case 2:
            for(i = 0; i < (q?4:2); i++) {
                regs->v[rd].s[i] = regs->v[rn].s[i] * regs->v[rm].s[i];
            }
            break;
        case 3:
            for(i = 0; i < (q?2:1); i++)
                regs->v[rd].d[i] = regs->v[rn].d[i] * regs->v[rm].d[i];
            break;
    }
    if (!q)
        regs->v[rd].v.msb = 0;
}

static void dis_smax_smin(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int size = INSN(23,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int rm = INSN(20,16);
    int is_min = INSN(11,11);
    int i;
    union simd_register res;

    res.v.msb = 0;
    switch(size) {
        case 0:
            {
                uint8_t (*minmax)(int8_t,int8_t) = is_min?smin8:smax8;
                for(i = 0; i < (q?16:8); i++)
                    res.b[i] = (*minmax)(regs->v[rn].b[i],regs->v[rm].b[i]);
            }
            break;
        case 1:
            {
                uint16_t (*minmax)(int16_t,int16_t) = is_min?smin16:smax16;
                for(i = 0; i < (q?8:4); i++)
                    res.h[i] = (*minmax)(regs->v[rn].h[i],regs->v[rm].h[i]);
            }
            break;
        case 2:
            {
                uint32_t (*minmax)(int32_t,int32_t) = is_min?smin32:smax32;
                for(i = 0; i < (q?4:2); i++)
                    res.s[i] = (*minmax)(regs->v[rn].s[i],regs->v[rm].s[i]);
            }
            break;
        default:
            assert(0);
    }
    regs->v[rd] = res;
}

static void dis_sabd_saba(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int size = INSN(23,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int rm = INSN(20,16);
    int is_acc = INSN(11,11);
    int i;
    union simd_register res;

    res.v.msb = 0;
    switch(size) {
        case 0:
            for(i = 0; i < (q?16:8); i++)
                res.b[i] = (is_acc?regs->v[rd].b[i]:0) + sabs8(regs->v[rn].b[i], regs->v[rm].b[i]);
            break;
        case 1:
            for(i = 0; i < (q?8:4); i++)
                res.h[i] = (is_acc?regs->v[rd].h[i]:0) + sabs16(regs->v[rn].h[i], regs->v[rm].h[i]);
            break;
        case 2:
            for(i = 0; i < (q?4:2); i++)
                res.s[i] = (is_acc?regs->v[rd].s[i]:0) + sabs32(regs->v[rn].s[i], regs->v[rm].s[i]);
            break;
        default:
            assert(0);
    }

    regs->v[rd] = res;
}

static void dis_sqrshl(uint64_t _regs, uint32_t insn)
{
    assert(0);
}

static void dis_sqadd(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int is_scalar = INSN(28,28);
    int q = INSN(30,30);
    int size = INSN(23,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int rm = INSN(20,16);
    int i;
    union simd_register res = {0};
    switch(size) {
        case 0:
            for(i = 0; i < (is_scalar?1:(q?16:8)); i++)
                res.b[i] = ssat8(regs, (int8_t)regs->v[rn].b[i] + (int8_t)regs->v[rm].b[i]);
            break;
        case 1:
            for(i = 0; i < (is_scalar?1:(q?8:4)); i++)
                res.h[i] = ssat16(regs, (int16_t)regs->v[rn].h[i] + (int16_t)regs->v[rm].h[i]);
            break;
        case 2:
            for(i = 0; i < (is_scalar?1:(q?4:2)); i++)
                res.s[i] = ssat32(regs, (int64_t)(int32_t)regs->v[rn].s[i] + (int64_t)(int32_t)regs->v[rm].s[i]);
            break;
        case 3:
            for(i = 0; i < (is_scalar?1:(q?2:1)); i++)
                res.d[i] = ssat64(regs, (__int128_t)(int64_t)regs->v[rn].d[i] + (__int128_t)(int64_t)regs->v[rm].d[i]);
            break;
    }
    regs->v[rd] = res;
}

static void dis_sqsub(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int is_scalar = INSN(28,28);
    int q = INSN(30,30);
    int size = INSN(23,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int rm = INSN(20,16);
    int i;
    union simd_register res = {0};
    switch(size) {
        case 0:
            for(i = 0; i < (is_scalar?1:(q?16:8)); i++)
                res.b[i] = ssat8(regs, (int8_t)regs->v[rn].b[i] - (int8_t)regs->v[rm].b[i]);
            break;
        case 1:
            for(i = 0; i < (is_scalar?1:(q?8:4)); i++)
                res.h[i] = ssat16(regs, (int16_t)regs->v[rn].h[i] - (int16_t)regs->v[rm].h[i]);
            break;
        case 2:
            for(i = 0; i < (is_scalar?1:(q?4:2)); i++)
                res.s[i] = ssat32(regs, (int64_t)(int32_t)regs->v[rn].s[i] - (int64_t)(int32_t)regs->v[rm].s[i]);
            break;
        case 3:
            for(i = 0; i < (is_scalar?1:(q?2:1)); i++)
                res.d[i] = ssat64(regs, (__int128_t)(int64_t)regs->v[rn].d[i] - (__int128_t)(int64_t)regs->v[rm].d[i]);
            break;
    }
    regs->v[rd] = res;
}

static void dis_add_sub(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int is_scalar = INSN(28,28);
    int q = INSN(30,30);
    int size = INSN(23,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int rm = INSN(20,16);
    int is_sub = INSN(29,29);
    int i;

    switch(size) {
        case 0:
            for(i = 0; i < (q?16:8); i++)
                if (is_sub)
                    regs->v[rd].b[i] = regs->v[rn].b[i] - regs->v[rm].b[i];
                else
                    regs->v[rd].b[i] = regs->v[rn].b[i] + regs->v[rm].b[i];
            break;
        case 1:
            for(i = 0; i < (q?8:4); i++)
                if (is_sub)
                    regs->v[rd].h[i] = regs->v[rn].h[i] - regs->v[rm].h[i];
                else
                    regs->v[rd].h[i] = regs->v[rn].h[i] + regs->v[rm].h[i];
            break;
        case 2:
            for(i = 0; i < (q?4:2); i++) {
                if (is_sub)
                    regs->v[rd].s[i] = regs->v[rn].s[i] - regs->v[rm].s[i];
                else
                    regs->v[rd].s[i] = regs->v[rn].s[i] + regs->v[rm].s[i];
            }
            break;
        case 3:
            for(i = 0; i < (q && !is_scalar?2:1); i++)
                if (is_sub)
                    regs->v[rd].d[i] = regs->v[rn].d[i] - regs->v[rm].d[i];
                else
                    regs->v[rd].d[i] = regs->v[rn].d[i] + regs->v[rm].d[i];
            break;
    }
    if (!q || is_scalar)
        regs->v[rd].v.msb = 0;
}

static void dis_sqdmulh_sqrdmulh(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int is_scalar = INSN(28,28);
    int q = INSN(30,30);
    int size = INSN(23,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int rm = INSN(20,16);
    int is_round = INSN(29,29);
    int i;
    union simd_register res = {0};

    switch(size) {
        case 1:
            for(i = 0; i < (is_scalar?1:(q?8:4)); i++)
                res.h[i] = ssat16(regs, (2 * (int16_t)regs->v[rn].h[i] * (int16_t)regs->v[rm].h[i] + (is_round?(1<<15):0)) >> 16);
            break;
        case 2:
            for(i = 0; i < (is_scalar?1:(q?4:2)); i++)
                res.s[i] = ssat32(regs, (2 * (int64_t)(int32_t)regs->v[rn].s[i] * (int64_t)(int32_t)regs->v[rm].s[i] + (is_round?(1L<<31):0)) >> 32);
            break;
        default:
            assert(0);
    }

    regs->v[rd] = res;
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

static void dis_orn(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int rm = INSN(20,16);

    regs->v[rd].v.lsb = regs->v[rn].v.lsb | ~regs->v[rm].v.lsb;
    regs->v[rd].v.msb = q?regs->v[rn].v.msb | ~regs->v[rm].v.msb:0;
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

static void dis_smaxp_sminp(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int size = INSN(23,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int rm = INSN(20,16);
    int is_min = INSN(11,11);
    int i;
    union simd_register res = {0};

    switch(size) {
        case 0:
            {
                uint8_t (*minmax)(int8_t,int8_t) = is_min?smin8:smax8;
                for(i = 0; i < (q?8:4); i++) {
                    res.b[i] = (*minmax)(regs->v[rn].b[2*i],regs->v[rn].b[2*i + 1]);
                    res.b[(q?8:4) + i] = (*minmax)(regs->v[rm].b[2*i],regs->v[rm].b[2*i + 1]);
                }
            }
            break;
        case 1:
            {
                uint16_t (*minmax)(int16_t,int16_t) = is_min?smin16:smax16;
                for(i = 0; i < (q?4:2); i++) {
                    res.h[i] = (*minmax)(regs->v[rn].h[2*i], regs->v[rn].h[2*i + 1]);
                    res.h[(q?4:2) + i] = (*minmax)(regs->v[rm].h[2*i], regs->v[rm].h[2*i + 1]);
                }
            }
            break;
        case 2:
            {
                uint32_t (*minmax)(int32_t,int32_t) = is_min?smin32:smax32;
                for(i = 0; i < (q?2:1); i++) {
                    res.s[i] = (*minmax)(regs->v[rn].s[2*i], regs->v[rn].s[2*i + 1]);
                    res.s[(q?2:1) + i] = (*minmax)(regs->v[rm].s[2*i], regs->v[rm].s[2*i + 1]);
                }
            }
            break;
        default:
            assert(0);
    }
    regs->v[rd] = res;
}

static void dis_saddl_ssubl(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int size = INSN(23,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int rm = INSN(20,16);
    int is_sub = INSN(13,13);
    int i;
    union simd_register res;

    res.v.msb = 0;
    switch(size) {
        case 0:
            for(i = 0; i < 8; i++)
                if (is_sub)
                    res.h[i] = (int8_t)regs->v[rn].b[q?i+8:i] - (int8_t)regs->v[rm].b[q?i+8:i];
                else
                    res.h[i] = (int8_t)regs->v[rn].b[q?i+8:i] + (int8_t)regs->v[rm].b[q?i+8:i];
            break;
        case 1:
            for(i = 0; i < 4; i++)
                if (is_sub)
                    res.s[i] = (int16_t)regs->v[rn].h[q?i+4:i] - (int16_t)regs->v[rm].h[q?i+4:i];
                else
                    res.s[i] = (int16_t)regs->v[rn].h[q?i+4:i] + (int16_t)regs->v[rm].h[q?i+4:i];
            break;
        case 2:
            for(i = 0; i < 2; i++)
                if (is_sub)
                    res.d[i] = (int64_t)(int32_t)regs->v[rn].s[q?i+2:i] - (int64_t)(int32_t)regs->v[rm].s[q?i+2:i];
                else
                    res.d[i] = (int64_t)(int32_t)regs->v[rn].s[q?i+2:i] + (int64_t)(int32_t)regs->v[rm].s[q?i+2:i];
            break;
        default:
            assert(0);
    }
    regs->v[rd] = res;
}

static void dis_saddw_ssubw(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int size = INSN(23,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int rm = INSN(20,16);
    int is_sub = INSN(13,13);
    int i;
    union simd_register res;

    res.v.msb = 0;
    switch(size) {
        case 0:
            for(i = 0; i < 8; i++)
                if (is_sub)
                    res.h[i] = (int16_t)regs->v[rn].h[i] - (int8_t)regs->v[rm].b[q?i+8:i];
                else
                    res.h[i] = (int16_t)regs->v[rn].h[i] + (int8_t)regs->v[rm].b[q?i+8:i];
            break;
        case 1:
            for(i = 0; i < 4; i++)
                if (is_sub)
                    res.s[i] = (int32_t)regs->v[rn].s[i] - (int16_t)regs->v[rm].h[q?i+4:i];
                else
                    res.s[i] = (int32_t)regs->v[rn].s[i] + (int16_t)regs->v[rm].h[q?i+4:i];
            break;
        case 2:
            for(i = 0; i < 2; i++)
                if (is_sub)
                    res.d[i] = (int64_t)regs->v[rn].d[i] - (int64_t)(int32_t)regs->v[rm].s[q?i+2:i];
                else
                    res.d[i] = (int64_t)regs->v[rn].d[i] + (int64_t)(int32_t)regs->v[rm].s[q?i+2:i];
            break;
        default:
            assert(0);
    }
    regs->v[rd] = res;
}

static void dis_addhn_raddhn_subhn_rsubhn(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int size = INSN(23,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int rm = INSN(20,16);
    int is_round = INSN(29,29);
    int is_sub = INSN(13,13);
    int i;
    union simd_register res;

    res.v.msb = 0;
    switch(size) {
        case 0:
            for(i = 0; i < 8; i++) {
                if (is_sub)
                    res.b[i] = (regs->v[rn].h[i] - regs->v[rm].h[i] + (is_round?(1UL << 7):0)) >> 8;
                else
                    res.b[i] = (regs->v[rn].h[i] + regs->v[rm].h[i] + (is_round?(1UL << 7):0)) >> 8;
            }
            break;
        case 1:
            for(i = 0; i < 4; i++) {
                if (is_sub)
                    res.h[i] = (regs->v[rn].s[i] - regs->v[rm].s[i] + (is_round?(1UL << 15):0)) >> 16;
                else
                    res.h[i] = (regs->v[rn].s[i] + regs->v[rm].s[i] + (is_round?(1UL << 15):0)) >> 16;
            }
            break;
        case 2:
            for(i = 0; i < 2; i++) {
                if (is_sub)
                    res.s[i] = (regs->v[rn].d[i] - regs->v[rm].d[i] + (is_round?(1UL << 31):0)) >> 32;
                else
                    res.s[i] = (regs->v[rn].d[i] + regs->v[rm].d[i] + (is_round?(1UL << 31):0)) >> 32;
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

static void dis_sabdl_sabal(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int size = INSN(23,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int rm = INSN(20,16);
    int is_acc = (INSN(13, 13) == 0);
    int i;
    union simd_register res = {0};

    if (is_acc)
        res = regs->v[rd];
    switch(size) {
        case 0:
            for(i = 0; i < 8; i++)
                res.h[i] += sabs8(regs->v[rn].b[q?i+8:i], regs->v[rm].b[q?i+8:i]);
            break;
        case 1:
            for(i = 0; i < 4; i++)
                res.s[i] += sabs16(regs->v[rn].h[q?i+4:i], regs->v[rm].h[q?i+4:i]);
            break;
        case 2:
            for(i = 0; i < 2; i++)
                res.d[i] += sabs32(regs->v[rn].s[q?i+2:i], regs->v[rm].s[q?i+2:i]);
            break;
        default:
            assert(0);
    }
    regs->v[rd] = res;
}

static void dis_smlal_smlsl(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int size = INSN(23,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int rm = INSN(20,16);
    int is_sub = INSN(13,13);
    int i;
    union simd_register res;

    res = regs->v[rd];
    switch(size) {
        case 0:
            for(i = 0; i < 8; i++)
                if (is_sub)
                    res.h[i] -= (int8_t)regs->v[rn].b[q?i+8:i] * (int8_t)regs->v[rm].b[q?i+8:i];
                else
                    res.h[i] += (int8_t)regs->v[rn].b[q?i+8:i] * (int8_t)regs->v[rm].b[q?i+8:i];
            break;
        case 1:
            for(i = 0; i < 4; i++)
                if (is_sub)
                    res.s[i] -= (int16_t)regs->v[rn].h[q?i+4:i] * (int16_t)regs->v[rm].h[q?i+4:i];
                else
                    res.s[i] += (int16_t)regs->v[rn].h[q?i+4:i] * (int16_t)regs->v[rm].h[q?i+4:i];
            break;
        case 2:
            for(i = 0; i < 2; i++)
                if (is_sub)
                    res.d[i] -= (int64_t)(int32_t)regs->v[rn].s[q?i+2:i] * (int64_t)(int32_t)regs->v[rm].s[q?i+2:i];
                else
                    res.d[i] += (int64_t)(int32_t)regs->v[rn].s[q?i+2:i] * (int64_t)(int32_t)regs->v[rm].s[q?i+2:i];
            break;
        default:
            assert(0);
    }
    regs->v[rd] = res;
}

static void dis_sqdmlal_sqdmlsl(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int is_scalar = INSN(28,28);
    int size = INSN(23,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int rm = INSN(20,16);
    int is_sub = INSN(13,13);
    int i;
    union simd_register res = {0};

    switch(size) {
        case 1:
            for(i = 0; i < (is_scalar?1:4); i++) {
                int32_t product = ssat32(regs, 2 * (int16_t)regs->v[rn].h[q?i+4:i] * (int16_t)regs->v[rm].h[q?i+4:i]);
                if (is_sub)
                    res.s[i] = ssat32(regs, (int64_t)(int32_t)regs->v[rd].s[i] - (int64_t)product);
                else
                    res.s[i] = ssat32(regs, (int64_t)(int32_t)regs->v[rd].s[i] + (int64_t)product);
            }
        break;
        case 2:
            for(i = 0; i < (is_scalar?1:2); i++) {
                int64_t product = ssat64(regs, 2 * (int64_t)(int32_t)regs->v[rn].s[q?i+2:i] * (int64_t)(int32_t)regs->v[rm].s[q?i+2:i]);
                if (is_sub)
                    res.d[i] = ssat64(regs, (__int128_t)(int64_t)regs->v[rd].d[i] - (__int128_t)product);
                else
                    res.d[i] = ssat64(regs, (__int128_t)(int64_t)regs->v[rd].d[i] + (__int128_t)product);
            }
        break;
        default:
            assert(0);
    }
    regs->v[rd] = res;
}

static void dis_smull(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int size = INSN(23,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int rm = INSN(20,16);
    int i;
    union simd_register res = {0};

    switch(size) {
        case 0:
            for(i = 0; i < 8; i++)
                res.h[i] = (int8_t)regs->v[rn].b[q?i+8:i] * (int8_t)regs->v[rm].b[q?i+8:i];
            break;
        case 1:
            for(i = 0; i < 4; i++)
                res.s[i] = (int16_t)regs->v[rn].h[q?i+4:i] * (int16_t)regs->v[rm].h[q?i+4:i];
            break;
        case 2:
            for(i = 0; i < 2; i++)
                res.d[i] = (int64_t)(int32_t)regs->v[rn].s[q?i+2:i] * (int64_t)(int32_t)regs->v[rm].s[q?i+2:i];
            break;
        default:
            assert(0);
    }
    regs->v[rd] = res;
}

static void dis_pmull(uint64_t _regs, uint32_t insn)
{
    assert(0);
}

static void dis_saddlv(uint64_t _regs, uint32_t insn)
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
                res.h[0] += (int8_t)regs->v[rn].b[i];
            break;
        case 1:
            for(i = 0; i < (q?8:4); i++)
                res.s[0] += (int16_t)regs->v[rn].h[i];
            break;
        case 2:
            assert(q);
            for(i = 0; i < 4; i++)
                res.d[0] += (int64_t)(int32_t)regs->v[rn].s[i];
            break;
        default:
            assert(0);
    }
    regs->v[rd].v = res.v;
}

static void dis_smaxv_sminv(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int size = INSN(23,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int is_min = INSN(16,16);
    int i;
    union simd_register res = {0};

    switch(size) {
        case 0:
            {
                uint8_t (*minmax)(int8_t,int8_t) = is_min?smin8:smax8;
                res.b[0] = regs->v[rn].b[0];
                for(i = 1; i < (q?16:8); i++)
                    res.b[0] = (*minmax)(regs->v[rn].b[i], res.b[0]);
            }
            break;
        case 1:
            {
                uint16_t (*minmax)(int16_t,int16_t) = is_min?smin16:smax16;
                res.h[0] = regs->v[rn].h[0];
                for(i = 1; i < (q?8:4); i++)
                    res.h[0] = (*minmax)(regs->v[rn].h[i], res.h[0]);
            }
            break;
        case 2:
            {
                uint32_t (*minmax)(int32_t,int32_t) = is_min?smin32:smax32;
                assert(q);
                res.s[0] = regs->v[rn].s[0];
                for(i = 1; i < (q?4:2); i++)
                    res.s[0] = (*minmax)(regs->v[rn].s[i], res.s[0]);
            }
            break;
        default:
            assert(0);
    }
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

static void dis_shadd_srhadd(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int size = INSN(23,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int rm = INSN(20,16);
    int is_round = INSN(12,12);
    int i;
    union simd_register res = {0};

    switch(size) {
        case 0:
            for(i = 0; i < (q?16:8); i++)
                res.b[i] = ((int8_t)regs->v[rn].b[i] + (int8_t)regs->v[rm].b[i] + (is_round?1:0)) >> 1;
            break;
        case 1:
            for(i = 0; i < (q?8:4); i++)
                res.h[i] = ((int16_t)regs->v[rn].h[i] + (int16_t)regs->v[rm].h[i] + (is_round?1:0)) >> 1;
            break;
        case 2:
            for(i = 0; i < (q?4:2); i++)
                res.s[i] = ((int64_t)(int32_t)regs->v[rn].s[i] + (int64_t)(int32_t)regs->v[rm].s[i] + (is_round?1:0)) >> 1;
            break;
        default:
            assert(0);
    }
    regs->v[rd].v = res.v;
}

static void dis_shsub(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int size = INSN(23,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int rm = INSN(20,16);
    int i;
    union simd_register res = {0};

    switch(size) {
        case 0:
            for(i = 0; i < (q?16:8); i++)
                res.b[i] = ((int8_t)regs->v[rn].b[i] - (int8_t)regs->v[rm].b[i]) >> 1;
            break;
        case 1:
            for(i = 0; i < (q?8:4); i++)
                res.h[i] = ((int16_t)regs->v[rn].h[i] - (int16_t)regs->v[rm].h[i]) >> 1;
            break;
        case 2:
            for(i = 0; i < (q?4:2); i++)
                res.s[i] = ((int64_t)(int32_t)regs->v[rn].s[i] - (int64_t)(int32_t)regs->v[rm].s[i]) >> 1;
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

static void dis_revxx(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int size = INSN(23,22);
    int op = (INSN(12,12) << 1) | INSN(29,29);
    int revmask = (1 << (3-op-size)) - 1;
    int i;
    union simd_register res;

    res.v.msb = 0;
    switch(size) {
        case 0:
            for(i = 0; i < (q?16:8); i++)
                res.b[i] = regs->v[rn].b[i ^ revmask];
            break;
        case 1:
            for(i = 0; i < (q?8:4); i++)
                res.h[i] = regs->v[rn].h[i ^ revmask];
            break;
        case 2:
            for(i = 0; i < (q?4:2); i++)
                res.s[i] = regs->v[rn].s[i ^ revmask];
            break;
        default:
            assert(0);
    }
    regs->v[rd].v = res.v;
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

static uint8_t rbit(uint8_t op) {
    op = ((op & 0x55) << 1) | ((op & ~0x55) >> 1);
    op = ((op & 0x33) << 2) | ((op & ~0x33) >> 2);
    op = ((op & 0x0f) << 4) | ((op & ~0x0f) >> 4);

    return op;
}

static void dis_not(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int rd = INSN(4,0);
    int rn = INSN(9,5);

    regs->v[rd].v.lsb = ~regs->v[rn].v.lsb;
    regs->v[rd].v.msb = q?~regs->v[rn].v.msb:0;
}

static void dis_rbit(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int i;
    union simd_register res;

    res.v.msb = 0;
    for(i = 0; i < (q?16:8); i++) {
        res.b[i] = rbit(regs->v[rn].b[i]);
    }
    regs->v[rd].v = res.v;
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



static void dis_saddlp_sadalp(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int size = INSN(23,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int is_acc = INSN(14,14);
    int i;
    union simd_register res = {0};

    if (is_acc) {
        res.v.lsb = regs->v[rd].v.lsb;
        res.v.msb = q?regs->v[rd].v.msb:0;
    }
    switch(size) {
        case 0:
            for(i = 0; i < (q?8:4); i++)
                res.h[i] += (int8_t)regs->v[rn].b[2*i] + (int8_t)regs->v[rn].b[2*i+1];
            break;
        case 1:
            for(i = 0; i < (q?4:2); i++)
                res.s[i] += (int16_t)regs->v[rn].h[2*i] + (int16_t)regs->v[rn].h[2*i+1];
            break;
        case 2:
            for(i = 0; i < (q?2:1); i++)
                res.d[i] += (int64_t)(int32_t)regs->v[rn].s[2*i] + (int64_t)(int32_t)regs->v[rn].s[2*i+1];
            break;
        default:
            assert(0);
    }
    regs->v[rd] = res;
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

static void dis_mla_mls_by_element(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int rm = INSN(19,16);
    int size = INSN(23,22);
    int H = INSN(11,11);
    int L = INSN(21,21);
    int M = INSN(20,20);
    int is_sub = INSN(14,14);
    union simd_register res;
    int index;
    int i;

    res.v.msb = 0;
    switch(size) {
        case 1:
            index = (H << 2) | (L << 1) | M;
            for(i = 0; i < (q?8:4); i++)
                if (is_sub)
                    res.h[i] = regs->v[rd].h[i] - regs->v[rn].h[i] * regs->v[rm].h[index];
                else
                    res.h[i] = regs->v[rd].h[i] + regs->v[rn].h[i] * regs->v[rm].h[index];
            break;
        case 2:
            index = (H << 1) | L;
            rm += M << 4;
            for(i = 0; i < (q?4:2); i++)
                if (is_sub)
                    res.s[i] = regs->v[rd].s[i] - regs->v[rn].s[i] * regs->v[rm].s[index];
                else
                    res.s[i] = regs->v[rd].s[i] + regs->v[rn].s[i] * regs->v[rm].s[index];
            break;
        default:
            assert(0);
    }
    regs->v[rd] = res;
}

static void dis_smlal_smlsl_by_element(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int rm = INSN(19,16);
    int size = INSN(23,22);
    int H = INSN(11,11);
    int L = INSN(21,21);
    int M = INSN(20,20);
    int is_sub = INSN(14,14);
    union simd_register res = {0};
    int index;
    int i;

    switch(size) {
        case 1:
            index = (H << 2) | (L << 1) | M;
            for(i = 0; i < 4; i++)
                if (is_sub)
                    res.s[i] = regs->v[rd].s[i] - (int16_t)regs->v[rn].h[q?4+i:i] * (int16_t)regs->v[rm].h[index];
                else
                    res.s[i] = regs->v[rd].s[i] + (int16_t)regs->v[rn].h[q?4+i:i] * (int16_t)regs->v[rm].h[index];
            break;
        case 2:
            index = (H << 1) | L;
            rm += M << 4;
            for(i = 0; i < 2; i++)
                if (is_sub)
                    res.d[i] = regs->v[rd].d[i] - (int64_t)(int32_t)regs->v[rn].s[q?2+i:i] * (int64_t)(int32_t)regs->v[rm].s[index];
                else
                    res.d[i] = regs->v[rd].d[i] + (int64_t)(int32_t)regs->v[rn].s[q?2+i:i] * (int64_t)(int32_t)regs->v[rm].s[index];
            break;
        default:
            assert(0);
    }
    regs->v[rd] = res;
}

static void dis_smull_by_element(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int rm = INSN(19,16);
    int size = INSN(23,22);
    int H = INSN(11,11);
    int L = INSN(21,21);
    int M = INSN(20,20);
    union simd_register res = {0};
    int index;
    int i;

    switch(size) {
        case 1:
            index = (H << 2) | (L << 1) | M;
            for(i = 0; i < 4; i++)
                res.s[i] = (int16_t)regs->v[rn].h[q?4+i:i] * (int16_t)regs->v[rm].h[index];
            break;
        case 2:
            index = (H << 1) | L;
            rm += M << 4;
            for(i = 0; i < 2; i++)
                res.d[i] = (int64_t)(int32_t)regs->v[rn].s[q?2+i:i] * (int64_t)(int32_t)regs->v[rm].s[index];
            break;
        default:
            assert(0);
    }
    regs->v[rd] = res;
}

static void dis_mul_by_element(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int rm = INSN(19,16);
    int size = INSN(23,22);
    int H = INSN(11,11);
    int L = INSN(21,21);
    int M = INSN(20,20);
    union simd_register res;
    int index;
    int i;

    res.v.msb = 0;
    switch(size) {
        case 1:
            index = (H << 2) | (L << 1) | M;
            for(i = 0; i < (q?8:4); i++)
                res.h[i] = regs->v[rn].h[i] * regs->v[rm].h[index];
            break;
        case 2:
            index = (H << 1) | L;
            rm += M << 4;
            for(i = 0; i < (q?4:2); i++)
                res.s[i] = regs->v[rn].s[i] * regs->v[rm].s[index];
            break;
        default:
            assert(0);
    }
    regs->v[rd] = res;
}

static void dis_sqdmlal_sqdmlsl_by_element(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int is_scalar = INSN(28,28);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int rm = INSN(19,16);
    int size = INSN(23,22);
    int H = INSN(11,11);
    int L = INSN(21,21);
    int M = INSN(20,20);
    int is_sub = INSN(14,14);
    union simd_register res = {0};
    int index;
    int i;

    switch(size) {
        case 1:
            index = (H << 2) | (L << 1) | M;
            for(i = 0; i < (is_scalar?1:4); i++) {
                int32_t product = ssat32(regs, 2 * (int16_t)regs->v[rn].h[is_scalar?i:q?i+4:i] * (int16_t)regs->v[rm].h[index]);
                if (is_sub) 
                    res.s[i] = ssat32(regs, (int64_t)(int32_t)regs->v[rd].s[i] - (int64_t)product);
                else
                    res.s[i] = ssat32(regs, (int64_t)(int32_t)regs->v[rd].s[i] + (int64_t)product);
            }
        break;
        case 2:
            index = (H << 1) | L;
            rm += M << 4;
            for(i = 0; i < (is_scalar?1:2); i++) {
                int64_t product = ssat64(regs, 2 * (int64_t)(int32_t)regs->v[rn].s[is_scalar?i:q?i+2:i] * (int64_t)(int32_t)regs->v[rm].s[index]);
                if (is_sub)
                    res.d[i] = ssat64(regs, (__int128_t)(int64_t)regs->v[rd].d[i] - (__int128_t)product);
                else
                    res.d[i] = ssat64(regs, (__int128_t)(int64_t)regs->v[rd].d[i] + (__int128_t)product);
            }
        break;
        default:
            assert(0);
    }
    regs->v[rd] = res;
}

static void dis_sqdmull_by_element(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int is_scalar = INSN(28,28);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int rm = INSN(19,16);
    int size = INSN(23,22);
    int H = INSN(11,11);
    int L = INSN(21,21);
    int M = INSN(20,20);
    union simd_register res = {0};
    int index;
    int i;

    switch(size) {
        case 1:
            index = (H << 2) | (L << 1) | M;
            for(i = 0; i < (is_scalar?1:4); i++)
                res.s[i] = ssat32(regs, 2 * (int16_t)regs->v[rn].h[is_scalar?i:q?i+4:i] * (int16_t)regs->v[rm].h[index]);
            break;
        case 2:
            index = (H << 1) | L;
            rm += M << 4;
            for(i = 0; i < (is_scalar?1:2); i++)
                res.d[i] = ssat64(regs, 2 * (__int128_t)(int32_t)regs->v[rn].s[is_scalar?i:q?i+2:i] * (__int128_t)(int32_t)regs->v[rm].s[index]);
            break;
        default:
            assert(0);
    }
    regs->v[rd] = res;
}

static void dis_sqdmulh_sqrdmulh_by_element(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int is_scalar = INSN(28,28);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int rm = INSN(19,16);
    int size = INSN(23,22);
    int H = INSN(11,11);
    int L = INSN(21,21);
    int M = INSN(20,20);
    int is_round = INSN(12,12);
    union simd_register res = {0};
    int index;
    int i;

    switch(size) {
        case 1:
            index = (H << 2) | (L << 1) | M;
            for(i = 0; i < (is_scalar?1:(q?8:4)); i++)
                res.h[i] = ssat16(regs, (2 * (int16_t)regs->v[rn].h[i] * (int16_t)regs->v[rm].h[index] + (is_round?(1<<15):0)) >> 16);
            break;
        case 2:
            index = (H << 1) | L;
            rm += M << 4;
            for(i = 0; i < (is_scalar?1:(q?4:2)); i++)
                res.s[i] = ssat32(regs, (2 * (int64_t)(int32_t)regs->v[rn].s[i] * (int64_t)(int32_t)regs->v[rm].s[index] + (is_round?(1L<<31):0)) >> 32);
            break;
        default:
            assert(0);
    }
    regs->v[rd] = res;
}

static void dis_sqdmull(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int is_scalar = INSN(28,28);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int rm = INSN(20,16);
    int size = INSN(23,22);
    int i;
    union simd_register res = {0};

    switch(size) {
        case 1:
            for(i = 0; i < (is_scalar?1:4); i++)
                res.s[i] = ssat32(regs, 2 * (int16_t)regs->v[rn].h[is_scalar?i:q?i+4:i] * (int16_t)regs->v[rm].h[is_scalar?i:q?i+4:i]);
            break;
        case 2:
            for(i = 0; i < (is_scalar?1:2); i++)
                res.d[i] = ssat64(regs, 2 * (__int128_t)(int32_t)regs->v[rn].s[is_scalar?i:q?i+2:i] * (__int128_t)(int32_t)regs->v[rm].s[is_scalar?i:q?i+2:i]);
            break;
        default:
            assert(0);
    }

    regs->v[rd] = res;
}

/* FIXME: need address shift if offset active */
static int dis_load_multiple_structure(uint64_t _regs, uint32_t insn, uint64_t address)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int opcode = INSN(15,12);
    int rt = INSN(4,0);
    int size = INSN(11,10);
    int res = 0;

    if (opcode & 2) {
        //ld1
        int i;
        int nb;
        switch(opcode) {
            case 7: nb = 1; break;
            case 10: nb = 2; break;
            case 6: nb = 3; break;
            case 2: nb = 4; break;
            default: assert(0);
        }
        for(i = 0; i < nb; i++) {
            regs->v[(rt + i) % 32].v.lsb = *((uint64_t *) (address + i * (q?16:8)));
            regs->v[(rt + i) % 32].v.msb = q?*((uint64_t *) (address + i * 16 + 8)):0;
        }
        res = 8 * (1 + q) * nb;
    } else {
        int index;
        int r;
        int nb;

        switch(opcode) {
            case 8: nb=2; break;
            case 4: nb=3; break;
            case 0: nb=4; break;
            default: assert(0);
        }

        switch(size) {
            case 0:
                for(index = 0; index < (q?16:8); index++) {
                    for(r = 0; r < nb; r++) {
                        regs->v[(rt + r) % 32].b[index] = *((uint8_t *)(address + res));
                        res += 1;
                    }
                }
                break;
            case 1:
                for(index = 0; index < (q?8:4); index++) {
                    for(r = 0; r < nb; r++) {
                        regs->v[(rt + r) % 32].h[index] = *((uint16_t *)(address + res));
                        res += 2;
                    }
                }
                break;
            case 2:
                for(index = 0; index < (q?4:2); index++) {
                    for(r = 0; r < nb; r++) {
                        regs->v[(rt + r) % 32].s[index] = *((uint32_t *)(address + res));
                        res += 4;
                    }
                }
                break;
            case 3:
                for(index = 0; index < (q?2:1); index++) {
                    for(r = 0; r < nb; r++) {
                        regs->v[(rt + r) % 32].d[index] = *((uint64_t *)(address + res));
                        res += 8;
                    }
                }
                break;
        }
        if (q == 0)
            for(r = 0; r < nb; r++)
                regs->v[(rt + r) % 32].v.msb = 0;
    }

    return res;
}

static int dis_store_multiple_structure(uint64_t _regs, uint32_t insn, uint64_t address)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int opcode = INSN(15,12);
    int rt = INSN(4,0);
    int size = INSN(11,10);
    int res = 0;

    if (opcode & 2) {
        //st1
        int i;
        int nb;
        switch(opcode) {
            case 7: nb = 1; break;
            case 10: nb = 2; break;
            case 6: nb = 3; break;
            case 2: nb = 4; break;
            default: assert(0);
        }
        for(i = 0; i < nb; i++) {
            *((uint64_t *) (address + i * (q?16:8))) = regs->v[(rt + i) % 32].v.lsb;
            if (q)
                *((uint64_t *) (address + i * 16 + 8)) = regs->v[(rt + i) % 32].v.msb;
        }
        res = 8 * (1 + q) * nb;
    } else {
        int index;
        int r;
        int nb;

        switch(opcode) {
            case 8: nb=2; break;
            case 4: nb=3; break;
            case 0: nb=4; break;
            default: assert(0);
        }

        switch(size) {
            case 0:
                for(index = 0; index < (q?16:8); index++) {
                    for(r = 0; r < nb; r++) {
                        *((uint8_t *)(address + res)) = regs->v[(rt + r) % 32].b[index];
                        res += 1;
                    }
                }
                break;
            case 1:
                for(index = 0; index < (q?8:4); index++) {
                    for(r = 0; r < nb; r++) {
                        *((uint16_t *)(address + res)) = regs->v[(rt + r) % 32].h[index];
                        res += 2;
                    }
                }
                break;
            case 2:
                for(index = 0; index < (q?4:2); index++) {
                    for(r = 0; r < nb; r++) {
                        *((uint32_t *)(address + res)) = regs->v[(rt + r) % 32].s[index];
                        res += 4;
                    }
                }
                break;
            case 3:
                for(index = 0; index < (q?2:1); index++) {
                    for(r = 0; r < nb; r++) {
                        *((uint64_t *)(address + res)) = regs->v[(rt + r) % 32].d[index];
                        res += 8;
                    }
                }
                break;
        }
    }

    return res;
}

static int dis_load_single_structure_replicate(uint64_t _regs, uint32_t insn, uint64_t address)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int size = INSN(11,10);
    int rt = INSN(4,0);
    int selem = (INSN(13,13) << 1) + INSN(21,21) + 1;
    int res = 0;
    int i;
    int s;

    for(s = 0; s < selem; s++) {
        if (q == 0)
            regs->v[(rt + s) % 32].v.msb = 0;
        switch(size) {
            case 0:
                for(i = 0; i < (q?16:8); i++)
                    regs->v[(rt + s) % 32].b[i] = *((uint8_t *)(address + res));
                res += 1;
                break;
            case 1:
                for(i = 0; i < (q?8:4); i++)
                    regs->v[(rt + s) % 32].h[i] = *((uint16_t *)(address + res));
                res += 2;
                break;
            case 2:
                for(i = 0; i < (q?4:2); i++)
                    regs->v[(rt + s) % 32].s[i] = *((uint32_t *)(address + res));
                res += 4;
                break;
            case 3:
                for(i = 0; i < (q?2:1); i++)
                    regs->v[(rt + s) % 32].d[i] = *((uint64_t *)(address + res));
                res += 8;
                break;
        }
    }

    return res;
}

static int dis_store_single_structure_replicate(uint64_t _regs, uint32_t insn, uint64_t address)
{
    assert(0);
}

static int dis_load_single_structure(uint64_t _regs, uint32_t insn, uint64_t address)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int S = INSN(12,12);
    int size = INSN(11,10);
    int opcode_2_1 = INSN(15,14);
    int rt = INSN(4,0);
    int selem = (INSN(13,13) << 1) + INSN(21,21) + 1;
    int res = 0;
    int index;
    int s;

    for(s = 0; s < selem; s++) {
        if (opcode_2_1 == 0) {
            index = (q << 3) | (S << 2) | size;
            regs->v[(rt + s) % 32].b[index] = *((uint8_t *)(address + res));
            res += 1;
        } else if (opcode_2_1 == 1) {
            index = (q << 2) | (S << 1) | (size >> 1);
            regs->v[(rt + s) % 32].h[index] = *((uint16_t *)(address + res));
            res += 2;
        } else if (opcode_2_1 == 2) {
            if (size == 0) {
                index = (q << 1) | (S << 0);
                regs->v[(rt + s) % 32].s[index] = *((uint32_t *)(address + res));
                res += 4;
            } else {
                index = q;
                regs->v[(rt + s) % 32].d[index] = *((uint64_t *)(address + res));
                res += 8;
            }
        } else
            assert(0);
    }

    return res;
}

static int dis_store_single_structure(uint64_t _regs, uint32_t insn, uint64_t address)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int S = INSN(12,12);
    int size = INSN(11,10);
    int opcode_2_1 = INSN(15,14);
    int rt = INSN(4,0);
    int selem = (INSN(13,13) << 1) + INSN(21,21) + 1;
    int res = 0;
    int index;
    int s;

    for(s = 0; s < selem; s++) {
        if (opcode_2_1 == 0) {
            index = (q << 3) | (S << 2) | size;
            *((uint8_t *)(address + res)) = regs->v[(rt + s) % 32].b[index];
            res += 1;
        } else if (opcode_2_1 == 1) {
            index = (q << 2) | (S << 1) | (size >> 1);
            *((uint16_t *)(address + res)) = regs->v[(rt + s) % 32].h[index];
            res += 2;
        } else if (opcode_2_1 == 2) {
            if (size == 0) {
                index = (q << 1) | (S << 0);
                *((uint32_t *)(address + res)) = regs->v[(rt + s) % 32].s[index];
                res += 4;
            } else {
                index = q;
                *((uint64_t *)(address + res)) = regs->v[(rt + s) % 32].d[index];
                res += 8;
            }
        } else
            assert(0);
    }

    return res;
}

static void dis_trn1_trn2(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int rm = INSN(20,16);
    int size = INSN(23,22);
    union simd_register res = {0};
    int part = INSN(14,14);
    int i;

    switch(size) {
        case 0:
            for(i = 0; i < (q?8:4); i++) {
                res.b[2*i+0] = regs->v[rn].b[2*i+part];
                res.b[2*i+1] = regs->v[rm].b[2*i+part];
            }
            break;
        case 1:
            for(i = 0; i < (q?4:2); i++) {
                res.h[2*i+0] = regs->v[rn].h[2*i+part];
                res.h[2*i+1] = regs->v[rm].h[2*i+part];
            }
            break;
        case 2:
            for(i = 0; i < (q?2:1); i++) {
                res.s[2*i+0] = regs->v[rn].s[2*i+part];
                res.s[2*i+1] = regs->v[rm].s[2*i+part];
            }
            break;
        case 3:
            assert(q);
            for(i = 0; i < 1; i++) {
                res.d[2*i+0] = regs->v[rn].d[2*i+part];
                res.d[2*i+1] = regs->v[rm].d[2*i+part];
            }
            break;
    }
    regs->v[rd] = res;    
}

/* simd deasm */
void arm64_hlp_dirty_advanced_simd_two_reg_misc_simd(uint64_t _regs, uint32_t insn)
{
    int U = INSN(29,29);
    int opcode = INSN(16,12);
    int size = INSN(23,22);

    switch(opcode) {
        case 0:
            dis_revxx(_regs, insn);
            break;
        case 1:
            assert(U==0);
            dis_revxx(_regs, insn);
            break;
        case 2:
            if (U)
                assert(0);
            else
                dis_saddlp_sadalp(_regs, insn);
            break;
        case 3:
            U?assert(0):dis_suqadd(_regs, insn);
            break;
        case 4:
            dis_cls_clz(_regs, insn);
            break;
        case 5:
            if (U) {
                if (size == 0)
                    dis_not(_regs, insn);
                else if (size == 1)
                    dis_rbit(_regs, insn);
                else
                    fatal("size = %d\n", size);
            } else
                dis_cnt(_regs, insn);
            break;
        case 6:
            if (U)
                assert(0);
            else
                dis_saddlp_sadalp(_regs, insn);
            break;
        case 7:
            U?dis_sqneg(_regs, insn):dis_sqabs(_regs, insn);
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
                dis_neg(_regs, insn);
            else
                dis_abs(_regs, insn);
            break;
        case 18:
            U?dis_sqxtun(_regs, insn):assert(0);
            break;
        case 19:
            if (U)
                dis_shll(_regs, insn);
            else
                assert(0);
            break;
        case 20:
            U?assert(0):dis_sqxtn(_regs, insn);
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
        case 3:
            U?assert(0):dis_suqadd(_regs, insn);
            break;
        case 7:
            if (U)
                dis_sqneg(_regs, insn);
            else
                dis_sqabs(_regs, insn);
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
                dis_neg(_regs, insn);
            else
                dis_abs(_regs, insn);
            break;
        case 18:
            U?dis_sqxtun(_regs, insn):assert(0);
            break;
        case 20:
            U?assert(0):dis_sqxtn(_regs, insn);
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
        case 1:
            if (U)
                assert(0);
            else
                dis_sqadd(_regs, insn);
            break;
        case 5:
            U?assert(0):dis_sqsub(_regs, insn);
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
        case 11:
            U?assert(0):dis_sqrshl(_regs, insn);
            break;
        case 16:
            dis_add_sub(_regs, insn);
            break;
        case 17:
            if (U)
                dis_cmeq(_regs, insn);
            else
                dis_cmtst(_regs, insn);
            break;
        case 22:
            dis_sqdmulh_sqrdmulh(_regs, insn);
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
        case 0:
            if (U)
                assert(0);
            else
                dis_shadd_srhadd(_regs, insn);
            break;
        case 1:
            U?assert(0):dis_sqadd(_regs, insn);
            break;
        case 2:
            U?assert(0):dis_shadd_srhadd(_regs, insn);
            break;
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
                    dis_orn(_regs, insn);
            }
            break;
        case 4:
            if (U)
                assert(0);
            else
                dis_shsub(_regs, insn);
            break;
        case 5:
            U?assert(0):dis_sqsub(_regs, insn);
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
        case 12:
            if (U)
                assert(0);
            else
                dis_smax_smin(_regs, insn);
            break;
        case 13:
            if (U)
                assert(0);
            else
                dis_smax_smin(_regs, insn);
            break;
        case 14:
            if (U)
                assert(0);
            else
                dis_sabd_saba(_regs, insn);
            break;
        case 15:
            if (U)
                assert(0);
            else
                dis_sabd_saba(_regs, insn);
            break;
        case 16:
            dis_add_sub(_regs, insn);
            break;
        case 17:
            if (U)
                dis_cmeq(_regs, insn);
            else
                dis_cmtst(_regs, insn);
            break;
        case 18:
            dis_mal_mls(_regs, insn);
            break;
        case 19:
            if (U)
                dis_pmul(_regs, insn);
            else
                dis_mul(_regs, insn);
            break;
        case 20:
            if (U)
                assert(0);
            else
                dis_smaxp_sminp(_regs, insn);
            break;
        case 21:
            if (U)
                assert(0);
            else
                dis_smaxp_sminp(_regs, insn);
            break;
        case 22:
            dis_sqdmulh_sqrdmulh(_regs, insn);
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
        case 0:
            if (U)
                assert(0);
            else
                dis_saddl_ssubl(_regs, insn);
            break;
        case 1:
            if (U)
                assert(0);
            else
                dis_saddw_ssubw(_regs, insn);
            break;
        case 2:
            U?assert(0):dis_saddl_ssubl(_regs, insn);
            break;
        case 3:
            U?assert(0):dis_saddw_ssubw(_regs, insn);
            break;
        case 4: case 6:
            dis_addhn_raddhn_subhn_rsubhn(_regs, insn);
            break;
        case 5:
            if (U)
                assert(0);
            else
                dis_sabdl_sabal(_regs, insn);
            break;
        case 7:
            if (U)
                assert(0);
            else
                dis_sabdl_sabal(_regs, insn);
            break;
        case 8:
            if (U)
                assert(0);
            else
                dis_smlal_smlsl(_regs, insn);
            break;
        case 9:
            U?assert(0):dis_sqdmlal_sqdmlsl(_regs, insn);
            break;
        case 10:
            if (U)
                assert(0);
            else
                dis_smlal_smlsl(_regs, insn);
            break;
        case 11:
            U?assert(0):dis_sqdmlal_sqdmlsl(_regs, insn);
            break;
        case 12:
            if (U)
                assert(0);
            else
                dis_smull(_regs, insn);
            break;
        case 13:
            U?assert(0):dis_sqdmull(_regs, insn);
            break;
        case 14:
            assert(U==0);
            dis_pmull(_regs, insn);
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
        case 3:
            if (U)
                assert(0);
            else
                dis_saddlv(_regs, insn);
            break;
        case 10:
            if (U)
                assert(0);
            else
                dis_smaxv_sminv(_regs, insn);
            break;
        case 26:
            if (U)
                assert(0);
            else
                dis_smaxv_sminv(_regs, insn);
            break;
        case 27:
            assert(U==0);
            dis_addv(_regs, insn);
            break;
        default:
            fatal("opcode = %d(0x%x) / U=%d\n", opcode, opcode, U);
    }
}

void arm64_hlp_dirty_advanced_simd_vector_x_indexed_element_simd(uint64_t _regs, uint32_t insn)
{
    int U = INSN(29,29);
    int opcode = INSN(15,12);

    switch(opcode) {
        case 0:
        case 4:
            assert(U);
            dis_mla_mls_by_element(_regs, insn);
            break;
        case 2:
            if (U)
                assert(0);
            else
                dis_smlal_smlsl_by_element(_regs, insn);
            break;
        case 3:
            U?assert(0):dis_sqdmlal_sqdmlsl_by_element(_regs, insn);
            break;
        case 6:
            if (U)
                assert(0);
            else
                dis_smlal_smlsl_by_element(_regs, insn);
            break;
        case 7:
            U?assert(0):dis_sqdmlal_sqdmlsl_by_element(_regs, insn);
            break;
        case 8:
            assert(U == 0);
            dis_mul_by_element(_regs, insn);
            break;
        case 10:
            if (U)
                assert(0);
            else
                dis_smull_by_element(_regs, insn);
            break;
        case 11:
            U?assert(0):dis_sqdmull_by_element(_regs, insn);
            break;
        case 12:
            U?assert(0):dis_sqdmulh_sqrdmulh_by_element(_regs, insn);
            break;
        case 13:
            U?assert(0):dis_sqdmulh_sqrdmulh_by_element(_regs, insn);
            break;
        default:
            fatal("opcode = %d(0x%x) / U=%d\n", opcode, opcode, U);
    }
}

void arm64_hlp_dirty_advanced_simd_scalar_shift_by_immediate_simd(uint64_t _regs, uint32_t insn)
{
    int U = INSN(29,29);
    int opcode = INSN(15,11);

    switch(opcode) {
        case 0:
            U?assert(0):dis_srshr_srsra_sshr_ssra(_regs, insn);
            break;
        case 2:
            U?assert(0):dis_srshr_srsra_sshr_ssra(_regs, insn);
            break;
        case 4:
            U?assert(0):dis_srshr_srsra_sshr_ssra(_regs, insn);
            break;
        case 6:
            U?assert(0):dis_srshr_srsra_sshr_ssra(_regs, insn);
            break;
        case 8:
            U?dis_sri(_regs, insn):assert(0);
            break;
        case 10:
            if (U)
                dis_sli(_regs, insn);
            else
                dis_shl(_regs, insn);
            break;
        case 12:
            U?dis_sqshlu(_regs, insn):assert(0);
            break;
        case 16:
            U?dis_sqrshrun_sqshrun(_regs, insn):assert(0);
            break;
        case 17:
            U?dis_sqrshrun_sqshrun(_regs, insn):assert(0);
            break;
        case 18:
            U?assert(0):dis_sqrshrn_sqshrn(_regs, insn);
        case 19:
            U?assert(0):dis_sqrshrn_sqshrn(_regs, insn);
            break;
        default:
            fatal("opcode = %d(0x%x) / U=%d\n", opcode, opcode, U);
    }
}

void arm64_hlp_dirty_advanced_simd_scalar_x_indexed_element_simd(uint64_t _regs, uint32_t insn)
{
    int U = INSN(29,29);
    int opcode = INSN(15,12);

    switch(opcode) {
        case 3:
            U?assert(0):dis_sqdmlal_sqdmlsl_by_element(_regs, insn);
            break;
        case 7:
            U?assert(0):dis_sqdmlal_sqdmlsl_by_element(_regs, insn);
            break;
        case 11:
            U?assert(0):dis_sqdmull_by_element(_regs, insn);
            break;
        case 12:
            U?assert(0):dis_sqdmulh_sqrdmulh_by_element(_regs, insn);
            break;
        case 13:
            U?assert(0):dis_sqdmulh_sqrdmulh_by_element(_regs, insn);
            break;
        default:
            fatal("opcode = %d(0x%x) / U=%d\n", opcode, opcode, U);
    }
}

void arm64_hlp_dirty_advanced_simd_scalar_three_different_simd(uint64_t _regs, uint32_t insn)
{
    int opcode = INSN(15,12);

    switch(opcode) {
        case 13:
            dis_sqdmull(_regs, insn);
            break;
        default:
            fatal("opcode = %d(0x%x)\n", opcode, opcode);
    }
}

void arm64_hlp_dirty_advanced_simd_load_store_multiple_structure_simd(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int l = INSN(22,22);
    int rn = INSN(9,5);

    if (l)
        dis_load_multiple_structure(_regs, insn, regs->r[rn]);
    else
        dis_store_multiple_structure(_regs, insn, regs->r[rn]);
}

void arm64_hlp_dirty_advanced_simd_load_store_multiple_structure_post_index_simd(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int l = INSN(22,22);
    int rn = INSN(9,5);
    int rm = INSN(20,16);
    int offset;

    if (l)
        offset = dis_load_multiple_structure(_regs, insn, regs->r[rn]);
    else
        offset = dis_store_multiple_structure(_regs, insn, regs->r[rn]);

    //writeback
    if (rm == 31)
        regs->r[rn] += offset;
    else
        regs->r[rn] += regs->r[rm];
}

void arm64_hlp_dirty_advanced_simd_load_store_single_structure_simd(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int l = INSN(22,22);
    int rn = INSN(9,5);
    int is_replicate = (INSN(15,14) == 3);

    if (is_replicate) {
        if (l)
            dis_load_single_structure_replicate(_regs, insn, regs->r[rn]);
        else
            dis_store_single_structure_replicate(_regs, insn, regs->r[rn]);
    } else {
        if (l)
            dis_load_single_structure(_regs, insn, regs->r[rn]);
        else
            dis_store_single_structure(_regs, insn, regs->r[rn]);
    }
}

void arm64_hlp_dirty_advanced_simd_load_store_single_structure_post_index_simd(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int l = INSN(22,22);
    int rn = INSN(9,5);
    int rm = INSN(20,16);
    int is_replicate = (INSN(15,14) == 3);
    int offset;

    if (is_replicate) {
        if (l)
            offset = dis_load_single_structure_replicate(_regs, insn, regs->r[rn]);
        else
            offset = dis_store_single_structure_replicate(_regs, insn, regs->r[rn]);
    } else {
        if (l)
            offset = dis_load_single_structure(_regs, insn, regs->r[rn]);
        else
            offset = dis_store_single_structure(_regs, insn, regs->r[rn]);
    }

    //writeback
    if (rm == 31)
        regs->r[rn] += offset;
    else
        regs->r[rn] += regs->r[rm];
}

void arm64_hlp_dirty_advanced_simd_table_lookup_simd(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int len = INSN(14,13);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int rm = INSN(20,16);
    int is_tbx = INSN(12,12);
    union simd_register res = {0};
    int i;

    if (is_tbx) {
        res.v.lsb = regs->v[rd].v.lsb;
        if (q)
            res.v.msb = regs->v[rd].v.msb;
    }
    for(i = 0; i < (q?16:8); i++) {
        int index = regs->v[rm].b[i];

        if (index < 16 * (len + 1)) {
            int r = index / 16;
            int idx = index % 16;

            res.b[i] = regs->v[(rn + r) % 32].b[idx];
        }
    }

    regs->v[rd] = res;
}

void arm64_hlp_dirty_advanced_simd_permute_simd(uint64_t _regs, uint32_t insn)
{
    int opcode = INSN(14,12);

    switch(opcode) {
        case 2: case 6:
            dis_trn1_trn2(_regs, insn);
            break;
        default:
            fatal("opcode = %d(0x%x)\n", opcode, opcode);
    }
}
