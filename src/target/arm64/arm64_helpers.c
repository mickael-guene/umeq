#include <stdio.h>

#include "arm64_private.h"
#include "arm64_helpers.h"

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
    printf("pc  = 0x%016lx\n", context->regs.pc);
#ifdef DUMP_STACK
    for(i = 0 ;i < 16; i++) {
        if (context->regs.r[31] + i * 8 < context->sp_init)
            printf("0x%016lx [sp + 0x%02x] = 0x%016lx\n", context->regs.r[31] + i * 8, i * 8, *((uint64_t *)(context->regs.r[31] + i * 8)));
    }
#endif
}