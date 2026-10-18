# Native 0.37 optimization results

Verification status: **host tests, Metal regressions, CI and unsigned ARM64 package passed**. Physical iPhone 0.37 improvements are not yet measured.

## Baseline and evidence

Native 0.36 device build 36, built from `f30f83723017f2452dbe5660243906dedb425e13`; the working native branch started at documentation commit `51099f42fe09a033a2aec7443c8aa96499699e71`. Mach-O UUID `460455BF-B9CA-3454-B49A-DC81E95790DE` exactly matches the supplied CPU report. Original engine SHA256: `e65bec476e8fd8125515150c24037c9bd0d0c46530e3544f9d389ab108b5592e`. Backend remains wgpu-native v29.0.1.1 with the previously verified Metal patches.

The supplied `1gtalog.txt` is 19,398,084 bytes, SHA256 `016f21d47a8d9130338ec6121ca1ccbc7937226fb9582aba001d80f543a7a6c6`. Analysis extracted 237 report memory samples and avoided counting repeated preview copies as independent samples. The CPU diagnostic is a sampled CPU resource report, **Action taken: none**, not a jetsam report or crash backtrace. Its earlier 170.91-second interval reports 90 CPU seconds, 87 samples with 21 lost, and footprint 1,863.97→2,251.42 MB. It must not be treated as measuring the entire session or the final 3,372.3-MiB pressure period.

The user reports full Prologue, funeral, Los Santos introduction and Franklin/Lamar driving progression. This is device evidence for progression in 0.36, not evidence that the candidate is stable. Reported gameplay approximately 10 FPS; the final 256 display intervals average 158.934 ms, about 6.29 interval-FPS. Loading and cutscenes are mixed unless separately marked.

## Whole-app closure and memory

**Confirmed:** the final logged footprint is 3,372.3 MiB (approximately 3.29 GiB), high-water sample 3,375.7 MiB, committed linear heap 2,100.1 MiB and OS-reported available memory only 3.9 MiB. The last engine allocator message reports malloc arena 2,100 MB / in-use 2,045 MB, game virtual 345/352 MB and resource virtual 1,077/1,088 MB. These are engine counters and can include arena ownership; they do not prove every contained asset remains necessary. Logical GPU payload, engine commitment and footprint overlap and must not be added together.

**Unconfirmed:** jetsam is plausible, but no jetsam record or fatal native exception matching this session was supplied. The CPU report explicitly took no action. No specific multi-gigabyte leak has been demonstrated. The source does not justify forcibly freeing engine allocations, increasing budgets, or declaring memory exhaustion fixed.

Implemented changes:

* A GPU-service autorelease pool ends after each bounded drain/poll iteration. DRAW and compute already had pools; those existing pools are not claimed as new work. This bounds non-DRAW, poll and progress temporaries without releasing explicitly owned WGPU handles or engine memory.
* The progress callback also bounds temporary Objective-C strings. No measured GiB saving is claimed.
* A five-second resource census separates object counts, explicit metadata, CPU buffer mirrors, GPU buffers/textures and shader owners. It excludes opaque driver memory, shader source/variants and engine heap rather than presenting an invented total.
* A fixed 24,640-byte recovery record retains four recent records in each of eight categories. The existing journal writer persists it with a same-directory temporary file, `fsync`, close and atomic rename approximately every five seconds while records arrive and at clean shutdown. A partial temporary file does not replace the previous complete snapshot. This is process-crash consistency, not a guarantee against storage failure or power loss. Full logs still use the existing 2-MiB producer queue and 64-KiB copying/writing blocks.
* Advanced Diagnostics has a Share Recovery Snapshot action alongside Share Full Log. The saved session path is reused after relaunch. Missing snapshots and I/O failures remain visible; no successful termination diagnosis is fabricated.

Remaining P0: collect the new census and recovery file during an extended Los Santos run, and correlate an actual jetsam/native exception report if the app closes. Current changes improve ownership bounds and evidence, but do not establish sustainable memory headroom.

## Five largest evidence-supported costs

Ordering is engineering priority, not a sum of comparable exclusive timings. Audio stack counts, memory pressure and measured handler duration have different units and provenance. No GPU saturation or exclusive engine subsystem timing is inferred.

