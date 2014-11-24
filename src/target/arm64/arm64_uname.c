#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <signal.h>
#include <sys/utsname.h>
#include <assert.h>
#include <errno.h>
#include <string.h>

#include "arm64_syscall.h"

long arm64_uname(struct arm64_target *context)
{
    long res;
    struct utsname *buf = (struct utsname *) g_2_h(context->regs.r[0]);

    res = syscall(SYS_uname, buf);
    if (res == 0)
        strcpy(buf->machine, "aarch64");

    return res;
}
