#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include "arm64_private.h"
#include "arm64_syscall.h"

struct convertFlags {
    long arm64Flag;
    long x86Flag;
};

const static struct convertFlags arm64ToX86FlagsTable[] = {
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

long arm64ToX86Flags(long arm64_flags)
{
    long res = 0;
    int i;

    for(i=0;i<sizeof(arm64ToX86FlagsTable) / sizeof(struct convertFlags); i++) {
        if (arm64_flags & arm64ToX86FlagsTable[i].arm64Flag)
            res |= arm64ToX86FlagsTable[i].x86Flag;
    }

    return res;
}

long x86ToArm64Flags(long x86_flags)
{
    int res = 0;
    int i;

    for(i=0;i<sizeof(arm64ToX86FlagsTable) / sizeof(struct convertFlags); i++) {
        if (x86_flags & arm64ToX86FlagsTable[i].x86Flag)
            res |= arm64ToX86FlagsTable[i].arm64Flag;
    }

    return res;
}

static pid_t gettid()
{
    return syscall(SYS_gettid);
}

static long open_proc_self_maps()
{
    static uint32_t cnt = 0;
    char tmpName[1024];
    int fd_res;
    int fd_proc_self_maps;

    fd_proc_self_maps = open("/proc/self/maps", O_RDONLY);
    if (fd_proc_self_maps >= 0) {
        sprintf(tmpName, "/tmp/umeq-open-proc-self-maps.%d-%d", gettid(), cnt++);
        fd_res = open(tmpName, O_RDWR | O_CREAT, S_IRUSR|S_IWUSR);

        if (fd_res >= 0) {
            unlink(tmpName);
            write_offset_proc_self_maps(fd_proc_self_maps, fd_res);
        }
    } else
        fd_res = fd_proc_self_maps;

    return fd_res;
}

/* FIXME: /proc ?? */
long arm64_openat(struct arm64_target *context)
{
    long res;
    int dirfd = (int) context->regs.r[0];
    const char *pathname = (const char *) g_2_h(context->regs.r[1]);
    long flags = (long) arm64ToX86Flags(context->regs.r[2]);
    int mode = context->regs.r[3];

    if (strcmp(pathname, "/proc/self/auxv") == 0) {
        res = -EACCES;
    } else if (strcmp(pathname, "/proc/self/maps") == 0) {
        res = open_proc_self_maps();
    } else if (strncmp(pathname, "/proc/", strlen("/proc/")) == 0 && strncmp(pathname + strlen(pathname) - 5, "/auxv", strlen("/auxv")) == 0) {
        res = -EACCES;
    } else
        res = syscall(SYS_openat, dirfd, pathname, flags, mode);

    return res;
}
