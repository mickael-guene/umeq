#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "arm64_private.h"
#include "arm64_syscall.h"

#include <stdio.h>
/* FIXME: /proc ?? */
long arm64_openat(struct arm64_target *context)
{
    long res;
    int dirfd = (int) context->regs.r[0];
    const char *pathname = (const char *) g_2_h_64(context->regs.r[1]);
    long flags = (long) context->regs.r[2];
    int mode = context->regs.r[3];

    fprintf(stderr, "%s\n", pathname);
    res = syscall(SYS_openat, dirfd, pathname, flags, mode);

    return res;
}
