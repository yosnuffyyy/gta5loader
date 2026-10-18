# Native 0.39 — ordered transfers, texture-copy compatibility and timing evidence

## Status and provenance

This is an implementation and verification report, not a claim of 30 FPS, corrected speech, completed pause-menu gameplay, or tested base-iPhone performance. The final independently validated test build and its remaining limitations are recorded below.

- Source baseline: `61e271a535c3f951b8f3f8cf048325962cc3bb68` on `native-ios-prototype`; the previously released 0.38 executable was built from `360bb27d7c3e3750611bebcfdc6adf00c98303e8`.
- Validated build source: `68c1394b3d067e73aacb17bd3d7fe39091708315`.
- Device evidence: complete user-supplied `3GTA-Native-Log-E2F78E2A-33C3-4241-B80E-A0D9F8FF48F6.txt`, 19,894,824 bytes, SHA256 `11029ef1ae7ecd5cbdca8ade0ece871e5cb67e384d142a1e98078708b4fd0310`.
- Device: reported iPhone 16 Pro Max/A18 Pro, Native 0.38. Approximately 750 seconds, mixed loading/cutscene/gameplay phases, ending in a `COPY_REGION` failure. No reliable scene-specific FPS marker in this capture.
- Original engine SHA256: `e65bec476e8fd8125515150c24037c9bd0d0c46530e3544f9d389ab108b5592e`. Reference commands inspected in `work/minimal-engine/wgpu_worker.js`.
- Compiler foundation retained: 128 AOT C partitions, production iOS `-O1`, host engine `-O0`, pinned WABT 1.0.42 and wgpu-native 29.0.1.1. No unsafe floating-point flags, engine optimization-level promotion or ABI changes.
- Sources and changes remain in the native prototype. Protected Desktop folders, deployed `main`, original archives and existing IPAs were not used or modified.

## 1. Pause-menu failure: confirmed defect, implemented coverage and limit

The retained fatal operation is `COPY_REGION` (34), destination 35378 and source 10933. The six retained words are `[35378,0,0,0,0,10933]`; associated VS/PS hashes are `028921879df627f5` / `c8a465dc9354a574`. Pause-menu/Scaleform initialization precedes it.

**Confirmed native defect:** `copy_region` rejected every copy whose native formats differed, before considering the original D3D format family or block-compression cast rules. The JavaScript reference already implements compatible compressed/raw copies through a GPU buffer. These operations reinterpret stored bits; a color conversion or a multisample resolve is not equivalent.

**Evidence limitation:** this physical report did not retain the source/destination texture descriptors or all 14 command words. Consequently the precise format/sample pair of the actual failed menu copy remains UNKNOWN. Reusing the captured IDs does not make representative descriptors an exact capture. Opening/navigating/closing/reopening the actual game menu still requires the next device test.

Implemented:

- Strict DXGI family and documented BC/raw cast compatibility checks.
- Storage-block-based source/destination extents, including small virtual mips, array subresources and different raw/compressed texel dimensions.
- Same-format direct GPU copies; compatible different-format copies use a single lazily allocated, bounded 1 MiB GPU-only tile. Source bits survive unchanged. No CPU map/readback, numeric conversion, synthetic texture contents or additional completion wait.
- Sequential tile reuse in command-encoder/queue order; explicit source/destination bounds and complete compressed blocks.
- Missing BC4 SNORM (DXGI 81), BC5 SNORM (84), and BC6H signed float (96) mappings and block-size metadata. The expanded fixture found these missing mappings; the first CI attempt stopped rather than skipping them.
- Full source/destination descriptor, generation and 14-word payload diagnostics on a copy failure.

Depth copies remain restricted to valid identical-format complete subresources. Existing `RESOLVE` opcode 36 behavior is retained. Multisample `COPY_REGION`, unsupported depth reinterpretation, illegal format families, split blocks, and same-subresource self-copy remain explicit errors. A multisample copy is not silently changed into a resolve.

