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
#include <sys/uio.h>
#include <errno.h>

#include "syscall32_32_private.h"

/*
int readv_s3232(uint32_t fd_p, uint32_t iov_p, uint32_t iovcnt_p)
{
    int res;
    int fd = (int) fd_p;
    struct iovec_32 *iov_guest = (struct iovec_32 *) g_2_h(iov_p);
    int iovcnt = (int) iovcnt_p;
    struct iovec *iov;
    int i;

    if (iovcnt < 0)
        return -EINVAL;
    iov = (struct iovec *) alloca(sizeof(struct iovec) * iovcnt);
    for(i = 0; i < iovcnt; i++) {
        iov[i].iov_base = g_2_h(iov_guest[i].iov_base);
        iov[i].iov_len = iov_guest[i].iov_len;
    }
    res = syscall(SYS_readv, fd, iov, iovcnt);

    return res;
}*/

int writev_s3232(uint32_t fd_p, uint32_t iov_p, uint32_t iovcnt_p)
{
    int res;
    int fd = (int) fd_p;
    struct iovec *iov_guest = (struct iovec *) g_2_h(iov_p);
    int iovcnt = (int) iovcnt_p;
    struct iovec *iov;
    int i;

    if (iovcnt < 0)
        return -EINVAL;
    iov = (struct iovec *) alloca(sizeof(struct iovec) * iovcnt);
    for(i = 0; i < iovcnt; i++) {
        iov[i].iov_base = g_2_h(iov_guest[i].iov_base);
        iov[i].iov_len = iov_guest[i].iov_len;
    }
    res = syscall(SYS_writev, fd, iov, iovcnt);

    return res;
}
