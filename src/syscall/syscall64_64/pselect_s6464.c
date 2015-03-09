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

#include "runtime.h"
#include "syscall64_64_private.h"

struct data_64_internal {
    const sigset_t *ss;
    size_t ss_len;
};

long pselect6_s6464(uint64_t nfds_p, uint64_t readfds_p, uint64_t writefds_p,
                 uint64_t exceptfds_p, uint64_t timeout_p, uint64_t data_p)
{
    long res;
    int nfds = (int) nfds_p;
    fd_set *readfds = (fd_set *) g_2_h(readfds_p);
    fd_set *writefds = (fd_set *) g_2_h(writefds_p);
    fd_set *exceptfds = (fd_set *) g_2_h(exceptfds_p);
    struct timespec *timeout = (struct timespec *) g_2_h(timeout_p);
    struct data_64_internal *data_guest = (struct data_64_internal *) g_2_h(data_p);
    struct data_64_internal data;

    if (data_p) {
        data.ss = (const sigset_t *) data_guest->ss?g_2_h((uint64_t)data_guest->ss):NULL;
        data.ss_len = data_guest->ss_len;
    }

    res = syscall(SYS_pselect6, nfds, readfds_p?readfds:NULL, writefds_p?writefds:NULL,
                  exceptfds_p?exceptfds:NULL, timeout_p?timeout:NULL, data_p?&data:NULL);

    return res;
}

