#!/bin/bash
set -eu

wasmer run --enable-all --mapdir /app:. ./main | grep -q WAITPID_NONBLOCKING_OK
