#include <sys/syscall.h>
#include <signal.h>
#include <sys/utsname.h>
#include <assert.h>
#include <errno.h>
#include <string.h>

#include "arm_syscall.h"

int arm_uname(struct arm_target *context)
{
    int res;
    struct utsname *buf = (struct utsname *)(long) context->regs.r[0];

    res = syscall(SYS_uname, (long) buf);
    strcpy(buf->machine, "armv7l");

    return res;
}
