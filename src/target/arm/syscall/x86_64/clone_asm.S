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

.global clone_asm
.global clone_exit_asm

clone_asm:
    mov    %rdi,%rax
    mov    %rsi,%rdi
    mov    %rdx,%rsi
    mov    %rcx,%rdx
    mov    %r8,%r10
    mov    %r9,%r8
    mov    0x8(%rsp),%r9
    syscall
    testq  %rax,%rax
    jz     clone_thread_trampoline_arm
    retq

clone_exit_asm:
    mov %rdx, %r15
    mov %rcx, %r14
    mov $11, %rax
    syscall
    movb $0, (%r14)
    mov %r15,%rdi
    mov $60, %rax
    syscall
