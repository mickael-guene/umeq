/* This file is part of Umeq, an equivalent of qemu user mode emulation with improved robustness.
 *
 * Copyright (C) 2016 STMicroelectronics
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA.
 */

#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <unistd.h>
#include <sys/syscall.h>   /* For SYS_xxx definitions */
#include <errno.h>
#include <stdint.h>

#include "syscalls_neutral.h"
#include "syscalls_neutral_types.h"
#include "runtime.h"
#include "target32.h"
#include "umeq.h"

#define IS_NULL(px) ((uint32_t)((px)?g_2_h((px)):NULL))

static int futex_neutral(uint32_t uaddr_p, uint32_t op_p, uint32_t val_p, uint32_t timeout_p, uint32_t uaddr2_p, uint32_t val3_p)
{
    int res;
    int *uaddr = (int *) g_2_h(uaddr_p);
    int op = (int) op_p;
    int val = (int) val_p;
    const struct neutral_timespec_32 *timeout_guest = (struct neutral_timespec_32 *) g_2_h(timeout_p);
    int *uaddr2 = (int *) g_2_h(uaddr2_p);
    int val3 = (int) val3_p;
    struct neutral_timespec_32 timeout;
    int cmd = op & NEUTRAL_FUTEX_CMD_MASK;
    long syscall_timeout = (long) (timeout_p?&timeout:NULL);

    /* fixup syscall_timeout in case it's not really a timeout structure */
    if (cmd == NEUTRAL_FUTEX_REQUEUE || cmd == NEUTRAL_FUTEX_CMP_REQUEUE ||
        cmd == NEUTRAL_FUTEX_CMP_REQUEUE_PI || cmd == NEUTRAL_FUTEX_WAKE_OP) {
        syscall_timeout = timeout_p;
    } else if (cmd == NEUTRAL_FUTEX_WAKE) {
        /* in this case timeout argument is not use and can take any value */
        syscall_timeout = timeout_p;
    } else if (timeout_p) {
        timeout.tv_sec = timeout_guest->tv_sec;
        timeout.tv_nsec = timeout_guest->tv_nsec;
    }

    res = syscall_neutral_32(PR_futex, (uint32_t)uaddr, op, val, syscall_timeout, (uint32_t)uaddr2, val3);

    return res;
}

static int fcnt64_neutral(uint32_t fd_p, uint32_t cmd_p, uint32_t opt_p)
{
    int res = -EINVAL;
    int fd = fd_p;
    int cmd = cmd_p;

    switch(cmd) {
        case NEUTRAL_F_DUPFD:/*0*/
            res = syscall_neutral_32(PR_fcntl64, fd, cmd, opt_p, 0, 0, 0);
            break;
        case NEUTRAL_F_GETFD:/*1*/
            res = syscall_neutral_32(PR_fcntl64, fd, cmd, opt_p, 0, 0, 0);
            break;
        case NEUTRAL_F_SETFD:/*2*/
            res = syscall_neutral_32(PR_fcntl64, fd, cmd, opt_p, 0, 0, 0);
            break;
        case NEUTRAL_F_GETFL:/*3*/
            res = syscall_neutral_32(PR_fcntl64, fd, cmd, opt_p, 0, 0, 0);
            break;
        case NEUTRAL_F_SETFL:/*4*/
            res = syscall_neutral_32(PR_fcntl64, fd, cmd, opt_p, 0, 0, 0);
            break;
        case NEUTRAL_F_GETLK:/*5*/
            res = syscall_neutral_32(PR_fcntl64, fd, cmd, (uint32_t) g_2_h(opt_p), 0, 0, 0);
            break;
        case NEUTRAL_F_SETLK:/*6*/
            res = syscall_neutral_32(PR_fcntl64, fd, cmd, (uint32_t) g_2_h(opt_p), 0, 0, 0);
            break;
        case NEUTRAL_F_SETLKW:/*7*/
            res = syscall_neutral_32(PR_fcntl64, fd, cmd, (uint32_t) g_2_h(opt_p), 0, 0, 0);
            break;
        case NEUTRAL_F_SETOWN:/*8*/
            res = syscall_neutral_32(PR_fcntl64, fd, cmd, opt_p, 0, 0, 0);
            break;
        case NEUTRAL_F_GETOWN:/*9*/
            res = syscall_neutral_32(PR_fcntl64, fd, cmd, 0, 0, 0, 0);
            break;
        case NEUTRAL_F_SETSIG:/*10*/
            res = syscall_neutral_32(PR_fcntl64, fd, cmd, opt_p, 0, 0, 0);
            break;
        case NEUTRAL_F_GETSIG:/*11*/
            res = syscall_neutral_32(PR_fcntl64, fd, cmd, 0, 0, 0, 0);
            break;
        case NEUTRAL_F_GETLK64:/*12*/
            if (opt_p == 0 || opt_p == 0xffffffff)
                res = -EFAULT;
            else
                res = syscall_neutral_32(PR_fcntl64, fd, cmd, (uint32_t) g_2_h(opt_p), 0, 0, 0);
            break;
        case NEUTRAL_F_SETLK64:/*13*/
            if (opt_p == 0 || opt_p == 0xffffffff)
                res = -EFAULT;
            else
                res = syscall_neutral_32(PR_fcntl64, fd, cmd, (uint32_t) g_2_h(opt_p), 0, 0, 0);
            break;
        case NEUTRAL_F_SETLKW64:/*14*/
            if (opt_p == 0 || opt_p == 0xffffffff)
                res = -EFAULT;
            else
                res = syscall_neutral_32(PR_fcntl64, fd, cmd, (uint32_t) g_2_h(opt_p), 0, 0, 0);
            break;
        case NEUTRAL_F_SETOWN_EX:/*15*/
            res = syscall_neutral_32(PR_fcntl64, fd, cmd, (uint32_t) g_2_h(opt_p), 0, 0, 0);
            break;
        case NEUTRAL_F_GETOWN_EX:/*16*/
            res = syscall_neutral_32(PR_fcntl64, fd, cmd, (uint32_t) g_2_h(opt_p), 0, 0, 0);
            break;
        case NEUTRAL_F_SETLEASE:/*1024*/
            res = syscall_neutral_32(PR_fcntl64, fd, cmd, opt_p, 0, 0, 0);
            break;
        case NEUTRAL_F_GETLEASE:/*1025*/
            res = syscall_neutral_32(PR_fcntl64, fd, cmd, 0, 0, 0, 0);
            break;
        case NEUTRAL_F_DUPFD_CLOEXEC:/*1030*/
            res = syscall_neutral_32(PR_fcntl64, fd, cmd, opt_p, 0, 0, 0);
            break;
        case NEUTRAL_F_SETPIPE_SZ:/*1031*/
            res = syscall_neutral_32(PR_fcntl64, fd, cmd, opt_p, 0, 0, 0);
            break;
        case NEUTRAL_F_GETPIPE_SZ:/*1032*/
            res = syscall_neutral_32(PR_fcntl64, fd, cmd, 0, 0, 0, 0);
            break;

        default:
            /* invalid cmd use by ltp testsuite */
            if (cmd == 99999)
                res = -EINVAL;
            else
                fatal("unsupported fnctl64 command %d\n", cmd);
    }

    return res;
}

