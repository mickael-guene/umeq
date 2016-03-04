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
#include <sys/ptrace.h>
#include <asm/ldt.h>
#include <sys/user.h>
#include <assert.h>
#include <errno.h>

#define PROOT_MAGIC_NB          350

#define MAGIC1  0x7a711e06171129a2UL
#define MAGIC2  0xc6925d1ed6dcd674UL

void ptrace_event_proot(int event)
{
    syscall((long) PROOT_MAGIC_NB, event);
}

void ptrace_event_chroot(int event)
{
    syscall(SYS_gettid, MAGIC1, MAGIC2, event);
}

long get_regs_base(int pid, unsigned long *regs_base) {
    struct user_regs_struct user_regs;
    struct user_desc desc;
    int res;

    res = syscall(SYS_ptrace, PTRACE_GETREGS, pid, 0, &user_regs);
    if (!res) {
        desc.entry_number = user_regs.xgs >> 3;
        res = syscall(SYS_ptrace, 25/*PTRACE_GET_THREAD_AREA*/, pid, user_regs.xgs >> 3, &desc);
        if (!res) {
            res = syscall(SYS_ptrace, PTRACE_PEEKTEXT, pid, desc.base_addr + 4, regs_base);
        }
    }

    return res;
}

int is_magic_syscall(pid_t pid, int *syscall_no, int *command, int *is_entry)
{
    long res;
    struct user_regs_struct regs;
    int is_magic;

    res = syscall(SYS_ptrace, PTRACE_GETREGS, pid, NULL, &regs);
    assert(res == 0);
    *syscall_no = regs.orig_eax;
    *command = regs.edx;
    *is_entry = regs.eax==-ENOSYS?1:0;
    is_magic = regs.ebx== MAGIC1 && regs.ecx == MAGIC2;

    return is_magic;
}
