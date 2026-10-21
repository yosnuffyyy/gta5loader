# Isolated Muguet migration checkpoint

This crate is not connected to the production iOS launcher. See the three root `MUGUET-MIGRATION-*.md` reports. Do not call its host fixtures gameplay or install its structural Mach-O as an app.

Build from a full checkout on macOS with Rust1.98.1 and Xcode's iOS SDK. Commands mirror `.github/workflows/muguet-migration.yml`:

```sh
node native-ios/prepare-engine.mjs
node migration/prepare-engine.cjs native-build/engine-original.wasm migration/evidence
cargo +1.98.1 test --manifest-path migration/Cargo.toml --locked --release --features compiler
migration/target/release/migration-probe host migration/evidence/engine-probe.wasm migration/evidence
node migration/reference.cjs migration/evidence/engine-probe.wasm migration/evidence/reference.json migration/evidence/host.json
export MIGRATION_AOT="$PWD/migration/evidence/host.cwasm"
export MIGRATION_AOT_SHA256="$(shasum -a 256 "$MIGRATION_AOT" | cut -d' ' -f1)"
cargo +1.98.1 build --manifest-path migration/Cargo.toml --locked --release --no-default-features --features embedded-check --bin embedded-aot-check
node migration/audit-macho.cjs migration/target/release/embedded-aot-check migration/evidence/host.cwasm migration/evidence/mac-macho.json macos
migration/target/release/embedded-aot-check
migration/target/release/migration-probe ios migration/evidence/engine-probe.wasm migration/evidence
rustup target add aarch64-apple-ios --toolchain 1.98.1
export MIGRATION_AOT="$PWD/migration/evidence/ios.cwasm"
export MIGRATION_AOT_SHA256="$(shasum -a 256 "$MIGRATION_AOT" | cut -d' ' -f1)"
export IPHONEOS_DEPLOYMENT_TARGET=17.0
cargo +1.98.1 build --manifest-path migration/Cargo.toml --locked --release --no-default-features --features embedded-check --target aarch64-apple-ios --bin embedded-aot-check
node migration/audit-macho.cjs migration/target/aarch64-apple-ios/release/embedded-aot-check migration/evidence/ios.cwasm migration/evidence/macho.json
```

`MIGRATION_AOT` must be the trusted artifact just built from the exact pinned engine, never a file chosen from an untrusted game/download directory. The compiled-in SHA and Wasmtime configuration/version checks do not make arbitrary native code safe.

Unix filesystem integration tests run on macOS/Linux. Windows runs portable unit and original-engine/atomic fixtures; it does not exercise the Unix reader. Original large engine artifacts are generated locally from existing verified build inputs; CI publishes textual evidence only. No web folder, RPF, WASM, AOT code or partial IPA is added to a release.

All local changes are contained in `migration/`, its workflow and checkpoint reports. Reverting those changes returns the unmodified legacy source. The application remains on its existing runtime until the explicit promotion gates pass.
