"""Read-only Mach-O/IPA measurement; no extraction or execution."""
from pathlib import Path
import argparse
import collections
import hashlib
import json
import struct
import uuid
import zipfile

p=argparse.ArgumentParser()
p.add_argument('ipa',type=Path)
p.add_argument('output',type=Path)
a=p.parse_args()
with zipfile.ZipFile(a.ipa) as z:
    b=z.read('Payload/NativeProbe.app/NativeProbe')
    dsym=any('.dSYM/' in n for n in z.namelist())
assert struct.unpack_from('<I',b)[0]==0xfeedfacf
assert struct.unpack_from('<I',b,4)[0]==0x100000c
pos=32
segments=[]
identifier=None
for _ in range(struct.unpack_from('<I',b,16)[0]):
    cmd,size=struct.unpack_from('<II',b,pos)
    assert size>=8 and pos+size<=len(b)
    if cmd==0x1b:
        identifier=str(uuid.UUID(bytes=b[pos+8:pos+24])).upper()
    elif cmd==0x19:
        name=b[pos+8:pos+24].rstrip(b'\0').decode()
        vmaddr,vmbytes,offset,filebytes=struct.unpack_from('<4Q',b,pos+24)
        segments.append(dict(name=name,vmBytes=vmbytes,fileBytes=filebytes,fileOffset=offset))
    elif cmd==2:
        _,_,symoff,nsyms,stroff,strsize=struct.unpack_from('<6I',b,pos)
    pos+=size
assert symoff+nsyms*16<=len(b) and stroff+strsize<=len(b)
types=collections.Counter()
generated=collections.Counter()
for i in range(nsyms):
    offset,kind,sect,desc,value=struct.unpack_from('<IBBHQ',b,symoff+i*16)
    types[hex(kind)]+=1
    if offset<strsize:
        start=stroff+offset
        end=b.find(b'\0',start,stroff+strsize)
        assert end>=start
        if b'w2c_gta_f' in b[start:end]:
            generated[hex(kind)]+=1
result=dict(result='PASS',origin='Static on-disk v35 ARM64 artifact, not execution/residency',
            ipa=a.ipa.name,ipaBytes=a.ipa.stat().st_size,
            ipaSHA256=hashlib.sha256(a.ipa.read_bytes()).hexdigest(),
            binaryBytes=len(b),UUID=identifier,dSYMBundled=dsym,
            symbols=nsyms,stringTableBytes=strsize,symbolTableBytes=nsyms*16,
            metadataFileBytes=nsyms*16+strsize,
            symbolTypes=dict(types),generatedNameSymbolTypes=dict(generated),segments=segments)
a.output.write_bytes((json.dumps(result,indent=2)+'\n').encode())
print(json.dumps({k:result[k] for k in ['result','binaryBytes','UUID','metadataFileBytes']}))
