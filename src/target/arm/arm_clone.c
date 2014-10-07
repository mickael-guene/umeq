#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <sched.h>
#include <assert.h>
#include <sys/mman.h>
#include <string.h>

#include "arm_private.h"
#include "arm_syscall.h"
#include "cache.h"

/* FIXME: we assume a x86_64 host */
extern int loop(uint32_t entry, uint32_t stack_entry, uint32_t signum, void *parent_target);
extern int clone_asm(long number, ...);

void clone_thread_trampoline_arm()
{
    void *sp = __builtin_frame_address(0) + 8;//rbp has been push on stack
    struct arm_target *parent_context = (struct arm_target *) sp;

    loop(parent_context->regs.r[15], parent_context->regs.r[1], 0, &parent_context->target);
}

static int clone_thread_arm(struct arm_target *context)
{
    int res = -1;
    void *stack;

    //allocate memory for stub thread stack
    stack = mmap(NULL, CACHE_SIZE * 2, PROT_EXEC|PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_GROWSDOWN, -1, 0);

    if (stack) {//to be check what is return value of mmap
        stack = stack + CACHE_SIZE * 2 - sizeof(struct arm_target);
        //copy arm context onto stack
        memcpy(stack, context, sizeof(struct arm_target));
        //clone
        res = clone_asm(SYS_clone, (unsigned long) context->regs.r[0],
                                    stack,
                                    context->regs.r[2]?g_2_h(context->regs.r[2]):NULL,//ptid
                                    context->regs.r[4]?g_2_h(context->regs.r[4]):NULL,//ctid
                                    NULL); //host will not have tls area, target tls area will be set later using r[3] content
        // only parent return here
    }

    return res;
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
                        (unsigned long) context->regs.r[0],
                        context->regs.r[1]?g_2_h(context->regs.r[1]):NULL,
                        context->regs.r[2]?g_2_h(context->regs.r[2]):NULL,
                        context->regs.r[4]?g_2_h(context->regs.r[4]):NULL,
                        context->regs.r[3]?g_2_h(context->regs.r[3]):NULL);

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
