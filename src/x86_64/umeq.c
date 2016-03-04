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

#include "umeq.h"
#include "runtime.h"

/* public api */
void setup_thread_area(struct tls_context *main_thread_tls_context)
{
    main_thread_tls_context->target = NULL;
    main_thread_tls_context->target_runtime = NULL;

    syscall(SYS_arch_prctl, ARCH_SET_FS, main_thread_tls_context);
}

struct tls_context *get_tls_context()
{
    struct tls_context *tls_context;

    syscall(SYS_arch_prctl, ARCH_GET_FS, &tls_context);

    return tls_context;
}