| Rank | Cost | Native 0.36 measurement | Interpretation |
|---|---|---|---|
| 1 | Graphics-service serialization and completion waits | 1,375,293 completion calls; 538,514.215 ms elapsed wait; latest service thread CPU mean 111.084 ms/PRESENT | `completed()` blocks event-query END; reference replies asynchronously. Wait time overlaps handlers and is not all avoidable. Thread CPU is measured with CLOCK_THREAD_CPUTIME_ID, not GPU execution. |
| 2 | Upload/batch handling | 15,364,985 protocol buffer uploads; 186,564 texture uploads; 392,831.023 ms cumulative upload-handler wall; latest mean/p50/p95/p99 23.804/14.876/38.816/55.244 ms | Handler wall includes nested waits/flushes. Small protocol updates are not necessarily redundant. Existing deduplication remains. |
| 3 | Pressure-induced pipeline recreation | 37,335 creates; 37,207 evictions; 36,017 exact recent evicted-key matches; API 30,358.451 ms total and 516.576 ms worst; link 12,081.171 ms | Cache contracts 256→128. Exact-key recurrence establishes repeated preparation, but full Los Santos working-set size and driver bytes are not captured. |
| 4 | Audio mixing/decoding CPU | Heaviest sampled worker includes 26 samples at `audMixerDeviceWasmDiscard::MixThread`, 15 at MixBuffers/GeneratePcm, 11 at audWavePlayer/Opus update | Inclusive, overlapping microstackshot counts; not exclusive time or whole-session percentages. Native audio output remains incomplete. Do not disable mixing to improve an FPS statistic. |
| 5 | Memory and thermal pressure | Footprint 3,372.3 MiB; 3.9 MiB available; serious thermal state; 114,783 staging-pressure waits | Memory contraction correlates with thrashing. Thermal clock loss, true GPU occupancy and scheduler delay are not measured. |

## Translation and synchronization changes

`renderer/event_queries.inc` replaces synchronous waits for the existing event/type-2/type-3 END_QUERY protocol with bounded queue-completion callbacks. Result values remain the established compatibility values; type 2 remains a CPU timestamp shim and must not be described as GPU timing. Occlusion query behavior is unchanged.

The callback publishes low/high result words before the sequence with release ordering, and wakes the waiting engine address only after actual queue completion. It checks resource ID, generation and End token. Destroy/reuse and later End calls cannot publish stale results into the replacement query. A fixed 128-entry pool applies bounded backpressure with cancellation, failure and timeout handling. No unbounded callback allocation, early result, asynchronous engine-state mutation or premature uniform retirement is introduced. The original draw submission boundary is preserved. Readbacks, explicit fences, 16-batch/4-MiB staging limits and the original data dependencies remain.

