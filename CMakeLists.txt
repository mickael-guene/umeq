cmake_minimum_required (VERSION 2.8)
enable_language(C ASM)
project (umeq)

set(CMAKE_C_FLAGS "-fno-stack-protector -O2 -g -fPIC")
set(CMAKE_CXX_FLAGS "-fno-stack-protector -O2 -g -fPIC")

add_subdirectory(src)

#Need to enable testing before to include subdirectory
enable_testing()
add_subdirectory(test)
