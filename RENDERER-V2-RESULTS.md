# Renderer V2 — Native 0.40 validation record

**Validated experimental test build; not promoted. Existing renderer remains the default. No iPhone performance gain or complete Renderer V2 replacement is claimed.**

## Source and preserved baseline

The working native head was `19b1c9bbf770885b0ecdfc27093648c07ba6505b`; Native 0.39's executable source was `68c1394b3d067e73aacb17bd3d7fe39091708315`. Work is isolated in `renderer-v2-experimental`. Deployed main remains `de7664a8e6448096716c5a341250d32d4f594fd1`. All 39 existing local IPAs were inventoried by SHA256 and preserved.

Candidate executable source: `e14b5884feee5dc5d39f550899e4c797f168953d`, version `0.40.0`, build `40`. [Release-candidate validation run](https://github.com/yosnuffyyy/gta5loader/actions/runs/39161149752). Source, build flags and dependencies are pinned in source-manifest.json and the existing backend records.

## What changed

This is milestone B: a bounded completion-service implementation around the measured END_QUERY hotspot. The original frontend, shader translation, resource registry, immutable uniforms, ordered updates, copies, draw/pass scheduler and resource retirement are reused. It is not a full backend replacement.

- `completion_serial.h`: fixed 128-ticket pool, free lists, queue-work serials and success-only completion high-water mark. Equivalent fences share one callback. A later upload invalidates the completed-fence fast path.
- `completion_service.inc`, `event_queries.inc`, `native_gpu.m`: generation/token-checked result publication shared by both execution modes; bounded fan-out; actual GPU completion; cancellation and errors remain failures. Type 2 retains the existing CPU timestamp compatibility value and is never labelled GPU timing.
- Queue-write wrappers track pending buffer/texture writes. V2 submits those writes before event or plain protocol FENCE completion. The pinned backend does not do this automatically. This correctness repair is distinct from the matched performance test.
- UI and launch bridge: **Graphics → Renderer V2 · experimental, next session**, off by default, persisted, disabled during an active session. No duplicate GPU resource sets. Restore Defaults selects the existing renderer.
- Diagnostic counters identify END types, registrations, joined/known-complete replies, pending-write fences and bounded reply latency. New benchmark and fault fixtures exercise the shared production path.
- Packaging preserves a UUID-matched symbol map and compiler settings in validation evidence. The app identity, icon, game-file bookmarks and save locations stay unchanged.

No shader semantics, texture contents, reference-shadow policy, engine timing, PCM output, input ordering, memory budget, 640×360 baseline, 30 FPS cap or pipeline-cache capacity were changed. Prior improvements are not counted as new V2 gains.

## Native 0.39 device evidence

Input: `4GTA-Native-Log-CFB3AF82-67E7-43FB-8C27-B37D7616D34A.txt`, 12,425,338 bytes, SHA256 `c625c739c067aa947f144c4c409af79381d618a89852fd2764d76f6771191276`. Final cumulative rows were extracted once; repeated report/preview text was not summed. The run ended through user stop, Exit 0. There is no marked gameplay window.

| Measurement | Value / interpretation |
|---|---|
| END_QUERY | 603,610 calls = 597,424 event/timestamp + 6,186 occlusion ENDs |
| END_QUERY cost | 126,500.105 ms inclusive wall; 103,509.695 ms inclusive active service-thread CPU; 104,461 nested submissions |
| DRAW cost | 4,852,654 calls; 52,682.926 ms wall / 46,084.351 ms active CPU |
| READBACK cost | 16,555 calls; 36,546.200 ms wall / 10,721.343 ms active CPU |
| Total submissions | 269,770; not frames |
| Final 256 actual display intervals | mean 61.383 ms; p50 50.005, p95 100.009, p99 116.677, max 583.387 ms; unclassified phases |
| Final 256 service-thread CPU samples | mean 46.779 ms/PRESENT; p50 34.809, p95 79.336, p99 84.973 ms |
| Final handler/upload windows | mean 50.080 / 2.564 ms/PRESENT; overlapping costs |
| Pipelines | 606 creations, 4,852,048 hits, 350 evictions; bounded 256 entries |
| Uniform traffic | 10,770,255,360 cumulative bytes, not resident memory |
| Memory | final sampled footprint 2,097.7 MiB; highest 2,378.9 MiB; committed engine heap 1,432.1 MiB; sampled headroom 1,278.3 MiB. Do not add these categories. |
| Input | event-to-hook mean/max 45.803 / 552.633 ms over 1,543 measured consumptions; not input-to-display latency |
| Thermal / GPU time | serious thermal state; genuine GPU execution duration UNKNOWN |

END_QUERY's CPU attribution includes draw finalization, submission, callbacks and profiler overhead. Counts do not prove that all calls target identical fences. Extremely short opcode CPU and wall intervals differ because clock sampling boundaries differ; see the design document. No exclusive driver-cost estimate is invented.

## Verification and matched comparison

Release-candidate CI `39161149752` completed successfully, including ARM64 packaging. The downloaded artifacts were checked against GitHub size/SHA256, safely extracted, and independently audited. All 275 source-manifest entries match the executable source tree. The 39 previous local IPAs still match their saved hashes. The initial same-source run `39158844413` passed both host processes, all 32 matched replays, and all 4,808 shader pipelines. It was cancelled during cold iPhone compilation after the identical source was queued on `native-ios-prototype`, whose verified 0.39 compiler cache was restored. Its benchmark is retained, but its cancelled workflow is not a release validation pass. The first candidate failed early on a UI log-message scope error, which was corrected before packaging. Superseded validation runs were cancelled when the pending-write FENCE contract and early syntax checks were added; these cancellations are not test passes.

The new comparison uses original startup shaders with a **synthetic** sequence: 64 draws, 513 event ENDs, 256 pixel checks, query types 0/2/3, repeated tokens and replaced resource IDs. It is not a captured phone command stream. Four alternating-order pairs run with profiling off and on, in each of two independent native processes: 32 replays. Device/service objects are new per replay; warmed process/driver caches may persist. Replay timing excludes service initialization and cleanup. The CI adapter identifies itself as **Apple Paravirtual device**; this is hosted Metal validation, not physical A18 Pro GPU profiling. The existing build policy uses `-O0` for the macOS native harness and `-O1` for the iPhone build, with the pinned Rust backend built in release mode. Host timings must not be projected onto ARM64 iPhone execution. There is no outlier removal. Both candidates include the new bounded query-latency instrumentation; this is a matched execution-mode comparison within 0.40, not a binary-level timing comparison against the untouched 0.39 release.

The parser requires successful results, matching draw/submission counts and correct alternating order. Wall time, inclusive command/END CPU, callback counts and query readiness are reported separately. Profiling-off CPU is UNKNOWN. A parser-only test checked arithmetic and rejected incomplete, failed, unequal-work and wrong-order inputs; its fabricated inputs are not performance evidence.

Reproduce on the supported macOS build environment:

```sh
bash native-ios/ci-convert.sh
node evidence/renderer-v2/compare.mjs native-build/evidence/native-engine-test.txt native-build/evidence/renderer-v2-benchmark.json
```

## Initial host comparison (run 39158844413)

All 32 correctness replays passed. Each candidate had 64 draws and 65 submissions. Callback registrations fell from 513 to 64 (87.5%), with 442 joined replies and 7 already-completed replies per V2 replay; pool peak was 128 and compiled pool size 9,256 bytes. This does not prove a comparable CPU saving.

| Metric | Existing median | V2 median | Paired median change |
|---|---:|---:|---:|
| Profiling off, replay wall | 82.035 ms | 75.650 ms | -8.155% |
| Profiling on, replay wall | 83.129 ms | 91.244 ms | -0.991% |
| Profiling on, all command CPU | 14.167 ms | 14.433 ms | +2.462% |
| Profiling on, END CPU (overlaps preceding row) | 10.966 ms | 11.183 ms | -2.322% |
| Profiling off, query p95 | 10.996 ms | 11.236 ms | not aggregated as a paired ratio |
| Profiling on, query p95 | 11.109 ms | 11.159 ms | not aggregated as a paired ratio |

Medians of separate sets and the median of per-pair percentage changes are different statistics; they can disagree in direction. Both are retained. Some pairs regress substantially. The profiled wall median rises approximately 9.8%, which requires review under the provisional 5% threshold; the unprofiled improvement does not cancel that concern. There is no supported 25% dominant-CPU reduction or reliable iPhone FPS improvement. The next identical-source run is retained separately to expose run-to-run variability. No outliers are removed and no promotion is justified.

## Release-candidate repeat (run 39161149752)

All 32 matched replays and both full runtime processes passed. This second run is retained separately from the initial run; no slower result was discarded. All matched replays retain 64 draws and 65 submissions. V2 uses 64 registrations, 442 joined replies and 7 already-complete replies versus 513 registrations in existing mode. Pool storage is 9,256 bytes; peak outstanding replies are 128. Submission count is unchanged.

| Metric | Existing median | V2 median | Paired median change |
|---|---:|---:|---:|
| Profiling off, replay wall | 77.226 ms | 78.630 ms | -1.888% |
| Profiling off, query p95 | 10.646 ms | 9.703 ms | not paired |
| Profiling on, replay wall | 81.404 ms | 93.128 ms | +17.891% |
| Profiling on, all command CPU | 12.130 ms | 12.352 ms | +1.626% |
| Profiling on, END CPU (overlaps command CPU) | 9.265 ms | 8.944 ms | +2.300% |
| Profiling on, query p95 | 11.822 ms | 11.468 ms | not paired |

The repeat profiled paired wall median regressed **17.891%**; its separate-set wall medians also rose 81.404 to 93.128 ms (about 14.4%). The first run also had a greater-than-5% separate-set profiled wall regression. This is a repeated review blocker, not evidence to promote V2. Unprofiled results are also inconsistent across separate-set and paired summaries.

There are eight pairs per profiling setting in each workflow. The benchmark JSON retains every pair, its order, latency statistics and raw counters. Different individual and separate-set/paired results must not be collapsed into a single promised gain. These runs do not establish a reliable 25% dominant-cost improvement or satisfy device promotion gates. Query latency is the latest 256 END-entry-to-publication samples plus the lifetime maximum; it is not GPU execution time. Profiling-off active CPU is unknown.

## Test and package evidence

- Full native runtime regression suite: two independent processes, existing and V2 selected, passed; 32 matched replays in each of the two workflows (64 total).
- Shader audit: 4,808 modules and pipelines, including 500 clip shaders, zero failures. The 111 untranslated hull/domain/geometry records remain unsupported.
- Both SRV 709 pixel shaders and SRV 5633 contracts retained; 20 neutral-shadow ring cases, 163,840 checked ring pixels, plus 6,144 multi-light pixels. Exact copies, immutable constants, depth, UAV, occlusion and resource reuse checks passed.
- Completion pool: 100,000 deterministic lifetime/failure/reordering transitions under ASan/UBSan; seven callback fault modes; queued-write/event/plain-FENCE progress and exact 64-bit readback. No timeout or fake-ready bypass.
- Input bridge: 71 production-byte checks; UIKit: 201 checks; watchdog: 1,374 assertions. Existing input, session, journal and PCM concurrency tests passed with the applicable TSan builds. The entire AOT engine/Metal driver was not run under TSan.
- PCM ring and offline native output tests passed. They do not prove audible or intelligible game sound on a phone. Original clock fixture still exposes the low-cadence time loss.
- ARM64 iPhone executable, unchanged bundle identity/name/icon, iOS 17 minimum, no signature or provisioning profile, no WebKit/JavaScriptCore or writable-executable segment, no web directory/game archives in the IPA. CI-only benchmark entry points are absent from the device binary.
- Full downloaded evidence, symbol map, UUID, source revision and compiler settings are preserved locally under `outputs/GTA-Native-Prototype/evidence/0.40/`. The symbol map is not a full source-line dSYM.

## Memory accounting

The optional completion pool is fixed at 128 replies and 128 fence records (the C test records its compiled size). `state_bytes` in the benchmark means this pool only. It is not total renderer memory. LP64 field accounting adds approximately 5.1 KiB of fixed context/counter/timing/reply-timestamp storage to both modes, plus approximately 9.0 KiB for the enabled V2 pool. No duplicate texture/buffer resource set or extra frame queue is retained. Driver callback allocations, total native object residency, thermal impact and iPhone process-footprint changes remain UNKNOWN; a lower callback count is not a measured memory-footprint saving.

## Milestones and rollback

Milestone A has a pinned baseline and a first durable regression ledger. Milestone B has a buildable, host-validated completion-service vertical slice. Its broader submission-scheduling portion still needs implementation and validation. Milestone C (broader scheduling/resource execution), D (lower-capability devices), and physical promotion gates remain unfinished. This candidate must not be described as a completed Renderer V2 replacement.

For an immediate rollback, stop the current session, turn Renderer V2 off in Graphics, then start again. The previous 0.39 IPA and its source remain preserved. Selection is frozen while a session is running. The next engineering task is to measure the residual END_QUERY draw-finalization/submission cost after this callback change before considering deferred logical fences; no result consumer may depend on reaching PRESENT.

## Open defects and scope limits

- Native timing fixture still advances 6.667 game seconds over 10 real seconds at 10 Hz due to the approximately 66.667 ms engine delta cap. Safe simulation substepping needs physics/script ordering evidence; no clamp removal or speed multiplier was applied.
- Native PCM output remains present. The 0.39 log reports 4,022,016 missing audio frames. Underrun, speech quality and producer scheduling remain separate unresolved work.
- iPhone 13 is an explicit unverified target. Missing BC support still fails explicitly. A bounded format-conversion path preserving mips, colorspace and copy/readback semantics is not implemented by this milestone.
- Submission scheduling, direct-Metal comparison, broader execution-core changes and phone thermal/residency measurements remain future milestones. V2 currently preserves draw submission boundaries.
- The 111 untranslated hull/domain/geometry records remain unsupported. Successful translation of 4,808 other modules does not establish support for those stages.
- No physical V2 gameplay, input/display latency, sustained memory or actual GPU-time comparison has been collected.

## Exact IPA

- Version/build: **0.40.0 / 40**.
- Executable source: `e14b5884feee5dc5d39f550899e4c797f168953d` (later documentation commits do not change the binary).
- IPA: `GTA-Native-Probe-0.40-unsigned.ipa`, 104,973,130 bytes.
- SHA256: `fd19c2f7078e7e10cac07066d223774633e93215cfeffad9f260266629d9b860`.
- Mach-O UUID: `0ae8b0f255963266a2e0f23b36462d74`; the retained symbol map matches it.
- Build: [successful run 39161149752](https://github.com/yosnuffyyy/gta5loader/actions/runs/39161149752).
- Intended release: [Native 0.40 experimental prerelease](https://github.com/yosnuffyyy/gta5loader/releases/tag/native-0.40), with only the IPA attached. Publication is verified separately from compilation.

## Device comparison protocol

Use the same save/checkpoint, 640×360 profile, shadow option and diagnostic settings. Stop before changing renderer. Run existing → V2, then V2 → existing on a comparable thermal state; allow cool-down rather than comparing a cold first run to an overheated second run. Mark the same visible gameplay phase and share each full text log. Keep loading and cutscene intervals separate.

Check Story Mode movement/combat, pause/map/copies, both character-shadow paths, controller/touch switching, audio continuity, background/resume and clean stop/restart. Compare marked displayed-frame p50/p95/p99, service CPU, END CPU, registrations, reply latency, memory headroom and thermal state. Use profiling-on runs for attribution and profiling-off runs for normal pacing. Repeat a long transition session before any promotion decision.

V2 stays experimental until it demonstrates real device benefit without visual, query-readiness, latency, memory or thermal regression. A synthetic result alone cannot authorize promotion or a 30 FPS claim.

## Compact report for ChatGPT

Executable Native 0.40.0/build40 source `e14b5884feee5dc5d39f550899e4c797f168953d`, successful CI39161149752; SHA256 `fd19c2f7078e7e10cac07066d223774633e93215cfeffad9f260266629d9b860`. This is the first optional Renderer V2 completion-service slice, not a finished replacement. Existing renderer remains default. The source branch is native-ios-prototype; isolated development is renderer-v2-experimental. Main and all 39 prior IPAs were preserved.

V2 shares notifications for identical queue-work serials through a fixed 128-reply pool, publishes only actual successful GPU completion, checks query generations/tokens, and fences unsent buffer/texture writes before event/plain-FENCE completion. It retains draw submission boundaries, original shaders, SRV709/5633 handling, exact occlusion, immutable uniforms, ordered copies/uploads, 256 pipelines, 640x360 and the 30 FPS cap. No simulation, audio, input ordering or memory-budget changes.

Physical 0.39 baseline: 603,610 END_QUERY calls, 103,509.695 ms inclusive active CPU, 104,461 nested submissions; total submissions269,770. Last256 displayed intervals mean61.383/p5050.005/p95100.009/p99116.677ms, but no marked gameplay phase. GPU time UNKNOWN. Final footprint2097.7MiB, peak2378.9, committed heap1432.1, sampled headroom1278.3; these are not additive. Serious thermal state. Input event-to-hook45.803ms mean/552.633max, not input-to-display.

Host comparison uses Apple Paravirtual Metal, -O0 native harness versus -O1 iPhone build, synthetic commands with original startup shaders, not an A18 capture. Two workflows each ran32 matched alternating replays; all64 passed. Each replay:64 draws,65 submissions,256 pixel checks,513 events. Registrations513->64 (87.5% fewer),442 joins+7 known-complete replies; submissions unchanged; fixed optional pool9256bytes plus roughly5.1KiB common context/timing storage. No measured resident-memory saving.

First run paired median changes: profile-off wall -8.155%; profile-on wall -0.991%, all-command CPU +2.462%, END CPU -2.322%. Separate-set profiled wall medians83.129->91.244ms regress about9.8%. Repeat run paired changes: profile-off wall -1.888%; profile-on wall +17.891%, all-command CPU +1.626%, END CPU +2.300%. Every raw pair is retained. There is no supported25% dominant-CPU improvement, sustained iPhone FPS claim, or promotion decision in favor of V2.

Tests:4808 shader/pipelines incl500clip, both known shadow PS, SRV5633, exact copy/UAV/query/lifetime tests,100000 completion transitions, callback faults,71 engine-input/201UIKit/1374watchdog checks, applicable ASan/UBSan/TSan, ARM64 package and source/hash/symbol audits passed. GPU-driver/AOT full-engine sanitizer coverage is not claimed.

Open: draw finalization/submission CPU remains to be separately attributed; no logical-fence scheduling reduction yet. Safe simulation substepping unfinished (10real seconds at10Hz advances6.667engine seconds). Audio underruns/distortion remain (0.39 missing4,022,016frames). iPhone13 BC conversion and111untranslated shader records remain unsupported/unverified. Real GPU time, device memory/thermal/input/audio effects and long Story Mode equivalence need physical testing. Next work: attribute residual END costs, then test a bounded logical-fence scheduler that progresses without PRESENT and preserves readiness deadlines. Keep V2 experimental/off; compare repeated existing/V2 runs at identical checkpoint/settings/thermal conditions before any promotion.
