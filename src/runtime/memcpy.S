.GLOBAL memcpy

#ifndef __i386__
memcpy:
	movq %rdi, %rax
	movq %rdx, %rcx
	rep movsb
	ret
#endif