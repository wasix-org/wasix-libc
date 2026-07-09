cmake_minimum_required(VERSION 3.5.0)
project(dynamic_loading C)

add_custom_target(main ALL
    COMMAND ${CMAKE_COMMAND} -E echo "dynamic_loading: build happens in test.sh"
)