/* FIXME: use alloca to allocate space for ptr */
/* FIXME: work only under proot */
static int execve_neutral(uint32_t filename_p,uint32_t argv_p,uint32_t envp_p)
{
    int res;
    const char *filename = (const char *) g_2_h(filename_p);
    uint32_t * argv_guest = (uint32_t *) g_2_h(argv_p);
    uint32_t * envp_guest = (uint32_t *) g_2_h(envp_p);
    char *ptr[4096];
    char **argv;
    char **envp;
    int index = 0;

    argv = &ptr[index];
    /* handle -execve mode */
    /* code adapted from https://github.com/resin-io/qemu/commit/6b9e5be0fbc07ae3d6525bbd57c60da58d33b840 and
       https://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/tree/fs/binfmt_script.c */
    if (is_umeq_call_in_execve) {
        char *i_arg = NULL;
        char *i_name = NULL;

        /* test for shebang and file exists */
        res = parse_shebang(filename, &i_name, &i_arg);
        if (res)
            return res;

        /* insert umeq */
        ptr[index++] = umeq_filename;
        ptr[index++] = "-execve";
        ptr[index++] = "-0";
        if (i_name) {
            ptr[index++] = i_name;
            ptr[index++] = i_name;
            if (i_arg)
                ptr[index++] = i_arg;
        } else {
            ptr[index++] = (char *) g_2_h(*argv_guest);
        }
        argv_guest = (uint32_t *) ((long)argv_guest + 4);
        ptr[index++] = filename;
    }
    /* Manual say 'On Linux, argv and envp can be specified as NULL' */
    /* In that case we give array of length 1 with only one null element */
    if (argv_p) {
        while(*argv_guest != 0) {
            ptr[index++] = (char *) g_2_h(*argv_guest);
            argv_guest = (uint32_t *) ((long)argv_guest + 4);
        }
    }
    ptr[index++] = NULL;
    envp = &ptr[index];
    /* add internal umeq environment variable if process may be ptraced */
    if (maybe_ptraced)
        ptr[index++] = "__UMEQ_INTERNAL_MAYBE_PTRACED__=1";
    if (envp_p) {
        while(*envp_guest != 0) {
            ptr[index++] = (char *) g_2_h(*envp_guest);
            envp_guest = (uint32_t *) ((long)envp_guest + 4);
        }
    }
    ptr[index++] = NULL;
    /* sanity check in case we overflow => see fixme */
    if (index >= sizeof(ptr) / sizeof(char *))
        assert(0);

    res = syscall_neutral_32(PR_execve, (uint32_t)(is_umeq_call_in_execve?umeq_filename:filename), (uint32_t)argv, (uint32_t)envp, 0, 0, 0);

    return res;
}

static int readv_neutral(uint32_t fd_p, uint32_t iov_p, uint32_t iovcnt_p)
{
    int res;
    int fd = (int) fd_p;
    struct neutral_iovec_32 *iov_guest = (struct neutral_iovec_32 *) g_2_h(iov_p);
    int iovcnt = (int) iovcnt_p;
    struct neutral_iovec_32 *iov;
    int i;

    if (iovcnt < 0)
        return -EINVAL;
    iov = (struct neutral_iovec_32 *) alloca(sizeof(struct neutral_iovec_32) * iovcnt);
    for(i = 0; i < iovcnt; i++) {
        iov[i].iov_base = ptr_2_int(g_2_h(iov_guest[i].iov_base));
        iov[i].iov_len = iov_guest[i].iov_len;
    }
    res = syscall_neutral_32(PR_readv, fd, (uint32_t) iov, iovcnt, 0, 0, 0);

    return res;
}

static int writev_neutral(uint32_t fd_p, uint32_t iov_p, uint32_t iovcnt_p)
{
    int res;
    int fd = (int) fd_p;
    struct neutral_iovec_32 *iov_guest = (struct neutral_iovec_32 *) g_2_h(iov_p);
    int iovcnt = (int) iovcnt_p;
    struct neutral_iovec_32 *iov;
    int i;

    if (iovcnt < 0)
        return -EINVAL;
    iov = (struct neutral_iovec_32 *) alloca(sizeof(struct neutral_iovec_32) * iovcnt);
    for(i = 0; i < iovcnt; i++) {
        iov[i].iov_base = ptr_2_int(g_2_h(iov_guest[i].iov_base));
        iov[i].iov_len = iov_guest[i].iov_len;
    }
    res = syscall_neutral_32(PR_writev, fd, (uint32_t) iov, iovcnt, 0, 0, 0);

    return res;
}

static int prctl_neutral(uint32_t option_p, uint32_t arg2_p, uint32_t arg3_p, uint32_t arg4_p, uint32_t arg5_p)
{
    int res;
    int option = (int) option_p;

    switch(option) {
        case NEUTRAL_PR_GET_PDEATHSIG:/*2*/
        case NEUTRAL_PR_SET_NAME:/*15*/
            res = syscall_neutral_32(PR_prctl, option, (uint32_t) g_2_h(arg2_p), arg3_p, arg4_p, arg5_p, 0);
            break;
        default:
            res = syscall_neutral_32(PR_prctl, option, arg2_p, arg3_p, arg4_p, arg5_p, 0);
    }

    return res;
}

/* FIXME: not well done. Should use neutral_sigevent_32 to be portable */
static int mq_notify_neutral(uint32_t mqdes_p, uint32_t sevp_p)
{
    int res;
    neutral_mqd_t mqdes = (neutral_mqd_t) mqdes_p;
    struct neutral_sigevent_32 *sevp_guest = (struct neutral_sigevent_32 *) g_2_h(sevp_p);
    struct neutral_sigevent_32 evp;

    if (sevp_p) {
        evp = *sevp_guest;
        if (evp.sigev_notify == NEUTRAL_SIGEV_THREAD)
            evp.sigev_value.sival_ptr = ptr_2_int(g_2_h(evp.sigev_value.sival_ptr));
    }
    res = syscall_neutral_32(PR_mq_notify, mqdes, (uint32_t) (sevp_p?&evp:NULL), 0, 0, 0, 0);

    return res;
}

static int keyctl_neutral(uint32_t cmd_p, uint32_t arg2_p, uint32_t arg3_p, uint32_t arg4_p, uint32_t arg5_p)
{
    int res;
    int cmd = (int) cmd_p;

    switch(cmd) {
        case NEUTRAL_KEYCTL_GET_KEYRING_ID:
            res = syscall_neutral_32(PR_keyctl, cmd, (neutral_key_serial_t) arg2_p, (int) arg3_p, arg4_p, arg5_p, 0);
            break;
        case NEUTRAL_KEYCTL_REVOKE:
            res = syscall_neutral_32(PR_keyctl, cmd, (neutral_key_serial_t) arg2_p, arg3_p, arg4_p, arg5_p, 0);
            break;
        case NEUTRAL_KEYCTL_READ:
            res = syscall_neutral_32(PR_keyctl, cmd, (neutral_key_serial_t) arg2_p, (uint32_t) g_2_h(arg3_p), (size_t) arg4_p, arg5_p, 0);
            break;
        default:
            fatal("keyctl cmd %d not supported\n", cmd);
    }

    return res;
}

static int semctl_neutral(uint32_t semid_p, uint32_t semnum_p, uint32_t cmd_p, uint32_t arg_p)
{
    void *arg = ((cmd_p & ~0x100) == NEUTRAL_SETVAL) ? (void *)arg_p : g_2_h(arg_p);

    return syscall_neutral_32(PR_semctl, semid_p, semnum_p, cmd_p, (uint32_t) arg, 0, 0);
}

