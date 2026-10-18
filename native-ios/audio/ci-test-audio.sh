#!/bin/bash
set -euo pipefail
NATIVE_SHADOW_ENGINE=native-build/engine-original.wasm node native-ios/audio/test-publish.cjs native-build/evidence/native-audio-publish.json
for sanitizer in address,undefined thread; do
  clang -std=gnu11 -Wall -Wextra -Werror -O2 -g -fsanitize="$sanitizer" native-ios/audio/test_pcm.c -o native-build/test-pcm
  native-build/test-pcm | tee "native-build/evidence/native-pcm-$sanitizer.txt"
done
clang -std=gnu11 -Wall -Wextra -Werror -O2 native-ios/audio/bench_pcm.c -o native-build/bench-pcm
native-build/bench-pcm | tee native-build/evidence/native-pcm-benchmark.txt
clang -std=gnu11 -fobjc-arc -O2 -g -fsanitize=address,undefined -framework Foundation -framework AVFoundation native-ios/audio/test_output.m -o native-build/test-audio-output
native-build/test-audio-output | tee native-build/evidence/native-audio-output.txt
xcrun --sdk iphoneos clang -target arm64-apple-ios17.0 -isysroot "$(xcrun --sdk iphoneos --show-sdk-path)" -fobjc-arc -std=gnu11 -Wall -Wextra -Werror -fsyntax-only native-ios/audio/native_audio.m
echo 'PASS: iOS native AVAudioEngine/AVAudioSession source compiles; actual playback requires device.' > native-build/evidence/native-audio-api.txt
