enable_language(C ASM)
add_library(arm_target arm.c arm_load_image.c arm_disassembler.c thumb_disassembler.c arm_helpers.c arm_syscall.c arm_uname.c arm_brk.c arm_open.c arm_signal.c arm_restorer.S arm_clone.c arm_ptrace.c clone_asm.S arm_mmap.c)
