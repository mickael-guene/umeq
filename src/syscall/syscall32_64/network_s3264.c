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

#include "syscall32_64_types.h"
#include "syscall32_64_private.h"

int recvmsg_s3264(uint32_t sockfd_p, uint32_t msg_p, uint32_t flags_p)
{
	int res;
	int sockfd = (int) sockfd_p;
	struct msghdr_32 *msg_guest = (struct msghdr_32 *) g_2_h(msg_p);
	int flags = (int) flags_p;
	struct iovec iovec[16]; //max 16 iovect. If more is need then use mmap or alloca
    struct msghdr msg;
    int i;

    assert(msg_guest->msg_iovlen <= 16);

    msg.msg_name = msg_guest->msg_name?(void *) g_2_h(msg_guest->msg_name):NULL;
    msg.msg_namelen = msg_guest->msg_namelen;
    for(i = 0; i < msg_guest->msg_iovlen; i++) {
        struct iovec_32 *iovec_guest = (struct iovec_32 *) g_2_h(msg_guest->msg_iov + sizeof(struct iovec_32) * i);

        iovec[i].iov_base = (void *) g_2_h(iovec_guest->iov_base);
        iovec[i].iov_len = iovec_guest->iov_len;
    }
    msg.msg_iov = iovec;
    msg.msg_iovlen = msg_guest->msg_iovlen;
    msg.msg_control = (void *) g_2_h(msg_guest->msg_control);
    msg.msg_controllen = msg_guest->msg_controllen;
    msg.msg_flags = msg_guest->msg_flags;

    res = syscall(SYS_recvmsg, sockfd, &msg, flags);
    if (res >= 0) {
        msg_guest->msg_namelen = msg.msg_namelen;
        msg_guest->msg_controllen = msg.msg_controllen;
    }

    return res;
}

int sendmsg_s3264(uint32_t sockfd_p, uint32_t msg_p, uint32_t flags_p)
{
    int res;
    int sockfd = (int) sockfd_p;
    struct msghdr_32 *msg_guest = (struct msghdr_32 *) g_2_h(msg_p);
    int flags = (int) flags_p;
    struct iovec iovec[16]; //max 16 iovect. If more is need then use mmap or alloca
    struct msghdr msg;
    int i;

    assert(msg_guest->msg_iovlen <= 16);

    msg.msg_name = msg_guest->msg_name?(void *) g_2_h(msg_guest->msg_name):NULL;
    msg.msg_namelen = msg_guest->msg_namelen;
    for(i = 0; i < msg_guest->msg_iovlen; i++) {
        struct iovec_32 *iovec_guest = (struct iovec_32 *) g_2_h(msg_guest->msg_iov + sizeof(struct iovec_32) * i);

        iovec[i].iov_base = (void *) g_2_h(iovec_guest->iov_base);
        iovec[i].iov_len = iovec_guest->iov_len;
    }
    msg.msg_iov = iovec;
    msg.msg_iovlen = msg_guest->msg_iovlen;
    msg.msg_control = (void *) g_2_h(msg_guest->msg_control);
    msg.msg_controllen = msg_guest->msg_controllen;
    msg.msg_flags = msg_guest->msg_flags;

    res = syscall(SYS_sendmsg, sockfd, &msg, flags);

    return res;
}

int sendmmsg_s3264(uint32_t sockfd_p, uint32_t msgvec_p, uint32_t vlen_p, uint32_t flags_p)
{
    int res;
    int sockfd = (int) sockfd_p;
    struct mmsghdr_32 *msgvec_guest = (struct mmsghdr_32 *) g_2_h(msgvec_p);
    unsigned int vlen = (unsigned int) vlen_p;
    unsigned int flags = (unsigned int) flags_p;
    struct mmsghdr *msgvec = (struct mmsghdr *) alloca(vlen * sizeof(struct mmsghdr));
    int i, j;

    for(i = 0; i < vlen; i++) {
        msgvec[i].msg_hdr.msg_name = msgvec_guest[i].msg_hdr.msg_name?(void *) g_2_h(msgvec_guest[i].msg_hdr.msg_name):NULL;
        msgvec[i].msg_hdr.msg_namelen = msgvec_guest[i].msg_hdr.msg_namelen;
        msgvec[i].msg_hdr.msg_iov = (struct iovec *) alloca(msgvec_guest[i].msg_hdr.msg_iovlen * sizeof(struct iovec));
        for(j = 0; j < msgvec_guest[i].msg_hdr.msg_iovlen; j++) {
            struct iovec_32 *iovec_guest = (struct iovec_32 *) g_2_h(msgvec_guest[i].msg_hdr.msg_iov + sizeof(struct iovec_32) * j);

            msgvec[i].msg_hdr.msg_iov[j].iov_base = (void *) g_2_h(iovec_guest->iov_base);
            msgvec[i].msg_hdr.msg_iov[j].iov_len = iovec_guest->iov_len;
        }
        msgvec[i].msg_hdr.msg_iovlen = msgvec_guest[i].msg_hdr.msg_iovlen;
        msgvec[i].msg_hdr.msg_control = (void *) g_2_h(msgvec_guest[i].msg_hdr.msg_control);
        msgvec[i].msg_hdr.msg_controllen = msgvec_guest[i].msg_hdr.msg_controllen;
        msgvec[i].msg_hdr.msg_flags = msgvec_guest[i].msg_hdr.msg_flags;
        msgvec[i].msg_len = msgvec_guest[i].msg_len;
    }
    res = syscall(SYS_sendmmsg, sockfd, msgvec, vlen, flags);
    for(i = 0; i < vlen; i++)
        msgvec_guest[i].msg_len = msgvec[i].msg_len;

    return res;
}
