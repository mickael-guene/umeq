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

#include "arm64_private.h"
#include "arm64_syscall.h"
#include "cache.h"
#include "umeq.h"

/* FIXME: we assume a x86_64 host */
extern int loop(uint64_t entry, uint64_t stack_entry, uint32_t signum, void *parent_target);
extern int clone_asm(long number, ...);
extern void clone_exit_asm(void *stack , long stacksize, int res, void *patch_address);

void clone_thread_trampoline_arm64()
{
    struct tls_context *new_thread_tls_context;
    struct arm64_target *parent_context;
    void *stack;
    void *patch_address;
    int res;

    syscall(SYS_arch_prctl, ARCH_GET_FS, &new_thread_tls_context);
    assert(new_thread_tls_context != NULL);
    stack = (void *) new_thread_tls_context - mmap_size[memory_profile] + sizeof(struct tls_context);
    parent_context = (void *) new_thread_tls_context - sizeof(struct arm64_target);

    res = loop(parent_context->regs.pc, parent_context->regs.r[1], 0, &parent_context->target);

    /* release vma descr */
    patch_address = munmap_guest_ongoing(h_2_g(stack), mmap_size[memory_profile]);
    /* unmap thread stack and exit without using stack */
    clone_exit_asm(stack, mmap_size[memory_profile], res, patch_address);
}

static long clone_thread_arm64(struct arm64_target *context)
{
    int res = -1;
    guest_ptr guest_stack;
    void *stack;

    //allocate memory for stub thread stack
    guest_stack = mmap_guest((uint64_t) NULL, mmap_size[memory_profile], PROT_EXEC|PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    stack = g_2_h(guest_stack);

    if (stack) {//to be check what is return value of mmap
        struct tls_context *new_thread_tls_context;
        struct arm64_target *parent_target;

        stack = stack + mmap_size[memory_profile] - sizeof(struct arm64_target) - sizeof(struct tls_context);
        //copy arm64 context onto stack
        memcpy(stack, context, sizeof(struct arm64_target));
        //setup new_thread_tls_context
        parent_target = stack;
        new_thread_tls_context = stack + sizeof(struct arm64_target);
        new_thread_tls_context->target = &parent_target->target;
        new_thread_tls_context->target_runtime = &parent_target->regs;
        //in case new created thread take a signal before thread context is setup, it will use
        //context of parent but with the stack of the new thread. Yet this is confuse but it allow
        //to take a signal before new guest thread context is init. Perhaps it will be a better idea
        //to prepare new thread context here and just do a copy in arm init ?
        parent_target->regs.r[31] = context->regs.r[1];
        // do the same for tpidr_el0 so it has the correct value
        parent_target->regs.tpidr_el0 = context->regs.r[3];
        //clone
        res = clone_asm(SYS_clone, context->regs.r[0],
                                     stack,
                                     context->regs.r[2]?g_2_h(context->regs.r[2]):NULL,//ptid
                                     context->regs.r[4]?g_2_h(context->regs.r[4]):NULL,//ctid
                                     new_thread_tls_context);
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
                        (unsigned long) (context->regs.r[0]& ~(CLONE_VM | CLONE_SETTLS)),
                        NULL,
                        context->regs.r[2]?g_2_h(context->regs.r[2]):NULL,
                        context->regs.r[4]?g_2_h(context->regs.r[4]):NULL,
                        NULL);
    /* support use of a new guest stack */
    if (res == 0 && context->regs.r[1])
        context->regs.r[31] = context->regs.r[1];

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
