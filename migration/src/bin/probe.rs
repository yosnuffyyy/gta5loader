//! Host-only compatibility gate. Unexpected imports trap; no fake engine startup.
use anyhow::{bail, ensure, Context, Result};
use gta_muguet_runtime::{
    artifact::{digest, ExpectedArtifact},
    runtime::*,
};
use serde_json::json;
use std::{path::Path, time::Instant};
use wasmtime::*;

struct Host {
    memory: SharedMemory,
    mode: i32,
    published: bool,
    output: String,
    clock: f64,
}
// These helpers are only used by the single-threaded original-engine fixture.
// No live workers, callbacks, slices or aliases access the memory concurrently.
fn read(mem: &SharedMemory, at: u64, n: usize) -> Result<Vec<u8>> {
    let data = mem.data();
    let r = range(at, n, data.len())?;
    Ok(unsafe { std::slice::from_raw_parts(data.as_ptr().cast::<u8>().add(r.start), n) }.to_vec())
}
fn write(mem: &SharedMemory, at: u64, bytes: &[u8]) -> Result<()> {
    let data = mem.data();
    let r = range(at, bytes.len(), data.len())?;
    unsafe {
        std::ptr::copy_nonoverlapping(
            bytes.as_ptr(),
            data.as_ptr().cast::<u8>().cast_mut().add(r.start),
            bytes.len(),
        );
    }
    Ok(())
}
fn u64_at(mem: &SharedMemory, at: u64) -> Result<u64> {
    Ok(u64::from_le_bytes(read(mem, at, 8)?.try_into().unwrap()))
}
fn u32_at(mem: &SharedMemory, at: u64) -> Result<u32> {
    Ok(u32::from_le_bytes(read(mem, at, 4)?.try_into().unwrap()))
}
fn arg(p: &[Val], i: usize) -> Result<u64> {
    match p.get(i) {
        Some(Val::I64(v)) => Ok(*v as u64),
        Some(Val::I32(v)) => Ok(*v as u32 as u64),
        _ => bail!("invalid integer argument"),
    }
}
fn instantiate(engine: &Engine, module: &Module, mode: i32) -> Result<(Store<Host>, Instance)> {
    let memory = SharedMemory::new(
        engine,
        MemoryType::shared(INITIAL_PAGES as u32, MAX_PAGES as u32),
    )?;
    let mut store = Store::new(
        engine,
        Host {
            memory: memory.clone(),
            mode,
            published: false,
            output: String::new(),
            clock: 0.0,
        },
    );
    let mut linker = Linker::new(engine);
    for imp in module.imports() {
        match imp.ty() {
            ExternType::Memory(ty) => {
                ensure!(
                    imp.module() == "env"
                        && imp.name() == "memory"
                        && ty.is_shared()
                        && !ty.is_64()
                        && ty.minimum() == INITIAL_PAGES
                        && ty.maximum() == Some(MAX_PAGES),
                    "memory ABI changed"
                );
                linker.define(&store, imp.module(), imp.name(), memory.clone())?;
            }
            ExternType::Func(ty) => {
                let name = imp.name().to_owned();
                linker.func_new(
                    imp.module(),
                    imp.name(),
                    ty,
                    move |mut caller: Caller<'_, Host>, params, results| {
                        let mut run = || -> Result<()> {
                            match name.as_str() {
                                "fd_write" => {
                                    ensure!(
                                        arg(params, 0)? == 1 || arg(params, 0)? == 2,
                                        "fixture accepts log streams only"
                                    );
                                    let n = arg(params, 2)?;
                                    ensure!(n <= 32, "iov count");
                                    let base = arg(params, 1)?;
                                    let mut total = 0;
                                    for i in 0..n {
                                        let at =
                                            base.checked_add(i * 16).context("iov overflow")?;
                                        let p = u64_at(&caller.data().memory, at)?;
                                        let len = u64_at(&caller.data().memory, at + 8)?;
                                        ensure!(len <= 4096, "log too large");
                                        let s = read(&caller.data().memory, p, len as usize)?;
                                        ensure!(
                                            caller.data().output.len() + s.len() <= 16384,
                                            "log fixture bound"
                                        );
                                        caller.data_mut().output.push_str(std::str::from_utf8(&s)?);
                                        total += len;
                                    }
                                    write(
                                        &caller.data().memory,
                                        arg(params, 3)?,
                                        &total.to_le_bytes(),
                                    )?;
                                    results[0] = Val::I32(0);
                                    Ok(())
                                }
                                "wasm_input_publish_js" => {
                                    let base = arg(params, 0)?;
                                    ensure!(base == 6583436, "input ABI changed");
                                    let mem = &caller.data().memory;
                                    for (off, v) in [
                                        (280, 1i32),
                                        (432, 640),
                                        (436, 360),
                                        (440, caller.data().mode),
                                    ] {
                                        write(mem, base + off, &v.to_le_bytes())?;
                                    }
                                    caller.data_mut().published = true;
                                    bail!("MIGRATION_PUBLICATION_BOUNDARY")
                                }
                                "wasm_now_ms" => {
                                    results[0] = Val::F64(caller.data().clock.to_bits());
                                    Ok(())
                                }
                                _ => bail!("unimplemented host import {name}"),
                            }
                        };
                        run().map_err(wasmtime::Error::from_anyhow)
                    },
                )?;
            }
            _ => bail!("unsupported import kind"),
        }
    }
    let inst = linker.instantiate(&mut store, module)?;
    inst.get_typed_func::<(), ()>(&mut store, "migration_init")?
        .call(&mut store, ())?;
    inst.get_global(&mut store, "migration_stack")
        .context("stack export")?
        .set(&mut store, Val::I64(96 * 1024 * 1024))?;
    Ok((store, inst))
}

