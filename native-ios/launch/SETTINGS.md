# Native launcher settings contract

The launcher stores one versioned `NativeSettings` value. UIKit owns its editable copy; the launch bridge takes a value snapshot. Engine threads do not read `NSUserDefaults` or UIKit. Existing native/AOT execution is the only operational runtime; Muguet Stage A remains on its independent branch.

## Precedence and lifetime

1. Defaults apply only when no saved value exists.
2. Legacy preferences migrate once into `launcherSettings.v1`; they never override that record afterward.
3. A preset edits graphics and ambient population fields. Mode, frame cap, audio, input, renderer/core, diagnostics and fullscreen remain independent.
4. An individual graphics/population edit selects Custom. Relaunch never reapplies a preset over Custom.
5. Invalid schema, types, ranges or nonfinite values block launch. Reset explicitly restores defaults without changing the selected folder or either save namespace.
6. PLAY copies the validated configuration. Startup arguments are generated once, without duplicate flags. Original CApp command-line overrides apply after settings-file loading.
7. Every five seconds at the existing engine-owner pad boundary, diagnostics read the original effective configuration. Requested arguments and current engine values are distinct. This is not a visual-quality or achieved-FPS assertion.

Graphics, mode, renderer/core, profiling, compatibility and display-metric collection require a new session. Host master gain/mute, input configuration, overlay geometry/opacity and fullscreen can change through the existing paused settings lifecycle. No GPU target or engine configuration changes occur live.

## Supported mappings

| Setting | Actual consumer / domain |
| --- | --- |
| Mode | Original input publication +440, Story=1 / Sandbox=2; existing startup/script decision and isolated saves |
| Base | 640×360, 960×540, 1280×720; exact 16:9 |
| Scale | 50/60/70/75/80/90/100%; rounded to a 32×18 lattice, actual pixels previewed |
| 640 base examples | 50%=320×180; 70%=448×252; 100%=640×360 |
| FPS limit | Original `frameLimit` is a 60 Hz vertical-sync divisor: 15→4, 20→3, 30→2 |
| Texture | `textureQuality` 0/1/2 = Normal/High/Very High |
| Anti-aliasing | `fxaa` 0/1; single-sample targets, TXAA and reflection MSAA disabled |
| Shadow | `shadowQuality` -1/0/1/2 maps to engine 0/off or 1/2/3 |
| Reflection | `reflectionQuality` 0..3 |
| Water / particles / grass | `waterQuality` 0..1 / `particleQuality` 0..2 / `grassQuality` 0..3 |
| Post / SSAO / anisotropy | `postFX` 0..3 / `SSAO` 0..2 / `anisotropicQualityLevel` 0/2/4/8/16 |
| LOD / motion blur | `lodScale`, `pedLodBias`, `vehicleLodBias`, `MotionBlurStrength`, each 0..1 |
| Population | `cityDensity`, `pedVariety`, `vehicleVariety` 0..1; variety is not density |
| Audio | Atomic native gain applied to actual stereo PCM after ring consumption; mute drains without audible output |
| Input | Separate touch/controller look scaling, invert-Y, deadzone, touch enable; original ordered input bridge |
| Fullscreen / UI | Existing safe-area aspect-fit surface; persisted host preference |
| Renderer / CPU core | Existing independent native launch switches; no new engine implementation |

Exact original-engine parameter objects, command names, value-reader instructions, destinations and SHA256 instruction-slice fingerprints are recorded in `settings-contract.json`. The regression runs C-generated arguments through original main/argument parsing and original, hash-verified CApp override blocks. Its test-only binding isolates consumers; it does not execute the complete world or prove physical rendering quality.

## Explicitly unavailable

24/40 FPS do not map to integral original 60 Hz divisors; 60/uncapped simulation is not validated. MSAA2 is unavailable in this backend; complete game-wide MSAA4 resolve/readback handling is not validated. Ultra Low/Low texture tiers lack a verified lower mip policy. Native display/854×480, alternate upscalers, soft/long shadows, individual bloom/DOF/fog/lens toggles, independent audio buses, haptics and manual source locking remain unavailable. No dummy controls are provided.

Higher valid graphics values consume more resources and may encounter unobserved game shader/operation combinations. Domain/argument validation is not a guarantee of sufficient memory for every Story/Sandbox scene. Physical iPhone visual correctness, sustained memory/FPS, complete AA quality and audible output remain device checks.

## UI and overhead

Original procedural vector artwork, system typography and SF Symbols; no proprietary launcher assets, remote downloads, blur, animation loop or display link. The PLAY dock remains visible while the library scrolls. Sidebar navigation becomes horizontal on narrow screens. Settings pages use cancellable scrolling and dynamic text. No settings serialization occurs per draw/frame. The fixed configuration is 128 bytes; PCM gain uses one lock-free atomic read per callback and bounded multiplication, with a unity fast path.
