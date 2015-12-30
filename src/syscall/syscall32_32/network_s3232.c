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
#include <sys/socket.h>
#include <assert.h>
#include <errno.h>

#include "syscall32_32_types.h"
#include "syscall32_32_private.h"

#include <stdio.h>

int recvmsg_s3232(uint32_t sockfd_p, uint32_t msg_p, uint32_t flags_p)
{
	int res;
	int sockfd = (int) sockfd_p;
	struct msghdr *msg_guest = (struct msghdr *) g_2_h(msg_p);
	int flags = (int) flags_p;
	struct iovec iovec[16]; //max 16 iovect. If more is need then use mmap or alloca
    struct msghdr msg;
    int i;
    unsigned long args[3];

    /* sanity checks */
    if (!msg_guest->msg_iov)
        return -EFAULT;
    if (msg_guest->msg_iovlen >= 16)
        return -EMSGSIZE;
    //assert(msg_guest->msg_iovlen <= 16);

    msg.msg_name = msg_guest->msg_name?(void *) g_2_h(msg_guest->msg_name):NULL;
    msg.msg_namelen = msg_guest->msg_namelen;
    for(i = 0; i < msg_guest->msg_iovlen; i++) {
        struct iovec *iovec_guest = (struct iovec *) g_2_h(&msg_guest->msg_iov[i]);

        iovec[i].iov_base = (void *) g_2_h(iovec_guest->iov_base);
        iovec[i].iov_len = iovec_guest->iov_len;
    }
    msg.msg_iov = iovec;
    msg.msg_iovlen = msg_guest->msg_iovlen;
    msg.msg_control = (void *) g_2_h(msg_guest->msg_control);
    msg.msg_controllen = msg_guest->msg_controllen;
    msg.msg_flags = msg_guest->msg_flags;

    args[0] = sockfd;
    args[1] = (long)(struct msghdr *) &msg;
    args[2] = flags;
    res = syscall(SYS_socketcall, 17/*sys_recvmsg*/, args);
    if (res >= 0) {
        msg_guest->msg_namelen = msg.msg_namelen;
        msg_guest->msg_controllen = msg.msg_controllen;
    }

    return res;
}

int sendmsg_s3232(uint32_t sockfd_p, uint32_t msg_p, uint32_t flags_p)
{
    int res;
    int sockfd = (int) sockfd_p;
    struct msghdr *msg_guest = (struct msghdr *) g_2_h(msg_p);
    int flags = (int) flags_p;
    struct iovec iovec[16]; //max 16 iovect. If more is need then use mmap or alloca
    struct msghdr msg;
    int i;
    unsigned long args[3];

    assert(msg_guest->msg_iovlen <= 16);

    msg.msg_name = msg_guest->msg_name?(void *) g_2_h(msg_guest->msg_name):NULL;
    msg.msg_namelen = msg_guest->msg_namelen;
    for(i = 0; i < msg_guest->msg_iovlen; i++) {
        struct iovec *iovec_guest = (struct iovec *) g_2_h(&msg_guest->msg_iov[i]);

        iovec[i].iov_base = (void *) g_2_h(iovec_guest->iov_base);
        iovec[i].iov_len = iovec_guest->iov_len;
    }
    msg.msg_iov = iovec;
    msg.msg_iovlen = msg_guest->msg_iovlen;
    msg.msg_control = (void *) g_2_h(msg_guest->msg_control);
    msg.msg_controllen = msg_guest->msg_controllen;
    msg.msg_flags = msg_guest->msg_flags;

    args[0] = sockfd;
    args[1] = (long)(struct msghdr *) &msg;
    args[2] = flags;
    res = syscall(SYS_socketcall, 16/*sys_sendmsg*/, args);

    return res;
}
