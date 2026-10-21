# Project Muguet Rebirth — measurements

Date: 2026-10-21. **No migration gameplay FPS, GPU timing, audio playback, input latency or sustained iPhone result exists yet. No performance improvement is claimed.** See the architecture report for exact engine/source pins and scope.

## Measured checkpoint costs

Initial integrated source `23ddd0c98d66717a05d7a7e078a52a622bb54131`, workflow [39644345448](https://github.com/yosnuffyyy/gta5loader/actions/runs/39644345448), macOS 15 **ARM64 hosted runner**, optimized Rust/Cranelift `Speed`. Each row below is one run, not a robust distribution or a gameplay workload.

| Operation | Result | Measurement origin/limit |
|---|---:|---|
| Compile complete original probe for macOS ARM64 | 20.382275 s wall | Host release compiler, 66,173,163-byte probe |
| Serialized macOS artifact | 160,984,984 bytes | Disk size, not resident RAM |
| Original shadow lookup 400,000 calls | 8.667417 ms wall, checksum 700,000 | Includes one Rust/Wasmtime typed-call boundary per call; 200k hits + 200k misses |
| Compile probe for iOS ARM64 | 25.248421 s wall | Host cross-target AOT, no iPhone execution |
| Embedded iOS artifact | 159,918,840 bytes | Hash `6612e8594e9218f2435e709cf500d4b8bdbd487e80e4d920af5e900c6bbb8170` |
| Rust release tests + compiler build | 3m 28s reported Cargo wall | Cold dependencies; not engine CPU execution time |
| Compiler-free iOS link | 45.29s Cargo wall | Structural Mach-O audit only |
| Guest memory | 512 MiB initial / 4 GiB max | Module limits; reservation/commitment/physical footprint are different |
| Adapter transfer/cache | 64 KiB scratch / 8 archive descriptors | Hard bounds; excludes bounded manifest/index metadata |

The independent Node WebAssembly reference matched the original sin/cos outputs exactly for six inputs. This is correctness evidence, not a comprehensive floating-point equivalence benchmark. Shared-memory fixture: four workers, 80,000 atomic increments, timeout and bounds checks; follow-up adds actual wait/notify wakeup.

Windows x86-64 local **debug** Rust host, same release-optimized Cranelift guest: initial compile 68.602219s, 192,973,408-byte artifact; boundary-inclusive shadow loop 429.2251ms. Follow-up compile 70.666793s, loop472.8852ms, identical artifact SHA `a95f4426bb3cbeb05a9b43692dc183d6933d006b3bbf3c76f9be605338b8bad4`. These debug wrapper/architecture results are not comparable to the ARM64 release row or phone performance. The earlier Python Wasmtime 49.0.0 exploratory compile is excluded from accepted 49.0.2 evidence.

The follow-up ARM64 release component run 39645670173 produced byte-identical host/iOS AOT artifacts. Observed compile wall was21.785551/21.698485s and the shadow loop5.733375ms. This is another uncontrolled hosted observation, not an optimization or statistically established improvement.

Final source 8a1d6819e5e8b51802bb26251b9e24942d98a3a0 / CI 39646902321: host AOT24.828068s, iOS AOT22.667902s; original400k shadow loop7.938625ms wall. Artifacts again byte-identical. Compiler-free linked-RX execution of original sin/cos passed on macOS ARM64. These three observations are retained individually, not presented as robust p95/p99 or a speedup. The final iOS link took16.23s with cached dependencies, not comparable to the initial cold link.

## Baseline comparison limits

Native 0.44's prior generated-C shadow fixture reported O1 median4.072ms CPU/4.110ms wall and O2 median4.076917/4.080500ms for400,000 calls on its hosted workload. The new8.667417ms single observation includes a Wasmtime host boundary on every call; the old loop calls native C directly. Runner instance, sampling, call boundary and compilation differ. **Neither a speedup nor a slowdown of the engine can be deduced from that comparison.** A matched in-Wasm loop/whole subsystem workload and repeated alternating runs are required.

The prior physical0.43 log in the0.44 report showed unclassified last256 display intervals79.825/83.341/100.009/116.677ms mean/p50/p95/p99, handler CPU48.130549ms/PRESENT and service CPU67.863ms/PRESENT with profiling on. It is a different build/workload, not a migration baseline measurement. Native 0.44 itself had no verified physical FPS acceptance result in that report. Upstream Muguet whole-game performance was not measured here.

## Unknown metrics

| Metric | Migration result | Required measurement |
|---|---|---|
| Engine simulation/render CPU | UNKNOWN | Full host, workers and original-world execution; active CPU versus waits |
| Command decode, bindings, uploads, submissions | UNKNOWN | Rust renderer integration + same recorded command workload |
| Actual Metal GPU duration/utilization | UNKNOWN | Supported timestamps/capture on the actual device |
| Displayed p50/p95/p99,30FPS | UNKNOWN | Same marked scene/controller/resolution/preset with real display callbacks |
| Memory footprint/headroom/driver allocations | UNKNOWN | Signed app session, resource census and OS samples; do not add overlapping counters |
| Input-to-hook/display latency | UNKNOWN | Original input bridge integration and physical event/display correlation |
| Audio underruns/latency/speaker sound | UNKNOWN | PCM routing, actual render callbacks and device playback |
| Long-session streaming/thermal behavior | UNKNOWN | Equivalent city driving/mission transitions over20+minutes |
| Full-engine AOT versus wasm2c performance | UNKNOWN | Same subsystem or session, alternating runs, matched settings |

## Simulation timing is not fixed by changing the compiler

The unchanged original timer regression reproduces a66.667ms maximum update delta. Over 10seconds at5/10/15/30 updates per second, summed engine deltas are approximately3.333/6.667/10/10seconds. Integer timers also quantize. This is a known original-engine behavior, not a new migration fix. Arbitrary global time scaling or removing the clamp would risk physics and scripts; no such change is applied. Original animation/cutscene/audio clocks and a safe scheduling design still need validation.

## Performance experiments after compatibility

1. Compare original shadow/allocator/command-production loops entirely inside each runtime, including checksums and matched builds; gather repeated active CPU and wall samples.
2. Run identical graphics fixtures through the old renderer and new Rust backend, with exact pixels/query/copy results and fixed quality. Count commands, actual submissions, draw-visible upload bytes and retirement waits separately.
3. Compare the same marked Sandbox drive and Story sequence at 640x360,30FPS cap, matched renderer settings/input and thermal starting condition. Record loading separately; no duplicate frames as throughput.
4. Only then evaluate validated wasm-opt passes, compiler flags, texture conversion, batching/cache policies and optional quality settings. None is enabled merely because upstream uses it.

## Compact report for independent analysis

Muguet source1b4600b052b864c78fe36180073ff406663b7e0b; our migration checkpoint8a1d6819e5e8b51802bb26251b9e24942d98a3a0; preserved Native 0.44 executablefb7a11e2c0571c5187b98411807226edb7fcc863. Original engine SHAe65bec476e8fd8125515150c24037c9bd0d0c46530e3544f9d389ab108b5592e, wasm32 shared512MiB initial/4GiBmax withi64C ABI pointers. Rust 1.98.1/Wasmtime 49.0.2/Cranelift Speed. Full original module compiles/instantiates and selected original functions run; the startup fixture stops at input publication before complete constructors/world startup. MacARM64 release initial AOT20.382s/160,984,984B;400k boundary-inclusive shadow calls8.667ms checksum 700000. Final iOS AOT22.668s/159,918,840B; compiler-free RX embedding executed original math on macOS, and iOS link audit passed. No iPhone execution. Final400k shadow call observation7.939ms; no matched native-C comparison. No new renderer/audio/input/save integration or playable IPA; production0.44 untouched. No comparable FPS/CPU speedup established. Next: exact session host+TLS/workers, then Rust renderer semantic parity and matched benchmarks. Do not infer30FPS capability from this milestone.
