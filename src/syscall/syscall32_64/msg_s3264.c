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
#include "runtime.h"

#define IPC_64      (0x100)

struct msgbuf_64 {
    uint64_t mtype;
    char mtext[1];
};

int msgsnd_s3264(uint32_t msqid_p, uint32_t msgp_p, uint32_t msgsz_p, uint32_t msgflg_p)
{
    int res;
    int msqid = (int) msqid_p;
    struct msgbuf_32 *msgp_guest = (struct msgbuf_32 *) g_2_h(msgp_p);
    size_t msgsz = (size_t) msgsz_p;
    int msgflg = (int) msgflg_p;
    struct msgbuf_64 *msgp;

    msgp = (struct msgbuf_64 *) alloca(sizeof(uint64_t) + msgsz);
    assert(msgp != NULL);
    msgp->mtype = msgp_guest->mtype;
    if (msgsz)
        memcpy(msgp->mtext, msgp_guest->mtext, msgsz);
    res = syscall(SYS_msgsnd, msqid, msgp, msgsz, msgflg);

    return res;
}

int msgrcv_s3264(uint32_t msqid_p, uint32_t msgp_p, uint32_t msgsz_p, uint32_t msgtyp_p, uint32_t msgflg_p)
{
    int res;
    int msqid = (int) msqid_p;
    struct msgbuf_32 *msgp_guest = (struct msgbuf_32 *) g_2_h(msgp_p);
    size_t msgsz = (size_t) msgsz_p;
    long msgtyp = (int) msgtyp_p;
    int msgflg = (int) msgflg_p;
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

int msgctl_s3264(uint32_t msqid_p, uint32_t cmd_p, uint32_t buf_p)
{
    int res;
    int msqid = (int) msqid_p;
    int cmd = (int) cmd_p;
    struct msqid_ds_32 *buf_guest = (struct msqid_ds_32 *) g_2_h(buf_p);
    struct msqid_ds buf;

    cmd &= ~IPC_64;
    switch(cmd) {
        case IPC_RMID:
            res = syscall(SYS_msgctl, msqid, cmd, NULL/*not use*/);
            break;
        case IPC_STAT:
            res = syscall(SYS_msgctl, msqid, cmd , &buf);
            buf_guest->msg_perm.__key = buf.msg_perm.__key;
            buf_guest->msg_perm.uid = buf.msg_perm.uid;
            buf_guest->msg_perm.gid = buf.msg_perm.gid;
            buf_guest->msg_perm.cuid = buf.msg_perm.cuid;
            buf_guest->msg_perm.cgid = buf.msg_perm.cgid;
            buf_guest->msg_perm.mode = buf.msg_perm.mode;
            buf_guest->msg_perm.__seq = buf.msg_perm.__seq;
            buf_guest->msg_stime = buf.msg_stime;
            buf_guest->msg_rtime = buf.msg_rtime;
            buf_guest->msg_ctime = buf.msg_ctime;
            buf_guest->__msg_cbytes = buf.__msg_cbytes;
            buf_guest->msg_qnum = buf.msg_qnum;
            buf_guest->msg_qbytes = buf.msg_qbytes;
            buf_guest->msg_lspid = buf.msg_lspid;
            buf_guest->msg_lrpid = buf.msg_lrpid;
            break;
        default:
            fatal("msgctl_s3264: unsupported command %d\n", cmd);
    }

    return res;
}
