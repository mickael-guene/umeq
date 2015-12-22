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
#include <sys/select.h>

#include "syscall32_32_types.h"
#include "syscall32_32_private.h"

struct data_32_internal {
    uint32_t ss;
    uint32_t ss_len;
};

int pselect6_s3232(uint32_t nfds_p, uint32_t readfds_p, uint32_t writefds_p, uint32_t exceptfds_p, uint32_t timeout_p, uint32_t data_p)
{
    int res;
    int nfds = (int) nfds_p;
    fd_set *readfds = (fd_set *) g_2_h(readfds_p);
    fd_set *writefds = (fd_set *) g_2_h(writefds_p);
    fd_set *exceptfds = (fd_set *) g_2_h(exceptfds_p);
    struct timespec *timeout_guest = (struct timespec *) g_2_h(timeout_p);
    struct data_32_internal *data_guest = (struct data_32_internal *) g_2_h(data_p);
    struct timespec timeout;
    struct data_32_internal data;


    if (timeout_p) {
        timeout.tv_sec = timeout_guest->tv_sec;
        timeout.tv_nsec = timeout_guest->tv_nsec;
    };

    /* FIXME: what if data_p NULL ? */
    data.ss = (uint32_t) (data_guest->ss?g_2_h(data_guest->ss):NULL);
    data.ss_len = data_guest->ss_len;
    res = syscall(SYS_pselect6, nfds, readfds_p?readfds:NULL, writefds_p?writefds:NULL,
                  exceptfds_p?exceptfds:NULL, timeout_p?&timeout:NULL, &data);

    if (timeout_p) {
        timeout_guest->tv_sec = timeout.tv_sec;
        timeout_guest->tv_nsec = timeout.tv_nsec;
    };

    return res;
}
