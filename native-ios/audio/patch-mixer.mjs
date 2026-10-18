import fs from 'node:fs';
import crypto from 'node:crypto';
const folder=process.argv[2],report=process.argv[3];
let found=0,record;
for(const name of fs.readdirSync(folder).filter(n=>/^gta.*\.c$/.test(n))){
 const path=folder+'/'+name;let text=fs.readFileSync(path,'utf8');
 const start=text.indexOf('\nvoid w2c_gta_f7394(w2c_gta* instance, u64 var_p0) {');if(start<0)continue;
 const end=text.indexOf('\n}',start)+2;if(end<start)throw Error('Mixer definition extent');
 const before=text.slice(start,end),calls=before.match(/w2c_gta_f7266\(instance, var_j0\);/g)||[];
 if(calls.length!==2||!before.includes('8153472')||!before.includes('3072u')||before.includes('native_audio_mix_begin'))throw Error('Original mixer call/queue contract changed or already patched');
 const after=before.replaceAll('w2c_gta_f7266(instance, var_j0);','native_audio_mix_begin();\n      w2c_gta_f7266(instance, var_j0);\n      native_audio_mix_end();');
 text=text.slice(0,start)+'\n#include "audio/native_mixer_profile.h"\n'+after+text.slice(end);fs.writeFileSync(path,text);found++;
 record={partition:name,originalFunction:7394,measuredFunction:7266,beforeSHA256:crypto.createHash('sha256').update(before).digest('hex'),afterSHA256:crypto.createHash('sha256').update(after).digest('hex'),scope:'Sample original MixBuffers wall/thread CPU every 64 calls. Original PCM, waits, clocks and decode unchanged.'};
}
if(found!==1)throw Error('Expected one original mixer definition, got '+found);
fs.writeFileSync(report,JSON.stringify(record,null,2)+'\n');console.log('PASS: verified mixer sampling hook; no production clock, PCM or scheduling change');
