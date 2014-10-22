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
        case PR_faccessat:
            res = syscall(SYS_faccessat, (int) p0, (const char *) g_2_h_64(p1), (int) p2, (int) p3);
            break;
        case PR_exit_group:
            res = syscall(SYS_exit_group, (int) p0);
            break;
        case PR_close:
            res = syscall(SYS_close, (int) p0);
            break;
        case PR_mmap:
            res = syscall(SYS_mmap, (void *) g_2_h_64(p0), (size_t) p1, (int) p2, (int) p3, (int) p4, (off_t) p5);
            break;
        case PR_read:
            res = syscall(SYS_read, (int) p0, (void *) g_2_h_64(p1), (size_t) p2);
            break;
        case PR_mprotect:
            res = syscall(SYS_mprotect, (void *) g_2_h_64(p0), (size_t) p1, (int) p2);
            break;
        case PR_munmap:
            res = syscall(SYS_munmap, (void *) g_2_h_64(p0), (size_t) p1);
            break;
        case PR_set_tid_address:
            res = syscall(SYS_set_tid_address, (int *) g_2_h_64(p0));
            break;
        case PR_set_robust_list:
            /* FIXME: implement this correctly */
            res = -EPERM;
            break;
        case PR_rt_sigprocmask:
            res = syscall(SYS_rt_sigprocmask, (int)p0, p1?(const sigset_t *) g_2_h_64(p1):NULL,
                                              p2?(sigset_t *) g_2_h_64(p2):NULL, (size_t) p3);
            break;
        case PR_getrlimit:
            res = syscall(SYS_getrlimit, (int) p0, (struct rlimit *) g_2_h_64(p1));
            break;
        case PR_statfs:
            res = syscall(SYS_statfs, (const char *) g_2_h_64(p0), (struct statfs *) g_2_h_64(p1));
            break;
        case PR_ioctl:
            /* FIXME: need specific version to offset param according to ioctl */
            res = syscall(SYS_ioctl, (int) p0, (unsigned long) p1, (char *) g_2_h_64(p2));
            break;
        case PR_getdents64:
            res = syscall(SYS_getdents64, (unsigned int) p0, (struct linux_dirent *) g_2_h_64(p1), (unsigned int) p2);
            break;
        default:
            fatal("syscall64_64: unsupported neutral syscall %d\n", no);
    }

    return res;
}
