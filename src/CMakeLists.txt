list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/")
include(GetGitRevisionDescription)
git_describe(GIT_DESCRIBE --always --tags --long --abbrev=12 --dirty)
git_describe(GIT_VERSION --abbrev=0)
configure_file(
	${CMAKE_SOURCE_DIR}/src/version/version.h.in
	${CMAKE_BINARY_DIR}/generated/version/version.h
)

set(CMAKE_C_FLAGS "-fno-stack-protector -m32 -O0 -g -fPIC -Wall -U_FORTIFY_SOURCE -ffreestanding -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast")
set(CMAKE_CXX_FLAGS "-fno-stack-protector -m32 -O0 -g -fPIC -Wall -U_FORTIFY_SOURCE -ffreestanding -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast")
set(CMAKE_ASM_FLAGS "-m32")

include_directories(${CMAKE_BINARY_DIR}/generated/version)
include_directories(jitter)
include_directories(cache)
include_directories(loader/loader32)
include_directories(loader/loader64)
include_directories(runtime)
include_directories(target)
include_directories(target/arm)
include_directories(target/arm64)
include_directories(syscall)
include_directories(syscall/syscall32_64)
include_directories(syscall/syscall64_64)
include_directories(syscall/syscall32_32)
include_directories(fpu)
include_directories(.)

add_subdirectory(jitter)
add_subdirectory(cache)
add_subdirectory(loader)
add_subdirectory(runtime)
add_subdirectory(target)
add_subdirectory(syscall)
add_subdirectory(fpu)

add_executable(umeq-arm main.c start.S)
target_link_libraries(umeq-arm -m32 -nostdlib -Wl,-Ttext-segment=0xb0000000 -Wl,-z,execstack -static -Wl,--start-group jitter loader32 cache runtime arm_target syscall32_32 softfloat -Wl,--end-group)
