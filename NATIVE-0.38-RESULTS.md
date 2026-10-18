# Native 0.38 engineering results

Status: independently validated unsigned ARM64 build 0.38.0/38, source `360bb27d7c3e3750611bebcfdc6adf00c98303e8`, CI [39141219526](https://github.com/yosnuffyyy/gta5loader/actions/runs/39141219526) PASS. This is not a claim of physical iPhone FPS, audible game sound, fixed animation scheduling or completed missions in 0.38. The scheduler fix remains unfinished; native audio and ordered upload batching are implemented and component-tested.

## 1. Revision, workload and timing

Baseline branch revision: `6f99da290b4307569b5503a4f4c38e094f598447`; shipped 0.37 executable source: `61f2bfa21812bfd4b50b64342b57a9a6397f71ec`. The full physical 0.37 log was found in Downloads, `2GTA-Native-Log-E73B02D5-F995-4048-A5AC-250123551D43.txt`, 11,769,116 bytes, SHA256 `b42db8e8133761b7ac856e25943911bcaf44e83d17a0c9ebed19df1b67106b58`. Its raw contents are kept local. The 0.36 CPU report is not a matching 0.37 profile and is not used as a new measurement.

The original pinned engine (`e65bec476e8fd8125515150c24037c9bd0d0c46530e3544f9d389ab108b5592e`) converts host milliseconds to nanoseconds in function 2809, `rage::sysTimer::GetTicks`. The native host clock uses monotonic elapsed time. No incorrect conversion was demonstrated. Function 8924, `fwTimer::Update`, clamps its normal delta to approximately 1/30..1/15 seconds. Executing that unchanged function with a controlled clock reproduces lost simulation time below 15 updates/s:

| Timer cadence | Real elapsed | Sum of engine deltas | Integer game timer |
|---|---:|---:|---:|
| 5/s | 10 s | 3.333333507 s | 3,300 ms |
| 10/s | 10 s | 6.666667014 s | 6,600 ms |
| 15/s | 10 s | 9.999999404 s | 9,900 ms |
| 30/s | 10 s | 10.000000522 s | 10,000 ms |

The integer timer is separately quantized; it is not the delta sum. Script pause/resume also passes. These are original-Wasm timer tests, not a rendered cutscene fixture. The test cadences represent timer-call rates; a displayed FPS does not independently prove the same simulation cadence.

**Slow-motion scheduling remains unresolved.** Removing the clamp or injecting extra main-loop/physics updates is not verified safe. The engine's main/render dependency and physics/cutscene substep contracts must be established before introducing catch-up. No global animation multiplier, time-scale change, second simulation thread or fake interpolation was added. New five-second observations at the real engine pad boundary log monotonic elapsed time, game/unwarped milliseconds, hook count, delta bounds, forced delta and pause flags. Clip time, physics advancement and end-to-end audio/video synchronization still need physical traces. The 30 FPS cap, `-frameLimit=2`, VSync setting and 640×360 baseline remain unchanged.

Files: `native-ios/timing/test-timer.cjs`, `native_time_trace.h`, and `threading/native_threads.c`.

## 2. Native audio architecture and asset limits

The original `audMixerDeviceWasmDiscard::MixThread` (7394) is not always a discard-only path. With a fresh consumer heartbeat it calls the original mixer, copies 256-frame planar L/R blocks into the published interleaved float32 ring and advances the write cursor. Without a consumer heartbeat it continues its headless scheduling path. Previously the native host recorded the ring address but supplied no playback consumer. Original engine strings mentioning WebAssembly/AudioWorklet or a headless mixer are retained; the new native PCM counters and actual listening, not those historical labels, distinguish output behavior.

New path: existing asset lookup/decoder/mixer → original 48-kHz stereo ring → `AVAudioSourceNode` → `AVAudioEngine` main mixer/output → iOS audio session. The original engine selects its stereo mixer mode; multichannel decoding/downmix remains the engine's responsibility. No Opus decoder, waveform, bank identity or compressed data is replaced.

The original main (103), executed with a test export and a deliberate stop at its audio-publication boundary, initializes capacity 16,384 and publishes address 8,153,472. The ABI has a 64-byte header, stereo frame cursors and 131,072 bytes of PCM. The native bridge checks the full 131,136-byte extent, alignment, capacity and initialized header. It reuses that ring instead of allocating a second PCM queue.

The callback does not allocate, lock, log, perform I/O or wait. Acquire/release cursors preserve producer/consumer ordering and uint32 wrap. It consumes available original samples, zero-fills genuine underruns without advancing over unavailable data, records invalid occupancy/nonfinite samples and never fabricates decoded assets. The callback context is process-lived; disabling and draining callbacks precedes engine-memory detachment. AVAudioSession activation, preferred rate/buffer duration, interruption and media-service recovery run on a serialized control queue. Graph configuration changes rebuild/restart output as appropriate. Stop/background handling releases playback safely. System output volume and original mixer gains apply; no separate new in-app volume slider was added.

Verified components: original engine publish ABI; concurrent PCM ring; exact offline stereo output through the production source callback; iOS API compilation. The final callback microbenchmark on the macOS ARM64 CI host processed 10,000 blocks of 256 silent fixture frames in 5.966 ms wall (0.597 microseconds/callback, equivalent to 0.112 ms of callback wall work per second of 48-kHz audio). This O2 fixture excludes decoding, AVAudioEngine scheduling and iPhone behavior. It is not an audio latency measurement. These tests do **not** establish actual audible game assets. Device speaker/headphone routing, interruption recovery, decoder coverage and lip-sync remain pending.

`update:/x64/audio/sfx/RESIDENT\\LOW_LATENCY.awc` still fails in the supplied log. Other actual audio archives are requested. The phone's mounted archive contents are unavailable for content-identity verification here, so no bank substitution or success suppression was introduced. Dialogue/effects/vehicles/ambient coverage cannot be promised until nonzero real PCM and audible output are checked.

Files: `native-ios/audio/*`, `threading/native_threads.c`, `native_session_bridge.inc`, build/link scripts.

## 3. Ordered buffer mutations

0.37 already snapshots constant-buffer uniforms and safely batches independent resource updates. This is preserved, not counted as new work. Its remaining vertex/index/storage updates can force submission of an unsubmitted encoder that already uses the destination. The supplied log records 1,292,713 such mutations.

0.38 adds immutable batch-local staging for aligned buffer updates to destinations already referenced by the pending encoder. Independent destinations retain the previous direct queue-write path and avoid an unnecessary extra copy. CPU payloads are copied into a bounded arena. At the original mutation position the render pass ends and a buffer-to-buffer copy is encoded. Before submission, one queue write fills the staging buffer; GPU copies then execute between the exact draws that bracket each mutation. Later draws keep the same destination resource and see the correct version by command order. GPU-written portions outside each update are preserved; the code never clones a possibly stale CPU mirror.

There are two 512-KiB GPU staging slots and one 512-KiB CPU arena, allocated on first use. Arena rollover submits before reuse. Queue ordering protects slot reuse across submissions. The existing 16-batch/4-MiB queued staging-pressure policy includes these new bytes. Unaligned/large/invalid updates retain the existing strict update path. Encoded writes mark the destination's pending epoch so a later direct queue update cannot overtake them. Destruction, queries, readbacks and other synchronization boundaries remain enforced.

Render-pass boundaries are preserved; this is not a render-pass-merging or draw-skipping optimization. Protocol upload counts and actual mutation bytes need not fall. The intended savings are fewer submissions, repeated uniform uploads after flushes, and staging-pressure waits. A real-device gain remains unmeasured.

Files: `renderer/ordered_upload.inc`, `native_gpu.m`, `native_draw.inc`, `native_metrics.inc`, `ordered_fixture.inc`; the historical `submission_fixture.inc` explicitly runs its old comparison mode so earlier results are not relabeled as new improvements.

## 4. Submission, upload and timing measurements

Physical baseline, final 256 samples from the unclassified 0.37 window:

| Metric | Mean | p50 | p95 | p99 | Max |
|---|---:|---:|---:|---:|---:|
| Actual display intervals, ms | 81.160 | 66.673 | 116.677 | 133.346 | 133.346 |
| GPU-service thread CPU/PRESENT, ms | 51.888 | 46.033 | 72.676 | 78.071 | 88.085 |
| Command-handler wall/PRESENT, ms | 76.735 | 64.424 | 110.429 | 123.157 | 129.880 |
| Upload-handler wall/PRESENT, ms | 46.666 | 34.783 | 75.803 | 80.392 | 95.546 |
| Individual completion wait, ms | 1.340 | 1.315 | 1.428 | 1.511 | 3.040 |

Categories overlap; do not add them. The display window corresponds to about 12.3 interval-FPS, not classified sustained gameplay FPS. Whole-run counters: 8,163 protocol PRESENTs, 5,851,813 draws, 1,599,403 draw batches, 1,898,619 submissions, 8,689,429 protocol buffer uploads, 15,600,164,608 uniform-upload bytes, 96,210 staging-pressure waits and 129,899.077 ms cumulative completion wait. These are a baseline, not 0.38 improvements.

The host Metal replay passes in two independent runtime processes. It checks 256 original-shader pixel versions (maximum one 8-bit channel unit tolerance), exact final vertex bytes, 256 occlusion samples, staging rotations, 64-KiB byte contents, subsequent unaligned writes and destroyed/recreated resource IDs. A 257th degenerate draw establishes a real vertex-buffer dependency without adding query samples; an independent buffer's direct update is also read back. Both comparison paths use the same command stream and controlled drain-policy clock. The native host executable uses the established O0 build; iOS remains O1, and the pinned Rust Metal backend uses its release build. Host wall/active CPU includes instrumentation and is not A18 Pro gameplay performance.

| Same replay | Previous path | Ordered copies | Provenance |
|---|---:|---:|---|
| Queue submissions | 262 | 13 | Identical in both processes; 95.0% fewer |
| Draw batches | 256 | 7 | Identical in both processes |
| Uniform GPU bytes | 131,072 | 2,560 | 98.05% fewer; original draw constants retained |
| Explicit completion calls | 17 | 1 | Both processes |
| Added ordered copies/bytes | 0 / 0 | 1,540 / 1,316,800 | Mutation payload preserved, not eliminated |
| Total wall, ms | 1,051.813 / 1,035.198 | 238.193 / 203.961 | Process 1 / 2 |
| Handler active CPU, ms | 112.877 / 108.661 | 18.393 / 19.849 | Excludes driver workers and GPU execution |
| Drain residual CPU, ms | 0.759 / 0.818 | 0.686 / 0.693 | Separate profiler category |
| Handler wall, ms | 1,048.401 / 1,025.698 | 181.991 / 149.954 | Includes nested waits |
| Upload handler wall, ms | 208.663 / 196.971 | 19.422 / 16.513 | Subset of handler wall |
| Completion wait wall, ms | 140.617 / 135.377 | 0.002 / 0.002 | Subset of wall categories |

For the last 256 DRAW handlers, process 1 p50/p95/p99/max changed from 0.009/18.689/19.004/24.566 ms to 0.002/0.018/1.655/21.195 ms; process 2 from 0.009/18.121/19.167/23.213 to 0.002/0.015/2.013/22.905 ms. These are command-handler distributions, not displayed-frame percentiles. No profiler clock failures or residual underflows occurred. Before then after is a fixed order; driver cache warmth and shared CI scheduling can affect timing. The deterministic submission/byte counts and checked output are stronger evidence than a general speedup extrapolation. This synthetic workload deliberately exercises frequent mutable vertex data and is not a full Prologue trace.

## 5. CPU execution and compiler settings

The engine remains AOT-translated native ARM64 C. No JavaScript engine runs gameplay. No blanket O2/O3, unsafe floating-point flags, ThinLTO or PGO was introduced without representative CPU workload evidence. The existing engine flags and object-cache integrity checks remain intact. The new replay reports handler active CPU separately from wall time and waits; it does not measure driver worker threads or GPU execution.

## 6. Pipelines and shaders

The 0.37 bounded 256-entry hot-cache policy, exact pipeline keys/fingerprint lookup, inter-stage linking, clip distances, SRV 5633 correction, inactive specialization and optional reference-shadow mode are preserved. Baseline 685 creations / 5,851,128 hits / 429 evictions is distinct from the 0.36 pressure-collapse workload. No unsupported Metal binary archive or asynchronous shader compilation was invented. All 4,808 translated modules and native Metal pipelines pass, including 500 clip shaders and both previously failing original shadow shader pairs. The preserved 192-key, 576-draw working-set replay has 192 creations/384 hits/zero evictions at capacity 256. 111 previously untranslated shader records remain a known limitation.

## 7. Memory and thermal behavior

0.37 final sampled footprint was 1,858.8 MiB, sampled peak 1,889.0 MiB; engine heap committed 1,398.1 MiB; recent OS headroom 1,517.2 MiB. These overlap and are not additive. Logical engine GPU payload peak was 366,849 KiB, excluding driver overhead and intermediates. The run ended through user stop, Exit 0; cleanup footprint 117.9 MiB. Serious thermal state was reported. No new 0.38 physical memory/thermal result exists.

New explicit upload storage is bounded at 512 KiB CPU + 1 MiB GPU; driver staging/residency remains opaque and already bounded by the queue-pressure policy. PCM reuses the original 131,136-byte engine ring, with an 88-byte tested native context plus AVAudioEngine/framework-owned storage. It is not a measured net memory reduction. Existing recovery snapshots, autorelease-pool bounds, archive reader (one 64-KiB buffer/eight descriptors), allocator fixes and resource retirement remain. No arbitrary heap/budget growth was added.

## 8. Input responsiveness

Baseline event-to-engine-hook mean/max is 74.840/793.156 ms, not event-to-displayed-response latency. Existing latest-analog/ordered-digital delivery, DualSense/touch switching and fullscreen coordinates are unchanged. Fewer graphics stalls may let the engine hook run sooner, but that benefit is not claimed without a device trace. No unsafe asynchronous writes to engine input or independent busy loop was added.

## 9. App identity, artwork and UI

Display name: **GTA V (Test Build)**; bundle identifier remains `io.github.yosnuffyyy.gtanativeprobe`; executable remains `NativeProbe`; packaged version/build verified 0.38.0/38. Folder bookmarks, preferences, saves and diagnostic locations are preserved. User artwork SHA256 `45ee3ae0d618e7e51c1ed5328fe77d4025f1572b27cbc107eacf89b3d4ad2385` is pinned before deterministic conversion. The build generates 18 opaque iPhone/iPad/1024px marketing entries and compiles an AppIcon catalog using actool. Assets.car and icon plist declarations are present; the extracted iPhone icon was visually inspected. Actual installed Home Screen/Settings appearance requires an iPhone. Existing dark UI, touch/controller controls and file-backed share-only diagnostics remain.

## 10. Regression and sanitizer results

Initial candidate `b4a774591748d85f74f4585124ff440cbbed152f`, run 39138760839: PCM ASan/UBSan + TSan, offline AVAudioEngine and timer tests passed; build stopped because the isolated watchdog bridge test lacked the newly introduced audio-suspend callback. The fixture now records/asserts that lifecycle notification. A fixture-only atomic alignment warning was corrected with explicit 8-byte alignment. Neither failure was hidden.

Second candidate `02479a8d020ae144e44f187c7c75e85f141d83ea`, run 39139029435, passed the early regressions but stopped when the integrated host compiler rejected loss of the shared engine memory's `volatile` qualifier. The PCM interfaces now retain both `volatile` and atomic access end to end; no cast suppresses this check.

Third candidate `d81938b2e2dd19274ab493f540d71c843586898a`, run 39140114968, exposed an old uniform-retirement fixture assumption: a buffer mutation no longer forces its intended 96 batch boundaries. The fixture now issues an explicit legal GPU copy to preserve that workload. Every prior pixel, query, pressure, completion and retirement assertion remains. This is a test boundary correction, not relaxed correctness. Independent buffer writes were also kept on the prior fast path rather than adding needless copies.

Final source `360bb27d7c3e3750611bebcfdc6adf00c98303e8`, run 39141219526/job117715877470: **PASS**. All 265 local native/workflow/manifest blobs match the remote tree. Two complete runtime processes, persistent saves, 4,808 shader pipelines, 30 SRV contract cases/process, counting queries, compute, depth/clip, original shadow pixels, buffer ordering, allocator/streaming, worker/mailbox, 1,374 watchdog assertions, 71 actual engine input checks and 197 UIKit checks pass. PCM ring ASan/UBSan and TSan, 20,000 concurrent ordered PCM frames, original publish ABI and 1,024 exact offline AVAudioEngine frames pass. Existing targeted memory/input/session/logging sanitizer suites also pass. The entire game/Metal driver is not sanitizer-instrumented. Actual game asset decoding and physical interruptions are not established by fixture PCM.

## 11. IPA and release

Validated IPA: `GTA-Native-Probe-0.38-unsigned.ipa`, 104,980,739 bytes, version 0.38.0/build38. SHA256: `fe019a129b4ad68459347a6744d14fd78f8d1acc5c2f7e22309dbbba7356ea04`. Source `360bb27d7c3e3750611bebcfdc6adf00c98303e8`; CI39141219526, evidence artifact12142645751, IPA artifact12142640834. Archive digests checked before extraction. Audit confirms unsigned ARM64 iPhone executable, no writable-executable segment, no WebKit, unchanged install identity, original engine hash and pinned backend, linked AVFoundation/AudioToolbox, compiled icons, and no CI shader packs/web/game assets in the payload.

All 37 prior local IPAs are hash-identical. Native changes stay on `native-ios-prototype`; `main` remains `de7664a8e6448096716c5a341250d32d4f594fd1`. Release location: [Native 0.38](https://github.com/yosnuffyyy/gta5loader/releases/tag/native-0.38). Only the IPA is attached; no web/game files or private raw reports.

## 12. Device verification and remaining work

Install over the same app identity, retain 640×360/30-FPS and the same shadow compatibility setting. Mark visible gameplay, compare matching Prologue and Los Santos sections, then share the full text log. Check original actors/materials/shadows, weapon fire, driving and scene transitions. Compare ordered-copy counts, pending mutation flushes, submissions, staging waits, uniform traffic, actual display percentiles, memory/headroom and thermal state. An improvement in this host fixture is not a promised phone FPS gain.

Check dialogue, gunshots, footsteps, engines and ambience through speakers and headphones. Confirm the report shows running output, nonzero PCM, bounded queue depth and sensible underruns. Background/resume and interrupt playback; watch for stale or stuck audio. Share output errors and remaining bank failures. Nonzero PCM is not proof of speaker audibility; a test tone would not prove game audio.

Compare a ten-second wall-clock cutscene interval against the new engine timer observations. Slow-motion scheduling, independent clip/physics timelines, actual GPU timing, end-to-end input latency and sustainable 30-FPS performance remain open. No optional lower-quality preset or below-baseline resolution is used to mask these gaps.

## Compact report for ChatGPT

Native 0.38.0/build38 source `360bb27d7c3e3750611bebcfdc6adf00c98303e8`, CI39141219526 PASS, based on shipped 0.37 source `61f2bfa21812bfd4b50b64342b57a9a6397f71ec`. Unsigned IPA104,980,739bytes, SHA256`fe019a129b4ad68459347a6744d14fd78f8d1acc5c2f7e22309dbbba7356ea04`, release native-0.38. Target: iPhone 16 Pro Max/A18 Pro, iOS 27.0.1, 640×360, capped 30 FPS. Full supplied 0.37 log SHA256 `b42db8e8133761b7ac856e25943911bcaf44e83d17a0c9ebed19df1b67106b58`, 11,769,116 bytes. It ends by user stop, Exit 0. Final 256 display intervals mean/p50/p95/p99/max 81.160/66.673/116.677/133.346/133.346 ms (~12.3 interval-FPS, phases unclassified). Service CPU mean51.888ms/PRESENT, handler wall76.735ms, upload wall46.666ms overlap. True GPU time UNKNOWN.

0.37: 1,898,619 submissions; 1,599,403 draw batches; 1,292,713 mutation flushes; 8,689,429 buffer uploads; 15,600,164,608 uniform bytes; 96,210 staging-pressure waits totaling129,899.077ms completion wall. Pipelines685/5,851,128/429 creates/hits/evictions, capacity256. Input event→hook74.840ms mean/793.156ms max. Final footprint1858.8MiB, heap1398.1MiB, headroom1517.2MiB; serious thermal. Counters are overlapping, not additive. No 0.38 device measurement yet.

New implementation: immutable ordered upload arena (512KiB CPU +2×512KiB GPU), encoded copies between original draws; preserve GPU-written untouched bytes and all pass/query/readback/lifetime boundaries. Queue ordering allows staging reuse;16-batch/4MiB pressure cap includes new bytes. Two host Metal processes pass257draws/256checkedpixels+occlusion,64KiB contents, arena rotation, unaligned writes, independent updates and object reuse. Same-workload submissions262→13; batches256→7; uniform bytes131072→2560; completions17→1. Wall1051.813/1035.198→238.193/203.961ms; handler active CPU112.877/108.661→18.393/19.849ms; upload wall208.663/196.971→19.422/16.513ms. Categories overlap. New1540copies/1316800bytes retain exact mutation data. HostO0, pinned release Metal backend, fixed before/after order; cache warmth/CI noise affect timing. Not a full mission trace or device speedup claim.

Audio: original function7394 already produces48kHz stereo PCM when a consumer heartbeat exists. New native AVAudioSourceNode/AVAudioEngine consumes the original16384-frame ring, preserving decoder/mixer. Original main publication ABI verified. ASan/UBSan/TSan ring tests and1024 exact offline AVAudioEngine output frames passed. Physical game sound, routes, interruptions, bank coverage and lip-sync remain pending; missing LOW_LATENCY.awc is not substituted.

Timing: original fwTimer::Update8924 clamps normal dt to33.33..66.67ms. Actual unmodified Wasm tests over10s real time advance3.333/6.667/10/10s at5/10/15/30 calls per second. Pause/resume passes. This proves a slowdown mechanism, not every clip's cause. Scheduler fix remains unresolved because removing clamps/extra physics ticks lacks validated semantics. New5s owner-hook clock trace; no time-scale or game-logic change.

Existing0.37async queries/hot cache/input/recovery fixes retained, not counted as new. AOT flags unchanged; no unsupported direct-Metal path, quality reduction or new preset. App renamed GTA V (Test Build) with pinned supplied artwork; bundle identity preserved. Final regression:4808modules/pipelines including500clip, two full runtime processes,71engine-input/197UIKit checks,1374watchdog assertions and targeted ASan/UBSan/TSan pass.111untranslated records remain. All37priorIPAs preserved. Next work: validate new graphics/PCM path on device, establish safe simulation scheduling, obtain genuine GPU and active-engine profiles. Stable30FPS and16.67ms processing headroom remain unproven.
