#!/bin/bash
set -euo pipefail

BACKEND_FLAG=${WASMER_BACKEND_FLAG:---llvm}
WASMER_BIN=${WASMER_BIN:-wasmer}
WASIXCC_BIN=${WASIXCC_BIN:-wasixcc}
SYSROOT=${WASIX_SYSROOT_PIC:-${WASIX_SYSROOT:-$HOME/.wasixcc/sysroot/sysroot}}

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)

export WASIXCC_WASM_EXCEPTIONS=${WASIXCC_WASM_EXCEPTIONS:-yes}
export WASIXCC_PIC=${WASIXCC_PIC:-yes}

run_case() {
    local name="$1"
    if ! (cd "$name" && "$WASMER_BIN" run --verbose $BACKEND_FLAG ./main.wasm --volume .); then
        echo "dynamic_loading case failed: $name" >&2
        return 1
    fi
    return 0
}

build_basic() {
    local out="basic"
    mkdir -p "$out"
    "$WASIXCC_BIN" --sysroot "$SYSROOT" "$SCRIPT_DIR/basic_main.c" -o "$out/main.wasm" -Wl,-pie
    "$WASIXCC_BIN" --sysroot "$SYSROOT" "$SCRIPT_DIR/basic_side.c" -o "$out/libbasic.so" -Wl,-shared
}

build_cache() {
    local out="cache"
    mkdir -p "$out"
    "$WASIXCC_BIN" --sysroot "$SYSROOT" "$SCRIPT_DIR/cache_main.c" -o "$out/main.wasm" -Wl,-pie
    "$WASIXCC_BIN" --sysroot "$SYSROOT" "$SCRIPT_DIR/cache_side1.c" -o "$out/libcache1.so" -Wl,-shared
    "$WASIXCC_BIN" --sysroot "$SYSROOT" "$SCRIPT_DIR/cache_side2.c" -o "$out/libcache2.so" -Wl,-shared
}

build_data() {
    local out="data"
    mkdir -p "$out"
    "$WASIXCC_BIN" --sysroot "$SYSROOT" "$SCRIPT_DIR/data_side2.c" -o "$out/libdata2.so" -Wl,-shared
    "$WASIXCC_BIN" --sysroot "$SYSROOT" "$SCRIPT_DIR/data_side1.c" "$out/libdata2.so" -o "$out/libdata1.so" -Wl,-shared -Wl,-rpath,\$ORIGIN
    "$WASIXCC_BIN" --sysroot "$SYSROOT" "$SCRIPT_DIR/data_main.c" -o "$out/main.wasm" -Wl,-pie
}

build_needed() {
    local out="needed"
    mkdir -p "$out"
    "$WASIXCC_BIN" --sysroot "$SYSROOT" "$SCRIPT_DIR/needed_main_needed.c" -o "$out/libmain-needed.so" -Wl,-shared
    "$WASIXCC_BIN" --sysroot "$SYSROOT" "$SCRIPT_DIR/needed_main.c" "$out/libmain-needed.so" -o "$out/main.wasm" -Wl,-pie -Wl,-rpath,\$ORIGIN
    "$WASIXCC_BIN" --sysroot "$SYSROOT" "$SCRIPT_DIR/needed_side_needed.c" -o "$out/libside-needed.so" -Wl,-shared
    "$WASIXCC_BIN" --sysroot "$SYSROOT" "$SCRIPT_DIR/needed_side.c" "$out/libside-needed.so" -o "$out/libside.so" -Wl,-shared -Wl,-rpath,\$ORIGIN
}

build_basic
build_cache
build_data
build_needed

FAIL=0
run_case basic || FAIL=1
run_case cache || FAIL=1
run_case data || FAIL=1
run_case needed || FAIL=1

if [ "$FAIL" -ne 0 ]; then
    echo "dynamic_loading test failed" >&2
    exit 1
fi

echo "dynamic_loading test passed"
