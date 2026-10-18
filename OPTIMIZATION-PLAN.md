# Native 0.36 measurement and targeted optimization plan

## 1. Baseline, scope and evidence

The evidence baseline is `PERFORMANCE-AUDIT.md` at documentation revision `cb2d8d5aa8ae480c87af8164479d30711dd1a0c1`. Its actual tested Native 0.35 production source is `8047937b21007cea1efad62df2beb7d1c007bb43`. These are different revisions of the same audit history, not two measured game builds.

Native 0.36 tested/released source: `f30f83723017f2452dbe5660243906dedb425e13`.

**No physical Native 0.35 or 0.36 performance capture has been supplied.** Native 0.34's reported 7–9 FPS gameplay, 15–20 FPS cutscenes and roughly 19-minute completed Prologue are historical physical-device evidence. They are not Native 0.35/0.36 measurements. Device CPU/GPU balance, sustained 30 FPS feasibility, thermal behavior and post-funeral loading remain UNKNOWN.

This increment implements full session text-file logging and optional bounded CPU instrumentation. It does not introduce another rendering optimization before collecting the missing performance evidence. The already verified submission, uniform reuse, semantic pipeline keys, allocator adaptation, controls and shadow compatibility changes are preserved. It does not lower 640×360, change the 30 FPS cap, change core-count/compiler/ABI/FP settings, enlarge the 256-entry pipeline policy or alter staging/memory budgets.

## 2. Ranked remaining bottlenecks and candidates

Ranking indicates the order to investigate, not a proven ordering of iPhone frame-time costs. No numerical device benefit range is defensible yet.

| Rank | Evidence and confidence | Exact source boundary | Candidate and correctness gate |
|---|---|---|---|
| 1 | Required mutation boundaries still exist. Exact v35 96-draw fixtures retain 3 dependent updates while 93 independent updates avoid the old unconditional flush. Mechanism confirmed; device critical-path share UNKNOWN. | `native-ios/renderer/native_gpu.m:execute_impl`, `native_upload.inc`, `native_draw.inc:draw_flush` | Use new per-opcode submission/preflush counts and CPU/wall times to find actual costly dependencies. Only consider bounded resource versioning when overwritten bytes, reads, completion and retirement can be reproduced. Tests must retain pixels, query counts and final buffer bytes. |
| 2 | Pipeline creation is synchronous on misses. Fresh v35 192-key/576-draw host runs at capacity256: 192 creates,384 hits,0 evictions; API totals26.613/37.165ms. Device expensive keys and miss cost UNKNOWN. | `renderer/draw_pipeline.inc:draw_pipeline` | Collect the existing worst-key records in a matched scene. Prepare only demonstrably encountered descriptors with correct generation/storage/shadow variants. Do not guess future keys, add unsupported Metal archives or enlarge retention blindly. |
| 3 | The same pipeline-miss path repeats pair reflection/link processing. Fresh v35 host link totals11.538/15.021ms for the working-set fixture. There is no independent evidence that this dominates phone gameplay. | `renderer/inter_stage.inc:draw_link_modules` | Consider a bounded shader-pair link cache if new profile proves repeated cost. Key must distinguish complete source/variant/interpolation/negotiated limit, not just a shader hash or recycled object pointer. Validate actual Four/Eight shadow variants, clip pairs, replacement, teardown and pixels before implementation. Not implemented here. |
| 4 | Uniform compare/packing remains real work after the already completed90.6% traffic reduction. Fresh production-policy fixture uses4,608–5,120 uniform bytes, compared with49,152 legacy bytes; exact draw-visible bytes are checked. Active compare/packing CPU share UNKNOWN. | `renderer/native_draw.inc:draw_group`, `native_host.c:host_load/host_store` | First determine whether DRAW active CPU remains high after separating linking/API/waits. Add a narrowly timed pack/compare scope if needed. No bulk replacement of shared atomic loads/stores without a valid ownership/publication proof. |
| 5 | Completion/map waits and drain boundaries remain in code. Fresh v35 production fixture: one explicit completion, no staging-pressure waits; older multi-million device totals belong to earlier revisions. GPU execution and feed gaps UNKNOWN. | `native_gpu.m:completed/submit/loop`, readback and query includes | Use new submit/completion/preflush/ring residual counters. Keep pass/query boundaries and ordered map retirement. Do not label a CPU wait as GPU utilization. A Metal trace is needed to identify actual GPU gaps. |
| 6 | Post-funeral37,314,560-byte allocation failure is reproduced in the original allocator. v35 native request-aware growth/slot correction passes controlled long-session fixtures. Actual phone transition is still unverified. | `native-ios/memory/`, `asset_io/native_assets.c`, runtime allocator snapshots | Collect allocation request/free-block/slot/headroom/resource-retention context across the first next-scene load. Do not evict assets or raise budgets without identifying ownership and the actual limiting allocator. Preserve rejection of nonempty NULL archive destinations. |
| 7 | Engine main/render dependencies, per-byte shared-memory copies and visibility/animation/physics/scripts are visible in source. Their active iPhone CPU costs are UNKNOWN; coarse engine wait labels do not establish exclusive CPU hotspots. | mapped `w2c_gta_f47953`, `f43492`, `f46429`, `f16020`, `f16953/f16959`, `f2040`, `f6211` | Obtain an external sampled CPU trace on a suitable developer-signed build. Distinguish active computation from semaphore/event/frame-limit sleeps before changing engine work or scheduling. |
| 8 | The GPU service has one outer autorelease pool covering its thread lifetime; Objective-C source/reflection/progress temporary objects can outlive a command drain. Retained byte cost is UNKNOWN; this is not a proven leak or primary FPS cause. | `renderer/native_gpu.m:loop`, `App.m:RuntimeProgress` | Use an allocation trace/long repeated original-shader fixture before considering narrower pools. Validate all Objective-C/C-string ownership, asynchronous GPU references and retained outputs; do not drain borrowed data early. Not changed here. |

