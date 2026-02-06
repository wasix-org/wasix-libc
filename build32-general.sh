#!/bin/bash
set -Eeuxo pipefail

### Determine build configuration

EH=${EH:-OFF}
if [ "$EH" != "ON" ]; then EH=OFF ; fi
PIC=${PIC:-OFF}
if [ "$PIC" != "ON" ]; then PIC=OFF ; fi

REPO_ROOT="$(pwd)"

case "${EH}-${PIC}" in
    ON-ON)   NAME="32-ehpic" ;;
    ON-OFF)  NAME="32-eh" ;;
    OFF-ON)  echo "PIC is only supported when exception handling is enabled" ; exit 1 ;;
    OFF-OFF)  NAME="32" ;;
    *)       echo "Invalid EH/PIC combination: EH:${EH} PIC:${PIC}" ; exit 1 ;;
esac

if [ "$EH" = "ON" ]; then
    CMAKE_TOOLCHAIN="$REPO_ROOT"/tools/clang-wasix-eh.cmake_toolchain
else
    CMAKE_TOOLCHAIN="$REPO_ROOT"/tools/clang-wasix.cmake_toolchain
fi

### Path names of various build artifacts

wasix_libc_output="build/wasix-libc-sysroot-$NAME"

compiler_rt_build_dir="build/compiler-rt-$NAME"
compiler_rt_output="build/compiler-rt-sysroot-$NAME"
compiler_rt_build_sysroot="build/compiler-rt-build-sysroot-$NAME"

libcxx_build_dir="build/libcxx-$NAME"
libcxx_output="build/libcxx-sysroot-$NAME"
libcxx_build_sysroot="build/libcxx-build-sysroot-$NAME"

sysroot_output="sysroot$NAME"

### Build settings

export TARGET_ARCH=wasm32
export TARGET_OS=wasix


### Build steps

compiler_rt() {
    # Cleanup previous build artifacts
    rm -rf ${compiler_rt_build_dir}
    rm -rf ${compiler_rt_output}
    rm -rf ${compiler_rt_build_sysroot}

    # Empty sysroot for the build
    mkdir -p $compiler_rt_build_sysroot
    export WASIXCC_SYSROOT="$REPO_ROOT"/$compiler_rt_build_sysroot

    # Build the compiler runtime lib
    cmake \
        -DCMAKE_SYSTEM_NAME=WASI \
        -DCMAKE_SYSTEM_VERSION=1 \
        -DCMAKE_SYSTEM_PROCESSOR=wasm32 \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DCMAKE_C_COMPILER_WORKS=ON \
        -DCMAKE_CXX_COMPILER_WORKS=ON \
        -DCMAKE_C_LINKER_DEPFILE_SUPPORTED=OFF \
        -DCMAKE_CXX_LINKER_DEPFILE_SUPPORTED=OFF \
        -DCOMPILER_RT_BAREMETAL_BUILD=ON \
        -DCOMPILER_RT_BUILD_XRAY=OFF \
        -DCOMPILER_RT_INCLUDE_TESTS=OFF \
        -DCOMPILER_RT_HAS_FPIC_FLAG="$PIC" \
        -DCOMPILER_RT_DEFAULT_TARGET_ONLY=ON \
        -DCOMPILER_RT_BUILD_SANITIZERS=OFF \
        -DCOMPILER_RT_BUILD_XRAY=OFF \
        -DCOMPILER_RT_BUILD_LIBFUZZER=OFF \
        -DCOMPILER_RT_BUILD_PROFILE=OFF \
        -DCOMPILER_RT_BUILD_CTX_PROFILE=OFF \
        -DCOMPILER_RT_BUILD_MEMPROF=OFF \
        -DCOMPILER_RT_BUILD_ORC=OFF \
        -DCOMPILER_RT_BUILD_GWP_ASAN=OFF \
        -DCOMPILER_RT_USE_LLVM_UNWINDER=OFF \
        -DCOMPILER_RT_BUILTINS_ENABLE_PIC="$PIC" \
        -DSANITIZER_USE_STATIC_LLVM_UNWINDER=OFF \
        -DCOMPILER_RT_ENABLE_STATIC_UNWINDER=OFF \
        -DHAVE_UNWIND_H=OFF \
        -DCOMPILER_RT_HAS_FUNWIND_TABLES_FLAG=OFF \
        -DCMAKE_C_COMPILER_TARGET=wasm32-wasi \
        -DCOMPILER_RT_OS_DIR=wasm32-wasi \
        -DCMAKE_TOOLCHAIN_FILE="$CMAKE_TOOLCHAIN" \
        -DCMAKE_SYSROOT="$REPO_ROOT"/$compiler_rt_build_sysroot \
        -DCMAKE_INSTALL_PREFIX="$REPO_ROOT"/$compiler_rt_output \
        -DUNIX:BOOL=ON \
        -B $compiler_rt_build_dir \
        -S tools/llvm-project/compiler-rt
    cmake --build $compiler_rt_build_dir --parallel 16
    cmake --install $compiler_rt_build_dir
    llvm-ranlib $compiler_rt_output/lib/wasm32-wasi/libclang_rt.builtins-wasm32.a
}

