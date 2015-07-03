.GLOBAL memcpy

memcpy:
	movq %rdi, %rax
	movq %rdx, %rcx
	rep movsb
	ret