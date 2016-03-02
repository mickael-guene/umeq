/* This file is part of Umeq, an equivalent of qemu user mode emulation with improved robustness.
 *
 * Copyright (C) 2015 STMicroelectronics
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

#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/ptrace.h>

#include "arm_private.h"
#include "arm_syscall.h"
#include "runtime.h"
#include "umeq.h"

extern void *auxv_start;
extern void *auxv_end;


/* FIXME: cleanup and try to merge to 32 => 64 syscall stuff */

/* FIXME :
    - also check pathname is equal to /proc/{tid}/auxv
    - really create unique tmp file
 */

extern void *auxv_start;
extern void *auxv_end;

guest_ptr *arm_env_startup_pointer;

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
    static uint32_t cnt = 0;
    char tmpName[1024];
    int fd;

    sprintf(tmpName, "/tmp/umeq-open-proc-self-auxv.%d-%d", gettid(), cnt++);
    fd = open(tmpName, O_RDWR | O_CREAT, S_IRUSR|S_IWUSR);

    if (fd >= 0) {
        ssize_t res;

        unlink(tmpName);
        res = write(fd, auxv_start, auxv_end - auxv_start);
        assert(res == auxv_end - auxv_start);
        lseek(fd, 0, SEEK_SET);
    }

    return fd;
}

static long remote_read(int pid, void *from, void *to, char *buffer)
{
    long res = 0;
    long data;
    int i;
    int j;

    assert((to - from) % sizeof(void *) == 0);
    for(i = 0; i < (to - from) / sizeof(void *); i++) {
        res = syscall(SYS_ptrace, PTRACE_PEEKDATA, pid, from + i * sizeof(void *), &data);
        if (res)
            goto error;
        for(j = 0; j < 8; j++)
            *buffer++ = (data >> (j * 8)) & 0xff;
    }

error:
    return res;
}

/*  This code will only work when a tracer try to read a tracee
 * auxv. This allow gdb to be able to debug pie binaries.
 */
static int open_proc_pid_auxv(const char *pathname)
{
    int fd = -EACCES;
    int pid = 0;
    long res;
    int i;
    void *auxv_start_value;
    void *auxv_end_value;
    char *auxv_buffer;
    char tmpName[128];
    static uint32_t cnt = 0;

    /* compute pid */
    for(i = 6; i < strlen(pathname) - 5; i++)
        pid = pid * 10 + (pathname[i] - '0');

    /* read auxv_start and auxv_end value and remote read auxv data in buffer */
    res = syscall(SYS_ptrace, PTRACE_PEEKDATA, pid, &auxv_start, &auxv_start_value);
    if (res)
        goto error;
    res = syscall(SYS_ptrace, PTRACE_PEEKDATA, pid, &auxv_end, &auxv_end_value);
    if (res)
        goto error;
    auxv_buffer = alloca(auxv_end_value - auxv_start_value);
    res = remote_read(pid, auxv_start_value, auxv_end_value, auxv_buffer);
    if (res)
        goto error;

    /* now generate file */
    sprintf(tmpName, "/tmp/umeq-open-proc-%d-auxv.%d-%d", pid, gettid(), cnt++);
    fd = open(tmpName, O_RDWR | O_CREAT, S_IRUSR|S_IWUSR);

    if (fd >= 0) {
        ssize_t res;

        unlink(tmpName);
        res = write(fd, auxv_buffer, auxv_end_value - auxv_start_value);
        assert(res == auxv_end_value - auxv_start_value);
        lseek(fd, 0, SEEK_SET);
    }

error:
    return fd;
}

static int getline_internal(char *line, int max_length, int fd)
{
    char c;
    int i = 0;

    while(i<max_length - 1 && read(fd, &c, 1) > 0) {
        line[i++] = c;
        if (c == '\n')
            break;
    }
    line[i] = '\0';

    return i==0?-1:i;
}

int conv_hex_to_int(int c)
{
    int res;

    switch(c) {
        case 48 ... 57:
            res = c - 48;
            break;
        case 97 ... 102:
            res = c - 97 + 10;
            break;
        case 65 ... 70:
            res = c - 65 + 10;
            break;
        default:
            fatal("invalid char %c\n", c);
    }

    return res;
}

