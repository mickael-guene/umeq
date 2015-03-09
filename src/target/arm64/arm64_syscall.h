
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
#include "arm64_private.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ARM64_SYSCALL__
#define __ARM64_SYSCALL__ 1

#include "target64.h"

extern long arm64_uname(struct arm64_target *context);
extern long arm64_brk(struct arm64_target *context);
extern long arm64_openat(struct arm64_target *context);
extern long arm64_fstat(struct arm64_target *context);
extern long arm64_rt_sigaction(struct arm64_target *context);
extern long arm64_fstatat64(struct arm64_target *context);
extern long arm64_clone(struct arm64_target *context);
extern long arm64_sigaltstack(struct arm64_target *context);
extern long arm64_ptrace(struct arm64_target *context);
extern long arm64_mmap(struct arm64_target *context);
extern long arm64_munmap(struct arm64_target *context);
extern long arm64_mremap(struct arm64_target *context);
extern long arm64_shmat(struct arm64_target *context);
extern long arm64_shmdt(struct arm64_target *context);

#endif

#ifdef __cplusplus
}
#endif
