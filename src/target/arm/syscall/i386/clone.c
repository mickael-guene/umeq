/* This file is part of Umeq, an equivalent of qemu user mode emulation with improved robustness.
 *
 * Copyright (C) 2016 STMicroelectronics
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
#include <linux/unistd.h>
#include <asm/ldt.h>
#include <assert.h>

#include "arm_private.h"
#include "umeq.h"

extern int clone_asm(long number, ...);

/* FIXME : factorize this code somewhere with umeq.c */
# ifndef TLS_GET_GS
#  define TLS_GET_GS() \
  ({ int __seg; __asm ("movw %%gs, %w0" : "=q" (__seg)); __seg & 0xffff; })
# endif

# ifndef TLS_SET_GS
#  define TLS_SET_GS(val) \
  __asm ("movw %w0, %%gs" :: "q" (val))
# endif

static int get_thread_area(struct user_desc *u_info)
{
    return syscall(SYS_get_thread_area, u_info);
}

static void setup_desc(struct tls_context *tls_context, struct user_desc *desc)
{
    int res;;

    desc->entry_number = TLS_GET_GS() >> 3;
    res = get_thread_area(desc);
    assert(res == 0);
    desc->base_addr = (int) tls_context;
}

int clone_thread_host(struct arm_target *context, void *stack, struct tls_context *new_thread_tls_context)
{
    struct user_desc desc;

    setup_desc(new_thread_tls_context, &desc);
    //clone
    return clone_asm(SYS_clone, (unsigned long) context->regs.r[0],
                                stack,
                                context->regs.r[2]?g_2_h(context->regs.r[2]):NULL,//ptid
                                &desc,
                                context->regs.r[4]?g_2_h(context->regs.r[4]):NULL);//ctid
}

int clone_fork_host(struct arm_target *context)
{
    return syscall(SYS_clone,
                   (unsigned long) (context->regs.r[0] & ~(CLONE_VM | CLONE_SETTLS)),
                   NULL, //host fork use a new stack
                   context->regs.r[2]?g_2_h(context->regs.r[2]):NULL,
                   NULL, //continue to use parent tls settings
                   context->regs.r[4]?g_2_h(context->regs.r[4]):NULL);
}
