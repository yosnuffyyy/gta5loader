#!/bin/bash
set -euo pipefail
clang -std=gnu11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined native-ios/renderer/test_pipeline_policy.c -o native-build/test-pipeline-policy
native-build/test-pipeline-policy | tee native-build/evidence/native-pipeline-policy-test.txt
clang -std=gnu11 -Wall -Wextra -Werror -O2 native-ios/renderer/test_pipeline_policy.c -o native-build/bench-pipeline-policy
native-build/bench-pipeline-policy | tee native-build/evidence/native-pipeline-policy-benchmark.txt
clang -std=gnu11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined native-ios/renderer/test_shadow_reference.c -o native-build/test-shadow-reference
native-build/test-shadow-reference | tee native-build/evidence/native-shadow-reference-test.txt
clang -std=gnu11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined native-ios/renderer/test_srv_contract.c -o native-build/test-srv-contract
native-build/test-srv-contract | tee native-build/evidence/native-srv-contract-test.txt
clang -std=gnu11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined native-ios/renderer/test_cache_budget.c -o native-build/test-cache-budget
native-build/test-cache-budget | tee native-build/evidence/native-cache-budget-test.txt
clang -std=gnu11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined native-ios/renderer/test_pipeline_key.c -o native-build/test-pipeline-key
native-build/test-pipeline-key | tee native-build/evidence/native-pipeline-key-test.txt
clang -std=gnu11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined native-ios/renderer/test_shadow_guard.c -o native-build/test-shadow-guard
native-build/test-shadow-guard | tee native-build/evidence/native-shadow-guard-test.txt
clang -std=gnu11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined native-ios/renderer/test_shadow_trace.c -o native-build/test-shadow-trace
native-build/test-shadow-trace | tee native-build/evidence/native-shadow-trace-test.txt
clang -fobjc-arc -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined native-ios/test_shadow_failure_log.m -framework Foundation -o native-build/test-shadow-failure-log
native-build/test-shadow-failure-log | tee native-build/evidence/native-shadow-failure-log-test.txt
node native-ios/renderer/test_reference_inputs.mjs | tee native-build/evidence/native-reference-inputs-test.txt
clang -std=gnu11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined native-ios/renderer/test_timing_window.c -o native-build/test-timing-window
native-build/test-timing-window | tee native-build/evidence/native-timing-window-test.txt
clang -std=gnu11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined native-ios/renderer/test_display_timeline.c -o native-build/test-display-timeline
native-build/test-display-timeline | tee native-build/evidence/native-display-timeline-test.txt
clang -std=gnu11 -Wall -Wextra -Werror -O1 native-ios/renderer/test_gpu_ring.c native-ios/renderer/gpu_ring.c -o native-build/test-gpu-ring
native-build/test-gpu-ring | tee native-build/evidence/native-gpu-ring-test.txt
clang -std=gnu11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined native-ios/renderer/test_gpu_ring.c native-ios/renderer/gpu_ring.c -o native-build/test-gpu-ring-sanitized
native-build/test-gpu-ring-sanitized | tee native-build/evidence/native-gpu-ring-sanitized-test.txt
clang -std=gnu11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined native-ios/renderer/test_readback_layout.c -o native-build/test-readback-layout
native-build/test-readback-layout | tee native-build/evidence/native-readback-layout-test.txt
clang -std=gnu11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined native-ios/renderer/test_vertex_step.c -o native-build/test-vertex-step
native-build/test-vertex-step | tee native-build/evidence/native-vertex-step-test.txt
clang -std=gnu11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined native-ios/renderer/test_depth_proxy_layout.c -o native-build/test-depth-proxy-layout
native-build/test-depth-proxy-layout | tee native-build/evidence/native-depth-proxy-layout-test.txt
clang -std=gnu11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined native-ios/renderer/test_uav_layout.c -o native-build/test-uav-layout
native-build/test-uav-layout | tee native-build/evidence/native-uav-layout-test.txt
clang -std=gnu11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined native-ios/renderer/test_sampler_border.c -o native-build/test-sampler-border
native-build/test-sampler-border | tee native-build/evidence/native-sampler-border-test.txt
clang -std=gnu11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined native-ios/renderer/test_buffer_upload.c -o native-build/test-buffer-upload
native-build/test-buffer-upload | tee native-build/evidence/native-buffer-upload-test.txt
clang -std=gnu11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined native-ios/renderer/test_shader_diagnostic.c -o native-build/test-shader-diagnostic
native-build/test-shader-diagnostic | tee native-build/evidence/native-shader-diagnostic-test.txt
clang -std=gnu11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined native-ios/renderer/test_resource_bytes.c -o native-build/test-resource-bytes
native-build/test-resource-bytes | tee native-build/evidence/native-resource-bytes-test.txt
clang -std=gnu11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined native-ios/renderer/test_inter_stage_layout.c -o native-build/test-inter-stage-layout
native-build/test-inter-stage-layout | tee native-build/evidence/native-inter-stage-layout-test.txt
clang -std=gnu11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined native-ios/renderer/test_query_state.c -o native-build/test-query-state
 native-build/test-query-state | tee native-build/evidence/native-query-state-test.txt
node native-ios/renderer/prepare-shader-audit.mjs
node native-ios/renderer/test-shadow-source.mjs | tee native-build/evidence/native-shadow-source-test.txt
node native-ios/renderer/test-inter-stage-source.mjs
node native-ios/renderer/prepare-renderer.mjs
bash native-ios/renderer/build-patched-backend.sh
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
