# Migration source attribution

Muguet by Constantin Clerc (c22dev), GPL-3.0-or-later:
https://github.com/c22dev/muguet/tree/1b4600b052b864c78fe36180073ff406663b7e0b

`src/runtime.rs` adapts the configuration approach in `src/main.rs`.
`src/bin/embedded.rs` and `build.rs` adapt its signed executable-section embedding
and linker setup. Original author notices are retained in adapted files.
The upstream GPL text is preserved in `licenses/MUGUET-GPL-3.0.txt`.

The asset adapter is newly written from the existing native project's manifest
and bounded-reader contract. It intentionally does not use Muguet's destructive
ZIP import or its 1,024-file reader policy. The test host is a strict compatibility
fixture, not a copied permissive host stub table.

`upstream-lock.json` records SHA256 and size for 316 relevant upstream files,
including dependencies, vendor sources, launcher, iOS sources and build scripts.
Only the components identified above have been adapted; a lock entry is not a
claim that its component was imported or its behavior fully validated.

Rust 1.98.1; Wasmtime 49.0.2 (matching the pinned upstream lock, not merely its
`version = "49"` range). Cargo.lock pins all selected dependencies and registry
integrity checksums. Wasmtime/Cranelift and Rust dependencies retain their own
licenses. No upstream binary, game archive, shader content, credentials or web
folder is added to this migration source.
