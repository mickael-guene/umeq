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
