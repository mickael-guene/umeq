#include <stddef.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>

#include "syscall32_64_types.h"
#include "syscall32_64_private.h"

int wait4_s3264(uint32_t pid_p,uint32_t status_p,uint32_t options_p,uint32_t rusage_p)
{
    int res;
    pid_t pid = (pid_t) pid_p;
    int *status = (int *) g_2_h(status_p);
    int options = (int) options_p;
    struct rusage_32 *rusage_guest = (struct rusage_32 *) g_2_h(rusage_p);
    struct rusage rusage;

    res = syscall(SYS_wait4, pid, status, options, rusage_p?&rusage:NULL);
    if (rusage_p) {
        rusage_guest->ru_utime.tv_sec = rusage.ru_utime.tv_sec;
        rusage_guest->ru_utime.tv_usec = rusage.ru_utime.tv_usec;
        rusage_guest->ru_stime.tv_sec = rusage.ru_stime.tv_sec;
        rusage_guest->ru_stime.tv_usec = rusage.ru_stime.tv_usec;
        rusage_guest->ru_maxrss = rusage.ru_maxrss;
        rusage_guest->ru_ixrss = rusage.ru_ixrss;
        rusage_guest->ru_idrss = rusage.ru_idrss;
        rusage_guest->ru_isrss = rusage.ru_isrss;
        rusage_guest->ru_minflt = rusage.ru_minflt;
        rusage_guest->ru_majflt = rusage.ru_majflt;
        rusage_guest->ru_nswap = rusage.ru_nswap;
        rusage_guest->ru_inblock = rusage.ru_inblock;
        rusage_guest->ru_oublock = rusage.ru_oublock;
        rusage_guest->ru_msgsnd = rusage.ru_msgsnd;
        rusage_guest->ru_msgrcv = rusage.ru_msgrcv;
        rusage_guest->ru_nsignals = rusage.ru_nsignals;
        rusage_guest->ru_nvcsw = rusage.ru_nvcsw;
        rusage_guest->ru_nivcsw = rusage.ru_nivcsw;
    }

    return res;
}
