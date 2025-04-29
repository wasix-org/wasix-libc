# WASI(X) Libc

This fork of wasi-libc extends libc with the missing functionality needed to
build complete and useful applications, in particular it incorporates these
additional extensions:

- full support for efficient multithreading including joins, signals
  and `getpid`
- `pthreads` support (now extended from the WASI threads spec)
- full support for sockets (`socket`, `bind`, `connect`, `resolve`)
    - IPv4, IPv6
    - UDP, TCP
    - Multicast, Anycast
    - RAW sockets
- current directory support (`chdir`) integrated with the runtime
- `setjmp` / `longjmp` support (used extensively in `libc` ) via `asyncify`
- process forking (`fork` and `vfork` )
- subprocess spawning and waiting (`exec` , `wait` )
- TTY support
- asynchronous polling of sockets and files
- pipe and event support (`pipe`, `event` )
- DNS resolution support (`resolve` )

# NOTES
- Memory mapping is currently not compatible with threads

# WASI(X) Extensions Spec

[WASIX](https://wasix.org) is maintained by wasix.org [here](https://github.com/wasix-org/wasix-witx)

WASI(X) intent is to extend the WASI proposal and complete the ABI sufficiently
now to build useful and productive applications today - it is not intended as a
fork but rather to be a superset on top of WASI. Therefore it maintains full
forwards and backwards compatibility with this major version of WASI and stabilizes
it for the long term.

# WASI(X) Contributions

All contributions are welcome on extending WASI(X) with other extension(s). Just
submit your pull request here and we will review via normal GitHub processes.

# Long-term Support

WASIX will receive long term support by this community with a guarantee of
backwards compatibility on the ABI. Runtime(s) that support this ABI are assured
of its stability just as standard libraries and libraries can also count on that
same stability to join the dots and make the connections.

Major bug fixes and/or zero day vulnerabilities will be addressed promptly here
with careful consideration for resolving issues without compromising the
long-term support goal.

# Installation

A pre-built sysroot suitable for C/C++ is released in this repository.

To build from source, have a look at the `build_cxx_sysroot.yml` script and
replicate its build steps.

# Usage

### CMAKE based projects
For building CMake projects, a toolchain file is included in the released sysroot
at `wasix-sysroot/clang-wasm.cmake_toolchain`. In addition to specifying the 
toolchain:

* `wasm-ld` is needed for linking. It is usually available in your system's `LLVM`
  linker package. It should be in the `PATH`.
* `CMAKE_TOOLCHAIN_FILE` and `CMAKE_SYSROOT` should both be set by you.

For an example of a CMAKE based project being built for WASIX, please read the (notes)[https://github.com/wasix-org/llvm-project/blob/llvmorg-16.0.0-wasix/NOTES-WASIX.md] 
on how to build LLVM/clang for WASIX.

### Makefile based projects
For building `Makefile` based projects, there are certain compiler and linker flags that
need to be passed to as env vars:
```
CFLAGS="--target=wasm32-wasi --sysroot=$SYSROOT -matomics -mbulk-memory -mmutable-globals -pthread -mthread-model posix -ftls-model=local-exec \
    -fno-trapping-math -D_WASI_EMULATED_MMAN -D_WASI_EMULATED_SIGNAL -D_WASI_EMULATED_PROCESS_CLOCKS \
    -g -flto -O2"

CXXFLAGS="--target=wasm32-wasi --sysroot=$SYSROOT -matomics -mbulk-memory -mmutable-globals -pthread -mthread-model posix -ftls-model=local-exec \
    -fno-trapping-math -D_WASI_EMULATED_MMAN -D_WASI_EMULATED_SIGNAL -D_WASI_EMULATED_PROCESS_CLOCKS \
    -g -flto -fno-exceptions -O2"

LDFLAGS="-Wl,--shared-memory -Wl,--max-memory=4294967296 -Wl,--import-memory -Wl,--export-dynamic \
    -Wl,--export=__heap_base -Wl,--export=__stack_pointer -Wl,--export=__data_end -Wl,--export=__wasm_init_tls \
    -Wl,--export=__wasm_signal -Wl,--export=__tls_size -Wl,--export=__tls_align -Wl,--export=__tls_base \
    -lwasi-emulated-mman -flto -g -Wl,-z,stack-size=8388608 -Wl,--error-limit=0"
```
For an example of a Makefile based project being built for WASIX, please read the (notes)[https://github.com/wasix-org/openssl/blob/master/NOTES-WASIX.md] 
on how to build OpenSSL for WASIX.

The WASIX-specific tests and script in `test/wasix` can serve as examples for how
to set this up.

# WASI Libc

WASI Libc is a libc for WebAssembly programs built on top of WASI system calls.
It provides a wide array of POSIX-compatible C APIs, including support for
standard I/O, file I/O, filesystem manipulation, memory management, time, string,
environment variables, program startup, and many other APIs.

WASI Libc is sufficiently stable and usable for many purposes, as most of the
POSIX-compatible APIs are stable, though it is continuing to evolve to better
align with wasm and WASI. For example, pthread support is still a work in
progress.

## Usage

The easiest way to get started with this is to use [wasi-sdk], which includes a
build of WASI Libc in its sysroot.

## Building from source

To build a WASI sysroot from source, obtain a WebAssembly-supporting C compiler
(currently this is only clang 8+, though we'd like to support other compilers as well),
and then run:

```sh
make CC=/path/to/clang/with/wasm/support \
     AR=/path/to/llvm-ar \
     NM=/path/to/llvm-nm
```

This makes a directory called "sysroot", by default. See the top of the Makefile
for customization options.

To use the sysroot, use the `--sysroot=` option:

```sh
/path/to/wasm/supporting/c/compiler --sysroot=/path/to/the/newly/built/sysroot ...
```

to run the compiler using the newly built sysroot.

Note that Clang packages typically don't include cross-compiled builds of
compiler-rt, libcxx, or libcxxabi, for `libclang_rt.builtins-wasm32.a`, libc++.a,
or libc++abi.a, respectively, so they may not be usable without
extra setup. This is one of the things [wasi-sdk] simplifies, as it includes
cross-compiled builds of compiler-rt, libc++.a, and libc++abi.a.
