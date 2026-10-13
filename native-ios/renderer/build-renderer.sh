#!/bin/bash
set -euo pipefail
clang -std=gnu11 -Wall -Wextra -Werror -O1 native-ios/renderer/test_gpu_ring.c native-ios/renderer/gpu_ring.c -o native-build/test-gpu-ring
native-build/test-gpu-ring | tee native-build/evidence/native-gpu-ring-test.txt
clang -std=gnu11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined native-ios/renderer/test_gpu_ring.c native-ios/renderer/gpu_ring.c -o native-build/test-gpu-ring-sanitized
native-build/test-gpu-ring-sanitized | tee native-build/evidence/native-gpu-ring-sanitized-test.txt
node native-ios/renderer/prepare-renderer.mjs
for target in macos ios; do
  if [ "$target" = ios ]; then sdkname=iphoneos; else sdkname=macosx; fi
  sdk=$(xcrun --sdk "$sdkname" --show-sdk-path)
  flags=(-std=gnu11 -Wall -Wextra -Werror -O1 -isysroot "$sdk" -Inative-ios/renderer -I"native-build/wgpu/$target/include")
  if [ "$target" = ios ]; then flags+=(-target arm64-apple-ios17.0); fi
  mkdir -p "native-build/$target"
  clang "${flags[@]}" -c native-ios/renderer/native_renderer.c -o "native-build/$target/native_renderer.o"
  if [ "$target" = macos ]; then
    clang -isysroot "$sdk" -fobjc-arc -Inative-ios/renderer native-ios/renderer/renderer_cli.m native-build/macos/native_renderer.o native-build/wgpu/macos/lib/libwgpu_native.a -framework Metal -framework Foundation -framework QuartzCore -framework CoreGraphics -framework IOKit -lc++ -o native-build/macos/renderer-probe
    native-build/macos/renderer-probe | tee native-build/evidence/native-renderer-test.txt
  fi
done
echo 'PASS: native renderer fixture compiled for Mac and ARM64 iPhone.'
