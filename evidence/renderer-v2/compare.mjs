// Parse only explicit machine records, never repeated diagnostic summaries.
// Usage: node evidence/renderer-v2/compare.mjs evidence/native-engine-test.txt output.json
import fs from 'node:fs';
import assert from 'node:assert/strict';
const [input,output]=process.argv.slice(2);
assert(input&&output,'Supply the complete two-process test log and JSON destination');
const text=fs.readFileSync(input,'utf8'),rows=[];let processId=0;
for(const line of text.split(/\r?\n/)){
 const start=line.match(/^PROCESS (\d+) EXECUTION (EXISTING|V2)$/);if(start){processId=Number(start[1]);continue;}
 if(line.startsWith('V2_BENCH '))rows.push({...JSON.parse(line.slice(9)),process:processId});
}
assert.equal(rows.length,32,'Expected two independent processes, two profile settings, four alternating pairs');
for(const r of rows){assert(r.ok&&r.draws===64);assert(['existing','v2'].includes(r.backend));assert(r.pending_peak<=128);assert(r.registrations>0);assert(r.command_cpu_ms>=0||!r.profile);assert(!r.profile||(r.service_cpu_ms>=0&&r.service_cpu_errors===0));}
const median=a=>{a=[...a].sort((a,b)=>a-b);return a.length%2?a[(a.length-1)/2]:(a[a.length/2-1]+a[a.length/2])/2;};
const summary=[];
for(const profile of [false,true]){
 const pairs=[];
 for(const process of [1,2])for(let pair=0;pair<4;pair++){
  const a=rows.filter(x=>x.process===process&&x.pair===pair&&x.profile===profile);
  assert.equal(a.length,2);const old=a.find(x=>x.backend==='existing'),v2=a.find(x=>x.backend==='v2');assert(old&&v2);
  assert.equal(old.order,pair%2);assert.equal(v2.order,1-pair%2);
  // V2 may coalesce bounded logical fences. The isolated-write
  // regression is deliberately separate from this matched pixel workload.
  assert(v2.submissions<=old.submissions,'V2 increased submissions in matched replay');
  pairs.push({process,pair,existing:old,v2,wall_change_pct:(v2.wall_ms/old.wall_ms-1)*100,service_cpu_change_pct:profile?(v2.service_cpu_ms/old.service_cpu_ms-1)*100:null,command_cpu_change_pct:profile?(v2.command_cpu_ms/old.command_cpu_ms-1)*100:null,end_cpu_change_pct:profile?(v2.end_cpu_ms/old.end_cpu_ms-1)*100:null});
 }
 const metrics=['wall_ms','registrations','submissions','query_p50_ms','query_p95_ms','query_max_ms',...(profile?['command_cpu_ms','end_cpu_ms','service_cpu_ms']:[])];
 summary.push({profile,pairs:pairs.length,metrics:Object.fromEntries(metrics.map(k=>[k,{existing_median:median(pairs.map(x=>x.existing[k])),v2_median:median(pairs.map(x=>x.v2[k]))}])),paired_median_wall_change_pct:median(pairs.map(x=>x.wall_change_pct)),paired_median_service_cpu_change_pct:profile?median(pairs.map(x=>x.service_cpu_change_pct)):null,paired_median_command_cpu_change_pct:profile?median(pairs.map(x=>x.command_cpu_change_pct)):null,paired_median_end_cpu_change_pct:profile?median(pairs.map(x=>x.end_cpu_change_pct)):null,pair_details:pairs});
}
fs.writeFileSync(output,JSON.stringify({result:'PASS',provenance:'Synthetic contract replay on macOS Metal; original startup shaders, 64 draws, 513 event ENDs and 256 pixel comparisons per run. Not a phone command capture or gameplay FPS.',workload:'Identical six-events-per-draw sequence; new service/device objects each replay, warmed process/driver caches may persist. Four order-alternated pairs per profile setting per independent process. No outlier removal.',limits:'CPU command/op46 timings are inclusive and overlap; deferred drain CPU can move outside opcode attribution. Service-loop CPU measures the whole service-thread iteration, including submission/polling/wake CPU, excluding initialization/cleanup and separately reported from command CPU. Query latency is END handler entry to publication, latest256 quantiles/lifetime maximum; not exclusive GPU duration. Profiling-off CPU values are unknown. Initialization/cleanup outside replay wall timer. No genuine GPU timestamps, phone thermal or driver memory measurement.',summary,raw:rows},null,2)+'\n');
console.log('PASS: 32 matched completion replays parsed; correctness and non-increasing-submission checks passed. Host evidence only.');
