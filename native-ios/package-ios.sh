#!/bin/bash
set -euo pipefail
app=native-build/Payload/NativeProbe.app
mkdir -p "$app"
sdk=$(xcrun --sdk iphoneos --show-sdk-path)
link=(xcrun --sdk iphoneos clang -target arm64-apple-ios17.0 -isysroot "$sdk" \
  -fobjc-arc -O1 -Inative-ios -framework UIKit -framework Foundation \
  -framework Metal -framework GameController -framework UniformTypeIdentifiers \
  -framework QuartzCore -framework CoreGraphics -framework IOSurface -lc++ \
  -Wl,-no_adhoc_codesign -Wl,-dead_strip native-ios/App.m @native-build/ios/objects.txt native-build/ios/native_renderer.o native-build/wgpu/ios/lib/libwgpu_native.a -o "$app/NativeProbe")
if ! "${link[@]}" > native-build/evidence/ios-link.txt 2>&1; then
  cat native-build/evidence/ios-link.txt
  # The large generated engine can expose a branch-island layout bug in ld.
  # Retry only this specific error, using the SDK's alternate linker if present.
  if grep -Eq 'B/BL out of range|branch out of range' native-build/evidence/ios-link.txt && xcrun --find ld-classic >/dev/null 2>&1; then
    echo 'Retrying large ARM64 link with the available SDK classic linker.'
    "${link[@]}" -Wl,-ld_classic >> native-build/evidence/ios-link.txt 2>&1 || { cat native-build/evidence/ios-link.txt; exit 1; }
    echo 'SDK classic linker used for the large ARM64 executable.' > native-build/evidence/ios-linker-selection.txt
  else
    exit 1
  fi
else
  echo 'Default SDK linker with unreachable-section stripping.' > native-build/evidence/ios-linker-selection.txt
fi
cat native-build/evidence/ios-link.txt
cp native-ios/Info.plist "$app/Info.plist"
cp native-ios/file-probes.json "$app/file-probes.json"
cp native-build/wabt/LICENSE "$app/WABT-LICENSE.txt"
cp native-build/wabt/third_party/simde/COPYING "$app/SIMDe-LICENSE.txt"
cp native-build/evidence/engine-inventory.json "$app/engine-inventory.json"
cp native-build/wgpu/LICENSE.MIT "$app/wgpu-native-LICENSE.MIT"
cp native-build/wgpu/LICENSE.APACHE "$app/wgpu-native-LICENSE.APACHE"
cp native-ios/renderer/backend.json "$app/renderer-backend.json"
cp native-ios/renderer/shader-fixtures.json "$app/shader-fixtures.json"
cp native-ios/renderer/draw-fixtures.json "$app/draw-fixtures.json"
cp native-build/evidence/metal-bc-patch.json "$app/metal-bc-patch.json"
cp native-build/evidence/renderer-backend.json "$app/renderer-build.json"
plutil -lint "$app/Info.plist"
file "$app/NativeProbe" | tee native-build/evidence/ios-binary.txt
xcrun vtool -show-build "$app/NativeProbe" | tee -a native-build/evidence/ios-binary.txt
otool -L "$app/NativeProbe" | tee -a native-build/evidence/ios-binary.txt
if otool -L "$app/NativeProbe" | grep -q WebKit; then echo 'Unexpected WebKit dependency';exit 1;fi
if codesign --verify "$app" 2>/dev/null; then echo 'Expected an unsigned app';exit 1;fi
cd native-build
ditto -c -k --keepParent Payload GTA-Native-Probe-unsigned.ipa
shasum -a 256 GTA-Native-Probe-unsigned.ipa | tee evidence/ipa-sha256.txt
echo 'PASS: unsigned ARM64 iPhone diagnostic IPA packaged.'
