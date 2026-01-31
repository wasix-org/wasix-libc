#!/bin/bash
BACKEND_FLAG=${WASMER_BACKEND_FLAG:---llvm}

WASMER_BIN=${WASMER_BIN:-wasmer}
WASM_OPT_BIN=${WASM_OPT_BIN:-$HOME/.wasixcc/binaryen/bin/wasm-opt}

if [ ! -x "$WASM_OPT_BIN" ]; then
    echo "Test failed: wasm-opt not found at $WASM_OPT_BIN (set WASM_OPT_BIN)" > /dev/stderr
    exit 1
fi

"$WASM_OPT_BIN" --asyncify ./main -o ./main
"$WASM_OPT_BIN" --asyncify ./exec_helper -o ./exec_helper

$WASMER_BIN run --verbose $BACKEND_FLAG --dir . ./main
RESULT=$?
if [ "$RESULT" != "0" ]; then
    echo "Test failed: different exit code ($RESULT vs. 0)" > /dev/stderr
    exit 1
fi

echo "c_process_control test passed"
