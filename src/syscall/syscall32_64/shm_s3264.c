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

#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/mman.h>
#include <errno.h>

#include "syscall32_64_types.h"
#include "syscall32_64_private.h"
#include "runtime.h"

#define IPC_64      (0x100)

int shmat_s3264(uint32_t shmid_p, uint32_t shmaddr_p, uint32_t shmflg_p)
{
    int res;
    void *res2;
    int shmid = (int) shmid_p;
    void *shmaddr = (void *) g_2_h(shmaddr_p);
    int shmflg = (int) shmflg_p;

    if (shmaddr_p == 0) {
        struct shmid_ds shm_info;
        guest_ptr mmap_res;

        res = shmctl(shmid, IPC_STAT, &shm_info);
        if (res < 0)
            return res;
        mmap_res = mmap_guest(0, shm_info.shm_segsz, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mmap_res == -1)
            return -ENOMEM;
        shmaddr = g_2_h(mmap_res);
    }

    res2 = (void *) syscall(SYS_shmat, shmid, shmaddr, shmflg|SHM_REMAP);

    return res2==(void *)-1?-1:h_2_g(res2);
}

int shmctl_s3264(uint32_t shmid_p, uint32_t cmd_p, uint32_t buf_p)
{
    int res;
    int shmid = (int) shmid_p;
    int cmd = (int) cmd_p;
    struct shmid_ds_32 *buf_guest = (struct shmid_ds_32 *) g_2_h(buf_p);
    struct shmid_ds buf;

    cmd &= ~IPC_64;
    switch(cmd) {
        case IPC_RMID:
            res = shmctl(shmid, cmd, NULL);
            break;
        case IPC_SET:
            {
                buf.shm_perm.__key = buf_guest->shm_perm.__key;
                buf.shm_perm.uid = buf_guest->shm_perm.uid;
                buf.shm_perm.gid = buf_guest->shm_perm.gid;
                buf.shm_perm.cuid = buf_guest->shm_perm.cuid;
                buf.shm_perm.cgid = buf_guest->shm_perm.cgid;
                buf.shm_perm.mode = buf_guest->shm_perm.mode;
                buf.shm_perm.__seq = buf_guest->shm_perm.__seq;
                buf.shm_segsz = buf_guest->shm_segsz;
                buf.shm_atime = buf_guest->shm_atime;
                buf.shm_dtime = buf_guest->shm_dtime;
                buf.shm_ctime = buf_guest->shm_ctime;
                buf.shm_cpid = buf_guest->shm_cpid;
                buf.shm_lpid = buf_guest->shm_lpid;

                res = shmctl(shmid, cmd, &buf);

                buf_guest->shm_perm.__key = buf.shm_perm.__key;
                buf_guest->shm_perm.uid = buf.shm_perm.uid;
                buf_guest->shm_perm.gid = buf.shm_perm.gid;
                buf_guest->shm_perm.cuid = buf.shm_perm.cuid;
                buf_guest->shm_perm.cgid = buf.shm_perm.cgid;
                buf_guest->shm_perm.mode = buf.shm_perm.mode;
                buf_guest->shm_perm.__seq = buf.shm_perm.__seq;
                buf_guest->shm_segsz = buf.shm_segsz;
                buf_guest->shm_atime = buf.shm_atime;
                buf_guest->shm_dtime = buf.shm_dtime;
                buf_guest->shm_ctime = buf.shm_ctime;
                buf_guest->shm_cpid = buf.shm_cpid;
                buf_guest->shm_lpid = buf.shm_lpid;
            }
            break;
        case IPC_STAT:
            {
                res = shmctl(shmid, cmd, &buf);

                buf_guest->shm_perm.__key = buf.shm_perm.__key;
                buf_guest->shm_perm.uid = buf.shm_perm.uid;
                buf_guest->shm_perm.gid = buf.shm_perm.gid;
                buf_guest->shm_perm.cuid = buf.shm_perm.cuid;
                buf_guest->shm_perm.cgid = buf.shm_perm.cgid;
                buf_guest->shm_perm.mode = buf.shm_perm.mode;
                buf_guest->shm_perm.__seq = buf.shm_perm.__seq;
                buf_guest->shm_segsz = buf.shm_segsz;
                buf_guest->shm_atime = buf.shm_atime;
                buf_guest->shm_dtime = buf.shm_dtime;
                buf_guest->shm_ctime = buf.shm_ctime;
                buf_guest->shm_cpid = buf.shm_cpid;
                buf_guest->shm_lpid = buf.shm_lpid;
                buf_guest->shm_nattch = buf.shm_nattch;
            }
            break;
        default:
            fatal("shmctl_s3264: unsupported command %d\n", cmd);
    }

    return res;
}

