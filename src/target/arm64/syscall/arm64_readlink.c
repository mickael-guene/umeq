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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <sys/mman.h>

#include "arm64_private.h"
#include "arm64_syscall.h"
#include "runtime.h"
#include "umeq.h"
#include "syscalls_neutral.h"
#include "syscalls_neutral_types.h"

static long readlinkat_proc_self_exe(char *buf, size_t bufsiz)
{
    long res;
    int fd;
    char fd_buffer[32];

    fd = open(exe_filename, O_RDONLY);
    if (fd >= 0) {
        sprintf(fd_buffer, "/proc/self/fd/%d", fd);
        res = syscall(SYS_readlink, fd_buffer, buf, bufsiz);
        close(fd);
    } else
        res = fd;

    return res;
}

long arm64_readlinkat(struct arm64_target *context)
{
    long res;
    const char *pathname = (const char *) g_2_h(context->regs.r[1]);

    if (strcmp(pathname, "/proc/self/exe") == 0) {
        res = readlinkat_proc_self_exe(g_2_h(context->regs.r[2]), context->regs.r[3]);
    } else
        res = syscall_adapter_guest64(PR_readlinkat, context->regs.r[0], context->regs.r[1], context->regs.r[2],
                                                     context->regs.r[3], context->regs.r[4], context->regs.r[5]);

    return res;
}