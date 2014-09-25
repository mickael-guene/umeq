#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <unistd.h>
#include <sys/syscall.h>   /* For SYS_xxx definitions */
#include <sys/time.h>
#include <sys/resource.h>
#include <errno.h>

#include "syscall32_64_types.h"
#include "syscall32_64_private.h"

int getrusage_s3264(uint32_t who_p, uint32_t usage_p)
{
    int res;
    int who = (int) who_p;
    struct rusage_32 *usage_guest = (struct rusage_32 *) g_2_h(usage_p);
    struct rusage rusage;

    if (usage_p == 0 || usage_p == 0xffffffff)
        res = -EFAULT;
    else {
        res = syscall(SYS_getrusage, who, &rusage);

        usage_guest->ru_utime.tv_sec = rusage.ru_utime.tv_sec;
        usage_guest->ru_utime.tv_usec = rusage.ru_utime.tv_usec;
        usage_guest->ru_stime.tv_sec = rusage.ru_stime.tv_sec;
        usage_guest->ru_stime.tv_usec = rusage.ru_stime.tv_usec;
        usage_guest->ru_maxrss = rusage.ru_maxrss;
        usage_guest->ru_ixrss = rusage.ru_ixrss;
        usage_guest->ru_idrss = rusage.ru_idrss;
        usage_guest->ru_isrss = rusage.ru_isrss;
        usage_guest->ru_minflt = rusage.ru_minflt;
        usage_guest->ru_majflt = rusage.ru_majflt;
        usage_guest->ru_nswap = rusage.ru_nswap;
        usage_guest->ru_inblock = rusage.ru_inblock;
        usage_guest->ru_oublock = rusage.ru_oublock;
        usage_guest->ru_msgsnd = rusage.ru_msgsnd;
        usage_guest->ru_msgrcv = rusage.ru_msgrcv;
        usage_guest->ru_nsignals = rusage.ru_nsignals;
        usage_guest->ru_nvcsw = rusage.ru_nvcsw;
        usage_guest->ru_nivcsw = rusage.ru_nivcsw;
    }

    return res;
}
