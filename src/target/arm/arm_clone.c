#include <sys/syscall.h>
#define _GNU_SOURCE
#include <sched.h>
#include <assert.h>

#include "arm_private.h"
#include "arm_syscall.h"

/* FIXME: we assume a x86_64 host */

static int clone_thread_arm(struct arm_target *context)
{
    assert(0);
    return -1;
}

static int clone_vfork_arm(struct arm_target *context)
{
    assert(0);
    return -1;
}

static int clone_fork_arm(struct arm_target *context)
{
    /* just do the syscall */
    int res = syscall(SYS_clone,
                        (long) context->regs.r[0],
                        (long) context->regs.r[1],
                        (long) context->regs.r[2],
                        (long) context->regs.r[4],
                        (long) context->regs.r[3]);

    return res;
}

int arm_clone(struct arm_target *context)
{
    int res;
    int flags = context->regs.r[0];

    if ((flags & CLONE_SIGHAND) && (flags & CLONE_THREAD))
        res = clone_thread_arm(context);
    else if (flags & CLONE_VFORK)
        res = clone_vfork_arm(context);
    else
        res = clone_fork_arm(context);

    return res;
}
