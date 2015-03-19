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

#ifndef __SYSCALL32_64_PRIVATE__
#define __SYSCALL32_64_PRIVATE__ 1

#include "target32.h"

extern int stat64_s3264(uint32_t pathname_p, uint32_t buf_p);
extern int fstat64_s3264(uint32_t fd, uint32_t buf);
extern int fstat64_s3264(uint32_t fd_p, uint32_t buf_p);
extern int getdents64_s3264(uint32_t fd, uint32_t dirp, uint32_t count);
extern int llseek_s3264(uint32_t fd_p, uint32_t offset_high_p, uint32_t offset_low_p, uint32_t result_p, uint32_t whence_p);
extern int clock_gettime_s3264(uint32_t clk_id_p, uint32_t tp_p);
extern int ugetrlimit_s3264(uint32_t resource_p, uint32_t rlim_p);
extern int futex_s3264(uint32_t uaddr_p, uint32_t op_p, uint32_t val_p, uint32_t timeout_p, uint32_t uaddr2_p, uint32_t val3_p);
extern int statfs64_s3264(uint32_t path_p, uint32_t dummy_p, uint32_t buf_p);
extern int fnctl64_s3264(uint32_t fd_p, uint32_t cmd_p, uint32_t opt_p);
extern int lstat64_s3264(uint32_t pathname_p, uint32_t buf_p);
extern int gettimeofday_s3264(uint32_t tv_p, uint32_t tz_p);
extern int wait4_s3264(uint32_t pid_p,uint32_t status_p,uint32_t options_p,uint32_t rusage_p);
extern int execve_s3264(uint32_t filename_p,uint32_t argv_p,uint32_t envp_p);
extern int prlimit64_s3264(uint32_t pid_p, uint32_t resource_p, uint32_t new_limit_p, uint32_t old_limit_p);
extern int writev_s3264(uint32_t fd_p, uint32_t iov_p, uint32_t iovcnt_p);
extern int getrusage_s3264(uint32_t who_p, uint32_t usage_p);
extern int fstatat64_s3264(uint32_t dirfd_p, uint32_t pathname_p, uint32_t buf_p, uint32_t flags_p);
extern int newselect_s3264(uint32_t nfds_p, uint32_t readfds_p, uint32_t writefds_p, uint32_t exceptfds_p, uint32_t timeout_p);
extern int recvmsg_s3264(uint32_t sockfd_p, uint32_t msg_p, uint32_t flags_p);
extern int utimes_s3264(uint32_t filename_p, uint32_t times_p);
extern int statfs_s3264(uint32_t path_p, uint32_t buf_p);
extern int pselect6_s3264(uint32_t nfds_p, uint32_t readfds_p, uint32_t writefds_p, uint32_t exceptfds_p, uint32_t timeout_p, uint32_t data_p);
extern int utimensat_s3264(uint32_t dirfd_p, uint32_t pathname_p, uint32_t times_p, uint32_t flags_p);
extern int nanosleep_s3264(uint32_t req_p, uint32_t rem_p);
extern int sysinfo_s3264(uint32_t info_p);
extern int clock_getres_s3264(uint32_t clk_id_p, uint32_t res_p);
extern int getdents_s3264(uint32_t fd_p, uint32_t dirp_p, uint32_t count_p);
extern int setitimer_s3264(uint32_t which_p, uint32_t new_value_p, uint32_t old_value_p);
extern int timer_create_s3264(uint32_t clockid_p, uint32_t sevp_p, uint32_t timerid_p);
extern int fstatfs64_s3264(uint32_t fd_p, uint32_t dummy_p, uint32_t buf_p);
extern int timer_settime_s3264(uint32_t timerid_p, uint32_t flags_p, uint32_t new_value_p, uint32_t old_value_p);
extern int fstatfs_s3264(uint32_t fd_p, uint32_t buf_p);
extern int sendmsg_s3264(uint32_t sockfd_p, uint32_t msg_p, uint32_t flags_p);
extern int rt_sigtimedwait_s3264(uint32_t set_p, uint32_t info_p, uint32_t timeout_p);
extern int shmat_s3264(uint32_t shmid_p, uint32_t shmaddr_p, uint32_t shmflg_p);
extern int shmctl_s3264(uint32_t shmid_p, uint32_t cmd_p, uint32_t buf_p);
extern int semctl_s3264(uint32_t semid_p, uint32_t semnum_p, uint32_t cmd_p, uint32_t arg0_p, uint32_t arg1_p, uint32_t arg2_p);
extern int semop_s3264(uint32_t semid_p, uint32_t sops_p, uint32_t nsops_p);
extern int clock_nanosleep_s3264(uint32_t clock_id_p, uint32_t flags_p, uint32_t request_p, uint32_t remain_p);
extern int times_s3264(uint32_t buf_p);
extern int epoll_ctl_s3264(uint32_t epfd_p, uint32_t op_p, uint32_t fd_p, uint32_t event_p);
extern int epoll_wait_s3264(uint32_t epfd_p, uint32_t events_p, uint32_t maxevents_p, uint32_t timeout_p);

#endif

#ifdef __cplusplus
}
#endif
