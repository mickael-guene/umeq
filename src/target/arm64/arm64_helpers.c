#include <stdio.h>
#include <assert.h>

#include "arm64_private.h"
#include "arm64_helpers.h"
#include "runtime.h"

//#define DUMP_STACK 1

void arm64_hlp_dump(uint64_t regs)
{
    struct arm64_target *context = container_of((void *) regs, struct arm64_target, regs);
    int i;

    printf("==============================================================================\n\n");
    for(i = 0; i < 32; i++) {
        printf("r[%2d] = 0x%016lx  ", i, context->regs.r[i]);
        if (i % 4 == 3)
            printf("\n");
    }
    printf("nzcv  = 0x%08x\n", context->regs.nzcv);
    printf("pc    = 0x%016lx\n", context->regs.pc);
#ifdef DUMP_STACK
    for(i = 0 ;i < 16; i++) {
        if (context->regs.r[31] + i * 8 < context->sp_init)
            printf("0x%016lx [sp + 0x%02x] = 0x%016lx\n", context->regs.r[31] + i * 8, i * 8, *((uint64_t *)(context->regs.r[31] + i * 8)));
    }
#endif
}

uint32_t arm64_hlp_compute_next_nzcv_32(uint64_t context, uint32_t opcode, uint32_t op1, uint32_t op2, uint32_t oldnzcv)
{
    int n, z, c, v;
    enum ops ops = opcode;
    uint32_t calc = 0;
    uint32_t c_in = (oldnzcv >> 29) & 1;

    /*fprintf(stderr, "context = 0x%016lx\n", context);
    fprintf(stderr, "opcode = 0x%08x\n", opcode);
    fprintf(stderr, "op1 = 0x%08x\n", op1);
    fprintf(stderr, "op2 = 0x%08x\n", op2);
    fprintf(stderr, "oldcpsr = 0x%018x\n", oldnzcv);*/

    c = oldnzcv & 0x20000000;
    v = oldnzcv & 0x10000000;
    switch(ops) {
        case OPS_SUB:
            calc = op1 - op2;
            c = (op1 >= op2)?0x20000000:0;
            v = (((op1 ^ op2) & (op1 ^ calc)) >> 3) & 0x10000000;
            break;
        default:
            fatal("ops = %d\n", ops);
    }

    n = (calc & 0x80000000);
    z = (calc == 0)?0x40000000:0;

    return n|z|c|v|(oldnzcv&0x0fffffff);
}

uint32_t arm64_hlp_compute_next_nzcv_64(uint64_t context, uint32_t opcode, uint64_t op1, uint64_t op2, uint32_t oldnzcv)
{
    fprintf(stderr, "context = 0x%016lx\n", context);
    fprintf(stderr, "opcode = 0x%08x\n", opcode);
    fprintf(stderr, "op1 = 0x%016lx\n", op1);
    fprintf(stderr, "op2 = 0x%016lx\n", op2);
    fprintf(stderr, "oldcpsr = 0x%018x\n", oldnzcv);

    assert(0);

    return 0;
}

uint32_t arm64_hlp_compute_flags_pred(uint64_t context, uint32_t cond, uint32_t nzcv)
{
    uint32_t pred = 0;

    switch(cond >> 1) {
        case 0://EQ + NE
            pred = (nzcv & 0x40000000)?0:1;
            break;
        case 1://HS + LO
            pred = (nzcv & 0x20000000)?0:1;
            break;
       case 2://MI + PL
            pred = (nzcv & 0x80000000)?0:1;
            break;
       case 3://VS + VC
            pred = (nzcv & 0x10000000)?0:1;
            break;
       case 4://HI + LS
            pred = ((nzcv & 0x20000000) && !(nzcv & 0x40000000))?0:1;
            break;
       case 5://GE + LT
            {
                uint32_t n = (nzcv >> 31) & 1;
                uint32_t v = (nzcv >> 28) & 1;
                pred = (n == v)?0:1;
            }
            break;
       case 6://GT + LE
            {
                uint32_t n = (nzcv >> 31) & 1;
                uint32_t z = (nzcv >> 30) & 1;
                uint32_t v = (nzcv >> 28) & 1;
                pred = ((z == 0) && (n == v))?0:1;
            }
            break;
        default:
            assert(0);
    }
    //invert cond
    if (cond&1)
        pred = 1 - pred;

    return pred;
}
