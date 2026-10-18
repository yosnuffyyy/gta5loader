# Native 0.35 performance audit

Audit target: **Native 0.35.0 / build 35**, source `8047937b21007cea1efad62df2beb7d1c007bb43`. Investigation only; no performance implementation or renderer-setting changes are part of this audit. The user's correction makes **v35**, not v34, the measurement target.

## A. Executive summary

**There is no supplied physical-iPhone v35 performance trace. Its gameplay FPS, actual GPU duration and primary device bottleneck are UNKNOWN.** The reported 7–9 FPS gameplay, 15–20 FPS cutscenes and completed 19-minute Prologue belong to v34. They are historical context, not measurements of v35. Neither a host replay nor a successful iOS build fills that gap.

The strongest available v35 evidence consists of independently checked original-shader Metal replays, native allocator tests, source inspection and a fresh rerun of the exact tested revision. The host adapter is **Apple Paravirtual device**, not an A18 Pro. The CLI engine/translation code uses **-O0**; the iPhone build uses **-O1**. These differences prevent projecting host milliseconds into iPhone FPS.

**Fresh audit result:** exact-source CI attempt 2 passed. Production v35 replay processes measured **12 / 13 submissions**, **4,608 / 5,120 packed uniform bytes**, one completion and zero staging-pressure waits per 96 draws, versus 103 submissions under the legacy comparison. The 192-key cache test again had zero evictions at capacity 256. All checked pixels, buffer bytes and query counts matched. These are new v35 host measurements; their variation and full timing distributions are retained below.

### Top five confirmed cost mechanisms and constraints

These are confirmed in v35 source/host workloads; they are **not five proven remaining iPhone FPS bottlenecks**.

1. **Submission granularity and staging pressure can dominate this translation path.** In the retained v35 96-draw comparison, legacy policy submits 103 times and waits for staging pressure six times; production v35 submits 12 times and has zero such waits. All 512 checked pixels, 96 exact query samples and buffer-order checks agree. This optimization is already present; it must not be proposed as new work or assumed to remove every device stall.
2. **Uniform packing/upload amplification is real, and v35 already avoids a large part of it in the fixture.** Production packs 18 times / 4,608 bytes, versus 192 / 49,152 with reuse disabled or legacy batching. The original protocol payload remains 53,044 bytes. Those are different transfer channels; dividing one by the other does not measure total memory-copy amplification.
3. **Pipeline creation and inter-stage linking remain synchronous on cache misses.** A v35 working-set fixture with 192 required keys and 576 draws creates 192 pipelines with zero evictions at capacity 256. Capacity 128 recreates all 576 and evicts 448. The existing 256 policy is effective for that workload. Actual v35 mission working-set size and expensive device keys are UNKNOWN.
4. **Readbacks and fences still serialize required dependencies.** `readback_map()` submits and waits for mapping; opcode 8 waits for prior submitted work. In the ordered-readback fixture, v35 requires one explicit completion and zero pressure waits; the reference policy requires seven/six. Queue/map wall duration includes scheduling and driver processing, not just GPU execution. The remaining cost cannot be assigned to the GPU without a timeline.
5. **Streaming allocation is constrained by allocator segment geometry, slot count and linear memory, not just physical RAM.** The old 32 MiB segment cannot satisfy the 37,314,560-byte request. V35's native adaptive policy passes the original-root and fragmented-transition fixtures, including a long-session 23/32-slot result. Post-funeral loading on an iPhone remains unverified. Larger retained segments can increase memory commitment; no device footprint reduction is established.

**Most likely primary FPS limitation:** UNKNOWN for v35. Translation/submission synchronization and cold pipeline work are the first hypotheses to test because source and controlled replays establish relevant mechanisms. Confidence that these mechanisms exist is high; confidence that any one dominates sustained A18 Pro gameplay is low without a v35 trace. GPU saturation, physics dominance and thermal throttling are not established.

The most useful next work is a phase-separated v35 device trace, followed by measured work on dependent-upload batching, repeated pipeline/link work and remaining uniform repacking. Do not increase cache/staging budgets or lower quality based on the older totals.

### Provenance and reproducibility

