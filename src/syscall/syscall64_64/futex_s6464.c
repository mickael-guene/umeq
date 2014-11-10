#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <unistd.h>
#include <sys/syscall.h>   /* For SYS_xxx definitions */
#include <stddef.h>
#include <linux/futex.h>
#include <sys/time.h>

#include "syscall64_64_private.h"

/* WARNING : timeout is not always a timeout structure but is sometimes an additionnal argument */
long futex_s6464(uint64_t uaddr_p, uint64_t op_p, uint64_t val_p, uint64_t timeout_p, uint64_t uaddr2_p, uint64_t val3_p)
{
    long res;
    int *uaddr = (int *) g_2_h(uaddr_p);
    int op = (int) op_p;
    int val = (int) val_p;
    int *uaddr2 = (int *) g_2_h(uaddr2_p);
    int val3 = (int) val3_p;
    int cmd = op & FUTEX_CMD_MASK;
    /* default value is a timout parameter */
    uint64_t syscall_timeout = (uint64_t) (timeout_p?g_2_h(timeout_p):NULL);

    /* fixup syscall_timeout in case it's not really a timeout structure */
    if (cmd == FUTEX_REQUEUE || cmd == FUTEX_CMP_REQUEUE ||
        cmd == FUTEX_CMP_REQUEUE_PI || cmd == FUTEX_WAKE_OP) {
        syscall_timeout = timeout_p;
    } else if (cmd == FUTEX_WAKE) {
        /* in this case timeout argument is not use and can take any value */
        syscall_timeout = timeout_p;
    }

    res = syscall(SYS_futex, uaddr, op, val, syscall_timeout, uaddr2, val3);

    return res;
}
