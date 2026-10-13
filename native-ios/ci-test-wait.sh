#!/bin/bash
set -euo pipefail
mkdir -p native-build/wait
native-build/wabt-build/wat2wasm --enable-threads native-ios/wait-fixture.wat -o native-build/wait/wait.wasm
native-build/wabt-build/wasm2c --enable-threads -n wait native-build/wait/wait.wasm -o native-build/wait/wait.c
clang -std=gnu11 -O1 -g -pthread \
  -DWASM_RT_USE_MMAP=1 -DWASM_RT_MEMCHECK_BOUNDS_CHECK=1 \
  -DWASM_RT_MEMCHECK_GUARD_PAGES=0 -DWASM_RT_SKIP_SIGNAL_RECOVERY=1 \
  -I native-build/wabt/wasm2c -I native-ios -I native-build/wait \
  -include native-ios/native_wait.h \
  native-build/wait/wait.c native-ios/test_wait.c native-ios/native_wait.c \
  native-build/wabt/wasm2c/wasm-rt-impl.c \
  native-build/wabt/wasm2c/wasm-rt-mem-impl.c \
  native-build/wabt/wasm2c/wasm-rt-exceptions-impl.c \
  -o native-build/wait/test-wait
native-build/wait/test-wait | tee native-build/evidence/wait-tests.txt