fn original_mode(engine: &Engine, module: &Module, mode: i32) -> Result<serde_json::Value> {
    let (mut s, i) = instantiate(engine, module, mode)?;
    let mem = s.data().memory.clone();
    // Existing original startup fixture: disable GPU construction, and stop at
    // input publication. This does NOT execute full constructors/world startup.
    write(&mem, 8348848, &(-1i32).to_le_bytes())?;
    let args = if mode == 1 {
        vec!["native-probe", "-nonetwork", "-noautoload"]
    } else {
        vec![
            "native-probe",
            "-nonetwork",
            "-playercoords=94.95,-742.96,44.75",
        ]
    };
    let mut pos = 101000u64;
    for (n, a) in args.iter().enumerate() {
        write(&mem, 100000 + n as u64 * 8, &pos.to_le_bytes())?;
        let text = format!("{a}\0");
        write(&mem, pos, text.as_bytes())?;
        pos += text.len() as u64;
    }
    write(&mem, 100000 + args.len() as u64 * 8, &0u64.to_le_bytes())?;
    let error = i
        .get_typed_func::<(i32, i64), i32>(&mut s, "migration_main")?
        .call(&mut s, (args.len() as i32, 100000))
        .expect_err("main must stop at explicit fixture boundary");
    ensure!(
        s.data().published && format!("{error:?}").contains("MIGRATION_PUBLICATION_BOUNDARY"),
        "unexpected main trap: {error:?}"
    );
    for name in ["mode_now", "choice", "mode"] {
        ensure!(
            i.get_typed_func::<(), i32>(&mut s, &format!("migration_{name}"))?
                .call(&mut s, ())?
                == mode,
            "wrong chosen mode"
        );
    }
    let flag = i.get_typed_func::<i64, ()>(&mut s, "migration_script_flag")?;
    for (name, expected) in [
        ("DoReleaseStartup", i32::from(mode == 1)),
        ("WasmSandbox", i32::from(mode == 2)),
    ] {
        write(&mem, 110000, &111000u64.to_le_bytes())?;
        write(&mem, 110016, &112000u64.to_le_bytes())?;
        write(&mem, 112000, &113000u64.to_le_bytes())?;
        write(&mem, 113000, format!("{name}\0").as_bytes())?;
        write(&mem, 111000, &(-1i32).to_le_bytes())?;
        flag.call(&mut s, 110000)?;
        ensure!(
            u32_at(&mem, 111000)? == expected as u32,
            "wrong original script flag"
        );
    }
    ensure!(
        s.data().output.contains(if mode == 1 {
            "[wasm] start mode: story"
        } else {
            "[wasm] start mode: sandbox"
        }),
        "missing original mode log"
    );
    Ok(
        json!({"mode":mode,"args":args,"originalScriptFlags":true,"publication":true,"worldExecuted":false}),
    )
}

