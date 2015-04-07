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
#include "runtime.h"

int mq_open_s3264(uint32_t name_p, uint32_t oflag_p, uint32_t mode_p, uint32_t attr_p)
{
    int res;
    char *name = (char *) g_2_h(name_p);
    int oflag = (int) oflag_p;
    mode_t mode = (mode_t) mode_p;
    struct mq_attr_32 *attr_guest = (struct mq_attr_32 *) g_2_h(attr_p);
    struct mq_attr attr;

    if (attr_p) {
        attr.mq_flags = attr_guest->mq_flags;
        attr.mq_maxmsg = attr_guest->mq_maxmsg;
        attr.mq_msgsize = attr_guest->mq_msgsize;
        attr.mq_curmsgs = attr_guest->mq_curmsgs;
    }    
    res = syscall(SYS_mq_open, name, oflag, mode, attr_p?&attr:NULL);

    return res;
}

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

int mq_timedreceive_s3264(uint32_t mqdes_p, uint32_t msg_ptr_p, uint32_t msg_len_p, uint32_t msg_prio_p, uint32_t abs_timeout_p)
{
    int res;
    mqd_t mqdes = (mqd_t) mqdes_p;
    char *msg_ptr = (char *) g_2_h(msg_ptr_p);
    size_t msg_len = (size_t) msg_len_p;
    unsigned int *msg_prio = (unsigned int *) g_2_h(msg_prio_p);
    struct timespec_32 *abs_timeout_guest = (struct timespec_32 *) g_2_h(abs_timeout_p);
    struct timespec abs_timeout;

    if (abs_timeout_p) {
        abs_timeout.tv_sec = abs_timeout_guest->tv_sec;
        abs_timeout.tv_nsec = abs_timeout_guest->tv_nsec;
    }
    res = syscall(SYS_mq_timedreceive, mqdes, msg_ptr, msg_len, msg_prio_p?msg_prio:NULL, abs_timeout_p?&abs_timeout:NULL);

    return res;
}

int mq_notify_s3264(uint32_t mqdes_p, uint32_t sevp_p)
{
    int res;
    mqd_t mqdes = (mqd_t) mqdes_p;
    struct sigevent_32 *sevp_guest = (struct sigevent_32 *) g_2_h(sevp_p);
    struct sigevent evp;

    if (sevp_p) {
        switch(sevp_guest->sigev_notify) {
            case SIGEV_NONE:
                evp.sigev_notify = SIGEV_NONE;
                break;
            case SIGEV_SIGNAL:
                evp.sigev_notify = SIGEV_SIGNAL;
                evp.sigev_signo = sevp_guest->sigev_signo;
                /* FIXME: need to check kernel since doc is not clear which union part is use */
                //evp.sigev_value.sival_ptr = (void *) g_2_h(sevp_guest->sigev_value.sival_ptr);
                evp.sigev_value.sival_int = sevp_guest->sigev_value.sival_int;
                break;
            default:
                /* FIXME : add SIGEV_THREAD + SIGEV_THREAD_ID support */
                assert(0);
        }
    }

    res = syscall(SYS_mq_notify, mqdes, sevp_p?&evp:NULL);

    return res;
}

int mq_getsetattr_s3264(uint32_t mqdes_p, uint32_t newattr_p, uint32_t oldattr_p)
{
    int res;
    mqd_t mqdes = (mqd_t) mqdes_p;
    struct mq_attr_32 *newattr_guest = (struct mq_attr_32 *) g_2_h(newattr_p);
    struct mq_attr_32 *oldattr_guest = (struct mq_attr_32 *) g_2_h(oldattr_p);
    struct mq_attr newattr;
    struct mq_attr oldattr;

    if (newattr_p) {
        newattr.mq_flags = newattr_guest->mq_flags;
        newattr.mq_maxmsg = newattr_guest->mq_maxmsg;
        newattr.mq_msgsize = newattr_guest->mq_msgsize;
        newattr.mq_curmsgs = newattr_guest->mq_curmsgs;
    }
    res = syscall(SYS_mq_getsetattr, mqdes, newattr_p?&newattr:NULL, oldattr_p?&oldattr:NULL);
    if (oldattr_p) {
        oldattr_guest->mq_flags = oldattr.mq_flags;
        oldattr_guest->mq_maxmsg = oldattr.mq_maxmsg;
        oldattr_guest->mq_msgsize = oldattr.mq_msgsize;
        oldattr_guest->mq_curmsgs = oldattr.mq_curmsgs;
    }

    return res;
}
