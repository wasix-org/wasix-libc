#!/usr/bin/env bash
set -euo pipefail
wasmer run --volume "$PWD:/work" --cwd /work ./main -- 100000 1
wasmer run --volume "$PWD:/work" --cwd /work ./main -- 100000 2