static char *split_line(char *line, uint64_t *start_addr_p, uint64_t *end_addr_p)
{
    uint64_t start_addr = 0;
    uint64_t end_addr = 0;
    unsigned char c;
    int i = 0;
    int len = strlen(line);
    int is_in_start_addr = 1;
    char *res = NULL;

    while(i < len) {
        c = line[i++];
        if (is_in_start_addr) {
            if (c == '-') {
                is_in_start_addr = 0;
            } else {
                start_addr = start_addr * 16 + conv_hex_to_int(c);
            }
        } else {
            if (c == ' ') {
                res = &line[i];
                break;
            } else {
                end_addr = end_addr * 16 + conv_hex_to_int(c);
            }
        }
    }

    *start_addr_p = start_addr;
    *end_addr_p = end_addr;

    return res;
}

static void write_offset_proc_self_maps(int fd_orig, int fd_res)
{
    char line[512];
    char *line_with_remove_addresses;
    uint64_t start_addr;
    uint64_t end_addr;

    while (getline_internal(line, sizeof(line), fd_orig) != -1) {
        line_with_remove_addresses = split_line(line, &start_addr, &end_addr);
        if ((start_addr >= mmap_offset) && (start_addr < mmap_offset + 4 * 1024 * 1024 * 1024ULL)) {
            char new_line[512];
            ssize_t res;

            guest_ptr start_addr_guest = h_2_g(start_addr);
            guest_ptr end_addr_guest = h_2_g(end_addr);

            sprintf(new_line, "%08x-%08x %s", start_addr_guest, end_addr_guest, line_with_remove_addresses);
            res = write(fd_res, new_line, strlen(new_line));
            assert(res == strlen(new_line));
        }
    }
    lseek(fd_res, 0, SEEK_SET);
}

static int open_proc_self_maps()
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
        close(fd_proc_self_maps);
    } else
        fd_res = fd_proc_self_maps;

    return fd_res;
}

static void write_guest_proc_self_environ(int fd_res)
{
    guest_ptr *current = arm_env_startup_pointer;

    /* write guest environment variable */
    while(*current) {
        char *var = g_2_h(*current);
        int len = strlen(var);

        /* remove empty environment variable: see https://github.com/proot-me/PRoot/issues/90 */
        if (len)
            write(fd_res, var, len + 1);
        current++;
    }
    /* write termination string */
    write(fd_res, "", 1);
    lseek(fd_res, 0, SEEK_SET);
}

static int open_proc_self_environ()
{
    static uint32_t cnt = 0;
    char tmpName[1024];
    int fd_res;
    int fd_proc_self_environ;

    fd_proc_self_environ = open("/proc/self/environ", O_RDONLY);
    if (fd_proc_self_environ >= 0) {
        sprintf(tmpName, "/tmp/umeq-open-proc-self-environ.%d-%d", gettid(), cnt++);
        fd_res = open(tmpName, O_RDWR | O_CREAT, S_IRUSR|S_IWUSR);
        if (fd_res >= 0) {
            unlink(tmpName);
            write_guest_proc_self_environ(fd_res);
        }
        close(fd_proc_self_environ);
    } else
        fd_res = fd_proc_self_environ;

    return fd_res;
}

int arm_open(struct arm_target *context)
{
    int res;
    char *pathname = (char *)(long) g_2_h(context->regs.r[0]);
    long flags = armToX86Flags(context->regs.r[1]);
    int mode = context->regs.r[2];

    if (strcmp(pathname, "/proc/self/auxv") == 0)
        res = open_proc_self_auxv();
    else if (strcmp(pathname, "/proc/self/maps") == 0)
        res = open_proc_self_maps();
    else if (is_under_proot && strcmp(pathname, "/proc/self/environ") == 0)
        res = open_proc_self_environ();
    else if (strncmp(pathname, "/proc/", strlen("/proc/")) == 0 && strncmp(pathname + strlen(pathname) - 5, "/auxv", strlen("/auxv")) == 0)
        res = open_proc_pid_auxv(pathname);
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
