#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <unistd.h>
#include <sys/syscall.h>   /* For SYS_xxx definitions */
#include <sys/time.h>
#include <errno.h>

#include "syscall32_64_types.h"
#include "syscall32_64_private.h"

int setitimer_s3264(uint32_t which_p, uint32_t new_value_p, uint32_t old_value_p)
{
    int res;
    int which = (int) which_p;
    struct itimerval_32 *new_value_guest = (struct itimerval_32 *) g_2_h(new_value_p);
    struct itimerval_32 *old_value_guest = (struct itimerval_32 *) g_2_h(old_value_p);
    struct itimerval new_value;
    struct itimerval old_value;

    if (new_value_p == 0xffffffff || old_value_p == 0xffffffff)
        res = -EFAULT;
    else {
        new_value.it_interval.tv_sec = new_value_guest->it_interval.tv_sec;
        new_value.it_interval.tv_usec = new_value_guest->it_interval.tv_usec;
        new_value.it_value.tv_sec = new_value_guest->it_value.tv_sec;
        new_value.it_value.tv_usec = new_value_guest->it_value.tv_usec;

        res = syscall(SYS_setitimer, which, &new_value, old_value_p?&old_value:NULL);

        if (old_value_p) {
            old_value_guest->it_interval.tv_sec = old_value.it_interval.tv_sec;
            old_value_guest->it_interval.tv_usec = old_value.it_interval.tv_usec;
            old_value_guest->it_value.tv_sec = old_value.it_value.tv_sec;
            old_value_guest->it_value.tv_usec = old_value.it_value.tv_usec;
        }
    }

    return res;
}