fn engine_checks(engine: &Engine, module: &Module) -> Result<serde_json::Value> {
    let (mut s, i) = instantiate(engine, module, 1)?;
    let mem = s.data().memory.clone();
    let a = 64 * 1024 * 1024u64;
    write(&mem, a, b"Alpha\0")?;
    write(&mem, a + 1024, b"Alpha\0")?;
    ensure!(
        i.get_typed_func::<i64, i64>(&mut s, "migration_strlen")?
            .call(&mut s, a as i64)?
            == 5,
        "strlen"
    );
    ensure!(
        i.get_typed_func::<(i64, i64), i32>(&mut s, "migration_strcmp")?
            .call(&mut s, (a as i64, a as i64 + 1024))?
            == 0,
        "strcmp"
    );
    write(&mem, a + 1028, b"b")?;
    ensure!(
        i.get_typed_func::<(i64, i64, i64), i32>(&mut s, "migration_memcmp")?
            .call(&mut s, (a as i64, a as i64 + 1024, 6))?
            < 0,
        "memcmp"
    );
    let sin = i.get_typed_func::<f32, f32>(&mut s, "migration_sinf")?;
    let cos = i.get_typed_func::<f32, f32>(&mut s, "migration_cosf")?;
    let mut trig = Vec::new();
    for x in [0.0f32, 0.25, -0.5, 1.0, 3.25, 100.0] {
        trig.push(json!({"x":x,"sin":sin.call(&mut s,x)?,"cos":cos.call(&mut s,x)?}));
    }
    write(&mem, 23074832, &0u32.to_le_bytes())?;
    for k in 0..9u64 {
        write(&mem, a + k * 1024 + 100, &256u32.to_le_bytes())?;
        write(&mem, a + k * 1024 + 136, &(0x123400 + k).to_le_bytes())?;
        if k < 8 {
            write(
                &mem,
                23076016 + k * 304 + 272,
                &(0x123400 + k).to_le_bytes(),
            )?;
        }
    }
    let f = i.get_typed_func::<(i64, i32), i32>(&mut s, "migration_shadow")?;
    let mut checksum = 0;
    let start = Instant::now();
    for k in 0..200000 {
        let slot = k & 7;
        let got = f.call(&mut s, ((a + slot * 1024) as i64, 0))?;
        ensure!(got == slot as i32, "shadow slot");
        checksum += got;
        ensure!(
            f.call(&mut s, ((a + 8 * 1024) as i64, 0))? == -1,
            "shadow absent"
        );
    }
    let wall = start.elapsed().as_secs_f64() * 1000.0;
    ensure!(checksum == 700000, "checksum");
    let before = mem.data().as_ptr() as usize;
    let old = mem.grow(1)?;
    ensure!(old == INITIAL_PAGES, "initial pages");
    ensure!(mem.data().as_ptr() as usize == before, "shared base moved");
    ensure!(
        mem.grow(MAX_PAGES).is_err(),
        "over-cap memory growth accepted"
    );
    Ok(
        json!({"strings":true,"trig":trig,"shadowCalls":400000,"checksum":checksum,"hostBoundaryInclusiveWallMs":wall,"sharedGrowthBaseStable":true,"overLimitRejected":true}),
    )
}

