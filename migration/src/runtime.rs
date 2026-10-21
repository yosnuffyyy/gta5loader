// Adapted from Muguet src/main.rs, Constantin Clerc (c22dev), GPL-3.0-or-later.
// Pinned upstream: 1b4600b052b864c78fe36180073ff406663b7e0b.
// Deliberate differences: exact wasm32 limits; no gameplay patches, arbitrary
// coercion, disabled Spectre mitigation, default-success imports or global RT.
use wasmtime::Config;

pub const ENGINE_SHA256: &str = "e65bec476e8fd8125515150c24037c9bd0d0c46530e3544f9d389ab108b5592e";
pub const PROBE_SHA256: &str = "6386a4047a5c9c5d5384b1127b4dbd37d76970abfca620c9632bd9335c1b6d7b";
pub const INITIAL_PAGES: u64 = 8192;
pub const MAX_PAGES: u64 = 65536;
pub const CONFIG_ID: &str = "wasmtime49.0.2-wasm32-shared-4GiB-64KiBguard-speed-v1";

pub fn config() -> Config {
    let mut c = Config::new();
    c.wasm_memory64(false);
    c.wasm_threads(true);
    c.shared_memory(true);
    c.wasm_bulk_memory(true);
    c.wasm_simd(true);
    c.memory_reservation(MAX_PAGES * 65536);
    c.memory_guard_size(65536);
    c.memory_may_move(false);
    #[cfg(feature = "compiler")]
    c.cranelift_opt_level(wasmtime::OptLevel::Speed);
    c
}

/// Checked wasm32 address range for a 64-bit C-ABI pointer. Never truncate.
pub fn range(
    address: u64,
    length: usize,
    committed: usize,
) -> anyhow::Result<std::ops::Range<usize>> {
    anyhow::ensure!(
        address <= u32::MAX as u64 || length == 0 && address == 1u64 << 32,
        "non-wasm32 address"
    );
    let start = usize::try_from(address)?;
    let end = start
        .checked_add(length)
        .ok_or_else(|| anyhow::anyhow!("address overflow"))?;
    anyhow::ensure!(
        end <= committed && end as u64 <= 1u64 << 32,
        "range outside committed memory"
    );
    Ok(start..end)
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn ranges() {
        assert_eq!(range(8, 8, 16).unwrap(), 8..16);
        assert_eq!(range(16, 0, 16).unwrap(), 16..16);
        for (p, n) in [
            (17, 0),
            (15, 2),
            (u64::MAX, 1),
            (0x100000008, 8),
            (8, usize::MAX),
        ] {
            assert!(range(p, n, 16).is_err());
        }
    }
}
