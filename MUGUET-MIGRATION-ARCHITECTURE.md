# Project Muguet Rebirth — architecture checkpoint

Date: 2026-10-21. Branch: `muguet-rebirth`. **Implemented Stage A runtime/ABI checkpoint; full migration incomplete. Not a gameplay release.** The production application still uses Native 0.44. No legacy execution layer has been removed from that application. The separate Rust gate executes original Wasm through Wasmtime/Cranelift without wasm2c or the old renderer.

## Source identity and rollback

| Input | Exact identity |
|---|---|
| Preserved Native 0.44 executable | `fb7a11e2c0571c5187b98411807226edb7fcc863` |
| Preserved native branch documentation baseline | `6ee0c328af8dde211fca95580db669cf3531897b` |
| Muguet source | `c22dev/muguet@1b4600b052b864c78fe36180073ff406663b7e0b` |
| New runtime checkpoint source | `8a1d6819e5e8b51802bb26251b9e24942d98a3a0` |
| Checkpoint source tree | `1a641963dadaa085d35dbb92f07ce641d240df01` |
| Original engine | 66,172,903 bytes; SHA256 `e65bec476e8fd8125515150c24037c9bd0d0c46530e3544f9d389ab108b5592e` |
| Export-only test module | SHA256 `6386a4047a5c9c5d5384b1127b4dbd37d76970abfca620c9632bd9335c1b6d7b` |
| Preserved 0.44 IPA | 105,001,835 bytes; SHA256 `e847e89988e792f530c301f17a1c00296a356bd7a8ce141e70f6d5cce127f4af` |

All 44 previously retained IPA files were rehashed and remained byte-identical. The previous release remains at https://github.com/yosnuffyyy/gta5loader/releases/tag/native-0.44. Source publication is isolated to the explicitly requested migration branch. `native-ios-prototype`, deployed `main`, installed assets, user saves, selected-folder bookmarks and previous IPAs are not migrated or overwritten. The two protected Desktop packages are not development inputs.

## Implemented execution path

`original pinned game.wasm -> additional test exports only -> host Cranelift AOT -> trusted artifact -> Wasmtime runtime -> strict test host`.

`migration/prepare-engine.cjs` verifies the complete original SHA before parsing and records import/export/function signatures and section digests. Its only transformation appends test exports; code, data, imports, types and elements remain unchanged. No `wasm-opt`, gameplay hook, memory-map rewrite, scenario rewrite or Opus replacement is applied. All 11 named upstream patch targets are present; their exact signatures are recorded. Presence does not authorize a patch.

The complete engine module compiles and instantiates. Its original string/math functions, forward-shadow lookup, mode publication and original script command-line decisions execute. The launch fixture deliberately disables GPU construction and traps at input publication, as the existing native fixture does. **It does not run complete constructors, initialize a playable world or prove mission progression.** Unexpected host imports trap by name rather than returning zero. Three fixture import behaviors exist: bounded log writes, test-clock reads and the explicit mode-publication boundary; these are not a complete host ABI.

### Memory and ABI

The engine has 91,111 functions and 86 imports: 85 functions plus `env.memory`. Memory is **wasm32 shared**, initially 8,192 pages / 512 MiB, maximum 65,536 pages / 4 GiB. Its C ABI nevertheless uses i64 pointers, 8-byte argv entries and 16-byte iovecs. Global 0 is the stack pointer; global 1 is TLS base. `runtime::range` rejects high pointer bits and arithmetic overflow instead of truncating them.

Wasmtime reserves a stable 4-GiB address region with a 64-KiB guard. This is virtual reservation, not a claim that 4 GiB is resident or available to iOS. Initial memory and growth still obey the exact original module; a one-page growth preserves the base, and over-limit growth fails. No increased engine heap budget is introduced. Shared-memory fixture access is quiescent except for explicit Wasm atomic operations. It is not a general concurrent Rust slice API.

