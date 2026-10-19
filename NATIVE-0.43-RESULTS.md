# Native 0.43 — collision-triggered coverage fix and measured CPU work reduction

Executable source: `9ea0c38bcfe9e673fa7ac7f5ae2a0ef811ce1f24`; tree `209d01b2847d21dd0bb88fdbef725fcc69cf64ea`. Baseline executable: Native 0.42 `ab51b47752066092280136f8327a8543b8a612ad`. The checkout had the newer verified 0.42 documentation tree; it was preserved. Full device log SHA256: `3e9f978a59b7b852dbe0798e7d48573bce1d7c4cfcb7b1df1dbd557b94f2ec6b`.

## Confirmed crash cause and implementation

The final phone fatal was the explicit unimplemented **single-sample alpha-to-coverage** guard in `draw_pipeline.inc`, DRAW32, VS2588 `90bb870460e2835c` / PS2597 `0030c87264713865`, payload `3,48,1,0,0,0`. The pinned shader database identifies these as grass effect VS_Transform_REC / PS_DeferredTextured. Vehicle impact is the reported trigger; the trace does not establish a physics failure.

For a single-sample target and float4 SV_Target0, the fragment entry now evaluates its original result, discards when alpha does not satisfy **alpha >= 0.5**, and returns the complete original result otherwise. The NaN comparison rejects coverage. RGB, surviving alpha, other MRT outputs and shader body are preserved. Discard controls color, depth, stencil and occlusion coverage; draws and collision physics are not skipped. Specialization occurs after varying linking, with retained source-string ownership. The normal multisampled hardware A2C path remains enabled. An explicit fragment output sample_mask disables A2C, as required by D3D11. Unknown output shapes retain explicit failure; fixed partial/zero sample masks remain an unsupported pinned-Metal boundary.

This is the pinned browser reference's single-sample threshold approximation, **not a claim of bit-identical dithering on every D3D GPU**. D3D11 specifies endpoint/NaN behavior and monotonic coverage but leaves intermediate quantization implementation-dependent. Scope is A2C-enabled, single-sample, reflected float4 Target0, without explicit shader coverage; shader IDs are not hardcoded in production. Blend A2C, samples, shader specializations and attachment formats remain in the exact pipeline key. Original textures, geometry and shaders are retained.

