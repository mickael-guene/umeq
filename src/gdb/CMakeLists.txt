add_library(gdb32 gdb.c breakpoints.c)
add_library(gdb64 gdb.c breakpoints.c)

set_target_properties(gdb32 PROPERTIES COMPILE_FLAGS "-D__TARGET32")
set_target_properties(gdb64 PROPERTIES COMPILE_FLAGS "-D__TARGET64")