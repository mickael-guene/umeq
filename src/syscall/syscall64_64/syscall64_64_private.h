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

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __SYSCALL64_64_PRIVATE__
#define __SYSCALL64_64_PRIVATE__ 1

#include "target64.h"

extern long fcntl_s6464(uint64_t fd_p, uint64_t cmd_p, uint64_t opt_p);
extern long execve_s6464(uint64_t filename_p, uint64_t argv_p, uint64_t envp_p);
extern long futex_s6464(uint64_t uaddr_p, uint64_t op_p, uint64_t val_p, uint64_t timeout_p, uint64_t uaddr2_p, uint64_t val3_p);
extern long pselect6_s6464(uint64_t nfds_p, uint64_t readfds_p, uint64_t writefds_p, uint64_t exceptfds_p, uint64_t timeout_p, uint64_t data_p);
extern long semctl_s6464(uint64_t semid_p, uint64_t semnum_p, uint64_t cmd_p, uint64_t arg_p);
extern long epoll_ctl_s6464(uint64_t epfd_p, uint64_t op_p, uint64_t fd_p, uint64_t event_p);
extern long epoll_pwait_s6464(uint64_t epfd_p, uint64_t events_p, uint64_t maxevents_p, uint64_t timeout_p, uint64_t sigmask_p, uint64_t sigmask_size_p);

#endif

#ifdef __cplusplus
}
#endif
