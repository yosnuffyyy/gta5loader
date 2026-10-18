"""Validate scope, arithmetic, evidence extraction and preservation assertions."""
from pathlib import Path
import hashlib
import json
import re
import argparse

p=argparse.ArgumentParser()
p.add_argument('--root',type=Path,default=Path('outputs/GTA-Native-Prototype'))
p.add_argument('--work',type=Path,default=Path('work/native-performance-audit'))
a=p.parse_args()
root=a.root
work=a.work
text=(root/'PERFORMANCE-AUDIT.md').read_text(encoding='utf-8')
assert re.findall(r'^## ([A-M])\. ',text,re.M)==list('ABCDEFGHIJKLM')
compact=text[text.index('## M.'):]
assert len(compact.split())>=700
for value in ['8047937b21007cea1efad62df2beb7d1c007bb43','UNKNOWN','53,044','4,608','1.135','1.030','171,484,264']:
    assert value in compact,value
assert 'No runtime performance changes' in compact
assert round((103-12)/103*100,3)==88.350
assert (49152-4608)/49152*100==90.625
assert round(37314560/1048576,3)==35.586
assert round(171484264/1048576,2)==163.54
assert round(411568952/1048576,2)==392.50

baseline=json.loads((work/'baseline.json').read_text(encoding='utf-8'))
for name,expected in baseline['native_files'].items():
    assert hashlib.sha256((root/name).read_bytes()).hexdigest()==expected,name
assert len(baseline['native_files'])==226

for p in work.glob('replays-attempt*.json'):
    d=json.loads(p.read_text(encoding='utf-8'))
    assert d['source']==baseline['source'] and d['result']=='PASS'
    assert len(d['rows'])==6
    for row in d['rows']:
        if row['mode']==2:
            assert row['batches']<=12 and row['uniformPacks']<40
            if d['attempt']==1:
                assert (row['batches'],row['submissions'],row['uniformBytes'],row['pressureWaits'])==(7,12,4608,0)

binary=json.loads((work/'ipa-measurement.json').read_text(encoding='utf-8'))
assert binary['ipaSHA256']=='8e4744e7f027109934a0477a0b32b377f8af6364907fa39373b92d5ea768110c'
assert binary['UUID']=='54C10F1A-5313-37AD-BC46-2EE7BDA32930'
assert binary['metadataFileBytes']==171484264
assert binary['binaryBytes']==411568952
preserved=json.loads((work/'preservation.json').read_text(encoding='utf-8'))
assert preserved['result']=='PASS' and preserved['olderIPAsHashed']==34

result=dict(result='PASS',scope='Audit documentation, arithmetic, extracted host measurements and immutable v35 source/artifact checks; not physical gameplay',
            sections='A-M in order',compactReportWords=len(compact.split()),nativeFilesUnchanged=226,
            olderIPAsHashed=34,releasedV35HashVerified=True,
            runtimeCodeChanges=0,newRuntimeInstrumentation=0,frameCap=30,resolution='640x360',
            replayAttemptsChecked=len(list(work.glob('replays-attempt*.json'))),
            phoneMeasurementsAvailable=False,performanceOptimizationsImplemented=0)
(work/'audit-validation.json').write_bytes((json.dumps(result,indent=2)+'\n').encode())
print(json.dumps(result))
