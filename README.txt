GTA IPHONE SETUP — MEMORY32 COMPATIBILITY EDITION

Open https://yosnuffyyy.github.io/gta5loader/ in Safari, use Share > Add to
Home Screen, then open that icon before importing. Tap Check this device
and choose the complete web folder from GTA-Mobile-Offline or
GTA-Browser-Offline in Files or on a USB-C drive. Keep the app visible,
phone unlocked, and Wi-Fi connected during import.

This edition converts the engine to shared wasm32 memory using Binaryen
133. The initial allocation is 512 MiB, with a 4 GiB maximum. Memory64 is
no longer required. Import your existing web folder unchanged; the
installer downloads approximately 66 MB of verified replacement engine
chunks from this site, then stores them locally with the game. The 21 GB
of game archives are read locally and are not uploaded to GitHub.
After installation, the engine and data are available offline.

Allow 22 GB for installation plus free space for saves and iOS. A source
folder copied onto the phone needs additional storage until removed.
Physical iPhone gameplay still needs verification. The converted engine
rendered Story Mode in a desktop browser with touch controls. Its heap
grew to approximately 2.3 GiB; iOS may stop the game if runtime memory
use exceeds available resources. Passing the feature check does not
guarantee that the entire game fits in the iPhone browser process.

If the old Memory64 message is still shown, close all tabs and the Home
Screen app, open the site online to fetch the update, then close and reopen
it once more. Do not clear website data to update.

Hosting: main branch, /(root), with .nojekyll. Files are configured for
/gta5loader/. The service worker adds isolation headers after the initial
controlled reload. Keep the original website address for offline storage.

IMPORT REPAIR V2

The setup automatically downloads the verified audio, controller, mobile
and engine support scripts along with the compatible engine. Keep Wi-Fi
on during import. Folder paths are preserved before passing files to the
storage worker. A missing, unreadable, wrong-sized or corrupt input file
is listed separately; import continues with the remaining files.

Missing loading-screen artwork/font/spinner and byte-verified duplicate
shader sources are optional. Play offline becomes available once every
required file is verified. Required shader packs and game archives still
need to be installed. No corrupt or missing file is counted as installed.
Select the folder again to repair or resume; verified chunks are retained.

To update, close all tabs and Home Screen windows for this address, open
the site online, then close and reopen once more if the update is waiting.
Look for "Installer update: resilient folder import v2" at the bottom.
Do not clear website data. Your existing game storage is reused.

STARTUP REPAIR V3

Close all game tabs and Home Screen windows, open the site online, then
close and reopen to activate this update. Tap Update engine support.
Only the changed support files (about 137 KB) need downloading; existing
archives and saves are retained. Do not clear website data or reinstall.

Worker initialization is staggered (two at a time in the mobile profile),
with visible worker counts and explicit startup errors/timeouts. Engine
streaming compilation no longer clones the response for progress tracking.
The generic stall message no longer assumes an out-of-memory failure.
If loading still stops, open Loading details and use Copy details. These
diagnostics stay on your device unless you choose to share them.

These changes address confirmed startup error-handling and concurrency
issues. They do not establish the cause on a physical iPhone; full iPhone
gameplay still needs verification.
