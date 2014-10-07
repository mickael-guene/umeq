#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <assert.h>

#include "syscall32_64_types.h"
#include "syscall32_64_private.h"

int recvmsg_s3264(uint32_t sockfd_p, uint32_t msg_p, uint32_t flags_p)
{
	int res;
	int sockfd = (int) sockfd_p;
	struct msghdr_32 *msg_guest = (struct msghdr_32 *) g_2_h(msg_p);
	int flags = (int) flags_p;
	struct iovec iovec[16]; //max 16 iovect. If more is need then use mmap or alloca
    struct msghdr msg;
    int i;

    assert(msg_guest->msg_iovlen <= 16);

    msg.msg_name = msg_guest->msg_name?(void *) g_2_h(msg_guest->msg_name):NULL;
    msg.msg_namelen = msg_guest->msg_namelen;
    for(i = 0; i < msg_guest->msg_iovlen; i++) {
        struct iovec_32 *iovec_guest = (struct iovec_32 *) g_2_h(msg_guest->msg_iov + sizeof(struct iovec_32) * i);

        iovec[i].iov_base = (void *) g_2_h(iovec_guest->iov_base);
        iovec[i].iov_len = iovec_guest->iov_len;
    }
    msg.msg_iov = iovec;
    msg.msg_iovlen = msg_guest->msg_iovlen;
    msg.msg_control = (void *) g_2_h(msg_guest->msg_control);
    msg.msg_controllen = msg_guest->msg_controllen;
    msg.msg_flags = msg_guest->msg_flags;

    res = syscall(SYS_recvmsg, sockfd, &msg, flags);

    return res;
}

int sendmsg_s3264(uint32_t sockfd_p, uint32_t msg_p, uint32_t flags_p)
{
    int res;
    int sockfd = (int) sockfd_p;
    struct msghdr_32 *msg_guest = (struct msghdr_32 *) g_2_h(msg_p);
    int flags = (int) flags_p;
    struct iovec iovec[16]; //max 16 iovect. If more is need then use mmap or alloca
    struct msghdr msg;
    int i;

    assert(msg_guest->msg_iovlen <= 16);

    msg.msg_name = msg_guest->msg_name?(void *) g_2_h(msg_guest->msg_name):NULL;
    msg.msg_namelen = msg_guest->msg_namelen;
    for(i = 0; i < msg_guest->msg_iovlen; i++) {
        struct iovec_32 *iovec_guest = (struct iovec_32 *) g_2_h(msg_guest->msg_iov + sizeof(struct iovec_32) * i);

        iovec[i].iov_base = (void *) g_2_h(iovec_guest->iov_base);
        iovec[i].iov_len = iovec_guest->iov_len;
    }
    msg.msg_iov = iovec;
    msg.msg_iovlen = msg_guest->msg_iovlen;
    msg.msg_control = (void *) g_2_h(msg_guest->msg_control);
    msg.msg_controllen = msg_guest->msg_controllen;
    msg.msg_flags = msg_guest->msg_flags;

    res = syscall(SYS_sendmsg, sockfd, &msg, flags);

    return res;
}
