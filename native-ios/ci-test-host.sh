#!/bin/bash
set -euo pipefail
node native-ios/prepare-fixtures.mjs
flags=(-std=gnu11 -Werror=incompatible-pointer-types -DWASM_RT_USE_MMAP=1 -DWASM_RT_MEMCHECK_BOUNDS_CHECK=1 -DWASM_RT_MEMCHECK_GUARD_PAGES=0 -DWASM_RT_SKIP_SIGNAL_RECOVERY=1 -Inative-build/wabt/wasm2c -Inative-build/wabt/third_party/simde -Inative-ios -Inative-build -Inative-build/generated)
clang "${flags[@]}" -fsyntax-only native-ios/native_host.c native-ios/native_runtime.c native-ios/threading/native_threads.c native-build/generated/probe.c
clang "${flags[@]}" -DNATIVE_HOST_UNIT -O1 native-ios/test_host.c native-ios/native_host.c native-build/wabt/wasm2c/wasm-rt-impl.c native-build/wabt/wasm2c/wasm-rt-mem-impl.c native-build/wabt/wasm2c/wasm-rt-exceptions-impl.c -o native-build/test-host
native-build/test-host | tee native-build/evidence/native-host-test.txt