The new `abi::Stat64` writes the original **104-byte** layout, preserves timestamps/nanoseconds and 64-bit size/inode, zeroes padding, checks destination capacity before writing, and leaves trailing bytes intact. Muguet's `fs::write_stat` writes 112 bytes. Importing it unchanged could overwrite eight bytes beyond the original caller's record. The serializer is tested but not yet connected to a live syscall host.

### AOT trust and iOS executable memory

Rust 1.98.1 and Wasmtime **49.0.2** are pinned, matching upstream's resolved Wasmtime version. `Cargo.lock` pins registry checksums. The host compiler enables cross-architecture backends; the default/iOS runtime has no Cranelift code generator. ThinLTO, opt-level 3 and one codegen unit apply to the Rust release build. Cranelift uses `Speed`; bounds and Spectre mitigations are not deliberately disabled.

The host tool precompiles the exact probe for `aarch64-apple-ios`. `embedded-aot-check` adapts Muguet's executable-section mechanism: artifact bytes in 16-KiB-aligned `__GAME,__code`, linked RX, compiled-in SHA, bounds-checked `CustomCodeMemory`, then `Module::deserialize_raw`. No JIT, user-selected executable artifact, writable/executable segment, `mprotect` bypass or jailbreak is introduced. The host-only ordinary deserializer is excluded on iOS. Its unsafe contract requires a trusted build digest, not a digest supplied with arbitrary downloaded code.

CI checks ARM64 architecture, iOS platform load command, byte-exact embedded artifact, alignment, and no writable/executable segment at either initial or maximum permissions. **A linked Mach-O is not an installed app, signed execution proof or IPA.** iPhone execution remains a gate before application integration.

