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
#include <sys/sem.h>
#include <errno.h>

#include "syscall64_64_private.h"

/*
struct semid_ds for x86 has some padding that others 64 bits don't have. So
we need to translate.
*/

struct semid_ds_64 {
    struct ipc_perm sem_perm;
    time_t sem_otime;
    time_t sem_ctime;
    unsigned long sem_nsems;
};

long semctl_s6464(uint64_t semid_p, uint64_t semnum_p, uint64_t cmd_p, uint64_t arg_p)
{
    long res = 0;
    int semid = (int) semid_p;
    int semnum = (int) semnum_p;
    int cmd = (int) cmd_p;

    switch(cmd) {
        case IPC_STAT:
        case IPC_SET:
            {
                struct semid_ds buf;
                struct semid_ds_64 *buf_guest = (struct semid_ds_64 *) g_2_h(arg_p);

                /* try to improve robustness */
                if (arg_p == 0 || arg_p == -1 || arg_p == 0xffffffff)
                    res = -EFAULT;
                else {
                    buf.sem_perm = buf_guest->sem_perm;
                    buf.sem_otime = buf_guest->sem_otime;
                    buf.sem_ctime = buf_guest->sem_ctime;
                    buf.sem_nsems = buf_guest->sem_nsems;
                    res = syscall(SYS_semctl, semid, semnum, cmd, &buf);
                    buf_guest->sem_perm = buf.sem_perm;
                    buf_guest->sem_otime = buf.sem_otime;
                    buf_guest->sem_ctime = buf.sem_ctime;
                    buf_guest->sem_nsems = buf.sem_nsems;
                }
            }
            break;
        default:
            res = syscall(SYS_semctl, semid, semnum, cmd, g_2_h(arg_p));
    }

    return res;
}
