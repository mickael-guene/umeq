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
#include <stddef.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>

#include "syscall32_64_types.h"
#include "syscall32_64_private.h"

int wait4_s3264(uint32_t pid_p,uint32_t status_p,uint32_t options_p,uint32_t rusage_p)
{
    int res;
    pid_t pid = (pid_t) pid_p;
    int *status = (int *) g_2_h(status_p);
    int options = (int) options_p;
    struct rusage_32 *rusage_guest = (struct rusage_32 *) g_2_h(rusage_p);
    struct rusage rusage;

    res = syscall(SYS_wait4, pid, status_p?status:NULL, options, rusage_p?&rusage:NULL);
    if (rusage_p) {
        rusage_guest->ru_utime.tv_sec = rusage.ru_utime.tv_sec;
        rusage_guest->ru_utime.tv_usec = rusage.ru_utime.tv_usec;
        rusage_guest->ru_stime.tv_sec = rusage.ru_stime.tv_sec;
        rusage_guest->ru_stime.tv_usec = rusage.ru_stime.tv_usec;
        rusage_guest->ru_maxrss = rusage.ru_maxrss;
        rusage_guest->ru_ixrss = rusage.ru_ixrss;
        rusage_guest->ru_idrss = rusage.ru_idrss;
        rusage_guest->ru_isrss = rusage.ru_isrss;
        rusage_guest->ru_minflt = rusage.ru_minflt;
        rusage_guest->ru_majflt = rusage.ru_majflt;
        rusage_guest->ru_nswap = rusage.ru_nswap;
        rusage_guest->ru_inblock = rusage.ru_inblock;
        rusage_guest->ru_oublock = rusage.ru_oublock;
        rusage_guest->ru_msgsnd = rusage.ru_msgsnd;
        rusage_guest->ru_msgrcv = rusage.ru_msgrcv;
        rusage_guest->ru_nsignals = rusage.ru_nsignals;
        rusage_guest->ru_nvcsw = rusage.ru_nvcsw;
        rusage_guest->ru_nivcsw = rusage.ru_nivcsw;
    }

    return res;
}
