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

STARTUP REPAIR V7

Open the site online, close and reopen the Home Screen app until the footer
says minimal engine v7, then tap Update engine support. The replacement
engine downloads about 66 MB. Keep the existing installation and website
data; reimporting the whole game is unnecessary.

Play offline starts the Prologue instead of automatically loading the saved
downtown scene. Existing game files are retained; normal game autosaving
can still occur. Compact mode uses 512x288 at 30 FPS, shorter draw distance,
no optional DLC, random pedestrians, random traffic or ambient events.
Mission characters and audio remain enabled. Full-map stability and later
missions have not been established. Recording capacity is heavily reduced.

Main/extra heaps reserve 352/80 MiB; replay 16 MiB; recorder 1 MiB. Resource
heaps grow in 32 MiB segments within the original budget. Streaming permits
four pending reads per device and an 8 MiB burst; a single larger requested
asset is still allowed to complete. The file cache keeps at most 16 MiB.

Original PC game files and the 21 GB archives are unchanged. Update only
replaces the iPhone engine and support files. Patches preserve section
offsets, ABI, imports/exports and checked allocation paths. Game details
remains available during play. The recovery report stays local.

The physical iPhone must still be tested; desktop rendering is not proof
of iPhone success. Do not clear website data if the phone restarts.
