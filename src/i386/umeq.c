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

#define _GNU_SOURCE        /* or _BSD_SOURCE or _SVID_SOURCE */
#include <unistd.h>
#include <sys/syscall.h>
#include <asm/prctl.h>
#include <sys/prctl.h>
#include <asm/ldt.h>

#include "umeq.h"
#include "runtime.h"

# ifndef TLS_GET_GS
#  define TLS_GET_GS() \
  ({ int __seg; __asm ("movw %%gs, %w0" : "=q" (__seg)); __seg & 0xffff; })
# endif

# ifndef TLS_SET_GS
#  define TLS_SET_GS(val) \
  __asm ("movw %w0, %%gs" :: "q" (val))
# endif


static int set_thread_area(struct user_desc *u_info)
{
    return syscall(SYS_set_thread_area, u_info);
}

static int get_thread_area(struct user_desc *u_info)
{
    return syscall(SYS_get_thread_area, u_info);
}

/* public api */
void setup_thread_area(struct tls_context *main_thread_tls_context)
{
    main_thread_tls_context->target = NULL;
    main_thread_tls_context->target_runtime = NULL;
    struct user_desc desc;
    int res;

    desc.entry_number = -1;
    desc.base_addr = (int) main_thread_tls_context;
    desc.limit = 0xfffff; /* We use 4GB which is 0xfffff pages. */
    desc.seg_32bit = 1;
    desc.contents = 0;
    desc.read_exec_only = 0;
    desc.limit_in_pages = 1;
    desc.seg_not_present = 0;
    desc.useable = 1;
    res = set_thread_area(&desc);
    assert(res == 0);
    TLS_SET_GS (desc.entry_number * 8 + 3);
}

struct tls_context *get_tls_context()
{
    struct user_desc desc;
    int res;

    desc.entry_number = TLS_GET_GS() >> 3;
    res = get_thread_area(&desc);
    assert(res == 0);

    return (struct tls_context *) desc.base_addr;
}
