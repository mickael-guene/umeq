
.GLOBAL clone_asm

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
    jz     clone_thread_trampoline_arm64
    retq