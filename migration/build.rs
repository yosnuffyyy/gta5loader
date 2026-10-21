fn main() {
    println!("cargo:rerun-if-env-changed=MIGRATION_AOT");
    println!("cargo:rerun-if-env-changed=MIGRATION_AOT_SHA256");
    if std::env::var("CARGO_CFG_TARGET_OS").as_deref() == Ok("ios")
        && std::env::var_os("CARGO_FEATURE_EMBEDDED_CHECK").is_some()
    {
        assert!(
            std::env::var_os("CARGO_FEATURE_COMPILER").is_none(),
            "iOS executable must not include a compiler"
        );
        let path =
            std::env::var("MIGRATION_AOT").expect("trusted build-time ARM64 artifact required");
        assert!(std::path::Path::new(&path).is_file());
        let hash = std::env::var("MIGRATION_AOT_SHA256").expect("trusted build digest required");
        assert!(hash.len() == 64 && hash.bytes().all(|b| b.is_ascii_hexdigit()));
        println!("cargo:rerun-if-changed={path}");
        println!("cargo:rustc-link-arg-bin=embedded-aot-check=-Wl,-segprot,__GAME,rx,rx");
    }
}
