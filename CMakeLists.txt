cmake_minimum_required (VERSION 2.8)
enable_language(C ASM)
project (umeq)

#detect host architecture
include(CheckSymbolExists)
check_symbol_exists("__i386__" "" UMEQ_ARCH_i386)
check_symbol_exists("__x86_64__" "" UMEQ_ARCH_x64_64)

add_subdirectory(src)

#Need to enable testing before to include subdirectory
enable_testing()
add_subdirectory(test)
