# Native 0.42 — Sandbox launch repair (validation pending)

Baseline: Native 0.41 executable `da272c5bba805337a42bff4b965ea820b1d2b4d9`; native branch documentation `cd406e6baff0a357d4288f31d9d696f9ab8ffbe3`.

## Confirmed cause and mechanism

Original `main` publishes WasmInputBlock at 6583436. Its requested mode is at **+440** (6583876), not +280. Offset +280 is mouse focus. Native 0.41 put the selection into focus, then unconditionally stored **1** at +440. Both choices therefore requested Story. The old test wrote cached StartChoice at 6583880 directly, bypassing the broken bridge.

The new production helper writes focus=1, preserves all other input bytes, publishes geometry and selected mode at +440 with release ordering. UI/bridge/publication/engine-commit stages are logged once. A contradictory engine commit or actual Prologue1 execution in Sandbox fails explicitly rather than silently becoming Story.

Mode 2 is an existing original-engine offline Sandbox path. `StartChoice`, session initialization and original script native `GET_COMMANDLINE_PARAM_EXISTS` distinguish `DoReleaseStartup` from `WasmSandbox`. Sandbox now reaches that original path, with its original player/world/streaming/camera setup and built-in loadout, rather than a fabricated save or teleport patch. Its initial city coordinates use the reference launcher's existing `-playercoords=94.95,-742.96,44.75` option. Player/world correctness still requires device verification.

`-noautoload` was not the cause of the mode overwrite: the original script-native fixture selects Sandbox even with that flag. Production now applies it only to Story, preserving Story's prior argv behavior. Sandbox follows the reference path without the Story new-game override. Story retains NativeRuntimeProbe saves; Sandbox retains the separate NativeSandboxProbe root. No save files or checkpoint state are modified or fabricated.

Sandbox has no required animated-cutscene camera event. Its original positive spawn-loadout event, which requires a live player ped/inventory and granted weapons, can instead establish a **liveness marker**. A later input hook and PRESENT are still required to retire the startup deadline; 60-second stall detection remains. This marker does not establish collision, camera, visual output or complete initialization.

## Verification

Local original-Wasm checks now enter original main to the real host publication boundary, then execute original startup choice and script-native decisions, without modifying engine instructions. Alternating Story/Sandbox selections across all three presets reproduce the old forced-Story bug and verify the corrected requested field. A separate Sandbox-with-noautoload control isolates that flag. Local publication bytes are synthetic; CI must rerun with bytes produced by the actual C publication helper under ASan/UBSan.

Full integration, component sanitizer, input/UI, shader and renderer CI checks and unsigned ARM64 package audit are pending. No physical Sandbox gameplay is claimed. The host fixture stops before archives/world simulation; it cannot verify walking, camera, vehicles, collisions, audio hardware or mandatory mission absence during a full session.

## Preservation and next device test

Existing/V2 renderer selection, 256-entry pipeline cache, presets, 30 FPS cap, controls, audio, branding, graphics/SRV compatibility and original assets remain unchanged. No unrelated renderer work. Previous IPAs and protected main remain intact.

After validation: select Sandbox and confirm publication +440=2, engine commit sandbox, original player event, Los Santos spawn with no bank/Prologue, movement/camera/vehicles/collisions/streaming, pause and audio. Test both renderers; stop/relaunch and switch Story→Sandbox→Story. Confirm Story retains its previous startup behavior and saves. Share full logs on failure; do not infer playable Sandbox from mode numbers alone.
