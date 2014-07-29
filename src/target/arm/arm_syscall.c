#include <stdio.h>
#include <assert.h>
#include <stddef.h>
#include <errno.h>

#include "arm_private.h"
#include "arm_helpers.h"
#include "sysnums-arm.h"
#include "hownums-arm.h"
#include "syscall32_64.h"

void arm_hlp_syscall(uint64_t regs)
{
    struct arm_target *context = container_of((void *) regs, struct arm_target, regs);
    uint32_t no = context->regs.r[7];
    Sysnum no_neutral;
    Syshow how;
    int res = ENOSYS;

    /* sanity check about range */
    if (no >= sysnums_arm_nb)
        fatal("Out of range syscall number %d >= %d\n", no, sysnums_arm_nb);
    no_neutral = sysnums_arm[context->regs.r[7]];
    how = syshow_arm[no_neutral];
    /* so how we handle this sycall */
    if (how == HOW_32_to_64) {
        res = syscall32_64(no_neutral, context->regs.r[0], context->regs.r[1], context->regs.r[2], 
                                            context->regs.r[3], context->regs.r[4], context->regs.r[5]);
    } else if (how == HOW_custom_implementation) {

    } else if (how == HOW_not_yet_supported) {
        fatal("Syscall not yet implemented no=%d/no_neutral=%d\n", no, no_neutral);
    } else if (how == HOW_not_implemented) {
        res = ENOSYS;
    } else {
        fatal("Unknown how .... %d\n", how);
    }

    context->regs.r[0] = res;
}
