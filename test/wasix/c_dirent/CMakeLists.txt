set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_C_COMPILER_WORKS TRUE)

cmake_minimum_required (VERSION 3.5.0)
project (dirent_tests)

add_executable(main main.c)
