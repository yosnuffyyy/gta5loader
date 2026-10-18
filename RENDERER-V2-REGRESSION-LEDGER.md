# Renderer V2 regression ledger

Recovered from native branch history, reports0.19–0.39, PERFORMANCE-AUDIT.md, EXTREME-OPTIMIZATION-RESULTS.md, source comments and existing fixtures. Repository history is authoritative; this ledger is a bounded first-milestone reconstruction, not a claim that inaccessible issues or every old device state was recovered.

Baseline19b1c9b / executable68c1394. Existing0.39 Metal CI39152842578 passed. V2 results are tracked separately in RENDERER-V2-RESULTS.md. All rows below require inherited regression reruns after integration; historical passes alone do not establish V2 correctness.

| Defect / evidence class | Cause / fix history | Required invariant and fixture | Physical status / V2 requirement |
|---|---|---|---|
| Clip extension/Metal array emission; EXACT shader records + DEVICE OBSERVATION | 4b36ac5,15b0aca | Negotiated clip support, array bounds; clip_fixture + all4808 module/pipeline audit,500clip | Original crash passed; inherited unchanged |
| Sparse inter-stage location15; EXACT shader pair | 35eba44,21d969b | Linked types/interpolation/builtins preserved under15limit; inter_stage + recorded_pair_audit | Earlier iPhone progressed; rerun |
| SRV709/source708 cube-vs2D, both PS996bb…/6e961…; EXACT descriptors/shaders, REDUCED constants | 847c103,55131bc,6943481 | Certified inactive specialization; opt-in verified reference empty-shadow behavior across light counts; shadow_semantic/transition fixtures | Device progression reported; visual shadow fidelity still unverified; preserve real cubes and strict unrelated errors |
| SRV5633five-mipBC3; EXACT descriptor/shader, REDUCED repro | array_srv_audit/srv_contract; history in ENGINEERING0.25/0.26 | 2D singleton array view is legal only with matching declared sample type/format/range; replacement generations | Prior builds progressed; rerun |
| Depth-as-color and sentinel subresource ranges; REDUCED REPRODUCER | ee8dc86, reports0.21 | depth_proxy_layout, proxy_range, depth_proxy_verify; no silent clamp, refresh after mutation | Prior device passed earlier point; retain |
| SNORM/norm16/BC/comparison-border; SYNTHETIC CONTRACT TEST | d0face1,4e0ef39,e17ae8a | snorm/norm16/border/bc fixtures; signed channel and exact border contracts | A18 native BC tested; iPhone13 unsupported path not validated |
| Options COPY_REGION crash; DEVICE OBSERVATION (incomplete14words) | 9881973,3371a5e,c24f74f | texture_copy + copy_cast16pairs; raw bitcasts, smallmips,arraybounds, tile reuse; copy!=resolve | 0.39 log normal stop; exact original descriptor remains unknown |
| Fatal descriptor publication race; SYNTHETIC CONTRACT TEST | 68c1394 | Full14words and descriptors immutable before release-storefailed; copy_error_fixture | No race claimed measured on phone |
| Occlusion Begin unimplemented; EXACT opcode45 capture + REDUCED replay | ENGINEERING0.20, native_queries | Exact counts, split passes/overlapping scopes, generation tokens, map cancellation; query_fixture/query_stop/test_query_state | Prior iPhone scenes progressed; V2 must leave counts intact |
| Event replies blocked CPU; SYNTHETIC replay + DEVICE counters | 3f9c5d5 | Async actual completion, latest token/ID generation, no fabricated ready; event_fixture | 0.39 597424requests,576405results,21019stale; NEW V2 fan-out target |
| UAV counters/compute; REDUCED REPRODUCER | c92cf0f | Initial-count preserve/reset, append/counter copy, clear/storage views; uav/compute/mip fixtures | Previously working; unchanged |
| Instances/base offsets and byte-edge uploads; SYNTHETIC CONTRACT TEST | 8781595,35eba44 | Divisors/baseinstance/index/indirect checks; instance/buffer_upload/resource fixtures | Preserve bytes, neverexpandblindly |
| Mutation batching WAR/WAW/UAV hazards; SYNTHETIC CONTRACT TEST | a650683,4921699,360bb27,c24f74f | Resource dependency epochs; ordered_fixture,transfer_fixture,resource_verify; reuse only after ordered submission | Prior0.35–0.39 wins retained, notnewV2credit |
| Uniform duplicate packs and unsafe zero-tail reuse; SYNTHETIC CONTRACT TEST | 485d213,851adcc | Complete draw-visible prefix+zero-tail equality, batch-local immutable slices; reuse_fixture | Preserve |
| Readback map/retirement/lifetime; SYNTHETIC CONTRACT TEST | 68231cb,47f699c,d692a54 | Exact raw bytes, ownership, bounded staging, no premature retire; readback36cases | Preserve |
| Funeral~35.6MiBallocation/NULLarchive targets; DEVICE OBSERVATION + original allocator REDUCED repro | 6e91a21,e14d853,c446f7b,cc21e63 | Original actualfree distinctlogicalheadroom; safe buddy growth/NULLpreflight; allocator/memory/asset tests | Later StoryMode progressed; longsession memory still device-dependent |
|0.36app closure/pressure/cache churn; DEVICE OBSERVATION |3f9c5d5,f297da8|Hot retention+exact fingerprint equality, bounded autorelease/recovery; pipeline_policy/stability/recovery |Jetsam plausible NOTconfirmed;0.39 endeduserstop |
|180secdeadline duringlive game; DEVICE OBSERVATION + controlledclock tests |47f699c,9168930,761ae3d,f116cae|Postcamera presentation+engine/IO liveness, genuine stalls and cancellation;1374watchdogchecks |Prior prolongedsession reported; preserve |
|Mailbox/proxy/stop deadlock and drawable wait; REDUCED REPRODUCER |b1855a8,8cfb438|Owner delivery/cancellationunwind, bounded drawable; threading/session/gpu_ring |Preserve |
|Input holds/disconnect/reconnect/hiddentouch detector; SYNTHETIC UIKit/controller tests + DEVICE movement |0652357,77cf215,ac9b464,911742c|Digitaledges, sourcequarantine, persistenttouchsurface,fullscreenaspect; input/test_ui |Devicecontrolsreported; no new latencyclaim |
|Unscrollable menu andUI/lifecycle; DEVICE OBSERVATION + UIKit |8d08a98,c7ec3a3,3153373|Filesreachable, safemodalpause/resume,mainchrome,exactviewport |Preserve; backendselection mustfreeze per session |
|Truncatedlogs/staleerrors/sharing; SYNTHETIC CONTRACT TEST |a6b9884,84cba4d,62d8c49,68c1394|BoundedRAM journal/fulltextdisk, exportconsistentprefix, fatalfirstpublication; journal/profile/UI |0.39log12.4MB no unrecordedbytes; preserve |
|Silent/discardaudio; original PCM REDUCED + hardwareoutput SYNTHETIC |b4a7745,d81938b,3371a5e|48kPCM exactstereo, boundedSPSC/interruption; audio tests |Audibility/quality remainsdevicecheck;0.39 underrun4022016frames, noinvalidPCM |
|Slowmotion maxdelta; original engine REDUCED fixture + DEVICE observations |0.39timing/test-timer.cjs; NOTfixed |10s@10Hz→6.667game; futurefixneedsphysics/scriptorderingtests |Open defect, notcompatibilitycontract |
|Missingbanks/parserclasses; DEVICE OBSERVATION |reports0.16onward, audioinvestigation |Neverfabricateassets/classes; missingLOW_LATENCY.awc remains explicit |Open; rendererwork doesnotresolve |

New V2 tests must distinguish synthetic contract workloads from captured phone commands. Counters are not command captures. No raw game assets or full copyrighted shader captures are added by this ledger.