A deeper backend rewrite, memoryless attachments, transient heap aliasing, quality cuts and unsafe compiler flags are not justified by these data. The pinned backend's placeholder pipeline-cache implementation does not establish usable persistent Metal binary archives.

## 3. Exact source changes implemented

### Complete session logs and Share-only export

- `native-ios/diagnostics/native_journal.h/.m`: one persistent session `.txt` file under Documents/NativeLogs, a fixed 2 MiB producer queue, one writer and64 KiB write/copy blocks. File offsets are64-bit. There is **no app-imposed file-size or record-count quota**. Available storage and I/O errors remain real limits; no literal infinity is promised.
- `native-ios/native_host.c`: capture original engine log messages and every stdout write byte before the existing engine/UI preview truncation. Preserve the original engine data and the bounded preview behavior. Host ABI byte-copy and archive-reader semantics are unchanged.
- `native-ios/App.m`: create a unique file at session start; retain session/header/progress/final diagnostics; finish after producers stop; report file write/backpressure/errors. Clear the previous session's export association before starting a new file, so a failed new log cannot silently export the previous run.
- `native-ios/ui/NativeDashboard.inc`: remove the Copy action, disable log text selection, use **Share Full Log (.txt)** with a file URL and explicitly exclude the system Copy-to-Pasteboard activity. The long file is never converted to a clipboard string. Share takes a fixed-prefix snapshot in the background; it does not stop gameplay to copy an ever-growing file indefinitely. Export cancellation deletes only the temporary snapshot, not the original log. Duplicate presentations and background/dismissed presenters are guarded.
- `native-ios/Info.plist`: version0.36.0/build36; expose the Documents folder in Files using public iOS file-sharing/document settings. Previous session files are retained for the user; there is no automatic log deletion.

