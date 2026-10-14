#!/usr/bin/env bash

set -u

stderr_file=$(mktemp)
trap 'rm -f "$stderr_file"' EXIT

wasmer run --enable-all ./main siginfo
RESULT=$?
if [ "$RESULT" != "0" ]; then
    echo "SA_SIGINFO test failed: different exit code ($RESULT vs. 0)" >&2
    exit 1
fi

wasmer run --enable-all ./main reset 2>"$stderr_file"
RESULT=$?
if [ "$RESULT" != "138" ]; then
    echo "SA_RESETHAND test failed: different exit code ($RESULT vs. 138)" >&2
    cat "$stderr_file" >&2
    exit 1
fi
if grep -q "fatal signal: Aborted" "$stderr_file"; then
    echo "SA_RESETHAND test failed: default termination recursed into SIGABRT" >&2
    cat "$stderr_file" >&2
    exit 1
fi

wasmer run --enable-all ./main terminate 2>"$stderr_file"
RESULT=$?
if [ "$RESULT" != "143" ]; then
    echo "default SIGTERM test failed: different exit code ($RESULT vs. 143)" >&2
    cat "$stderr_file" >&2
    exit 1
fi
if [ -s "$stderr_file" ]; then
    echo "default SIGTERM test failed: default termination wrote to stderr" >&2
    cat "$stderr_file" >&2
    exit 1
fi

echo "c_signals test passed"
