enable_language(C ASM)
set (SRCS jitter.c)
add_subdirectory (${UMEQ_ARCH_HOST_NAME})
add_library(jitter ${SRCS})