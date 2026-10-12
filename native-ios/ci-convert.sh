#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p native-build/evidence native-build/generated
exec > >(tee native-build/evidence/build.log) 2>&1
shasum -a 256 b/8b0b5899ed/game.wasm
echo 'e65bec476e8fd8125515150c24037c9bd0d0c46530e3544f9d389ab108b5592e  b/8b0b5899ed/game.wasm' | shasum -a 256 -c -
git clone --depth 1 --branch 1.0.42 https://github.com/WebAssembly/wabt.git native-build/wabt
test "$(git -C native-build/wabt rev-parse HEAD)" = ff0ef7e0009402740c805a9744c09b05be063e48
git -C native-build/wabt submodule update --init --depth 1 third_party/picosha2 third_party/simde
node native-ios/patch-wabt.mjs native-build/wabt
cmake -S native-build/wabt -B native-build/wabt-build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF -DBUILD_LIBWASM=OFF
cmake --build native-build/wabt-build --target wasm2c wasm-validate wat2wasm -j 3
native-build/wabt-build/wasm-validate b/8b0b5899ed/game.wasm --enable-threads
native-build/wabt-build/wasm2c b/8b0b5899ed/game.wasm --enable-threads --no-debug-names --num-outputs=128 -n gta -o native-build/generated/gta.c
cp native-build/generated/gta.h native-build/evidence/gta.h
du -sh native-build/generated
find native-build/generated -type f | wc -l
echo 'PASS: complete engine converted to C with native wait/notify calls.'
