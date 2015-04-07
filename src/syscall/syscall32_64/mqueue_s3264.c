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

#include "syscall32_64_types.h"
#include "syscall32_64_private.h"

int mq_timedsend_s3264(uint32_t mqdes_p, uint32_t msg_ptr_p, uint32_t msg_len_p, uint32_t msg_prio_p, uint32_t abs_timeout_p)
{
    int res;
    mqd_t mqdes = (mqd_t) mqdes_p;
    char *msg_ptr = (char *) g_2_h(msg_ptr_p);
    size_t msg_len = (size_t) msg_len_p;
    unsigned int msg_prio = (unsigned int) msg_prio_p;
    struct timespec_32 *abs_timeout_guest = (struct timespec_32 *) g_2_h(abs_timeout_p);
    struct timespec abs_timeout;

    if (abs_timeout_p) {
        abs_timeout.tv_sec = abs_timeout_guest->tv_sec;
        abs_timeout.tv_nsec = abs_timeout_guest->tv_nsec;
    }
    res = syscall(SYS_mq_timedsend, mqdes, msg_ptr, msg_len, msg_prio, abs_timeout_p?&abs_timeout:NULL);

    return res;
}
