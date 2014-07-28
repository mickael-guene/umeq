include_directories(jitter)
include_directories(cache)
include_directories(loader)
include_directories(runtime)
include_directories(target)
include_directories(target/arm)


add_subdirectory(jitter)
add_subdirectory(cache)
add_subdirectory(loader)
add_subdirectory(runtime)
add_subdirectory(target)

add_executable(umeq main.c start.S)
target_link_libraries(umeq -nostdlib -T${umeq_SOURCE_DIR}/src/custom.ld -Wl,-z,execstack -static -Wl,--start-group jitter loader cache runtime arm_target -Wl,--end-group)