struct data_32_internal {
    uint32_t ss;
    uint32_t ss_len;
};

static int pselect6_neutral(uint32_t nfds_p, uint32_t readfds_p, uint32_t writefds_p, uint32_t exceptfds_p, uint32_t timeout_p, uint32_t data_p)
{
    int res;
    int nfds = (int) nfds_p;
    neutral_fd_set *readfds = (neutral_fd_set *) g_2_h(readfds_p);
    neutral_fd_set *writefds = (neutral_fd_set *) g_2_h(writefds_p);
    neutral_fd_set *exceptfds = (neutral_fd_set *) g_2_h(exceptfds_p);
    struct neutral_timespec_32 *timeout_guest = (struct neutral_timespec_32 *) g_2_h(timeout_p);
    struct data_32_internal *data_guest = (struct data_32_internal *) g_2_h(data_p);
    struct neutral_timespec_32 timeout;
    struct data_32_internal data;


    if (timeout_p) {
        timeout.tv_sec = timeout_guest->tv_sec;
        timeout.tv_nsec = timeout_guest->tv_nsec;
    };

    /* FIXME: what if data_p NULL ? */
    data.ss = (uint32_t) (data_guest->ss?g_2_h(data_guest->ss):NULL);
    data.ss_len = data_guest->ss_len;
    res = syscall_neutral_32(PR_pselect6, nfds, (uint32_t) (readfds_p?readfds:NULL), (uint32_t) (writefds_p?writefds:NULL),
                             (uint32_t) (exceptfds_p?exceptfds:NULL), (uint32_t) (timeout_p?&timeout:NULL), (uint32_t) (&data));

    if (timeout_p) {
        timeout_guest->tv_sec = timeout.tv_sec;
        timeout_guest->tv_nsec = timeout.tv_nsec;
    };

    return res;
}

static int vmsplice_neutral(uint32_t fd_p, uint32_t iov_p, uint32_t nr_segs_p, uint32_t flags_p)
{
    int res;
    int fd = (int) fd_p;
    struct neutral_iovec_32 *iov_guest = (struct neutral_iovec_32 *) g_2_h(iov_p);
    unsigned long nr_segs = (unsigned long) nr_segs_p;
    unsigned int flags = (unsigned int) flags_p;
    struct neutral_iovec_32 *iov;
    int i;

    iov = (struct neutral_iovec_32 *) alloca(sizeof(struct neutral_iovec_32) * nr_segs);
    for(i = 0; i < nr_segs; i++) {
        iov[i].iov_base = ptr_2_int(g_2_h(iov_guest->iov_base));
        iov[i].iov_len = iov_guest->iov_len;
    }
    res = syscall_neutral_32(PR_vmsplice, fd, (uint32_t) iov, nr_segs, flags, 0, 0);

    return res;
}

static int sendmsg_neutral(uint32_t sockfd_p, uint32_t msg_p, uint32_t flags_p)
{
    int res;
    int sockfd = (int) sockfd_p;
    struct neutral_msghdr_32 *msg_guest = (struct neutral_msghdr_32 *) g_2_h(msg_p);
    int flags = (int) flags_p;
    struct neutral_iovec_32 iovec[16]; //max 16 iovect. If more is need then use mmap or alloca
    struct neutral_msghdr_32 msg;
    int i;

    assert(msg_guest->msg_iovlen <= 16);

    msg.msg_name = ptr_2_int(msg_guest->msg_name?(void *) g_2_h(msg_guest->msg_name):NULL);
    msg.msg_namelen = msg_guest->msg_namelen;
    for(i = 0; i < msg_guest->msg_iovlen; i++) {
        struct neutral_iovec_32 *iovec_guest = (struct neutral_iovec_32 *) g_2_h(msg_guest->msg_iov + sizeof(struct neutral_iovec_32) * i);

        iovec[i].iov_base = ptr_2_int(g_2_h(iovec_guest->iov_base));
        iovec[i].iov_len = iovec_guest->iov_len;
    }
    msg.msg_iov = ptr_2_int(iovec);
    msg.msg_iovlen = msg_guest->msg_iovlen;
    msg.msg_control = ptr_2_int(g_2_h(msg_guest->msg_control));
    msg.msg_controllen = msg_guest->msg_controllen;
    msg.msg_flags = msg_guest->msg_flags;

    res = syscall_neutral_32(PR_sendmsg, sockfd, (uint32_t) &msg, flags, 0, 0, 0);

    return res;
}

static int recvmsg_neutral(uint32_t sockfd_p, uint32_t msg_p, uint32_t flags_p)
{
    int res;
    int sockfd = (int) sockfd_p;
    struct neutral_msghdr_32 *msg_guest = (struct neutral_msghdr_32 *) g_2_h(msg_p);
    int flags = (int) flags_p;
    struct neutral_iovec_32 iovec[16]; //max 16 iovect. If more is need then use mmap or alloca
    struct neutral_msghdr_32 msg;
    int i;

    assert(msg_guest->msg_iovlen <= 16);

    msg.msg_name = ptr_2_int(msg_guest->msg_name?(void *) g_2_h(msg_guest->msg_name):NULL);
    msg.msg_namelen = msg_guest->msg_namelen;
    for(i = 0; i < msg_guest->msg_iovlen; i++) {
        struct neutral_iovec_32 *iovec_guest = (struct neutral_iovec_32 *) g_2_h(msg_guest->msg_iov + sizeof(struct neutral_iovec_32) * i);

        iovec[i].iov_base = ptr_2_int(g_2_h(iovec_guest->iov_base));
        iovec[i].iov_len = iovec_guest->iov_len;
    }
    msg.msg_iov = ptr_2_int(iovec);
    msg.msg_iovlen = msg_guest->msg_iovlen;
    msg.msg_control = ptr_2_int(g_2_h(msg_guest->msg_control));
    msg.msg_controllen = msg_guest->msg_controllen;
    msg.msg_flags = msg_guest->msg_flags;

    res = syscall_neutral_32(PR_recvmsg, sockfd, (uint32_t) &msg, flags, 0, 0, 0);
    if (res >= 0) {
        msg_guest->msg_namelen = msg.msg_namelen;
        msg_guest->msg_controllen = msg.msg_controllen;
    }

    return res;
}

