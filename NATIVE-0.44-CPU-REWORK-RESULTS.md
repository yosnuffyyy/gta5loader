# Native 0.44 — experimental CPU execution core

Status: implementation candidate; integrated macOS/Metal, sanitizer, compiler experiment and unsigned ARM64 packaging validation PENDING. No iPhone 0.44 performance claim.

Baseline executable Native0.43: `9ea0c38bcfe9e673fa7ac7f5ae2a0ef811ce1f24`; preserved documentation checkout `1fadb5a`. The complete physical0.43 log is `NotgpuGTA-Native-Log-08E67252-76A2-4B1C-B8A1-B6AB0C7C0E9E.txt` (5,929,818 bytes). Working source remains isolated from protected PC directories and deployed main.

## Implementation and acceptance

CPU Execution Core V2 is a separate, OFF-by-default startup selection, independent of renderer Existing/V2. It retains the original protocol decoder, immutable acquired ring payload, native renderer, ordering and hazards. A bounded sparse exact-ID index accelerates object lookup; absent pages, high IDs and allocation/budget exhaustion use the original dictionary. The index owns no GPU objects. Removal clears the exact slot before destruction, and replacement receives the existing monotonic resource generation. Maximum index allocation is 1,081,352 bytes on 64-bit hosts, including directory/count/alignment; no GPU resources or frames in flight are added.

An exact 336-byte pipeline-input snapshot avoids rebuilding/normalizing the 1,468-byte key on unchanged validated state. It includes shader/layout/state identities, strides, target descriptors, object lifecycle counters, storage formats and runtime shadow specialization masks. Any creation/destruction or pipeline eviction forces the original slow path. Dynamic shadow/storage selection, buffer ranges, hazard marking, uniform bytes, proxies, draw commands, query readiness and backend validation still execute. The existing 256 pipeline bound and pass-local API reuse remain.

The experimental mailbox consumer tests the pending flag before claiming work, avoiding an exclusive atomic RMW on an empty mailbox. Release/acquire notification semantics and the original waiting/wakeup protocol remain; the original core uses its previous exchange behavior. Tests compare both paths under publication/consumption races.

## Physical0.43 evidence and limitations

Late displayed interval mean/p50/p95/p99/max:79.825/83.341/100.009/116.677/266.691ms,256 intervals, no marked gameplay phase. Handler active mean48.130549ms/PRESENT. Cumulative inclusive DRAW CPU9,307.454ms for841,928 calls; PRESENT3,083.703ms; readback2,668.710ms; uploads12/47 total3,603.680ms; END_QUERY1,686.474ms. Categories overlap submission/flush stages and cannot be added as exclusive engine cost. Queue submits19,667, readbacks1,554, pipelines262/6evictions. True GPU duration and full engine hot stacks remain UNKNOWN.

The phone log reports PCM starts/callbacks0, not working audible output. It also retains original timestep loss. The pinned timer regression reproduces10 real seconds ->3.333/6.667/10.0 engine delta seconds at5/10/15Hz, including pause/resume. No unsafe clock multiplication, clamp removal, extra physics updates or simulation fix is claimed. Owner-thread CPU is now sampled only once per existing five-second pad-boundary report; it is inclusive owner CPU, not exclusive physics/AI/animation. A safe catch-up policy requires independently established engine subsystem scheduling.

## Compiler experiment

An isolated host test recompiles the original GetLightShadowIndex-containing AOT partition at O1 and O2, retaining ABI, bounds, atomics and floating-point options. Sixteen alternating runs check400,000 exact producer results each. Compilation time, source digest, object/binary size and CPU/wall time are recorded. Other partitions remain fixed at host O0. This does not identify a whole-engine hotspot or establish ARM64 benefit; shipping AOT stays O1 until justified. Renderer preparation has an additional matched O1 host benchmark; existing O0 regression evidence is retained separately.

## Preservation inventory and remaining work

Story/Sandbox launch publication and save roots, alpha-to-coverage,709/5633, shadows, texture copies, queries, compute/UAV, all4,808 shader pipelines, native PCM, controls, fullscreen, presets, branding,30FPS cap, archive64KiB/eight descriptors, journals and recovery remain. No resolution, memory budget, worker count, frame cap, quality or simulation semantics changed. No leak or guaranteed30FPS is asserted.

Validation results, before/after statistics, exact source and artifact identification will be finalized after the integrated workflow. Physical comparison at a marked identical location/route, core OFF/ON, renderer held fixed, cold/warm passes and15–20minute driving remain required. iPhoneA15/A18 results are unknown.
