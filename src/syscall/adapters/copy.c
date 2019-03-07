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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <alloca.h>

#include "runtime.h"

#define BUFFER_LEN                      1024
#define MIN(a,b)                        ((a) < (b) ? (a) : (b))

static char *strchr_local(char *s, int c)
{
    do {
        if (*s == c)
            return s;
    } while(*s++ != '\0');

    return NULL;
}

long copy_file(const char *filename, const char *path)
{
    int fd;
    int fd_in;
    int len;
    char *target;
    int target_len;
    char *pos;
    int res;
    int current;
    char buffer[BUFFER_LEN];
    int count;

    /* compute target and check if it already exists */
    target_len = strlen(path) + strlen(filename) + 1;
    target = alloca(target_len);
    strcpy(target, path);
    strcpy(target + strlen(path), filename);
    target[target_len - 1] = '\0';
    fd = open(target, O_RDONLY);
    if (fd >= 0) {
        close(fd);
        return 0;
    }

    /* compute filename len */
    fd = open(filename, O_RDONLY);
    if (fd < 0)
        return fd;
    len = lseek(fd, 0, SEEK_END);
    close(fd);
    if (len <=0)
        return len;

    /* build directory hierarchy */
    pos = strchr_local(target + 1, '/');
    while(pos) {
        *pos = '\0';
        mkdir(target, 0777);
        *pos = '/';
        pos = strchr_local(pos + 1, '/');
    }

    /* copy file */
    fd_in = open(filename, O_RDONLY);
    if (fd_in < 0)
        return fd_in;
    fd = open(target, O_WRONLY | O_CREAT, 0777);
    if (fd < 0)
        return fd;
    current = len;
    while(current) {
        count = MIN(current, BUFFER_LEN);
        res = read(fd_in, buffer, count);
        assert(res == count);
        res = write(fd, buffer, count);
        current -= count;
    }
    close(fd);
    close(fd_in);

    return 0;
}