# Regenerate bindings in libc-bottom-half
prepare_wasix_libc() {
    # Build the extensions
    cargo run --manifest-path tools/wasix-headers/Cargo.toml generate-libc
    cp -f libc-bottom-half/headers/public/wasi/api.h libc-bottom-half/headers/public/wasi/api_wasix.h
    sed -i 's|__wasi__|__wasix__|g' libc-bottom-half/headers/public/wasi/api_wasix.h
    sed -i 's|__wasi_api_h|__wasix_api_h|g' libc-bottom-half/headers/public/wasi/api_wasix.h
    cp -f libc-bottom-half/sources/__wasilibc_real.c libc-bottom-half/sources/__wasixlibc_real.c
    
    # Build WASI
    cargo run --manifest-path tools/wasi-headers/Cargo.toml generate-libc
    cp -f libc-bottom-half/headers/public/wasi/api.h libc-bottom-half/headers/public/wasi/api_wasi.h
    
    # Emit the API header
    cat > libc-bottom-half/headers/public/wasi/api.h<<EOF
#include "api_wasi.h"
#include "api_wasix.h"
#include "api_poly.h"
EOF
}

# Build wasix-libc
wasix_libc() {
    MAKEFLAGS=""
    if [ "$PIC" = "ON" ]; then
        MAKEFLAGS="$MAKEFLAGS PIC=yes"
    else
        MAKEFLAGS="$MAKEFLAGS PIC=no"
    fi
    if [ "$EH" = "ON" ]; then
        MAKEFLAGS="$MAKEFLAGS -f Makefile-eh"
    else
        MAKEFLAGS="$MAKEFLAGS -f Makefile"
    fi

    # Cleanup previous build artifacts
    rm -rf sysroot
    rm -rf build/wasm32-wasi
    rm -rf $wasix_libc_output

    # shellcheck disable=SC2086
    CC=clang CXX=clang++ make CHECK_SYMBOLS="${CHECK_SYMBOLS:-yes}" -j 16 $MAKEFLAGS
    rm -f sysroot/lib/wasm32-wasi/libc-printscan-long-double.a
    rm -rf $wasix_libc_output
    mv sysroot $wasix_libc_output
}

