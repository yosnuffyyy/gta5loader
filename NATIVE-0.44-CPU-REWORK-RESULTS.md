# Native 0.44 — CPU preparation core and controlled compiler experiments

## 1. Status, source and acceptance decision

**Validated experimental test build, not a claim that the full CPU/simulation mission is complete.** CPU Execution Core V2 stays **OFF by default**. Existing rendering remains available, with its independent Renderer V2 option. No physical 0.44 FPS, correct real-time simulation, speaker output, sustained thermal performance or new mission progression is verified. **The requested major CPU reduction has not been demonstrated.** This release supplies a correctness-validated, bounded experimental preparation path and reproducible comparison harness; it does not satisfy the sustained-performance acceptance gate.

Executable source `fb7a11e2c0571c5187b98411807226edb7fcc863`, tree `66c1b8d5b5d31697112f7d71a358fbca79867a93`. Baseline executable Native 0.43 `9ea0c38bcfe9e673fa7ac7f5ae2a0ef811ce1f24`; newer verified 0.43 documentation was preserved. [Integrated workflow 39332037894](https://github.com/yosnuffyyy/gta5loader/actions/runs/39332037894) passed. The first 0.44 candidate was superseded during review before release; no failed/unfinished candidate is called a successful build.

## 2. Confirmed physical 0.43 costs

Complete phone log: `NotgpuGTA-Native-Log-08E67252-76A2-4B1C-B8A1-B6AB0C7C0E9E.txt`,5,929,818 bytes, SHA256 `18393d85ae7cb2d06fe0d4f863f54f7eb69cb7c8414ec084112950f8ea9eb964`. It records Sandbox, Renderer V2, Ultra Potato 320×180, detailed profiling ON, user stop/Exit 0, and **no marked gameplay window**. First/last windows are different workloads. Identical Normal/Ultra performance is the user's separate observation, not a controlled trace pair.

Last 256 actual display intervals mean/p50/p95/p99/max **79.825/83.341/100.009/116.677/266.691 ms** (~12.53 interval-FPS, unclassified). Handler active CPU 48.130549 ms/PRESENT; service-thread CPU 67.863 ms/PRESENT. These measurements establish a substantial CPU cost under enabled profiling; they do not measure GPU execution or establish that the GPU is idle.

| Ranked measured cost | Cumulative active CPU | Evidence/limits |
|---|---:|---|
| DRAW preparation/execution |9,307.454 ms /841,928 calls|`draw_execute`, pipeline key/reflection/binding/uniform preparation; inclusive |
| Encoding/submission finalization |drain 8,400.649 ms; pending preflush6,262.357 ms; submit API 4,285.980 ms|**Overlapping** with handlers/each other; not an additive total |
| Buffer-upload handlers 12/47 |3,603.680 ms /1,203,051 calls|Correct ordered bytes; repeated commands are not all redundant |
| Texture readback 42 |2,668.710 ms CPU /5,543.373 ms wall /1,554 calls|Fresh results/dependencies required; not all wait time is removable |
| END_QUERY 46 |1,686.474 ms /70,954 calls|Includes completion/occlusion work, not GPU execution |

PRESENT itself accounted for 3,083.703 ms CPU and overlaps finalization. Current pipelines 262 creates/6 evictions,489.047 ms API wall /211.692 ms service-thread CPU,272.153 ms worst API wall. The old tens-of-thousands pipeline-thrashing evidence belongs to another build/workload. No new capacity increase was justified. Native 0.43 already removed major submission/uniform overhead; those savings are preserved, not counted as0.44 improvements.

## 3. New execution-core architecture

The acquired command-ring payload/checked decoder, D3D semantics, dependency scheduler and wgpu-native/Metal backend are reused. This is a focused replacement of repeated **CPU preparation**, not a second renderer or complete engine rewrite.

* A sparse exact-ID Object index replaces common CFDictionary lookups.4096 directory slots,256 pointers/page, maximum 512 allocated pages. Maximum index allocation **1,081,864 bytes** on 64-bit hosts, plus the fixed preparation snapshot/counters. It owns no Objects/GPU resources. High IDs, exhausted page budget and page-allocation failure use the canonical dictionary. Initial directory-allocation failure is an explicit startup error. A failed page remains dictionary-only until clear, preventing a later partial page from incorrectly hiding older objects. Create publishes the same pointer/generation; common destruction invalidates before freeing; teardown clears the index.
* An exact **336-byte** preparation-input snapshot avoids rebuilding/normalizing the 1,468-byte DrawKey when validated inputs match. Shader/layout/state IDs, strides, render-target descriptors, lifecycle counters, storage formats and specialization masks are compared directly. No hash-only equality. Any object creation/destruction or pipeline eviction invalidates conservatively; unrelated creations can cause extra misses. One snapshot, no GPU ownership/extra frame in flight.
* Dynamic storage/shadow decisions still run **every draw before the cache probe**. Current CB values, resource bindings/proxies, buffer bounds/hazards, immutable uniform bytes, draw order, query readiness and backend validation remain. No cached pixels, skipped draws, fake visibility, new shader semantics or relaxed validation.
* The 256-entry pipeline policy and 0.43 retained-handle pass-state cache remain. Cold compilation is not eliminated. No new batching/submission policy or unconditional synchronization removal is claimed.

## 4. Changed source files

| Source group | Change |
|---|---|
| `renderer/core_index.h`, `core_pipeline.inc`, `draw_types.inc`, `draw_pipeline.inc`, `native_gpu.m/.h`, `native_metrics.inc` | Bounded identity index, exact preparation reuse, lifecycle integration/counters |
| `threading/native_mailbox.h`, `native_threads.c/.h`, `timing/native_time_trace.h` | Optional mailbox claim, startup selection, inclusive owner CPU sample |
| `renderer/native_scene.c`, `native_launch.h`, `App.m`, `ui/NativeDashboard.inc`, `Info.plist` | Separate persisted OFF-default core selector, frozen while running;0.44.0/build 44 |
| `renderer/test_core_index.c`, `core_fixture.inc`, `reuse_fixture.inc`, `build-renderer.sh`, `threading/test_mailbox_claim.c`, `ci-test-session.sh`, `input/test_ui.m` | Allocation/reuse/exact-state, rendering, concurrency and settings regressions |
| `aot-benchmark.inc`, `generate-probe.mjs`, `compile-native.mjs`, `source-manifest.json` | Controlled compiler/renderer experiments and integrated verification;291 manifest entries |

## 5. Matched CPU measurements

Original startup IM shader fixture; **64 optimized-renderer host replays**,16 alternating pairs with profiling ON and 16 pairs OFF across two processes. Each replay checks 96 draws,512 pixels,96 exact occlusion samples, changing constants, offsets, null/rebound CBs, resource-ID replacement and flushes. O1 applies to the renderer object; remaining harness/engine objects stay host O0. The standalone O1 fixture uses the existing/default renderer; both renderer selections are covered by the full O0 runtime suites. Metal is the hosted Apple Paravirtual device, **not an iPhone**. 0.44 core OFF/ON is the matched comparison, not untouched 0.43-versus 0.44 binaries.

All timings below are ms. Values are medians of 16 replay statistics per variant, **not pooled gameplay frame percentiles**. DRAW wall includes cold compile/driver variation; CPU/wall/service overlap. Profiling order and core order alternate. Process/driver caches may persist. No outliers removed.

| Metric | Existing core | Core V2 | Change |
|---|---:|---:|---:|
| DRAW active CPU,96 draws | 1.424751 | 1.353081 | -5.03% |
| Whole service-thread active CPU | 3.513355 | 3.349499 | -4.66% |
| Sum DRAW wall,96 draws | 1.467500 | 1.493500 | +1.77% |
| Per-DRAW wall p50 | 0.001000 | 0.001000 | +0.00% |
| Per-DRAW wall p95 | 0.007000 | 0.005000 | -28.57% |
| Per-DRAW wall p99 | 1.136500 | 1.094500 | -3.70% |
| Per-DRAW worst wall | 1.136500 | 1.094500 | -3.70% |

Paired comparisons are mixed: median DRAW CPU change **+0.335%**, service CPU **-4.559%**, and DRAW wall **+4.815%** (negative means lower time). Only 8/16 DRAW CPU pairs and 9/16 service CPU pairs favor the new core. Ratios of marginal medians in the table differ from medians of paired ratios. This small fixture does not establish a robust overall speedup or statistically validated gameplay benefit.

With detailed CPU profiling OFF, sum DRAW wall median **1.451500→1.391000 ms** (-4.17%); paired median change +0.09%. CLI-only per-DRAW wall sampling remains equally enabled. Unprofiled active CPU and whole-service wall are UNKNOWN; this is not a measurement of all profiler overhead.

Preparation hits median **84/96**; cached-key reconstruction avoided on those exact inputs. Index lookups median indexed/fallback **1624/8** (includes negative/creation lookups). Observed additional index metadata peak **41480 bytes**, separate from driver/physical footprint.

**Unchanged work:**96 draws,6 queue submissions,4,608 uniform GPU bytes, one pipeline creation/95 hits/no evictions per matched fixture. The 0.43 state-setter optimization is already active in both variants. No submission/upload/FPS reduction is invented from these tests.

## 6. Thread scheduling and mailbox work

Only the experimental core adds an acquire load before the acquire exchange on the pending-mailbox flag. An empty check avoids an exclusive RMW/cache-line write; racing notifications remain set for the next original tick/wait. No new poll loop, wake policy, priority or engine-memory write is added. Original exchange behavior stays in the fallback.

ASan/UBSan/TSan tests checked 40,000 cross-thread publication/consumption handoffs, both paths. An isolated O1 host empty-flag benchmark,8 alternating pairs of 10 million polls, measured median CPU **52.276104→5.991396 ms** (-88.54%). This is **not** the real notification rate, engine wait saving or input latency. Original worker/mailbox regressions remain; physical optimized-core scheduling needs testing.

`-numCores=2` remains. Original main 103 parses it into 6594340/6594344 and scheduler mask 6594352. Verified consumers include task-manager 3088, dependency scheduler 3111, physics 43206 and audio 6964. The dependency wake path uses `min(available,max(1,cores-2))`:2 permits one wake in that path;4 would permit two. This establishes an internal constraint, **not its FPS impact or safe whole-engine scaling**. Host logical-core import also still returns 2. Alternative worker profiles/device thermal comparisons were not validated and were not shipped.

## 7. AOT compiler experiment

Original `CParaboloidShadow::GetLightShadowIndex` f44528, containing generated partition only, rebuilt at O1/O2. Other objects/flags remain fixed.16 order-alternated runs each check 400,000 exact cached/absent-light results, checksum 700,000. The unchanged original Wasm workload was independently checked locally. This is a constructed producer workload, **not proof this function dominates gameplay CPU**; it does not validate O2 physics/AI/decoder behavior or ARM64 performance.

| Option | Partition | Compile wall ms | Object bytes | Host binary bytes | Median producer CPU ms | Median wall ms |
|---|---|---:|---:|---:|---:|---:|
| -O1 | native-build/generated/gta_53.c | 26882.027 | 6,740,064 | 436,913,704 | 4.072000 | 4.110000 |
| -O2 | native-build/generated/gta_53.c | 27349.487 | 7,128,536 | 437,333,752 | 4.076917 | 4.080500 |

Full source hashes/raw observations are retained in `aot-compiler-experiment.json`. O2 producer CPU median is **+0.12%**, with **+5.76%** larger partition object. No meaningful producer speedup is established. Shipping engine remains **O1**; no blanketO2/O3, ThinLTO, PGO, fast-math, weaker atomics/bounds or CPU-specific ISA promotion. Eight timing samples/option do not support robust tail quantiles. Compiler RAM/ARM64 instruction-cache effects are UNKNOWN. Some existing generated loops report non-vectorization warnings; a warning alone is not hotspot evidence.

## 8. Engine CPU profiling

One thread-CPU clock read is added at the existing five-second owner pad-boundary report, with error handling and inclusive CPU/wall/hook counts. No per-function tracing, locks, allocations or engine-clock changes in that sampler. At most one added clock call per five seconds while hooks arrive, plus the initial sample. It measures the **engine-owner OS thread**, not UIKit's main thread or exclusive simulation/physics. Worker hot stacks, AI/animation/physics exclusives, true GPU execution, scheduler delay and 0.44 device input-to-display latency remain UNKNOWN. No flame graph was fabricated from wait labels.

## 9. Memory and thermal findings

Physical 0.43 final census47,775 objects/22,503 buffers/7,816 textures, only 75 explicit destroys. Descriptor payload852 KiB means only `o->words*4`; it excludes fixed Object structs, dictionary storage, shaders/variants and driver objects. CPU mirrors8,535 KiB. Sampled footprint2,053.1 MiB, committed engine heap1,556.1 MiB, cleanup97.9 MiB. These quantities overlap and **must not be added**. Serious thermal state is recorded; causality/sustained clocks are not measured.

No engine-owned resource can safely be purged by age. No leak, long-session memory reduction or driver-memory saving is established. New cache metadata is bounded and freed; no GPU resource copies, heap-budget increase, archive cache or additional frame-in-flight. The earlier 0.36 termination remains unconfirmed; this 0.43 run stopped normally.

## 10. Simulation timing and audio — unresolved

The actual 0.43 timer sample is 5.040 wall seconds versus 3.884 game/unwarped seconds,59 pad hooks. Original fwTimer8924 has a66.7 ms maximum delta. The original fixture reproduces 10 real seconds→3.333/6.667/10.0 engine-delta seconds at 5/10/15 Hz, with pause/resume checks;20/30 Hz also reach 10 seconds. The integer game/script clock also rounds:3,300/6,600/9,900/10,000/10,000 ms at 5/10/15/20/30 Hz over those 10 real seconds. **This correctness issue is not fixed.** Safe bounded catch-up requires validating the original scheduler, physics/collision, scripts, streaming and cutscene clocks; neither multiplying animation speed nor removing the clamp was justified.

The 0.43 log also reports audio starts/callbacks/nonzero frames0 despite6,067 mixer calls. The 48 kHz output/ring implementation and tests remain, but this is **not audible game sound**. MissingLOW_LATENCY.awc remains a visible diagnostic; no bank was fabricated. A new audio-path fix is not claimed by this CPU-core build.

## 11. Preservation and rollback

Both renderers; Story/Sandbox publication/startup and separate saves; collision/vehicle damage; A2C; clip distances/varying linkage;709/5633/valid shadows; depth/stencil; copies; immutable uniforms/ordered uploads; query/compute/UAV; PCM/input edge ordering; DualSense/touch switching; fullscreen/UI/branding; quality presets;30 FPS cap;64 KiB/eight-descriptor archive reader; shared-memory ABI; recovery and full-log sharing are preserved. No gameplay operations, mission entities, source textures or shader outputs were removed.

The experimental core stays OFF until comparable sustained phone results demonstrate benefit without meaningful regressions. Turn it OFF at a stopped-session boundary for rollback. Original renderer selection remains independent. Host percentages do not establish whole-session FPS gains or guarantee 30 FPS. **43 previous IPAs were rehashed and preserved.**

## 12. Regression and sanitizer evidence

Four complete native runtime processes cover both renderer selections with the GPU preparation core OFF/ON. Original shader/byte/pixel/resource-generation, copies, depth/stencil, compute/UAV, event/occlusion readiness, streaming/memory, worker, input and startup checks pass. All4,808 original translated modules/native pipelines (500 clip) validate with core ON;111 hull/domain/geometry records remain untranslated. The previously fatal grass pair retains16 actual draw cases/16,384 checked MRT pixels. Existing709/5633 and dynamic-shadow strict failures remain covered.

New exact-input/lifecycle and allocation-failure tests pass. Original input bridge71 checks; production UIKit **210 checks** including core persistence/disabled-while-busy/default restore. Component ASan/UBSan/TSan checks pass where configured, including the new mailbox helper. **The whole AOT engine and Metal driver were not sanitizer-instrumented.** Native tests do not prove new-build world gameplay or vehicle collision correctness on a phone. Source manifest291 entries verified against the exact Git tree; engine original digest unchanged.

## 13. Package

Unsigned ARM64 iPhone **0.44.0/build 44**, `GTA-Native-Probe-0.44-unsigned.ipa`, **105,001,835 bytes**. Bundle ID remains `io.github.yosnuffyyy.gtanativeprobe`; display name `GTA V (Test Build)`; same opaque icon/save and install identity. ARM64/iOS17 minimum, unsigned/no provision, no WebKit/WX, game archives, original Wasm/web package or CLI benchmark payload. Mach-O UUID `a72d02aea6953220a4d1b287dc50d469`.

SHA256: `e847e89988e792f530c301f17a1c00296a356bd7a8ce141e70f6d5cce127f4af`

CI artifact identification: `12227718729`; compiler evidence `12228695499`. [Release](https://github.com/yosnuffyyy/gta5loader/releases/tag/native-0.44) / [IPA](https://github.com/yosnuffyyy/gta5loader/releases/download/native-0.44/GTA-Native-Probe-0.44-unsigned.ipa). Release publication is separately audited; only the IPA is manually attached, no web files. Protected PC directories, deployed main, game assets and previous releases remain unchanged.

## 14. Next physical comparison and remaining objectives

1. Keep one renderer, mode and preset fixed. Use640×360 for the primary comparison. At the same stationary city location and repeated driving route, mark60-second visible-scene windows with core OFF andON; alternate order and repeat cold/warm. Start with detailed profilingOFF, then a separate brief ON capture. Do not compare loading/cutscene windows against city gameplay.
2. Settings→Graphics→CPU Execution Core V2 is stopped-session-only and independent of Renderer V2. Verify the startup/core-counter lines, actual displayed p50/p95/p99, input behavior, pause/resume, audio output starts and resource trends. Repeat on the other renderer separately. Compare0.43 versus 0.44 core OFF if checking release-level fallback performance.
3. Perform15–20 minute Sandbox driving, repeated impacts/debris/damage, Story transitions and stop/relaunch. Confirm saves, camera, shadows, touch/DualSense edges/fullscreen, audible output and engine-time/wall-time ratio. Export full TXT/recovery reports. A15/baseA18 performance remains untested.

Highest-value remaining work: measure real owner/worker hot stacks and preparation hit rates on equivalent phone workloads; establish a physics-safe simulation catch-up contract; diagnose why the live mixer never starts native PCM output; then target measured remaining uniform/binding/finalization costs. Worker-count experiments need validated startup/ownership plus device comparisons. **30 FPS at correct real time is not supported by current device evidence.**

## 15. Compact report for ChatGPT

Native 0.44.0/build 44 code `fb7a11e2c0571c5187b98411807226edb7fcc863`, CI 39332037894 PASS. Physical baseline 0.43 is Sandbox/RendererV2/Ultra320×180/profile ON, manual stop, no marked gameplay. Final256 actual intervals mean/p50/p95/p99/max79.825/83.341/100.009/116.677/266.691 ms (~12.53 unclassified interval-FPS); handler CPU 48.130549 ms/PRESENT, service CPU 67.863 ms.6,485,149 commands;841,928 draws;19,667 submits;1,554 readbacks;1,471,026,432 uniform GPU bytes;262 pipeline creates/6 evictions. DRAW CPU 9,307.454 ms; upload CPU 3,603.680 ms; readback CPU 2,668.710 ms; overlapping drain/preflush/submit8,400.649/6,262.357/4,285.980 ms. True GPU time and exclusive engine CPU UNKNOWN.

New OFF-default core: exact-ID bounded paged registry (1,081,864 bytes maximum),336 byte exact immutable-input cache replacing repeated1,468 byte key preparation, dynamic variants/bounds/hazards still checked; optional load-before-RMW mailbox claim. No new GPU work/data/frames or skipped operations. Host renderer-O1 matched96 draw fixture,16 pairs/profile,64 replays total, identical512 pixels/96 query samples/6 submits/4608 uniform bytes per replay. DRAW CPU median 1.424751→1.353081 ms; service CPU 3.513355→3.349499 ms; profiled DRAW wall 1.467500→1.493500 ms; unprofiled DRAW wall 1.451500→1.391000 ms. Paired medians DRAW CPU+0.335%, service CPU-4.559%, DRAW wall+4.815%; profiling-OFF paired wall+0.089%. Mixed, modest evidence; no robust major speedup or phone FPS claim. Original f44528 partition O1/O2 CPU medians4.072000/4.076917 ms per 400000 checked results; no shipping flag promotion.

All4808 shader pipelines/500 clip and four runtime configurations pass; component sanitizers only, not full AOT/driver.47,775 live resources,75 explicit destroys,2,053.1 MiB sampled footprint,1,556.1 MiB committed heap, serious thermal baseline; not additive/not leakproof. Input mean/max39.612/306.301 ms490 samples, no new device latency measurement. Original66.7 ms timer clamp still loses time at low cadence (10 real seconds→6.667 engine seconds at 10 Hz); audio starts0 in 0.43, still unresolved. Core stays experimental/OFF; both renderers, saves, compatibility, controls and 30 cap preserved. Next priorities: comparable device coreA/B with active-owner/worker evidence, physics-safe timing repair, native PCM activation diagnosis, then measured remaining CPU preparation/synchronization work. IPA SHA256 `e847e89988e792f530c301f17a1c00296a356bd7a8ce141e70f6d5cce127f4af`. No0.44 physical success or 30 FPS claim.
