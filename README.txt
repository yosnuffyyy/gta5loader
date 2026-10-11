GTA OFFLINE SETUP — GITHUB PAGES

Publish these files at the root of yosnuffyyy/gta5loader.
This package is configured for the /gta5loader/ project-site path.
Enable Pages from the main branch, /(root). The .nojekyll file keeps the
prebuilt assets intact. The intended URL is https://yosnuffyyy.github.io/gta5loader/.
The initial visit installs the offline handler and reloads once to enable
the isolation needed for shared WebAssembly memory. The server does not
need custom headers. Browser support is checked before game installation.

On iPhone: open the site in Safari, Share > Add to Home Screen, open the
new icon, Check this device, then Import a local web folder. Select the
complete web folder from GTA-Mobile-Offline or GTA-Browser-Offline on a
USB-C drive or in Files. This only reads local files, without uploading
them to GitHub. Allow 22 GB for the installed game, plus source-copy space.
Keep the app visible and unlocked. When Installed, test local files, then
reopen in Airplane Mode and choose Play offline.

The 21 GB game archives are not in this repository. This contains only the
small installer, launcher and file-check manifest. It is not a native iOS
port. Physical iPhone gameplay remains unverified; the existing engine
requires a 3 GB initial WebAssembly allocation before graphics resources.
Do not clear website data; doing so removes installed files and may remove
saves. The operating system can also evict website storage.
