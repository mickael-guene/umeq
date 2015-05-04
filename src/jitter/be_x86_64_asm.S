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

.GLOBAL execute_be_x86_64
.GLOBAL restore_be_x86_64

/*  This will execute a jit sequence.
 *  rdi : contains backend structure pointer. We will save sp on struct inter.
 *        this allow to return to execute_be_x86_64 caller by calling restore_be_x86_64.
 *  rsi : contain jit code to execute.
 *  rdx : contain backend client context. Will be first and only parameter
 *        given to jit code to execute.
 */
execute_be_x86_64:
	push   %rbp
	push   %rbx
	push   %r12
	push   %r13
	push   %r14
	push   %r15
	mov    %rsp, -8(%rdi)
	mov    %rsi,%rax
	mov	   %rdx,%rdi
	callq  *%rax
	pop    %r15
	pop    %r14
	pop    %r13
	pop    %r12
	pop    %rbx
	pop    %rbp
	retq

/*  Calling this function allow to return to execute_be_x86_64 caller transparently.
 *  Typical usage of this function is to output from a signal handler with a modify
 *  guest pc.
 *  rdi : contains backend structure pointer. we will use sp that was store during
 *        execute_be_x86_64 call.
 *  rsi : contain next guest pc to execute
 */
restore_be_x86_64:
	mov    -8(%rdi), %sp
	pop    %r15
	pop    %r14
	pop    %r13
	pop    %r12
	pop    %rbx
	pop    %rbp
	mov    %rsi,%rax
	retq
