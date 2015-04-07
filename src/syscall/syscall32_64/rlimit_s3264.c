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
#include <sys/time.h>
#include <sys/resource.h>
#include <errno.h>
#include <assert.h>

#include "syscall32_64_types.h"
#include "syscall32_64_private.h"

int ugetrlimit_s3264(uint32_t resource_p, uint32_t rlim_p)
{
    int res;
    int ressource = (int) resource_p;
    struct rlimit_32 *rlim_guest = (struct rlimit_32 *) g_2_h(rlim_p);
    struct rlimit rlim;

    if (rlim_p == 0 || rlim_p == 0xffffffff)
        res = -EFAULT;
    else {
        res = syscall(SYS_getrlimit, ressource, &rlim);
        rlim_guest->rlim_cur = rlim.rlim_cur;
        rlim_guest->rlim_max = rlim.rlim_max;
    }

    return res;
}

int setrlimit_s3264(uint32_t resource_p, uint32_t rlim_p)
{
    int res;
    int ressource = (int) resource_p;
    struct rlimit_32 *rlim_guest = (struct rlimit_32 *) g_2_h(rlim_p);
    struct rlimit rlim;

    rlim.rlim_cur = rlim_guest->rlim_cur;
    rlim.rlim_max = rlim_guest->rlim_max;
    res = syscall(SYS_setrlimit, ressource, &rlim);

    return res;
}

int prlimit64_s3264(uint32_t pid_p, uint32_t resource_p, uint32_t new_limit_p, uint32_t old_limit_p)
{
    int res = 0;
    pid_t pid = (pid_t) pid_p;
    int resource = (int) resource_p;
    struct rlimit64_32 *new_limit_guest = (struct rlimit64_32 *) g_2_h(new_limit_p);
    struct rlimit64_32 *old_limit_guest = (struct rlimit64_32 *) g_2_h(old_limit_p);
    struct rlimit new_limit;
    struct rlimit old_limit;

    assert(pid == 0);
    if (old_limit_p) {
        res = syscall(SYS_getrlimit, resource, &old_limit);
        old_limit_guest->rlim_cur = old_limit.rlim_cur;
        old_limit_guest->rlim_max = old_limit.rlim_max;
    }
    if (res == 0 && new_limit_p) {
        new_limit.rlim_cur = new_limit_guest->rlim_cur;
        new_limit.rlim_max = new_limit_guest->rlim_max;

        res = syscall(SYS_setrlimit, resource, &new_limit);
    }

    return res;
}
