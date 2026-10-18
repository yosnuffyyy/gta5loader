# Native 0.42 — Sandbox launch repair (validated test build)

Baseline: Native 0.41 executable `da272c5bba805337a42bff4b965ea820b1d2b4d9`; native branch documentation `cd406e6baff0a357d4288f31d9d696f9ab8ffbe3`.

## Confirmed cause and mechanism

Original `main` publishes WasmInputBlock at 6583436. Its requested mode is at **+440** (6583876), not +280. Offset +280 is mouse focus. Native 0.41 put the selection into focus, then unconditionally stored **1** at +440. Both choices therefore requested Story. The old test wrote cached StartChoice at 6583880 directly, bypassing the broken bridge.

The new production helper writes focus=1, preserves all other input bytes, publishes geometry and selected mode at +440 with release ordering. UI/bridge/publication/engine-commit stages are logged once. A contradictory engine commit or actual Prologue1 execution in Sandbox fails explicitly rather than silently becoming Story.

Mode 2 is an existing original-engine offline Sandbox path. `StartChoice`, session initialization and original script native `GET_COMMANDLINE_PARAM_EXISTS` distinguish `DoReleaseStartup` from `WasmSandbox`. Sandbox now reaches that original path, with its original player/world/streaming/camera setup and built-in loadout, rather than a fabricated save or teleport patch. Its initial city coordinates use the reference launcher's existing `-playercoords=94.95,-742.96,44.75` option. Player/world correctness still requires device verification.

`-noautoload` was not the cause of the mode overwrite: the original script-native fixture selects Sandbox even with that flag. Production now applies it only to Story, preserving Story's prior argv behavior. Sandbox follows the reference path without the Story new-game override. Story retains NativeRuntimeProbe saves; Sandbox retains the separate NativeSandboxProbe root. No save files or checkpoint state are modified or fabricated.

Sandbox has no required animated-cutscene camera event. Its original positive spawn-loadout event, which requires a live player ped/inventory and granted weapons, can instead establish a **liveness marker**. A later input hook and PRESENT are still required to retire the startup deadline; 60-second stall detection remains. This marker does not establish collision, camera, visual output or complete initialization.

## Verification

The original-Wasm integration checks enter original main to the real host publication boundary, then execute original startup choice and script-native decisions, without modifying engine instructions. Alternating Story/Sandbox selections across all three presets reproduce the old forced-Story bug and verify the corrected requested field. A separate Sandbox-with-noautoload control isolates that flag. CI passed using the actual production C publication bytes under ASan/UBSan: 12 alternating mode/preset blocks, 5,136 untouched-byte checks and 169 counted original-main/startup/script/argument checks, plus exact Sandbox spawn-argument checks. The previous forced-Story behavior is reproduced as a negative control. A simulated 20-minute Sandbox liveness policy and a subsequent real-stall case also pass.

The integrated CI and package audit passed: 4,808 Metal shader modules/pipelines (500 clip shaders), both SRV 709 shaders and SRV 5633, pixel/byte/query/resource-lifetime regressions, two full native runtime processes, 100,000 completion lifecycle sequences, 71 production input checks, 205 UIKit checks, 1,374 existing watchdog assertions and applicable component ASan/UBSan/TSan. Full driver/AOT-engine sanitizer coverage is not claimed. Persistence and renderer selection UI checks pass; actual Sandbox gameplay with either backend is not exercised by the host fixture. No physical Sandbox gameplay is claimed. The host fixture stops before archives/world simulation; it cannot verify walking, camera, vehicles, collisions, audio hardware or mandatory mission absence during a full session.

## Preservation and next device test

Existing/V2 renderer selection, 256-entry pipeline cache, presets, 30 FPS cap, controls, audio, branding, graphics/SRV compatibility and original assets remain unchanged. No unrelated renderer work. Previous IPAs and protected main remain intact.

After validation: select Sandbox and confirm publication +440=2, engine commit sandbox, original player event, Los Santos spawn with no bank/Prologue, movement/camera/vehicles/collisions/streaming, pause and audio. Test both renderers; stop/relaunch and switch Story→Sandbox→Story. Confirm Story retains its previous startup behavior and saves. Share full logs on failure; do not infer playable Sandbox from mode numbers alone.

## Changed files

- `launch/options.h`: checked shared-memory publication, Story/Sandbox argument policy and diagnostic classifiers.
- `App.m`, `ui/NativeDashboard.inc`, `renderer/native_scene.c`: version and distinct UI/bridge launch-stage diagnostics; existing selector/preferences retained.
- `threading/native_threads.c`, `native_session.h`, `native_session_bridge.inc`: actual mode publication, explicit mismatch errors and Sandbox player-event liveness.
- `launch/test_engine_mode.cjs`, new `test_publication.c`, `ci-convert.sh`: production-byte/original-main/script-native integration and sanitizer fixtures. `Info.plist`: version 0.42.0/build42, unchanged install identity.

The initial CI run 39170883166 failed on a volatile-atomic pointer qualifier mismatch before packaging. The helper was corrected to retain qualifiers, use the host's atomic byte-store conventions and release-publish the final mode word. The successful full run below supersedes that failed candidate; no failed IPA was released.

## Build and delivery

- **0.42.0 / build 42**, executable source `ab51b47752066092280136f8327a8543b8a612ad`.
- [Successful CI 39171815837](https://github.com/yosnuffyyy/gta5loader/actions/runs/39171815837). All 280 source-manifest entries audited.
- `GTA-Native-Probe-0.42-unsigned.ipa`, 104,977,964 bytes. SHA256 `3db37e432c515f17892272633843f0cee70adf5d0d1daa7fbb85348086f8f13d`.
- Mach-O UUID `4b57b6d55e4f3762a366429679890693`; matching compiler/symbol evidence retained locally. ARM64/iPhone/iOS 17 minimum/unsigned; existing bundle ID/name/icon. No game/web payload or CI-only fixtures.
- [Release and IPA](https://github.com/yosnuffyyy/gta5loader/releases/tag/native-0.42). Only the IPA is uploaded as a release asset; publication verified separately. All 41 previous IPA hashes preserved.

No FPS, audio, thermal or memory-footprint improvement is claimed. This is a targeted startup correctness repair. The host fixture proves publication and original script-native decisions, **not complete Sandbox gameplay**. Physical checks remain: no mandatory Story missions, player/camera/vehicles/collision/streaming, pause/audio/input, repeated launches, both renderers and unchanged Story saves/progression.
