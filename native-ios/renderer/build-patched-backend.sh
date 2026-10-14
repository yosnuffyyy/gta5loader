#!/bin/bash
set -euo pipefail
commit=6aed50955d934ac36049ba8d002034841633ae02
source_dir="$PWD/native-build/wgpu-source"
rustup toolchain install 1.93.1 --profile minimal
rustup target add --toolchain 1.93.1 aarch64-apple-ios aarch64-apple-darwin
git init "$source_dir"
git -C "$source_dir" remote add origin https://github.com/gfx-rs/wgpu-native.git
git -C "$source_dir" fetch --depth 1 origin "$commit"
git -C "$source_dir" checkout --detach FETCH_HEAD
test "$(git -C "$source_dir" rev-parse HEAD)" = "$commit"
git -C "$source_dir" submodule update --init --depth 1
test "$(shasum -a 256 "$source_dir/Cargo.lock" | cut -d' ' -f1)" = 9f780c3e7e792c19ae22e708cf899eb177b5696c9ab61215c6ae235adcecbdb0
(
 cd "$source_dir"
 mkdir -p .cargo
 cargo +1.93.1 vendor --locked --versioned-dirs vendor > .cargo/config.toml
)
node native-ios/renderer/patch-metal-bc.mjs "$source_dir" native-build/evidence/metal-bc-patch.json
export CARGO_TARGET_DIR="$PWD/native-build/rust-target"
for target in aarch64-apple-darwin aarch64-apple-ios; do
 if [ "$target" = aarch64-apple-ios ]; then sdk=iphoneos; else sdk=macosx; fi
 (
  cd "$source_dir"
  export SDKROOT="$(xcrun --sdk "$sdk" --show-sdk-path)"
  export IPHONEOS_DEPLOYMENT_TARGET=17.0
  # Cargo treats restored vendored registry dependencies as immutable. A new
  # checksum table does not reliably invalidate their old compiled objects.
  # Explicitly rebuild this one patched crate and its dependent artifacts.
  cargo +1.93.1 clean --package wgpu-hal --target "$target" --release
  cargo +1.93.1 clean --package naga --target "$target" --release
  cargo +1.93.1 build --locked --offline --release --jobs 2 --target "$target" --no-default-features --features metal,wgsl
 )
 if [ "$sdk" = iphoneos ]; then dest=ios; else dest=macos; fi
 cp "$CARGO_TARGET_DIR/$target/release/libwgpu_native.a" "native-build/wgpu/$dest/lib/libwgpu_native.a"
done
node --input-type=module <<'JS'
import fs from 'node:fs';import crypto from 'node:crypto';
const file='native-build/evidence/renderer-backend.json',j=JSON.parse(fs.readFileSync(file));
j.sourceBuild={commit:j.commit,rust:'1.93.1',features:['metal','wgsl'],locked:true,offlineBuild:true,patch:JSON.parse(fs.readFileSync('native-build/evidence/metal-bc-patch.json')),libraries:['macos','ios'].map(target=>{const p='native-build/wgpu/'+target+'/lib/libwgpu_native.a',b=fs.readFileSync(p);return {target,bytes:b.length,sha256:crypto.createHash('sha256').update(b).digest('hex')};})};
fs.writeFileSync(file,JSON.stringify(j,null,2)+'\n');
console.log('PASS: source-built Metal/WGSL backends for Mac and iPhone; exact BC capability patch and library digests recorded.');
JS
