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
