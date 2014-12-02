#include <stdio.h>
#include <assert.h>
#include <math.h>

#include "arm64_private.h"
#include "arm64_helpers.h"
#include "runtime.h"
#include "softfloat.h"

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

#define DECLARE_USAT_U(size,max) \
static inline uint##size##_t usat##size##_u(struct arm64_registers *regs, uint64_t op) \
{ \
    uint##size##_t res; \
 \
    if (op > max) { \
        res = max; \
        regs->fpsr |= 1 << 27; \
    } else \
        res = op; \
 \
    return res; \
}
DECLARE_USAT_U(8,0xff)
DECLARE_USAT_U(16,0xffff)
DECLARE_USAT_U(32,0xffffffff)
static inline uint64_t usat64_u(struct arm64_registers *regs, __uint128_t op)
{
    uint64_t res;
    __int128_t max = 0xffffffffffffffffUL;

    if (op > max) {
        res = max;
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

#include "arm64_helpers_simd_fpu_common.c"

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

static void dis_sqshl(uint64_t _regs, uint32_t insn)
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
            res.b[i] = ssat8(regs, (int8_t)regs->v[rn].b[i] << shift);
    } else if (immh < 4) {
        shift = imm - 16;
        for(i = 0; i < (is_scalar?1:(q?8:4)); i++)
            res.h[i] = ssat16(regs, (int16_t)regs->v[rn].h[i] << shift);
    } else if (immh < 8) {
        shift = imm - 32;
        for(i = 0; i < (is_scalar?1:(q?4:2)); i++)
            res.s[i] = ssat32(regs, (int64_t)(int32_t)regs->v[rn].s[i] << shift);
    } else {
        shift = imm - 64;
        for(i = 0; i < (is_scalar?1:2); i++)
            res.d[i] = ssat64(regs, (__int128_t)(int64_t)regs->v[rn].d[i] << shift);
    }
    regs->v[rd] = res;
}

static void dis_uqshl(uint64_t _regs, uint32_t insn)
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
            res.b[i] = usat8(regs, regs->v[rn].b[i] << shift);
    } else if (immh < 4) {
        shift = imm - 16;
        for(i = 0; i < (is_scalar?1:(q?8:4)); i++)
            res.h[i] = usat16(regs, regs->v[rn].h[i] << shift);
    } else if (immh < 8) {
        shift = imm - 32;
        for(i = 0; i < (is_scalar?1:(q?4:2)); i++)
            res.s[i] = usat32(regs, (uint64_t)regs->v[rn].s[i] << shift);
    } else {
        shift = imm - 64;
        for(i = 0; i < (is_scalar?1:2); i++)
            res.d[i] = usat64(regs, (__uint128_t)regs->v[rn].d[i] << shift);
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

static void dis_sqrshrn_sqshrn_uqrshrn_uqshrn(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int U = INSN(29,29);
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
            if (U)
                res.b[is_scalar?i:(q?i+8:i)] = usat8(regs, (regs->v[rn].h[i] + (is_round?(1UL<<(shift-1)):0)) >> shift);
            else
                res.b[is_scalar?i:(q?i+8:i)] = ssat8(regs, ((int16_t)regs->v[rn].h[i] + (is_round?(1L<<(shift-1)):0)) >> shift);
    } else if (immh < 4) {
        shift = 32 - imm;
        for(i = 0; i < (is_scalar?1:4); i++)
            if (U)
                res.h[is_scalar?i:(q?i+4:i)] = usat16(regs, ((uint64_t)regs->v[rn].s[i] + (is_round?(1UL<<(shift-1)):0)) >> shift);
            else
                res.h[is_scalar?i:(q?i+4:i)] = ssat16(regs, ((int64_t)(int32_t)regs->v[rn].s[i] + (is_round?(1L<<(shift-1)):0)) >> shift);
    } else if (immh < 8) {
        shift = 64 -imm;
        for(i = 0; i < (is_scalar?1:2); i++)
            if (U)
                res.s[is_scalar?i:(q?i+2:i)] = usat32(regs, ((__uint128_t)regs->v[rn].d[i] + (is_round?(1UL<<(shift-1)):0)) >> shift);
            else
                res.s[is_scalar?i:(q?i+2:i)] = ssat32(regs, ((__int128_t)(int64_t)regs->v[rn].d[i] + (is_round?(1L<<(shift-1)):0)) >> shift);
    } else
        assert(0);
    regs->v[rd] = res;
}

/* FIXME: add fpcr rounding mode */
static void dis_ucvtf_fixed(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int is_double = INSN(22,22);
    int is_scalar = INSN(28,28);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int fracbits = (1 + is_double) * 64 - INSN(22,16);
    union simd_register res = {0};
    int i;

    if (is_double) {
        assert(q);
        for(i = 0; i < (is_scalar?1:2); i++)
            res.df[i] = (double)(uint64_t)regs->v[rn].d[i] / (1UL << fracbits);
    } else {
        for(i = 0; i < (is_scalar?1:(q?4:2)); i++)
        res.sf[i] = (double)(uint32_t)regs->v[rn].s[i] / (1UL << fracbits);
    }

    regs->v[rd] = res;
}

/* FIXME: add fpcr rounding mode */
static void dis_scvtf_fixed(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int is_double = INSN(22,22);
    int is_scalar = INSN(28,28);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int fracbits = (1 + is_double) * 64 - INSN(22,16);
    union simd_register res = {0};
    int i;

    if (is_double) {
        assert(q);
        for(i = 0; i < (is_scalar?1:2); i++)
            res.df[i] = (double)(int64_t)regs->v[rn].d[i] / (1UL << fracbits);
    } else {
        for(i = 0; i < (is_scalar?1:(q?4:2)); i++)
        res.sf[i] = (double)(int32_t)regs->v[rn].s[i] / (1UL << fracbits);
    }

    regs->v[rd] = res;
}

static void dis_fcvtzu_fixed(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int is_double = INSN(22,22);
    int is_scalar = INSN(28,28);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int fracbits = (1 + is_double) * 64 - INSN(22,16);
    union simd_register res = {0};
    int i;

    if (is_double) {
        assert(q);
        for(i = 0; i < (is_scalar?1:2); i++)
            res.d[i] = (uint64_t) usat64_d(fcvt_fract(regs->v[rn].df[i], fracbits));
    } else {
        for(i = 0; i < (is_scalar?1:(q?4:2)); i++)
            res.s[i] = (uint32_t) usat32_d(fcvt_fract(regs->v[rn].sf[i], fracbits));
    }

    regs->v[rd] = res;
}

static void dis_fcvtzs_fixed(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int is_double = INSN(22,22);
    int is_scalar = INSN(28,28);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int fracbits = (1 + is_double) * 64 - INSN(22,16);
    union simd_register res = {0};
    int i;

    if (is_double) {
        assert(q);
        for(i = 0; i < (is_scalar?1:2); i++)
            res.d[i] = (int64_t) ssat64_d(fcvt_fract(regs->v[rn].df[i], fracbits));
    } else {
        for(i = 0; i < (is_scalar?1:(q?4:2)); i++)
            res.s[i] = (int32_t) ssat32_d(fcvt_fract(regs->v[rn].sf[i], fracbits));
    }

    regs->v[rd] = res;
}

static void dis_sshll_ushll(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int U = INSN(29,29);
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
            if (U)
                res.h[i] = regs->v[rn].b[q?i+8:i] << shift;
            else
                res.h[i] = (int8_t)regs->v[rn].b[q?i+8:i] << shift;
    } else if (immh < 4) {
        shift = imm - 16;
        for(i = 0; i < 4; i++)
            if (U)
                res.s[i] = regs->v[rn].h[q?i+4:i] << shift;
            else
                res.s[i] = (int16_t)regs->v[rn].h[q?i+4:i] << shift;
    } else if (immh < 8) {
        shift = imm - 32;
        for(i = 0; i < 2; i++)
            if (U)
                res.d[i] = (uint64_t)regs->v[rn].s[q?i+2:i] << shift;
            else
                res.d[i] = (int64_t)(int32_t)regs->v[rn].s[q?i+2:i] << shift;
    } else
        assert(0);

    regs->v[rd] = res;
}