References: [D3D11.3 functional specification §17.18](https://microsoft.github.io/DirectX-Specs/d3d/archive/D3D11_3_FunctionalSpec.htm#17.18%20Alpha-to-Coverage), [Microsoft blending documentation](https://learn.microsoft.com/en-us/windows/win32/direct3d11/d3d10-graphics-programming-guide-blend-state#alpha-to-coverage), pinned `work/minimal-engine/wgpu_worker.js` a2cModule/pipelineDesc.

## Independently verified rendering

- Exact original fatal shader sources and exact indexed-instanced DRAW payload: **16 actual DRAW cases / 16,384 checked pixels**, four MRTs, alpha 0/.25/.499/.5/.75/1/-1/2, sample counts1/4. Synthetic geometry/material/attachment descriptors provide deterministic pixels; the phone's full vertex, blend, sampler and RT descriptors were not captured. IDs alone do not reconstruct them.
- Both execution modes, each in both independent native processes: nine alpha cases including NaN and the float immediately below .5; **2,304 RGBA pixels,18 exact depth/occlusion results,2 stencil write/read results and5 native MSAA query results per run**. Hardware intermediate MSAA coverage is checked for monotonicity/bounds and endpoints, not forced to a portable fractional pattern.
- Original location15 Prologue shader pair also validates with A2C after real varying relabeling, retaining clip outputs and source lifetime.
- Pipeline keys, resource replacement, uniform bytes, buffer ranges, manual instance stepping, queries and pass/submit boundaries retain the existing regressions. Explicit shader-coverage override was source-reviewed; no dedicated new pixel fixture for that uncommon override is claimed.

## Sandbox startup-watchdog repair

The 0.42 capture still reported Session startup at107.9s despite positive live-player/loadout events. Original engine mode output is one WASI I/O-vector payload `[wasm] start mode: sandbox\n`; the native detector compared against a string without the newline, leaving launch_commit=0 and preventing the player event from retiring startup. This was reproduced against the original engine, including its actual I/O-vector segments.

The detector now accepts exact bare/LF/CRLF framing, rejecting arbitrary suffixes and additional messages. It changes no mode value or engine startup decision. A positive original player event plus subsequent pad hook/PRESENT still establishes live liveness; the genuine60-second stall policy, pause/background handling, cancellation and shutdown remain. Tests cover the old missed comparison, both modes, malformed framing, original LF output, **20 simulated minutes beyond180s**, and a genuine stall. This is policy/bridge proof, not20 minutes of phone gameplay.

Publication remains +440 mode/+280 focus. Story retains -noautoload and NativeRuntimeProbe; Sandbox retains its original offline path/city spawn and NativeSandboxProbe. No save data, Story checkpoints or mission flags were edited.

## CPU optimization and measurements

New bounded **pass-local exact state reuse** avoids repeating SetPipeline, SetBindGroup (including dynamic offset/count), vertex/index bindings/ranges, viewport, scissor, blend constant and stencil reference when effective state is identical. Reset occurs at every pass boundary. Cached handles are retained until replacement/end, preventing pointer reuse from aliasing stale state. Validation, buffer-hazard marking, resource generations, actual draws, ordering, query readiness and submission policy remain. No new GPU resource, staging arena or frame in flight is added. Metadata is **536 bytes +16 counter bytes**, with at most20 temporary handle references to already bound resources.

Host comparison: original startup shaders; identical96 draws/512 checked pixels/96 exact query samples per replay; **16 order-alternated pairs across two independent processes**, no outlier removal. Native host -O0, shipping iOS -O1; Apple Paravirtual Metal, not A18 Pro. Direct API baseline does not pay cache retain/release overhead. Profiling is enabled equally. Timing categories overlap.

| Per-replay metric (median across16 runs/variant) | Direct state API | State reuse | Result |
|---|---:|---:|---|
| State setter calls | 768 | 134 | 82.55% fewer;634 exact duplicates avoided |
| DRAW active CPU,96 draws | 1.743 ms | 1.649 ms | 5.39% lower |
| Whole service-loop active CPU | 4.881 ms | 4.381 ms | 10.23% lower |
| DRAW total wall, includes cold compilation | 1.784 ms | 1.796 ms | 0.67% higher; no wall-time gain established |
| Per-DRAW p50 | .0025 ms | .0020 ms | Host fixture only |
| Per-DRAW p95 | .0165 ms | .0070 ms | Host fixture only |
| Per-DRAW p99/worst (96 samples/run) | 1.1625 ms | 1.3300 ms | 14.41% higher; cold compile/driver variation remains |
| Queue submissions | 6 | 6 | Unchanged |
| Uniform GPU upload bytes | 4,608 | 4,608 | Unchanged; exact bytes/pixels preserved |

These are medians of replay statistics, not pooled gameplay frame percentiles. Cold pipeline/link calls are inside DRAW walls; driver/process caches may persist between replays. Active CPU reduction and deterministic work removal are verified; **better iPhone FPS or sustained frame pacing is not yet verified**. No existing0.35–0.42 optimization is counted as new.

## Physical0.42 findings and remaining bottlenecks

The full phone log has no user-marked gameplay window. First/last windows are different workloads. Final256 displayed intervals: mean59.836 / p50 50.005 / p95 83.341 / p99 100.009 / max266.691ms; this is not a classified gameplay FPS measurement. Final service-thread CPU mean47.194ms/PRESENT; handler active mean32.109ms and wall39.736ms. Final upload-handler wall mean **3.850ms**, not the older50ms evidence. Cumulative uniform upload traffic4,635,556,352 bytes; queue submissions45,214 (~19.9/protocol PRESENT); staging-pressure waits819. These quantities overlap other reported costs.

Highest measured costs to pursue next (cumulative inclusive opcode costs; do not add nested stages):

| Work | Calls | Service-thread active CPU | Wall | Remaining issue |
|---|---:|---:|---:|---|
| DRAW32 | 2,427,141 |17,617.863ms|25,410.145ms| Descriptor/key/binding/uniform preparation still executes; new reuse targets repeated encoder API work |
| PRESENT7 |2,270|6,436.519ms|7,253.194ms| Includes preceding flush/display preparation; not GPU execution |
| Readback42 |3,957|5,819.017ms|12,933.955ms| Required fresh data/order; no stale-result shortcut |
| Buffer upload12/47 |3,462,261|5,386.265ms|5,453.401ms| Ordered data preparation/copies remain; bytes and dependencies must be preserved |
| Pipeline API |309|229.459ms on this thread|6,285.143ms; max480.102ms| Driver/compiler blocking remains; worst loading frame spent2,176.760ms creating117 pipelines |

Pipeline cache remains256 entries: the phone recorded309 creations/54 evictions/19 recent exact-key recurrences, **zero pressure evictions**. This is different from0.36's tens of thousands of recreations; no blind capacity increase or unsupported prewarming was added. True GPU execution time, GPU saturation, energy, sustained thermal response and full engine CPU breakdown remain UNKNOWN.

## Resource ownership, memory, input and audio

Phone final census52,711 objects /26,549 buffers /8,247 textures, with only65 explicit destroy commands. The bridge cannot safely evict live engine-owned handles based on age alone. The log does not prove a leak. CPU buffer mirrors8,535KiB, metadata886KiB; logical GPU payload peak596,000KiB. Sampled footprint2,556.8MiB (peak sample2,572.3MiB), committed heap1,700.1MiB. These are **not additive** and exclude/overlap driver/physical residency differently. Cleanup returned footprint to109.3MiB. No unsafe asset purge, memory-budget increase or sustained-memory reduction is claimed.

Full journal delivery was7,056,686 bytes with zero producer waits/unrecorded bytes; there is no measured log-I/O bottleneck here. New state counters are fixed scalar metadata and reported through the existing bounded summaries.

Phone input mean/max36.915/624.085ms (839 sampled event-to-hook observations); PCM underruns718,592 frames and late serious thermal state remain baseline observations. The48kHz PCM bridge, DualSense/touch edge ordering/switching, fullscreen, UI, branding, quality presets,640×360 baseline and30 FPS cap are preserved. No0.43 device latency/audio/thermal result is available. No governor or optional quality reduction is claimed to solve a CPU bottleneck. Existing animation/game-clock limitations are not hidden or changed by this work.

## Regression, sanitizer and package status

[Full CI 39187176166: PASS](https://github.com/yosnuffyyy/gta5loader/actions/runs/39187176166). Two independent native runtime processes;4,808 original shader modules/native pipelines,500 clip shaders; both known SRV709 shaders and5633; pixel/byte/query/resource-lifetime/copy/compute/worker/streaming checks;32 existing completion comparisons;32 new state-reuse comparisons;100,000 completion lifecycle cases;71 production input checks;205 UIKit checks;1,374 existing watchdog assertions plus new framed-commit/liveness tests;169 counted original startup/script checks,13 real LF-output cases and5,136 untouched publication bytes. The284 source-manifest entries and original engine digest were audited. Component ASan/UBSan/TSan checks passed where configured; **the complete AOT engine and Metal driver were not fully sanitizer-instrumented**.

Superseded candidates were canceled while fixtures and ownership review were completed. No failed IPA was published. Final integrated CI and delivery verification passed. The42 previous IPA hashes remain intact; protected main, PC folders and original assets were untouched.

- **0.43.0 / build43**, unsigned ARM64/iPhone, iOS17 minimum; unchanged `io.github.yosnuffyyy.gtanativeprobe`, display name GTA V (Test Build).
- `GTA-Native-Probe-0.43-unsigned.ipa`, **104,983,240 bytes**; SHA256 **`405a57c1f7263a68ac3068d1b7cf9310f690ed512ea2749f7c2dfe51cb5d9aed`**.
- Mach-O UUID `f18f4497dc803c7e99adf75c4712e717`; matching source/compiler/symbol evidence retained. No WebKit, writable-executable segment, game/web payload or CI-only fixture payload.
- IPA Actions artifact12159530228; compiler evidence12159290766. Local artifact: `outputs/GTA-Native-Prototype/GTA-Native-Probe-0.43-unsigned.ipa`. Public release: [native-0.43](https://github.com/yosnuffyyy/gta5loader/releases/tag/native-0.43) (publication is verified separately after upload).

## Next physical test and remaining risks

1. Start Sandbox with V2, Low640×360 and the same compatibility options as0.42. Check Launch engine commit: Sandbox, Sandbox player event and then Session live; continue beyond180 seconds.
2. Drive a matched Los Santos route. Test low/medium/high-speed rear-end and head-on collisions, several NPC vehicles, damage/deformation, glass/debris and collision sound. Repeat over15–20 minutes; these collision tests have not been performed for0.43.
3. Mark an actual visible gameplay window after loading; record cold/warm pipelines, display p50/p95/p99, state emitted/skipped deltas, CPU service, memory, thermal, input and audio underruns. Share the complete TXT.
4. Repeat the same location/quality with the selectable original renderer. V2 remains experimental; neither is claimed faster on this iPhone from host tests.
5. Verify stop/relaunch, Story saves/checkpoints after Sandbox, independent namespaces, pause/menu, controller/touch switching, fullscreen and audio.

Physical collision stability, sustained FPS/memory, vehicle damage visuals and audio quality remain pending. Cold pipeline stalls, engine-owned city-resource growth, required readbacks,111 untranslated HS/DS/GS records and unknown/new runtime operations remain risks. Partial fixed sample masks retain explicit rejection. No crash-free Prologue/Sandbox or30 FPS claim is made.

Next recommended engineering task: use matched post-collision driving traces to attribute remaining DRAW preparation and readback/flush CPU, alongside producer resource ownership/release history. Validate any pipeline-preparation or retirement change against exact data and pixels before implementation.

## Changed source files

- `native-ios/App.m`
- `native-ios/Info.plist`
- `native-ios/launch/options.h`
- `native-ios/launch/test_engine_mode.cjs`
- `native-ios/launch/test_publication.c`
- `native-ios/renderer/alpha_coverage.inc`
- `native-ios/renderer/coverage_fixture.inc`
- `native-ios/renderer/coverage_ring_fixture.inc`
- `native-ios/renderer/draw_pipeline.inc`
- `native-ios/renderer/draw_types.inc`
- `native-ios/renderer/inter_stage.inc`
- `native-ios/renderer/native_draw.inc`
- `native-ios/renderer/native_gpu.m`
- `native-ios/renderer/native_metrics.inc`
- `native-ios/renderer/pass_state.inc`
- `native-ios/renderer/recorded_pair_audit.inc`
- `native-ios/renderer/reuse_fixture.inc`
- `native-ios/renderer/shader_audit.inc`
- `native-ios/ui/NativeDashboard.inc`

Reports/manifest: NATIVE-0.43-RESULTS.md, VERIFICATION-0.43.txt, ROADMAP.txt, source-manifest.json. Original engine instructions, game assets and protected project sources were not changed.

## Compact Report for ChatGPT

Native0.43 source9ea0c38bcfe9e673fa7ac7f5ae2a0ef811ce1f24, CI39187176166 PASS, build43. Baseline physical0.42 sourceab51b47752066092280136f8327a8543b8a612ad; complete log hash3e9f978a59b7b852dbe0798e7d48573bce1d7c4cfcb7b1df1dbd557b94f2ec6b. Collision-triggered failure was missing single-sample A2C in grass VS90bb870460e2835c / PS0030c87264713865, DRAW3,48,1,0,0,0. Implemented reflected final-result alpha>=.5 discard, NaN zero, original outputs preserved, native4x A2C retained, explicit output coverage overrides; intermediate D3D coverage approximation matches pinned browser reference. Exact pair16 draws/16,384 checked MRT pixels; both-mode9-alpha/depth/stencil/occlusion/MSAA suites pass. Found separate Sandbox startup deadline bug: real mode stdout includes LF, old exact detector missed it; framing fix plus real player event/subsequent hook/PRESENT retires startup, with20-minute policy and true-stall tests. No mode/save-path changes.

Bounded pass-state API reuse:536B metadata+16B counters, at most20 temporary handle references, no new GPU/staging allocations. Sixteen paired host comparisons:768→134 setters(-82.55%);96-draw active CPU1.743→1.649ms(-5.39%), whole-loop CPU4.881→4.381ms(-10.23%); wall1.784→1.796ms(+.67%); per-draw p95 .0165→.0070ms, p99/max1.1625→1.3300ms(+14.41%, cold compile variation). Six submissions and4,608 uniform bytes unchanged;512 pixels/96 query samples checked per replay. Mac Metal/-O0, shipping iOS/-O1; no A18 FPS extrapolation.

Physical0.42 final256 display mean/p50/p95/p99/max59.836/50.005/83.341/100.009/266.691ms, unclassified workload; service CPU47.194ms/PRESENT, handler active32.109ms, uploads wall3.850ms,45,214 submissions,4.636GB uniforms,819 staging waits. Pipeline309/54 create/evict; max480.102ms;256 cache retained. Live census52,711, buffers26,549,textures8,247,65 explicit destroys; not proof of leak. Footprint2,556.8MiB, heap1,700.1MiB, logicalGPUpeak596,000KiB—not additive. Input36.915/624.085ms mean/max;718,592 PCM underrun frames; serious thermal. GPU time and0.43 sustained behavior UNKNOWN. All4,808 shader/pipelines/500clip,71 input,205UI,1,374 watchdog and applicable component sanitizers pass;42 old IPAs preserved. IPA104983240bytes, SHA256405a57c1f7263a68ac3068d1b7cf9310f690ed512ea2749f7c2dfe51cb5d9aed, UUIDf18f4497dc803c7e99adf75c4712e717. Next phone test:>180s Sandbox, repeated rear-end/head-on/multi-vehicle collisions, matched driving windows in both renderers,15–20min memory/thermal/input/audio plus preserved Story saves. Next optimization: measured DRAW preparation and required readback/flush overhead with ownership traces; no30FPS or60FPS headroom established.
