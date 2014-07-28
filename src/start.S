.GLOBAL _start

_start:
    xorl %ebp, %ebp
    mov  %rsp, %rdi
    call main_wrapper
