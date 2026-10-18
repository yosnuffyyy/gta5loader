# Native 0.41 — validated experimental test build

Executable source: `da272c5bba805337a42bff4b965ea820b1d2b4d9`. Baseline: Native 0.40 executable `e14b588`, documentation `46b544d`. [CI 39166914440 passed](https://github.com/yosnuffyyy/gta5loader/actions/runs/39166914440). No physical 0.41 session has been measured. The existing renderer remains default; V2 remains experimental.

## Progressive slowdown findings

The complete 0.40 phone log was found in Downloads: 10,263,180 bytes, SHA256 `27adfcbc69a1e05bacc80d94f3551850b97c0cc7f8565963ce0439194de031b9`. Final cumulative counters were counted once, excluding duplicate previews:

- 401,814 END_QUERY calls; 63,910.727 ms inclusive active CPU; 77,444 nested submissions out of 187,430 total.
- 397,359 event/timestamp requests: 95,968 registrations, 288,966 joined replies, 12,425 already complete. Callback sharing had not eliminated per-END draw submission.
- 3,246,725 draws; 556 pipeline creations, 300 evictions and 60 recent exact-key recurrence matches in a bounded 64-key history. Cache capacity remains 256.
- Final 256 display intervals: mean 62.506, median 66.673, p95 100.009, p99 116.677 ms. No marked gameplay window; phases are mixed.
- Serious thermal state; 2,798,848 missing PCM frames. Input event-to-hook mean/max 38.405/210.971 ms over 1,775 samples. Input-to-display remains UNKNOWN.

Scene complexity, retention and thermal effects cannot be separated causally from changing rolling windows. No leak or jetsam is declared fixed. A fixed completion pool cannot explain multi-GiB growth. Existing allocation protections/resource census are retained.

## Implemented changes

**V2 logical event fences:** event/timestamp ENDs can share a bounded draw submission. Combined deferred/in-flight replies remain capped at 128; flush after 32 events or a 1 ms deadline checked between commands, and at dependencies, pressure and idle. Long nonpreemptive commands can exceed that deadline. A producer waiting immediately after END needs neither PRESENT nor another command. Blocking FENCE publishes preceding replies before its completion flag.

Results require the actual covering GPU submission/completion. Original timestamp compatibility values, generations/tokens, occlusion counts and draw-visible bytes remain intact. No additional frames-in-flight queue. Whole-service-loop CPU includes drain/submission/poll/wake work, preventing moved opcode costs from masquerading as savings.

Sources: `native-ios/renderer/completion_serial.h`, `completion_service.inc`, `completion_fixture.inc`, `native_draw.inc`, `native_gpu.m`, associated tests and `evidence/renderer-v2/compare.mjs`.

**Cancellation race:** `native_thread_tick` could intentionally trap on scene stop before `runtime.stopping` was set; catch could report a worker failure. Cancellation is now explicitly tagged at its origin. Genuine unreachable traps remain failures, even during shutdown. The 0.40 Stop messages are consistent with this race, not proof excluding another mission-replay fault. Entry 15 maps to `rage::sysIpcThreadWrapper`, not the inner fault. Genuine failures retain up to 16 pre-unwind image-relative frames for UUID-matched symbol lookup. Sources: `native_wait.c`, `threading/native_threads.c`, `diagnostics/native_trap.h`, wait tests.

## Controlled results

Hosted Apple Paravirtual Metal, original startup shaders, native harness `-O0`; production ARM64 uses `-O1`. Two independent processes, four alternating pairs per profiling setting: 32 replays. Each has 64 draws, 513 event ENDs and 256 pixel comparisons. No outliers removed; warmed driver caches may persist.

| Metric | Existing median | V2 median | Median paired change |
|---|---:|---:|---:|
| Wall, profiling off | 111.863 ms | 67.657 ms | −37.00% |
| Wall, profiling on | 130.510 ms | 58.571 ms | −53.22% |
| Whole service-loop CPU, profiling on | 22.051 ms | 7.823 ms | −62.27% |
| Inclusive END CPU, profiling on | 17.456 ms | 4.138 ms | −74.18% |
| Submissions, both settings | 65 | 14 | −78.46% |
| Callback registrations, both settings | 513 | 13 | −97.47% |
| Reply p50, profiling off | 0.204 ms | 8.985 ms | — |
| Reply p95, profiling off | 15.615 ms | 9.985 ms | — |
| Reply p50, profiling on | 0.188 ms | 8.638 ms | — |
| Reply p95, profiling on | 19.524 ms | 10.427 ms | — |

Separate-set medians and median paired ratios differ. CPU categories overlap; do not sum them. Reply timing is END entry to publication, not GPU execution. Coalescing increases median reply latency in this stress fixture while reducing CPU, submissions and p95. Immediate-consumer tests separately verify forward progress.

The fixture is deliberately query-heavy: 513 ENDs/64 draws versus approximately 0.124 ENDs/draw on the phone. **Do not project these gains onto gameplay.** Profile-off CPU, GPU execution, new upload-byte savings, phone memory/thermal effects and sustained FPS remain UNKNOWN. Every pair is retained in `evidence/renderer-v2/benchmark-0.41.json`.

Completion storage is fixed at 15,408 bytes versus 9,256 in 0.40 (+6,152), not a measured phone-footprint reduction.

## Presets and modes

| Preset | Internal size | Pixels versus Low |
|---|---|---:|
| Low | 640×360 | 100% |
| Potato | 480×270 | 56.25% |
| Ultra Potato | 320×180 | 25% |

Startup choices configure engine window arguments, published input dimensions and native surface consistently. Aspect ratio and 30 FPS cap remain. HUD also softens; native touch controls stay clear. Original window APIs accepted all sizes in an original-Wasm fixture; complete presentation needs device testing.

Existing low-quality flags are unchanged. No pass skipping, unsupported quality values, adaptive resizing, shader replacement or population writes. Density controls exist, but frame-reset/mission ordering remains unproven. Presets reduce pixel work; they are not a claimed solution to CPU-bound scenes.

Pre-game Story/Sandbox uses original engine startMode 1/2, verified with 12 original-engine checks. Preferences persist, lock while busy and remain independent of renderer choice. Story retains `NativeRuntimeProbe` and prior `-noautoload`; no new automatic checkpoint loader. Sandbox uses isolated `NativeSandboxProbe`. Full spawn, camera, collisions, vehicles, mission suppression and repeat launches need iPhone verification. Sources: `launch/options.h`, scene/thread/surface bridges, `App.m`, `ui/NativeDashboard.inc` and tests.

## Verification and preservation

- 4,808 Metal shader modules/pipelines, including 500 clip shaders; both SRV 709 pixel shaders, SRV 5633, depth/copy/UAV/occlusion/resource-lifetime regressions passed.
- Two full native runtime processes; 100,000 completion lifecycle/failure/reordering sequences; combined bounds, deadline and stale-reply tests passed.
- Immediate-consumer DRAW/END: three phases without PRESENT, changed textures, 768 checked pixels. Upload→END→FENCE verifies exact replies before completion flag.
- Intentional cancellation versus genuine unreachable, wait cleanup, memory/streaming, native audio, worker and input checks passed. Applicable component ASan/UBSan/TSan passed; full driver/AOT-engine sanitizer coverage is not claimed.
- 71 production input-bridge checks, 205 UIKit checks and 1,374 watchdog assertions passed. Simulator engine launch is stubbed; these are not gameplay tests.
- All 40 prior IPAs retain their hashes. Existing renderer/default, 256-entry cache, uniform/upload/copy optimizations, shadow/SRV compatibility, controls, fullscreen, icon, identity and assets remain. Protected main unchanged. Superseded partial CI runs are not counted as completed validation.

## IPA and next device sequence

- **0.41.0 / build 41**, source `da272c5bba805337a42bff4b965ea820b1d2b4d9`.
- `GTA-Native-Probe-0.41-unsigned.ipa`: **104,976,427 bytes**.
- SHA256 `7b9c73d7afb152a78f96ba3790c1348f4d61ad766e6771e9e25eda3f04a43a05`.
- Mach-O UUID `A273A901-40AE-3FB6-900B-310C011BD906`; matching symbol map/compiler evidence retained locally.
- ARM64/iPhone/iOS 17 minimum/unsigned; same app identity. No web/game archives, CI fixtures, WebKit or writable-executable segment. All 279 source-manifest entries audited.
- [Release and IPA](https://github.com/yosnuffyyy/gta5loader/releases/tag/native-0.41). Only the IPA is uploaded as a release asset; publication verified separately.

1. Story + V2 + Ultra Potato: mark opening gameplay and bank exterior separately; share full log after clean Stop.
2. Compare existing renderer at the same preset/checkpoint and similar thermal state. Use Low for architecture comparisons independent of resolution; collect one separate detailed-profile run.
3. Test mission failure/replay and manual Stop. Genuine traps should retain explicit errors and trace offsets.
4. Test Sandbox world/player/camera/collisions/vehicles, pause, controls and audio. Stop/relaunch, return to Story and verify progress/files remain intact.

**Open:** sustained slowdown, scene/thermal retention causes, audio underruns, existing animation delta cap (10 real seconds at 10 Hz advances 6.667 engine seconds), 111 untranslated shader records and iPhone 13 BC compatibility. No new clock/audio/physics rewrite. No physical 0.41 FPS, complete Sandbox gameplay or crash-free session is claimed. V2 remains experimental pending matched device evidence.
