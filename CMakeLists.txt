cmake_minimum_required (VERSION 2.8)
enable_language(C ASM)
project (umeq)

#detect host architecture
include(CheckSymbolExists)
check_symbol_exists("__i386__" "" UMEQ_ARCH_i386)
check_symbol_exists("__x86_64__" "" UMEQ_ARCH_x64_64)

#setup host variables
if (UMEQ_ARCH_x64_64)
	set(UMEQ_ARCH_HOST_SIZE 64)
	set(UMEQ_ARCH_HOST_NAME "x86_64")
elseif (UMEQ_ARCH_i386)
	set(UMEQ_ARCH_HOST_SIZE 32)
	set(UMEQ_ARCH_HOST_NAME "i386")
else (UMEQ_ARCH_x64_64)
	message(FATAL_ERROR "Unsupported host")
endif (UMEQ_ARCH_x64_64)

add_subdirectory(src)

#Need to enable testing before to include subdirectory
enable_testing()
add_subdirectory(test)
