# Project Muguet Rebirth — compatibility ledger

Date:2026-10-21. **A preserved legacy test is not a passed migration test.** The new runtime is not production-enabled. The existing native sources/reports remain the behavioral ledger and rollback implementation; no historical fix is deleted.

## Executed checkpoint gates

Initial integrated CI [39644345448](https://github.com/yosnuffyyy/gta5loader/actions/runs/39644345448), source 23ddd0c98d66717a05d7a7e078a52a622bb54131: successful macOS ARM64 release tests, exact original-engine execution, independent Node math comparison, iOS AOT/link/dependency/segment audit, original-mode169 checks, original timer reproduction, ASan/UBSan launch options and100,000 completion-serial sequences. Initial Rust suite: 4 unit+2 Unix integration tests. Follow-up source 75fd4258d8c7a4be2ed0f675594c92d2dac07d87 adds stat ABI canaries, preserved manifest bytes, numeric manifest compatibility, cross-architecture host compiler and Wasm wait/notify; Its component CI 39645670173 passed all 5 unit+2 Unix integration tests, the full-module probes including wait/notify, and the iOS link audit. A subsequent gate tests actual compiler-free linked-RX execution on macOS; its result is recorded below.

| Gate | Exact coverage | Limit |
|---|---|---|
| Engine identity/exports | Full SHA,86imports, typed exports,11 upstream patch target signatures, export-only transformation | No upstream hook applied |
| Wasmtime execution | Original strings/trig/shadow function,400k checked shadow results | No complete constructor/world run |
| Original mode decision | Story/Sandbox/Sandbox/Story, original main publication and script flags | Not player spawn, mission prevention or actual save persistence |
| Shared memory | Stable growth, over-cap rejection, checked pointers,4workers/80k atomic increments, wait timeout/OOB trap; follow-up notify wake | Standard Wasm fixture, not original pthread/TLS/mailbox lifecycle |
| Artifact integrity | Wrong hash/length/engine/config rejected before unsafe deserialization | Build digest must be trusted; arbitrary user executable code forbidden |
| iOS AOT | ARM64 target, RX embedded bytes/alignment, no compiler backend, noW+X segments | Link audit; no signed on-device execution |
| Asset adapter | Original IDs/bytes, case ambiguity,12archives,8FD cap,64KiBchunks, EOF, cancellation/sink failure, symlinks, truncation, replacement | Synthetic files; not live5814-file phone installation or HTTPFS ring |
| Stat ABI |104-byte fields,64-bit sizes, signed timestamps/nanos, padding, trailing canaries, rejected short destination | Serializer; syscall dispatch not wired |
| Legacy options |24 mode/resolution/save-isolation checks +4invalid inputs, ASan/UBSan | Existing C component, not new Rust gameplay |
| Legacy completion |100,000 lifecycle/failure/reordering sequences,64-bit replies,128entry saturation, ASan/UBSan | No new GPU query implementation |

Rust tests/host engine execution are not claimed to run under ASan/TSan. Driver, iOS process and full game sanitizer coverage is absent. The whole legacy Metal renderer/shader suite was not rerun for this isolated checkpoint; no renderer file changed. Full rerun and new-backend equivalence become mandatory at graphics integration.

Final integrated CI [39646902321](https://github.com/yosnuffyyy/gta5loader/actions/runs/39646902321) **PASS**, source 8a1d6819e5e8b51802bb26251b9e24942d98a3a0. All 7 Rust tests and all above applicable component gates passed again. Original sin/cos executed from the compiler-free linked RX Mach-O on macOS, with bit-exact expected outputs. ARM64 iOS structural/dependency audit passed. Artifact 12367029256 ZIP SHA256 `49ba369de7feeadd783cdd525fd6d9cec763448f4b3199f0f36a7068015428a8` was downloaded and verified. `evidence/muguet-stage-a/validation.json` binds source, CI, individual file hashes and explicit limitations. No CI failure required retry in these three runs; early local compiler/toolchain errors were corrected before publication.

## Historical regressions that block promotion unless preserved

| Behavior/history | Existing reference/test | Migration requirement/status |
|---|---|---|
| Clip-distance extension and varying-location overflow | `clip_fixture.inc`, `test-inter-stage-source.mjs`, `test_inter_stage_layout.c` | Preserve clip/discard/interpolation and pair-linked outputs. Not ported. |
| Depth-as-color proxy, mip/array sentinel ranges | `proxy_range_fixture.inc`, `test_depth_proxy_layout.c`, `test_srv_contract.c` | Exact requested subresources; no clamping. Not ported. |
| SRV709 active cube/2D shadow placeholder | shadow semantic/transition/reference fixtures; PS996bb42f448956f2 and6e9619cde3dac275 | Certified inactive specialization plus optional, narrow neutral-shadow semantics, valid maps untouched. Not ported. |
| SRV5633 dimensional/sample-type mismatch | `srv_fixture.inc`, `test_srv_contract.c` | Preserve 2D-array/view/sample compatibility and dynamic generation checks. Not ported. |
| Vehicle-impact single-sample alpha-to-coverage | `coverage_fixture.inc`, `coverage_ring_fixture.inc`; VS90bb870460e2835c/PS0030c87264713865 | Preserve verified single-sample discard/coverage semantics and MSAA behavior, depth/stencil/occlusion pixels. Upstream disabling is insufficient. |
| UAV initial counts, copy count, clears, compute | `test_uav_layout.c`, compute fixtures | Exact counter state/ordering; no fake success. Not ported. |
| Occlusion sample results and reuse | query/query-stop/completion fixtures | Preserve counts, validity, generation, empty/stale/error handling. Reject upstream normalization to65,536. |
| Event query availability without per-query blocking | event/completion fixtures, `test_completion_serial.c` | Preserve submission serials, bounded waits and callback lifetime. Not ported. |
| NO_OVERWRITE/byte-edge uploads and immutable draw bytes | ordered/transfer/reuse fixtures, `test_buffer_upload.c` | Correct overlapping mutations/versioning and in-flight retirement. Not ported. |
| Buffer/texture/cross-format copies and readbacks | copy-cast/transfer fixtures, `test_texture_copy.c`, `test_readback_layout.c` | No ignored copies/stale data; exact format/range checks. Not ported. |
| BC/SNORM/norm16/border/stepped-instance formats | BC/SNORM/norm16/border fixtures, `test_vertex_step.c` | Per-device capability negotiation, correct fallback pixels. Not ported. |
| Pipeline key equality/cache churn/object-ID reuse | `test_pipeline_key.c`, policy/core-index/reuse fixtures | Bounded exact semantic keys, lifecycle generations; no hash-only hit. Not ported. |
| Sandbox always entering Story | `launch/test_engine_mode.cjs`, publication/options tests | Offset+440 and Story-only-noautoload preserved in original runtime fixture; full world spawn remains untested. |
|180-second gameplay termination, stop/relaunch/mailboxes | session/mailbox/wait tests | Session cancellation and lifecycle must replace global once-only state. Not ported. |
| NULL streaming destination/post-funeral allocation failure | allocator fixture, memory policy, asset tests | Check ownership/allocation before publishing reads, preserve allocator safeguards. Adapter cancellation/size validation only. |
| Story/Sandbox saves and folder permissions | host/asset-index/options tests | Existing separate roots unchanged. Transactions/bookmark lifecycle not integrated. |
| Input source switching and digital edges | input queue/engine input/UI tests | Preserve real engine publication, simultaneous actions, release on lifecycle. Not ported. |
| PCM output/Opus/routing/interruption | audio PCM/publish/output fixtures | Real assets/PCM, bounded realtime callback, verified decoder ABI. Not ported; sound unknown. |
| Slow game-time below 15updates/s | original timer test |66.7ms clamp reproduced, not fixed. No unvalidated global speed multiplier. |
| Late-session memory/cache pressure and possible jetsam | memory policy/resource census/recovery tests | Do not treat CPU report as jetsam proof. New runtime resident memory unknown. |
| UI scroll/fullscreen/log sharing/branding | input/UI/diagnostic tests | Legacy app unchanged; integration must retain preferences/icon/bundle ID. |
|111 untranslated HS/DS/GS shader records | shader audit reports | Still unresolved when actually required. Do not silently skip. |

## Upstream differences found, not blindly imported

* `fs.rs::write_stat`:112-byte record versus this engine's104-byte record. New serializer corrects the size/fields before integration; real syscalls pending.
* `patch.rs` Opus six emitted arguments versus five parameters: local5 is an i32, so verify its intended FEC/state meaning; not a proven validation failure or explanation for silence.
* Scenario, movie, missing-texture, allocator and resource-map patches change behavior. Target signatures are recorded, but semantic tests are missing; none applied.
* `gpu.rs` uses65,536 for visible occlusion, disables A2C for one sample, contains dummy-resource policies and unsupported-copy/mipmap returns. These do not satisfy this project's exact contracts.
* Unknown imports/default return values, fatal GPU-init drain behavior, unchecked ring reads and globals/detached worker lifetimes require replacement before promotion.
* ZIP importer can replace directories and change DLC inputs. Excluded permanently from existing-folder adaptation.

## Remaining gates and delivery decision

No migration IPA is produced: the full host services, original engine worker/TLS/exception model, Rust renderer, saves, input and audio are not integrated. Packaging this probe as a playable build would misrepresent its behavior. Current app version/build remains0.44.0/44; no version is consumed by a structural executable.

Next engineering gate: implement exact session-owned host imports and lifecycle, integrate manifest/HTTPFS transport with real bytes, run constructors to a real renderer handshake, and prove original worker TLS/mailbox/exception behavior with clean stop/relaunch. Then port graphics and replay the table above before device gameplay. No physical test is requested for the current non-playable checkpoint.

Before eventual promotion: signed AOT load on iPhone, same existing web folder, separate Story/Sandbox saves, walking/driving/rear-end collision, guard/vault/funeral/Los Santos transitions, real shadows, audio, controller/touch/fullscreen, pause/background/relaunch and long-session memory/thermal comparison. These remain **NOT TESTED** in the migration.
