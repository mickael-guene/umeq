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

static int is_symlink(int dirfd, char *pathname)
{
    long res;
    struct neutral_stat64 stat64;

    res = syscall_neutral_64(PR_fstatat64, dirfd, (uint64_t)pathname, (uint64_t)&stat64, AT_SYMLINK_NOFOLLOW, 0, 0);

    return res ? 0 : ((stat64.st_mode & S_IFMT) == S_IFLNK);
}

static long recursive_proc_self_exe(int dirfd, char *pathname, char *buf, size_t bufsiz)
{
    long res = -EINVAL;
    char buffer[4096];

    strcpy(buffer, pathname);
    while(is_symlink(dirfd, buffer)) {
        res = syscall_neutral_64(PR_readlinkat, dirfd, (uint64_t) buffer, (uint64_t) buffer, 4095, 0, 0);
        if (res < 0)
            break;
        buffer[res] = '\0';
    }
    if (res > 0) {
        res = res < bufsiz ? res : bufsiz;
        memcpy(buf, buffer, res);
    }

    return res;
}

long arm64_readlinkat(struct arm64_target *context)
{
    long res;
    const char *pathname = (const char *) g_2_h(context->regs.r[1]);

    if (strcmp(pathname, "/proc/self/exe") == 0) {
        res = recursive_proc_self_exe(context->regs.r[0], exe_filename, g_2_h(context->regs.r[2]), context->regs.r[3]);
    } else
        res = syscall_adapter_guest64(PR_readlinkat, context->regs.r[0], context->regs.r[1], context->regs.r[2],
                                                     context->regs.r[3], context->regs.r[4], context->regs.r[5]);

    return res;
}