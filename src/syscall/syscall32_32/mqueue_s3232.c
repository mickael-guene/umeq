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
#include <time.h>
#include <mqueue.h>

#include "syscall32_32_types.h"
#include "syscall32_32_private.h"
#include "runtime.h"

int mq_notify_s3232(uint32_t mqdes_p, uint32_t sevp_p)
{
    int res;
    mqd_t mqdes = (mqd_t) mqdes_p;
    struct sigevent *sevp_guest = (struct sigevent *) g_2_h(sevp_p);
    struct sigevent evp;

    if (sevp_p) {
        evp = *sevp_guest;
        if (evp.sigev_notify == SIGEV_THREAD)
            evp.sigev_value.sival_ptr = g_2_h(evp.sigev_value.sival_ptr);
    }
    res = syscall(SYS_mq_notify, mqdes, sevp_p?&evp:NULL);

    return res;
}