The reference `wgpu_worker.js:endQuery/queryStore` queues pending event replies instead of blocking the command worker. WebGPU completion means work submitted before that call has completed ([GPUQueue API contract](https://gpuweb.github.io/types/interfaces/GPUQueue.html#onsubmittedworkdone)); it does not measure GPU duration.

Tests include real original-shader draws, pixel readback, all three existing result types, repeated Ends and handle reuse; separate injected callback tests cover stale tokens, generations, stop, closing, completion errors and invalid result addresses. The exact final results and timings are retained below.

## Pipeline cache

The hard cache bound remains **256**. Under low OS headroom, the policy removes only entries unused for both ten seconds and sixty protocol PRESENTs, stopping at 128 if enough cold entries exist. It no longer destroys a demonstrably hot working set solely because the process crossed a memory threshold. Normal exact-key LRU replacement still applies at 256. This may retain up to 128 more hot driver objects than the old pressure policy; actual driver memory cost is unknown and must be evaluated on the phone.

Exact key equality, specialized shader masks, storage formats, layout semantics and in-flight command ownership are preserved. The existing 192-key / 576-draw pixel-and-occlusion fixture compares 128, 256, old shrink/restore and new pressure pruning. It is a controlled host workload, not a recorded full Los Santos key trace.

Fingerprint-assisted lookup rejects nonmatching keys before a full comparison, while an exact first-entry check preserves the cheap common hit. Benchmarks include rotating same-shader state, first-entry hits, last-entry hits and distinct shaders. All hash matches still require full key equality; collisions cannot alias different state. Exact final measurements are recorded below.

## Input

Input events still enter through the original synchronous engine pad hook. When digital edges are queued, stick axes use the newest normalized state, while button edges and trigger values retain their ordered snapshots. Explicit neutral barriers prevent source switching, focus changes or queue overflow from acquiring axes from another source. There are no new polling threads, engine writes outside the hook, allocations or GPU waits.

Controlled-clock tests verify press/release, trigger pressure, latest sticks, switching neutrality and lifecycle behavior. This removes stale analog replay across queued digital events; it does not make the engine hook run during a simulation/render stall. Event-to-hook statistics do not include Bluetooth/UI latency or the time to display a response. Physical input improvement remains unverified. The exact final 0.36 diagnostic reports 18,016 controller events, 17,003 engine updates, three source switches, zero queue overflows, and oldest-coalesced event-to-hook mean/max **93.824/2,044.697 ms across 3,647 samples**. The queue was empty at that final sample; a persistent input backlog is therefore not established. This metric retains the oldest timestamp of coalesced analog updates and is not a complete event-to-display measurement.

## Exact affected files

All paths below are relative to the isolated native repository.

| Files | Purpose |
|---|---|
| `native-ios/renderer/event_queries.inc`, `event_fixture.inc`, `native_queries.inc` | Completion-based event replies, stale-generation protection and focused replay/fault tests. |
| `native-ios/renderer/native_gpu.m`, `native_metrics.inc` | Service drain pools, pending-event polling, resource census and bounded event diagnostics. |
| `native-ios/renderer/draw_pipeline.inc`, `draw_types.inc`, `pipeline_policy.h`, `test_pipeline_policy.c`, `stability_fixture.inc` | Bounded hot-cache pressure policy, exact-key lookup and controlled comparison tests. |
| `native-ios/renderer/submission_fixture.inc` | Deterministic policy-clock replay; pixel, query, byte and profiler-accounting assertions retained. No production timeout changed. |
| `native-ios/input/native_input.c`, `test_input_queue.c` | Latest stick state with ordered digital/trigger snapshots and explicit neutral barriers. |
| `native-ios/diagnostics/native_recovery.h`, `native_journal.m`, `test_recovery.c`, `test_journal.m` | Bounded persistent recovery and concurrent journal regression. |
| `native-ios/App.m`, `ui/NativeDashboard.inc`, `Info.plist` | Progress temporary lifetime, recovery sharing, version/build 37. |
| `native-ios/diagnostics/ci-test-diagnostics.sh`, `renderer/build-renderer.sh`, `compile-native.mjs`, `source-manifest.json` | Test integration, compiler dependency integrity, preserved failure output and exact source manifest. |

## Changes intentionally not shipped

No unsafe compiler flags, -O2/-O3 engine promotion, heap expansion, forced asset eviction, quality reduction, sub-640×360 resolution, frame-cap change, new audio output, direct-Metal rewrite, memoryless attachment assumption or fabricated GPU result. The CPU sample identifies audio hot functions but lacks a representative PCM equivalence workload for safely changing their compilation in this build. GPU time remains unknown, so optional quality presets are not justified by a measured GPU bottleneck yet.

The source retains the 64-KiB archive buffer, maximum eight file descriptors, 256 pipeline bound, existing shadow compatibility, SRV 5633 correction, shaders, clip distances, texture proxies, compute, occlusion, readbacks, controller/touch switching and fullscreen. Prior IPAs are preserved.

## Validation and delivery

Validated source: `61f2bfa21812bfd4b50b64342b57a9a6397f71ec`. [Successful macOS/Metal and ARM64 CI](https://github.com/yosnuffyyy/gta5loader/actions/runs/39091280279). Package: `GTA-Native-Probe-0.37-unsigned.ipa`, version 0.37.0/build 37, 102,339,977 bytes. SHA256: `cc58cc617a3d143fe88305bc0d482015aa64d6f51b7d32605425f019331559b4`. [IPA-only release](https://github.com/yosnuffyyy/gta5loader/releases/tag/native-0.37). The source and unsigned package are independently validated; physical 0.37 gameplay is pending.

* Two complete native runtime processes passed, plus 4,808 translated shader modules and pipelines, including 500 clip shaders.111 untranslated records remain; this inventory is not proof of all future gameplay paths.
* Original shadow shaders 996bb42f448956f2 and 6e9619cde3dac275, dynamic shadow states, SRV 5633, compute/UAV, occlusion counts, readback ordering, uniform bytes, resource lifetime and allocator regressions passed. The existing optional neutral-shadow behavior is preserved, not newly proven visually correct on iPhone.
* 197 production UIKit assertions,71 original engine-input checks, 1,374 controlled-clock watchdog assertions, input concurrency and worker/session tests passed. ASan/UBSan and TSan checks ran for the applicable host C/Objective-C suites; this does not mean every GPU driver or iPhone code path ran under a sanitizer.
* Recovery: 10,000 updates, fixed 24,640-byte ring, independent categories, complete atomic replacement, orphan temporary file preservation and injected file errors. Journal: 80,000 concurrent records under ASan/UBSan and TSan, 4,435,652 bytes written, 0 lost bytes.
* 241 Git blob IDs match the exact candidate; 36 previous IPAs retain their original sizes/hashes. Package audit passed ARM64/iPhone/minimumiOS17, unsigned, no writable executable segments, no WebKit or game/web payload, pinned engine/backend and no CI shader inventory or UI injection code.

### Host Metal comparison results

These are controlled regression workloads, not captured full Los Santos sessions and not A18 Pro FPS. Async and legacy runs use the same original shader/pixel workload. The query timing includes host GPU/scheduling/callback effects; it is not isolated GPU execution time. Two process repetitions are shown rather than invented p95/p99 values from tiny samples.

```text
PASS: pipeline working set policy 128: 192 distinct required keys, 3 cycles, 576 draws/147456 samples/256 pixels; creates/hits/evictions 576/0/448; live peak 128, pressure evictions 0; pipeline API 76.337ms, link 34.982ms; encoded commands survive eviction. Policy 1 is legacy shrink; policy 2 preserves the bounded hot working set. Driver bytes and iPhone FPS unmeasured. 
PASS: pipeline working set policy 256: 192 distinct required keys, 3 cycles, 576 draws/147456 samples/256 pixels; creates/hits/evictions 192/384/0; live peak 192, pressure evictions 0; pipeline API 21.224ms, link 9.950ms; encoded commands survive eviction. Policy 1 is legacy shrink; policy 2 preserves the bounded hot working set. Driver bytes and iPhone FPS unmeasured. 
PASS: pipeline working set policy 1: 192 distinct required keys, 3 cycles, 576 draws/147456 samples/256 pixels; creates/hits/evictions 448/128/256; live peak 192, pressure evictions 64; pipeline API 50.467ms, link 23.558ms; encoded commands survive eviction. Policy 1 is legacy shrink; policy 2 preserves the bounded hot working set. Driver bytes and iPhone FPS unmeasured. 
PASS: pipeline working set policy 2: 192 distinct required keys, 3 cycles, 576 draws/147456 samples/256 pixels; creates/hits/evictions 192/384/0; live peak 192, pressure evictions 0; pipeline API 27.160ms, link 12.062ms; encoded commands survive eviction. Policy 1 is legacy shrink; policy 2 preserves the bounded hot working set. Driver bytes and iPhone FPS unmeasured. 
PASS: event callback fault injection: 64-bit reply, stale token, replaced generation, stop, closing, GPU error and invalid address; no fabricated successful completion. Separate from hardware replay.
PASS: event-query async: 384 original shader draws/256 pixels,513 ENDs; latest-token/replaced-handle/type0,2,3 values checked; synchronous completion calls 25, event requests/results/stale 513/387/126 peak90 pressure0; 346.231ms wall. CPU timestamp shim preserved; no GPU timing or iPhone FPS claim. 
PASS: event-query legacy: 384 original shader draws/256 pixels,513 ENDs; latest-token/replaced-handle/type0,2,3 values checked; synchronous completion calls 514, event requests/results/stale 0/0/0 peak0 pressure0; 2729.740ms wall. CPU timestamp shim preserved; no GPU timing or iPhone FPS claim. 
PASS: buffer dependency replay legacy0:96 original-shader draws,512 checked pixels,96 exact samples / 96 original pass-query segments,32 final buffer bytes, clear/counter upload ordering; batches/submissions 9/14; uniform packs/bytes 22/5632; independent/dependent uploads 93/3; completions 1; pressure waits 0; same resource replacement, no new staging. ready-drain submits avoided 0; Handler/upload wall 96.378/5.495 ms; protocol upload payload 53044 bytes (nested waits included, not GPU execution).
PASS: buffer dependency replay legacy1:96 original-shader draws,512 checked pixels,96 exact samples / 96 original pass-query segments,32 final buffer bytes, clear/counter upload ordering; batches/submissions 98/103; uniform packs/bytes 192/49152; independent/dependent uploads 0/95; completions 7; pressure waits 6; same resource replacement, no new staging. ready-drain submits avoided 0; Handler/upload wall 377.099/50.567 ms; protocol upload payload 53044 bytes (nested waits included, not GPU execution).
PASS: buffer dependency replay legacy2:96 original-shader draws,512 checked pixels,96 exact samples / 96 original pass-query segments,32 final buffer bytes, clear/counter upload ordering; batches/submissions 6/11; uniform packs/bytes 16/4096; independent/dependent uploads 94/3; completions 1; pressure waits 0; same resource replacement, no new staging. ready-drain submits avoided 3; Handler/upload wall 96.493/3.315 ms; protocol upload payload 53044 bytes (nested waits included, not GPU execution).
PASS: pipeline working set policy 128: 192 distinct required keys, 3 cycles, 576 draws/147456 samples/256 pixels; creates/hits/evictions 576/0/448; live peak 128, pressure evictions 0; pipeline API 73.077ms, link 32.820ms; encoded commands survive eviction. Policy 1 is legacy shrink; policy 2 preserves the bounded hot working set. Driver bytes and iPhone FPS unmeasured. 
PASS: pipeline working set policy 256: 192 distinct required keys, 3 cycles, 576 draws/147456 samples/256 pixels; creates/hits/evictions 192/384/0; live peak 192, pressure evictions 0; pipeline API 21.715ms, link 10.446ms; encoded commands survive eviction. Policy 1 is legacy shrink; policy 2 preserves the bounded hot working set. Driver bytes and iPhone FPS unmeasured. 
PASS: pipeline working set policy 1: 192 distinct required keys, 3 cycles, 576 draws/147456 samples/256 pixels; creates/hits/evictions 448/128/256; live peak 192, pressure evictions 64; pipeline API 96.471ms, link 39.781ms; encoded commands survive eviction. Policy 1 is legacy shrink; policy 2 preserves the bounded hot working set. Driver bytes and iPhone FPS unmeasured. 
PASS: pipeline working set policy 2: 192 distinct required keys, 3 cycles, 576 draws/147456 samples/256 pixels; creates/hits/evictions 192/384/0; live peak 192, pressure evictions 0; pipeline API 45.994ms, link 18.829ms; encoded commands survive eviction. Policy 1 is legacy shrink; policy 2 preserves the bounded hot working set. Driver bytes and iPhone FPS unmeasured. 
PASS: event callback fault injection: 64-bit reply, stale token, replaced generation, stop, closing, GPU error and invalid address; no fabricated successful completion. Separate from hardware replay.
PASS: event-query async: 384 original shader draws/256 pixels,513 ENDs; latest-token/replaced-handle/type0,2,3 values checked; synchronous completion calls 25, event requests/results/stale 513/387/126 peak90 pressure0; 348.574ms wall. CPU timestamp shim preserved; no GPU timing or iPhone FPS claim. 
PASS: event-query legacy: 384 original shader draws/256 pixels,513 ENDs; latest-token/replaced-handle/type0,2,3 values checked; synchronous completion calls 514, event requests/results/stale 0/0/0 peak0 pressure0; 4089.479ms wall. CPU timestamp shim preserved; no GPU timing or iPhone FPS claim. 
PASS: buffer dependency replay legacy0:96 original-shader draws,512 checked pixels,96 exact samples / 96 original pass-query segments,32 final buffer bytes, clear/counter upload ordering; batches/submissions 9/14; uniform packs/bytes 22/5632; independent/dependent uploads 93/3; completions 1; pressure waits 0; same resource replacement, no new staging. ready-drain submits avoided 0; Handler/upload wall 98.522/3.808 ms; protocol upload payload 53044 bytes (nested waits included, not GPU execution).
PASS: buffer dependency replay legacy1:96 original-shader draws,512 checked pixels,96 exact samples / 96 original pass-query segments,32 final buffer bytes, clear/counter upload ordering; batches/submissions 98/103; uniform packs/bytes 192/49152; independent/dependent uploads 0/95; completions 7; pressure waits 6; same resource replacement, no new staging. ready-drain submits avoided 0; Handler/upload wall 271.297/48.123 ms; protocol upload payload 53044 bytes (nested waits included, not GPU execution).
PASS: buffer dependency replay legacy2:96 original-shader draws,512 checked pixels,96 exact samples / 96 original pass-query segments,32 final buffer bytes, clear/counter upload ordering; batches/submissions 6/11; uniform packs/bytes 16/4096; independent/dependent uploads 94/3; completions 1; pressure waits 0; same resource replacement, no new staging. ready-drain submits avoided 3; Handler/upload wall 93.521/4.327 ms; protocol upload payload 53044 bytes (nested waits included, not GPU execution).
```

### Host CPU lookup microbenchmarks

Each row uses 100,000 lookups, 256 keys of 1,468 bytes, with unchanged exact-key comparisons. The third column is the shipped first-entry-plus-fingerprint path. Both optimized host and sanitizer runs are retained; only the optimized host run is appropriate for a relative CPU-performance claim. Neither predicts iPhone frame times.

```text
Optimized host (-O2):
PASS: lookup benchmark workload0 (0 rotating same-shader,1 first-key,2 last-key,3 distinct shaders);100000 lookups linear 734.497ms fingerprint+exact 54.983ms; first-entry+fingerprint 58.269ms;256 keys/1468 bytes each. Synthetic CPU only.
PASS: lookup benchmark workload1 (0 rotating same-shader,1 first-key,2 last-key,3 distinct shaders);100000 lookups linear 6.470ms fingerprint+exact 45.792ms; first-entry+fingerprint 6.274ms;256 keys/1468 bytes each. Synthetic CPU only.
PASS: lookup benchmark workload2 (0 rotating same-shader,1 first-key,2 last-key,3 distinct shaders);100000 lookups linear 1156.480ms fingerprint+exact 59.413ms; first-entry+fingerprint 64.436ms;256 keys/1468 bytes each. Synthetic CPU only.
PASS: lookup benchmark workload3 (0 rotating same-shader,1 first-key,2 last-key,3 distinct shaders);100000 lookups linear 64.714ms fingerprint+exact 60.896ms; first-entry+fingerprint 54.177ms;256 keys/1468 bytes each. Synthetic CPU only.
PASS: pressure cold-boundaries and exact-key collision safety

Sanitized host (-O1 ASan/UBSan):
PASS: lookup benchmark workload0 (0 rotating same-shader,1 first-key,2 last-key,3 distinct shaders);100000 lookups linear 1094.590ms fingerprint+exact 71.424ms; first-entry+fingerprint 87.799ms;256 keys/1468 bytes each. Synthetic CPU only.
PASS: lookup benchmark workload1 (0 rotating same-shader,1 first-key,2 last-key,3 distinct shaders);100000 lookups linear 13.991ms fingerprint+exact 53.063ms; first-entry+fingerprint 11.957ms;256 keys/1468 bytes each. Synthetic CPU only.
PASS: lookup benchmark workload2 (0 rotating same-shader,1 first-key,2 last-key,3 distinct shaders);100000 lookups linear 2462.552ms fingerprint+exact 77.723ms; first-entry+fingerprint 104.700ms;256 keys/1468 bytes each. Synthetic CPU only.
PASS: lookup benchmark workload3 (0 rotating same-shader,1 first-key,2 last-key,3 distinct shaders);100000 lookups linear 634.674ms fingerprint+exact 64.234ms; first-entry+fingerprint 67.183ms;256 keys/1468 bytes each. Synthetic CPU only.
PASS: pressure cold-boundaries and exact-key collision safety
```

The first fingerprint-only candidate regressed the first-entry microbenchmark (5.847→48.187ms in the earlier optimized run). It was not released; the final implementation first checks exact entry 0 before hashing. Full equality is always required after a fingerprint match. The test validates distinct keys and exact comparison; it does not force a cryptographic collision in the production hash.

### Memory and performance limits

No before/after physical process-footprint, thermal, displayed-frame percentile or input-to-display measurement is available for0.37. The new callback pool is fixed 128 entries (6,144 bytes on the host64-bit layout); cache metadata adds 24 bytes per 256 entries (6,144 bytes); the recovery ring adds 24,640 bytes plus bounded writer snapshots. Opaque driver allocations are excluded. Autorelease lifetime is bounded by each service drain/poll, but no specific MiB saving or original leak is proven. The pressure policy may retain 128 additional hot driver pipelines compared with 0.36. This must be checked against actual headroom, not marketed as a memory reduction.

The original submission/uniform optimizations remain and pass their same byte/pixel tests. Their existing improvements are not counted as new 0.37 work. The new event path removes synchronous END waits, not the original draw submission boundary. Pool-pressure/cancellation logic is present; the real GPU replay did not saturate all 128 slots, so physical backlog behavior remains a test target.


## CI failure investigated

First candidate `3f9c5d508bd522d0b4f08ba78bfd37a7be084aef`, run39089801482, failed the instrumented submission replay. Its GPU pixels and explicit profiler counters passed inspection, but the fixture required fewer drain submissions than the reference despite a real 4-ms scheduling cutoff. The slower instrumented host execution legitimately crossed that cutoff. The replay now supplies a controlled clock only to that scheduling-policy decision, retaining actual measured handler times, real GPU work, the four-drain limit, all byte/pixel/query assertions and the independent 289-case real-threshold policy test. It is not a production relaxation or evidence of iPhone batching speed. Complete failure output is retained. Final rerun status is recorded above.

## Physical verification

Keep 640×360 and the 30-FPS cap. Match the previous reference-shadow option. Mark actual visible gameplay and compare equivalent Prologue/vault and Franklin/Lamar driving sections. Record at least 10–20 minutes when practical; separate cutscenes/loading. Check held movement, aiming, fire/release, quick taps, DualSense/touch switching and interruption recovery. Check shadows, characters, occlusion and scene transitions for visual regressions.

Share the full log after stopping; if the app closes, relaunch and share the previous Recovery Snapshot before starting another session. Inspect cache creates/evictions, async event pending/peak/pressure, completion waits, upload costs, resource census, thermal state, footprint/headroom and display percentiles. If available, supply the corresponding iOS jetsam/native crash record; a CPU usage report alone cannot settle termination cause.

30 FPS requires sustained approximately 33.3-ms displayed frames; the supplied 0.36 late window is 158.934 ms mean. Neither 30 FPS nor 60-FPS-equivalent 16.67-ms headroom is established. Remaining constraints include service CPU work, batching/upload costs, memory pressure, audio mixing CPU, compilation misses and unmeasured GPU/engine execution.

## Compact report for ChatGPT

**Build and workload.** Native 0.37.0 / build 37, source `61f2bfa21812bfd4b50b64342b57a9a6397f71ec`, CI run 39091280279 passed. Baseline Native 0.36 / build 36, source `f30f83723017f2452dbe5660243906dedb425e13`, Mach-O UUID `460455BF-B9CA-3454-B49A-DC81E95790DE`. Physical device: iPhone 16 Pro Max / A18 Pro / iOS 27.0.1. Both use 640×360 and a 30 FPS cap. User reports complete Prologue, funeral, Los Santos introduction and Franklin/Lamar driving in 0.36, followed by whole-app closure. No physical 0.37 result yet.

**Physical 0.36 performance.** User reports about 10 FPS gameplay. Final 256 actual-display intervals: mean/p50/p95/p99/max **158.934/166.682/233.355/416.705/2216.870 ms**, about 6.29 interval-FPS, with phases mixed. Graphics-service thread CPU mean **111.084 ms/PRESENT**; command-handler wall mean **160.899 ms/PRESENT**. These are neither engine simulation time nor GPU execution time. True GPU time and utilization remain UNKNOWN.

**Memory and closure.** Final footprint **3,372.3 MiB**, committed engine heap **2,100.1 MiB**, OS-reported headroom **3.9 MiB**, serious thermal state. Engine malloc arena 2,100 MB / in-use 2,045 MB; game virtual 345/352 MB; resource virtual 1,077/1,088 MB. Counters overlap and cannot be added. The exact-UUID CPU report covers an earlier 170.91-second period: 90 CPU seconds, 87 samples, 21 lost, **Action taken: none**. It is not a crash or jetsam report. Severe memory pressure is confirmed; jetsam and a specific leak remain unconfirmed. The multi-gigabyte memory problem is not declared fixed.

**Repeated work.** 0.36 ended with cache capacity 128 after pressure, **37,335 creations / 37,207 evictions**, and **36,017 exact recent-evicted-key matches** within an incomplete 64-key history. Pipeline API wall totaled **30,358.451 ms**, max **516.576 ms**; linking totaled **12,081.171 ms**. Completion waits: **1,375,293 calls / 538,514.215 ms**. Upload handler: **392,831.023 ms** cumulative, final-window mean **23.804 ms**. Protocol uploads: **15,364,985 buffer / 186,564 texture**. Uniform packs/reuses: **12,692,829 / 6,368,536**, **24,578,823,168 bytes**. Queue submissions **3,207,309**; draw batches **2,592,128**; staging-pressure waits **114,783**. Timing categories overlap; repeated operations are not all redundant.

**Implemented in 0.37.** (1) Event/timestamp compatibility replies now use actual queue-completion callbacks, a fixed 128-entry pool, generation/End-token validation and release-ordered publication. Original result values, draw submission boundaries, occlusion and readbacks are preserved. (2) Under pressure, prune only pipelines unused for at least 10 seconds AND 60 PRESENTs; keep the hard 256-entry limit. Exact-key fingerprint prefilter plus a cheap exact first-entry hit reduces lookup work without allowing hash-only equality. (3) Deliver newest stick axes while retaining queued button/trigger snapshots and neutral handoff barriers. (4) Bound service-drain/progress autorelease lifetimes; add a five-second resource census and fixed 24,640-byte atomically replaced recovery snapshot, exportable after relaunch.

**Host Metal results, not phone FPS.** Same 384 original-shader draws / 256 checked pixels / 513 ENDs: synchronous completion calls **514 → 25**. Legacy/async wall times were **2729.740/346.231 ms** and **4089.479/348.574 ms** in two repetitions. A 192-key, 576-draw controlled pressure workload, with 147,456 exact occlusion samples and 256 checked pixels: old/new policy **448/192 creations**, **256/0 evictions**. Pipeline API wall **50.467/27.160 ms** and **96.471/45.994 ms**; link wall **23.558/12.062 ms** and **39.781/18.829 ms**. Driver memory is opaque. The new policy can retain up to 128 more hot driver pipelines than the old policy; its device memory tradeoff remains unmeasured.

**Host CPU lookup results.** Optimized host build, 100,000 lookups over 256 keys of 1,468 bytes: linear/new path **734.497/58.269 ms** for rotating same-shader keys; **6.470/6.274 ms** for first-entry hits; **1156.480/64.436 ms** for last-entry hits; **64.714/54.177 ms** for distinct shaders. These isolated microbenchmarks are not end-to-end frame improvements. An earlier fingerprint-only first-entry regression was fixed before release.

**Input and CPU unknowns.** 0.36 oldest-coalesced event-to-hook mean/max **93.824/2044.697 ms**, 3,647 samples; queue empty at final sample, zero overflows. New latest-axis behavior passes a controlled 100 ms backlog test but does not make the engine consume input during rendering stalls. No Bluetooth-to-display latency is measured. Exact-binary symbolication identifies audio mixer/Opus activity: 26 inclusive samples at mixer thread, 15 at MixBuffers/GeneratePcm, 11 at wave-player/Opus update. These counts overlap and are not exclusive percentages. Audio output remains unfinished; no audio work was disabled and engine compiler settings were preserved.

**Validation and remaining limits.** Two full native runtime processes; 4,808 Metal shader modules/pipelines including 500 clip shaders; shadow/SRV 5633, query, compute, pixel/readback, resource lifetime, allocator, worker and input regressions passed. 197 UIKit, 71 original engine-input and 1,374 watchdog checks passed, with applicable ASan/UBSan and TSan suites. 111 shader records remain untranslated. The GPU fixture reached 90 pending event replies, not full 128-slot saturation. No new device FPS, footprint reduction, thermal improvement, correct shadows or long-session stability is claimed. Prior 36 IPAs are unchanged.

**Next priorities.** First, collect the new resource-owner census/recovery plus an actual jetsam/native crash report if closure recurs; compare equivalent Los Santos gameplay and memory growth. Second, measure event-completion/cache behavior and display percentiles on-device, then profile remaining upload/batch/service CPU overhead. Third, obtain genuine Metal GPU timing and representative engine/audio workloads before quality changes or compiler promotion. Stable 30 FPS (33.3 ms) and 60-FPS-equivalent processing headroom (16.67 ms) are not supported by current physical measurements.

**Delivery.** `GTA-Native-Probe-0.37-unsigned.ipa`, **102,339,977 bytes**, SHA256 `cc58cc617a3d143fe88305bc0d482015aa64d6f51b7d32605425f019331559b4`. IPA-only release: https://github.com/yosnuffyyy/gta5loader/releases/tag/native-0.37. Full logs and private CPU diagnostic identifiers were not uploaded.
