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
#include <sys/times.h>

#include "syscall32_64_types.h"
#include "syscall32_64_private.h"

int times_s3264(uint32_t buf_p)
{
    int res;
    struct tms_32 *buf_guest = (struct tms_32 *) g_2_h(buf_p);
    struct tms buf;

    res = syscall(SYS_times, &buf);
    if (buf_p) {
        buf_guest->tms_utime = buf.tms_utime;
        buf_guest->tms_stime = buf.tms_stime;
        buf_guest->tms_cutime = buf.tms_cutime;
        buf_guest->tms_cstime = buf.tms_cstime;
    }

    return res;
}
