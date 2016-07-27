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

#include "arm_private.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ARM_SYSCALL__
#define __ARM_SYSCALL__ 1

#include "target32.h"

extern int arm_uname(struct arm_target *context);
extern int arm_brk(struct arm_target *context);
extern int arm_open(struct arm_target *context);
extern int arm_openat(struct arm_target *context);
extern int arm_rt_sigaction(struct arm_target *context);
extern int arm_clone(struct arm_target *context);
extern int arm_ptrace(struct arm_target *context);
extern int arm_sigaltstack(struct arm_target *context);
extern int arm_mmap(struct arm_target *context);
extern int arm_mmap2(struct arm_target *context);
extern int arm_munmap(struct arm_target *context);
extern int arm_mremap(struct arm_target *context);
extern int arm_shmat(struct arm_target *context);
extern int arm_shmdt(struct arm_target *context);
extern int arm_process_vm_readv(struct arm_target *context);
extern int arm_rt_sigqueueinfo(struct arm_target *context);
extern int arm_wait4(struct arm_target *context);
extern int arm_fstat64(struct arm_target *context);
extern int arm_stat64(struct arm_target *context);
extern int arm_lstat64(struct arm_target *context);
extern int arm_fstatat64(struct arm_target *context);
extern int arm_fstat(struct arm_target *context);
extern int arm_stat(struct arm_target *context);
extern int arm_readlink(struct arm_target *context);

#endif

#ifdef __cplusplus
}
#endif
