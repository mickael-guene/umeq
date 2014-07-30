#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>

//gcc main.c --static -nostdlib -e main -O0 -m{arm|thumb}
void hello(int fd, const void *msg, size_t msgSize, int syscallNb)
{
    asm volatile(
        "push {r7}\n\t"
        "mov r0, %1\n\t"
        "mov r1, %2\n\t"
        "mov r2, %3\n\t"
        "mov r7, %0\n\t"
        "svc 0\n\t"
        "pop {r7}\n\t"
        :
        : "r" (syscallNb), "r" (fd), "r" (msg), "r" (msgSize)
        : "r0", "r1", "r2"
    );
}
void bye(int status, int syscallNb)
{
    asm volatile(
        "push {r7}\n\t"
        "mov r0, %1\n\t"
        "mov r7, %0\n\t"
        "svc 0\n\t"
        "pop {r7}\n\t"
        :
        : "r" (syscallNb), "r" (status)
        : "r0"
    );
}

void main()
{
    int i;
    for(i = 0; i < 16; i++) {
        hello(1, "Hello World from arm world\n", sizeof("Hello World from arm world\n"), SYS_write);
    }
    bye(0, SYS_exit);
}

