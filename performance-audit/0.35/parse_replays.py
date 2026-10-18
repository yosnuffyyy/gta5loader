"""Extract existing Native 0.35 counters without changing its workload."""
import argparse
import hashlib
import json
import re
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument('log', type=Path)
p.add_argument('output', type=Path)
p.add_argument('--attempt', type=int, required=True)
a = p.parse_args()
raw = a.log.read_bytes()
lines = raw.decode('utf-8').splitlines()
rows = []
process = 0
for i, line in enumerate(lines):
    match = re.fullmatch(r'PROCESS (\d+)', line)
    if match:
        process = int(match[1])
    if 'PASS: buffer dependency replay legacy' not in line:
        continue
    mode = int(re.search(r'legacy(\d):', line)[1])
    def pair(label):
        return tuple(map(int, re.search(re.escape(label)+r' (\d+)/(\d+)', line).groups()))
    batches, submissions = pair('batches/submissions')
    packs, uniform_bytes = pair('uniform packs/bytes')
    independent, dependent = pair('independent/dependent uploads')
    handler, upload = map(float, re.search(r'Handler/upload wall ([\d.]+)/([\d.]+) ms', line).groups())
    draw = lines[i+1]
    values = list(map(float, re.search(r'mean/median/p95/p99/max ([\d.]+)/([\d.]+)/([\d.]+)/([\d.]+)/([\d.]+)', draw).groups()))
    assert int(re.search(r'latest (\d+) ms', draw)[1]) == 96
    total = float(re.search(r'lifetime 96 calls ([\d.]+) ms', draw)[1])
    rows.append(dict(process=process, mode=mode, draws=96, pixelsChecked=512,
                     querySamples=96, querySegments=96, batches=batches,
                     submissions=submissions, uniformPacks=packs,
                     uniformBytes=uniform_bytes, independent=independent,
                     dependent=dependent, protocolUploadBytes=int(re.search(r'protocol upload payload (\d+) bytes', line)[1]),
                     completions=int(re.search(r'completions (\d+)', line)[1]),
                     pressureWaits=int(re.search(r'pressure waits (\d+)', line)[1]),
                     handlerWallMs=handler, uploadWallMs=upload,
                     drawWallTotalMs=total,
                     drawWallMs=dict(zip(['mean','p50','p95','p99','max'], values))))
assert len(rows) == 6 and {(x['process'],x['mode']) for x in rows} == {(i,m) for i in (1,2) for m in (0,1,2)}
for row in rows:
    assert row['protocolUploadBytes'] == 53044
    if row['mode'] == 2:
        # The production drain policy has a wall-clock bound. Preserve measured
        # variation rather than requiring an old machine's exact batch count.
        assert row['batches'] <= 12 and row['uniformPacks'] < 40
        assert row['independent'] > 80 and row['dependent'] >= 3
        reference=next(r for r in rows if r['process']==row['process'] and r['mode']==0)
        assert row['submissions'] < reference['submissions']
        if a.attempt == 1:
            assert (row['batches'],row['submissions'],row['uniformBytes']) == (7,12,4608)
    assert row['handlerWallMs'] >= row['uploadWallMs']
    assert row['drawWallMs']['p50'] <= row['drawWallMs']['p95'] <= row['drawWallMs']['p99'] <= row['drawWallMs']['max']
working_set = [l for l in lines if 'PASS: pipeline working set policy ' in l]
equivalence = [l for l in lines if 'PASS: pipeline state equivalence ' in l]
retirement = [l for l in lines if 'PASS: ordered readback uniform retirement ' in l]
assert len(working_set) == 6 and len(equivalence) == 4 and len(retirement) == 4
out = dict(result='PASS', source='8047937b21007cea1efad62df2beb7d1c007bb43',
           ciRun=39078684670, attempt=a.attempt, rawSHA256=hashlib.sha256(raw).hexdigest(),
           origin='Original-shader synthetic ring fixtures on macOS Apple Paravirtual Metal, native CLI -O0. Not the iPhone -O1 production path, real gameplay frames or GPU execution duration.',
           modes={'0':'dependency-aware uploads; old drain boundaries','1':'legacy upload and drain flush policy','2':'production v35 policy'},
           rows=rows, pipelineWorkingSetLines=working_set,
           pipelineEquivalenceLines=equivalence, readbackRetirementLines=retirement,
           limitations=['Handler and upload walls overlap; draw wall can contain flushes/waits.',
                        'Fixture has no PRESENT or drawable; no per-frame FPS inference.',
                        'Fixed test order and cold driver state are not controlled A/B timing.',
                        'Only mode 2 is the v35 production policy; reference modes are comparisons.'])
a.output.write_bytes((json.dumps(out, indent=2)+'\n').encode())
print(json.dumps({'result':'PASS','attempt':a.attempt,'rows':len(rows),
                  'production':[{k:r[k] for k in ['process','batches','submissions','uniformBytes','handlerWallMs','uploadWallMs']} for r in rows if r['mode']==2]}))
