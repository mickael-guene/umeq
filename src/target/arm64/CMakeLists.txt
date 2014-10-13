enable_language(C ASM)
add_library(arm64_target arm64.c arm64_load_image.c arm64_mmap.c arm64_disassembler.c arm64_helpers.c arm64_syscall.c)