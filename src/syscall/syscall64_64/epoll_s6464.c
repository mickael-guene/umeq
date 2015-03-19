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
#include <sys/epoll.h>

#include "syscall64_64_private.h"

/* x86 epoll_event is packed. So we need to convert for others arch */

struct epoll_event_64 {
    uint32_t events;
    uint32_t __pad0;
    uint64_t data;
};

long epoll_ctl_s6464(uint64_t epfd_p, uint64_t op_p, uint64_t fd_p, uint64_t event_p)
{
    long res;
    int epfd = (int) epfd_p;
    int op = (int) op_p;
    int fd = (int) fd_p;
    struct epoll_event_64 *event_guest = (struct epoll_event_64 *) g_2_h(event_p);
    struct epoll_event event;

    if (event_p) {
        event.events = event_guest->events;
        event.data.u64 = event_guest->data;
    }
    res = syscall(SYS_epoll_ctl, epfd, op, fd, event_p?&event:NULL);

    return res;
}

long epoll_pwait_s6464(uint64_t epfd_p, uint64_t events_p, uint64_t maxevents_p, uint64_t timeout_p, uint64_t sigmask_p, uint64_t sigmask_size_p)
{
    long res;
    int epfd = (int) epfd_p;
    struct epoll_event_64 *events_guest = (struct epoll_event_64 *) g_2_h(events_p);
    int maxevents = (int) maxevents_p;
    int timeout = (int) timeout_p;
    sigset_t *sigmask = (sigset_t *) g_2_h(sigmask_p);
    int sigmask_size = (int) sigmask_size_p;
    struct epoll_event *events = (struct epoll_event *) alloca(maxevents * sizeof(struct epoll_event));
    int i;

    res = syscall(SYS_epoll_pwait, epfd, events, maxevents, timeout, sigmask_p?sigmask:NULL, sigmask_size);
    for(i = 0; i < maxevents; i++) {
        events_guest[i].events = events[i].events;
        events_guest[i].data = events[i].data.u64;
    }

    return res;
}
