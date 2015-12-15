/* This file is part of Umeq, an equivalent of qemu user mode emulation with improved robustness.
 *
 * Copyright (C) 2015 STMicroelectronics
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA.
 */

#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <sched.h>
#include <assert.h>
#include <sys/mman.h>
#include <string.h>
#include <asm/prctl.h>
#include <sys/prctl.h>

#include "arm_private.h"
#include "arm_syscall.h"
#include "cache.h"
#include "umeq.h"

/* FIXME: we assume a x86_64 host */
extern int loop(uint32_t entry, uint32_t stack_entry, uint32_t signum, void *parent_target);
extern int clone_asm(long number, ...);
extern void clone_exit_asm(void *stack , long stacksize, int res, void *patch_address);

static int is_syscall_error(int res)
{
    if (res < 0 && res > -4096)
        return 1;
    else
        return 0;
}

void clone_thread_trampoline_arm()
{
    struct tls_context *new_thread_tls_context;
    struct arm_target *parent_context;
    void *stack;
    void *patch_address;
    int res;

    /* FIXME: how to get tls area ? */
#if 1
    assert(0 && "implement me");
#else
    syscall(SYS_arch_prctl, ARCH_GET_FS, &new_thread_tls_context);
#endif
    assert(new_thread_tls_context != NULL);
    stack = (void *) new_thread_tls_context - mmap_size[memory_profile] + sizeof(struct tls_context);
    parent_context = (void *) new_thread_tls_context - sizeof(struct arm_target);

    res = loop(parent_context->regs.r[15], parent_context->regs.r[1], 0, &parent_context->target);

    /* release vma descr */
    patch_address = munmap_guest_ongoing(h_2_g(stack), mmap_size[memory_profile]);
    /* unmap thread stack and exit without using stack */
    clone_exit_asm(stack, mmap_size[memory_profile], res, patch_address);
}

static int clone_thread_arm(struct arm_target *context)
{
    int res = -1;
    guest_ptr guest_stack;
    void *stack;

    assert(0);
    //allocate memory for stub thread stack
    guest_stack = mmap_guest((uint64_t) NULL, mmap_size[memory_profile], PROT_EXEC|PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_GROWSDOWN, -1, 0);
    if (is_syscall_error(guest_stack))
        return guest_stack;
    stack = g_2_h(guest_stack);

    if (stack) {//to be check what is return value of mmap
        struct tls_context *new_thread_tls_context;
        struct arm_target *parent_target;

        stack = stack + mmap_size[memory_profile] - sizeof(struct arm_target) - sizeof(struct tls_context);
        //copy arm context onto stack
        memcpy(stack, context, sizeof(struct arm_target));
        //setup new_thread_tls_context
        parent_target = stack;
        new_thread_tls_context = stack + sizeof(struct arm_target);
        new_thread_tls_context->target = &parent_target->target;
        new_thread_tls_context->target_runtime = &parent_target->regs;
        //in case new created thread take a signal before thread context is setup, it will use
        //context of parent but with the stack of the new thread. Yet this is confuse but it allow
        //to take a signal before new guest thread context is init. Perhaps it will be a better idea
        //to prepare new thread context here and just do a copy in arm init ?
        parent_target->regs.r[13] = context->regs.r[1];
        // do the same for c13_tls2 so it has the correct value
        parent_target->regs.c13_tls2 = context->regs.r[3];
        //clone
        res = clone_asm(SYS_clone, (unsigned long) context->regs.r[0],
                                    stack,
                                    context->regs.r[2]?g_2_h(context->regs.r[2]):NULL,//ptid
                                    new_thread_tls_context,
                                    context->regs.r[4]?g_2_h(context->regs.r[4]):NULL);//ctid
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
                        (unsigned long) (context->regs.r[0] & ~(CLONE_VM | CLONE_SETTLS)),
                        NULL, //host fork use a new stack
                        context->regs.r[2]?g_2_h(context->regs.r[2]):NULL,
                        NULL, //continue to use parent tls settings
                        context->regs.r[4]?g_2_h(context->regs.r[4]):NULL);

    /* support use of a new guest stack */
    if (res == 0 && context->regs.r[1])
        context->regs.r[13] = context->regs.r[1];

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