Final CI [39646902321](https://github.com/yosnuffyyy/gta5loader/actions/runs/39646902321) passed at source 8a1d6819e5e8b51802bb26251b9e24942d98a3a0. A compiler-free macOS ARM64 executable also loaded the full module from the linked RX section and executed original initialization plus sin/cos, checking exact result bits. This closes the hosted executable-memory mechanism gate; it does not establish iOS execution. Textual evidence and artifact hashes are retained under `evidence/muguet-stage-a/`.

## Existing-folder compatibility adapter

`assets::Reader` opens the already-authorized selected `web` root and its existing `data/manifest.json`. It preserves original manifest bytes, tuple order and numeric file IDs. Paths map to existing `web/data/<entry>` files; nothing is copied, downloaded or rewritten. It rejects traversal, unsupported paths, ambiguous case-folding, duplicate entries, invalid numeric sizes and size mismatches. Exact case takes precedence; optional ASCII-insensitive resolution fails on collisions. Directory-descriptor walks reject symlinks and keep open-file identity stable across pathname replacement.

Reads use **one reusable 64-KiB transfer buffer and at most eight cached archive descriptors**, plus directory descriptors and bounded manifest/index storage. The manifest is capped at 8 MiB and 20,000 entries; it is not whole-archive caching. Source data larger than a transfer is read in chunks. Expected EOF clipping is distinct from an unexpected short file. Cancellation or destination failure is an error even after a partial transfer; no caller may publish a successful incomplete request. Descriptor ownership is RAII and session-local. Tests cover 12 archives, eviction, replacement, EOF, invalid ranges, symlinks, truncation and cancellation.

Existing save namespace names remain `NativeRuntimeProbe` for Story and `NativeSandboxProbe` for Sandbox. The adapter exposes the distinction but **save writes/persistence are not integrated or tested through the new runtime**. iOS security-scope lifetime, external bookmark recovery, native HTTPFS ring publication, selected shader paths/database loading, and save transactions are next-stage work. The engine/AOT comes from verified existing build inputs, not a different game download. Upstream's destructive ZIP importer is excluded.

## Upstream component decisions

The complete upstream repository was fetched. `migration/upstream-lock.json` hashes 316 relevant source/vendor/build files. The table identifies inspected contracts; it does not claim every upstream line or feature has been audited or imported.

| Upstream component | Decision and remaining contract |
|---|---|
| Cargo manifests, `main.rs`, build/iOS scripts | Pinned; adapt runtime/AOT embedding. Do not copy 5-GiB iOS reservation or disabled mitigations blindly. |
| `host.rs` | Reviewed host dispatch/thread boundary. Rebuild with exact signatures and session ownership; unknown calls must error. |
| `patch.rs` | Enumerated 11 named targets/signatures. Apply none yet; typed/behavioral tests required for every transformation. |
| `fs.rs`, `io.rs` | Read-only adapter rewritten for existing layout, 104-byte stat and 8-file/64-KiB bounds; no 1,024-file policy. Full syscalls/ring pending. |
| `import.rs` | Excluded destructive ZIP/import and DLC rewriting. |
| `gpu.rs`, vendored wgpu | Inspected init, query, texture, copy, sampling and A2C paths. Renderer migration pending; upstream policies are not pixel references. |
| `audio.rs`, `opus.rs` | Inspected output/decode contracts. Keep real PCM/decoder tests, bounded callback ownership and restart lifecycle; not yet ported. |
| `input.rs`, `pad.rs`, `touch.rs`, keyboard/mouse | Inventoried interfaces; preserve existing native event ordering and automatic switching when integrating. No controls changed. |
| `app.rs`, `config.rs`, `ios/` | Preserve existing UI, app identity and persisted options. No launcher replacement shipped. |
| `heapcheck.rs` | Diagnostic design reviewed; no heap mutation or unsupported memory-budget increase imported. |
| `mp.rs`, `proto.rs`, `server.rs` | Offline target; no multiplayer/network hooks imported. Authentication/thread-lifetime work required before optional networking. |

Upstream Opus patch emits six arguments for `opus_decode_frame`, while this original function has five parameters `(i64,i64,i32,i64,i32)->i32`. Its local 5 is a zero-initialized i32, so this is **not by itself proof of invalid Wasm**: it would supply that local as the sixth value. Decoder-state and FEC semantics still require exact validation. No claim is made that this explains historical silent audio.

Other deferred incompatibilities include upstream query normalization to 65,536 visible samples, single-sample A2C disablement, generic dummy comparison resources, silent unsupported copy/mipmap behavior, and global once-only worker/audio ownership. These cannot replace the existing project's exact semantics without tests. See the regression ledger.

## Planned ownership, not yet implemented

One session must own the shared memory, main/worker Stores, typed import registry, bounded file service, save root, audio ring, input queue, renderer, cancellation and join handles. One Store per engine thread; no concurrent Store access. Shared memory must initialize once, with original per-worker stack/TLS lifecycle. Worker TLS and original mailbox/proxy calls must execute with their original ownership/order. Stop must wake waits, finish/cancel I/O, retire GPU work, join workers and only then release memory. Process-global raw runtime pointers and detached workers must not survive relaunch.

The new renderer will use upstream Rust wgpu 30.0.1 / Metal after preserving the existing protocol and semantic fixes. Current migration Cargo does **not** compile wgpu or replace the renderer. No two runtimes execute one session. Promotion requires original-world startup, graphics/pixel equivalence, audio/input/save integration, stop/relaunch and sustained device comparisons.

## Acceptance gates and next action

Stage A core ABI/AOT/atomic/asset components have executable tests. Stage A remains incomplete for engine TLS, longjmp/exception handling, all host imports and on-device signed AOT execution. Stages B–F are uncompleted. **Next priority: strict session-owned host/import and Emscripten TLS/worker/mailbox integration, then full original constructors with failure propagation.** Wire the bounded adapter to the original HTTPFS protocol without fabricating reads. Only then integrate the renderer and replay historical pixel/query fixtures.

GPL-3.0-or-later Muguet notices and full license are retained in `migration/ATTRIBUTION.md` and `migration/licenses/`; dependency licenses remain distinct. This source checkpoint does not redistribute game archives or an upstream application binary.
