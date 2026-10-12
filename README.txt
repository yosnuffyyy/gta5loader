GTA OFFLINE IPHONE SETUP — LIGHT STREETS V8

Hosted setup: https://yosnuffyyy.github.io/gta5loader/

UPDATING AN EXISTING INSTALLATION
Open online, then close and reopen until the footer says light streets v8.
Existing v7 installs receive a small launcher update; installed engine bytes
and game archives are unchanged. Use Update engine support only if offered.
Keep website data and the existing game installation.

PLAY OPTIONS
Play Prologue: 640x360 with a 30 FPS target and reduced world detail.
Try city streets: experimental sparse pedestrians and traffic in downtown.
Use stable v7: the unchanged 512x288 profile that ran on the iPhone 16 Pro Max
for 229 seconds without a reported exception, as confirmed by the user.
Prologue starts a new session; normal game autosaving can still occur.
City scenes use more memory. Physical iPhone testing of v8, later missions,
long sessions and the full map is still required. Desktop tests do not prove
phone stability or a sustained phone frame rate.

OPTIMIZATIONS AND LIMITS
The v8 renderer keeps a fixed 640x360 backing buffer across orientation changes.
Random population, parked vehicles, world detail and optional ambient events
are reduced. Mission characters/audio remain enabled. Main and extra engine
heaps remain 352/80 MiB, replay 16 MiB, recorder 1 MiB. Streaming retains four
pending reads per device, an 8 MiB burst and a 16 MiB file cache.
V8 uses 64 MiB resource segments so a 31 MiB map allocation can fit; v7 retains
32 MiB segments. The v8 loader checks and transforms two constant bytes while
streaming the existing engine to its compiler. It never rewrites the installed
engine and avoids a second full-engine download or full-buffer copy. Original
allocation checks, imports/exports, code offsets and resource budgets remain.

CONTROLLERS
Pair a compatible Bluetooth controller in iPhone Settings, open the game,
and press a button. The input loop samples at up to 60 Hz, stops while hidden,
and resumes safely. Keyboard and touch remain available. Native input mapping
and simulated PlayStation/Xbox hotplug, analog input, held-button release,
keyboard takeover, focus and legacy adapters are tested. Physical Bluetooth
on the iPhone still needs confirmation. Legacy adapters require the phone and
browser to expose them as gamepads; this website cannot add an OS driver.

NEW INSTALLATIONS / HOSTING
Host the contents of this folder on HTTPS at /gta5loader/. GitHub Pages is the
current host. The setup contains the support engine, not the 21 GB game archive.
Open it in Safari, add it to the Home Screen, and open the Home Screen app before
importing the complete local web folder. Let the import finish, then play offline.
Opening an HTML file directly in iPhone Files cannot start this engine.
WebGPU, shared WebAssembly, cross-origin isolation and sufficient free storage
and runtime memory are required. Installed content remains in website storage;
clearing that data removes it. Game details and restart recovery stay local.

The original desktop and full offline packages have not been modified by v8.
SHA256SUMS.txt verifies the files shipped in this setup package.
