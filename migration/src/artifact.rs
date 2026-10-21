//! Native module bytes are executable code, not an ordinary game asset.
use anyhow::{ensure, Result};
use sha2::{Digest, Sha256};
use wasmtime::{Engine, Module};

pub fn digest(bytes: &[u8]) -> String {
    format!("{:x}", Sha256::digest(bytes))
}

/// An expectation must originate in trusted build metadata compiled into the
/// host, never from a manifest supplied alongside user-selected game files.
pub struct ExpectedArtifact<'a> {
    pub sha256: &'a str,
    pub bytes: usize,
    pub engine_sha256: &'a str,
    pub config: &'a str,
}
impl ExpectedArtifact<'_> {
    pub fn verify(&self, bytes: &[u8]) -> Result<()> {
        ensure!(
            self.engine_sha256 == crate::runtime::ENGINE_SHA256,
            "wrong original engine"
        );
        ensure!(
            self.config == crate::runtime::CONFIG_ID,
            "wrong runtime configuration"
        );
        ensure!(bytes.len() == self.bytes, "native artifact length mismatch");
        ensure!(
            digest(bytes) == self.sha256,
            "native artifact digest mismatch"
        );
        Ok(())
    }
}

/// # Safety
/// The expected digest must be trusted, and identify bytes produced by this
/// exact Wasmtime configuration/version for the executing target. A matching
/// hash from an untrusted manifest is not sufficient authorization to execute.
/// iOS requires a separate signed, embedded executable-section loader; this
/// host-only loader must never be used to mmap executable code on iOS.
#[cfg(not(target_os = "ios"))]
pub unsafe fn load_host(
    engine: &Engine,
    bytes: &[u8],
    expected: &ExpectedArtifact<'_>,
) -> Result<Module> {
    expected.verify(bytes)?;
    Ok(unsafe { Module::deserialize(engine, bytes)? })
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn rejects_changed_code_and_configuration_before_deserialize() {
        let bytes = b"not executable; verification-only test";
        let hash = digest(bytes);
        let mut e = ExpectedArtifact {
            sha256: &hash,
            bytes: bytes.len(),
            engine_sha256: crate::runtime::ENGINE_SHA256,
            config: crate::runtime::CONFIG_ID,
        };
        assert!(e.verify(bytes).is_ok());
        assert!(e.verify(b"changed").is_err());
        let mut corrupt = bytes.to_vec();
        corrupt[3] ^= 1;
        assert!(e.verify(&corrupt).is_err());
        e.config = "other";
        assert!(e.verify(bytes).is_err());
        e.config = crate::runtime::CONFIG_ID;
        e.engine_sha256 = "other";
        assert!(e.verify(bytes).is_err());
    }
}
