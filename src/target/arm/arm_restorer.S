# This is host dependent. Just a pb if we want multiple host support which is not the case.
.GLOBAL wrap_signal_restorer

wrap_signal_restorer:
    mov $0xf,%rax
    syscall
