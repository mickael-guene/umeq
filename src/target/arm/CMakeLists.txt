enable_language(C ASM)
include_directories (disassembler)
include_directories (syscall)

set (SRCS arm.c arm_load_image.c)

add_subdirectory (disassembler)
add_subdirectory (syscall)
add_subdirectory (${UMEQ_ARCH_HOST_NAME})
add_library(arm_target ${SRCS})
