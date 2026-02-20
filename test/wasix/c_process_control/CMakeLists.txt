set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_C_COMPILER_WORKS TRUE)

cmake_minimum_required (VERSION 3.5.0)
project (process_control_tests C)

add_executable(main main.c)
add_executable(exec_helper exec_helper.c)
