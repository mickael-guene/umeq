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

#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <unistd.h>
#include <sys/syscall.h>   /* For SYS_xxx definitions */
#include <sys/uio.h>

#include "syscall32_64_types.h"
#include "syscall32_64_private.h"

int writev_s3264(uint32_t fd_p, uint32_t iov_p, uint32_t iovcnt_p)
{
	int res_total = 0;
    int res;
    int fd = (int) fd_p;
    struct iovec_32 *iov_guest = (struct iovec_32 *) g_2_h(iov_p);
    int iovcnt = (int) iovcnt_p;
    int i;

    /* FIXME: here we loose semantic
        - use lock but don't work with signals
        - use kernel syscall
     */
    for(i = 0; i < iovcnt; i++) {
        res = syscall(SYS_write, fd, g_2_h(iov_guest->iov_base), iov_guest->iov_len);
        if (res < 0) {
            res_total = res;
            break;
        }
        iov_guest++;
        res_total += res;
    }

    return res_total;
}
