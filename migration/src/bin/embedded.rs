// Embedding mechanism adapted from Muguet src/main.rs/build.rs:
// Constantin Clerc (c22dev), GPL-3.0-or-later, revision pinned in upstream-lock.
// A structural/link check, not an iOS application or gameplay release.
#[cfg(all(target_os = "ios", feature = "compiler"))]
compile_error!("No JIT/compiler in the iOS artifact");
#[cfg(not(target_os = "ios"))]
fn main() {
    panic!("embedded-aot-check is an iOS link/packaging check only");
}

#[cfg(target_os = "ios")]
std::arch::global_asm!(
    ".section __GAME,__code,regular,pure_instructions",
    ".p2align 14",
    "_migration_code:",
    concat!(".incbin \"", env!("MIGRATION_AOT"), "\""),
    "_migration_code_end:",
    ".text",
);

#[cfg(target_os = "ios")]
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
    println!(
        "Embedded original-engine probe loaded; {} imports. No gameplay initialized.",
        module.imports().len()
    );
    Ok(())
}
