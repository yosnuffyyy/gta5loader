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
