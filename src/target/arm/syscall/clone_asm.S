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

.GLOBAL clone_asm
.GLOBAL clone_exit_asm

#ifdef __i386__
clone_asm:
    push %ebp
    mov %esp, %ebp
    push %ebx
    push %esi
    push %edi
    mov 0x08(%ebp), %eax
    mov 0x0c(%ebp), %ebx
    mov 0x10(%ebp), %ecx
    mov 0x14(%ebp), %edx
    mov 0x18(%ebp), %esi
    mov 0x1c(%ebp), %edi
    mov 0x20(%ebp), %ebp
    int $0x80
    test %eax,%eax
    jz clone_thread_trampoline_arm
    pop %edi
    pop %esi
    pop %ebx
    pop %ebp
    ret

clone_exit_asm:
    mov 4(%esp), %ebx
    mov 8(%esp), %ecx
    mov 12(%esp), %esi
    mov 16(%esp), %edi
    #here destroy stack
    mov $91, %eax
    int $0x80
    #here patch
    movb $0, (%edi)
    mov %esi, %ebx
    mov $1, %eax
    int $0x80
#else
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
#endif