The on-screen preview remains 32,768 UTF-16 code units, with a16,384-unit pending UI buffer (roughly64/32KiB of character storage before object overhead). These are UI memory bounds, **not full-file truncation limits**. Preview summaries can still mention omitted/repeated messages; raw messages captured before those summaries are already in the file. Categories distinguish original engine records, runtime summaries and preview/status copies. Internal sampled counters and source-side diagnostic histories remain bounded; this records emitted text, not a new unlimited trace of every allocation or asset operation. Do not count repeated summaries as independent engine activity.

Writer backpressure is counted rather than silently dropping records. Disk-full/write/fsync errors remain visible. An export after an I/O failure identifies itself as INCOMPLETE; a run without a full file exports an explicitly incomplete preview. A force-killed process can lose the pending queue/unfinished OS writes. This is not a promise that an arbitrary OS termination flushes everything. Normal Stop joins producers and drains/flushes the file. Completed files and snapshots use bounded copy memory; Share temporarily needs storage for another copy of the selected prefix.

### Optional CPU profiling

- `diagnostics/native_profile.h/.c`: fixed-size opcode aggregates and latest 256 wall/active-CPU samples per stage; option defaults OFF and is frozen per GPU instance. No per-command allocation. CPU-clock failures and residual underflows are reported explicitly.
- `renderer/native_gpu.m`: profile handler CPU versus wall by opcode, protocol upload bytes, nested queue submissions, pending mutation preflushes, actual submit API time, completion-wait CPU/wall, drain flushes and ring residual. Reporting uses the existing five-second cadence and final report. A profiling allocation failure is reported as UNAVAILABLE and does not terminate the game.
- `renderer/draw_pipeline.inc`: optional active CPU for linking and render-pipeline API calls, alongside existing wall timers. No pipeline key, capacity, shader, driver-object lifetime or submission-order change.
- `renderer/submission_fixture.inc`: enable profiling in the existing production-policy96-draw command replay so the instrumented path is checked against actual pixels/queries/final bytes. No invented successful game frames.
- `App.m` and dashboard: persisted **Advanced → Detailed CPU profiling · optional, next session** setting. Disabled while running; restore-defaults turns it off.
- `compile-native.mjs`, `ci-convert.sh`, `input/ci-test-ui.sh`: compile/link diagnostics and run sanitizer/UI regression tests. Nested diagnostics headers do not invalidate the128 engine-partition cache dependency set.

## 4. Measurement definitions and remaining unknowns

| New output | Meaning and limitations |
|---|---|
| Opcode calls / wall / active CPU | Inclusive time within one graphics command handler on the service pthread. Includes its nested driver/API/wait work; not engine simulation CPU or whole-process CPU. |
| Protocol upload bytes | Inline payload bytes of buffer/texture upload commands. Not uniform GPU bytes, driver residency, or all CPU copies. Existing uniform counters remain separate. |
| Nested submissions | Queue submissions occurring synchronously inside that opcode. Other drain/cleanup submissions can occur outside handlers; do not assume their sum is the complete lifetime total. |
| Pending mutation preflush count | Only counted when an encoder was actually pending at the original generic mutation barrier. Opcode supplies the reason. It does not count every internal specialized barrier or every render-pass close. |
| Submit API | CPU/wall around `wgpuQueueSubmit`, excluding encoder finish/release. Not command-buffer execution time. |
| Completion wait | CPU/wall of the existing completion callback/poll path. Includes callback registration, polling, sleeps and bookkeeping. Not a GPU timer. |
| Linking / pipeline API | Nested inside DRAW/miss processing. Do not sum these with handler totals. API return alone may not cover all driver work deferred until first use. |
| Ring residual | Drain wall/CPU minus measured handler wall/CPU. Includes header checks, publication/wakes, loop and measurement overhead. **Not pure decoder time.** Negative measurement residues are bounded to zero with an underflow counter. |
| Handler totals per protocol PRESENT | Sum since the prior protocol PRESENT, excluding producer-idle time and outside-handler drain work. Not a displayed frame or scene classifier. |
| Stage percentiles | Latest up to256 samples, nearest-rank p50/p95/p99, mean and max. Quantiles withheld below20 samples. Opcode totals/max are lifetime aggregates, not percentiles. |

