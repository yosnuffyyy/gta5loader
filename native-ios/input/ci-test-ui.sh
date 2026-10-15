#!/bin/bash
set -euo pipefail
mkdir -p native-build/input-ui
xcrun simctl list runtimes --json > native-build/input-ui/runtimes.json
runtime=$(node -e "const r=require('./native-build/input-ui/runtimes.json').runtimes.find(r=>r.isAvailable&&r.identifier.includes('.iOS-'));process.stdout.write(r?.identifier||'')")
if [ -z "$runtime" ]; then
  echo '{"result":"UNAVAILABLE","reason":"No installed iOS simulator runtime. UIKit execution not verified."}' > native-build/evidence/native-input-ui.json
  exit 0
fi
sdk=$(xcrun --sdk iphonesimulator --show-sdk-path)
arch=$(uname -m)
app=native-build/input-ui/InputUITest.app
mkdir -p "$app"
clang -target "$arch-apple-ios17.0-simulator" -isysroot "$sdk" -fobjc-arc -O1 -Inative-ios native-ios/input/test_ui.m native-ios/input/native_input.c -framework UIKit -framework Foundation -framework Metal -framework GameController -framework UniformTypeIdentifiers -framework QuartzCore -framework CoreGraphics -o "$app/InputUITest"
cat > "$app/Info.plist" <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?><!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd"><plist version="1.0"><dict><key>CFBundleIdentifier</key><string>io.github.yosnuffyyy.inputuitest</string><key>CFBundleExecutable</key><string>InputUITest</string><key>CFBundlePackageType</key><string>APPL</string><key>CFBundleName</key><string>Input UI Test</string><key>CFBundleVersion</key><string>1</string><key>CFBundleShortVersionString</key><string>1.0</string><key>MinimumOSVersion</key><string>17.0</string><key>LSRequiresIPhoneOS</key><true/><key>UIDeviceFamily</key><array><integer>1</integer></array><key>UILaunchScreen</key><dict/></dict></plist>
PLIST
codesign --force --sign - "$app"
xcrun simctl list devicetypes --json > native-build/input-ui/device-types.json
kind=$(node -e "process.stdout.write(require('./native-build/input-ui/device-types.json').devicetypes.filter(x=>x.name.startsWith('iPhone')).at(-1).identifier)")
device=$(xcrun simctl create NativeInput029 "$kind" "$runtime")
trap 'xcrun simctl shutdown "$device" >/dev/null 2>&1 || true; xcrun simctl delete "$device" >/dev/null 2>&1 || true' EXIT
xcrun simctl boot "$device"
xcrun simctl bootstatus "$device" -b
xcrun simctl install "$device" "$app"
xcrun simctl launch "$device" io.github.yosnuffyyy.inputuitest
container=$(xcrun simctl get_app_container "$device" io.github.yosnuffyyy.inputuitest data)
for attempt in $(seq 1 30); do
  if [ -f "$container/Documents/input-ui-result.json" ]; then break; fi
  sleep 1
done
cp "$container/Documents/input-ui-result.json" native-build/evidence/native-input-ui.json
cat native-build/evidence/native-input-ui.json
node -e "const r=require('./native-build/evidence/native-input-ui.json');if(r.result!=='PASS'||r.checks<60)throw Error('UIKit/controller regression failed: '+JSON.stringify(r))"
