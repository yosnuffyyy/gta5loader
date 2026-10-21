//! Isolated migration components. Not connected to the production iOS launcher.
//! A default build contains the Wasmtime runtime, but no JIT compiler.
pub mod artifact;
pub mod assets;
pub mod runtime;