# Build C++ sysroot
libcxx() {
    if [ "$EH" = "ON" ]; then
        runtimes="libcxx;libcxxabi;libunwind"
    else
        runtimes="libcxx;libcxxabi"
    fi

    # Cleanup previous build artifacts
    rm -rf $libcxx_build_dir
    rm -rf $libcxx_build_sysroot
    rm -rf $libcxx_output


    mkdir -p $libcxx_build_sysroot
    rsync -abuz  $compiler_rt_output/ $libcxx_build_sysroot/
    rsync -abuz  $wasix_libc_output/ $libcxx_build_sysroot/
    export WASIXCC_SYSROOT="$REPO_ROOT"/$libcxx_build_sysroot

    cmake \
        -DCMAKE_POSITION_INDEPENDENT_CODE="$PIC" \
        -DCMAKE_TOOLCHAIN_FILE="$CMAKE_TOOLCHAIN" \
        -DCMAKE_SYSROOT="$REPO_ROOT"/$libcxx_build_sysroot \
        -DCMAKE_INSTALL_PREFIX="$REPO_ROOT"/$libcxx_output \
        -DCXX_SUPPORTS_CXX23=ON \
        -DLIBCXX_ENABLE_THREADS:BOOL=ON \
        -DLIBCXX_HAS_PTHREAD_API:BOOL=ON \
        -DLIBCXX_HAS_EXTERNAL_THREAD_API:BOOL=OFF \
        -DLIBCXX_BUILD_EXTERNAL_THREAD_LIBRARY:BOOL=OFF \
        -DLIBCXX_HAS_WIN32_THREAD_API:BOOL=OFF \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DLIBCXX_ENABLE_SHARED:BOOL=OFF \
        -DLIBCXX_ENABLE_EXPERIMENTAL_LIBRARY:BOOL=OFF \
        -DLIBCXX_ENABLE_EXCEPTIONS:BOOL="$EH" \
        -DLIBCXX_ENABLE_FILESYSTEM:BOOL=ON \
        -DLIBCXX_CXX_ABI=libcxxabi \
        -DLIBCXX_HAS_MUSL_LIBC:BOOL=ON \
        -DLIBCXX_ABI_VERSION=2 \
        -DLIBCXX_USE_COMPILER_RT=ON \
        -DLIBCXXABI_ENABLE_EXCEPTIONS:BOOL="$EH" \
        -DLIBCXXABI_ENABLE_SHARED:BOOL=OFF \
        -DLIBCXXABI_SILENT_TERMINATE:BOOL=ON \
        -DLIBCXXABI_ENABLE_THREADS:BOOL=ON \
        -DLIBCXXABI_HAS_PTHREAD_API:BOOL=ON \
        -DLIBCXXABI_HAS_EXTERNAL_THREAD_API:BOOL=OFF \
        -DLIBCXXABI_BUILD_EXTERNAL_THREAD_LIBRARY:BOOL=OFF \
        -DLIBCXXABI_HAS_WIN32_THREAD_API:BOOL=OFF \
        -DLIBCXXABI_ENABLE_PIC:BOOL="$PIC" \
        -DLIBCXXABI_USE_LLVM_UNWINDER:BOOL="$EH" \
        -DLIBUNWIND_ENABLE_SHARED:BOOL=OFF \
        -DLIBUNWIND_ENABLE_STATIC:BOOL="$EH" \
        -DLIBUNWIND_USE_COMPILER_RT:BOOL="$EH" \
        -DLIBUNWIND_ENABLE_THREADS:BOOL="$EH" \
        -DLIBUNWIND_HAS_PTHREAD_LIB:BOOL="$EH" \
        -DLIBUNWIND_INSTALL_LIBRARY:BOOL="$EH" \
        -DCMAKE_C_COMPILER_WORKS=ON \
        -DCMAKE_CXX_COMPILER_WORKS=ON \
        -DLLVM_COMPILER_CHECKED=ON \
        -DUNIX:BOOL=ON \
        -DLIBCXX_LIBDIR_SUFFIX=/wasm32-wasi \
        -DLIBCXXABI_LIBDIR_SUFFIX=/wasm32-wasi \
        -DLLVM_LIBDIR_SUFFIX=/wasm32-wasi \
        -DLLVM_ENABLE_RUNTIMES="$runtimes" \
        -B $libcxx_build_dir \
        -S tools/llvm-project/runtimes
    cmake --build $libcxx_build_dir --parallel 16
    cmake --install $libcxx_build_dir
}

sysroot() {
    rm -rf $sysroot_output
    mkdir -p $sysroot_output
    rsync -abuz  $compiler_rt_output/ $sysroot_output/
    rsync -abuz  $wasix_libc_output/ $sysroot_output/
    rsync -abuz  $libcxx_output/ $sysroot_output/
}

### Run build steps

# Build compiler rt
compiler_rt
# Generate files in wasix-libc
prepare_wasix_libc
# Build wasix-libc
wasix_libc
# Build C++ sysroot
libcxx
# Combine libcxx, wasix-libc and compiler-rt into one
sysroot
