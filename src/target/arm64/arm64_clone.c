#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <sched.h>
#include <assert.h>
#include <sys/mman.h>
#include <string.h>

#include "arm64_private.h"
#include "arm64_syscall.h"
#include "cache.h"

/* FIXME: we assume a x86_64 host */
extern int loop(uint64_t entry, uint64_t stack_entry, uint32_t signum, void *parent_target);
extern int clone_asm(long number, ...);
extern void clone_exit_asm(void *stack , long stacksize, int res);

void clone_thread_trampoline_arm64()
{
    void *sp = __builtin_frame_address(0) + 8;//rbp has been push on stack
    struct arm64_target *parent_context = (struct arm64_target *) sp;
    int res;
    void *stack = sp + sizeof(struct arm64_target) - CACHE_SIZE * 2;

    res = loop(parent_context->regs.pc, parent_context->regs.r[1], 0, &parent_context->target);

    /* unmap thread stack and exit without using stack */
    clone_exit_asm(stack, CACHE_SIZE * 2, res);
}

static long clone_thread_arm64(struct arm64_target *context)
{
    int res = -1;
    void *stack;

    //allocate memory for stub thread stack
    stack = mmap(NULL, CACHE_SIZE * 2, PROT_EXEC|PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_GROWSDOWN, -1, 0);

    if (stack) {//to be check what is return value of mmap
        stack = stack + CACHE_SIZE * 2 - sizeof(struct arm64_target);
        //copy arm64 context onto stack
        memcpy(stack, context, sizeof(struct arm64_target));
        //clone
        res = clone_asm(SYS_clone, context->regs.r[0],
                                     stack,
                                     context->regs.r[2]?g_2_h(context->regs.r[2]):NULL,//ptid
                                     context->regs.r[4]?g_2_h(context->regs.r[4]):NULL,//ctid
                                     NULL); //host will not have tls area, target tls area will be set later using r[3] content
        // only parent return here
    }

    return res;
}

static long clone_vfork_arm64(struct arm64_target *context)
{
    /* implement with fork to avoid sync problem but semantic is not fully preserved ... */

    return syscall(SYS_fork);
}

static long clone_fork_arm64(struct arm64_target *context)
{
    /* just do the syscall */
    long res = syscall(SYS_clone,
                        (unsigned long) context->regs.r[0],
                        context->regs.r[1]?g_2_h(context->regs.r[1]):NULL,
                        context->regs.r[2]?g_2_h(context->regs.r[2]):NULL,
                        context->regs.r[4]?g_2_h(context->regs.r[4]):NULL,
                        context->regs.r[3]?g_2_h(context->regs.r[3]):NULL);

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