static int sendmmsg_neutral(uint32_t sockfd_p, uint32_t msgvec_p, uint32_t vlen_p, uint32_t flags_p)
{
    long res;
    int sockfd = (int) sockfd_p;
    struct neutral_mmsghdr_32 *msgvec_guest = (struct neutral_mmsghdr_32 *) g_2_h(msgvec_p);
    unsigned int vlen = (unsigned int) vlen_p;
    unsigned int flags = (unsigned int) flags_p;
    struct neutral_mmsghdr_32 *msgvec = (struct neutral_mmsghdr_32 *) alloca(vlen * sizeof(struct neutral_mmsghdr_32));
    int i, j;

    for(i = 0; i < vlen; i++) {
        msgvec[i].msg_hdr.msg_name = ptr_2_int(msgvec_guest[i].msg_hdr.msg_name?(void *) g_2_h(msgvec_guest[i].msg_hdr.msg_name):NULL);
        msgvec[i].msg_hdr.msg_namelen = msgvec_guest[i].msg_hdr.msg_namelen;
        msgvec[i].msg_hdr.msg_iov = ptr_2_int(alloca(msgvec_guest[i].msg_hdr.msg_iovlen * sizeof(struct neutral_iovec_32)));
        for(j = 0; j < msgvec_guest[i].msg_hdr.msg_iovlen; j++) {
            struct neutral_iovec_32 *iovec_guest = (struct neutral_iovec_32 *) g_2_h(msgvec_guest[i].msg_hdr.msg_iov + sizeof(struct neutral_iovec_32) * j);
            struct neutral_iovec_32 *iovec = (struct neutral_iovec_32 *) (msgvec[i].msg_hdr.msg_iov + sizeof(struct neutral_iovec_32) * j);

            iovec->iov_base = ptr_2_int(g_2_h(iovec_guest->iov_base));
            iovec->iov_len = iovec_guest->iov_len;
        }
        msgvec[i].msg_hdr.msg_iovlen = msgvec_guest[i].msg_hdr.msg_iovlen;
        msgvec[i].msg_hdr.msg_control = ptr_2_int(g_2_h(msgvec_guest[i].msg_hdr.msg_control));
        msgvec[i].msg_hdr.msg_controllen = msgvec_guest[i].msg_hdr.msg_controllen;
        msgvec[i].msg_hdr.msg_flags = msgvec_guest[i].msg_hdr.msg_flags;
        msgvec[i].msg_len = msgvec_guest[i].msg_len;
    }
    res = syscall_neutral_32(PR_sendmmsg, sockfd, (uint32_t) msgvec, vlen, flags, 0, 0);
    for(i = 0; i < vlen; i++)
        msgvec_guest[i].msg_len = msgvec[i].msg_len;

    return res;
}

