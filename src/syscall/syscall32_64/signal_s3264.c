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
#include <signal.h>

#include "syscall32_64_types.h"
#include "syscall32_64_private.h"

int rt_sigtimedwait_s3264(uint32_t set_p, uint32_t info_p, uint32_t timeout_p)
{
	int res;
	sigset_t *set = (sigset_t *) g_2_h(set_p);
	siginfo_t_32 *info_guest = (siginfo_t_32 *) g_2_h(info_p);
	struct timespec_32 *timeout_guest = (struct timespec_32 *) g_2_h(timeout_p);
    siginfo_t info;
    struct timespec timeout;

	if (timeout_p) {
        timeout.tv_sec = timeout_guest->tv_sec;
        timeout.tv_nsec = timeout_guest->tv_nsec;
	}
	res = syscall(SYS_rt_sigtimedwait, set, info_p?&info:NULL, timeout_p?&timeout:NULL, _NSIG / 8);
    if (info_p) {
        info_guest->si_signo = info.si_signo;
        info_guest->si_code = info.si_code;
        info_guest->_sifields._sigchld._si_pid = info.si_pid;
        info_guest->_sifields._sigchld._si_uid = info.si_uid;
        info_guest->_sifields._sigchld._si_status = info.si_status;
    }

    return res;
}
