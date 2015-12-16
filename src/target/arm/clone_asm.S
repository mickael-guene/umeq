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

/* FIXME : implement i386.S */
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

.GLOBAL clone_exit_asm

/* FIXME : implement i386.S */
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
