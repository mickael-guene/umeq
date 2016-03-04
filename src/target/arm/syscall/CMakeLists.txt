add_subdirectory (${UMEQ_ARCH_HOST_NAME})
set(SRCS ${SRCS} syscall/arm_syscall.c syscall/arm_brk.c syscall/arm_clone.c syscall/arm_mmap.c
syscall/arm_ptrace.c syscall/arm_uname.c syscall/arm_open.c syscall/arm_syscall_stat.c syscall/arm_signal.c PARENT_SCOPE)