All nested categories overlap. A whole-thread CPU clock excludes time when this pthread sleeps or is not scheduled, but includes CPU consumed inside its driver calls. Other driver/GPU/engine threads are outside it. CPU numbers are invalid if clock errors are reported. Ring subtraction is a useful upper accounting boundary, not a causal profiler.

Existing drawable `presentedTime` monitoring, user-marked visible-scene intervals, pipeline identities, staging/query counters, input event-to-hook summaries, allocator snapshots, resource estimates, process footprint, available-memory and thermal summaries remain available. Marker windows classify **user-observed** scenes; they do not automatically detect gameplay.

Still UNKNOWN without additional physical evidence: actual0.35/0.36 FPS, sustained CPU/GPU budget, owner/render/worker exclusive CPU, GPU execution/bandwidth/occupancy, input-to-display latency, exact uniform compare cost, thermal frequency changes, physical post-funeral load and shadow/audio correctness. The protocol's CPU-sampled timestamp query is not converted into a GPU timestamp. No fake GPU timer was added.

## 5. Before/after evidence and overhead

### Prior v35 optimization already completed — preserved, not claimed again

Fresh same-source v35 host tests:

| Controlled96-draw policy | Batches/submissions | Uniform packs/bytes | Handler/upload wall, ms |
|---|---|---|---|
| Legacy, process1 |98/103|192/49,152|422.975/50.945|
| v35 production, process1 |7/12|18/4,608|117.696/2.731|
| Legacy, process2 |98/103|192/49,152|469.861/74.892|
| v35 production, process2 |8/13|20/5,120|125.560/6.812|

Each checks512 pixels,96 query samples/segments and final bytes with the same53,044-byte protocol upload payload. Timing/batch variation under the existing bounded drain policy is real. This is Mac/Paravirtual Metal fixture evidence, not phone FPS or a v36 speedup.

### This increment

Before: recent engine22×768-byte buffer plus UI pending/preview trimming; exporting the preview could lose earlier detail and clipboard-oriented Copy existed. After: complete accepted session records go to disk first, fixed 2 MiB queue, Share exports a text file, preview still bounded. This is a correctness/diagnostic capacity improvement, not an FPS optimization.

Final exact-source CI evidence:

| Measurement | Result | Origin / limit |
|---|---|---|
| Full journal stress |80,000 records /4,435,612 accepted and written bytes;0 unrecorded bytes and0 producer waits|Four concurrent producers; verified full UTF-8 records, live/final/closed snapshots and disk-full behavior. ASan+UBSan and TSan. Not iPhone latency.|
| Producer/join host test |178.659ms ASan+UBSan;345.506ms TSan|Includes formatting, contention, writer scheduling and concurrent snapshot. Final flush is checked separately. This is not a matched no-logging overhead ratio.|
| Inactive capture path |80,000 calls:0.184ms /2.323ms|Separate simple no-op benchmark under the respective sanitizers; not the same work as the concurrent producer test.|
| Optional profiler overhead |500,000 operations: baseline0.178ms; enabled216.381ms; incremental0.432µs/operation|Matched volatile arithmetic plus the actual CPU-clock/aggregate path, host ASan+UBSan -O1. Reporting and actual game work are outside this microbenchmark. No phone overhead claim.|
| Optional profiler memory |36,144 bytes|Fixed opcode counters and rolling windows; no GPU resources added.|
| Production-policy replay, process1 |96 draws;8 batches/13 submissions;20 uniform packs/5,120 bytes;204.789ms handler /7.063ms upload|New optional CPU profiling ON.512 checked pixels,96 query samples/segments and final resource bytes/order PASS.|
| Production-policy replay, process2 |96 draws;7 batches/12 submissions;18 packs/4,608 bytes;138.850ms handler /4.872ms upload|Same assertions. Source policy preserved; wall-bound drain variation retained.|
| Pipeline working-set regression |192 creations /384 hits /0 evictions in both processes|192 required keys,576 draws,256-entry policy. API34.033/33.058ms and link14.117/13.349ms totals; no new cache optimization claimed.|

