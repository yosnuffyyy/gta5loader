import fs from 'node:fs';
import path from 'node:path';
const root='native-build/fixtures';
const paths=['data/common/data/levels/gta5/paths.xml','data/x64/anim/cutscene/cuts_prologue.rpf','data/x64/levels/gta5/_prologue/plg_01.rpf'];
fs.mkdirSync(root+'/saves',{recursive:true});
for(let i=0;i<paths.length;i++){
 const p=root+'/web/'+paths[i];fs.mkdirSync(path.dirname(p),{recursive:true});
 const data=Buffer.alloc(65536*2+257+i);
 for(let n=0;n<data.length;n++)data[n]=((n*37+19+i)^(n>>>8))&255;
 fs.writeFileSync(p,data);
}
fs.symlinkSync(path.resolve(root+'/saves'),root+'/web/data/escape','dir');
fs.symlinkSync(path.resolve(root+'/web/'+paths[0]),root+'/saves/link-file');
console.log('Created synthetic file fixtures; no game assets copied into the app.');

fs.writeFileSync(root+'/web/data/manifest.json',JSON.stringify({mount:'/game/',version:'native-fixture',files:[...paths.map((p,i)=>[p.slice(5),65536*2+257+i,0]),['x64/audio/audio.rpf',1024,0]]}));
