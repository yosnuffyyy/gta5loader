#!/bin/bash
set -euo pipefail
clang -std=gnu11 -Wall -Wextra -Werror -O1 -g -DNI_TEST_CLOCK -fsanitize=address,undefined native-ios/input/test_input_queue.c native-ios/input/native_input.c -lpthread -lm -o native-build/test-native-input-queue
native-build/test-native-input-queue | tee native-build/evidence/native-input-queue-test.txt
clang -std=gnu11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer native-ios/input/test_input.c native-ios/input/native_input.c -lpthread -lm -o native-build/test-native-input
native-build/test-native-input native-build/evidence/native-input-pad.bin | tee native-build/evidence/native-input-test.txt
NATIVE_INPUT_ENGINE=native-build/engine-original.wasm node native-ios/input/test_engine_input.cjs native-build/evidence/native-input-pad.bin | tee native-build/evidence/native-input-engine-test.txt
clang -std=gnu11 -Wall -Wextra -Werror -O1 -g -fsanitize=thread native-ios/input/test_input.c native-ios/input/native_input.c -lpthread -lm -o native-build/test-native-input-tsan
native-build/test-native-input-tsan | tee native-build/evidence/native-input-thread-test.txt