int syscall_adapter_guest32(Sysnum no, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5)
{
    int res = -ENOSYS;

    switch(no) {
        case PR__newselect:
            res = syscall_neutral_32(PR__newselect, p0, IS_NULL(p1), IS_NULL(p2), IS_NULL(p3), IS_NULL(p4), p5);
            break;
        case PR__llseek:
            res = syscall_neutral_32(PR__llseek, p0, p1, p2, (uint32_t) g_2_h(p3), p4, p5);
            break;
        case PR_accept:
            res = syscall_neutral_32(PR_accept, p0, IS_NULL(p1), IS_NULL(p2), p3, p4, p5);
            break;
        case PR_accept4:
            res = syscall_neutral_32(PR_accept4, p0, IS_NULL(p1), IS_NULL(p2), p3, p4, p5);
            break;
        case PR_access:
            res = syscall_neutral_32(PR_access, (uint32_t)g_2_h(p0), p1, p2, p3, p4, p5);
            break;
        case PR_add_key:
            res = syscall_neutral_32(PR_add_key, (uint32_t) g_2_h(p0), IS_NULL(p1), IS_NULL(p2), p3, p4, p5);
            break;
        case PR_bdflush:
            /* This one is deprecated since 2.6 and p1 is no more use */
            res = 0;
            break;
        case PR_bind:
            res= syscall_neutral_32(PR_bind, p0, (uint32_t) g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_capget:
            res = syscall_neutral_32(PR_capget, (uint32_t) g_2_h(p0), IS_NULL(p1), p2, p3, p4, p5);
            break;
        case PR_capset:
            res = syscall_neutral_32(PR_capset, (uint32_t) g_2_h(p0), IS_NULL(p1), p2, p3, p4, p5);
            break;
        case PR_chdir:
            res = syscall_neutral_32(PR_chdir, (uint32_t) g_2_h(p0), p1, p2, p3, p4, p5);
            break;
        case PR_chmod:
            res = syscall_neutral_32(PR_chmod, (uint32_t) g_2_h(p0), p1, p2, p3, p4, p5);
            break;
        case PR_chown32:
            res = syscall_neutral_32(PR_chown32, (uint32_t) g_2_h(p0), p1, p2, p3, p4, p5);
            break;
        case PR_chroot:
            res = syscall_neutral_32(PR_chroot, (uint32_t) g_2_h(p0), p1, p2, p3, p4, p5);
            break;
        case PR_clock_getres:
            res = syscall_neutral_32(PR_clock_getres, p0, IS_NULL(p1), p2, p3, p4, p5);
            break;
        case PR_clock_gettime:
            res = syscall_neutral_32(PR_clock_gettime, p0, (uint32_t)g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_clock_nanosleep:
            res = syscall_neutral_32(PR_clock_nanosleep, p0, p1, (uint32_t) g_2_h(p2), IS_NULL(p3), p4, p5);
            break;
        case PR_clock_settime:
            res = syscall_neutral_32(PR_clock_settime, p0, (uint32_t)g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_close:
            res = syscall_neutral_32(PR_close, p0, p1, p2, p3, p4, p5);
            break;
        case PR_connect:
            res = syscall_neutral_32(PR_connect, p0, (uint32_t)g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_creat:
            res = syscall_neutral_32(PR_creat, (uint32_t) g_2_h(p0), p1, p2, p3, p4, p5);
            break;
        case PR_dup:
            res = syscall_neutral_32(PR_dup, p0, p1, p2, p3, p4, p5);
            break;
        case PR_dup2:
            res = syscall_neutral_32(PR_dup2, p0, p1, p2, p3, p4, p5);
            break;
        case PR_dup3:
            res = syscall_neutral_32(PR_dup3, p0, p1, p2, p3, p4, p5);
            break;
        case PR_epoll_ctl:
            res = syscall_neutral_32(PR_epoll_ctl, p0, p1, p2, IS_NULL(p3), p4, p5);
            break;
        case PR_epoll_create:
            res = syscall_neutral_32(PR_epoll_create, p0, p1, p2, p3, p4, p5);
            break;
        case PR_epoll_create1:
            res = syscall_neutral_32(PR_epoll_create1, p0, p1, p2, p3, p4, p5);
            break;
        case PR_epoll_wait:
            res = syscall_neutral_32(PR_epoll_wait, p0, (uint32_t)g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_eventfd:
            res = syscall_neutral_32(PR_eventfd, p0, p1, p2, p3, p4, p5);
            break;
        case PR_eventfd2:
            res = syscall_neutral_32(PR_eventfd2, p0, p1, p2, p3, p4, p5);
            break;
        case PR_execve:
            res = execve_neutral(p0, p1, p2);
            break;
        case PR_exit_group:
            res = syscall_neutral_32(PR_exit_group, p0, p1, p2, p3, p4, p5);
            break;
        case PR_faccessat:
            res = syscall_neutral_32(PR_faccessat, p0, (uint32_t)g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_fadvise64_64:
            res = syscall_neutral_32(PR_fadvise64_64, p0, p1, p2, p3, p4, p5);
            break;
        case PR_fallocate:
            res = syscall_neutral_32(PR_fallocate, p0, p1, p2, p3, p4, p5);
            break;
        case PR_fchdir:
            res = syscall_neutral_32(PR_fchdir, p0, p1, p2, p3, p4, p5);
            break;
        case PR_fchmod:
            res = syscall_neutral_32(PR_fchmod, p0, p1, p2, p3, p4, p5);
            break;
        case PR_fchmodat:
            res = syscall_neutral_32(PR_fchmodat, p0, (uint32_t)g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_fchown32:
            res = syscall_neutral_32(PR_fchown32, p0, p1, p2, p3, p4, p5);
            break;
        case PR_fchownat:
            res = syscall_neutral_32(PR_fchownat, p0, (uint32_t) g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_fcntl64:
            res = fcnt64_neutral(p0, p1, p2);
            break;
        case PR_fdatasync:
            res = syscall_neutral_32(PR_fdatasync, p0, p1, p2, p3, p4, p5);
            break;
        case PR_fgetxattr:
            res = syscall_neutral_32(PR_fgetxattr, p0, (uint32_t) g_2_h(p1), (uint32_t) g_2_h(p2), p3, p4, p5);
            break;
        case PR_flistxattr:
            res = syscall_neutral_32(PR_flistxattr, p0, (uint32_t) g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_flock:
            res = syscall_neutral_32(PR_flock, p0, p1, p2, p3, p4, p5);
            break;
        case PR_fsetxattr:
            res = syscall_neutral_32(PR_fsetxattr, p0, (uint32_t) g_2_h(p1), (uint32_t) g_2_h(p2), p3, p4, p5);
            break;
        case PR_ftruncate:
            res = syscall_neutral_32(PR_ftruncate, p0, p1, p2, p3, p4, p5);
            break;
        case PR_ftruncate64:
            res = syscall_neutral_32(PR_ftruncate64, p0, p1, p2, p3, p4, p5);
            break;
        case PR_futimesat:
            res = syscall_neutral_32(PR_futimesat, p0, IS_NULL(p1), IS_NULL(p2), p3, p4, p5);
            break;
        case PR_futex:
            res = futex_neutral(p0, p1, p2, p3, p4, p5);
            break;
        case PR_fstat64:
            res = syscall_neutral_32(PR_fstat64, p0, (uint32_t)g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_fstatat64:
            res = syscall_neutral_32(PR_fstatat64, p0, (uint32_t)g_2_h(p1), (uint32_t)g_2_h(p2), p3, p4, p5);
            break;
        case PR_fstatfs:
            res = syscall_neutral_32(PR_fstatfs, p0, (uint32_t)g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_fstatfs64:
            res = syscall_neutral_32(PR_fstatfs64, p0, p1, (uint32_t) g_2_h(p2), p3, p4, p5);
            break;
        case PR_fsync:
            res = syscall_neutral_32(PR_fsync, p0, p1, p2, p3, p4, p5);
            break;
        case PR_getcwd:
            res = syscall_neutral_32(PR_getcwd, (uint32_t)g_2_h(p0), p1, p2, p3, p4, p5);
            break;
        case PR_getdents:
            res = syscall_neutral_32(PR_getdents, p0, (uint32_t)g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_getdents64:
            res = syscall_neutral_32(PR_getdents64, p0, (uint32_t)g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_getegid:
            res = syscall_neutral_32(PR_getegid, p0, p1, p2, p3, p4, p5);
            break;
        case PR_getegid32:
            res = syscall_neutral_32(PR_getegid32, p0, p1, p2, p3, p4, p5);
            break;
        case PR_geteuid:
            res = syscall_neutral_32(PR_geteuid, p0, p1, p2, p3, p4, p5);
            break;
        case PR_geteuid32:
            res = syscall_neutral_32(PR_geteuid32, p0, p1, p2, p3, p4, p5);
            break;
        case PR_getgid:
            res = syscall_neutral_32(PR_getgid, p0, p1, p2, p3, p4, p5);
            break;
        case PR_getgid32:
            res = syscall_neutral_32(PR_getgid32, p0, p1, p2, p3, p4, p5);
            break;
        case PR_getitimer:
            res = syscall_neutral_32(PR_getitimer, p0, (uint32_t)g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_getgroups:
            res = syscall_neutral_32(PR_getgroups, p0, (uint32_t) g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_getgroups32:
            res = syscall_neutral_32(PR_getgroups32, p0, (uint32_t) g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_getrusage:
            res = syscall_neutral_32(PR_getrusage, p0, (uint32_t)g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_getpeername:
            res = syscall_neutral_32(PR_getpeername, p0, (uint32_t)g_2_h(p1), (uint32_t)g_2_h(p2), p3, p4, p5);
            break;
        case PR_getpgid:
            res = syscall_neutral_32(PR_getpgid, p0, p1, p2, p3, p4, p5);
            break;
        case PR_getpgrp:
            res = syscall_neutral_32(PR_getpgrp, p0, p1, p2, p3, p4, p5);
            break;
        case PR_getpid:
            res = syscall_neutral_32(PR_getpid, p0, p1, p2, p3, p4, p5);
            break;
        case PR_getppid:
            res = syscall_neutral_32(PR_getppid, p0, p1, p2, p3, p4, p5);
            break;
        case PR_getpriority:
            res = syscall_neutral_32(PR_getpriority, p0, p1, p2, p3, p4, p5);
            break;
        case PR_getresgid32:
            res = syscall_neutral_32(PR_getresgid32, (uint32_t) g_2_h(p0), (uint32_t) g_2_h(p1), (uint32_t) g_2_h(p2), p3, p4, p5);
            break;
        case PR_getresuid32:
            res = syscall_neutral_32(PR_getresuid32, (uint32_t) g_2_h(p0), (uint32_t) g_2_h(p1), (uint32_t) g_2_h(p2), p3, p4, p5);
            break;
        case PR_getrlimit:
            res = syscall_neutral_32(PR_getrlimit, p0, (uint32_t) g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_getsid:
            res = syscall_neutral_32(PR_getsid, p0, p1, p2, p3, p4, p5);
            break;
        case PR_getsockname:
            res = syscall_neutral_32(PR_getsockname, p0, (uint32_t) g_2_h(p1), (uint32_t) g_2_h(p2), p3, p4, p5);
            break;
        case PR_getsockopt:
            res = syscall_neutral_32(PR_getsockopt, p0, p1, p2, IS_NULL(p3), IS_NULL(p4), p5);
            break;
        case PR_gettid:
            res = syscall_neutral_32(PR_gettid, p0, p1, p2, p3, p4, p5);
            break;
        case PR_gettimeofday:
            res = syscall_neutral_32(PR_gettimeofday, IS_NULL(p0), IS_NULL(p1), p2, p3, p4, p5);
            break;
        case PR_getuid:
            res = syscall_neutral_32(PR_getuid, p0, p1, p2, p3, p4, p5);
            break;
        case PR_getuid32:
            res = syscall_neutral_32(PR_getuid32, p0, p1, p2, p3, p4, p5);
            break;
        case PR_getxattr:
            res = syscall_neutral_32(PR_getxattr, (uint32_t) g_2_h(p0), (uint32_t) g_2_h(p1), (uint32_t) g_2_h(p2), p3, p4, p5);
            break;
        case PR_inotify_init:
            res = syscall_neutral_32(PR_inotify_init, p0, p1, p2, p3, p4, p5);
            break;
        case PR_inotify_init1:
            res = syscall_neutral_32(PR_inotify_init1, p0, p1, p2, p3, p4, p5);
            break;
        case PR_inotify_add_watch:
            res = syscall_neutral_32(PR_inotify_add_watch, p0, (uint32_t) g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_inotify_rm_watch:
            res = syscall_neutral_32(PR_inotify_rm_watch, p0, p1, p2, p3, p4, p5);
            break;
        case PR_ioctl:
            /* FIXME: need specific version to offset param according to ioctl */
            res = syscall_neutral_32(PR_ioctl, p0, p1, (uint32_t) g_2_h(p2), p3, p4, p5);
            break;
        case PR_keyctl:
            res = keyctl_neutral(p0, p1, p2, p3, p4);
            break;
        case PR_kill:
            res = syscall_neutral_32(PR_kill, p0, p1, p2, p3, p4, p5);
            break;
        case PR_lchown32:
            res = syscall_neutral_32(PR_lchown32, (uint32_t) g_2_h(p0), p1, p2, p3, p4, p5);
            break;
        case PR_link:
            res = syscall_neutral_32(PR_link, (uint32_t) g_2_h(p0), (uint32_t) g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_linkat:
            res = syscall_neutral_32(PR_linkat, p0, (uint32_t) g_2_h(p1), p2, (uint32_t) g_2_h(p3), p4, p5);
            break;
        case PR_listen:
            res = syscall_neutral_32(PR_listen, p0, p1, p2, p3, p4, p5);
            break;
        case PR_lgetxattr:
            res = syscall_neutral_32(PR_lgetxattr, (uint32_t) g_2_h(p0), (uint32_t) g_2_h(p1), (uint32_t) g_2_h(p2), p3, p4, p5);
            break;
        case PR_lseek:
            res = syscall_neutral_32(PR_lseek, p0, p1, p2, p3, p4, p5);
            break;
        case PR_lstat64:
            res = syscall_neutral_32(PR_lstat64, (uint32_t) g_2_h(p0), (uint32_t)g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_madvise:
            res = syscall_neutral_32(PR_madvise, (uint32_t) g_2_h(p0), p1, p2, p3, p4, p5);
            break;
        case PR_mincore:
            res = syscall_neutral_32(PR_mincore, (uint32_t) g_2_h(p0), p1, (uint32_t) g_2_h(p2), p3, p4, p5);
            break;
        case PR_mkdir:
            res = syscall_neutral_32(PR_mkdir, (uint32_t) g_2_h(p0), p1, p2, p3, p4, p5);
            break;
        case PR_mkdirat:
            res = syscall_neutral_32(PR_mkdirat, p0, (uint32_t) g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_mknod:
            res = syscall_neutral_32(PR_mknod, (uint32_t) g_2_h(p0), p1, p2, p3, p4, p5);
            break;
        case PR_mknodat:
            res = syscall_neutral_32(PR_mknodat, p0, (uint32_t) g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_mlock:
            res = syscall_neutral_32(PR_mlock, (uint32_t) g_2_h(p0), p1, p2, p3, p4, p5);
            break;
        case PR_mprotect:
            res = syscall_neutral_32(PR_mprotect, (uint32_t) g_2_h(p0), p1, p2, p3, p4, p5);
            break;
        case PR_mq_getsetattr:
            res = syscall_neutral_32(PR_mq_getsetattr, p0, IS_NULL(p1), IS_NULL(p2), p3, p4, p5);
            break;
        case PR_mq_notify:
            res = mq_notify_neutral(p0, p1);
            break;
        case PR_mq_open:
            res = syscall_neutral_32(PR_mq_open, (uint32_t) g_2_h(p0), p1, p2, IS_NULL(p3), p4, p5);
            break;
        case PR_mq_timedreceive:
            res = syscall_neutral_32(PR_mq_timedreceive, p0, (uint32_t) g_2_h(p1), p2, IS_NULL(p3), IS_NULL(p4), p5);
            break;
        case PR_mq_timedsend:
            res = syscall_neutral_32(PR_mq_timedsend, p0, (uint32_t) g_2_h(p1), p2, p3, IS_NULL(p4), p5);
            break;
        case PR_mq_unlink:
            res = syscall_neutral_32(PR_mq_unlink, (uint32_t) g_2_h(p0), p1, p2, p3, p4, p5);
            break;
        case PR_msgctl:
            res = syscall_neutral_32(PR_msgctl, p0, p1, (uint32_t) g_2_h(p2), p3, p4, p5);
            break;
        case PR_msgget:
            res = syscall_neutral_32(PR_msgget, p0, p1, p2, p3, p4, p5);
            break;
        case PR_msgrcv:
            res = syscall_neutral_32(PR_msgrcv, p0, (uint32_t) g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_msgsnd:
            res = syscall_neutral_32(PR_msgsnd, p0, (uint32_t) g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_msync:
            res = syscall_neutral_32(PR_msync, (uint32_t) g_2_h(p0), p1, p2, p3, p4, p5);
            break;
        case PR_munlock:
            res = syscall_neutral_32(PR_munlock, (uint32_t) g_2_h(p0), p1, p2, p3, p4, p5);
            break;
        case PR_nanosleep:
            res = syscall_neutral_32(PR_nanosleep, (uint32_t) g_2_h(p0), (uint32_t) g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_open:
            res = syscall_neutral_32(PR_open, (uint32_t) g_2_h(p0), p1, p2, p3, p4, p5);
            break;
        case PR_openat:
            res = syscall_neutral_32(PR_openat, p0, (uint32_t) g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_pause:
            res = syscall_neutral_32(PR_pause, p0, p1, p2, p3, p4, p5);
            break;
        case PR_perf_event_open:
            res = syscall_neutral_32(PR_perf_event_open, (uint32_t) g_2_h(p0), p1, p2, p3, p4, p5);
            break;
        case PR_personality:
            res = syscall_neutral_32(PR_personality, p0, p1, p2, p3, p4, p5);
            break;
        case PR_pipe:
            res = syscall_neutral_32(PR_pipe, (uint32_t)g_2_h(p0), p1, p2, p3, p4, p5);
            break;
        case PR_pipe2:
            res = syscall_neutral_32(PR_pipe2, (uint32_t) g_2_h(p0), p1, p2, p3, p4, p5);
            break;
        case PR_poll:
            res = syscall_neutral_32(PR_poll, (uint32_t) g_2_h(p0), p1, p2, p3, p4, p5);
            break;
        case PR_ppoll:
            res = syscall_neutral_32(PR_ppoll, (uint32_t) g_2_h(p0), p1, IS_NULL(p2), IS_NULL(p3), p4, p5);
            break;
        case PR_prctl:
            res = prctl_neutral(p0, p1, p2, p3, p4);
            break;
        case PR_pread64:
            res = syscall_neutral_32(PR_pread64, p0, (uint32_t)g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_prlimit64:
            res = syscall_neutral_32(PR_prlimit64, p0, p1, IS_NULL(p2), IS_NULL(p3), p4, p5);
            break;
        case PR_pselect6:
            res = pselect6_neutral(p0,p1,p2,p3,p4,p5);
            break;
        case PR_pwrite64:
            res = syscall_neutral_32(PR_pwrite64, p0, (uint32_t)g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_quotactl:
            res = syscall_neutral_32(PR_quotactl, p0, IS_NULL(p1), p2, (uint32_t) g_2_h(p3), p4, p5);
            break;
        case PR_read:
            res = syscall_neutral_32(PR_read, p0, (uint32_t)g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_readahead:
            res = syscall_neutral_32(PR_readahead, p0, p1, p2, p3, p4, p5);
            break;
        case PR_readlink:
            res = syscall_neutral_32(PR_readlink, (uint32_t)g_2_h(p0), (uint32_t)g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_readlinkat:
            res = syscall_neutral_32(PR_readlinkat, p0, (uint32_t) g_2_h(p1), (uint32_t) g_2_h(p2), p3, p4, p5);
            break;
        case PR_readv:
            res = readv_neutral(p0, p1, p2);
            break;
        case PR_recv:
            res = syscall_neutral_32(PR_recv, p0, (uint32_t) g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_recvfrom:
            res = syscall_neutral_32(PR_recvfrom, p0, (uint32_t) g_2_h(p1), p2, p3, IS_NULL(p4), IS_NULL(p5));
            break;
        case PR_recvmsg:
            res = recvmsg_neutral(p0,p1,p2);
            break;
        case PR_remap_file_pages:
            res = syscall_neutral_32(PR_remap_file_pages, (uint32_t) g_2_h(p0), p1, p2, p3, p4, p5);
            break;
        case PR_rename:
            res = syscall_neutral_32(PR_rename, (uint32_t)g_2_h(p0), (uint32_t)g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_rmdir:
            res = syscall_neutral_32(PR_rmdir, (uint32_t) g_2_h(p0), p1, p2, p3, p4, p5);
            break;
        case PR_rt_sigpending:
            res = syscall_neutral_32(PR_rt_sigpending, (uint32_t) g_2_h(p0), p1, p2, p3, p4, p5);
            break;
        case PR_rt_sigprocmask:
            res = syscall_neutral_32(PR_rt_sigprocmask, p0, IS_NULL(p1), IS_NULL(p2), p3, p4, p5);
            break;
        case PR_rt_sigqueueinfo:
            res = syscall_neutral_32(PR_rt_sigqueueinfo, p0, p1, (uint32_t) g_2_h(p2), p3, p4, p5);
            break;
        case PR_rt_sigsuspend:
            res = syscall_neutral_32(PR_rt_sigsuspend, (uint32_t) g_2_h(p0), p1, p2, p3, p4, p5);
            break;
        case PR_rt_sigtimedwait:
            res = syscall_neutral_32(PR_rt_sigtimedwait, (uint32_t) g_2_h(p0), IS_NULL(p1), IS_NULL(p2), p3, p4, p5);
            break;
        case PR_tgkill:
            res = syscall_neutral_32(PR_tgkill, p0, p1, p2, p3, p4, p5);
            break;
        case PR_truncate:
            res = syscall_neutral_32(PR_truncate, (uint32_t) g_2_h(p0), p1, p2, p3, p4, p5);
            break;
        case PR_sched_getaffinity:
            res = syscall_neutral_32(PR_sched_getaffinity, p0, p1, (uint32_t) g_2_h(p2), p3, p4, p5);
            break;
        case PR_sched_getparam:
            res = syscall_neutral_32(PR_sched_getparam, p0, IS_NULL(p1), p2, p3, p4, p5);
            break;
        case PR_sched_getscheduler:
            res = syscall_neutral_32(PR_sched_getscheduler, p0, p1, p2, p3, p4, p5);
            break;
        case PR_sched_get_priority_max:
            res = syscall_neutral_32(PR_sched_get_priority_max, p0, p1, p2, p3, p4, p5);
            break;
        case PR_sched_get_priority_min:
            res = syscall_neutral_32(PR_sched_get_priority_min, p0, p1, p2, p3, p4, p5);
            break;
        case PR_sched_rr_get_interval:
            res = syscall_neutral_32(PR_sched_rr_get_interval, p0, (uint32_t) g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_sched_setaffinity:
            res = syscall_neutral_32(PR_sched_setaffinity, p0, p1, (uint32_t) g_2_h(p2), p3, p4, p5);
            break;
        case PR_sched_setparam:
            res = syscall_neutral_32(PR_sched_setparam, p0, IS_NULL(p1), p2, p3, p4, p5);
            break;
        case PR_sched_setscheduler:
            res = syscall_neutral_32(PR_sched_setscheduler, p0, p1, (uint32_t) g_2_h(p2), p3, p4, p5);
            break;
        case PR_sched_yield:
            res = syscall_neutral_32(PR_sched_yield, p0, p1, p2, p3, p4, p5);
            break;
        case PR_semctl:
            res = semctl_neutral(p0, p1, p2, p3);
            break;
        case PR_semget:
            res = syscall_neutral_32(PR_semget, p0, p1, p2, p3, p4, p5);
            break;
        case PR_semop:
            res = syscall_neutral_32(PR_semop, p0, (uint32_t) g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_send:
            res = syscall_neutral_32(PR_send, p0, (uint32_t) g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_sendfile:
            res = syscall_neutral_32(PR_sendfile, p0, p1, IS_NULL(p2), p3, p4, p5);
            break;
        case PR_sendmmsg:
            res = sendmmsg_neutral(p0, p1, p2, p3);
            break;
        case PR_sendmsg:
            res = sendmsg_neutral(p0,p1,p2);
            break;
        case PR_sendto:
            res = syscall_neutral_32(PR_sendto, p0, (uint32_t) g_2_h(p1), p2, p3, (uint32_t) g_2_h(p4), p5);
            break;
        case PR_sendfile64:
            res = syscall_neutral_32(PR_sendfile64, p0, p1, IS_NULL(p2), p3, p4, p5);
            break;
        case PR_set_robust_list:
            /* FIXME: implement this correctly */
            res = -EPERM;
            break;
        case PR_set_tid_address:
            res = syscall_neutral_32(PR_set_tid_address, (uint32_t) g_2_h(p0), p1, p2, p3, p4, p5);
            break;
        case PR_setfsgid:
            res = syscall_neutral_32(PR_setfsgid, p0, p1, p2, p3, p4, p5);
            break;
        case PR_setfsgid32:
            res = syscall_neutral_32(PR_setfsgid32, p0, p1, p2, p3, p4, p5);
            break;
        case PR_setfsuid:
            res = syscall_neutral_32(PR_setfsuid, p0, p1, p2, p3, p4, p5);
            break;
        case PR_setfsuid32:
            res = syscall_neutral_32(PR_setfsuid32, p0, p1, p2, p3, p4, p5);
            break;
        case PR_setgroups32:
            res = syscall_neutral_32(PR_setgroups32, p0, (uint32_t) g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_setgid:
            res = syscall_neutral_32(PR_setgid, (gid_t) (int16_t)p0, p1, p2, p3, p4, p5);
            break;
        case PR_setgid32:
            res = syscall_neutral_32(PR_setgid32, p0, p1, p2, p3, p4, p5);
            break;
        case PR_sethostname:
            res = syscall_neutral_32(PR_sethostname, (uint32_t) g_2_h(p0), p1, p2, p3, p4, p5);
            break;
        case PR_setitimer:
            res = syscall_neutral_32(PR_setitimer, p0, (uint32_t) g_2_h(p1), (uint32_t) g_2_h(p2), p3, p4, p5);
            break;
        case PR_setpgid:
            res = syscall_neutral_32(PR_setpgid, p0, p1, p2, p3, p4, p5);
            break;
        case PR_setpriority:
            res = syscall_neutral_32(PR_setpriority, p0, p1, p2, p3, p4, p5);
            break;
        case PR_setresgid32:
            res = syscall_neutral_32(PR_setresgid32, p0, p1, p2, p3, p4, p5);
            break;
        case PR_setresuid32:
            res = syscall_neutral_32(PR_setresuid32, p0, p1, p2, p3, p4, p5);
            break;
        case PR_setreuid:
            res = syscall_neutral_32(PR_setreuid, (uid_t) (int16_t)p0, (uid_t) (int16_t)p1, p2, p3, p4, p5);
            break;
        case PR_setreuid32:
            res = syscall_neutral_32(PR_setreuid32, p0, p1, p2, p3, p4, p5);
            break;
        case PR_setregid:
            res = syscall_neutral_32(PR_setregid, (gid_t) (int16_t)p0, (gid_t) (int16_t)p1, p2, p3, p4, p5);
            break;
        case PR_setregid32:
            res = syscall_neutral_32(PR_setregid32, p0, p1, p2, p3, p4, p5);
            break;
        case PR_setrlimit:
            res = syscall_neutral_32(PR_setrlimit, p0, (uint32_t) g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_setsid:
            res = syscall_neutral_32(PR_setsid, p0, p1, p2, p3, p4, p5);
            break;
        case PR_setsockopt:
            res = syscall_neutral_32(PR_setsockopt, p0, p1, p2, (uint32_t) g_2_h(p3), p4, p5);
            break;
        case PR_setuid:
            res = syscall_neutral_32(PR_setuid, (uid_t) (int16_t)p0, p1, p2, p3, p4, p5);
            break;
        case PR_setuid32:
            res = syscall_neutral_32(PR_setuid32, p0, p1, p2, p3, p4, p5);
            break;
        case PR_setxattr:
            res = syscall_neutral_32(PR_setxattr, (uint32_t) g_2_h(p0), (uint32_t) g_2_h(p1), (uint32_t) g_2_h(p2), p3, p4, p5);
            break;
        case PR_shmctl:
            res = syscall_neutral_32(PR_shmctl, p0, p1, IS_NULL(p2), p3, p4, p5);
            break;
        case PR_shmget:
            res = syscall_neutral_32(PR_shmget, p0, p1, p2, p3, p4, p5);
            break;
        case PR_shutdown:
            res = syscall_neutral_32(PR_shutdown, p0, p1, p2, p3, p4, p5);
            break;
        case PR_signalfd4:
            res = syscall_neutral_32(PR_signalfd4, p0, (uint32_t) g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_socket:
            res = syscall_neutral_32(PR_socket, p0, p1, p2, p3, p4, p5);
            break;
        case PR_socketpair:
            res = syscall_neutral_32(PR_socketpair, p0, p1, p2, (uint32_t) g_2_h(p3), p4, p5);
            break;
        case PR_splice:
            res = syscall_neutral_32(PR_splice, p0, IS_NULL(p1), p2, IS_NULL(p3), p4, p5);
            break;
        case PR_stat64:
            res = syscall_neutral_32(PR_stat64, (uint32_t) g_2_h(p0), (uint32_t)g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_statfs:
            res = syscall_neutral_32(PR_statfs, (uint32_t)g_2_h(p0), (uint32_t) g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_statfs64:
            res = syscall_neutral_32(PR_statfs64, (uint32_t) g_2_h(p0), p1, (uint32_t) g_2_h(p2), p3, p4, p5);
            break;
        case PR_symlink:
            res = syscall_neutral_32(PR_symlink, (uint32_t) g_2_h(p0), (uint32_t) g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_symlinkat:
            res = syscall_neutral_32(PR_symlinkat, (uint32_t) g_2_h(p0), p1, (uint32_t) g_2_h(p2), p3, p4, p5);
            break;
        case PR_sync:
            res = syscall_neutral_32(PR_sync, p0, p1, p2, p3, p4, p5);
            break;
        case PR_sync_file_range:
            res = syscall_neutral_32(PR_sync_file_range, p0, p1, p2, p3, p4, p5);
            break;
        case PR_sysinfo:
            res = syscall_neutral_32(PR_sysinfo, (uint32_t) g_2_h(p0), p1, p2, p3 ,p4, p5);
            break;
        case PR_tee:
            res = syscall_neutral_32(PR_tee, p0, p1, p2, p3, p4, p5);
            break;
        case PR_timer_create:
            res = syscall_neutral_32(PR_timer_create, p0, IS_NULL(p1), IS_NULL(p2), p3, p4, p5);
            break;
        case PR_timer_delete:
            res = syscall_neutral_32(PR_timer_delete, p0, p1, p2, p3 ,p4, p5);
            break;
        case PR_timer_getoverrun:
            res = syscall_neutral_32(PR_timer_getoverrun, p0, p1, p2, p3 ,p4, p5);
            break;
        case PR_timer_gettime:
            res = syscall_neutral_32(PR_timer_gettime, p0, (uint32_t) g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_timer_settime:
            res = syscall_neutral_32(PR_timer_settime, p0, p1, (uint32_t) g_2_h(p2), IS_NULL(p3), p4, p5);
            break;
        case PR_timerfd_create:
            res = syscall_neutral_32(PR_timerfd_create, p0, p1, p2, p3 ,p4, p5);
            break;
        case PR_timerfd_gettime:
            res = syscall_neutral_32(PR_timerfd_gettime, p0, (uint32_t) g_2_h(p1), p2, p3 ,p4, p5);
            break;
        case PR_timerfd_settime:
            res = syscall_neutral_32(PR_timerfd_settime, p0, p1, (uint32_t) g_2_h(p2), IS_NULL(p3), p4, p5);
            break;
        case PR_times:
            res = syscall_neutral_32(PR_times, (uint32_t) g_2_h(p0), p1, p2, p3 ,p4, p5);
            break;
        case PR_tkill:
            res = syscall_neutral_32(PR_tkill, p0, p1, p2, p3 ,p4, p5);
            break;
        case PR_truncate64:
            res = syscall_neutral_32(PR_truncate64, (uint32_t) g_2_h(p0), p1, p2, p3 ,p4, p5);
            break;
        case PR_umask:
            res = syscall_neutral_32(PR_umask, p0, p1, p2, p3 ,p4, p5);
            break;
        case PR_uname:
            res = syscall_neutral_32(PR_uname, (uint32_t) g_2_h(p0), p1, p2, p3 ,p4, p5);
            break;
        case PR_unlink:
            res = syscall_neutral_32(PR_unlink, (uint32_t) g_2_h(p0), p1, p2, p3 ,p4, p5);
            break;
        case PR_unlinkat:
            res = syscall_neutral_32(PR_unlinkat, p0, (uint32_t) g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_utimensat:
            res = syscall_neutral_32(PR_utimensat, p0, IS_NULL(p1), IS_NULL(p2), p3, p4, p5);
            break;
        case PR_utimes:
            res = syscall_neutral_32(PR_utimes, (uint32_t) g_2_h(p0), IS_NULL(p1), p2, p3, p4, p5);
            break;
        case PR_vfork:
            res = syscall_neutral_32(PR_vfork, p0, p1, p2, p3, p4, p5);
            break;
        case PR_vhangup:
            res = syscall_neutral_32(PR_vhangup, p0, p1, p2, p3, p4, p5);
            break;
        case PR_vmsplice:
            res = vmsplice_neutral(p0, p1, p2 , p3);
            break;
        case PR_waitid:
            res = syscall_neutral_32(PR_waitid, p0, p1, IS_NULL(p2), p3, IS_NULL(p4), p5);
            break;
        case PR_write:
            res = syscall_neutral_32(PR_write, p0, (uint32_t)g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_writev:
            res = writev_neutral(p0, p1, p2);
            break;
        case PR_getrandom:
            res = syscall_neutral_32(PR_getrandom, (uint32_t) g_2_h(p0), (size_t) p1, (unsigned int) p2, p3, p4, p5);
            break;
        default:
            fatal("syscall_adapter_guest32: unsupported neutral syscall %d\n", no);
    }

    return res;
}
