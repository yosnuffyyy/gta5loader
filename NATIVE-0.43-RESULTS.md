# Native 0.43 — candidate under verification

Baseline executable ab51b47752066092280136f8327a8543b8a612ad, documentation tree f2f0eacb25f1270c02d7a7fdbf8d16f172dfcd2e. No newer native branch changes found. Full physical log examined; its final trap is the unimplemented single-sample A2C path, using grass VS90bb870460e2835c / PS0030c87264713865. Vehicle impact is the observed trigger; this is not evidence of a physics failure.

Implementation: final-entry-result alpha coverage specialization for float4 Target0, threshold >=0.5 matching pinned wgpu_worker.js, with D3D NaN=zero behavior; native multisample A2C retained and explicit output sample_mask overrides A2C. D3D intermediate quantization is implementation-dependent. No original shader body or texture is replaced. Pipeline key already includes blend A2C and sample count.

Pass-local exact state reuse: pipeline, bind groups including dynamic offsets, vertex/index ranges, viewport, scissor, blend constant and stencil reference. Cached handles are retained against pointer reuse, released at replacement/pass end; fixed metadata, no extra GPU resources or frames. Validation, hazard tracking and draw ordering preserved.

New regressions: original fatal pair with synthetic deterministic geometry/material and four MRTs, alpha endpoints/thresholds and 1x/4x; protocol alpha/depth/occlusion cases including NaN; paired original-shader pixel/query replays comparing identical workloads with reuse off/on. CI results, benchmarks and IPA identity will be filled only after validation. Existing Sandbox/Story publication/save roots, renderers, presets, controls, PCM and renderer compatibility remain unchanged.

Physical validation pending: repeat rear-end/head-on/multiple vehicle collisions and glass/debris, driving through matching Los Santos route, retained saves/modes, audio underruns, input, thermal/memory over long sessions. No sustained FPS or crash-free gameplay claim.

Reference: https://learn.microsoft.com/en-us/windows/win32/direct3d11/d3d10-graphics-programming-guide-blend-state#alpha-to-coverage ; pinned work/minimal-engine/wgpu_worker.js a2cModule and pipelineDesc.
