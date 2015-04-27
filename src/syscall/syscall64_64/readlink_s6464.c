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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#include "umeq.h"
#include "runtime.h"
#include "syscall64_64_private.h"

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

long readlinkat_s6464(uint64_t dirfd_p, uint64_t pathname_p, uint64_t buf_p, uint64_t bufsiz_p)
{
    long res;
    int dirfd = (int) dirfd_p;
    char *pathname = (char *) g_2_h(pathname_p);
    char *buf = (char *) g_2_h(buf_p);
    size_t bufsiz = (size_t) bufsiz_p;

    if (strcmp("/proc/self/exe", pathname))
        res = syscall(SYS_readlinkat, dirfd, pathname, buf, bufsiz);
    else
        res = readlinkat_proc_self_exe(buf, bufsiz);

    return res;
}
