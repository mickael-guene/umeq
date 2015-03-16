enable_language(C ASM)
set (SRCS syscall_wrapper/exit.c assert.c syscall_wrapper/abort.c stdio.c syscall_wrapper/write.c log.c syscall_wrapper/open.c syscall_wrapper/read.c syscall_wrapper/lseek.c syscall_wrapper/close.c syscall_wrapper/map.c string.c pthread.c syscall_wrapper/unlink.c syscall_wrapper/shmctl.c syscall_wrapper/limit.c syscall_wrapper/socket.c syscall_wrapper/fcntl.c)
add_subdirectory (${UMEQ_ARCH_HOST_NAME})
add_library(runtime ${SRCS})
