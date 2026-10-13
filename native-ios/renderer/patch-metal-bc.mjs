import fs from 'node:fs';
import crypto from 'node:crypto';
const source=process.argv[2],evidence=process.argv[3];
const file=source+'/vendor/wgpu-hal-29.0.3/src/metal/adapter.rs';
const hash=b=>crypto.createHash('sha256').update(b).digest('hex');
const original=fs.readFileSync(file);
if(hash(original)!=='6da21d590125e601015862dd2a66e4dae2fdd5e35d80624dd2c6cca52099d9bf')throw Error('Unexpected pinned Metal adapter source');
const before='            format_bc: os_type == super::OsType::Macos,';
const after=`            // Local native-iPhone patch: ask Metal rather than excluding iOS.
            // Unsupported devices remain unsupported; no feature is fabricated.
            format_bc: os_type == super::OsType::Macos
                || (available!(ios = 16.4, tvos = 16.4, visionos = 1.0)
                    && device_class_responds_to(device, sel!(supportsBCTextureCompression))
                    && device.supportsBCTextureCompression()),`;
let text=original.toString('utf8');if(text.split(before).length!==2)throw Error('BC capability patch is not unique');text=text.replace(before,after);fs.writeFileSync(file,text);
// Cargo's vendored source integrity table must describe this explicit patch.
const checksumFile=source+'/vendor/wgpu-hal-29.0.3/.cargo-checksum.json';const sums=JSON.parse(fs.readFileSync(checksumFile));
if(sums.package!=='31f8e1a9e7a8512f276f7c62e018c7fa8d60954303fed2e5750114332049193f'||sums.files['src/metal/adapter.rs']!==hash(original))throw Error('Unexpected vendor integrity table');
sums.files['src/metal/adapter.rs']=hash(Buffer.from(text));fs.writeFileSync(checksumFile,JSON.stringify(sums)+'\n');
fs.writeFileSync(evidence,JSON.stringify({name:'Metal BC capability detection',wgpuHalVersion:'29.0.3',originalCrateSHA256:sums.package,originalAdapterSHA256:hash(original),patchedAdapterSHA256:sums.files['src/metal/adapter.rs'],before,after,scope:'Only the BC capability condition; normal validation and compressed storage preserved'},null,2)+'\n');
console.log('PASS: exact pinned Metal BC condition patched to the runtime capability query.');