Relevant contracts: [D3D11 CopyResource](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-copyresource), [block-compression size casts](https://learn.microsoft.com/en-us/windows/win32/direct3d10/d3d10-graphics-programming-guide-resources-block-compression), [D3D11.3 functional specification](https://microsoft.github.io/DirectX-Specs/d3d/archive/D3D11_3_FunctionalSpec.htm), and [WebGPU copy rules](https://gpuweb.github.io/gpuweb/).

## 2. Actual engine graphics settings

The 0.38 engine's own settings dump reports 640×360; texture, shadow, reflection, grass, water, particles and PostFX quality 0; MSAA 0; FXAA/TXAA false; SSAO/tessellation/anisotropic filtering disabled; motion blur 0; low LOD. VSync 2 and `-frameLimit=2` remain the existing 30-FPS profile.

`ShaderQuality=1`, `Shader_SSA=true`, `Reflection_MipBlur=true`, `Shadow_Distance=1.0` were also observed. A setting's numeric zero is not automatically a valid lower-quality equivalent in every consuming subsystem. The shader-quality setter accepts a bounded numeric range, but the complete configuration-to-pipeline mapping and reset behavior were not established sufficiently to change those remaining values safely. No claim that every reset is covered, and no unverified graphics reduction is applied.

## 3. Ambient population and mission safety

No production density preset or adaptive population scaler is shipped in this revision. No characters/traffic are removed, and no new user setting is presented without a working safe engine contract.

New original-engine tests call the actual pedestrian, random-vehicle and parked-vehicle density functions. They verify the proposed 0.60/0.70, 0.40/0.50 and 0.25/0.35 multiplier pairs and preservation of stricter existing 0.10 script limits. The script setters take minima; vehicle getters combine persistent, per-frame and optional override values. `CityDensity`, pedestrian variety and vehicle variety are separate controls.

Further static inspection of original function 19099 (`CPopulationHelpers::GetConfigValue`) establishes `ceil((high - low) * effectiveDensity + low)`. Its usual density input is settings offset 132, also returned by `CommandGetCityDensity` (52903); an engine override flag can select a separate value. The inspected pedestrian initializer passes paired high/low configuration values into this helper. Thus `CityDensity=0` selects the configured low endpoint in this path; it does **not** mean zero pedestrians. Actual endpoint counts and override state during each mission were not captured. Functions 80420/80421 independently apply the random/parked vehicle multipliers. This is source-derived behavior, not a spawned-entity measurement.

**Remaining blocker:** the precise reset/application boundary and preservation of mission-requested ambient traffic have not been verified. Calling supported setters at an arbitrary time would not satisfy mission safety. Spawn counts, AI cost, mission preservation and density FPS benefit remain UNKNOWN. These contract tests are not spawning or mission-completion tests.

## 4. Animation, simulation and speech timing

The original timer function `f8924` is tested with a controllable clock, both its default minimum delta and the minimum observed on the phone. Its maximum delta is approximately 66.667 ms. The original behavior is reproduced, not changed:

| Update cadence | Real elapsed | Sum of engine deltas |
|---|---:|---:|
| 5 Hz | 10 s | 3.333 s |
| 10 Hz | 10 s | 6.667 s |
| 15 Hz | 10 s | 10.000 s |
| 20 Hz | 10 s | 10.000 s |
| 30 Hz | 10 s | 10.000 s |

Integer script-millisecond accumulation has separate truncation effects, captured in the test JSON. The fixture tests pause/resume and excludes intentional slow-motion claims.

The physical log contains 148 original timer records (duplicated preview text excluded). Across 146 records with nonzero game advancement, 734.772 seconds real time corresponded to 683.510 seconds game time. A slow window advanced 3.168 game seconds in 5.056 real seconds with 48 hooks and a 66.667 ms delta. This supports elapsed-time loss during slow updates. It does not identify every animation/camera/physics clock, and mixed phases are not a cutscene-only benchmark.

No global speed multiplier, delta-cap removal, speculative catch-up loop or asynchronous engine writes were introduced. Safe physics/script substepping or simulation/render decoupling remains unfinished; changing only the clock would risk collisions, mission ordering and vehicle physics. Rendering batching may reduce backpressure, but corrected real-time animation is not yet demonstrated.

## 5. Audio underruns and new mixer evidence

The existing 48 kHz stereo PCM path is retained. The final report records 24,579,840 consumed frames plus 11,422,720 missing frames: 36,002,560 requested frames, about 750.053 seconds, with **31.7275% unavailable**. The underrun counter counts requested output frames for which no produced PCM was available, not compressed asset bytes or silent samples already produced by the mixer. Nonfinite/invalid counts were zero. The maximum queued 3,072 frames corresponds to 64 ms.

Source inspection found that the original mixer already observes the shared consumer cursor and fills toward its existing target. Its live-consumer path does not simply add an unconditional sleep after every mix. There is not enough evidence to choose a new priority, queue depth, sample rate or resampler as a root-cause fix. Missing `RESIDENT\\LOW_LATENCY.awc` is still reported and is not fabricated or substituted.

New instrumentation surrounds the two verified original `MixBuffers` call sites in AOT function 7394, measuring wall and thread CPU time for one call in 64. Fixed atomic aggregates plus thread-local timestamps are used; the hardware callback, PCM bytes, pitch, queue size and scheduler are unchanged. Sampling reports total calls, sample count, mean/max wall, mean thread CPU and clock errors. It distinguishes slow mixing from descheduling inside mixing; gaps outside mixing still need scheduling/producer evidence. No audio-quality or underrun reduction is claimed yet.

## 6. Ordered clear/copy batching

The latest log does **not** show 270,340 dependent-buffer flushes: only four remain in that specific existing optimization. The larger counter covers every mutation opcode. Preserving that distinction avoids reimplementing 0.38's successful upload work.

New behavior retains an existing draw encoder across compatible color/depth clears and region copies. It ends the current render pass and occlusion segment, encodes the operation in order, then allows subsequent draws to continue in a new pass in the same command buffer. A maximum of 64 retained transfers forces submission; existing drain, uniform staging and completion bounds remain. Upload, destruction, readback and query synchronization rules are retained.

Buffer copies mark both source and destination as used in the current draw epoch. Subsequent uploads must therefore use the existing ordered upload path; they cannot overtake an encoded copy. Texture content/proxy versions are invalidated as before. No buffer is reused before completion, and no new frame queue is introduced.

The workload motivating this change is directly measured in 0.38:

| Opcode | Calls | Inclusive handler wall | Active CPU | Nested submissions | Mutation preflushes |
|---|---:|---:|---:|---:|---:|
| Color clear 5 | 187,159 | 55,805.493 ms | 44,989.798 ms | 233,179 | 46,020 |
| Depth clear 6 | 125,678 | 68,783.554 ms | 62,549.245 ms | 139,229 | 13,551 |
| Region copy 34 | 156,505 | 80,770.665 ms | 58,629.033 ms | 310,796 | 154,292 |
| Whole copy 35 | 47,086 | 26,261.229 ms | 19,197.931 ms | 76,963 | 29,877 |

Nested submissions include the preceding flush, not just the named operation. Times overlap broader handler/service categories. The optimization covers 5/6/34, not every whole copy, resolve or readback. Savings on the full phone trace cannot be inferred by subtracting all these costs.

The new paired fixture runs the same 128 original-shader draws, clears and copies with legacy versus retained-encoder behavior. It checks every accumulated output pixel and source/destination buffer-copy hazards. Quantitative CI results are recorded in the final validation section.

## 7. Baseline measurements and remaining bottlenecks

All following values are physical **0.38**, not post-change measurements:

| Metric | Recorded value / scope |
|---|---|
| Actual display intervals, last 256 | mean 39.522; p50 33.336; p95 50.005; p99/max 66.673 ms |
| GPU-service CPU, latest window | mean 27.354 ms/PRESENT; not GPU execution |
| Command handler wall / active CPU | mean 35.455 / 24.847 ms/PRESENT; overlapping categories |
| Upload handler wall | mean 2.031 ms/PRESENT, included above |
| Queue submissions / draw batches / draws | 1,141,628 / 581,875 / 9,678,635 |
| Protocol buffer / texture uploads | 14,929,971 / 95,983 |
| Staging pressure waits | 29,198 |
| Completion-wait wall | 48,918.642 ms lifetime |
| Readback count / map wall | 27,539 readbacks; 44,331.267 ms map wall lifetime |
| Uniform bytes / packs / reuse | 22,877,379,840 bytes / 10,490,491 / 8,433,029 |
| Pipeline creates / hits / evictions | 970 / 9,677,665 / 714; bounded capacity 256 |
| Pipeline API worst / cumulative | 412.559 / 2,303.997 ms |
| Input enqueue-to-engine hook | mean 44.707, max 467.989 ms over 4,109 samples |
| Process memory | recent samples approximately 2,558 then 2,388 MiB; cleanup 95.8 MiB |
| Logical GPU payload peak | 577,660 KiB; not driver residency or process footprint |
| Thermal state | serious |

No original rendering samples were discarded. Actual GPU execution time, CPU core placement, driver allocations, end-to-end Bluetooth/touch-to-display latency and scene-specific sustained 0.39 FPS remain UNKNOWN.

The measured highest-value remaining work is: (1) draw/query/pass preparation and submission costs, (2) safe simulation time progression under renderer backpressure, (3) mixer starvation/decoding versus scheduling, (4) required readback and completion dependencies, and (5) recurring pipeline creation during transitions. CPU opcode timings are inclusive; sub-microsecond wall-clock quantization and profiler clock overhead make some very short-op active sums exceed wall sums. They must not be treated as exact instruction-level costs.

## 8. Engine compiler, input, memory and device policy

No new engine compiler optimization is claimed. There is no representative active-engine profile sufficient to promote hot partitions safely, and changing all partitions to `-O3` would not establish a gain. No new input mutation, frame-limit or scheduler policy is applied. Reduced native submission overhead may help response time, but 0.39 input latency remains unmeasured on device.

New copy scratch is bounded at 1 MiB GPU allocation and allocated only when needed; copy tiling does not retain whole textures or archives. Mixer timing aggregate storage is fixed (six 64-bit atomic counters plus small TLS state). Driver residency and final process footprint require physical testing. Existing one-64-KiB-buffer/eight-open-file archive limits are retained.

A single ARM64 build continues runtime adapter-feature/limit discovery. No A18 Pro-specific workload increase, assumed 30-FPS capability, memoryless-resource hack, direct-Metal rewrite, adaptive resolution or unvalidated quality preset was introduced. Base A18 hardware testing is unavailable. Default 640×360 and 30-FPS cap remain unchanged.

## 9. Preservation inventory and affected files

| Established behavior | Disposition |
|---|---|
| Ordered uploads, immutable draw-visible data, uniform dedup/reuse | Retained; new copy-buffer epoch hazards tested |
| Async event queries, occlusion results and completion-based retirement | Retained; no fabricated visibility or completions |
| 256-entry pipeline cache, pressure pruning, semantic keys/fingerprints | Retained; no capacity increase or blanket shrink change |
| Shadow/SRV709 compatibility, SRV5633 correction, clip/varying linking, compute/UAV | Retained; existing pixel/resource regressions required |
| Input digital edges, analog coalescing, controller/touch switching | Retained |
| Allocator, streaming, recovery snapshots, bounded logs/export | Retained |
| 48-kHz native audio output and interruption lifecycle | Retained; sparse producer instrumentation added |
| Icon/name, bundle identity, UI/fullscreen, file access | Retained; version/build advanced to 0.39/39 |
| Original assets and prior IPA releases | Preserved; only IPA will be a release asset |

Production changes: `native-ios/renderer/native_gpu.m`, `native_draw.inc`, `native_readback.inc`, new `texture_copy.h`; `audio/native_audio.m`, new `native_mixer_profile.h`/`patch-mixer.mjs`; summary capacity in `threading/native_threads.c`. Build/test integration: `ci-convert.sh`, `compile-native.mjs`, `copy_cast_fixture.inc`, `transfer_fixture.inc`, `test_texture_copy.c`, `audio/test_output.m`, `timing/test-timer.cjs`, new `timing/test-population.cjs`. Branding version changes only in `App.m`, `Info.plist` and `ui/NativeDashboard.inc`. No original-engine source or game files are published in the release assets.

## 10. Validation, build identification and delivery

Final source: `68c1394b3d067e73aacb17bd3d7fe39091708315`. [CI 39152842578](https://github.com/yosnuffyyy/gta5loader/actions/runs/39152842578), job 117749975884: **PASS**. All 272 native/workflow/manifest blob hashes matched the exact remote tree. Source baseline preservation: 252 pre-existing files unchanged, 12 deliberately changed, seven added; all 38 previous IPAs retain their original hashes. The deployed main branch is unchanged.

Validated executable: **0.39.0 / build 39**, unsigned ARM64 iPhone, minimum iOS 17.0, unchanged bundle ID `io.github.yosnuffyyy.gtanativeprobe`, display name **GTA V (Test Build)** and supplied AppIcon. No WebKit, writable-executable segment, provisioning profile or game/web archive payload.

- IPA: `GTA-Native-Probe-0.39-unsigned.ipa`, 104,978,601 bytes.
- IPA SHA256: `99b1f41e8f3a76d81e06aa844c813baf9793ffe667484b6af16a85b2455f4571`.
- IPA workflow artifact: 12146606164; ZIP 92651329 bytes; sha256:af7f1ae751691e64d88d6b45832a44a537048679388afa1b3e3af3dcd19f6560.
- Evidence artifact: 12146760680; ZIP 1972386 bytes; sha256:e461eba8ad7bb9b570e21bed35018b6c409228d854581a39b0f95c0c019d34b8.
- [IPA release](https://github.com/yosnuffyyy/gta5loader/releases/tag/native-0.39): only the IPA is uploaded as a release asset. Previous releases are preserved.

Two full macOS Metal runtime processes pass. New copy coverage: 16 format pairs, 66 copies, 49 exact-byte readbacks in each process; 2 MiB round trip across a 1 MiB tile, small mips, arrays, signed BC storage, source destruction and resource-ID reuse. The batching test checks 128 original-shader draws, all 256 RGBA output pixels, source/destination buffer mutation while the copy is pending, and ordered upload preservation. Invalid copy geometry/families remain rejected. The fatal report now publishes the complete copy descriptors and all 14 command words before the release-store of the error flag; both runtime processes verify that the first acquire-read is complete and remains unchanged through stop/join. Existing depth/stencil, MSAA resolve, SRV/shadow, compute/UAV, occlusion/query, readback and resource-lifetime regressions pass.

| Metric | Legacy process 1 / 2 → retained-encoder process 1 / 2 |
|---|---|
| Actual queue submissions | 388 / 388 → 9 / 9 |
| Draw batches | 128 / 128 → 4 / 4 |
| Fixture wall, ms | 180.134 / 212.243 → 151.198 / 154.071 |
| Inclusive handler wall, ms | 172.398 / 206.719 → 144.585 / 146.634 |
| Uniform upload bytes | 65536 / 65536 → 2048 / 2048 |

Same workload, legacy/new paths in fixed order, two process runs on the CI Apple Paravirtual Metal device; not A18/A18 Pro measurements. Renderer fixture code uses the project's host build configuration; native engine host -O0 versus production iOS -O1. Pixels have the existing one-8-bit-channel tolerance, buffer/copy bytes are exact. Counts are deterministic; wall times include readback/wait/driver noise and are not genuine GPU time or a reliable p95/p99 from two runs. No new active-CPU or device-FPS improvement is inferred.

Additional validation: 4,808 translated shader modules and native pipelines, including 500 clip shaders; 111 untranslated records remain known. 71 production engine-input checks, 197 UIKit checks, 1,374 watchdog assertions, worker/mailbox/allocator/archive/recovery/journal tests, original PCM publication and concurrent ring tests, and production AVAudioEngine offline output (1,024 exact stereo frames) pass. Focused ASan/UBSan and TSan suites pass; this is not full-engine/Metal-driver sanitizer coverage. Original-Wasm timer and population scalar contracts pass, with their limitations described above.

Mixer profiling test: PASS: mixer sampling exact100000 calls/1562 samples; finite clocks; empty sampled-pair overhead 12.320 ns/call (host O2, includes atomics and clocks, not game mixing).. This bounds the empty instrumentation pair on a host, not game-mixer cost or iPhone overhead.

First CI 39148996981 failed the new signed-BC fixture; mappings were corrected and no failing IPA was released. Intermediate full CI 39149966852 passed. Expanded CI 39151304228 then exposed that the isolated CLI replay heap was 4 MiB while the new tiled fixture requires 8 MiB; the CLI-only allocation was corrected without changing any production memory budget. Final CI includes a genuine pending buffer-copy hazard (away from the forced batch boundary) and the 2 MiB tiled round trip. Final review also corrected a race in the new copy diagnostics: appending details after publishing the fatal flag could expose a changing string to engine readers. The append now happens under the error lock before publication, with a focused first-read/stop regression. The superseded build was cancelled; no assertion was weakened to obtain the final result.

Still unverified: exact physical pause-menu recovery; sustained gameplay FPS/input-to-display improvement; corrected animation/physics/script timing; reduced audio starvation, full bank coverage and speech quality; mission-safe ambient presets; A18 hardware performance; long-session memory/thermal effects. These are unfinished work, not passed tests.

## 11. Next physical-device checks

1. Install the new IPA over the existing app, retaining its identity, data and selected game folder. Keep 640×360, 30-FPS cap and the same reference-shadow option for comparison.
2. Mark a visible gameplay window. Open Options, navigate pause-menu pages, close and reopen it several times. Repeat after a scene transition. Verify menu textures and restored scene contents. If it fails, export the report containing both copy descriptors and all 14 words.
3. Compare the same walking/driving route for at least 60 seconds cold, then repeat after 10–15 minutes. Export actual displayed intervals, input-to-hook, transfer savings, submissions, memory and thermal state. Do not compare loading-screen FPS to gameplay.
4. Time an identifiable 10-second dialogue/animation sequence against a wall clock. Listen for pitch changes versus gaps. Export the new mixer wall/CPU samples, consumed/underrun PCM and timer windows. Test speaker/headphones and one interruption/resume.
5. Check mission progression, shadows, character rendering, controller/touch switching and pause input. On base iPhone 16, record separately; no base-device result is inferred from Pro/host tests.

## Compact Report for ChatGPT

Native 0.39 validated source `68c1394b3d067e73aacb17bd3d7fe39091708315`, baseline native branch `61e271a535c3f951b8f3f8cf048325962cc3bb68`. Full physical 0.38 A18 Pro log SHA256 `11029ef1ae7ecd5cbdca8ade0ece871e5cb67e384d142a1e98078708b4fd0310`, approximately 750 seconds of mixed workload, ending in pause-menu COPY_REGION 34, not a confirmed process-memory kill.

Implemented strict bit-preserving D3D-compatible texture-copy casting through a bounded 1 MiB GPU tile, compressed-block/small-mip/array checks, signed BC4/BC5/BC6 mappings and complete fatal copy descriptors. Captured fatal IDs 35378←10933 and shader hashes are known; exact original texture descriptors are absent, so actual menu recovery remains device-pending. Unsupported multisample copies still fail; existing opcode 36 resolve is retained.

Implemented legal color/depth-clear and region-copy retention in the current draw encoder, maximum 64 transfers, correct render-pass/query boundaries and source/destination buffer upload-epoch hazards. No relaxed validation, CPU readback, extra completion wait or unbounded staging. Physical 0.38 clear 5/clear 6/copy 34 totals: 312,837 clears + 156,505 region copies; 683,204 nested submissions; 213,863 preflushes. Inclusive wall costs 55.805 s + 68.784 s + 80.771 s across the run. These are not independent GPU times or guaranteed savings.

Physical 0.38 latest 256 displayed intervals: mean 39.522 ms, p50 33.336, p95 50.005, p99 66.673; mixed phases, not sustained 25.3 FPS gameplay. Service CPU 27.354 ms/PRESENT; handler wall/active 35.455/24.847; upload wall 2.031 (overlap). Lifetime 1,141,628 submissions, 9,678,635 draws, 581,875 draw batches, 14,929,971 buffer uploads, 95,983 texture uploads, 29,198 staging waits, 48,918.642 ms completion wall. Existing dependent-buffer flushes only 4; preserve the 0.38 optimization. Uniform traffic 22,877,379,840 bytes, packs/reuse 10,490,491/8,433,029. Pipelines 970 creates / 9,677,665 hits / 714 evictions, capacity 256, worst 412.559 ms. Input hook 44.707 ms mean / 467.989 max. Serious thermal; recent footprint ~2.3–2.5 GiB; logical GPU peak 577,660 KiB is not additive.

Original timer caps delta at ~66.667 ms: deterministic 10 real seconds at 5/10/15/20/30 updates/sec → 3.333/6.667/10/10/10 engine seconds. Phone: 146 moving timer windows, 734.772 real → 683.510 game seconds. No unsafe cap removal/global speedup. Physics/script/animation substep correctness is still missing. Audio: 24,579,840 consumed + 11,422,720 missing PCM frames, 31.7275% underrun at 48 kHz. Existing stereo output retained; new 1-in-64 original MixBuffers wall/thread-CPU sampling, fixed storage, unchanged PCM/callback/scheduler. No sound-quality improvement yet claimed.

Density original-function tests verify proposed multipliers and stricter script minima, but frame-reset placement and mission-required ambient traffic safety remain unverified; no production preset shipped. No O2/O3 claim or quality/frame-cap change. Actual GPU execution, sustained 0.39 FPS, full menu behavior, corrected cutscene/audio timing and base A18 results remain UNKNOWN. Final CI 39152842578 passed all applicable checks after the first signed-format mapping failure was corrected. IPA 0.39.0/build39, 104978601 bytes, SHA256 99b1f41e8f3a76d81e06aa844c813baf9793ffe667484b6af16a85b2455f4571. Next priorities: verify copy/menu and batching gain on phone; use mixer/scheduler evidence to fix starvation; establish safe simulation substeps and population timing without mission regressions.


Final host comparison: submissions 388 / 388 → 9 / 9, batches 128 / 128 → 4 / 4, uniform bytes 65536 / 65536 → 2048 / 2048; wall ms 180.134 / 212.243 → 151.198 / 154.071. Two equivalent 128-draw pixel/byte-checked processes, not iPhone FPS. All 4,808 shader/pipeline checks and targeted sanitizer/input/UI/memory tests passed. Release: https://github.com/yosnuffyyy/gta5loader/releases/tag/native-0.39 .
