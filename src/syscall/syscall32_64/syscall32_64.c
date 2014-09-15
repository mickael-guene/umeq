#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <unistd.h>
#include <sys/syscall.h>   /* For SYS_xxx definitions */
#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <errno.h>

/* MAP_32BIT */
#include <sys/mman.h>

#include "sysnum.h"
#include "syscall32_64_types.h"
#include "syscall32_64_private.h"
#include "syscall32_64.h"
#include "runtime.h"

int syscall32_64(Sysnum no, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5)
{
    int res = ENOSYS;

    switch(no) {
        case PR_access:
            res = syscall(SYS_access, (const char *) g_2_h(p0), (int) p1);
            break;
        case PR_close:
            res = syscall(SYS_close, (int)p0);
            break;
        case PR_read:
            res = syscall(SYS_read, (int)p0, (void *) g_2_h(p1), (size_t) p2);
            break;
        case PR_exit:
            res = syscall(SYS_exit, (int)p0);
            break;
        case PR_exit_group:
            res = syscall(SYS_exit_group, (int)p0);
            break;
        case PR_write:
            res = syscall(SYS_write, (int)p0, (void *)g_2_h(p1), (size_t)p2);
            break;
        case PR_readlink:
            res = syscall(SYS_readlink, (const char *)g_2_h(p0), (char *)g_2_h(p1), (size_t)p2);
            break;
        case PR_fstat64:
            res = fstat64_s3264(p0, p1);
            break;
        case PR_mmap2:
            /* FIXME: 4096 make it not portable ... keep it here for the moment since mmap need reworks to bypass mmap_min_addr */
            res = syscall(SYS_mmap,
                          (void *) g_2_h(p0),
                          (size_t) p1,
                          (int) p2,
                          (int) (p3 | MAP_32BIT),
                          (int) p4,
                          (off_t) (p5 * 4096));
            /* Need to clean cache since old code can be in cache in the same area */
            cleanCaches(0, ~0);
            break;
        case PR_mprotect:
            res = syscall(SYS_mprotect, (void *) g_2_h(p0), (size_t) p1, (int) p2);
            break;
        case PR_munmap:
            res = syscall(SYS_munmap, (void *) g_2_h(p0), (size_t) p1);
            break;
        case PR_fcntl64:
            res = fnctl64_s3264(p0,p1,p2);
            break;
        case PR_ioctl:
            /* FIXME: need specific version to offset param according to ioctl */
            res = syscall(SYS_ioctl, (int) p0, (unsigned long) p1, p2, p3, p4, p5);
            break;
        case PR_getdents64:
            res = getdents64_s3264(p0,p1,p2);
            break;
        case PR_lstat64:
            res = lstat64_s3264(p0,p1);
            break;
        case PR_socket:
            res = syscall(SYS_socket, (int)p0, (int)(p1), (int)p2);
            break;
        case PR_connect:
            res = syscall(SYS_connect, (int)p0, (const struct sockaddr *) g_2_h(p1), (int)p2);
            break;
        case PR__llseek:
            res = llseek_s3264(p0,p1,p2,p3,p4);
            break;
        case PR_eventfd:
            res = syscall(SYS_eventfd, (int)p0, (int)(p1));
            break;
        case PR_clock_gettime:
            res = clock_gettime_s3264(p0, p1);
            break;
        case PR_stat64:
            res = stat64_s3264(p0,p1);
            break;
        case PR_rt_sigprocmask:
            res = syscall(SYS_rt_sigprocmask, (int)p0, (const sigset_t *) g_2_h(p1), (sigset_t *) g_2_h(p2));
            break;
        case PR_getuid32:
            res = syscall(SYS_getuid);
            break;
        case PR_getgid32:
            res = syscall(SYS_getgid);
            break;
        case PR_geteuid32:
            res = syscall(SYS_geteuid);
            break;
        case PR_getegid32:
            res = syscall(SYS_getegid);
            break;
        case PR_gettimeofday:
            res = gettimeofday_s3264(p0,p1);
            break;
        case PR_getpid:
            res = syscall(SYS_getpid);
            break;
        case PR_getppid:
            res = syscall(SYS_getppid);
            break;
        case PR_getpgrp:
            res = syscall(SYS_getpgrp);
            break;
        case PR_dup:
            res = syscall(SYS_dup, (int)p0);
            break;
        case PR_ugetrlimit:
            res = ugetrlimit_s3264(p0,p1);
            break;
        case PR_dup2:
            res = syscall(SYS_dup, (int)p0, (int)p1);
            break;
        case PR_setpgid:
            res = syscall(SYS_setpgid, (pid_t)p0, (pid_t)p1);
            break;
        case PR_pipe:
            res = syscall(SYS_pipe, (int *) g_2_h(p0));
            break;
        case PR_wait4:
            res = wait4_s3264(p0,p1,p2,p3);
            break;
        case PR_execve:
            res = execve_s3264(p0,p1,p2);
            break;
        case PR_chdir:
            res = syscall(SYS_chdir, (const char *) g_2_h(p0));
            break;
        case PR_prlimit64:
            res = prlimit64_s3264(p0,p1,p2,p3);
            break;
        case PR_lseek:
            res = syscall(SYS_lseek, (int)p0, (off_t)p1, (int)p2);
            break;
        case PR_getcwd:
            res = syscall(SYS_getcwd, (char *) g_2_h(p0), (size_t) p1);
            break;
        case PR_writev:
            res = writev_s3264(p0,p1,p2);
            break;
        default:
            fatal("syscall_32_to_64: unsupported neutral syscall %d\n", no);
    }

    return res;
}
