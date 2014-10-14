#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <unistd.h>
#include <sys/syscall.h>   /* For SYS_xxx definitions */
#include <errno.h>
#include <stdint.h>

#include "syscall64_64.h"
#include "syscall64_64_private.h"
#include "runtime.h"

long syscall64_64(Sysnum no, uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, uint64_t p4, uint64_t p5)
{
    long res = ENOSYS;

    switch(no) {
        case PR_write:
            res = syscall(SYS_write, (int)p0, (void *)g_2_h_64(p1), (size_t)p2);
            break;
        case PR_exit:
            res = syscall(SYS_exit, (int)p0);
            break;
        case PR_readlinkat:
            res = syscall(SYS_readlinkat, (int) p0, (const char *) g_2_h_64(p1), (char *) g_2_h_64(p2), (size_t) p3);
            break;
        default:
            fatal("syscall64_64: unsupported neutral syscall %d\n", no);
    }

    return res;
}
