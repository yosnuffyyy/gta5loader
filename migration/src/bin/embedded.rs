// Embedding mechanism adapted from Muguet src/main.rs/build.rs:
// Constantin Clerc (c22dev), GPL-3.0-or-later, revision pinned in upstream-lock.
// macOS execution + iOS structural/link check; not a gameplay release.
#[cfg(all(any(target_os = "ios", target_os = "macos"), feature = "compiler"))]
compile_error!("No JIT/compiler in the iOS artifact");
#[cfg(not(any(target_os = "ios", target_os = "macos")))]
fn main() {
    panic!("embedded-aot-check requires an Apple target");
}

#[cfg(any(target_os = "ios", target_os = "macos"))]
std::arch::global_asm!(
    ".section __GAME,__code,regular,pure_instructions",
    ".p2align 14",
    "_migration_code:",
    concat!(".incbin \"", env!("MIGRATION_AOT"), "\""),
    "_migration_code_end:",
    ".text",
);

#[cfg(any(target_os = "ios", target_os = "macos"))]
fn main() -> anyhow::Result<()> {
    use gta_muguet_runtime::{artifact::ExpectedArtifact, runtime::*};
    use std::sync::Arc;
    use wasmtime::{CustomCodeMemory, Engine, Module};
    extern "C" {
        #[link_name = "\x01_migration_code"]
        static START: u8;
        #[link_name = "\x01_migration_code_end"]
        static END: u8;
    }
    let start = &raw const START;
    let end = &raw const END;
    let size = (end as usize)
        .checked_sub(start as usize)
        .ok_or_else(|| anyhow::anyhow!("invalid linker bounds"))?;
    anyhow::ensure!(
        size > 0 && start as usize % 16384 == 0,
        "invalid executable section"
    );
    struct SignedRegion {
        start: usize,
        end: usize,
    }
    impl CustomCodeMemory for SignedRegion {
        fn required_alignment(&self) -> usize {
            1
        }
        fn publish_executable(&self, p: *const u8, n: usize) -> wasmtime::Result<()> {
            let a = p as usize;
            if a >= self.start && a.checked_add(n).is_some_and(|b| b <= self.end) {
                Ok(())
            } else {
                Err(wasmtime::Error::msg(
                    "executable code outside embedded signed section",
                ))
            }
        }
        fn unpublish_executable(&self, p: *const u8, n: usize) -> wasmtime::Result<()> {
            self.publish_executable(p, n)
        }
    }
    // Linker section has static lifetime and is RX from process load onward.
    // We never mprotect/mmap user files or make writable memory executable.
    let bytes = unsafe { std::slice::from_raw_parts(start, size) };
    ExpectedArtifact {
        sha256: env!("MIGRATION_AOT_SHA256"),
        bytes: include_bytes!(env!("MIGRATION_AOT")).len(),
        engine_sha256: ENGINE_SHA256,
        config: CONFIG_ID,
    }
    .verify(bytes)?;
    let mut c = config();
    c.with_custom_code_memory(Some(Arc::new(SignedRegion {
        start: start as usize,
        end: end as usize,
    })));
    let engine = Engine::new(&c)?;
    let raw =
        std::ptr::NonNull::new(std::ptr::slice_from_raw_parts(start, size) as *mut [u8]).unwrap();
    let module = unsafe { Module::deserialize_raw(&engine, raw) }?;
    // Execute original functions directly from the linked RX region. No host
    // deserializer, compiler, file-backed executable mmap or fake host success.
    use wasmtime::{ExternType, Linker, MemoryType, SharedMemory, Store, Val};
    let memory = SharedMemory::new(
        &engine,
        MemoryType::shared(INITIAL_PAGES as u32, MAX_PAGES as u32),
    )?;
    let mut store = Store::new(&engine, ());
    let mut linker = Linker::new(&engine);
    for import in module.imports() {
        match import.ty() {
            ExternType::Memory(ty) => {
                anyhow::ensure!(
                    import.module() == "env"
                        && import.name() == "memory"
                        && ty.is_shared()
                        && !ty.is_64()
                        && ty.minimum() == INITIAL_PAGES
                        && ty.maximum() == Some(MAX_PAGES),
                    "memory ABI mismatch"
                );
                linker.define(&store, import.module(), import.name(), memory.clone())?;
            }
            ExternType::Func(ty) => {
                let name = format!("{}::{}", import.module(), import.name());
                linker.func_new(import.module(), import.name(), ty, move |_, _, _| {
                    Err(wasmtime::Error::msg(format!(
                        "unexpected embedded-probe import: {name}"
                    )))
                })?;
            }
            _ => anyhow::bail!("unsupported embedded-probe import"),
        }
    }
    let instance = linker.instantiate(&mut store, &module)?;
    instance
        .get_typed_func::<(), ()>(&mut store, "migration_init")?
        .call(&mut store, ())?;
    instance
        .get_global(&mut store, "migration_stack")
        .ok_or_else(|| anyhow::anyhow!("missing stack"))?
        .set(&mut store, Val::I64(96 * 1024 * 1024))?;
    let sin = instance
        .get_typed_func::<f32, f32>(&mut store, "migration_sinf")?
        .call(&mut store, 0.25)?;
    let cos = instance
        .get_typed_func::<f32, f32>(&mut store, "migration_cosf")?
        .call(&mut store, 0.25)?;
    anyhow::ensure!(
        sin.to_bits() == 0x3e7d5777 && cos.to_bits() == 0x3f780aa5,
        "original embedded math mismatch"
    );
    println!(
        "{}",
        serde_json::json!({"result":"PASS","platform":std::env::consts::OS,
        "wasmtime":"49.0.2","compilerEnabled":false,"artifactBytes":size,
        "artifactSHA256":env!("MIGRATION_AOT_SHA256"),"originalSin":sin,"originalCos":cos,
        "linkedRXExecution":true,"gameplayVerified":false})
    );
    Ok(())
}
