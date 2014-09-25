#include <unistd.h>
#include <fcntl.h>

#include <errno.h>

#include "runtime.h"
#include "syscall32_64_types.h"
#include "syscall32_64_private.h"

/* FIXME: Found a way to handle this */
extern int x86ToArmFlags(long x86_flags);
extern long armToX86Flags(int arm_flags);

int fnctl64_s3264(uint32_t fd_p, uint32_t cmd_p, uint32_t opt_p)
{
    int res = -EINVAL;
    int fd = fd_p;
    int cmd = cmd_p;

    switch(cmd) {
        case F_GETFD:
            res = syscall(SYS_fcntl, fd, cmd);
            break;
        case F_SETFD:
            res = syscall(SYS_fcntl, fd, cmd, opt_p);
            break;
        case F_GETFL:
            res = syscall(SYS_fcntl, fd, cmd);
            res = x86ToArmFlags(res);
            break;
        case F_SETFL:
            res = syscall(SYS_fcntl, fd, cmd, armToX86Flags(opt_p));
            break;
        case 13://F_SETLK64
            {
                struct flock64_32 *lock_guest = (struct flock64_32 *) g_2_h(opt_p);
                struct flock lock;

                if (opt_p == 0 || opt_p == 0xffffffff)
                    res = -EFAULT;
                else {
                    lock.l_type = lock_guest->l_type;
                    lock.l_whence = lock_guest->l_whence;
                    lock.l_start = lock_guest->l_start;
                    lock.l_len = lock_guest->l_len;
                    lock.l_pid = lock_guest->l_pid;
                    res = syscall(SYS_fcntl, fd, F_SETLK, &lock);
                }
            }
            break;
        default:
            fatal("unsupported fnctl64 command %d\n", cmd);
    }

    return res;
}
