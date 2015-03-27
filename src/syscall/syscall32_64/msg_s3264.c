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
#include <sys/ipc.h>
#include <sys/msg.h>
#include <assert.h>
#include <string.h>

#include "syscall32_64_types.h"
#include "syscall32_64_private.h"

struct msgbuf_64 {
    uint64_t mtype;
    char mtext[1];
};

int msgsnd_s3264(uint32_t msqid_t, uint32_t msgp_t, uint32_t msgsz_t, uint32_t msgflg_t)
{
    int res;
    int msqid = (int) msqid_t;
    struct msgbuf_32 *msgp_guest = (struct msgbuf_32 *) g_2_h(msgp_t);
    size_t msgsz = (size_t) msgsz_t;
    int msgflg = (int) msgflg_t;
    struct msgbuf_64 *msgp;

    msgp = (struct msgbuf_64 *) alloca(sizeof(uint64_t) + msgsz);
    assert(msgp != NULL);
    msgp->mtype = msgp_guest->mtype;
    if (msgsz)
        memcpy(msgp->mtext, msgp_guest->mtext, msgsz);
    res = syscall(SYS_msgsnd, msqid, msgp, msgsz, msgflg);

    return res;
}

int msgrcv_s3264(uint32_t msqid_t, uint32_t msgp_t, uint32_t msgsz_t, uint32_t msgtyp_t, uint32_t msgflg_t)
{
    int res;
    int msqid = (int) msqid_t;
    struct msgbuf_32 *msgp_guest = (struct msgbuf_32 *) g_2_h(msgp_t);
    size_t msgsz = (size_t) msgsz_t;
    long msgtyp = (int) msgtyp_t;
    int msgflg = (int) msgflg_t;
    struct msgbuf_64 *msgp;

    msgp = (struct msgbuf_64 *) alloca(sizeof(uint64_t) + msgsz);
    assert(msgp != NULL);
    msgp->mtype = msgp_guest->mtype;
    res = syscall(SYS_msgrcv, msqid, msgp, msgsz, msgtyp, msgflg);
    if (res >= 0) {
        msgp_guest->mtype = msgp->mtype;
        memcpy(msgp_guest->mtext, msgp->mtext, res);
    }

    return res;
}
