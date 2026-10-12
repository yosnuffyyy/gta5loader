APPLE MEMORY V9 — EXPERIMENTAL MOBILE UPDATE

Open https://yosnuffyyy.github.io/gta5loader/ online. Close and reopen until
the footer says Apple memory v9. Keep website data and your installed files.
This is a small launcher update: no new engine or 21 GB reimport is needed
for an existing v7/v8 installation. Choose Play Prologue or Try Sandbox.
Use stable v7 remains available. V7 is the only confirmed iPhone baseline.

V9 keeps 512x288 at 30 FPS while testing memory improvements. It reads
installed files directly through up to eight OPFS handles, with at most
256 KiB scratch if shared buffers cannot be read directly. An unavailable
OPFS directory falls back to the existing HTTP file reader. Game details
shows which reader is active. Resource sizes and file contents are checked.

The graphics worker uses 2 MiB uniform-ring chunks, caps its temporary pool
at 4 MiB / 64 size classes, releases auxiliary resources, and reports more
allocations. Shader formats and math, GPU submission ordering, collision,
audio and required engine threads are retained. The engine keeps 64 MiB
resource segments and strips only trailing debug names while compiling.
The source engine and the separate v7 fallback are unchanged.

Small population limits apply to random generation, not every mission actor.
Pair a Bluetooth gamepad through iPhone Settings, then press a button in
the game. Existing controller and touch mappings are retained.

This build needs a physical iPhone test. Desktop tests and allocation
estimates do not establish iPhone process-memory limits or full-map stability.
The PC package is unchanged.
