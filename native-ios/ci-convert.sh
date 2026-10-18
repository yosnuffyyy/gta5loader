#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p native-build/evidence native-build/generated
exec > >(tee native-build/evidence/build.log) 2>&1
sdk=$(xcrun --sdk iphoneos --show-sdk-path)
xcrun --sdk iphoneos clang -target arm64-apple-ios17.0 -isysroot "$sdk" -fobjc-arc -Inative-ios -fsyntax-only native-ios/App.m
plutil -lint native-ios/Info.plist
node native-ios/prepare-engine.mjs
NATIVE_MEMORY_ENGINE=native-build/engine-original.wasm node native-ios/memory/test-reference.cjs native-build/evidence/native-memory-reference.json
clang -std=gnu11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined native-ios/memory/test_policy.c -o native-build/test-memory-policy
native-build/test-memory-policy | tee native-build/evidence/native-memory-policy.txt
clang -std=gnu11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined native-ios/renderer/test_drain_policy.c -o native-build/test-drain-policy
native-build/test-drain-policy | tee native-build/evidence/native-drain-policy.txt
bash native-ios/diagnostics/ci-test-diagnostics.sh
bash native-ios/threading/ci-test-session.sh
bash native-ios/input/ci-test-input.sh
bash native-ios/input/ci-test-ui.sh
bash native-ios/renderer/build-renderer.sh
NATIVE_SHADOW_ENGINE=native-build/engine-original.wasm NATIVE_SHADOW_REPORT=native-build/evidence/native-shadow-producer.json node native-ios/renderer/producer/test-producer.cjs | tee native-build/evidence/native-shadow-producer-test.txt
git clone --depth 1 --branch 1.0.42 https://github.com/WebAssembly/wabt.git native-build/wabt
test "$(git -C native-build/wabt rev-parse HEAD)" = ff0ef7e0009402740c805a9744c09b05be063e48
git -C native-build/wabt submodule update --init --depth 1 third_party/picosha2 third_party/simde
node native-ios/patch-wabt.mjs native-build/wabt
node native-ios/patch-simd.mjs native-build/wabt
node native-ios/patch-bulk.mjs native-build/wabt
node native-ios/patch-shared-metadata.mjs native-build/wabt
node native-ios/patch-wait-unwind.mjs native-build/wabt
bash native-ios/threading/ci-test-mailbox.sh
clang -std=gnu11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer native-ios/threading/test_native_log.c -o native-build/test-native-log
native-build/test-native-log | tee native-build/evidence/native-log-test.txt
cmake -S native-build/wabt -B native-build/wabt-build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF -DBUILD_LIBWASM=OFF
cmake --build native-build/wabt-build --target wasm2c wasm-validate wat2wasm -j 3
native-build/wabt-build/wasm-validate native-build/engine-probe.wasm --enable-threads
bash native-ios/ci-test-wait.sh
node native-ios/test-simd.mjs
node native-ios/test-bulk.mjs
native-build/wabt-build/wasm2c native-build/engine-probe.wasm --enable-threads --no-debug-names --num-outputs=128 -n gta -o native-build/generated/gta.c
cp native-build/generated/gta.h native-build/evidence/gta.h
du -sh native-build/generated
find native-build/generated -type f | wc -l
echo 'PASS: complete engine converted to C with native wait/notify calls.'
node native-ios/memory/patch-generated.mjs native-build/generated native-build/evidence/native-allocator-patch.json
node native-ios/generate-probe.mjs
bash native-ios/ci-test-host.sh
node native-ios/compile-native.mjs
bash native-ios/package-ios.sh