fn shared_atomic_checks(engine: &Engine) -> Result<serde_json::Value> {
    // Separate small standard-Wasm fixture; not claimed as engine workers.
    let m = Module::new(
        engine,
        r#"(module (import "env" "memory" (memory 1 2 shared))
     (func (export "add") (result i32) i32.const 0 i32.const 1 i32.atomic.rmw.add)
     (func (export "wait") (result i32) i32.const 0 i32.const 80000 i64.const 1000000 memory.atomic.wait32)
     (func (export "wake_wait") (result i32) i32.const 4 i32.const 0 i64.const 5000000000 memory.atomic.wait32)
     (func (export "notify") (result i32) i32.const 4 i32.const 1 memory.atomic.notify)
     (func (export "bad") (result i32) i32.const 65536 i32.atomic.load))"#,
    )?;
    let mem = SharedMemory::new(engine, MemoryType::shared(1, 2))?;
    let mut handles = Vec::new();
    for _ in 0..4 {
        let e = engine.clone();
        let m = m.clone();
        let mem = mem.clone();
        handles.push(std::thread::spawn(move || -> Result<()> {
            let mut s = Store::new(&e, ());
            let i = Instance::new(&mut s, &m, &[mem.into()])?;
            let f = i.get_typed_func::<(), i32>(&mut s, "add")?;
            for _ in 0..20000 {
                f.call(&mut s, ())?;
            }
            Ok(())
        }));
    }
    for h in handles {
        h.join().map_err(|_| anyhow::anyhow!("worker panic"))??;
    }
    ensure!(u32_at(&mem, 0)? == 80000, "lost atomic updates");
    let mut s = Store::new(engine, ());
    let i = Instance::new(&mut s, &m, &[mem.clone().into()])?;
    ensure!(
        i.get_typed_func::<(), i32>(&mut s, "wait")?
            .call(&mut s, ())?
            == 2,
        "atomic wait timeout"
    );
    ensure!(
        i.get_typed_func::<(), i32>(&mut s, "bad")?
            .call(&mut s, ())
            .is_err(),
        "missing bounds trap"
    );
    let e = engine.clone();
    let waiter = std::thread::spawn(move || -> Result<i32> {
        let mut s = Store::new(&e, ());
        let i = Instance::new(&mut s, &m, &[mem.into()])?;
        Ok(i.get_typed_func::<(), i32>(&mut s, "wake_wait")?
            .call(&mut s, ())?)
    });
    let notify = i.get_typed_func::<(), i32>(&mut s, "notify")?;
    let deadline = Instant::now() + std::time::Duration::from_secs(3);
    let mut awakened = false;
    while Instant::now() < deadline {
        if notify.call(&mut s, ())? == 1 {
            awakened = true;
            break;
        }
        // Fixture-only bounded rendezvous; never a production worker loop.
        std::thread::sleep(std::time::Duration::from_millis(1));
    }
    let result = waiter
        .join()
        .map_err(|_| anyhow::anyhow!("waiter panic"))??;
    ensure!(
        awakened && result == 0,
        "atomic notify did not wake blocked Wasm worker"
    );
    Ok(
        json!({"workers":4,"increments":80000,"waitTimeout":true,"waitNotify":true,"boundsTrap":true}),
    )
}

fn main() -> Result<()> {
    let args: Vec<String> = std::env::args().collect();
    ensure!(
        args.len() == 4,
        "usage: migration-probe host|ios engine-probe.wasm output-dir"
    );
    let bytes = std::fs::read(&args[2])?;
    ensure!(digest(&bytes) == PROBE_SHA256, "probe digest mismatch");
    let out = Path::new(&args[3]);
    std::fs::create_dir_all(out)?;
    let mut c = config();
    if args[1] == "ios" {
        c.target("aarch64-apple-ios")?;
    } else {
        ensure!(args[1] == "host", "unknown target");
    }
    let engine = Engine::new(&c)?;
    let start = Instant::now();
    let artifact = engine.precompile_module(&bytes)?;
    let elapsed = start.elapsed().as_secs_f64();
    let hash = digest(&artifact);
    std::fs::write(out.join(format!("{}.cwasm", args[1])), &artifact)?;
    let mut report = json!({"result":"PASS","target":args[1],"wasmtime":"49.0.2","config":CONFIG_ID,"engineSHA256":ENGINE_SHA256,"probeSHA256":PROBE_SHA256,"artifactSHA256":hash,"artifactBytes":artifact.len(),"compileWallSeconds":elapsed,"gameplayVerified":false,"iOSExecutionVerified":false});
    if args[1] == "host" {
        let expected = ExpectedArtifact {
            sha256: &hash,
            bytes: artifact.len(),
            engine_sha256: ENGINE_SHA256,
            config: CONFIG_ID,
        };
        // Trusted bytes compiled immediately above, not a user-supplied cache.
        let module =
            unsafe { gta_muguet_runtime::artifact::load_host(&engine, &artifact, &expected) }?;
        report["engineChecks"] = engine_checks(&engine, &module)?;
        let mut modes = Vec::new();
        for mode in [1, 2, 2, 1] {
            modes.push(original_mode(&engine, &module, mode)?);
        }
        report["startupChecks"] = json!(modes);
        report["sharedAtomicFixture"] = shared_atomic_checks(&engine)?;
    }
    std::fs::write(
        out.join(format!("{}.json", args[1])),
        serde_json::to_vec_pretty(&report)?,
    )?;
    println!("{}", serde_json::to_string(&report)?);
    Ok(())
}
