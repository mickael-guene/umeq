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

.GLOBAL execute_be_i386
.GLOBAL restore_be_i386

execute_be_i386:
	push %ebp
	push %edi
	push %esi
	push %ebx
	mov %esp, %ebp
	mov 24(%ebp), %edi
	mov 20(%ebp), %esi
	mov %ebp, -4(%edi)
	mov %esi, -8(%edi)
	sub $64, %esp
	mov 32(%ebp), %edi
	mov 20(%ebp), %esi
	call *28(%ebp)
	add $64, %esp
	pop %ebx
	pop %esi
	pop %edi
	pop %ebp
	ret $0x4

/*  Calling this function allow to return to execute_be_i386 caller transparently.
 *  Typical usage of this function is to output from a signal handler with a modify
 *  guest pc.
 *  Call it from assembly code with following registers set :
 *  edi : contains backend structure pointer. we will use sp that was store during
 *        execute_be_i386 call.
 *  esi : contain next guest pc to execute
 */
restore_be_i386:
	mov -4(%edi), %esp
	mov -8(%edi), %edi
	mov %esi, 0(%edi)
	xor %esi,%esi
	mov %esi, 4(%edi)
	mov %esi, 8(%edi)
	pop %ebx
	pop %esi
	pop %edi
	pop %ebp
	ret $0x4
