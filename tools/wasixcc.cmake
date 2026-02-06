# Cmake toolchain description file for the Makefile for WASI
cmake_minimum_required(VERSION 3.5.0)

# General options for WASIX
set(CMAKE_SYSTEM_NAME WASI)
set(UNIX 1 CACHE BOOL "" FORCE)
set(CMAKE_SYSTEM_VERSION 1)
set(CMAKE_SYSTEM_PROCESSOR wasm32)
# Don't look in the sysroot for executables to run during the build
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# Only look in the sysroot (not in the host paths) for the rest
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Configure target
set(triple wasm32-unknown-wasi)
set(CMAKE_C_COMPILER_TARGET ${triple})
set(CMAKE_CXX_COMPILER_TARGET ${triple})
set(CMAKE_ASM_COMPILER_TARGET ${triple})
set(CMAKE_C_LINKER_DEPFILE_SUPPORTED OFF)
set(CMAKE_CXX_LINKER_DEPFILE_SUPPORTED OFF)

# Set tool paths
set(CMAKE_C_COMPILER wasixcc)
set(CMAKE_CXX_COMPILER wasixcc++)
set(CMAKE_LINKER wasixld)
set(CMAKE_AR wasixar)
set(CMAKE_C_COMPILER_AR wasixar)
set(CMAKE_CXX_COMPILER_AR wasixar)
set(CMAKE_ASM_COMPILER_AR wasixar)
set(CMAKE_RANLIB wasixranlib)
set(CMAKE_C_COMPILER_RANLIB wasixranlib)
set(CMAKE_CXX_COMPILER_RANLIB wasixranlib)
set(CMAKE_ASM_COMPILER_RANLIB wasixranlib)

# TODO: Figure out how to properly set this path without hardcoding it
# set(LLVM_DIR ~/.wasixcc/llvm/lib/cmake/llvm)

