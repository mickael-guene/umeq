#include <stddef.h>
#include <linux/futex.h>
#include <sys/time.h>

#include "syscall32_64_types.h"
#include "syscall32_64_private.h"

int futex_s3264(uint32_t uaddr_p, uint32_t op_p, uint32_t val_p, uint32_t timeout_p, uint32_t uaddr2_p, uint32_t val3_p)
{
    int res;
    int *uaddr = (int *) g_2_h(uaddr_p);
    int op = (int) op_p;
    int val = (int) val_p;
    const struct timespec_32 *timeout_guest = (struct timespec_32 *) g_2_h(timeout_p);
    int *uaddr2 = (int *) g_2_h(uaddr2_p);
    int val3 = (int) val3_p;
    struct timespec timeout;

    if ((op & FUTEX_WAIT) && timeout_p) {
        timeout.tv_sec = timeout_guest->tv_sec;
        timeout.tv_nsec = timeout_guest->tv_nsec;
    }

    res = syscall(SYS_futex, uaddr, op, val, timeout_p?&timeout:NULL, uaddr2, val3);

    return res;
}
