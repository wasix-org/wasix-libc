# WASIX libc tests

This repo has a small WASIX-focused test suite under `test/wasix`. These tests
compile C/C++ sources to WASI/WASIX wasm and run them with Wasmer.

The key things that make the suite pass are:

- **Use a non‑EH sysroot + toolchain** (no wasm exceptions). This mirrors the
  working setup in `wasix-tests/wasmer/tests/c-wasi-tests/Makefile`.
- **Do not force `--enable-all`** when running Wasmer. Let Wasmer detect module
  features.
- **Pick a Wasmer binary with WASIX support** (the wasix-tests repo build works).

## Quickstart (known‑good)

```bash
export PATH="$HOME/.wasixcc/llvm/bin:$PATH"
export CC="$HOME/.wasixcc/llvm/bin/clang"
export CXX="$HOME/.wasixcc/llvm/bin/clang++"
export WASIX_TRIPLE=wasm32-wasi

# Non‑EH toolchain (avoids legacy-exceptions + libunwind issues)
export TOOLCHAIN="$(pwd)/tools/clang-wasix-pic.cmake_toolchain"

# Non‑EH sysroot (from wasixcc)
export WASIX_SYSROOT_PIC="$HOME/.wasixcc/sysroot/sysroot"

# Wasmer runtime + backend
export WASMER_BACKEND_FLAG=--llvm
# Optional: use a specific Wasmer binary (e.g. from wasix-tests)
# export WASMER_BIN="/Users/fessguid/Projects/wasix-tests/wasmer/target/release/wasmer"

./test/wasix/run_tests.sh
```

## Single test

```bash
./test/wasix/run_tests.sh cpp_executable
```

## Why this toolchain/sysroot?

The tests match the c‑wasi‑tests Makefile in the `wasix-tests` repo:

- uses `--target=wasm32-wasi` + `-matomics -mbulk-memory -mmutable-globals -pthread`
- **does not enable wasm exceptions**
- runs under Wasmer with `--enable-threads` and standard WASI caps

Using the non‑EH sysroot (`~/.wasixcc/sysroot/sysroot`) avoids:

- `legacy_exceptions` validation errors in Wasmer
- missing `libunwind` when using the non‑EH sysroot
- `__c_longjmp` tag import expectations

## Common problems & fixes

### 1) `legacy_exceptions feature required for try instruction`

You are compiling with wasm EH enabled (or using an EH sysroot). Switch to
`tools/clang-wasix-pic.cmake_toolchain` and `~/.wasixcc/sysroot/sysroot`.

### 2) `unknown import env.__c_longjmp` / `__cpp_exception`

Your module expects wasm EH tags. Either:

- use the non‑EH toolchain+sysroot (preferred for these tests), or
- ensure Wasmer supports wasm EH tags and you use an EH sysroot.

### 3) `wasm-ld: unable to find library -lunwind`

You are using a toolchain that links `-lunwind` with a sysroot that does not
provide `libunwind` (the non‑EH sysroot). Use the non‑EH toolchain file or an
EH sysroot that includes `libunwind`.

### 4) CMake warning: `System is unknown to cmake, create: Platform/WASI`

This is a CMake warning and can be ignored for these tests.

## Updating libc for local changes

If you change libc sources in this repo (e.g. `sigsetjmp`), you need those
objects in the sysroot used by the tests.

One practical path on macOS:

1) Build libc (C only) in this repo:
   - `build32.sh` builds libc into `sysroot/` but may fail later while building
     libc++. That is OK if you only need `libc.a`.
   - If `sed -i` fails on macOS, use GNU sed (`gsed`) or run the script with a
     small sed wrapper.
2) Replace the libc in the wasixcc sysroot:
   - copy `sysroot/lib/wasm32-wasi/libc.a` → `$HOME/.wasixcc/sysroot/sysroot/lib/wasm32-wasi/libc.a`
3) Re-run `test/wasix/run_tests.sh` with the same env from Quickstart.

## Toolchain files

- `tools/clang-wasix.cmake_toolchain` — enables wasm EH (`-fwasm-exceptions`) and
  links `libunwind`. Use with an EH sysroot (e.g. `sysroot-eh`) if needed.
- `tools/clang-wasix-pic.cmake_toolchain` — **non‑EH** toolchain used for tests.

## Environment variables used by the test runner

- `TOOLCHAIN` — CMake toolchain file used to build each test.
- `WASIX_SYSROOT_PIC` — sysroot path passed to CMake.
- `WASMER_BACKEND_FLAG` — e.g. `--llvm` (default in test scripts).
- `WASMER_BIN` — optional path to a Wasmer binary (defaults to `wasmer`).
