list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/")
include(GetGitRevisionDescription)
git_describe(GIT_DESCRIBE --always --tags --long --abbrev=12 --dirty)
git_describe(GIT_VERSION --abbrev=0)
configure_file(
	${CMAKE_SOURCE_DIR}/src/version/version.h.in
	${CMAKE_BINARY_DIR}/generated/version/version.h
)

set(CMAKE_ASM_FLAGS "${CMAKE_C_FLAGS} -fno-stack-protector -O2 -g -Wall -U_FORTIFY_SOURCE -D__NO_STRING_INLINES -ffreestanding -fno-builtin")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-stack-protector -O2 -g -Wall -U_FORTIFY_SOURCE -D__NO_STRING_INLINES -ffreestanding -fno-builtin")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -fno-stack-protector -O2 -g -Wall -U_FORTIFY_SOURCE -D__NO_STRING_INLINES -ffreestanding -fno-builtin")

# define or modify some variables according to host size
if (UMEQ_ARCH_HOST_SIZE EQUAL 64)
	set(UMEQ_SYSCALL_32 "syscall32_64")
	set(UMEQ_LOAD_ADDR 0x8000000000)
	add_definitions(-DUMEQ_ARCH_HOST_SIZE=64)
elseif (UMEQ_ARCH_HOST_SIZE EQUAL 32)
	set(UMEQ_SYSCALL_32 "syscall32_32")
	set(UMEQ_LOAD_ADDR 0xb0000000)
	add_definitions(-DUMEQ_ARCH_HOST_SIZE=32)
else (UMEQ_ARCH_HOST_SIZE EQUAL 64)
	message(FATAL_ERROR "UMEQ_ARCH_HOST_SIZE must be either 32 or 64")
endif (UMEQ_ARCH_HOST_SIZE EQUAL 64)

# define or modify some variables according to host architecture
if (UMEQ_ARCH_x64_64)
	set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -mcmodel=large")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mcmodel=large")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mcmodel=large")
endif (UMEQ_ARCH_x64_64)

# define top level source files
set (SRCS main.c)

# directories to include
include_directories(${CMAKE_BINARY_DIR}/generated/version)
include_directories(jitter)
include_directories(cache)
include_directories(loader/loader32)
if (UMEQ_ARCH_HOST_SIZE EQUAL 64)
	include_directories(loader/loader64)
endif (UMEQ_ARCH_HOST_SIZE EQUAL 64)
include_directories(runtime)
include_directories(target)
include_directories(target/arm)
include_directories(target/arm64)
include_directories(syscall)
include_directories(syscall/syscall32_64)
include_directories(syscall/syscall64_64)
include_directories(fpu)
include_directories(.)

# sub directories to parse
add_subdirectory(jitter)
add_subdirectory(cache)
add_subdirectory(loader)
add_subdirectory(runtime)
add_subdirectory(target)
add_subdirectory(syscall)
add_subdirectory(fpu)
add_subdirectory (${UMEQ_ARCH_HOST_NAME})

# umeq-arm
add_executable(umeq-arm ${SRCS})
target_link_libraries(umeq-arm -nostdlib -Wl,-Ttext-segment=${UMEQ_LOAD_ADDR} -Wl,-z,execstack -static -Wl,--start-group jitter loader32 cache runtime arm_target ${UMEQ_SYSCALL_32} softfloat -Wl,--end-group)

# umeq-arm64 is only build for 64 bits hosts
if (UMEQ_ARCH_HOST_SIZE EQUAL 64)
	add_executable(umeq-arm64 ${SRCS})
	target_link_libraries(umeq-arm64 -nostdlib -Wl,-Ttext-segment=0x8000000000 -Wl,-z,execstack -static -Wl,--start-group jitter loader64 cache runtime arm64_target syscall64_64 softfloat -Wl,--end-group)
endif (UMEQ_ARCH_HOST_SIZE EQUAL 64)
