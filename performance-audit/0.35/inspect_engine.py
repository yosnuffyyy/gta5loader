"""Read-only symbol and artifact inspection for the Native 0.35 audit."""
from pathlib import Path
import hashlib
import json
import re
import argparse

p = argparse.ArgumentParser()
p.add_argument('--engine', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
ENGINE = a.engine
EXPECTED = 'e65bec476e8fd8125515150c24037c9bd0d0c46530e3544f9d389ab108b5592e'
b = ENGINE.read_bytes()
assert hashlib.sha256(b).hexdigest() == EXPECTED

def leb(at):
    value, shift = 0, 0
    while True:
        c = b[at]
        at += 1
        value |= (c & 127) << shift
        if not c & 128:
            return value, at
        shift += 7
        assert shift < 70

def string(at):
    n, at = leb(at)
    return b[at:at+n].decode('utf-8'), at+n

names = {}
at = 8
while at < len(b):
    section = b[at]
    length, start = leb(at+1)
    end = start+length
    assert end <= len(b)
    if section == 0:
        name, pos = string(start)
        if name == 'name':
            while pos < end:
                sub = b[pos]
                size, pos = leb(pos+1)
                stop = pos+size
                if sub == 1:
                    count, pos = leb(pos)
                    for _ in range(count):
                        index, pos = leb(pos)
                        name, pos = string(pos)
                        names[index] = name
                pos = stop
    at = end

patterns = {
    'simulation': r'CGame::(Process|Update)|CWorld::Process',
    'visibility': r'CGameScan::KickVisibilityJobs|WaitForAllRasterizersToFinish',
    'animation': r'UpdateAnimation|UpdateSkeleton',
    'physics': r'phSimulator.*(Update|Step)|ProcessCollisions',
    'scripts': r'scrThread::(Run|Update)|scrProgram.*Run',
    'streaming': r'strStreamingLoaderManager::(Update|Flush)|pgRscBuilder::AllocateMap',
    'pad': r'ioPad::Update',
    'timebar': r'TimeBar|Timebar|timebar|FrameProfile',
    'submission': r'grcDevice.*(BeginFrame|EndFrame|Present)',
}
matches = {kind: [{'index': i, 'generated': 'w2c_gta_f'+str(i), 'name': n}
                  for i, n in names.items() if re.search(pattern, n)][:12]
           for kind, pattern in patterns.items()}
out = {'result': 'PASS', 'engineSHA256': EXPECTED, 'namedFunctions': len(names),
       'scope': 'Original Wasm name-section static mapping, not CPU sampling; optimized C may inline names.',
       'matches': matches}
a.output.write_bytes((json.dumps(out, indent=2)+'\n').encode())
print(json.dumps({'result':'PASS','namedFunctions':len(names),'groups':list(matches)}))
