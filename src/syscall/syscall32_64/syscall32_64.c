#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <unistd.h>
#include <sys/syscall.h>   /* For SYS_xxx definitions */
#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <errno.h>

#include "syscall32_64.h"

#define g_2_h(ptr)  ((uint64_t)(ptr))
#define h_2_g(ptr)  ((ptr))

int syscall32_64(Sysnum no, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5)
{
    int res = ENOSYS;

    switch(no) {
        case PR_exit:
            res = syscall(SYS_exit, (int)p0);
            break;
        case PR_write:
            res = syscall(SYS_write, (int)p0, (void *)g_2_h(p1), (size_t)p2);
            break;
        default:
            fatal("syscall_32_to_64: unsupported neutral syscall %d\n", no);
    }

    return res;
}