int semctl_s3264(uint32_t semid_p, uint32_t semnum_p, uint32_t cmd_p, uint32_t arg0_p, uint32_t arg1_p, uint32_t arg2_p)
{
    int res;
    int semid = (int) semid_p;
    int semnum = (int) semnum_p;
    int cmd = (int) cmd_p;

    cmd &= ~IPC_64;
    switch(cmd) {
        case GETVAL:
        case GETNCNT:
        case IPC_RMID:
        case GETPID:
        case GETZCNT:
            res = syscall(SYS_semctl, semid, semnum, cmd);
            break;
        case IPC_SET:
            {
                struct semid_ds buf;
                struct semid_ds_32 *arg_guest = (struct semid_ds_32 *) g_2_h(arg0_p);

                if (arg0_p == 0 || arg0_p == 0xffffffff)
                    res = -EFAULT;
                else {
                    buf.sem_perm.__key = arg_guest->sem_perm.__key;
                    buf.sem_perm.uid = arg_guest->sem_perm.uid;
                    buf.sem_perm.gid = arg_guest->sem_perm.gid;
                    buf.sem_perm.cuid = arg_guest->sem_perm.cuid;
                    buf.sem_perm.cgid = arg_guest->sem_perm.cgid;
                    buf.sem_perm.mode = arg_guest->sem_perm.mode;
                    buf.sem_perm.__seq = arg_guest->sem_perm.__seq;
                    buf.sem_otime = arg_guest->sem_otime;
                    buf.sem_ctime = arg_guest->sem_ctime;
                    buf.sem_nsems = arg_guest->sem_nsems;
                    res = syscall(SYS_semctl, semid, semnum, cmd, &buf);
                    arg_guest->sem_perm.__key = buf.sem_perm.__key;
                    arg_guest->sem_perm.uid = buf.sem_perm.uid;
                    arg_guest->sem_perm.gid = buf.sem_perm.gid;
                    arg_guest->sem_perm.cuid = buf.sem_perm.cuid;
                    arg_guest->sem_perm.cgid = buf.sem_perm.cgid;
                    arg_guest->sem_perm.mode = buf.sem_perm.mode;
                    arg_guest->sem_perm.__seq = buf.sem_perm.__seq;
                    arg_guest->sem_otime = buf.sem_otime;
                    arg_guest->sem_ctime = buf.sem_ctime;
                    arg_guest->sem_nsems = buf.sem_nsems;
                }
            }
            break;
        case SEM_STAT:
        case IPC_STAT:
            {
                struct semid_ds buf;
                struct semid_ds_32 *arg_guest = (struct semid_ds_32 *) g_2_h(arg0_p);

                if (arg0_p == 0 || arg0_p == 0xffffffff)
                    res = -EFAULT;
                else {
                    res = syscall(SYS_semctl, semid, semnum, cmd, &buf);
                    arg_guest->sem_perm.__key = buf.sem_perm.__key;
                    arg_guest->sem_perm.uid = buf.sem_perm.uid;
                    arg_guest->sem_perm.gid = buf.sem_perm.gid;
                    arg_guest->sem_perm.cuid = buf.sem_perm.cuid;
                    arg_guest->sem_perm.cgid = buf.sem_perm.cgid;
                    arg_guest->sem_perm.mode = buf.sem_perm.mode;
                    arg_guest->sem_perm.__seq = buf.sem_perm.__seq;
                    arg_guest->sem_otime = buf.sem_otime;
                    arg_guest->sem_ctime = buf.sem_ctime;
                    arg_guest->sem_nsems = buf.sem_nsems;
                }
            }
            break;
        case SEM_INFO:
        case IPC_INFO:
            {
                 struct seminfo* seminfo = (struct seminfo *) g_2_h(arg0_p);

                 res = syscall(SYS_semctl, semid, semnum, cmd, seminfo);
            }
            break;
        case GETALL:
            {
                unsigned short *array = (unsigned short *) g_2_h(arg0_p);

                res = syscall(SYS_semctl, semid, semnum, cmd, array);
            }
            break;
        case SETVAL:
            {
                int val = (int) arg0_p;

                res = syscall(SYS_semctl, semid, semnum, cmd, val);
            }
            break;
        case SETALL:
            {
                unsigned short *array = (unsigned short *) g_2_h(arg0_p);

                res = syscall(SYS_semctl, semid, semnum, cmd, array);
            }
            break;
        default:
            warning("semctl_s3264: unsupported command %d\n", cmd);
            res = -EINVAL;
    }

    return res; 
}

int semop_s3264(uint32_t semid_p, uint32_t sops_p, uint32_t nsops_p)
{
    int res;
    int semid = (int) semid_p;
    struct sembuf_32 *sops_guest = (struct sembuf_32 *) g_2_h(sops_p);
    size_t nsops = (size_t) nsops_p;
    struct sembuf sops[16];
    int i;

    assert(nsops <= 16);
    for(i=0; i < nsops; i++) {
        sops[i].sem_num = sops_guest[i].sem_num;
        sops[i].sem_op = sops_guest[i].sem_op;
        sops[i].sem_flg = sops_guest[i].sem_flg;
    }
    res = syscall(SYS_semop, semid, sops, nsops);

    return res;
}
