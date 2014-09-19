#define _GNU_SOURCE        /* or _BSD_SOURCE or _SVID_SOURCE */
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <stdint.h>

uint16_t htons(uint16_t hostshort)
{
    return ((hostshort >> 8) & 0xff) | ((hostshort << 8) & 0xff00);
}

ssize_t send(int sockfd, const void *buf, size_t len, int flags)
{
    int res = syscall((long) SYS_sendto, (long) sockfd, (long) buf, (long) len, (long) flags, (long) NULL, (long) 0);
    if (res < 0) {
        errno = -res;
        res = -1;
    }
    
    return res;
}

int socket(int domain, int type, int protocol)
{
    int res = syscall((long) SYS_socket, (long) domain, (long) type, (long) protocol);
    if (res < 0) {
        errno = -res;
        res = -1;
    }
    
    return res;
}

int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    int res = syscall((long) SYS_bind, (long) sockfd, (long) addr, (long) addrlen);
    if (res < 0) {
        errno = -res;
        res = -1;
    }
    
    return res;
}

int listen(int sockfd, int backlog)
{
    int res = syscall((long) SYS_listen, (long) sockfd, (long) backlog);
    if (res < 0) {
        errno = -res;
        res = -1;
    }
    
    return res;
}

int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
    int res = syscall((long) SYS_accept, (long) sockfd, (long) addr, (long) addrlen);
    if (res < 0) {
        errno = -res;
        res = -1;
    }
    
    return res;
}

int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen)
{
    int res = syscall((long) SYS_setsockopt, (long) sockfd, (long) level, (long) optname, (long) optval, (long) optlen);
    if (res < 0) {
        errno = -res;
        res = -1;
    }

    return res;
}