static void dis_srshr_srsra_sshr_ssra_urshr_ursra_ushr_usra(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int U = INSN(29,29);
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
            if (U)
                res.b[i] = ((regs->v[rn].b[i] + (is_round?(1UL<<(shift-1)):0)) >> shift) + (is_acc?regs->v[rd].b[i]:0);
            else
                res.b[i] = (((int8_t)regs->v[rn].b[i] + (is_round?(1L<<(shift-1)):0)) >> shift) + (is_acc?regs->v[rd].b[i]:0);
    } else if (immh < 4) {
        shift = 32 - imm;
        for(i = 0; i < (q?8:4); i++)
            if (U)
                res.h[i] = ((regs->v[rn].h[i] + (is_round?(1UL<<(shift-1)):0)) >> shift) + (is_acc?regs->v[rd].h[i]:0);
            else
                res.h[i] = (((int16_t)regs->v[rn].h[i] + (is_round?(1L<<(shift-1)):0)) >> shift) + (is_acc?regs->v[rd].h[i]:0);
    } else if (immh < 8) {
        shift = 64 - imm;
        for(i = 0; i < (q?4:2); i++)
            if (U)
                res.s[i] = ((regs->v[rn].s[i] + (is_round?(1UL<<(shift-1)):0)) >> shift) + (is_acc?regs->v[rd].s[i]:0);
            else
                res.s[i] = (((int32_t)regs->v[rn].s[i] + (is_round?(1L<<(shift-1)):0)) >> shift) + (is_acc?regs->v[rd].s[i]:0);
    } else {
        shift = 128 - imm;
        /* since we use 64 bits arithmetic, we need to handle shift by 64 in specific case */
        for(i = 0; i < (is_scalar?1:2); i++)
            if (shift == 64)
                if (U)
                    res.d[i] = (is_acc?regs->v[rd].d[i]:0);
                else
                    res.d[i] = (((int64_t)regs->v[rn].d[i] + (is_round?(1L<<(shift-1)):0)) >> 63) + (is_acc?regs->v[rd].d[i]:0);
            else
                if (U)
                    res.d[i] = (((uint64_t)regs->v[rn].d[i] + (is_round?(1UL<<(shift-1)):0)) >> shift) + (is_acc?regs->v[rd].d[i]:0);
                else
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
            dis_srshr_srsra_sshr_ssra_urshr_ursra_ushr_usra(_regs, insn);
            break;
        case 2:
            dis_srshr_srsra_sshr_ssra_urshr_ursra_ushr_usra(_regs, insn);
            break;
        case 4:
            dis_srshr_srsra_sshr_ssra_urshr_ursra_ushr_usra(_regs, insn);
            break;
        case 6:
            dis_srshr_srsra_sshr_ssra_urshr_ursra_ushr_usra(_regs, insn);
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
        case 14:
            U?dis_uqshl(_regs, insn):dis_sqshl(_regs, insn);
            break;
        case 16:
            U?dis_sqrshrun_sqshrun(_regs, insn):dis_shrn_rshrn(_regs, insn);
            break;
        case 17:
            U?dis_sqrshrun_sqshrun(_regs, insn):dis_shrn_rshrn(_regs, insn);
            break;
        case 18:
            dis_sqrshrn_sqshrn_uqrshrn_uqshrn(_regs, insn);
        case 19:
            dis_sqrshrn_sqshrn_uqrshrn_uqshrn(_regs, insn);
            break;
        case 20:
            dis_sshll_ushll(_regs, insn);
            break;
        case 28:
            U?dis_ucvtf_fixed(_regs, insn):dis_scvtf_fixed(_regs, insn);
            break;
        case 31:
            U?dis_fcvtzu_fixed(_regs, insn):dis_fcvtzs_fixed(_regs, insn);
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

static void dis_xtn(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int size = INSN(23,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int i;
    union simd_register res = {0};

    if (q)
        res.v.lsb = regs->v[rd].v.lsb;
    switch(size) {
        case 0:
            for(i = 0; i < 8; i++)
                res.b[q?i+8:i] = regs->v[rn].h[i];
            break;
        case 1:
            for(i = 0; i < 4; i++)
                res.h[q?i+4:i] = regs->v[rn].s[i];
            break;
        case 2:
            for(i = 0; i < 2; i++)
                res.s[q?i+2:i] = regs->v[rn].d[i];
            break;
        default:
            assert(0);
    }
    regs->v[rd] = res;
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

static void dis_sqxtn_uqxtn(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int is_scalar = INSN(28,28);
    int U = INSN(29,29);
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
                if (U)
                    res.b[is_scalar?i:q?i+8:i] = usat8(regs, regs->v[rn].h[i]);
                else
                    res.b[is_scalar?i:q?i+8:i] = ssat8(regs, (int16_t)regs->v[rn].h[i]);
            break;
        case 1:
            for(i = 0; i < (is_scalar?1:4); i++)
                if (U)
                    res.h[is_scalar?i:q?i+4:i] = usat16(regs, regs->v[rn].s[i]);
                else
                    res.h[is_scalar?i:q?i+4:i] = ssat16(regs, (int32_t)regs->v[rn].s[i]);
            break;
        case 2:
            for(i = 0; i < (is_scalar?1:2); i++)
                if (U)
                    res.s[is_scalar?i:q?i+2:i] = usat32_u(regs, (uint64_t)regs->v[rn].d[i]);
                else
                    res.s[is_scalar?i:q?i+2:i] = ssat32(regs, (int64_t)regs->v[rn].d[i]);
            break;
        default:
            assert(0);
    }

    regs->v[rd] = res;
}

static void dis_fcvtxn(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int is_scalar = INSN(28,28);
    int q = INSN(30,30);
    int is_double = INSN(22,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int i;
    union simd_register res = {0};

    if (!is_scalar)
        res.v.lsb = regs->v[rd].v.lsb;
    if (is_double) {
        for(i = 0; i < (is_scalar?1:2); i++) {
            float_status dummy = {0};
            dummy.float_rounding_mode = float_round_to_zero;
            float32 r = float64_to_float32(regs->v[rn].d[i], &dummy);
            /* FIXME: lsb bit should be set only when result is inexact */
            res.s[is_scalar?i:(q?2+i:i)] = float32_val(r) | 1;
        }
    } else {
        assert(0);
    }

    regs->v[rd] = res;
}

/* FIXME: use fpcr rounding mode */
static void dis_fcvtn(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int is_double = INSN(22,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int i;
    union simd_register res = {0};

    if (q)
        res.v.lsb = regs->v[rd].v.lsb;
    if (is_double) {
        for(i = 0; i < 2; i++) {
            res.sf[q?2+i:i] = regs->v[rn].df[i];
        }
    } else {
        for(i = 0; i < 4; i++) {
            float_status dummy = {0};
            float16 r = float32_to_float16(regs->v[rn].s[i], 1, &dummy);
            res.h[q?4+i:i] = float16_val(r);
        }
    }

    regs->v[rd] = res;
}

/* FIXME: use fpcr rounding mode */
static void dis_fcvtl(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int is_double = INSN(22,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int i;
    union simd_register res = {0};

    if (is_double) {
        for(i = 0; i < 2; i++) {
            res.df[i] = regs->v[rn].sf[q?2+i:i];;
        }
    } else {
        for(i = 0; i < 4; i++) {
            float_status dummy = {0};
            float32 r = float16_to_float32(regs->v[rn].h[q?4+i:i], 1, &dummy);
            res.s[i] = float32_val(r);
        }
    }

    regs->v[rd] = res;
}

static void dis_frint(uint64_t _regs, uint32_t insn, enum rm rmode)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int is_double = INSN(22,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int i;
    union simd_register res = {0};

    if (is_double) {
        for(i = 0; i < 2; i++) {
            res.df[i] = fcvt_rm(regs->v[rn].df[i], rmode);
            if (res.df[i] == 0.0 && regs->v[rn].df[i] < 0)
                res.d[i] = 0x8000000000000000UL;
        }
    } else {
        for(i = 0; i < (q?4:2); i++) {
            res.sf[i] = fcvt_rm(regs->v[rn].sf[i], rmode);
            if (res.sf[i] == 0.0 && regs->v[rn].sf[i] < 0)
                res.s[i] = 0x80000000;
        }
    }

    regs->v[rd] = res;
}

static void dis_frinta(uint64_t _regs, uint32_t insn)
{
    dis_frint(_regs, insn, RM_TIEAWAY);
}

static void dis_frintp(uint64_t _regs, uint32_t insn)
{
    dis_frint(_regs, insn, RM_POSINF);
}

static void dis_frintn(uint64_t _regs, uint32_t insn)
{
    dis_frint(_regs, insn, RM_TIEEVEN);
}

static void dis_frinti(uint64_t _regs, uint32_t insn)
{
    /* FIXME: use fpcr rm */
    dis_frint(_regs, insn, RM_TIEEVEN);
}

static void dis_frintz(uint64_t _regs, uint32_t insn)
{
    dis_frint(_regs, insn, RM_ZERO);
}

static void dis_frintx(uint64_t _regs, uint32_t insn)
{
    /* FIXME: use fpcr rm */
    dis_frint(_regs, insn, RM_TIEEVEN);
}

static void dis_frintm(uint64_t _regs, uint32_t insn)
{
    dis_frint(_regs, insn, RM_NEGINF);
}

static void dis_fcvtu(uint64_t _regs, uint32_t insn, enum rm rmode)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int is_scalar = INSN(28,28);
    int q = INSN(30,30);
    int is_double = INSN(22,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int i;
    union simd_register res = {0};

    if (is_double) {
        for(i = 0; i < (is_scalar?1:2); i++)
            res.d[i] = (uint64_t) usat64_d(fcvt_rm(regs->v[rn].df[i], rmode));
    } else {
        for(i = 0; i < (is_scalar?1:(q?4:2)); i++)
            res.s[i] = (uint32_t) usat32_d(fcvt_rm(regs->v[rn].sf[i], rmode));
    }

    regs->v[rd] = res;
}

static void dis_fcvts(uint64_t _regs, uint32_t insn, enum rm rmode)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int is_scalar = INSN(28,28);
    int q = INSN(30,30);
    int is_double = INSN(22,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int i;
    union simd_register res = {0};

    if (is_double) {
        for(i = 0; i < (is_scalar?1:2); i++)
            res.d[i] = (int64_t) ssat64_d(fcvt_rm(regs->v[rn].df[i], rmode));
    } else {
        for(i = 0; i < (is_scalar?1:(q?4:2)); i++)
            res.s[i] = (int32_t) ssat32_d(fcvt_rm(regs->v[rn].sf[i], rmode));
    }

    regs->v[rd] = res;
}

static void dis_fcvtpu(uint64_t _regs, uint32_t insn)
{
    dis_fcvtu(_regs, insn, RM_POSINF);
}

static void dis_fcvtps(uint64_t _regs, uint32_t insn)
{
    dis_fcvts(_regs, insn, RM_POSINF);
}

static void dis_fcvtnu(uint64_t _regs, uint32_t insn)
{
    dis_fcvtu(_regs, insn, RM_TIEEVEN);
}

static void dis_fcvtns(uint64_t _regs, uint32_t insn)
{
    dis_fcvts(_regs, insn, RM_TIEEVEN);
}

static void dis_fcvtzu(uint64_t _regs, uint32_t insn)
{
    dis_fcvtu(_regs, insn, RM_ZERO);
}

static void dis_fcvtzs(uint64_t _regs, uint32_t insn)
{
    dis_fcvts(_regs, insn, RM_ZERO);
}

static void dis_fcvtmu(uint64_t _regs, uint32_t insn)
{
    dis_fcvtu(_regs, insn, RM_NEGINF);
}

static void dis_fcvtms(uint64_t _regs, uint32_t insn)
{
    dis_fcvts(_regs, insn, RM_NEGINF);
}

static void dis_fcvtau(uint64_t _regs, uint32_t insn)
{
    dis_fcvtu(_regs, insn, RM_TIEAWAY);
}

static void dis_fcvtas(uint64_t _regs, uint32_t insn)
{
    dis_fcvts(_regs, insn, RM_TIEAWAY);
}

static double recip_sqrt_estimate(double a)
{
    int q0, q1, s;
    double r;

    if (a < 0.5) {
        q0 = (int)(a * 512.0);
        r = 1.0 / sqrt(((double)q0 + 0.5) / 512.0);
    } else {
        q1 = (int)(a * 256.0);
        r = 1.0 / sqrt(((double)q1 + 0.5) / 256.0);
    }
    s = (int)(256.0 * r + 0.5);

    return (double)s / 256.0;
}

static void dis_ursqrte(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    union simd_register res = {0};
    int i;

    for(i = 0; i < (q?4:2); i++) {
        uint32_t op = regs->v[rn].s[i];

        if ((op >> 30) == 0) {
            res.s[i] = 0xffffffff;
        } else {
            union {
                uint64_t i;
                double d;
            } dp_operand, estimate;

            if (op >> 31)
                dp_operand.i = (0x3feULL << 52) | ((uint64_t)(op & 0x7fffffff) << 21);
            else
                dp_operand.i = (0x3fdULL << 52) | ((uint64_t)(op & 0x3fffffff) << 22);
            estimate.d = recip_sqrt_estimate(dp_operand.d);
            res.s[i] = 0x80000000 | ((estimate.i >> 21) & 0x7fffffff);
        }
    }

    regs->v[rd] = res;
}

static double recip_estimate(double a)
{
    int q, s;
    double r;

    q = (int)(a * 512.0);
    r = 1.0 / (((double)q + 0.5) / 512.0);
    s = (int)(256.0 * r + 0.5);

    return (double)s / 256.0;
}

static void dis_urecpe(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    union simd_register res = {0};
    int i;

    for(i = 0; i < (q?4:2); i++) {
        uint32_t op = regs->v[rn].s[i];

        if ((op >> 31) == 0) {
            res.s[i] = 0xffffffff;
        } else {
            union {
                uint64_t i;
                double d;
            } dp_operand, estimate;

            dp_operand.i = (0x3feULL << 52) | ((int64_t)(op & 0x7fffffff) << 21);
            estimate.d = recip_estimate(dp_operand.d);
            res.s[i] = 0x80000000 | ((estimate.i >> 21) & 0x7fffffff);
        }
    }

    regs->v[rd] = res;
}

static void dis_ucvtf(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int is_scalar = INSN(28,28);
    int q = INSN(30,30);
    int is_double = INSN(22,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int i;
    union simd_register res = {0};

    if (is_double) {
        for(i = 0; i < (is_scalar?1:2); i++)
            res.df[i] = (double) regs->v[rn].d[i];
    } else {
        for(i = 0; i < (is_scalar?1:(q?4:2)); i++)
            res.sf[i] = (float) regs->v[rn].s[i];
    }

    regs->v[rd] = res;
}

static void dis_scvtf(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int is_scalar = INSN(28,28);
    int q = INSN(30,30);
    int is_double = INSN(22,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int i;
    union simd_register res = {0};

    if (is_double) {
        for(i = 0; i < (is_scalar?1:2); i++)
            res.df[i] = (double)(int64_t) regs->v[rn].d[i];
    } else {
        for(i = 0; i < (is_scalar?1:(q?4:2)); i++)
            res.sf[i] = (float)(int32_t) regs->v[rn].s[i];
    }

    regs->v[rd] = res;
}

static void dis_suqadd_usqadd(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int is_scalar = INSN(28,28);
    int U = INSN(29,29);
    int q = INSN(30,30);
    int size = INSN(23,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int i;
    union simd_register res = {0};

    switch(size) {
        case 0:
            for(i = 0; i < (is_scalar?1:(q?16:8)); i++)
                if (U)
                    res.b[i] = usat8(regs, (int64_t)(int8_t)regs->v[rn].b[i] + regs->v[rd].b[i]);
                else
                    res.b[i] = ssat8(regs, regs->v[rn].b[i] + (int64_t)(int8_t)regs->v[rd].b[i]);
            break;
        case 1:
            for(i = 0; i < (is_scalar?1:(q?8:4)); i++)
                if (U)
                    res.h[i] = usat16(regs, (int64_t)(int16_t)regs->v[rn].h[i] + regs->v[rd].h[i]);
                else
                    res.h[i] = ssat16(regs, regs->v[rn].h[i] + (int64_t)(int16_t)regs->v[rd].h[i]);
            break;
        case 2:
            for(i = 0; i < (is_scalar?1:(q?4:2)); i++)
                if (U)
                    res.s[i] = usat32(regs, (int64_t)(int32_t)regs->v[rn].s[i] + regs->v[rd].s[i]);
                else
                    res.s[i] = ssat32(regs, regs->v[rn].s[i] + (int64_t)(int32_t)regs->v[rd].s[i]);
            break;
        case 3:
            for(i = 0; i < (is_scalar?1:(q?2:1)); i++)
                if (U)
                    res.d[i] = usat64(regs, (__int128_t)(int64_t)regs->v[rn].d[i] + regs->v[rd].d[i]);
                else
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
    union simd_register res = {0};

    switch(size) {
        case 0:
            for(i = 0; i < (q?16:8); i++)
                res.b[i] = regs->v[rn].b[i]==0?0xff:0;
            break;
        case 1:
            for(i = 0; i < (q?8:4); i++)
                res.h[i] = regs->v[rn].h[i]==0?0xffff:0;
            break;
        case 2:
            for(i = 0; i < (q?4:2); i++) {
                res.s[i] = regs->v[rn].s[i]==0?0xffffffff:0;
            }
            break;
        case 3:
            for(i = 0; i < (q?2:1); i++)
                res.d[i] = regs->v[rn].d[i]==0?0xffffffffffffffffUL:0;
            break;
    }
    regs->v[rd] = res;
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
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int rm = INSN(20,16);
    int i, j;
    union simd_register res = {0};

    for(i = 0; i < (q?16:8); i++) {
        int product = 0;
        int op1 = regs->v[rn].b[i];
        int op2 = regs->v[rm].b[i];

        for(j = 0; j < 8; j++)
            if ((op1 >> j) & 1)
                product = product ^ (op2 << j);

        res.b[i] = product;
    }

    regs->v[rd] = res;
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

static void dis_smax_smin_umax_umin(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int U = INSN(29,29);
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
                if (U) {
                    uint8_t (*minmax)(uint8_t,uint8_t) = is_min?umin8:umax8;
                    for(i = 0; i < (q?16:8); i++)
                        res.b[i] = (*minmax)(regs->v[rn].b[i],regs->v[rm].b[i]);
                } else {
                    uint8_t (*minmax)(int8_t,int8_t) = is_min?smin8:smax8;
                    for(i = 0; i < (q?16:8); i++)
                        res.b[i] = (*minmax)(regs->v[rn].b[i],regs->v[rm].b[i]);
                }
            }
            break;
        case 1:
            {
                if (U) {
                    uint16_t (*minmax)(uint16_t,uint16_t) = is_min?umin16:umax16;
                    for(i = 0; i < (q?8:4); i++)
                        res.h[i] = (*minmax)(regs->v[rn].h[i],regs->v[rm].h[i]);
                } else {
                    uint16_t (*minmax)(int16_t,int16_t) = is_min?smin16:smax16;
                    for(i = 0; i < (q?8:4); i++)
                        res.h[i] = (*minmax)(regs->v[rn].h[i],regs->v[rm].h[i]);
                }
            }
            break;
        case 2:
            {
                if (U) {
                    uint32_t (*minmax)(uint32_t,uint32_t) = is_min?umin32:umax32;
                    for(i = 0; i < (q?4:2); i++)
                        res.s[i] = (*minmax)(regs->v[rn].s[i],regs->v[rm].s[i]);
                } else {
                    uint32_t (*minmax)(int32_t,int32_t) = is_min?smin32:smax32;
                    for(i = 0; i < (q?4:2); i++)
                        res.s[i] = (*minmax)(regs->v[rn].s[i],regs->v[rm].s[i]);
                }
            }
            break;
        default:
            assert(0);
    }
    regs->v[rd] = res;
}

static void dis_sabd_saba_uabd_uaba(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int U = INSN(29,29);
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
                if (U)
                    res.b[i] = (is_acc?regs->v[rd].b[i]:0) + uabs8(regs->v[rn].b[i], regs->v[rm].b[i]);
                else
                    res.b[i] = (is_acc?regs->v[rd].b[i]:0) + sabs8(regs->v[rn].b[i], regs->v[rm].b[i]);
            break;
        case 1:
            for(i = 0; i < (q?8:4); i++)
                if (U)
                    res.h[i] = (is_acc?regs->v[rd].h[i]:0) + uabs16(regs->v[rn].h[i], regs->v[rm].h[i]);
                else
                    res.h[i] = (is_acc?regs->v[rd].h[i]:0) + sabs16(regs->v[rn].h[i], regs->v[rm].h[i]);
            break;
        case 2:
            for(i = 0; i < (q?4:2); i++)
                if (U)
                    res.s[i] = (is_acc?regs->v[rd].s[i]:0) + uabs32(regs->v[rn].s[i], regs->v[rm].s[i]);
                else
                    res.s[i] = (is_acc?regs->v[rd].s[i]:0) + sabs32(regs->v[rn].s[i], regs->v[rm].s[i]);
            break;
        default:
            assert(0);
    }

    regs->v[rd] = res;
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
        res = (__int128_t)(val + (is_round?round:0)) >> 127;
    else if (shift >= 0) {
        /* to keep stuff for saturation we need to limit shift to 64 bits */
        shift = shift>64?64:shift;
        res = val << shift;
    }
    else
        res = (__int128_t)(val + (is_round?round:0)) >> -shift;

    return res;
}

static void dis_sshl_family(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int U = INSN(29,29);
    int is_scalar = INSN(28,28);
    int size = INSN(23,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int rm = INSN(20,16);
    int is_round = INSN(12,12);
    int is_sat = INSN(11,11);
    union simd_register res = {0};
    int i;
    __uint128_t u_res_before_sat;
    __int128_t s_res_before_sat;

    switch(size) {
        case 0:
            for(i = 0; i < (is_scalar?1:(q?16:8)); i++) {
                int8_t shift = regs->v[rm].b[i];
                if (U) {
                    u_res_before_sat = ushl((__uint128_t) regs->v[rn].b[i], shift, is_round);
                    res.b[i] = is_sat?usat8_u(regs, usat64_u(regs,u_res_before_sat)):u_res_before_sat;
                } else {
                    s_res_before_sat = sshl((__int128_t)(int8_t) regs->v[rn].b[i], shift, is_round);
                    res.b[i] = is_sat?ssat8(regs, ssat64(regs,s_res_before_sat)):s_res_before_sat;
                }
            }
            break;
        case 1:
            for(i = 0; i < (is_scalar?1:(q?8:4)); i++) {
                int8_t shift = regs->v[rm].h[i];
                if (U) {
                    u_res_before_sat = ushl((__uint128_t) regs->v[rn].h[i], shift, is_round);
                    res.h[i] = is_sat?usat16_u(regs, usat64_u(regs,u_res_before_sat)):u_res_before_sat;
                } else {
                    s_res_before_sat = sshl((__int128_t)(int16_t) regs->v[rn].h[i], shift, is_round);
                    res.h[i] = is_sat?ssat16(regs, ssat64(regs,s_res_before_sat)):s_res_before_sat;
                }
            }
            break;
        case 2:
            for(i = 0; i < (is_scalar?1:(q?4:2)); i++) {
                int8_t shift = regs->v[rm].s[i];
                if (U) {
                    u_res_before_sat = ushl((__uint128_t) regs->v[rn].s[i], shift, is_round);
                    res.s[i] = is_sat?usat32_u(regs, usat64_u(regs,u_res_before_sat)):u_res_before_sat;
                } else {
                    s_res_before_sat = sshl((__int128_t)(int32_t) regs->v[rn].s[i], shift, is_round);
                    res.s[i] = is_sat?ssat32(regs, ssat64(regs,s_res_before_sat)):s_res_before_sat;
                }
            }
            break;
        case 3:
            assert(q);
            for(i = 0; i < (is_scalar?1:2); i++) {
                int8_t shift = regs->v[rm].d[i];
                if (U) {
                    u_res_before_sat = ushl((__uint128_t) regs->v[rn].d[i], shift, is_round);
                    res.d[i] = is_sat?usat64_u(regs, u_res_before_sat):u_res_before_sat;
                } else {
                    s_res_before_sat = sshl((__int128_t)(int64_t) regs->v[rn].d[i], shift, is_round);
                    res.d[i] = is_sat?ssat64(regs, s_res_before_sat):s_res_before_sat;
                }
            }
            break;
        default:
            assert(0);
    }

    regs->v[rd] = res;
}

static void dis_sqadd_uqadd(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int is_scalar = INSN(28,28);
    int U = INSN(29,29);
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
                if (U)
                    res.b[i] = usat8(regs, regs->v[rn].b[i] + regs->v[rm].b[i]);
                else
                    res.b[i] = ssat8(regs, (int8_t)regs->v[rn].b[i] + (int8_t)regs->v[rm].b[i]);
            break;
        case 1:
            for(i = 0; i < (is_scalar?1:(q?8:4)); i++)
                if (U)
                    res.h[i] = usat16(regs, regs->v[rn].h[i] + regs->v[rm].h[i]);
                else
                    res.h[i] = ssat16(regs, (int16_t)regs->v[rn].h[i] + (int16_t)regs->v[rm].h[i]);
            break;
        case 2:
            for(i = 0; i < (is_scalar?1:(q?4:2)); i++)
                if (U)
                    res.s[i] = usat32(regs, (uint64_t)regs->v[rn].s[i] + (uint64_t)regs->v[rm].s[i]);
                else
                    res.s[i] = ssat32(regs, (int64_t)(int32_t)regs->v[rn].s[i] + (int64_t)(int32_t)regs->v[rm].s[i]);
            break;
        case 3:
            for(i = 0; i < (is_scalar?1:(q?2:1)); i++)
                if (U)
                    res.d[i] = usat64(regs, (__uint128_t)regs->v[rn].d[i] + (__uint128_t)regs->v[rm].d[i]);
                else
                    res.d[i] = ssat64(regs, (__int128_t)(int64_t)regs->v[rn].d[i] + (__int128_t)(int64_t)regs->v[rm].d[i]);
            break;
    }
    regs->v[rd] = res;
}

static void dis_sqsub_uqsub(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int is_scalar = INSN(28,28);
    int U = INSN(29,29);
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
                if (U)
                    res.b[i] = usat8(regs, regs->v[rn].b[i] - regs->v[rm].b[i]);
                else
                    res.b[i] = ssat8(regs, (int8_t)regs->v[rn].b[i] - (int8_t)regs->v[rm].b[i]);
            break;
        case 1:
            for(i = 0; i < (is_scalar?1:(q?8:4)); i++)
                if (U)
                    res.h[i] = usat16(regs, regs->v[rn].h[i] - regs->v[rm].h[i]);
                else
                    res.h[i] = ssat16(regs, (int16_t)regs->v[rn].h[i] - (int16_t)regs->v[rm].h[i]);
            break;
        case 2:
            for(i = 0; i < (is_scalar?1:(q?4:2)); i++)
                if (U)
                    res.s[i] = usat32(regs, (uint64_t)regs->v[rn].s[i] - (uint64_t)regs->v[rm].s[i]);
                else
                    res.s[i] = ssat32(regs, (int64_t)(int32_t)regs->v[rn].s[i] - (int64_t)(int32_t)regs->v[rm].s[i]);
            break;
        case 3:
            for(i = 0; i < (is_scalar?1:(q?2:1)); i++)
                if (U)
                    res.d[i] = usat64(regs, (__uint128_t)regs->v[rn].d[i] - (__uint128_t)regs->v[rm].d[i]);
                else
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

static void dis_fmla_fmls(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int is_double = INSN(22,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int rm = INSN(20,16);
    int is_sub = INSN(23,23);
    union simd_register res = {0};
    int i;

    if (is_double) {
        for(i = 0; i < 2; i++)
            if (is_sub)
                res.df[i] = regs->v[rd].df[i] - regs->v[rn].df[i] * regs->v[rm].df[i];
            else
                res.df[i] = regs->v[rd].df[i] + regs->v[rn].df[i] * regs->v[rm].df[i];
    } else {
        for(i = 0; i < (q?4:2); i++)
            if (is_sub)
                res.sf[i] = (double)regs->v[rd].sf[i] - (double)regs->v[rn].sf[i] * (double)regs->v[rm].sf[i];
            else
                res.sf[i] = (double)regs->v[rd].sf[i] + (double)regs->v[rn].sf[i] * (double)regs->v[rm].sf[i];
    }
    regs->v[rd] = res;
}

static void dis_fabd(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int is_scalar = INSN(28,28);
    int size = INSN(23,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int rm = INSN(20,16);
    union simd_register res = {0};
    int i;

    switch(size) {
        case 2:
            for(i = 0; i < (is_scalar?1:(q?4:2)); i++)
                res.sf[i] = regs->v[rn].sf[i]-regs->v[rm].sf[i]>=0?regs->v[rn].sf[i]-regs->v[rm].sf[i]:regs->v[rm].sf[i]-regs->v[rn].sf[i];
            break;
        case 3:
            assert(q);
            for(i = 0; i < (is_scalar?1:2); i++)
                res.df[i] = regs->v[rn].df[i]-regs->v[rm].df[i]>=0?regs->v[rn].df[i]-regs->v[rm].df[i]:regs->v[rm].df[i]-regs->v[rn].df[i];
            break;
        default:
            assert(0);
    }
    regs->v[rd] = res;
}

static void dis_fadd_fsub(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int is_double = INSN(22,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int rm = INSN(20,16);
    int is_sub = INSN(23,23);
    union simd_register res = {0};
    int i;

    if (is_double) {
        assert(q);
        for(i = 0; i < 2; i++)
            if (is_sub)
                res.df[i] = regs->v[rn].df[i] - regs->v[rm].df[i];
            else
                res.df[i] = regs->v[rn].df[i] + regs->v[rm].df[i];
    } else {
        for(i = 0; i < (q?4:2); i++)
            if (is_sub)
                res.sf[i] = regs->v[rn].sf[i] - regs->v[rm].sf[i];
            else
                res.sf[i] = regs->v[rn].sf[i] + regs->v[rm].sf[i];
    }
    regs->v[rd] = res;
}

/* macro to implement fcmxx family */
#define DIS_FCM(_op_,_is_zero_) \
do { \
struct arm64_registers *regs = (struct arm64_registers *) _regs; \
int q = INSN(30,30); \
int is_scalar = INSN(28,28); \
int is_double = INSN(22,22); \
int rd = INSN(4,0); \
int rn = INSN(9,5); \
int rm = INSN(20,16); \
union simd_register res = {0}; \
int i; \
 \
if (is_double) { \
    for(i = 0; i < (is_scalar?1:2); i++) \
        res.d[i] = regs->v[rn].df[i] _op_ (_is_zero_?0:regs->v[rm].df[i])?~0UL:0; \
} else { \
    for(i = 0; i < (is_scalar?1:(q?4:2)); i++) \
        res.s[i] = regs->v[rn].sf[i] _op_ (_is_zero_?0:regs->v[rm].sf[i])?~0UL:0; \
} \
regs->v[rd] = res; \
} while(0)

static void dis_fcmeq_zero(uint64_t _regs, uint32_t insn)
{
DIS_FCM(==,1);
}

static void dis_fcmge_zero(uint64_t _regs, uint32_t insn)
{
DIS_FCM(>=,1);
}

static void dis_fcmgt_zero(uint64_t _regs, uint32_t insn)
{
DIS_FCM(>,1);
}

static void dis_fcmle_zero(uint64_t _regs, uint32_t insn)
{
DIS_FCM(<=,1);
}

static void dis_fcmlt_zero(uint64_t _regs, uint32_t insn)
{
DIS_FCM(<,1);
}

static void dis_fcmeq(uint64_t _regs, uint32_t insn)
{
DIS_FCM(==,0);
}

static void dis_fcmge(uint64_t _regs, uint32_t insn)
{
DIS_FCM(>=,0);
}

static void dis_fcmgt(uint64_t _regs, uint32_t insn)
{
DIS_FCM(>,0);
}

static void dis_facge(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int is_scalar = INSN(28,28);
    int is_double = INSN(22,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int rm = INSN(20,16);
    union simd_register res = {0};
    int i;

    if (is_double) {
        double op1;
        double op2;

        for(i = 0; i < (is_scalar?1:2); i++) {
            op1 = regs->v[rn].df[i]>=0?regs->v[rn].df[i]:-regs->v[rn].df[i];
            op2 = regs->v[rm].df[i]>=0?regs->v[rm].df[i]:-regs->v[rm].df[i];

            res.d[i] = op1>=op2?~0UL:0;
        }
    } else {
        float op1;
        float op2;

        for(i = 0; i < (is_scalar?1:(q?4:2)); i++) {
            op1 = regs->v[rn].sf[i]>=0?regs->v[rn].sf[i]:-regs->v[rn].sf[i];
            op2 = regs->v[rm].sf[i]>=0?regs->v[rm].sf[i]:-regs->v[rm].sf[i];

            res.s[i] = op1>=op2?~0UL:0;
        }
    }
    regs->v[rd] = res;
}

static void dis_fmax_fmin(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int is_scalar = INSN(28,28);
    int is_double = INSN(22,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int rm = INSN(20,16);
    int is_min = INSN(23,23);
    union simd_register res = {0};
    int i;

    assert(is_scalar==0);
    if (is_double) {
        for(i = 0; i < 2; i++)
            if (is_min)
                res.df[i] = mind(regs->v[rn].df[i],regs->v[rm].df[i]);
            else
                res.df[i] = maxd(regs->v[rn].df[i],regs->v[rm].df[i]);
    } else {
        for(i = 0; i < (q?4:2); i++)
            if (is_min)
                res.sf[i] = minf(regs->v[rn].sf[i],regs->v[rm].sf[i]);
            else
                res.sf[i] = maxf(regs->v[rn].sf[i],regs->v[rm].sf[i]);
    }
    regs->v[rd] = res;
}

static void dis_fmaxnm_fminnm(uint64_t _regs, uint32_t insn)
{
    dis_fmax_fmin(_regs, insn);
}

static void dis_fdiv(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int is_scalar = INSN(28,28);
    int is_double = INSN(22,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int rm = INSN(20,16);
    union simd_register res = {0};
    int i;

    assert(is_scalar==0);
    if (is_double) {
        for(i = 0; i < 2; i++)
            res.df[i] = regs->v[rn].df[i] / regs->v[rm].df[i];
    } else {
        for(i = 0; i < (q?4:2); i++)
            res.sf[i] = regs->v[rn].sf[i] / regs->v[rm].sf[i];
    }
    regs->v[rd] = res;
}

static void dis_facgt(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int is_scalar = INSN(28,28);
    int is_double = INSN(22,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int rm = INSN(20,16);
    union simd_register res = {0};
    int i;

    if (is_double) {
        double op1;
        double op2;

        for(i = 0; i < (is_scalar?1:2); i++) {
            op1 = regs->v[rn].df[i]>0?regs->v[rn].df[i]:-regs->v[rn].df[i];
            op2 = regs->v[rm].df[i]>0?regs->v[rm].df[i]:-regs->v[rm].df[i];

            res.d[i] = op1>=op2?~0UL:0;
        }
    } else {
        float op1;
        float op2;

        for(i = 0; i < (is_scalar?1:(q?4:2)); i++) {
            op1 = regs->v[rn].sf[i]>0?regs->v[rn].sf[i]:-regs->v[rn].sf[i];
            op2 = regs->v[rm].sf[i]>0?regs->v[rm].sf[i]:-regs->v[rm].sf[i];

            res.s[i] = op1>=op2?~0UL:0;
        }
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

static void dis_fmaxp_fminp(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int is_scalar = INSN(28,28);
    int q = INSN(30,30);
    int is_double = INSN(22,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int rm = INSN(20,16);
    int is_min = INSN(23,23);
    int i;
    union simd_register res = {0};

    if (is_double) {
        for(i = 0; i < 1; i++) {
            if (is_min) {
                res.df[i] = mind(regs->v[rn].df[2*i],regs->v[rn].df[2*i + 1]);
                if (!is_scalar)
                    res.df[i+1] = mind(regs->v[rm].df[2*i],regs->v[rm].df[2*i + 1]);
            } else {
                res.df[i] = maxd(regs->v[rn].df[2*i],regs->v[rn].df[2*i + 1]);
                if (!is_scalar)
                    res.df[i+1] = maxd(regs->v[rm].df[2*i],regs->v[rm].df[2*i + 1]);
            }
        }
    } else {
        for(i = 0; i < (is_scalar?1:(q?2:1)); i++) {
            if (is_min) {
                res.sf[i] = minf(regs->v[rn].sf[2*i],regs->v[rn].sf[2*i + 1]);
                if (!is_scalar)
                    res.sf[(q?2:1) + i] = minf(regs->v[rm].sf[2*i],regs->v[rm].sf[2*i + 1]);
            } else {
                res.sf[i] = maxf(regs->v[rn].sf[2*i],regs->v[rn].sf[2*i + 1]);
                if (!is_scalar)
                    res.sf[(q?2:1) + i] = maxf(regs->v[rm].sf[2*i],regs->v[rm].sf[2*i + 1]);
            }
        }
    }
    regs->v[rd] = res;
}

static void dis_fmaxnmp_fminnmp(uint64_t _regs, uint32_t insn)
{
    dis_fmaxp_fminp(_regs,insn);
}

static void dis_faddp(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int is_scalar = INSN(28,28);
    int q = INSN(30,30);
    int is_double = INSN(22,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int rm = INSN(20,16);
    int i;
    union simd_register res = {0};

    if (is_double) {
        for(i = 0; i < 1; i++) {
            res.df[i] = regs->v[rn].df[2*i] + regs->v[rn].df[2*i + 1];
            if (!is_scalar)
                res.df[i+1] = regs->v[rm].df[2*i] + regs->v[rm].df[2*i + 1];
        }
    } else {
        for(i = 0; i < (is_scalar?1:(q?2:1)); i++) {
            res.sf[i] = regs->v[rn].sf[2*i] + regs->v[rn].sf[2*i + 1];
            if (!is_scalar)
                res.sf[(q?2:1) + i] = regs->v[rm].sf[2*i] + regs->v[rm].sf[2*i + 1];
        }
    }
    regs->v[rd] = res;
}

static void dis_fmul(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int is_scalar = INSN(28,28);
    int q = INSN(30,30);
    int is_double = INSN(22,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int rm = INSN(20,16);
    int i;
    union simd_register res = {0};

    /* scalar is only for fmulx */
    if (is_double) {
        for(i = 0; i < (is_scalar?1:2); i++)
            res.df[i] = regs->v[rn].df[i] * regs->v[rm].df[i];
    } else {
        for(i = 0; i < (is_scalar?1:(q?4:2)); i++)
            res.sf[i] = regs->v[rn].sf[i] * regs->v[rm].sf[i];
    }
    regs->v[rd] = res;
}

static void dis_fmulx(uint64_t _regs, uint32_t insn)
{
    dis_fmul(_regs, insn);
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

static void dis_smaxp_sminp_umaxp_uminp(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int U = INSN(29,29);
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
                if (U) {
                    uint8_t (*minmax)(uint8_t,uint8_t) = is_min?umin8:umax8;
                    for(i = 0; i < (q?8:4); i++) {
                        res.b[i] = (*minmax)(regs->v[rn].b[2*i],regs->v[rn].b[2*i + 1]);
                        res.b[(q?8:4) + i] = (*minmax)(regs->v[rm].b[2*i],regs->v[rm].b[2*i + 1]);
                    }
                } else {
                    uint8_t (*minmax)(int8_t,int8_t) = is_min?smin8:smax8;
                    for(i = 0; i < (q?8:4); i++) {
                        res.b[i] = (*minmax)(regs->v[rn].b[2*i],regs->v[rn].b[2*i + 1]);
                        res.b[(q?8:4) + i] = (*minmax)(regs->v[rm].b[2*i],regs->v[rm].b[2*i + 1]);
                    }
                }
            }
            break;
        case 1:
            {
                if (U) {
                    uint16_t (*minmax)(uint16_t,uint16_t) = is_min?umin16:umax16;
                    for(i = 0; i < (q?4:2); i++) {
                        res.h[i] = (*minmax)(regs->v[rn].h[2*i], regs->v[rn].h[2*i + 1]);
                        res.h[(q?4:2) + i] = (*minmax)(regs->v[rm].h[2*i], regs->v[rm].h[2*i + 1]);
                    }
                } else {
                    uint16_t (*minmax)(int16_t,int16_t) = is_min?smin16:smax16;
                    for(i = 0; i < (q?4:2); i++) {
                        res.h[i] = (*minmax)(regs->v[rn].h[2*i], regs->v[rn].h[2*i + 1]);
                        res.h[(q?4:2) + i] = (*minmax)(regs->v[rm].h[2*i], regs->v[rm].h[2*i + 1]);
                    }
                }
            }
            break;
        case 2:
            {
                if (U) {
                    uint32_t (*minmax)(uint32_t,uint32_t) = is_min?umin32:umax32;
                    for(i = 0; i < (q?2:1); i++) {
                        res.s[i] = (*minmax)(regs->v[rn].s[2*i], regs->v[rn].s[2*i + 1]);
                        res.s[(q?2:1) + i] = (*minmax)(regs->v[rm].s[2*i], regs->v[rm].s[2*i + 1]);
                    }
                } else {
                    uint32_t (*minmax)(int32_t,int32_t) = is_min?smin32:smax32;
                    for(i = 0; i < (q?2:1); i++) {
                        res.s[i] = (*minmax)(regs->v[rn].s[2*i], regs->v[rn].s[2*i + 1]);
                        res.s[(q?2:1) + i] = (*minmax)(regs->v[rm].s[2*i], regs->v[rm].s[2*i + 1]);
                    }
                }
            }
            break;
        default:
            assert(0);
    }
    regs->v[rd] = res;
}

static void dis_saddl_ssubl_uaddl_usubl(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int U = INSN(29,29);
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
                if (U) {
                    if (is_sub)
                        res.h[i] = regs->v[rn].b[q?i+8:i] - regs->v[rm].b[q?i+8:i];
                    else
                        res.h[i] = regs->v[rn].b[q?i+8:i] + regs->v[rm].b[q?i+8:i];
                } else {
                    if (is_sub)
                        res.h[i] = (int8_t)regs->v[rn].b[q?i+8:i] - (int8_t)regs->v[rm].b[q?i+8:i];
                    else
                        res.h[i] = (int8_t)regs->v[rn].b[q?i+8:i] + (int8_t)regs->v[rm].b[q?i+8:i];
                }
            break;
        case 1:
            for(i = 0; i < 4; i++)
                if (U) {
                    if (is_sub)
                        res.s[i] = regs->v[rn].h[q?i+4:i] - regs->v[rm].h[q?i+4:i];
                    else
                        res.s[i] = regs->v[rn].h[q?i+4:i] + regs->v[rm].h[q?i+4:i];
                } else {
                    if (is_sub)
                        res.s[i] = (int16_t)regs->v[rn].h[q?i+4:i] - (int16_t)regs->v[rm].h[q?i+4:i];
                    else
                        res.s[i] = (int16_t)regs->v[rn].h[q?i+4:i] + (int16_t)regs->v[rm].h[q?i+4:i];
                }
            break;
        case 2:
            for(i = 0; i < 2; i++)
                if (U) {
                    if (is_sub)
                        res.d[i] = (uint64_t)regs->v[rn].s[q?i+2:i] - (uint64_t)regs->v[rm].s[q?i+2:i];
                    else
                        res.d[i] = (uint64_t)regs->v[rn].s[q?i+2:i] + (uint64_t)regs->v[rm].s[q?i+2:i];
                } else {
                    if (is_sub)
                        res.d[i] = (int64_t)(int32_t)regs->v[rn].s[q?i+2:i] - (int64_t)(int32_t)regs->v[rm].s[q?i+2:i];
                    else
                        res.d[i] = (int64_t)(int32_t)regs->v[rn].s[q?i+2:i] + (int64_t)(int32_t)regs->v[rm].s[q?i+2:i];
                }
            break;
        default:
            assert(0);
    }
    regs->v[rd] = res;
}

static void dis_saddw_ssubw_uaddw_usubw(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int U = INSN(29,29);
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
                if (U) {
                    if (is_sub)
                        res.h[i] = regs->v[rn].h[i] - regs->v[rm].b[q?i+8:i];
                    else
                        res.h[i] = regs->v[rn].h[i] + regs->v[rm].b[q?i+8:i];
                } else {
                    if (is_sub)
                        res.h[i] = (int16_t)regs->v[rn].h[i] - (int8_t)regs->v[rm].b[q?i+8:i];
                    else
                        res.h[i] = (int16_t)regs->v[rn].h[i] + (int8_t)regs->v[rm].b[q?i+8:i];
                }
            break;
        case 1:
            for(i = 0; i < 4; i++)
                if (U) {
                    if (is_sub)
                        res.s[i] = regs->v[rn].s[i] - regs->v[rm].h[q?i+4:i];
                    else
                        res.s[i] = regs->v[rn].s[i] + regs->v[rm].h[q?i+4:i];
                } else {
                    if (is_sub)
                        res.s[i] = (int32_t)regs->v[rn].s[i] - (int16_t)regs->v[rm].h[q?i+4:i];
                    else
                        res.s[i] = (int32_t)regs->v[rn].s[i] + (int16_t)regs->v[rm].h[q?i+4:i];
                }
            break;
        case 2:
            for(i = 0; i < 2; i++)
                if (U) {
                    if (is_sub)
                        res.d[i] = (uint64_t)regs->v[rn].d[i] - (uint64_t)regs->v[rm].s[q?i+2:i];
                    else
                        res.d[i] = (uint64_t)regs->v[rn].d[i] + (uint64_t)regs->v[rm].s[q?i+2:i];
                } else {
                    if (is_sub)
                        res.d[i] = (int64_t)regs->v[rn].d[i] - (int64_t)(int32_t)regs->v[rm].s[q?i+2:i];
                    else
                        res.d[i] = (int64_t)regs->v[rn].d[i] + (int64_t)(int32_t)regs->v[rm].s[q?i+2:i];
                }
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

static void dis_sabdl_sabal_uabdl_uabal(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int U = INSN(29,29);
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
                if (U)
                    res.h[i] += uabs8(regs->v[rn].b[q?i+8:i], regs->v[rm].b[q?i+8:i]);
                else
                    res.h[i] += sabs8(regs->v[rn].b[q?i+8:i], regs->v[rm].b[q?i+8:i]);
            break;
        case 1:
            for(i = 0; i < 4; i++)
                if (U)
                    res.s[i] += uabs16(regs->v[rn].h[q?i+4:i], regs->v[rm].h[q?i+4:i]);
                else
                    res.s[i] += sabs16(regs->v[rn].h[q?i+4:i], regs->v[rm].h[q?i+4:i]);
            break;
        case 2:
            for(i = 0; i < 2; i++)
                if (U)
                    res.d[i] += uabs32(regs->v[rn].s[q?i+2:i], regs->v[rm].s[q?i+2:i]);
                else
                    res.d[i] += sabs32(regs->v[rn].s[q?i+2:i], regs->v[rm].s[q?i+2:i]);
            break;
        default:
            assert(0);
    }
    regs->v[rd] = res;
}

static void dis_smlal_smlsl_umlal_umlsl(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int U = INSN(29,29);
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
                if (U) {
                    if (is_sub)
                        res.h[i] -= regs->v[rn].b[q?i+8:i] * regs->v[rm].b[q?i+8:i];
                    else
                        res.h[i] += regs->v[rn].b[q?i+8:i] * regs->v[rm].b[q?i+8:i];
                } else {
                    if (is_sub)
                        res.h[i] -= (int8_t)regs->v[rn].b[q?i+8:i] * (int8_t)regs->v[rm].b[q?i+8:i];
                    else
                        res.h[i] += (int8_t)regs->v[rn].b[q?i+8:i] * (int8_t)regs->v[rm].b[q?i+8:i];
                }
            break;
        case 1:
            for(i = 0; i < 4; i++)
                if (U) {
                    if (is_sub)
                        res.s[i] -= regs->v[rn].h[q?i+4:i] * regs->v[rm].h[q?i+4:i];
                    else
                        res.s[i] += regs->v[rn].h[q?i+4:i] * regs->v[rm].h[q?i+4:i];
                } else {
                    if (is_sub)
                        res.s[i] -= (int16_t)regs->v[rn].h[q?i+4:i] * (int16_t)regs->v[rm].h[q?i+4:i];
                    else
                        res.s[i] += (int16_t)regs->v[rn].h[q?i+4:i] * (int16_t)regs->v[rm].h[q?i+4:i];
                }
            break;
        case 2:
            for(i = 0; i < 2; i++)
                if (U) {
                    if (is_sub)
                        res.d[i] -= (uint64_t)regs->v[rn].s[q?i+2:i] * (uint64_t)regs->v[rm].s[q?i+2:i];
                    else
                        res.d[i] += (uint64_t)regs->v[rn].s[q?i+2:i] * (uint64_t)regs->v[rm].s[q?i+2:i];
                } else {
                    if (is_sub)
                        res.d[i] -= (int64_t)(int32_t)regs->v[rn].s[q?i+2:i] * (int64_t)(int32_t)regs->v[rm].s[q?i+2:i];
                    else
                        res.d[i] += (int64_t)(int32_t)regs->v[rn].s[q?i+2:i] * (int64_t)(int32_t)regs->v[rm].s[q?i+2:i];
                }
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

static void dis_smull_umull(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int U = INSN(29,29);
    int size = INSN(23,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int rm = INSN(20,16);
    int i;
    union simd_register res = {0};

    switch(size) {
        case 0:
            for(i = 0; i < 8; i++)
                if (U)
                    res.h[i] = regs->v[rn].b[q?i+8:i] * regs->v[rm].b[q?i+8:i];
                else
                    res.h[i] = (int8_t)regs->v[rn].b[q?i+8:i] * (int8_t)regs->v[rm].b[q?i+8:i];
            break;
        case 1:
            for(i = 0; i < 4; i++)
                if (U)
                    res.s[i] = regs->v[rn].h[q?i+4:i] * regs->v[rm].h[q?i+4:i];
                else
                    res.s[i] = (int16_t)regs->v[rn].h[q?i+4:i] * (int16_t)regs->v[rm].h[q?i+4:i];
            break;
        case 2:
            for(i = 0; i < 2; i++)
                if (U)
                    res.d[i] = (uint64_t)regs->v[rn].s[q?i+2:i] * (uint64_t)regs->v[rm].s[q?i+2:i];
                else
                    res.d[i] = (int64_t)(int32_t)regs->v[rn].s[q?i+2:i] * (int64_t)(int32_t)regs->v[rm].s[q?i+2:i];
            break;
        default:
            assert(0);
    }
    regs->v[rd] = res;
}

/* FIXME: add 1Q arrangement for crypto extension */
static void dis_pmull(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int rm = INSN(20,16);
    int i, j;
    union simd_register res = {0};

    for(i = 0; i < 8; i++) {
        int product = 0;
        int op1 = regs->v[rn].b[q?i+8:i];
        int op2 = regs->v[rm].b[q?i+8:i];

        for(j = 0; j < 8; j++)
            if ((op1 >> j) & 1)
                product = product ^ (op2 << j);

        res.h[i] = product;
    }

    regs->v[rd] = res;
}

static void dis_saddlv_uaddlv(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int U = INSN(29,29);
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
                if (U)
                    res.h[0] += regs->v[rn].b[i];
                else
                    res.h[0] += (int8_t)regs->v[rn].b[i];
            break;
        case 1:
            for(i = 0; i < (q?8:4); i++)
                if (U)
                    res.s[0] += regs->v[rn].h[i];
                else
                    res.s[0] += (int16_t)regs->v[rn].h[i];
            break;
        case 2:
            assert(q);
            for(i = 0; i < 4; i++)
                if (U)
                    res.d[0] += (uint64_t)regs->v[rn].s[i];
                else
                    res.d[0] += (int64_t)(int32_t)regs->v[rn].s[i];
            break;
        default:
            assert(0);
    }
    regs->v[rd].v = res.v;
}

static void dis_smaxv_sminv_umaxv_uminv(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int U = INSN(29,29);
    int size = INSN(23,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int is_min = INSN(16,16);
    int i;
    union simd_register res = {0};

    switch(size) {
        case 0:
            {
                if (U) {
                    uint8_t (*minmax)(uint8_t,uint8_t) = is_min?umin8:umax8;
                    res.b[0] = regs->v[rn].b[0];
                    for(i = 1; i < (q?16:8); i++)
                        res.b[0] = (*minmax)(regs->v[rn].b[i], res.b[0]);
                } else {
                    uint8_t (*minmax)(int8_t,int8_t) = is_min?smin8:smax8;
                    res.b[0] = regs->v[rn].b[0];
                    for(i = 1; i < (q?16:8); i++)
                        res.b[0] = (*minmax)(regs->v[rn].b[i], res.b[0]);
                }
            }
            break;
        case 1:
            {
                if (U) {
                    uint16_t (*minmax)(uint16_t,uint16_t) = is_min?umin16:umax16;
                    res.h[0] = regs->v[rn].h[0];
                    for(i = 1; i < (q?8:4); i++)
                        res.h[0] = (*minmax)(regs->v[rn].h[i], res.h[0]);
                } else {
                    uint16_t (*minmax)(int16_t,int16_t) = is_min?smin16:smax16;
                    res.h[0] = regs->v[rn].h[0];
                    for(i = 1; i < (q?8:4); i++)
                        res.h[0] = (*minmax)(regs->v[rn].h[i], res.h[0]);
                }
            }
            break;
        case 2:
            {
                if (U) {
                    uint32_t (*minmax)(uint32_t,uint32_t) = is_min?umin32:umax32;
                    assert(q);
                    res.s[0] = regs->v[rn].s[0];
                    for(i = 1; i < (q?4:2); i++)
                        res.s[0] = (*minmax)(regs->v[rn].s[i], res.s[0]);
                } else {
                    uint32_t (*minmax)(int32_t,int32_t) = is_min?smin32:smax32;
                    assert(q);
                    res.s[0] = regs->v[rn].s[0];
                    for(i = 1; i < (q?4:2); i++)
                        res.s[0] = (*minmax)(regs->v[rn].s[i], res.s[0]);
                }
            }
            break;
        default:
            assert(0);
    }
    regs->v[rd].v = res.v;
}

static void dis_fmaxv_fminv(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int is_double = INSN(22,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int is_min = INSN(23,23);
    float tmp[2];
    union simd_register res = {0};

    assert(is_double == 0);
    assert(q);
    if (is_min) {
        tmp[0] = minf(regs->v[rn].sf[0],regs->v[rn].sf[1]);
        tmp[1] = minf(regs->v[rn].sf[2],regs->v[rn].sf[3]);
        res.sf[0] = minf(tmp[0],tmp[1]);
    } else {
        tmp[0] = maxf(regs->v[rn].sf[0],regs->v[rn].sf[1]);
        tmp[1] = maxf(regs->v[rn].sf[2],regs->v[rn].sf[3]);
        res.sf[0] = maxf(tmp[0],tmp[1]);
    }

    regs->v[rd] = res;
}

static void dis_fmaxnmv_fminnmv(uint64_t _regs, uint32_t insn)
{
    dis_fmaxv_fminv(_regs, insn);
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

static void dis_shadd_srhadd_uhadd_urhadd(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int U = INSN(29,29);
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
                if (U)
                    res.b[i] = (regs->v[rn].b[i] + regs->v[rm].b[i] + (is_round?1:0)) >> 1;
                else
                    res.b[i] = ((int8_t)regs->v[rn].b[i] + (int8_t)regs->v[rm].b[i] + (is_round?1:0)) >> 1;
            break;
        case 1:
            for(i = 0; i < (q?8:4); i++)
                if (U)
                    res.h[i] = (regs->v[rn].h[i] + regs->v[rm].h[i] + (is_round?1:0)) >> 1;
                else
                    res.h[i] = ((int16_t)regs->v[rn].h[i] + (int16_t)regs->v[rm].h[i] + (is_round?1:0)) >> 1;
            break;
        case 2:
            for(i = 0; i < (q?4:2); i++)
                if (U)
                    res.s[i] = ((uint64_t)regs->v[rn].s[i] + (uint64_t)regs->v[rm].s[i] + (is_round?1:0)) >> 1;
                else
                    res.s[i] = ((int64_t)(int32_t)regs->v[rn].s[i] + (int64_t)(int32_t)regs->v[rm].s[i] + (is_round?1:0)) >> 1;
            break;
        default:
            assert(0);
    }
    regs->v[rd].v = res.v;
}

static void dis_shsub_uhsub(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int U = INSN(29,29);
    int size = INSN(23,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int rm = INSN(20,16);
    int i;
    union simd_register res = {0};

    switch(size) {
        case 0:
            for(i = 0; i < (q?16:8); i++)
                if (U)
                    res.b[i] = (regs->v[rn].b[i] - regs->v[rm].b[i]) >> 1;
                else
                    res.b[i] = ((int8_t)regs->v[rn].b[i] - (int8_t)regs->v[rm].b[i]) >> 1;
            break;
        case 1:
            for(i = 0; i < (q?8:4); i++)
                if (U)
                    res.h[i] = (regs->v[rn].h[i] - regs->v[rm].h[i]) >> 1;
                else
                    res.h[i] = ((int16_t)regs->v[rn].h[i] - (int16_t)regs->v[rm].h[i]) >> 1;
            break;
        case 2:
            for(i = 0; i < (q?4:2); i++)
                if (U)
                    res.s[i] = ((uint64_t)regs->v[rn].s[i] - (uint64_t)regs->v[rm].s[i]) >> 1;
                else
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

static void dis_saddlp_sadalp_uaddlp_uadalp(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int U = INSN(29,29);
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
                if (U)
                    res.h[i] += regs->v[rn].b[2*i] + regs->v[rn].b[2*i+1];
                else
                    res.h[i] += (int8_t)regs->v[rn].b[2*i] + (int8_t)regs->v[rn].b[2*i+1];
            break;
        case 1:
            for(i = 0; i < (q?4:2); i++)
                if (U)
                    res.s[i] += regs->v[rn].h[2*i] + regs->v[rn].h[2*i+1];
                else
                    res.s[i] += (int16_t)regs->v[rn].h[2*i] + (int16_t)regs->v[rn].h[2*i+1];
            break;
        case 2:
            for(i = 0; i < (q?2:1); i++)
                if (U)
                    res.d[i] += (uint64_t)regs->v[rn].s[2*i] + (uint64_t)regs->v[rn].s[2*i+1];
                else
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

static void dis_smlal_smlsl_umlal_umlsl_by_element(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int U = INSN(29,29);
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
            for(i = 0; i < 4; i++) {
                if (U) {
                     if (is_sub)
                        res.s[i] = regs->v[rd].s[i] - regs->v[rn].h[q?4+i:i] * regs->v[rm].h[index];
                    else
                        res.s[i] = regs->v[rd].s[i] + regs->v[rn].h[q?4+i:i] * regs->v[rm].h[index];
                } else {
                    if (is_sub)
                        res.s[i] = regs->v[rd].s[i] - (int16_t)regs->v[rn].h[q?4+i:i] * (int16_t)regs->v[rm].h[index];
                    else
                        res.s[i] = regs->v[rd].s[i] + (int16_t)regs->v[rn].h[q?4+i:i] * (int16_t)regs->v[rm].h[index];
                }
            }
            break;
        case 2:
            index = (H << 1) | L;
            rm += M << 4;
            for(i = 0; i < 2; i++) {
                if (U) {
                    if (is_sub)
                        res.d[i] = regs->v[rd].d[i] - (uint64_t)regs->v[rn].s[q?2+i:i] * (uint64_t)regs->v[rm].s[index];
                    else
                        res.d[i] = regs->v[rd].d[i] + (uint64_t)regs->v[rn].s[q?2+i:i] * (uint64_t)regs->v[rm].s[index];
                } else {
                    if (is_sub)
                        res.d[i] = regs->v[rd].d[i] - (int64_t)(int32_t)regs->v[rn].s[q?2+i:i] * (int64_t)(int32_t)regs->v[rm].s[index];
                    else
                        res.d[i] = regs->v[rd].d[i] + (int64_t)(int32_t)regs->v[rn].s[q?2+i:i] * (int64_t)(int32_t)regs->v[rm].s[index];
                }
            }
            break;
        default:
            assert(0);
    }
    regs->v[rd] = res;
}

static void dis_smull_umull_by_element(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int U = INSN(29,29);
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
                if (U)
                    res.s[i] = regs->v[rn].h[q?4+i:i] * regs->v[rm].h[index];
                else
                    res.s[i] = (int16_t)regs->v[rn].h[q?4+i:i] * (int16_t)regs->v[rm].h[index];
            break;
        case 2:
            index = (H << 1) | L;
            rm += M << 4;
            for(i = 0; i < 2; i++)
                if (U)
                    res.d[i] = (uint64_t)regs->v[rn].s[q?2+i:i] * (uint64_t)regs->v[rm].s[index];
                else
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

static void dis_fmla_fmls_by_element(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int is_scalar = INSN(28,28);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int rm = INSN(19,16);
    int is_double = INSN(22,22);
    int H = INSN(11,11);
    int L = INSN(21,21);
    int M = INSN(20,20);
    int is_sub = INSN(14,14);
    union simd_register res = {0};
    int index;
    int i;

    if (is_double) {
        index = H;
        rm += M << 4;
        for(i = 0; i < (is_scalar?1:2); i++)
            if (is_sub)
                res.df[i] = regs->v[rd].df[i] - regs->v[rn].df[i] * regs->v[rm].df[index];
            else
                res.df[i] = regs->v[rd].df[i] + regs->v[rn].df[i] * regs->v[rm].df[index];
    } else {
        index = (H << 1) | L;
        rm += M << 4;
        for(i = 0; i < (is_scalar?1:(q?4:2)); i++)
            if (is_sub)
                res.sf[i] = (double)regs->v[rd].sf[i] - (double)regs->v[rn].sf[i] * (double)regs->v[rm].sf[index];
            else
                res.sf[i] = (double)regs->v[rd].sf[i] + (double)regs->v[rn].sf[i] * (double)regs->v[rm].sf[index];
    }
    regs->v[rd] = res;
}

static void dis_fmul_by_element(uint64_t _regs, uint32_t insn)
{
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int q = INSN(30,30);
    int is_scalar = INSN(28,28);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int rm = INSN(19,16);
    int is_double = INSN(22,22);
    int H = INSN(11,11);
    int L = INSN(21,21);
    int M = INSN(20,20);
    union simd_register res = {0};
    int index;
    int i;

    if (is_double) {
        index = H;
        rm += M << 4;
        for(i = 0; i < (is_scalar?1:2); i++)
            res.df[i] = regs->v[rn].df[i] * regs->v[rm].df[index];
    } else {
        index = (H << 1) | L;
        rm += M << 4;
        for(i = 0; i < (is_scalar?1:(q?4:2)); i++)
            res.sf[i] = regs->v[rn].sf[i] * regs->v[rm].sf[index];
    }
    regs->v[rd] = res;
}

static void dis_fmulx_by_element(uint64_t _regs, uint32_t insn)
{
    dis_fmul_by_element(_regs, insn);
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

static void dis_uzp1_uzp2(uint64_t _regs, uint32_t insn)
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
                res.b[i] = regs->v[rn].b[2*i+part];
                res.b[q?i+8:i+4] = regs->v[rm].b[2*i+part];
            }
            break;
        case 1:
            for(i = 0; i < (q?4:2); i++) {
                res.h[i] = regs->v[rn].h[2*i+part];
                res.h[q?i+4:i+2] = regs->v[rm].h[2*i+part];
            }
            break;
        case 2:
            for(i = 0; i < (q?2:1); i++) {
                res.s[i] = regs->v[rn].s[2*i+part];
                res.s[q?i+2:i+1] = regs->v[rm].s[2*i+part];
            }
            break;
        case 3:
            assert(q);
            for(i = 0; i < 1; i++) {
                res.d[i] = regs->v[rn].d[2*i+part];
                res.d[i+1] = regs->v[rm].d[2*i+part];
            }
            break;
    }
    regs->v[rd] = res;    
}

static void dis_zip1_zip2(uint64_t _regs, uint32_t insn)
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
                res.b[2*i+0] = regs->v[rn].b[(part?(q?8:4):0)+i];
                res.b[2*i+1] = regs->v[rm].b[(part?(q?8:4):0)+i];
            }
            break;
        case 1:
            for(i = 0; i < (q?4:2); i++) {
                res.h[2*i+0] = regs->v[rn].h[(part?(q?4:2):0)+i];
                res.h[2*i+1] = regs->v[rm].h[(part?(q?4:2):0)+i];
            }
            break;
        case 2:
            for(i = 0; i < (q?2:1); i++) {
                res.s[2*i+0] = regs->v[rn].s[(part?(q?2:1):0)+i];
                res.s[2*i+1] = regs->v[rm].s[(part?(q?2:1):0)+i];
            }
            break;
        case 3:
            assert(q);
            for(i = 0; i < 1; i++) {
                res.d[2*i+0] = regs->v[rn].d[(part?1:0)+i];
                res.d[2*i+1] = regs->v[rm].d[(part?1:0)+i];
            }
            break;
        default:
            assert(0);
    }
    regs->v[rd] = res; 
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
            dis_saddlp_sadalp_uaddlp_uadalp(_regs, insn);
            break;
        case 3:
            dis_suqadd_usqadd(_regs, insn);
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
            dis_saddlp_sadalp_uaddlp_uadalp(_regs, insn);
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
        case 12:
            if (size&2)
                U?dis_fcmge_zero(_regs, insn):dis_fcmgt_zero(_regs, insn);
            else
                assert(0);
            break;
        case 13:
            if (size&2)
                U?dis_fcmle_zero(_regs, insn):dis_fcmeq_zero(_regs, insn);
            else
                assert(0);
            break;
        case 14:
            if (size&2)
                U?assert(0):dis_fcmlt_zero(_regs, insn);
            else
                assert(0);
            break;
        case 15:
            U?dis_fneg(_regs, insn):dis_fabs(_regs, insn);
            break;
        case 18:
            U?dis_sqxtun(_regs, insn):dis_xtn(_regs, insn);
            break;
        case 19:
            if (U)
                dis_shll(_regs, insn);
            else
                assert(0);
            break;
        case 20:
            dis_sqxtn_uqxtn(_regs, insn);
            break;
        case 22:
            if (size&2)
                assert(0);
            else
                U?dis_fcvtxn(_regs, insn):dis_fcvtn(_regs, insn);
            break;
        case 23:
            if (size&2)
                assert(0);
            else
                U?assert(0):dis_fcvtl(_regs, insn);
            break;
        case 24:
            if (size&2)
                U?assert(0):dis_frintp(_regs, insn);
            else
                U?dis_frinta(_regs, insn):dis_frintn(_regs, insn);
            break;
        case 25:
            if (size&2)
                U?dis_frinti(_regs, insn):dis_frintz(_regs, insn);
            else
                U?dis_frintx(_regs, insn):dis_frintm(_regs, insn);
            break;
        case 26:
            if (size&2)
                U?dis_fcvtpu(_regs, insn):dis_fcvtps(_regs, insn);
            else
                U?dis_fcvtnu(_regs, insn):dis_fcvtns(_regs, insn);
            break;
        case 27:
            if (size&2)
                U?dis_fcvtzu(_regs, insn):dis_fcvtzs(_regs, insn);
            else
                U?dis_fcvtmu(_regs, insn):dis_fcvtms(_regs, insn);
            break;
        case 28:
            if (size&2)
                U?dis_ursqrte(_regs, insn):dis_urecpe(_regs, insn);
            else
                U?dis_fcvtau(_regs, insn):dis_fcvtas(_regs, insn);
            break;
        case 29:
            if(size&2)
                assert(0);
            else
                U?dis_ucvtf(_regs, insn):dis_scvtf(_regs, insn);
            break;
        default:
            fatal("opcode = %d(0x%x) / U=%d / size=%d\n", opcode, opcode, U, size);
    }
}

void arm64_hlp_dirty_advanced_simd_scalar_two_reg_misc_simd(uint64_t _regs, uint32_t insn)
{
    int U = INSN(29,29);
    int opcode = INSN(16,12);
    int size1 = INSN(23,23);

    switch(opcode) {
        case 3:
            dis_suqadd_usqadd(_regs, insn);
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
        case 12:
            if (size1)
                U?dis_fcmge_zero(_regs, insn):dis_fcmgt_zero(_regs, insn);
            else
                assert(0);
            break;
        case 13:
            if (size1)
                U?dis_fcmle_zero(_regs, insn):dis_fcmeq_zero(_regs, insn);
            else
                assert(0);
            break;
        case 14:
            if (size1)
                U?assert(0):dis_fcmlt_zero(_regs, insn);
            else
                assert(0);
            break;
        case 18:
            U?dis_sqxtun(_regs, insn):assert(0);
            break;
        case 20:
            dis_sqxtn_uqxtn(_regs, insn);
            break;
        case 22:
            if (size1)
                assert(0);
            else
                U?dis_fcvtxn(_regs, insn):assert(0);
            break;
        case 26:
            if (size1)
                U?dis_fcvtpu(_regs, insn):dis_fcvtps(_regs, insn);
            else
                U?dis_fcvtnu(_regs, insn):dis_fcvtns(_regs, insn);
            break;
        case 27:
            if (size1)
                U?dis_fcvtzu(_regs, insn):dis_fcvtzs(_regs, insn);
            else
                U?dis_fcvtmu(_regs, insn):dis_fcvtms(_regs, insn);
            break;
        case 28:
            if (size1)
                assert(0);
            else
                U?dis_fcvtau(_regs, insn):dis_fcvtas(_regs, insn);
            break;
        case 29:
            if (size1)
                assert(0);
            else
                U?dis_ucvtf(_regs, insn):dis_scvtf(_regs, insn);
            break;
        default:
            fatal("opcode = %d(0x%x) / U=%d / size1=%d\n", opcode, opcode, U, size1);
    }
}

void arm64_hlp_dirty_advanced_simd_scalar_three_same_simd(uint64_t _regs, uint32_t insn)
{
    int U = INSN(29,29);
    int opcode = INSN(15,11);
    int size1 = INSN(23,23);

    switch(opcode) {
        case 1:
            dis_sqadd_uqadd(_regs, insn);
            break;
        case 5:
            dis_sqsub_uqsub(_regs, insn);
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
        case 8:
            dis_sshl_family(_regs, insn);
            break;
        case 9:
            dis_sshl_family(_regs, insn);
            break;
        case 10:
            dis_sshl_family(_regs, insn);
            break;
        case 11:
            dis_sshl_family(_regs, insn);
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
        case 26:
            assert(size1);
            U?dis_fabd(_regs, insn):assert(0);
            break;
        case 27:
            assert(size1==0);
            U?assert(0):dis_fmulx(_regs, insn);
            break;
        case 28:
            if (size1 == 0)
                U?dis_fcmge(_regs, insn):dis_fcmeq(_regs, insn);
            else
                U?dis_fcmgt(_regs, insn):assert(0);
            break;
        case 29:
            if (size1 == 0)
                U?dis_facge(_regs, insn):assert(0);
            else
                U?dis_facgt(_regs, insn):assert(0);
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
            dis_shadd_srhadd_uhadd_urhadd(_regs, insn);
            break;
        case 1:
            dis_sqadd_uqadd(_regs, insn);
            break;
        case 2:
            dis_shadd_srhadd_uhadd_urhadd(_regs, insn);
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
            dis_shsub_uhsub(_regs, insn);
            break;
        case 5:
            dis_sqsub_uqsub(_regs, insn);
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
        case 8:
            dis_sshl_family(_regs, insn);
            break;
        case 9:
            dis_sshl_family(_regs, insn);
            break;
        case 10:
            dis_sshl_family(_regs, insn);
            break;
        case 11:
            dis_sshl_family(_regs, insn);
            break;
        case 12:
            dis_smax_smin_umax_umin(_regs, insn);
            break;
        case 13:
            dis_smax_smin_umax_umin(_regs, insn);
            break;
        case 14:
            dis_sabd_saba_uabd_uaba(_regs, insn);
            break;
        case 15:
            dis_sabd_saba_uabd_uaba(_regs, insn);
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
            dis_smaxp_sminp_umaxp_uminp(_regs, insn);
            break;
        case 21:
            dis_smaxp_sminp_umaxp_uminp(_regs, insn);
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
        case 24:
            U?dis_fmaxnmp_fminnmp(_regs, insn):dis_fmaxnm_fminnm(_regs, insn);
            break;
        case 25:
            if (size&2)
                U?assert(0):dis_fmla_fmls(_regs, insn);
            else
                U?assert(0):dis_fmla_fmls(_regs, insn);
            break;
        case 26:
            if (size&2)
                U?dis_fabd(_regs, insn):dis_fadd_fsub(_regs, insn);
            else
                U?dis_faddp(_regs, insn):dis_fadd_fsub(_regs, insn);
            break;
        case 27:
            if (size&2)
                assert(0);
            else
                U?dis_fmul(_regs, insn):dis_fmulx(_regs, insn);
            break;
        case 28:
            if (size&2)
                U?dis_fcmgt(_regs, insn):assert(0);
            else
                U?dis_fcmge(_regs, insn):dis_fcmeq(_regs, insn);
            break;
        case 29:
            if (size&2)
                U?dis_facgt(_regs, insn):assert(0);
            else
                U?dis_facge(_regs, insn):assert(0);
            break;
        case 30:
            U?dis_fmaxp_fminp(_regs, insn):dis_fmax_fmin(_regs, insn);
            break;
        case 31:
            if (size&2)
                assert(0);
            else
                U?dis_fdiv(_regs, insn):assert(0);
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
            dis_saddl_ssubl_uaddl_usubl(_regs, insn);
            break;
        case 1:
            dis_saddw_ssubw_uaddw_usubw(_regs, insn);
            break;
        case 2:
            dis_saddl_ssubl_uaddl_usubl(_regs, insn);
            break;
        case 3:
            dis_saddw_ssubw_uaddw_usubw(_regs, insn);
            break;
        case 4: case 6:
            dis_addhn_raddhn_subhn_rsubhn(_regs, insn);
            break;
        case 5:
            dis_sabdl_sabal_uabdl_uabal(_regs, insn);
            break;
        case 7:
            dis_sabdl_sabal_uabdl_uabal(_regs, insn);
            break;
        case 8:
            dis_smlal_smlsl_umlal_umlsl(_regs, insn);
            break;
        case 9:
            U?assert(0):dis_sqdmlal_sqdmlsl(_regs, insn);
            break;
        case 10:
            dis_smlal_smlsl_umlal_umlsl(_regs, insn);
            break;
        case 11:
            U?assert(0):dis_sqdmlal_sqdmlsl(_regs, insn);
            break;
        case 12:
            dis_smull_umull(_regs, insn);
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
    //int size1 = INSN(23,23);

    switch(opcode) {
        case 12:
            assert(U);
            dis_fmaxnmp_fminnmp(_regs, insn);
            break;
        case 13:
            assert(U);
            dis_faddp(_regs, insn);
            break;
        case 15:
            assert(U);
            dis_fmaxp_fminp(_regs, insn);
            break;
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
    //int size1 = INSN(23,23);

    switch(opcode) {
        case 3:
            dis_saddlv_uaddlv(_regs, insn);
            break;
        case 10:
            dis_smaxv_sminv_umaxv_uminv(_regs, insn);
            break;
        case 12:
            dis_fmaxnmv_fminnmv(_regs, insn);
            break;
        case 15:
            dis_fmaxv_fminv(_regs, insn);
            break;
        case 26:
            dis_smaxv_sminv_umaxv_uminv(_regs, insn);
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
        case 1:
            assert(U==0);
            dis_fmla_fmls_by_element(_regs, insn);
            break;
        case 2:
            dis_smlal_smlsl_umlal_umlsl_by_element(_regs, insn);
            break;
        case 3:
            U?assert(0):dis_sqdmlal_sqdmlsl_by_element(_regs, insn);
            break;
        case 5:
            assert(U==0);
            dis_fmla_fmls_by_element(_regs, insn);
            break;
        case 6:
            dis_smlal_smlsl_umlal_umlsl_by_element(_regs, insn);
            break;
        case 7:
            U?assert(0):dis_sqdmlal_sqdmlsl_by_element(_regs, insn);
            break;
        case 8:
            assert(U == 0);
            dis_mul_by_element(_regs, insn);
            break;
        case 9:
            U?dis_fmulx_by_element(_regs, insn):dis_fmul_by_element(_regs, insn);
            break;
        case 10:
            dis_smull_umull_by_element(_regs, insn);
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
            dis_srshr_srsra_sshr_ssra_urshr_ursra_ushr_usra(_regs, insn);
            break;
        case 2:
            dis_srshr_srsra_sshr_ssra_urshr_ursra_ushr_usra(_regs, insn);
            break;
        case 4:
            dis_srshr_srsra_sshr_ssra_urshr_ursra_ushr_usra(_regs, insn);
            break;
        case 6:
            dis_srshr_srsra_sshr_ssra_urshr_ursra_ushr_usra(_regs, insn);
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
        case 14:
            U?dis_uqshl(_regs, insn):dis_sqshl(_regs, insn);
            break;
        case 16:
            U?dis_sqrshrun_sqshrun(_regs, insn):assert(0);
            break;
        case 17:
            U?dis_sqrshrun_sqshrun(_regs, insn):assert(0);
            break;
        case 18:
            dis_sqrshrn_sqshrn_uqrshrn_uqshrn(_regs, insn);
        case 19:
            dis_sqrshrn_sqshrn_uqrshrn_uqshrn(_regs, insn);
            break;
        case 28:
            U?dis_ucvtf_fixed(_regs, insn):dis_scvtf_fixed(_regs, insn);
            break;
        case 31:
            U?dis_fcvtzu_fixed(_regs, insn):dis_fcvtzs_fixed(_regs, insn);
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
        case 1:
            U?assert(0):dis_fmla_fmls_by_element(_regs, insn);
            break;
        case 3:
            U?assert(0):dis_sqdmlal_sqdmlsl_by_element(_regs, insn);
            break;
        case 5:
            U?assert(0):dis_fmla_fmls_by_element(_regs, insn);
            break;
        case 7:
            U?assert(0):dis_sqdmlal_sqdmlsl_by_element(_regs, insn);
            break;
        case 9:
            U?dis_fmulx_by_element(_regs, insn):dis_fmul_by_element(_regs, insn);
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
        case 1: case 5:
            dis_uzp1_uzp2(_regs, insn);
            break;
        case 3: case 7:
            dis_zip1_zip2(_regs, insn);
            break;
        case 2: case 6:
            dis_trn1_trn2(_regs, insn);
            break;
        default:
            fatal("opcode = %d(0x%x)\n", opcode, opcode);
    }
}
