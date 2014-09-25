#define _GNU_SOURCE
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "arm_private.h"
#include "arm_syscall.h"

/* FIXME: cleanup and try to merge to 32 => 64 syscall stuff */

/* FIXME :
    - also check pathname is equal to /proc/{tid}/auxv
    - really create unique tmp file
 */

extern void *auxv_start;
extern void *auxv_end;

struct convertFlags {
    int armFlag;
    long x86Flag;
};

const static struct convertFlags armToX86FlagsTable[] = {
        {01,O_WRONLY},
        {02,O_RDWR},
        {0100,O_CREAT},
        {0200,O_EXCL},
        {0400,O_NOCTTY},
        {01000,O_TRUNC},
        {02000,O_APPEND},
        {04000,O_NONBLOCK},
        {04010000,O_SYNC},
        {020000,O_ASYNC},
        {040000,O_DIRECTORY},
        {0100000,O_NOFOLLOW},
        {02000000,O_CLOEXEC},
        {0200000,O_DIRECT},
        {01000000,O_NOATIME},
        //{010000000,O_PATH}, //Not on my machine
        {0400000,O_LARGEFILE},
};

long armToX86Flags(int arm_flags)
{
    long res = 0;
    int i;

    for(i=0;i<sizeof(armToX86FlagsTable) / sizeof(struct convertFlags); i++) {
        if (arm_flags & armToX86FlagsTable[i].armFlag)
            res |= armToX86FlagsTable[i].x86Flag;
    }

    return res;
}

int x86ToArmFlags(long x86_flags)
{
    int res = 0;
    int i;

    for(i=0;i<sizeof(armToX86FlagsTable) / sizeof(struct convertFlags); i++) {
        if (x86_flags & armToX86FlagsTable[i].x86Flag)
            res |= armToX86FlagsTable[i].armFlag;
    }

    return res;
}

static pid_t gettid()
{
    return syscall(SYS_gettid);
}

static int open_proc_self_auxv()
{
    char tmpName[1024];
    int fd;

    sprintf(tmpName, "/tmp/umeq-open.%d", gettid());
    fd = open(tmpName, O_CREAT, S_IRUSR|S_IWUSR);

    if (fd >= 0) {
        unlink(tmpName);
        write(fd, auxv_start, auxv_end - auxv_start);
        lseek(fd, 0, SEEK_SET);
    }

    return fd;
}

int arm_open(struct arm_target *context)
{
    int res;
    char *pathname = (char *)(long) g_2_h(context->regs.r[0]);
    long flags = armToX86Flags(context->regs.r[1]);
    int mode = context->regs.r[2];

    if (strcmp(pathname, "/proc/self/auxv") == 0)
        res = open_proc_self_auxv();
    else if (strncmp(pathname, "/proc/", strlen("/proc/")) == 0 && strncmp(pathname + strlen(pathname) - 5, "/auxv", strlen("/auxv")) == 0)
        res = -EACCES;
    else
        res = syscall(SYS_open, (long) pathname, (long) flags, (long) mode);

    return res;
}

int arm_openat(struct arm_target *context)
{
    int res;
    int dirfd = (int) context->regs.r[0];
    const char *pathname = (const char *) g_2_h(context->regs.r[1]);
    long flags = armToX86Flags(context->regs.r[2]);
    int mode = context->regs.r[3];

    res = syscall(SYS_openat, dirfd, pathname, flags, mode);

    return res;
}
