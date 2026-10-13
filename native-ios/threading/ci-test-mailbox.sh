#!/bin/bash
set -euo pipefail
mkdir -p native-build/wait native-build/evidence
for variant in baseline fixed; do
  source=native-ios/native_wait.c
  options=()
  if [[ "$variant" == baseline ]]; then source=native-ios/threading/native_wait_baseline.c; options+=(-DNATIVE_WAIT_BASELINE); fi
  clang -std=gnu11 -O1 -g -pthread -fsanitize=address,undefined \
    -DWASM_RT_USE_MMAP=1 -DWASM_RT_MEMCHECK_BOUNDS_CHECK=1 \
    -DWASM_RT_MEMCHECK_GUARD_PAGES=0 -DWASM_RT_SKIP_SIGNAL_RECOVERY=1 \
    -I native-build/wabt/wasm2c -I native-ios -I native-ios/threading \
    "${options[@]}" native-ios/threading/test_mailbox_wait.c "$source" \
    native-build/wabt/wasm2c/wasm-rt-impl.c \
    native-build/wabt/wasm2c/wasm-rt-mem-impl.c \
    native-build/wabt/wasm2c/wasm-rt-exceptions-impl.c \
    -o "native-build/wait/mailbox-$variant"
done
set +e
native-build/wait/mailbox-baseline baseline > native-build/evidence/mailbox-baseline.txt 2>&1
status=$?
set -e
cat native-build/evidence/mailbox-baseline.txt
test "$status" = 2
native-build/wait/mailbox-fixed | tee native-build/evidence/mailbox-wait-tests.txt
