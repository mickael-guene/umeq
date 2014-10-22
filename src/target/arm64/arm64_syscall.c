#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <unistd.h>
#include <sys/syscall.h>
#include <assert.h>
#include <errno.h>

#include "arm64_private.h"
#include "sysnums-arm64.h"
#include "hownums-arm64.h"
#include "runtime.h"
#include "syscall64_64.h"
#include "arm64_syscall.h"

void arm64_hlp_syscall(uint64_t regs)
{
    struct arm64_target *context = container_of((void *) regs, struct arm64_target, regs);
    uint32_t no = context->regs.r[8];
    Sysnum no_neutral;
    Syshow how;
    long res = ENOSYS;

    /* syscall entry sequence */
    context->regs.is_in_syscall = 1;
    syscall((long) 313, 0);
    /* translate syscall nb into neutral no */
    if (no >= sysnums_arm64_nb)
        fatal("Out of range syscall number %d >= %d\n", no, sysnums_arm64_nb);
    else
        no_neutral = sysnums_arm64[no];
    how = syshow_arm64[no_neutral];

	/* so how we handle this sycall */
    if (how == HOW_64_to_64) {
        res = syscall64_64(no_neutral, context->regs.r[0], context->regs.r[1], context->regs.r[2], 
                                            context->regs.r[3], context->regs.r[4], context->regs.r[5]);
    } else if (how == HOW_custom_implementation) {
        switch(no_neutral) {
            case PR_uname:
                res = arm64_uname(context);
                break;
            case PR_brk:
                res = arm64_brk(context);
                break;
            case PR_openat:
                res = arm64_openat(context);
                break;
            case PR_fstat:
                res = arm64_fstat(context);
                break;
            case PR_rt_sigaction:
                res = arm64_rt_sigaction(context);
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
    /* syscall exit sequence */
    context->regs.is_in_syscall = 2;
    syscall((long) 313, 1);
    /* no more in syscall */
    context->regs.is_in_syscall = 0;
}
