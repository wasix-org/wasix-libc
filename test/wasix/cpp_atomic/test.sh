#!/bin/bash
BACKEND_FLAG=${WASMER_BACKEND_FLAG:---llvm}

WASMER_BIN=${WASMER_BIN:-wasmer}

$WASMER_BIN run --verbose $BACKEND_FLAG ./main
RESULT=$?
if [ "$RESULT" != "0" ]; then
    echo "Test failed: different exit code ($RESULT vs. 0)" > /dev/stderr
    exit 1
fi
echo "cpp_atomic test passed"
