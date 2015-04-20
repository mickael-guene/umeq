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
#include <sys/types.h>
#include <signal.h>
#include <poll.h>

#include "syscall32_64_types.h"
#include "syscall32_64_private.h"
#include "runtime.h"

int ppoll_s3264(uint32_t fds_p, uint32_t nfds_p, uint32_t timeout_ts_p, uint32_t sigmask_p, uint32_t sigsetsize_p)
{
    int res;
    struct pollfd *fds = (struct pollfd *) g_2_h(fds_p);
    nfds_t nfds = (nfds_t) nfds_p;
    struct timespec_32 *timeout_ts_guest = (struct timespec_32 *) g_2_h(timeout_ts_p);
    sigset_t *sigmask = (sigset_t *) g_2_h(sigmask_p);
    size_t sigsetsize = (size_t) sigsetsize_p;
    struct timespec timeout_ts;

    if (timeout_ts_p) {
        timeout_ts.tv_sec = timeout_ts_guest->tv_sec;
        timeout_ts.tv_nsec = timeout_ts_guest->tv_nsec;
    }
    res = syscall(SYS_ppoll, fds, nfds, timeout_ts_p?&timeout_ts:NULL, sigmask_p?sigmask:NULL, sigsetsize);


    return res;
}
