#!/bin/sh
# Compiles every examples/c/*.c (or the files given) to examples/c/out/*.wasm
# Needs clang with the wasm32 target and lld:  sudo dnf install clang lld
set -e
cd "$(dirname "$0")"
mkdir -p out
files=${*:-*.c}
for src in $files; do
    clang --target=wasm32 -Os -nostdlib -ffreestanding \
        -Wl,--no-entry -Wl,--export=main -Wl,--allow-undefined \
        -Wl,-z,stack-size=8192 -Wl,--initial-memory=65536 \
        -o "out/${src%.c}.wasm" "$src"
    echo "out/${src%.c}.wasm  $(wc -c < "out/${src%.c}.wasm") bytes"
done