### What the new instrumentation actually resolved

The96-draw fixture deliberately performs **five synchronous validation readbacks**. Their inclusive wall/active-service-CPU totals are192.468/11.504917ms and130.174/11.499542ms. All863 command handlers total204.789/22.965968ms and138.850/20.264930ms. Most of this fixture's wall-time variation is inside READBACK while this service thread is not actively executing. That is not evidence of A18 GPU saturation: callback scheduling, off-thread driver/GPU work and sleeps are not separately timed, and these are test readbacks with no game scene or PRESENT.

The aggregate submit API costs are1.832/1.743669ms and0.961/0.833251ms wall/active; ring residual1.005/0.454864ms and0.672/0.310320ms. Submission/preflush attribution matches the protocol: six pending generic preflushes—three dependent buffer updates, one destruction, two readbacks. Both profiles have0 clock errors and0 residual underflows. Profiler counts match96 draws,53,044 upload bytes, one pipeline/link and every counted wrapper submission.

This explains why the larger handler wall in one run is not a demonstrated CPU optimization regression. It does **not** establish that the same readback share occurs during phone gameplay. Do not remove validation readbacks to improve this synthetic number. The additional phone trace is required before changing exposure/readback dependencies.

Very short scopes have wider CPU-clock brackets than the reused wall timer and include measurement bookkeeping; displayed active CPU can slightly exceed wall at this scale. Do not derive CPU utilization or negative wait duration from those tiny differences. Long scopes still distinguish service CPU work from off-CPU wall time. No new gameplay FPS gain is claimed.


## 6. Memory and streaming status

The native request-aware allocator and original-engine memory tests are unchanged. The exact37,314,560-byte request, growth-slot reserve, real free-byte reporting, fragmented long-session fixtures and NULL-destination read rejection remain regression requirements. The post-funeral physical transition remains pending.

Additional logging memory is2 MiB ring plus64 KiB writer buffer and small metadata; an export uses another64 KiB copy buffer. No whole-archive or whole-log RAM cache was introduced. Optional CPU profile storage is fixed and reported using `sizeof`, allocated only when enabled, freed after GPU shutdown. Driver/GPU memory policies are unchanged. Original logs consume disk until the user removes them in Files. Their disk size is not process RAM or committed engine memory.

## 7. Verification and build

