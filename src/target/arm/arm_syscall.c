#include <stdio.h>
#include <assert.h>
#include <stddef.h>
#include <errno.h>

#include "arm_private.h"
#include "arm_helpers.h"
#include "sysnums-arm.h"
#include "hownums-arm.h"
#include "syscall32_64.h"
#include "runtime.h"
#include "arm_syscall.h"

void arm_hlp_syscall(uint64_t regs)
{
    struct arm_target *context = container_of((void *) regs, struct arm_target, regs);
    uint32_t no = context->regs.r[7];
    Sysnum no_neutral;
    Syshow how;
    int res = ENOSYS;

    /* translate syscall nb into neutral no */
    if (no == 0xf0005)
        no_neutral = PR_ARM_set_tls;
    else if (no >= sysnums_arm_nb)
        fatal("Out of range syscall number %d >= %d\n", no, sysnums_arm_nb);
    else
        no_neutral = sysnums_arm[context->regs.r[7]];
    how = syshow_arm[no_neutral];
    /* so how we handle this sycall */
    if (how == HOW_32_to_64) {
        res = syscall32_64(no_neutral, context->regs.r[0], context->regs.r[1], context->regs.r[2], 
                                            context->regs.r[3], context->regs.r[4], context->regs.r[5]);
    } else if (how == HOW_custom_implementation) {
        switch(no_neutral) {
            case PR_open:
                res = arm_open(context);
                break;
            case PR_openat:
                res = arm_openat(context);
                break;
            case PR_uname:
                res = arm_uname(context);
                break;
            case PR_brk:
                res = arm_brk(context);
                break;
            case PR_ARM_set_tls:
                context->regs.c13_tls2 = context->regs.r[0];
                res = 0;
                break;
            case PR_rt_sigaction:
                res = arm_rt_sigaction(context);
                break;
            case PR_clone:
                res = arm_clone(context);
                break;
            case PR_sigreturn:
                context->isLooping = 0;
                context->exitStatus = 0;
                res = 0;
                break;
            default:
                fatal("You say custom but you don't implement it %d\n", no_neutral);
        }
    } else if (how == HOW_not_yet_supported) {
        fatal("Syscall not yet implemented no=%d/no_neutral=%d\n", no, no_neutral);
    } else if (how == HOW_not_implemented) {
        res = ENOSYS;
    } else {
        fatal("Unknown how .... %d\n", how);
    }

    context->regs.r[0] = res;
}
