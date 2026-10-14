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
let text=original.toString('utf8');if(text.split(before).length!==2)throw Error('BC capability patch is not unique');text=text.replace(before,after);
const borderBefore='            sampler_clamp_to_border: Self::supports_any(device, SAMPLER_CLAMP_TO_BORDER_SUPPORT),';
const borderAfter=`            // Apple7+ supports sampler border colors (Apple Metal feature table).
            // The old macOS-only feature-set list hides actual iOS support.
            sampler_clamp_to_border: Self::supports_any(device, SAMPLER_CLAMP_TO_BORDER_SUPPORT)
                || (family_check && device.supportsFamily(MTLGPUFamily::Apple7)),`;
if(text.split(borderBefore).length!==2)throw Error('Sampler-border capability patch is not unique');text=text.replace(borderBefore,borderAfter);fs.writeFileSync(file,text);
// Naga's pinned MSL writer emits [[clip_distance]], supported by Metal 1.0.
// The pinned HAL omits this feature from its advertised list. Keep actual
// hardware clipping; stripping the WGSL builtin would change game pixels.
const clipBefore='            | F::EXTERNAL_TEXTURE;';
const clipAfter='            | F::EXTERNAL_TEXTURE\n            | F::CLIP_DISTANCES;';
if(text.split(clipBefore).length!==2)throw Error('Clip-distance capability patch is not unique');text=text.replace(clipBefore,clipAfter);fs.writeFileSync(file,text);
// Cargo's vendored source integrity table must describe this explicit patch.
const checksumFile=source+'/vendor/wgpu-hal-29.0.3/.cargo-checksum.json';const sums=JSON.parse(fs.readFileSync(checksumFile));
if(sums.package!=='31f8e1a9e7a8512f276f7c62e018c7fa8d60954303fed2e5750114332049193f'||sums.files['src/metal/adapter.rs']!==hash(original))throw Error('Unexpected vendor integrity table');
sums.files['src/metal/adapter.rs']=hash(Buffer.from(text));fs.writeFileSync(checksumFile,JSON.stringify(sums)+'\n');
const nagaFile=source+'/vendor/naga-29.0.3/src/back/msl/writer.rs';
const nagaOriginal=fs.readFileSync(nagaFile);if(hash(nagaOriginal)!=='e0477d761c4fb20445bc54457a278320e7a65dc51bf25209d24aa951639e8704')throw Error('Unexpected pinned Naga MSL writer');
const nagaBefore=`                        write!(self.out, "{}{} {}", back::INDENT, ty_name, name)?;
                        if let Some(array_len) = array_len {
                            write!(self.out, " [{array_len}]")?;
                        }
                        resolved.try_fmt(&mut self.out)?;`;
const nagaAfter=`                        write!(self.out, "{}{} {}", back::INDENT, ty_name, name)?;
                        // Metal attaches clip_distance to the array declarator,
                        // before its extent. After [N], clang applies it to a type.
                        let clip = matches!(*binding, crate::Binding::BuiltIn(crate::BuiltIn::ClipDistance));
                        if clip { resolved.try_fmt(&mut self.out)?; }
                        if let Some(array_len) = array_len {
                            write!(self.out, " [{array_len}]")?;
                        }
                        if !clip { resolved.try_fmt(&mut self.out)?; }`;
let nagaText=nagaOriginal.toString('utf8');if(nagaText.split(nagaBefore).length!==2)throw Error('Naga clip-distance patch is not unique');nagaText=nagaText.replace(nagaBefore,nagaAfter);fs.writeFileSync(nagaFile,nagaText);
const nagaChecksumFile=source+'/vendor/naga-29.0.3/.cargo-checksum.json',nagaSums=JSON.parse(fs.readFileSync(nagaChecksumFile));
if(nagaSums.package!=='0dd91265cc2454558f659b3b4b9640f0ddb8cc6521277f166b8a8c181c898079'||nagaSums.files['src/back/msl/writer.rs']!==hash(nagaOriginal))throw Error('Unexpected Naga vendor integrity table');nagaSums.files['src/back/msl/writer.rs']=hash(Buffer.from(nagaText));fs.writeFileSync(nagaChecksumFile,JSON.stringify(nagaSums)+'\n');
const nagaPatch={version:'29.0.3',crateSHA256:nagaSums.package,originalWriterSHA256:hash(nagaOriginal),patchedWriterSHA256:nagaSums.files['src/back/msl/writer.rs'],before:nagaBefore,after:nagaAfter};
fs.writeFileSync(evidence,JSON.stringify({name:'Metal BC, sampler-border and clip-distance capabilities plus Naga clip-array declarator',wgpuHalVersion:'29.0.3',originalCrateSHA256:sums.package,originalAdapterSHA256:hash(original),patchedAdapterSHA256:sums.files['src/metal/adapter.rs'],before,after,borderBefore,borderAfter,clipBefore,clipAfter,nagaPatch,scope:'Only checked feature advertisement and clip_distance attribute placement; native primitive clipping and normal validation preserved'},null,2)+'\n');
console.log('PASS: pinned Metal capabilities patched; native clipping remains enabled and validated.');
