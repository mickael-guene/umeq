enable_language(C ASM)
include_directories (disassembler)
include_directories (syscall)

set (SRCS arm64.c arm64_load_image.c)

add_subdirectory (disassembler)
add_subdirectory (syscall)
add_library(arm64_target ${SRCS})