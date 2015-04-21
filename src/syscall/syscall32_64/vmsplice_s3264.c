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
#include <fcntl.h>
#include <sys/uio.h>

#include "syscall32_64_types.h"
#include "syscall32_64_private.h"

int vmsplice_s3264(uint32_t fd_p, uint32_t iov_p, uint32_t nr_segs_p, uint32_t flags_p)
{
	int res;
	int fd = (int) fd_p;
	struct iovec_32 *iov_guest = (struct iovec_32 *) g_2_h(iov_p);
	unsigned long nr_segs = (unsigned long) nr_segs_p;
	unsigned int flags = (unsigned int) flags_p;
	struct iovec *iov;
    int i;

    iov = (struct iovec *) alloca(sizeof(struct iovec) * nr_segs);
    for(i = 0; i < nr_segs; i++) {
        iov[i].iov_base = g_2_h(iov_guest->iov_base);
        iov[i].iov_len = iov_guest->iov_len;
    }
    res = syscall(SYS_vmsplice, fd, iov, nr_segs, flags);

    return res;
}