**PASS**, CI run [39083899358](https://github.com/yosnuffyyy/gta5loader/actions/runs/39083899358), attempt1, exact source `f30f83723017f2452dbe5660243906dedb425e13`. All available checks completed; no final-run tests were skipped for unavailable hardware. Three superseded development runs were canceled after additional test/lifecycle review. They are not the delivered build. One pre-existing UIKit test `fabsf`/double-literal compiler warning remains; it did not fail the suite and is unrelated to the new code.

- ASan+UBSan and TSan journal tests; deterministic optional-profile tests and real replay accounting.
-197 UIKit checks,116,441 stored UI integration bytes after preview trimming, share-file controller clipboard exclusion, preference/defaults/lifecycle and all prior controls/fullscreen cases;71 actual original engine-input checks;1,374 controlled session assertions; worker/mailbox/input concurrency checks.
- Two native runtime processes; actual original-shader pixel, depth/clip/SRV5633/shadow Four+Eight, UAV/compute, query, readback, reuse, resource lifetime, buffer mutation/drain ordering, uniform retirement and pipeline policy fixtures.
-4,808 native Metal shader modules/pipelines, including500 clip shaders;111 untranslated records remain explicitly unavailable.
- Original memory/fragmentation/37,314,560-byte request/slot-pressure and NULL archive destination tests;64KiB/eight-descriptor reader policy unchanged.
-235 remote source/workflow/manifest Git blobs match the local package inputs.215 pre-existing native files are byte-identical;11 changed and7 new native files. Main remains `de7664a8e6448096716c5a341250d32d4f594fd1`. All35 prior IPA hashes were rechecked and preserved.
- Verified unsigned ARM64 iPhone package, iOS17 minimum, no signing material, no writable/executable segment and no WebKit/JavaScriptCore dependency. No game/web archives or full CI shader packs are in the app. Test-only UI/log-failure/profiler benchmark code is absent.

IPA: **GTA-Native-Probe-0.36-unsigned.ipa**;102,336,482 bytes; SHA256 `18a3d8c257a003be03a72fa4c511cc86e952fd7b28b14dd0fdf7c1631469c2f2`.

Published experimental release: [Native0.36](https://github.com/yosnuffyyy/gta5loader/releases/tag/native-0.36). Tag points to the exact tested source. Exactly one manually attached asset, the IPA; GitHub's two automatic source-archive links are not application assets or game-folder uploads. Server asset size and SHA256 match the audited local IPA. Previous0.35 release/IPA remains intact.

Evidence is retained in `evidence/0.36`, especially `validation-summary.json`, `cpu-profile-replays.json`, `submission-benchmarks.json`, raw new journal/profile tests, `native-engine-test.txt` and the shader summary. These are host/simulator results; they do not establish new physical gameplay, post-funeral loading, correct shadows, audio output, sustainable FPS or GPU execution timing.

## 8. Next iPhone collection procedure

1. Install the final validated0.36 IPA over the existing app. Keep the same game folder,640×360,30 FPS cap, controller/touch preferences and shadow option as the comparison run. Preserve0.35 for rollback.
2. Start with **Detailed CPU profiling OFF** and display timing ON. Run a repeatable section; mark Visible Scene only once actual controllable3D is visible. Record route/actions, loading/cutscene/transition boundaries, charging state and starting thermal condition. Do not compare a cold loading interval with warm gameplay.
3. Stop normally, open Diagnostics and choose **Share Full Log (.txt) → Save to Files**. The main text area is only a preview. Share while running exports the prefix accepted when Share begins; later messages require another Share. The original is also under Files → On My iPhone → GTA Native Probe → NativeLogs, subject to the installed display name and Files availability.
4. Repeat the same route with **Advanced → Detailed CPU profiling ON**, enabled before Start. Export that run separately. Compare matched actual display windows and logging backpressure as well as CPU stage times. A measurable slowdown means the detailed mode should remain diagnostic-only; its clock cost is not assumed negligible on A18 Pro.
5. For the memory issue, continue through the funeral and first next-Story-Mode load if possible; export even on a failure. Required evidence includes the requested allocation, heap/segment/free-block snapshots, OS headroom, resource estimates and final streaming diagnostics. A new normal load must be physically observed before claiming a fix.
6. If the app is force-terminated, reopen it and Share the restored saved file. The last pending writer/OS bytes may be missing. Keep the system crash/jetsam report separately if available. If the app reports log storage failure, free space and start a new session; lost records cannot be reconstructed.

For true GPU busy/execution and engine active stack costs, use an Apple development Mac and a compatible developer-signed profiling build with the required attach permissions, then Time Profiler and Metal System Trace/GPU tools. This Windows workspace/unsigned IPA does not provide that access by itself. Do not change signing identities or entitlements silently. The existing mapped native symbols are useful for matching visibility/skeleton/physics/scripts/streaming stacks. A capture must cover a marked gameplay window and distinguish waits from active CPU, not only a loading screen.

## 9. Compact report for ChatGPT

**Build identity.** Baseline audit docs: cb2d8d5aa8ae480c87af8164479d30711dd1a0c1. Exact tested v35 source: 8047937b21007cea1efad62df2beb7d1c007bb43. Released v36 source: f30f83723017f2452dbe5660243906dedb425e13. CI run 39083899358, attempt 1: PASS. Unsigned ARM64 IPA: 102,336,482 bytes; SHA256 18a3d8c257a003be03a72fa4c511cc86e952fd7b28b14dd0fdf7c1631469c2f2. GitHub release native-0.36 has one IPA asset and the exact tested source tag. All 35 prior IPAs and main are preserved.

**Physical evidence.** v34 completed roughly 19 minutes of Prologue at reported 7–9 gameplay FPS and 15–20 cutscene FPS. The next scene failed a 37,314,560-byte allocation and then NULL archive destinations. No physical v35/v36 FPS, actual GPU timing or new post-funeral result is supplied. Primary CPU-versus-GPU FPS limitation: UNKNOWN. Stable 30 FPS and 60 FPS-equivalent headroom are unproven.

**Preserved v35 optimizations, host evidence.** Controlled 96-draw Mac/Paravirtual Metal fixtures check 512 pixels, 96 query samples/segments and final resource bytes. Legacy: 103 submissions and 49,152 uniform bytes. Production: 12–13 submissions and 4,608–5,120 uniform bytes, 93 independent/3 dependent pending updates, one completion and zero staging-pressure waits. At cache capacity 256, a 192-key/576-draw fixture gives 192 creations, 384 hits and zero evictions. Two fresh exact-v35 runs report pipeline API totals 26.613/37.165 ms and link totals 11.538/15.021 ms. No phone FPS projection follows.

**New v36 measurement findings.** The same production policy with optional CPU profiling ON retains 12–13 submissions and 4,608–5,120 uniform bytes. Its 863 command handlers consume 204.789/138.850 ms wall versus 22.966/20.265 ms service-thread CPU. Five deliberately synchronous verification readbacks account for 192.468/130.174 ms wall but only 11.505/11.500 ms service CPU. This explains host wall-time variation; it does not identify GPU saturation or gameplay readback frequency. The fixture has no PRESENT or game scene. Submit API totals are 1.832/0.961 ms wall; ring residual totals 1.005/0.672 ms wall. Six pending generic preflushes are attributed to three dependent uploads, one destruction and two readbacks. No clock errors or residual underflows; exact profiling, pixels, query counts and ordering PASS. New cache regression still gives 192/384/0; API totals 34.033/33.058 ms, link 14.117/13.349 ms. Nested timings overlap.

**What changed.** Gameplay text is captured before preview trimming into session files with no application size quota, a fixed 2 MiB writer queue and 64 KiB copies. Share exports a file URL; Copy/clipboard activity is removed. Disk errors/incomplete exports are explicit. An 80,000-record, 4,435,612-byte test loses zero bytes under ASan+UBSan and TSan. UI integration retains 116,441 bytes despite preview rollover; 197 UIKit checks PASS. Optional CPU profiling is OFF by default and uses 36,144 bytes. A 500,000-operation ASan host microbenchmark gives 0.178 ms baseline versus 216.381 ms enabled, approximately 0.432 microseconds incremental per operation. This is not measured iPhone overhead. Capture occurs on emitted text; internal sampled histories remain bounded.

**Remaining limits.** Owner/render/worker exclusive CPU, simulation CPU, actual GPU duration, uniform-compare cost, driver residency, thermals, input-to-display latency and new phone frame times remain UNKNOWN. The new profile separates inclusive service CPU/wall by opcode and selected API stages, not every exclusive graphics operation. CPU-side protocol timestamps are not GPU timestamps. Engine allocator growth and NULL-destination protection pass existing exact-request/fragmentation regressions; physical transition verification remains pending. A thread-lifetime autorelease pool may retain graphics temporaries, but retained bytes/leak impact are unmeasured. Audio output is unfinished; 111 shader records remain untranslated.

**First three next steps.** (1) Collect matched profiling-OFF/ON phone routes, mark actual visible scenes, export both full text logs and capture the post-funeral transition. (2) Reproduce the dominant actual opcode/flush/pipeline cost before changing dependencies; preserve pixels, bytes, queries and completion ordering. (3) Consider bounded generation-safe pair-link reuse or uniform packing only if measured active CPU cost exceeds noise. Do not optimize away verification readbacks, alter shared atomic copies without ownership proof, change graphics quality or enlarge caches blindly.
