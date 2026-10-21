'use strict';
const fs=require('node:fs'),path=require('node:path'),cp=require('node:child_process'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const root=process.argv[2],out=process.argv[3],revision='1b4600b052b864c78fe36180073ff406663b7e0b';
assert.equal(cp.execFileSync('git',['-C',root,'rev-parse','HEAD'],{encoding:'utf8'}).trim(),revision);
assert.equal(cp.execFileSync('git',['-C',root,'status','--porcelain'],{encoding:'utf8'}),'');
const paths=cp.execFileSync('git',['-C',root,'ls-files'],{encoding:'utf8'}).trim().split('\n');
const files=paths.filter(p=>p.startsWith('src/')||p.startsWith('vendor/')||p.startsWith('ios/')||['Cargo.toml','Cargo.lock','LICENSE','make-ios.sh','build.sh','build.rs','.cargo/config.toml'].includes(p)).map(p=>{const b=fs.readFileSync(path.join(root,p));return {path:p,bytes:b.length,sha256:crypto.createHash('sha256').update(b).digest('hex')};});
fs.writeFileSync(out,JSON.stringify({repository:'https://github.com/c22dev/muguet',revision,license:'GPL-3.0-or-later',files},null,2)+'\n');
console.log('Pinned',files.length,'upstream files; no execution or binary imports');