| ID | Evidence | Origin and limits |
|---|---|---|
| V35-S | Source above; local native-file preservation snapshot | Static analysis; no device execution |
| V35-H1 | [CI run 39078684670, attempt 1](https://github.com/yosnuffyyy/gta5loader/actions/runs/39078684670/attempts/1); [retained raw runtime log](evidence/0.35/native-engine-test.txt) | Two fresh native CLI processes on macOS / Paravirtual Metal, -O0; original shader fixtures, synthetic command sequences |
| V35-H2 | [Fresh CI attempt 2, PASS](https://github.com/yosnuffyyy/gta5loader/actions/runs/39078684670/attempts/2), job 117536797785; [new raw log](performance-audit/0.35/native-engine-test.txt) and [parsed measurements](performance-audit/0.35/replays-attempt2.json) | Exact source, two more native processes; same host limitations, not a new gameplay build |
| V35-W | Original pinned Wasm allocator/reference tests rerun during this audit | Host Node/WebAssembly test exports and controlled allocator constants; not the native adaptive policy or real scene |
| V35-B | Read-only inspection of the released ARM64 IPA | File layout, UUID and symbols; no execution or residency measurement |
| V34-U | User's v34 physical-device observations and approximate cumulative totals | Physical iPhone report, no complete phase-correlated raw trace supplied for this audit |

Released v35 IPA: `GTA-Native-Probe-0.35-unsigned.ipa`, **102,339,097 bytes**, SHA-256 `8e4744e7f027109934a0477a0b32b377f8af6364907fa39373b92d5ea768110c`. Mach-O UUID **54C10F1A-5313-37AD-BC46-2EE7BDA32930**. [Preserved release](https://github.com/yosnuffyyy/gta5loader/releases/tag/native-0.35). No replacement IPA is needed for an audit that changes no runtime code.

## B. Frame-time budget

The 30 FPS budget is **33.333 ms/displayed frame**. A 20 FPS comparison budget is 50 ms; 60 FPS-equivalent processing headroom would be 16.667 ms. These are targets, not achieved timings.

### Actual v35 device frame budget

| Component | Samples | Mean / p50 / p95 / p99 / max (ms) | Current measurement status |
|---|---:|---|---|
| Actual drawable display intervals | 0 supplied | UNKNOWN / UNKNOWN / UNKNOWN / UNKNOWN / UNKNOWN | Existing optional `presentedTime` monitor can collect this on v35 |
| Engine simulation CPU and frame duration | 0 | UNKNOWN | No simulation-boundary CPU timer; pad-hook count is not a simulation-frame count |
| Render-thread active CPU | 0 | UNKNOWN | Engine wall-time labels do not establish active CPU work |
| Native GPU-service thread CPU between PRESENTs | 0 supplied | UNKNOWN | `CLOCK_THREAD_CPUTIME_ID` exists, but no v35 device report supplied |
| Native command-handler wall/PRESENT | 0 supplied | UNKNOWN | Includes nested waits and API work |
| Pipeline API / shader-module / link wall | 0 supplied | UNKNOWN | Separately sampled, nested within command handling |
| Upload handler and readback map wall | 0 supplied | UNKNOWN | Overlap command handling and sometimes completion waits |
| GPU execution / vertex / fragment / compute time | 0 | UNKNOWN | No timestamp feature negotiation or timestamp query instrumentation |
| Presentation acquire/blit/display queue delay | 0 | UNKNOWN | Not separately timed; display callback measures completed display, not its CPU/GPU breakdown |
| Thermal, OS pressure and sustained input latency | 0 supplied | UNKNOWN | v35 has summaries; no synchronized physical trace supplied |

### Retained v35 host production-policy replay (V35-H1)

Each row is a **whole 96-draw fixture**, not one frame. It contains no PRESENT or display surface. Process order, driver caches and nested waits are not randomized or isolated.

| Metric | Process / samples | Mean | p50 | p95 | p99 | Max / total |
|---|---|---:|---:|---:|---:|---|
| DRAW handler wall, production mode 2 | 1 / 96 | 0.018 ms | 0.003 | 0.035 | 1.135 | max 1.135; total 1.741 ms |
| DRAW handler wall, production mode 2 | 2 / 96 | 0.019 ms | 0.003 | 0.060 | 1.030 | max 1.030; total 1.855 ms |
| All command-handler wall, mode 2 | 1 / one replay aggregate | — | — | — | — | total 108.280 ms |
| All command-handler wall, mode 2 | 2 / one replay aggregate | — | — | — | — | total 97.490 ms |
| Upload-handler wall, mode 2 | 1 / one replay aggregate | — | — | — | — | total 2.601 ms |
| Upload-handler wall, mode 2 | 2 / one replay aggregate | — | — | — | — | total 3.063 ms |
| Pipeline API wall, 192-key / 256-cap fixture | 1 / 192 creates | — | — | — | — | total 20.952 ms |
| Pipeline API wall, same fixture | 2 / 192 creates | — | — | — | — | total 21.612 ms |
| Inter-stage link wall, same fixture | 1 / 192 misses | — | — | — | — | total 9.651 ms |
| Inter-stage link wall, same fixture | 2 / 192 misses | — | — | — | — | total 9.686 ms |

The much larger all-handler aggregate cannot be explained by subtracting only DRAW and upload time: copies, readbacks, fences, creation and other handlers remain. The existing fixture output does not retain a complete opcode-time partition. **Unattributed remainder is UNKNOWN**, not automatically GPU time. A mean calculated from an aggregate is not a latency percentile.

### Fresh exact-v35 measurements from this audit (V35-H2)

CI attempt 2 **PASS**, same source and fixtures. Each row has 96 DRAW samples; all 512 checked pixels, 96 exact query samples and ordering checks passed.

| Process | DRAW mean / p50 / p95 / p99 / max (ms) | DRAW total (ms) | All-handler / upload-subset totals (ms) | Batches / submits | Uniform packs / bytes |
|---|---|---:|---|---|---|
| 1 | 0.016 / 0.003 / 0.011 / 1.047 / 1.047 | 1.503 | 117.696 / 2.731 | 7 / 12 | 18 / 4,608 |
| 2 | 0.057 / 0.011 / 0.072 / 3.003 / 3.003 | 5.506 | 125.560 / 6.812 | 8 / 13 | 20 / 5,120 |

Both retain 93 independent / 3 dependent pending-buffer updates, one explicit completion, zero pressure waits, one pipeline creation / 95 hits and the same 53,044-byte protocol payload. Fresh process 2 has one extra batch/submission and two extra uniform packs compared with the other retained runs. Counts as well as wall time can vary under the bounded ready-drain policy; no dedicated reason record establishes which guard caused this difference. It is not a pixel, data or ordering failure. Do not force all repetitions to match an older 12-submit result.

Fresh legacy-reference handler/upload totals are 422.975/50.945 ms and 469.861/74.892 ms. Dependency-only totals are 104.832/5.129 ms and 152.759/4.655 ms. Fixed order and uncontrolled driver/scheduler/cache state remain. No new optimization was applied between attempts, and this variation is not a device regression measurement.

### Timing containment and boundaries

`renderer/native_gpu.m:execute()` starts a monotonic timer before `execute_impl()`, adds its wall duration to handler and applicable upload/allocation categories, then calls `metrics_present()` **after a successful PRESENT handler**, using that handler's end timestamp. The current v35 code includes that PRESENT's handler cost in its interval; this was checked directly rather than inferred from older code.

- Upload, allocation, pipeline, link, module, map and completion categories are **not additive**. A buffer upload may first flush draws and wait for staging pressure, so its handler time is not a pure memcpy or API time.
- Drain-end flushes, service polling, report formatting and producer idle occur outside `execute()`; they can contribute to PRESENT interval/service CPU without appearing in handler wall. Some drain flushes contain completion waits already counted in the completion series.
- Service-thread CPU is active CPU charged to **one thread**; other engine threads, driver compiler threads and GPU execution are excluded. It is neither engine CPU nor total process CPU. CPU-time clock failure produces missing samples, not a valid zero-cost measurement.
- The first successful PRESENT seeds intervals. Pre-first-PRESENT work has no normal interval sample; cold windows mean first 64 intervals, not a known loading phase. First compute is not a scene detector.
- A successful protocol PRESENT can return after a drawable timeout without a displayed frame. `surface_frames` counts successful surface presentation submissions, not completed display. It must not replace the drawable monitor.

`timing_window.h` stores latest **256 samples** per series, plus lifetime count/sum/max. Quantiles use nearest rank; p50 is the lower median for even counts. Quantiles are withheld below 20 samples. The log phrase `latest 96 ms mean/...` means **96 samples, durations in ms**, not a 96-ms observation window. Lifetime means and rolling quantiles must not be mixed.

## C. CPU engine breakdown

The engine is ahead-of-time translated by WABT into 128 native C partitions with SIMDe support, then compiled for ARM64. It is **not running through browser JavaScript or a browser Wasm interpreter** in this IPA. Browser-reference measurements cannot be substituted for this native execution path.

Current source retains bounds checks, atomics, `-frounding-math`, `-fno-optimize-sibling-calls` and the shared-memory ABI. iPhone uses -O1; native CLI uses -O0. That optimization-level difference alone makes host CPU cost ratios unsuitable for projecting device function costs. No unsafe compiler flags were added.

### Engine functions mapped for subsequent profiling

The original, hash-pinned Wasm name section has **91,111 named functions**. The following mapping is static source evidence, **not a hot-function ranking**:

| Subsystem | Original function / native symbol | Inclusive CPU | Exclusive CPU | Active CPU % / calls per frame / percentiles |
|---|---|---|---|---|
| Visibility kick | `CGameScan::KickVisibilityJobs()` / `w2c_gta_f47953` | UNKNOWN | UNKNOWN | UNKNOWN |
| Visibility wait | `COcclusion::WaitForAllRasterizersToFinish()` / `w2c_gta_f43492` | UNKNOWN | UNKNOWN | UNKNOWN; waiting must be separated from raster work |
| Skeletons | `CDynamicEntity::UpdateSkeleton()` / `w2c_gta_f46429` | UNKNOWN | UNKNOWN | UNKNOWN |
| Physics | `rage::phSimulator::Update(float,bool)` / `w2c_gta_f16020` | UNKNOWN | UNKNOWN | UNKNOWN |
| Scripts | `rage::scrThread::Run(int)` / `w2c_gta_f16953`; `Update(int)` / `f16959` | UNKNOWN | UNKNOWN | UNKNOWN |
| Resource allocation | `rage::pgRscBuilder::AllocateMap(...)` / `w2c_gta_f2040` | UNKNOWN | UNKNOWN | UNKNOWN |
| Streaming flush | `rage::strStreamingLoaderManager::Flush()` / `w2c_gta_f6211` | UNKNOWN | UNKNOWN | UNKNOWN |
| Pad update | `rage::ioPad::Update(bool)` / `w2c_gta_f10454` | UNKNOWN | UNKNOWN | UNKNOWN; native hook timing measures delivery, not the whole update |
| Main simulation, AI, collision, animation jobs | Names/source mapping available, active call stacks not collected | UNKNOWN | UNKNOWN | UNKNOWN |

Inlining and wrapper symbols can move apparent cost. Resolve native symbols/addresses from the exact binary before attributing samples to original functions. The native stack import currently returns no Wasm-PC trace; it is not a substitute for Instruments call-stack sampling.

Past reports of `Wait for render`, `Wait on queued frames`, `Frame Limit`, visibility events and exposure readback identify **wall-time dependency labels**. Their percentages do not form an active-CPU function profile, are not from a supplied v35 run, and are not reproduced as v35 percentages here. A frame-limiter sleep at low average FPS may coexist with uneven frame pacing; its removal is not justified by the label alone.

### Native CPU boundaries worth sampling

- `native_host.c:host_load()/host_store()` access shared bytes with relaxed atomic operations after bounds validation. This is used by file delivery, input and host ABI copies. Per-byte operations are explicit in source; their actual CPU share is UNKNOWN.
- `native_wait.c:wait_value()` uses 128 bucket locks and condition variables, with cancellation/mailbox service outside bucket locks. Notifications/pokes wake relevant waiters; the 100-ms safety-check interval is **not an unconditional 100-ms input delay**. No busy loop was introduced.
- `threading/native_threads.c:native_thread_tick()` amortizes full deadline/I/O checks every 256 clock ticks and services pending mailboxes. Worker setup allocates 4 MiB stacks, up to 32 slots; virtual stack capacity is not resident stack usage.
- Host filesystem operations serialize on `file_mutex`; heap growth uses `grow_mutex`; the archive reader is separately single-threaded. Lock contention, syscall cost and current hot engine functions require a physical CPU/thread-state trace.

No engine-function exclusive/inclusive times or call counts were fabricated. CPU ranking by active samples is an unresolved measurement deliverable, not evidence that all subsystems cost the same.

## D. Graphics translation breakdown

The native path is: **native engine command serialization → shared ring publication → GPU-service pthread → native state/resource translation → wgpu-native → Metal**. It avoids JavaScript message serialization at runtime. `gpu_ring.c:gpu_ring_drain()` validates header/payload bounds, acquires the producer head, reads immutable inline payloads, calls the executor synchronously and release-publishes the consumed tail. Its header decoding, wakeups and checks are not separately timed from handler processing in production reports.

| Operation | v35 evidence | Source and cost interpretation |
|---|---|---|
| DRAW opcode 32 | 96/replay; 512 checked pixels; 96 query samples | `native_draw.inc:draw_execute()`; handler includes program selection, shadow/storage variants, pipeline lookup/create, proxies, uniforms, bind groups and API encoding |
| Native submissions | 12/replay, mode 2 | `native_gpu.m:submit()`; counts explicit wrapper calls, not every internal driver upload or completed frame |
| Draw encoder batches | 7/replay, mode 2 | `native_draw.inc:draw_flush()`; a batch can contain many separate render passes |
| Buffer uploads while an encoder is pending | 93 independent / 3 dependent in mode 2 | Dependency counters cover only this condition; **not all upload commands** |
| Protocol upload payload | 53,044 bytes/replay | Inline buffer/texture payload counter; excludes packed uniform writes and internal GPU copies |
| Uniform preparation | 18 packs / 4,608 GPU-upload bytes in mode 2 | `draw_group()` and `draw_flush()`; exact bytes/zero fill and batch lifetime checked |
| Pipeline reuse | 1 create / 95 hits in submission fixture | Different from the separate 192-key working-set fixture |
| Readback/query/fence | Exact data and results checked | These deliberate correctness dependencies make this a synchronization-heavy synthetic workload |
| Decode cost, state commands/sec, bind-group create/hit rates | UNKNOWN | Per-op counts exist internally, but full per-op costs and bind-group cache hit counters are not emitted by current v35 |

### State and binding work

`draw_pipeline()` constructs a zero-initialized semantic `DrawKey`, removes unused vertex declarations, normalizes irrelevant state, and scans up to 256 cache slots with exact key comparisons. There is no hash-indexed lookup. This is a static high-frequency path, but lookup CPU share is UNKNOWN; a 1,468-byte key does not imply every failed comparison reads all 1,468 bytes.

`draw_group()` reuses one immutable uniform slice per live program/batch after exact source comparison. Bind-group reuse uses resource IDs, a global resource epoch, per-ring slot and the reference-shadow mask. A resource destruction bumps the global epoch; this can invalidate unrelated cached groups as well. That is conservative correctness, not a demonstrated current bottleneck. Binding object generations separately reject stale SRV/UAV ownership after handle reuse.

Even on a pipeline/group hit, `draw_execute()` sets pipeline, groups, vertex/index buffers, viewport, scissor, blend constants and stencil reference. Repeated setters are observable in source; how much wgpu already suppresses internally and their v35 device CPU cost are UNKNOWN. Eliminating them needs per-pass state tracking and tests for pass restart, compute/copy boundaries and dynamic offsets.

`draw_prepare_proxies()` caches a no-proxy scan for unchanged bindings/epoch. Actual proxy subresources are refreshed when the source content version changes; this is not an unconditional copy of every depth texture on every draw. `draw_dirty_targets()` uses actual shader writes/depth/stencil state. Repeated refreshes may be required.

There is no retained full v35 Prologue command capture in this audit. The fixtures reproduce original shaders and protocol contracts; they do not establish the mission's opcode frequency or a per-gameplay-frame command average.

## E. GPU rendering breakdown

**Genuine v35 A18 Pro GPU duration, utilization, occupancy, bandwidth, overdraw, vertex/fragment/compute split and pass costs: UNKNOWN.** Host tests execute real Metal work and read real pixels/query samples, but no GPU timestamps are collected. Correct GPU output is not a timing measurement.

`native_gpu.m:hardware()` negotiates BC/BC3D, clip distances, normalized formats, depth/stencil, filtering and other required features. It does **not** request timestamp queries, create timestamp query sets or read a GPU timestamp period. Existing type-2 protocol query output samples `now_ns()` on the CPU **before** its draw flush/completion wait, then publishes that value after completion. It must not be interpreted as a GPU hardware timestamp or as the timestamp of actual GPU completion. This is also a timestamp-contract accuracy limitation for any engine timing consumer; it is unchanged by this audit.

The pinned Metal HAL has timestamp-query support subject to device counter-sampling capabilities; its adapter gates it on supported stage boundaries. This is a viable measurement route, not proof of availability on the selected v35 adapter until queried. Timestamp differences require backend-supported sampling points, result resolution, period conversion and bounded asynchronous readback. They would measure instrumented scopes, not all system GPU utilization. The pinned source was inspected locally; the general API explanation is in [wgpu timestamp semantics](https://docs.rs/wgpu/latest/wgpu/struct.Features.html), with platform behavior described by [Apple counter sampling](https://developer.apple.com/documentation/metal/sampling-gpu-data-into-counter-sample-buffers).

Current capabilities cannot be copied wholesale from Paravirtual Metal to A18 Pro. The retained host reports, for example, uniform/storage alignment 256/256 and an inter-stage limit of 31; earlier iPhone reports had different limits. V35 logs its actual negotiated capabilities on device.

### GPU workloads established by source

- The final surface is BGRA8, FIFO, 640×360, aspect-fit; actual offscreen targets retain the engine's descriptors. Resolution alone does not determine shadow, reflection or post-processing bandwidth.
- Ordinary draw passes load/store existing color/depth/stencil attachments. Clears, proxies, copies, queries and post-processing can create additional pass boundaries. Counted encoder batches are not counted render passes.
- Depth-as-color and color-as-depth proxies add fullscreen conversion passes when stale. Sample-correctness tests exist; phase-separated GPU cost does not.
- Occlusion results use actual GPU sample counts in four bounded pages of 64 segments. Query segmentation follows render-pass boundaries; 96 segments in the replay are **not 96 logical game queries**.
- Compute/UAV operations and unaligned byte-edge updates encode real GPU work. Their device fraction is unmeasured.
- PRESENT uses a fullscreen texture-load blit and obtains a drawable. This adds work beyond the engine draw counter. Draw counters also do not count all proxy/presentation work or every manually expanded instance batch.
- Blurry floor reflections cannot be diagnosed from the 640×360 profile alone. Target resolution, selected mip, sampler and actual pixels of a v35 frame are needed. No reflection quality setting was changed.

A Metal System Trace and GPU capture are the minimum way to separate GPU busy time, CPU submission gaps, pass bandwidth and presentation waiting. Apple documents a Game Performance template containing CPU/thread-state, GPU, display and thermal tracks, with optional hardware counters. These tools require an appropriate Mac/device/profiling connection; they are unavailable in this Windows session. [Apple profiling workflow](https://developer.apple.com/documentation/xcode/analyzing-the-performance-of-your-metal-app).

## F. Memory and streaming

### Distinct memory quantities

| Quantity | What v35 measures / constrains | What it does not mean |
|---|---|---|
| Linear memory limit | 4 GiB shared addressable memory; mmap base stays stable | 4 GiB physical RAM reserved for this app |
| Linear committed/high-water counter | `host->memory.size`, reported as heap committed | Process RSS or live engine allocations; pages can be sparsely resident |
| Engine allocator retained heap | Sum of retained child-segment capacity | Bytes of currently live resources |
| Allocator live/free/largest | Locked original allocator snapshots on retry/failure | OS available RAM; largest-block queries that can grow the heap must not be used as passive sampling |
| Growth target | Original per-allocator preflight/growth target; class-1 test root uses 1 GiB | A global maximum game-memory budget |
| Logical engine GPU payload | Descriptor-based buffer/texture byte estimate and peak | Driver allocation/residency, mirrors, uniform rings, pipelines, every proxy or physical RAM; depth24 is estimated |
| Process footprint | `task_vm_info.phys_footprint`, at most once/second | Exact peak; very short spikes can be missed |
| App headroom | `os_proc_available_memory()` snapshot | Guaranteed future allocation budget |

### Funeral failure and v35 status

The user reported v34 failing **37,314,560 bytes (35.586 MiB)** during `ch1_instance_placement.rpf`, with largest free block 0, reported available 16,777,216 and NULL destination entries. The source-backed defect is independently reproducible: the reduced-memory allocator's GrowBuddy growth was capped at 32 MiB; each segment loses an 8 KiB sentinel, and its largest initial buddy block is only 16 MiB. The request rounds to a **64 MiB buddy allocation**, which cannot fit a 32 MiB segment. There are only 32 child-segment slots.

V35 already changes native generated-C request growth, **not the original Wasm or game archives**, to choose bounded 32/64/128/256 MiB segments while maintaining a slot reserve. The 37.3 MB request selects 128 MiB under this policy. A 64 MiB+8 KiB isolated segment can fit the block; 128 MiB is the selected power-of-two policy, not a mathematical minimum. In a repeated mixed-size reference experiment, the smaller option retained at least as much memory across two segments, versus one for 128 MiB.

The prior `GetMemoryAvailable` profile could unsigned-wrap after retained/live allocation exceeded its growth target. V35 returns retained free plus bounded logical growability, constrained by remaining slots, the original segment maximum and the 4 GiB address limit. **Actual-free-only is not the streamer's full preflight contract** and can prevent valid growth. No allocation or successful read is fabricated.

Verified native results retained in V35-H1: actual class-1/root 64 MiB allocation succeeds; fragmented-transition tests execute 64 cycles/576 allocations per fixture invocation with byte guards and full live cleanup; retained 167,755,776 bytes, peak live 67,272,704. The adaptive held-allocation fixture uses 23/32 slots before the late large allocation, versus legacy exhaustion at 32/32. The separate constant-128-MiB Wasm comparison uses seven slots; that is **not** the adaptive native result.

Nonempty NULL archive destinations are rejected in `native_assets.c:serve()` before I/O or copying. That prevents corrupt downstream reads; it does not itself make an out-of-memory game load succeed. The report retains the last eight allocation/request contexts and distinguishes pending/cancelled work from successful reads.

**Still UNKNOWN:** actual post-funeral v35 retained/live/largest blocks, linear high-water, iOS headroom, temporary Prologue retention, asset ownership leaks or an OS jetsam event. The old failure establishes an allocator defect; it does not establish that iPhone physical RAM was exhausted, that all Prologue assets leaked, or that v35 has passed the next-scene transition. Increasing segment size can increase retained memory. No forced eviction or budget increase was added during this audit.

### Streaming and native resources

The reader remains **one 64 KiB scratch buffer, at most eight cached descriptors**, one reader thread, up to 512 request slots. Service timing starts after a slot is acquired and excludes queued delay. Each request may include open/cache lookup, `fstat`, `pread`, bounded scratch copying and shared-memory stores. Cumulative delivered bytes do not measure disk bytes if cache hits or clipping differ. Local files use no HTTP whole-archive cache.

`serve()` performs `pread` into scratch then `host_store` into the validated destination. These are two explicit destination writes in source, not a measured physical-memory-bandwidth ratio. Packed uniform uploads follow a different channel: protocol bytes → CPU constant shadow → immutable packed mirror → wgpu upload machinery. Padding/zero fill, reuse and CPU-only constants mean there is no valid single amplification ratio from existing totals.

Uniform storage is three 512 KiB GPU buffers plus a 512 KiB CPU mirror; queued **uniform** staging is bounded to 16 batches / 4 MiB. This is not a global bound on all protocol uploads or internal wgpu/driver staging memory. Texture/buffer readback staging is reusable, at most 1 MiB. Query bookkeeping estimates 17 KiB/page, four pages maximum, excluding native/driver padding. Depth proxies have a 256 MiB total logical budget. Pipeline handles are bounded at 256, reduced to 128 under headroom pressure, with unknown opaque driver bytes. These bounds are preserved.

The 256-pipeline cap does not bound every shader module, bind group, view or CPU mirror. Live resource objects retain their original program/source and bounded per-program variants (eight storage-format variants and four inactive-shadow variants), with cached groups for the three uniform slots. Object destruction frees this owned state. Total long-session program/native-driver retention is UNKNOWN without lifetime/allocation traces; per-object bounds are not a global memory budget.

### New read-only executable-footprint finding (V35-B)

The released binary is **411,568,952 bytes (392.50 MiB)** uncompressed. `__TEXT` occupies 230,866,944 file bytes; `__LINKEDIT` occupies 176,884,536. Its 4,048,948 symbol entries occupy 64,783,168 bytes, and their string table 106,701,096: **171,484,264 bytes (163.54 MiB)** together. There are 90,873 external generated-function-name entries; millions of local symbol names are not millions of engine functions. The IPA contains no dSYM bundle.

This is an on-disk/linker-layout observation. Mapping or compressing these bytes does **not** prove equal resident RAM, memory-bandwidth cost or instruction-cache misses. A 4 GiB Mach-O `__PAGEZERO` reservation is likewise not the engine linear memory or physical usage. Preserve symbols/UUID for profiling; any future symbol packaging or code-footprint investigation must establish actual launch/residency benefits before being called an FPS optimization.

## G. Pipelines and shaders

V35's complete host audit validates **4,808 translated modules/native pipelines**: 1,112 VS, 3,667 PS, 29 CS, including 500 clip shaders. **111 HS/DS/GS records remain untranslated.** This is corpus coverage, not 4,808 unique pipelines encountered in Prologue or proof every dynamic pair works. Actual v35 device encountered shader/pipeline counts are UNKNOWN until its final report is collected.

The production cache is 256 entries with memory-headroom hysteresis: below 256 MiB available → 128; recovery to at least 512 MiB → 256. No driver-byte estimate exists. Releasing a cache handle preserves references held by encoded/submitted commands; eviction does not add an explicit CPU GPU-completion wait.

| V35-H1 working-set policy | Draws / required keys | Creates / hits / evictions | Pipeline API wall, processes 1 / 2 | Link wall, processes 1 / 2 |
|---|---|---|---|---|
| 256 production capacity | 576 / 192 | 192 / 384 / 0 | 20.952 / 21.612 ms | 9.651 / 9.686 ms |
| 128 comparison capacity | 576 / 192 | 576 / 0 / 448 | 67.444 / 67.967 ms | 30.554 / 31.420 ms |
| Shrink/recover test | 576 / 192 | 448 / 128 / 256 | Retained in raw evidence | Retained in raw evidence |

Fresh attempt 2 confirms the same 192/384/0 production working-set counts. Its pipeline API totals are **26.613 / 37.165 ms**, link totals **11.538 / 15.021 ms**. The 128-cap comparison gives 66.036 / 96.719 ms API and 30.417 / 40.124 ms link. This timing spread occurs with unchanged source/keys and reinforces the need for repeated, phase-matched device measurements. No combined percentile is inferred from process aggregates.

All verify exact query samples/pixels and encoded-command survival across eviction. The hit fraction for the 256 fixture is 384/576 = **66.667% including its 192 compulsory first-use misses**. The submission fixture's 95/96 hit fraction is **98.958%**. Neither is the gameplay hit ratio.

Existing semantic-key tests create 1 instead of 32 equivalent pipelines while retaining pixels; unused layout IDs/declarations and irrelevant dynamic state have already been normalized. Do not repeat that completed change. Remaining key fields encode shader identity, specialization/storage format, consumed layout/strides, attachment formats, sample count, topology and applicable blend/depth/raster state.

On a miss, `draw_pipeline()` does reflection/link processing and creates temporary linked modules/layouts before synchronous `wgpuDeviceCreateRenderPipeline()`. The API timer excludes lookup/link work. Module API return may not capture every driver compilation/deferred-use cost. The original WGSL-module timer wraps only `wgpuDeviceCreateShaderModule()`; local pack reads, Objective-C regex/reflection, layout preparation and source copies before that call are not included. It excludes linked variants; shadow-variant timing is separate. These timing categories cannot be summed into exclusive compilation time without tracing containment.

The eight worst pipeline records contain CPU API duration, PRESENT ordinal, VS/PS hashes, an FNV key fingerprint and recent-eviction match. The recent exact-key history is only 64 entries, so recreation counts are a **lower bound**. A fingerprint is not a recoverable full pipeline descriptor. There is no automatic scene classifier. Most expensive **v35 device** keys, cold/warm recurrence and scene triggers are UNKNOWN; old 358–673 ms events must not be relabelled v35.

The pinned backend is wgpu-native **v29.0.1.1 / `6aed50955d934ac36049ba8d002034841633ae02`**, wgpu-hal/Naga 29.0.3, with the established iOS BC capability patch. Its Metal `create_pipeline_cache()` returns a placeholder cache object. A generic wgpu cache wrapper is **not** evidence of persistent Metal binary archives. No persistent archive or unverified async compiler was added. A future prewarm must use real observed keys, preserve original layouts/specialization and obey a bounded memory/work budget.

## H. Uploads and synchronization

### Controlled v35 comparisons

| Policy, per 96-draw replay | Draw batches | Wrapper submits | Uniform packs / bytes | Explicit completions | Staging-pressure waits | Inline protocol payload |
|---|---:|---:|---|---:|---:|---:|
| Legacy upload/drain reference | 98 | 103 | 192 / 49,152 | 7 | 6 | 53,044 bytes |
| Dependency-aware, old drain boundary | 9 | 14 | 22 / 5,632 | 1 | 0 | 53,044 bytes |
| **Production v35** | **7** | **12** | **18 / 4,608** | **1** | **0** | **53,044 bytes** |

Counts agree in both retained processes. Production versus legacy: **88.350% fewer explicit submissions, 90.625% fewer packed uniform bytes** in this fixture. Dependency-aware versus coalesced reduces 14→12 submissions and 5,632→4,608 bytes. Its handler wall varied in opposite directions across processes (89.713→108.280 ms; 111.922→97.490 ms), so **coalescing-alone CPU speedup is not established**. These are existing v35 changes, not new optimizations made in this audit.

Only one explicit completion and zero pressure waits remain in the production replay. This does not establish that staging waits are absent from a 19-minute v35 game session. Likewise 12/96 = 0.125 wrapper submissions per synthetic draw is a workload ratio, not submissions per gameplay frame.

### Required ordering and remaining boundaries

- `draw_flush()` ends the pass, uploads the immutable uniform prefix, finishes/submits the encoder and initiates query maps. It retains queue ordering for ring reuse rather than waiting every slot wrap.
- `draw_buffer_epoch` tracks buffers actually referenced by the pending encoder. Independent uploads avoid submitting that encoder, while preserving the old render-pass boundary. Dependent updates remain conservative. Unknown dependencies, copies, destruction, fences and readbacks retain necessary ordering.
- At most four already-ready 256-command drains are coalesced, with a 4-ms check between drains. No work is deliberately awaited to fill a batch. Individual slow API calls are not bounded to 4 ms. Idle never retains an unsubmitted encoder.
- Aligned buffer updates use `wgpuQueueWriteBuffer`; unaligned edges use a real read/modify/write compute path and a separate submit. A single logical upload can therefore cost more than one native operation. Frequency/byte distribution on v35 iPhone is UNKNOWN.
- Texture uploads use real subresource descriptors/bytes; whole-resource copy counts and byte-edge dispatch counts are not disjoint from wrapper submissions. `wgpuQueueWriteBuffer`/WriteTexture operations can cause backend staging/transfer work not counted as a call to the project's `submit()` wrapper.
- FENCE, timestamp/event publication, query-page pressure, staging pressure and synchronous readback all have distinct causes. Only aggregate completion timings and selected cause counters are available; a full flush-reason/time partition is missing.

### Exposure/readback stall interpretation

The previously reported ~2,777-ms readback wait / ~2,845-ms freeze is historical, not a v35 measurement. It cannot be assigned to GPU copy time from the engine stack label alone. The engine can wait behind earlier ring commands, compilation, resource preparation, queue work and callback scheduling before receiving its reply.

V35 records submit-to-map wall for readbacks, bounded first/slow contextual messages, completion waits and slow-command/PRESENT records. `readback_map()` excludes earlier engine/ring waiting, full destination-copy cost and other readback chunks. The code polls callbacks and sleeps 1 ms only while incomplete, then retires earlier uniform staging on successful ordered mapping. It never returns stale/fake data.

The 96-draw/13-readback fixture verifies identical bytes/pixels, 96 exact occlusion samples and pressure waits **6→0**, completions **7→1** under the existing corrected policy. Physical exposure frequency, one-time initialization versus repeats, and any remaining v35 long stall are **UNKNOWN**. Collect the first exposure map and a later warm exposure map with the surrounding native and engine timeline before redesigning this synchronous engine dependency.

### Historical numbers excluded from v35 totals

The user's v34 summary reported ~3.35 million wrapper submissions, ~2.81 million draw batches, ~9.51 million engine draws, ~15 million protocol-buffer uploads, 123,319 pressure waits and ~25 GB uniform traffic. These are approximate all-session values mixing phases, not v35 measurements. Total displayed/simulation frames and comparable byte channels are absent; uploads/frame, GB/s, redundant-byte fractions and copy amplification cannot be calculated reliably. Older ~50 ms upload/PRESENT and 117.5 ms input latency reports were identified as Native 0.29. They are not v35 baseline values.

## I. iOS hardware and thermal behavior

The target is A18 Pro, but v35's P-core/E-core residency, CPU frequency, GPU power state, limiter counters, sustained temperature and memory-pressure timeline are **UNKNOWN**. The engine's `-numCores=2` and host logical-core return value 2 are a bounded engine startup profile, not an OS affinity setting or a count of running workers. Changing them without active-CPU/job traces risks more contention or worse memory pressure.

The engine owner is an NSThread with UserInitiated QoS and a 4 MiB stack. Game workers, archive service and GPU service use pthreads without explicit per-thread P/E-core assignment. Their actual inherited/effective QoS, scheduler delays and priority inversions need Thread State Trace. No affinity or thermal-management tricks were applied.

V35 logs `NSProcessInfo.thermalState` every five-second startup/status update, and logs sampled app headroom/footprint. That identifies reported states, not exact throttling/frequency or causality. A near-simultaneous serious state and slow frame is correlation until controlled runs/system tracks support it. The main UI and controller callbacks share the main queue; periodic log formatting is bounded but its event impact is unmeasured.

FIFO presentation, fixed 640×360 drawable and engine frame cap remain. The host baseline arguments retain `-frameLimit=2` (30 FPS), two logical engine cores and the established low-quality profile. No UI-only FPS counter drives simulation. No frame cap was changed in this audit.

The pinned Metal HAL uses memoryless textures only for resources explicitly marked TRANSIENT and supported by the device. Current game attachments can be loaded, sampled, copied or retained across passes. Blind memoryless conversion, store-discard, heap aliasing or render-pass merging would be incorrect. Direct Metal features are not automatically exposed through wgpu. GPU counters must first establish a bandwidth/pass bottleneck and resource lifetime analysis must prove discard/alias safety. No GPU-bound conclusion or quality reduction is justified yet.

Apple documents that `MTLDrawable.presentedTime` is a host display timestamp and is zero for undelivered/dropped drawables; v35 filters zero/invalid/nonmonotonic values. Callback display intervals measure this app's completed drawable presentations, not unique simulation states or visual correctness. [Apple presentation timestamp contract](https://developer.apple.com/documentation/metal/mtldrawable/presentedtime).

## J. Input latency

**Physical v35 touch/DualSense latency: UNKNOWN.** Prior 92–117.5 ms event-to-hook means and ~1.8-second outliers are historical reports, not fresh v35 results.

`input/native_input.c` uses one short mutex, fixed-size state and a **64-entry** queue. Controller events are delivered on the main queue; touch events are UIKit-driven. No independent high-frequency discovery/polling loop is used. Analog snapshots with unchanged digital buttons coalesce while retaining the oldest pending timestamp; digital edges are preserved. Consumption can fold obsolete analog-only snapshots but must retain press/release transitions. Overflow resets to neutral/latest and increments diagnostics rather than leaving a stuck button.

The real synchronous engine pad hook in `native_threads.c:native_language()` consumes at pointer **12840656**, reads a 56-byte pad block and writes its relevant fields at the original update boundary. UI/controller handlers do not write Wasm state asynchronously. Source switching releases/rearms held input; backgrounding/focus loss clears state. Input is independent of the GPU thread, but an engine waiting for render cannot consume events until its next valid hook.

The reported latency is **oldest coalesced native event enqueue → engine consume**, mean/max/count. It excludes Bluetooth transport, UIKit event delivery before the handler, engine action processing after the hook and display latency. Keeping the original timestamp during analog coalescing measures backlog age rather than the freshness of the newest analog sample. There is no p95/p99 latency series or simulation-update interval histogram in current v35. Mean/max cannot reconstruct them.

Fresh v35 bridge-only host measurements: 100,000 event/consume pairs take **30.383 ms (0.304 microseconds/pair)** in the -O1 AddressSanitizer/UndefinedBehaviorSanitizer test, and **210.889 ms (2.109 microseconds/pair)** under ThreadSanitizer. These include sanitizer instrumentation and synthetic events without gameplay contention. They measure processing throughput, not event-to-engine latency, Bluetooth/UI delay or iPhone overhead. They do not prove that rendering stalls are the only latency source.

The overlay is constructed once and redraws for input/layout changes; hidden controller mode disables hit-testing. Status/display text is refreshed on a five-second timer. Log UI is bounded/coalesced and hidden diagnostic text does not receive every expensive text-view update. Existing UIKit tests are synthetic/simulator correctness tests, not measured live iPhone UI overhead.

To separate causes, correlate main-queue event arrival, native enqueue, engine pad consumption and the next displayed frame. Sampling the engine/render/service thread states at the same time is necessary to distinguish low simulation cadence from UI scheduling, queue backlog or an unrelated lock. No input-priority change is warranted until those intervals are measured.

## K. Ranked optimization opportunities

There is insufficient v35 device evidence to identify ten proven FPS fixes. The following **eight evidence-supported investigation candidates** are deliberately fewer than ten; padding this list would create speculative recommendations. Ranking prioritizes likely critical-path relevance, mechanistic evidence, risk and ability to verify. **No new iPhone benefit range is defensible; all are UNKNOWN until measured.** Already shipped v35 improvements are not new candidates.

| Rank / candidate | Source and current evidence | Hypothesis / confidence | Proposed later technique and impact | Complexity / correctness risks | Required validation / phone |
|---|---|---|---|---|---|
| 1. Residual dependent-upload / submission critical path | `native_gpu.m:execute_impl()/loop()`, `native_draw.inc:draw_flush()`. Fresh production replay: 7–8 batches/12–13 submits; 93 independent and 3 dependent pending updates. Legacy comparison establishes strong dependency on boundaries, not redundancy of the remaining ones. | High that boundaries cost work; UNKNOWN which dominate v35 gameplay. Needed writes may prevent batching. | First classify flush cause, resource/range, upload size and consumed epoch. Only then evaluate bounded versioned buffers or batching of provably independent updates. CPU/API/staging benefit UNKNOWN; no promise that all remaining submits disappear. | High; can expose old draws to new bytes, break query or lifetime ordering, retain extra memory. | Exact mixed upload/draw/readback/counter fixtures, old/new byte checks, pixel/query equality, bounded retirement under pressure. Host correctness first; phone for FPS/memory benefit. |
| 2. Repeated link/pipeline preparation on real misses | `draw_pipeline.inc:draw_pipeline()`, `inter_stage.inc:draw_link_modules()`. 192-key v35 fixture spends 20.952–21.612 ms API plus 9.651–9.686 ms link wall, with no evictions. | High that cold misses remain synchronous; device frequency and repeated pair work UNKNOWN. | Capture actual worst keys/pairs/phases; evaluate bounded pair/link-result reuse and preparing known upcoming descriptors during loading. Keep 256 cap. Benefits bounded by measured eliminated work only; device range UNKNOWN. | Medium/high; specialization, storage formats, interpolation, shader lifetime and target state must be part of identity. Async support cannot be assumed. | Exact pair/state pixel comparisons, changing constants/variants, release/recreate tests, cache pressure and cold/warm timing. Phone required for actual mission spikes. |
| 3. Remaining uniform repacking and cross-boundary duplication | `native_draw.inc:draw_group()/draw_flush()`. 18 packs/4,608 bytes vs 192/49,152 in comparison; exact per-batch reuse already shipped. | High that new batch identity requires repacking; proportion of truly duplicate remaining data UNKNOWN. | Measure copied/compared/padded bytes and changed members before considering bounded content-addressed or batch-local multi-slice reuse. Preserve exact zero fill and draw-specific values. CPU/memory benefit UNKNOWN, not another guaranteed 90.6%. | Medium/high; hash collision, truncated/null CBs, ring wrap and stale offsets can silently corrupt shading. | Byte-exact uploads, A→B→A constants, absent/truncated CBs, batch wrap, resource reuse, shader variants. Phone required for bandwidth/FPS effect. |
| 4. Required readback/fence scheduling | `native_readback.inc:readback_map()`, `native_gpu.m:completed()/opcode8`, `native_queries.inc`. Production replay one completion/zero pressure; 13-map fixture preserves exact output. | High that synchronous engine dependencies serialize; extra waiting beyond those dependencies UNKNOWN. | Correlate request publication, service start, queue submit/map and reply. Optimize only proven redundant fences/late submission or use supported pipelining when the caller can tolerate it. GPU/CPU benefit UNKNOWN. | High; returning stale exposure/query bytes or premature retirement is unacceptable. | Delayed completion, cancellation, map failure, source destruction, correct reply atomics/data, same-frame dependencies. Phone trace needed to locate the real stall. |
| 5. Render-pass/load-store and proxy traffic | `native_draw.inc:draw_pass()/draw_end_pass()`, `draw_depth_proxy.inc`, `native_queries.inc`. 96 physical query segments in the production replay despite only seven draw batches; proxies refresh by content version. | High that separate passes/conversions occur; bandwidth cost/mergeability UNKNOWN. | First GPU-capture pass list and first/last attachment uses. Investigate only redundant clears/stores or conservative refresh granularity with complete dependency proof. Do not blindly merge passes or use memoryless storage. | High; GPU hazards, occlusion segmentation, stale subresources and lost attachment data. | Golden pixels/depth/stencil, overlapping scopes, sampled/copied attachments and subresource updates. Physical GPU counters/capture required for benefit. |
| 6. Atomic byte-copy and host ABI hot paths | `native_host.c:host_load()/host_store()`, `native_assets.c:serve()`. Explicit per-byte atomic loops, scratch→shared copy, frequent input/ABI accesses. | High implementation evidence; active CPU percentage UNKNOWN. | Sample first. Where ownership is proven, benchmark legal bulk transfer or reduce duplicate host copies without weakening shared-memory semantics. No compiler/atomics shortcut. | High; races, publication ordering and memory growth safety. | ThreadSanitizer/ASan, immutable-payload ownership, concurrent heap growth/cancellation and original byte equality. Host microbench useful; phone CPU sampling needed. |
| 7. Pipeline/group lookup and invalidation CPU work | `draw_pipeline.inc` scans 256 exact keys; `draw_group()` compares IDs/bytes and uses global epoch. High hit counts in fixtures still execute lookup and state preparation. | High source evidence; share of CPU and amount of unnecessary group invalidation UNKNOWN. | Benchmark last-key/hashing and count group invalidation reasons before choosing a bounded index or narrower ownership epochs. Preserve all semantic key fields. FPS benefit UNKNOWN. | Medium; hash collision handling, shader identity, stale handle reuse and pass-bound state invalidation. | Forced collisions, pressure eviction, resource replacement, specialization toggles, exact pipeline/group/pixel identity. Phone needed if host gain is material. |
| 8. Transition allocation/residency and cold executable footprint | Allocator retry snapshots; v35 fragmented fixtures; binary 392.50 MiB, symbol/string data 163.54 MiB on disk. | High that segment constraints and metadata exist; leaks, residency and sustained CPU cost UNKNOWN. | First prove post-funeral v35 success and record live/retained/largest/high-water/headroom plus VM/resource lifetimes. Evaluate only measured dead-resource retention or code/symbol packaging with preserved symbolication. | Medium/high; premature frees/eviction, larger commitments, lost debugging info. Not an established steady-FPS fix. | Long-session scene transition, allocator guards, original assets, GPU-completion lifetime checks, matched-UUID symbols. Physical memory/VM trace required. |

For every candidate the relevant CPU/GPU/memory impact must be measured separately; host correctness is insufficient for a speed claim. Minimum benefit criterion: improvement beyond run-to-run dispersion in a comparable phase, no pixel/data differences, no unexplained memory/latency regression. Revert an attempted implementation if benefit is unmeasured or inconsistent.

Lower-priority ideas were not promoted: changing logical-core count, unsafe compiler flags, disabling shadows/reflections, raising staging/cache limits, switching wholesale to direct Metal, arbitrary Metal archives, whole-archive caching or lowering resolution. None has adequate v35 evidence for this pass.

## L. Unknowns and missing measurements

### Minimum trace set, in priority order

1. **Exact v35 on-device report plus phase marks.** Confirm app version/build 0.35/35 and source/IPA identity above. Keep 640×360, 30 cap, same input/shadow options and comparable thermal state. Collect separately: initial startup; a repeatable 30–60-second controllable Prologue route; a cutscene; vault/guard transition; funeral→next-scene load. Use the existing user-marked display window at the start of each visible segment and export its report immediately before resetting the marker. This is manual phase annotation, not automatic gameplay detection. A steady window should have at least 20 interval samples; 100+ is preferable, but no sample count turns one short run into sustained evidence.
2. **CPU/thread-state + Metal System Trace**, same workload/time window. Record active CPU inclusive/self sample weights separately for engine owner, render, workers, GPU service, archive, UI and driver threads. Capture waits/locks and scheduler states too. Use exact native symbols; the IPA retains function symbols, while original Wasm indices can be mapped with the audit helper. If attach/symbolication is unavailable, record that limitation rather than inventing function timings.
3. **One GPU frame capture during gameplay and one during a stall/transition**, with pass timings, target descriptors and limiter counters where supported. Determine actual GPU busy intervals, vertex/fragment/compute distribution, load/store traffic and CPU feed gaps. A queue-completion wait alone cannot answer this.
4. **Memory/VM/resource trace around funeral transition** with the v35 locked retry snapshots and most recent asset requests. Correlate linear commitment, per-owner live/retained/largest/slots, process footprint, OS headroom, native GPU resources and completion-based retirement. Capture a jetsam/OS termination report only if an actual OS kill occurs.
5. **Input timing window** during the same gameplay route. Pair native enqueue/consume diagnostics with main-thread scheduling and actual displays. To obtain p95/p99 or engine-update cadence, a future optional bounded timestamp series is required; current mean/max is insufficient.

On a Mac, Apple recommends the Game Performance template in Instruments for parallel CPU/thread/GPU/display/thermal context. Keep expensive GPU validation/captures separate from baseline performance recordings because diagnostic tools can perturb timing. Use the existing signed installation only if it permits profiling attachment. This unsigned IPA is not a provisioning profile. If permissions prevent attachment, a developer-authorized signing/profiling setup is needed; no credentials, signing identities or entitlements were modified by this audit. [Apple Metal tools](https://developer.apple.com/metal/tools/).

The user marker resets **only the marked display timeline**. Native service timing windows remain the latest 256 all-run samples; marking gameplay does not retroactively classify them. Take counter/time snapshots before and after a route and retain timestamps/PRESENT ordinals. Counter deltas can describe that interval if both boundaries are captured; rolling CPU percentiles can still include earlier phases. Do not present them as phase-pure quantiles without aligned samples. Separate profiler-off and profiler-on runs are needed to estimate collection perturbation.

If native function names do not symbolicate, preserve the matching binary/UUID and produce a separate **-O1, same-source** build with debug information and matching symbols. Do not compare a Debug/-O0 build to production and attribute the difference to the phone. Do not claim existing CI's Paravirtual GPU or Windows Metal shader tools can replace an iPhone GPU trace.

### Measurement gaps and minimum instrumentation, only if tools cannot supply them

| Gap | Minimal proposed measurement | Why current metric is insufficient |
|---|---|---|
| Engine simulation/update duration | Validated begin/end hook with monotonic wall and thread CPU; fixed rolling window, off by default | Pad hooks and presentation are different clocks/events |
| Command interpretation vs API vs waiting | Fixed 48-op count/wall/CPU aggregates; sampled boundary scopes for DRAW preparation/API; explicit flush-reason counters | Existing handler wall contains everything and opcode totals are only partly reported |
| Bind groups / cache reuse | Create/hit/invalidation counters and bytes compared, fixed-size | Global epoch/IDs do not quantify actual churn |
| GPU duration | Negotiate supported timestamp feature, fixed query pages, async resolve, period conversion, no per-frame CPU wait | CPU completion/map wall is not GPU execution |
| Asset queue delay | Publication/service-start/reply timestamps with validated ABI or host-side sampling | Current asset timer begins after acquisition |
| Input p95/p99 and update cadence | Optional latest-256 enqueue→hook samples plus consecutive hook intervals | Mean/max and coalesced age cannot reconstruct a distribution |
| Per-category upload bytes / copy amplification | Separate protocol, native-shadow, pack/zero-fill, QueueWriteBuffer/Texture and internal copy counters | Existing cumulative channels overlap/differ |
| Complete pipeline recurrence / descriptors | Bounded recent-key sequence or explicit key snapshots for selected worst events | Eight fingerprints and last 64 evicted keys cannot reconstruct a scene working set |

These hooks are proposals, **not implemented or measured during this audit**. Existing v35 diagnostics plus external tools are sufficient for the first trace; adding hot-path code before collecting it would alter the audit target. If added later, validate disabled/enabled behavior and measure overhead independently before producing a profiling build.

### Profiler correctness and overhead audit

Source/tests checked: `test_timing_window.c` (empty, wrap, nearest-rank quantiles, lifetime vs rolling, large values, small-sample withholding), `test_display_timeline.c` (zero/invalid/out-of-order, known synthetic cadence, user-marker exclusion/reset), shared-ring bounds/sanitizers, input fake-clock/concurrency, session fake-clock pause/stall/cancellation, resource/query lifetimes. The fresh exact-revision CI rerun re-executes these and the graphics/runtime suites. Arithmetic correctness is not an overhead measurement.

Hot-path instrumentation has fixed storage and bounded operation counts: **two monotonic clock reads per executor call**, fixed eight-command breadcrumbs and scalar counters; one thread-CPU read per successful PRESENT; 11 latest-256 timing windows plus cold/post-compute windows; eight worst pipeline/PRESENT/slow-command records. The current NativeMetrics object is **29,552 bytes** in retained host output. No per-command sort or log formatting occurs in the timing ring. Summary generation sorts at most 256 values/series, at most about once per five-second GPU report; it executes on the GPU-service thread. Display callbacks take a short monitor lock and append samples. Initial pixel diagnostics perform at most three actual 16×16 readbacks after compute, spaced by at least 30 PRESENTs in normal scene mode; that cost can contaminate the first sampled scene intervals but does not continue every frame.

**Wall-time overhead percentage remains UNKNOWN**: there is no identical-workload instrumentation-off v35 trace. Fixed bytes/operations bound memory and algorithmic work, not microseconds or impact during driver stalls. A controlled on/off display-monitor run plus external sampling is required before calling profiling overhead negligible. No extra clocks, logging or production hooks were added by this audit; its incremental runtime instrumentation overhead is therefore **zero added operations**.

### Audit verification and delivery

Production native sources, settings, original Wasm/game data and all released IPAs are unchanged by this audit. Local read-only binary inspection verifies v35's released hash/UUID. The original-Wasm allocator reference checks were rerun with the pinned approved engine and passed; they remain reference tests, not native gameplay measurements. Report calculations and replay extraction are checked by scripts. Fresh CI status and repeat measurements are recorded in the accompanying audit evidence. No new IPA version, quality change, cap change or stage advancement is claimed.

### Fresh audit validation results

- **CI PASS:** exact v35 source, run 39078684670 attempt 2, job 117536797785; evidence archive 1,944,915 bytes, SHA-256 `f7557edcf7d1ea62c38ff36a58f6430a0317b6aaa4153b61430c3c1bb6b37b5b` verified before extraction.
- **Shader/renderer PASS:** 4,808 modules/pipelines, 500 clip shaders, zero audit failures; 111 untranslated records remain. Original-shadow/SRV, depth proxy, compute/UAV, query, buffer/texture readback, binding/resource lifecycle and pixel regressions execute in the unchanged full suite.
- **Runtime PASS:** two native processes; worker/mailbox/wait, asset reader, native allocator and fragmented transition checks. The original allocator funeral fixture appears four times (two runtime probe invocations per process), not four real scene transitions.
- **Input/session/UI PASS:** 71 original-engine bridge checks; ASan/UBSan and TSan input/concurrency; 1,374 controllable-clock session assertions; 183 UIKit checks. These are host/simulator tests, not physical DualSense or gameplay validation.
- **Profiler arithmetic PASS:** timing-window and display-timeline tests, including synthetic cadence, wrap/quantiles and marker reset; 289 drain-policy clock cases. Original-Wasm reference allocator checks rerun locally with the pinned approved engine, PASS.
- **Preservation PASS:** all 226 local native source files unchanged; all 34 earlier IPAs and released v35 rehashed unchanged, original engine unchanged. Audit report/evidence/scripts only. No game/web files uploaded.
- **Hardware blocked/unavailable:** actual v35 iPhone gameplay frames/timings, GPU timestamps/counters, device active-CPU profile, thermals, post-funeral scene and memory residency. These remain UNKNOWN, not PASS.

The unchanged workflow also compiled and packaged an unsigned ARM64 app during the rerun. That newly generated CI artifact was not substituted for the previously audited/released v35 IPA; the preserved release above remains the test identity. No separate profiling-code variant or new release was created.

## M. Compact report for ChatGPT

**Target is GTA V iOS Native 0.35.0 / build 35**, source `8047937b21007cea1efad62df2beb7d1c007bb43`, released unsigned ARM64 IPA 102,339,097 bytes, SHA256 `8e4744e7f027109934a0477a0b32b377f8af6364907fa39373b92d5ea768110c`; Mach-O UUID `54C10F1A-5313-37AD-BC46-2EE7BDA32930`. Backend wgpu-native v29.0.1.1 / HAL / Naga 29.0.3 with existing iOS BC patch. AOT Wasm → C / SIMDe, iPhone -O1, host CLI -O0. Baseline unchanged: 640×360, 30 FPS cap, 256 pipeline handles (128 under memory pressure), one 64 KiB buffer / eight-file archive reader, existing controls/UI/optional neutral-shadow compatibility. No runtime performance changes were made in this audit.

**Measurement origin:** static source and released-binary inspection; real Metal execution of synthetic original-shader shared-ring fixtures on macOS Apple Paravirtual GPU; original-Wasm allocator references on host Node; fresh CI rerun of the exact v35 source. No v35 A18 Pro trace is supplied/available. **Verified v35 gameplay FPS, CPU-vs-GPU limitation, actual GPU time, thermals, input latency and post-funeral success are UNKNOWN.** User's full 19-minute Prologue at 7–9 FPS gameplay / 15–20 FPS cutscenes and memory failure was v34, not v35. Do not relabel it.

**Retained v35 host production replay:** each of two processes executes 96 draws, checks 512 pixels, 96 exact occlusion samples / 96 physical query segments, final 32 buffer bytes and clear/counter ordering. Production mode 2: 7 draw batches, 12 wrapper submits, 18 uniform packs / 4,608 bytes, 93 independent / 3 dependent pending-buffer updates, 1 explicit completion, 0 pressure waits; original protocol upload payload 53,044 bytes unchanged. Legacy policy in the same v35 harness: 98 batches / 103 submits, 192 packs / 49,152 bytes,7 completions / 6 pressure waits. These established v35 improvements cut fixture submits 88.350% and packed uniform bytes 90.625%; they do not establish a device FPS gain. Dependency-only old-drain mode: 9 batches / 14 submits / 22 packs / 5,632 bytes. Coalescing-alone handler walls vary both ways, so no isolated CPU speedup is established.

Mode 2 DRAW-handler wall process 1: 96 samples, mean 0.018 / p50 0.003 / p95 0.035 / p99 = worst 1.135 ms, total 1.741 ms. Process 2: mean 0.019 / p50 0.003 / p95 0.060 / p99 = worst 1.030 ms, total 1.855 ms. Whole fixture handler totals 108.280 / 97.490 ms; upload subset 2.601 / 3.063 ms. They contain blocking operations, no PRESENT/display, and cannot be divided into gameplay FPS. Native per-op exclusive costs and GPU duration are not retained. Do not assign unaccounted handler time to the GPU.

**Pipeline evidence:** 192 required-key / 576 draw fixture at 256 cap: 192 creates / 384 hits / 0 evictions; API total 20.952 / 21.612 ms, link 9.651 / 9.686 ms in two host processes. At 128 cap: 576 creates / 0 hits / 448 evictions; API 67.444 / 67.967 ms, link 30.554 / 31.420 ms. Production semantic-key fixture: 1 pipeline vs 32 reference, same pixels. All 4,808 translated modules/native pipelines audited (1,112 VS / 3,667 PS / 29 CS; 500 clip); 111 HS/DS/GS records untranslated. This is corpus coverage, not actual Prologue working set. Cache lookup still scans 256 semantic keys; on misses linking and pipeline API are synchronous. Driver bytes, worst v35 device keys/recurrence and mission phases are UNKNOWN. Metal pipeline cache in pinned HAL is a placeholder, not a persistent binary archive.

**Memory:** v34 failed 37,314,560 bytes = 35.586 MiB; rounds to 64 MiB in buddy allocator. Old 32 MiB segments minus an 8 KiB sentinel have a maximum 16 MiB initial block; fixed 32 slots can exhaust despite logical available bytes. V35 native request/slot-aware 32 / 64 / 128 / 256 MiB growth and bounded preflight fix pass original-root 64 MiB allocation, fragmented 64 cycles / 576 allocations and adaptive 23 / 32 slot late-request tests; legacy uses 32 / 32 and fails. Actual class 1 growth target 1 GiB is not a global memory budget; linear limit 4 GiB. NULL nonempty archive destinations rejected before I/O. Device retention/fragmentation/OS pressure and next-scene success UNKNOWN; larger segments can increase commitment. Raw executable 411,568,952 bytes, symbol + string tables 171,484,264 bytes; these are disk metadata, not measured resident RAM or FPS cost.

**Synchronization/upload findings:** uniform staging bound 16 batches / 4 MiB, 3 × 512 KiB GPU ring + 512 KiB CPU mirror; readback ≤ 1 MiB. Corrected 13-map / 96 draw fixture: 0 pressure waits / 1 completion versus reference 6 / 7, exact data. Independent updates avoid encoder submit but preserve pass/query boundaries; ready backlog coalesced ≤ 4 drains with 4 ms checks, not a bound on long API calls. Fences, readbacks, dependent writes still serialize. Present adds blit / acquire-drawable. CPU wall, service-thread CPU and hardware GPU time are different. Upload handlers include preflush/waits; wrapper submit counts exclude possible internal backend upload work. Full cause histogram/copy-byte breakdown missing. The historical ~25 GB uniform traffic / 15 M protocol uploads are different channels; no valid copy-amplification ratio.

**Fresh exact-source rerun during this audit: CI PASS, run 39078684670 attempt 2, two more native processes.** Production process 1: 7 batches / 12 submits / 18 packs / 4,608 uniform bytes; DRAW mean/p50/p95/p99/max 0.016/0.003/0.011/1.047/1.047 ms over 96 samples, DRAW total 1.503 ms; all-handler/upload totals 117.696/2.731 ms. Process 2: 8 batches / 13 submits / 20 packs / 5,120 bytes; DRAW 0.057/0.011/0.072/3.003/3.003 ms over 96 samples, total 5.506 ms; handler/upload 125.560/6.812 ms. Both preserve exact pixels/data/96 query samples, 93 independent/3 dependent updates, one completion, zero pressure waits and 53,044 protocol bytes. The bounded drain policy can produce count variation; no exact guard-reason attribution is retained. Production pipeline working set remains 192 creates/384 hits/0 evictions; fresh pipeline API totals 26.613/37.165 ms, links 11.538/15.021 ms. No source optimization changed between attempts. Host bridge throughput: 100,000 event/consume pairs, 30.383 ms under ASan/UBSan (0.304 microseconds/pair), 210.889 ms under TSan (2.109 microseconds/pair), not device input latency. Full 4,808 shader, runtime/memory/worker/input/session/UI suite PASS. These fresh numbers still do not measure iPhone FPS or actual GPU time.

**Assessment/confidence:** high confidence in source mechanisms and exact fixture counts/pixels; low confidence in any claimed dominant v35 device bottleneck. CPU translation/synchronization and cold pipeline work are first hypotheses, not proof that A18 Pro is CPU-bound. The type-2 protocol timestamp samples CPU time before flush and publishes after completion; it is not a GPU timestamp. V35 does not negotiate hardware timestamp queries. No evidence yet establishes 30 FPS capability or 60 FPS-equivalent headroom. Current active engine function ranking, GPU occupancy/bandwidth and phase-separated frame-time distributions UNKNOWN.

**Recommended first three subsequent actions/fixes, gated by measurement:** (1) collect a v35 actual-display + CPU/thread/Metal trace on a repeatable route, classify remaining dependent upload/flush causes; then change batching/versioning only if bytes/lifetime proof permits it; (2) capture actual expensive pipeline keys/pairs and evaluate bounded link reuse/mission-specific preparation, keeping 256 cap; (3) measure remaining uniform compare/copy/zero-fill and readback/fence critical paths before adding any cache or pipelining. Other evidence-supported candidates: pass/proxy traffic, per-byte atomic host copies, lookup/group invalidation, transition residency. All need exact data/pixel regressions; device benefit ranges UNKNOWN. Do not reduce quality, discard dependencies or treat host timing as iPhone FPS.

**Minimum missing data:** v35 export with actual marked display intervals (mean / p50 / p95 / p99 / max, count), pipeline worst-key records, protocol/uniform/submission/pressure counters, memory / thermal / input summaries; same-window Instruments active CPU self + inclusive stacks and waits; GPU capture / counters; funeral transition allocator/VM timeline. Existing report categories overlap; do not sum pipeline+upload+completion+handler walls. Engine pad hooks are not simulation frames, first compute is not a scene detector, successful PRESENT is not completed display, and rendered pixels alone do not prove mission progression.
