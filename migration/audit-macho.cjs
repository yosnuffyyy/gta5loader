'use strict';
const fs=require('node:fs'),assert=require('node:assert/strict'),crypto=require('node:crypto');
const b=fs.readFileSync(process.argv[2]),artifact=fs.readFileSync(process.argv[3]);
assert.equal(b.readUInt32LE(0),0xfeedfacf);assert.equal(b.readUInt32LE(4),0x100000c);
let off=32;const segments=[];let game,ios=false;
const name=at=>b.subarray(at,at+16).toString().replace(/\0.*$/s,'');
for(let i=0;i<b.readUInt32LE(16);i++){const cmd=b.readUInt32LE(off),size=b.readUInt32LE(off+4);assert(size>=8&&off+size<=b.length);
 if(cmd===0x19){const seg=name(off+8),max=b.readUInt32LE(off+56),init=b.readUInt32LE(off+60);assert(!((max&2)&&(max&4)),seg+' permits WX');assert(!((init&2)&&(init&4)),seg+' is WX');segments.push({name:seg,max,init});if(seg==='__GAME'){assert.equal(init,5);assert.equal(max,5);const count=b.readUInt32LE(off+64);assert.equal(count,1);const s=off+72;assert.equal(name(s),'__code');const n=Number(b.readBigUInt64LE(s+40)),at=b.readUInt32LE(s+48);assert.equal(n,artifact.length);assert.deepEqual(b.subarray(at,at+n),artifact);assert.equal(Number(b.readBigUInt64LE(s+32)%16384n),0);game={bytes:n,sha256:crypto.createHash('sha256').update(artifact).digest('hex')};}}
 if(cmd===0x32){assert.equal(b.readUInt32LE(off+8),2,'must target iOS');ios=true;}
 off+=size;
}
assert(ios&&game);const report={result:'PASS',arm64:true,ios:true,embedded:game,segments,executionVerified:false,appOrIPA:false};fs.writeFileSync(process.argv[4],JSON.stringify(report,null,2));console.log(JSON.stringify(report));
