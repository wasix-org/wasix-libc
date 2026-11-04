cmake_minimum_required(VERSION 3.5.0)
project(relative_path_threads C)

add_executable(main main.c)
target_compile_options(main PRIVATE -pthread)
target_link_libraries(main PRIVATE -pthread)
