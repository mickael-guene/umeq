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
#include <sched.h>

#include "syscall32_64_types.h"
#include "syscall32_64_private.h"

int sched_rr_get_interval_s3264(uint32_t pid_p, uint32_t tp_p)
{
    int result;
    pid_t pid = (pid_t) pid_p;
    struct timespec_32 *tp_guest = (struct timespec_32 *) g_2_h(tp_p);
    struct timespec tp;

    result = syscall(SYS_sched_rr_get_interval, pid, &tp);
    if (tp_p) {
        tp_guest->tv_sec = tp.tv_sec;
        tp_guest->tv_nsec = tp.tv_nsec;
    }

    return result;
}
