#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <sched.h>
#include <assert.h>

#include "arm64_private.h"
#include "arm64_syscall.h"

static long clone_thread_arm64(struct arm64_target *context)
{
    assert(0);
    return -1;
}

static long clone_vfork_arm64(struct arm64_target *context)
{
    assert(0);
    return -1;
}

static long clone_fork_arm64(struct arm64_target *context)
{
    /* just do the syscall */
    long res = syscall(SYS_clone,
                        (unsigned long) context->regs.r[0],
                        context->regs.r[1]?g_2_h_64(context->regs.r[1]):NULL,
                        context->regs.r[2]?g_2_h_64(context->regs.r[2]):NULL,
                        context->regs.r[4]?g_2_h_64(context->regs.r[4]):NULL,
                        context->regs.r[3]?g_2_h_64(context->regs.r[3]):NULL);

    return res;
}

long arm64_clone(struct arm64_target *context)
{
    long res;
    unsigned long flags = context->regs.r[0];

    if ((flags & CLONE_SIGHAND) && (flags & CLONE_THREAD))
        res = clone_thread_arm64(context);
    else if (flags & CLONE_VFORK)
        res = clone_vfork_arm64(context);
    else
        res = clone_fork_arm64(context);

    return res;
}
