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

#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "adapters_private.h"

#define BINPRM_BUF_SIZE 128

static char *strchr_local(char *s, int c)
{
    do {
        if (*s == c)
            return s;
    } while(*s++ != '\0');

    return NULL;
}

long parse_shebang(const char *filename, char **i_name, char **i_arg)
{
    int fd;
    char buf[BINPRM_BUF_SIZE];
    int ret;
    char *cp;

    assert(i_name);
    assert(i_arg);
    *i_name = NULL;
    *i_arg = NULL;
    /* read start of filename to test for shebang */
    fd = open(filename, O_RDONLY);
    if (fd == -1)
        return -ENOENT;
    ret = read(fd, buf, BINPRM_BUF_SIZE);
    if (ret < 0) {
        close(fd);
        return -ENOENT;
    }
    close(fd);

    /* shebang handling */
    if ((buf[0] == '#') && (buf[1] == '!')) {
        buf[BINPRM_BUF_SIZE - 1] = '\0';
        if ((cp = strchr_local(buf, '\n')) == NULL)
            cp = buf+BINPRM_BUF_SIZE-1;
        *cp = '\0';
        while (cp > buf) {
            cp--;
            if ((*cp == ' ') || (*cp == '\t'))
                *cp = '\0';
            else
                break;
        }
        for (cp = buf+2; (*cp == ' ') || (*cp == '\t'); cp++);
        if (*cp == '\0')
            return -ENOEXEC; /* No interpreter name found */
        *i_name = cp;
        for ( ; *cp && (*cp != ' ') && (*cp != '\t'); cp++)
            /* nothing */ ;
        while ((*cp == ' ') || (*cp == '\t'))
            *cp++ = '\0';
        if (*cp)
            *i_arg = cp;
    }

    return 0;
}
