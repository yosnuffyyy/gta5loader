/* Exact shaders reported with the latest depth-proxy failure. The phone's
   vertex/attachment/view descriptors were not provided; interface fixtures
   below test linking and caching, not the missing resource/view combination. */
static bool audit_ped_pair(NativeGpu *g){@autoreleasepool{
 const uint64_t hashes[]={UINT64_C(0xa276344f1111242f),UINT64_C(0x996bb42f448956f2)};Object *shaders[2]={0};bool ok=false;reset_state(g);
 for(unsigned stage=0;stage<2;stage++){Object *o=shaders[stage]=create(g,8200+stage,14);if(!o)goto cleanup;o->words=stage?5:33;o->meta=calloc(o->words,4);if(!o->meta)goto cleanup;uint32_t *m=o->meta;m[0]=o->id;m[1]=stage;m[2]=(uint32_t)hashes[stage];m[3]=hashes[stage]>>32;m[4]=stage?0:7;for(unsigned i=0;!stage&&i<7;i++){m[5+i*4]=i+1;m[7+i*4]=i;m[8+i*4]=15;}g->shader[stage]=o->id;}
 Object *input=create(g,8202,15);if(!input)goto cleanup;input->words=51;input->meta=calloc(input->words,4);if(!input->meta)goto cleanup;input->meta[0]=input->id;input->meta[1]=7;
 const unsigned formats[]={6,2,2,16,6,2,2};unsigned stride=0;for(unsigned i=0;i<7;i++){unsigned bytes=0;draw_vertex_format(formats[i],&bytes);uint32_t *e=input->meta+2+i*7;e[0]=i+1;e[2]=formats[i];e[4]=stride;stride+=bytes;}g->layout=input->id;g->vb[0][0]=8203;g->vb[0][1]=stride;
 Object *target=create(g,8204,3);if(!target)goto cleanup;target->td=(WGPUTextureDescriptor)WGPU_TEXTURE_DESCRIPTOR_INIT;target->td.dimension=WGPUTextureDimension_2D;target->td.size=(WGPUExtent3D){1,1,1};target->td.mipLevelCount=target->td.sampleCount=1;target->td.format=WGPUTextureFormat_RGBA8Unorm;target->td.usage=WGPUTextureUsage_RenderAttachment;target->texture=wgpuDeviceCreateTexture(g->device,&target->td);if(!target->texture||atomic_load(&g->failed))goto cleanup;g->targets[0]=target->id;
 DrawProgram *vp=draw_program(g,shaders[0]),*pp=draw_program(g,shaders[1]);if(!vp||!pp||atomic_load(&g->failed)||!draw_pipeline(g,shaders[0],shaders[1],vp,pp))goto cleanup;
 uint64_t hits=g->draw.hits,creates=g->draw.creates;if(!draw_pipeline(g,shaders[0],shaders[1],vp,pp)||g->draw.hits!=hits+1||g->draw.creates!=creates)goto cleanup;ok=!atomic_load(&g->failed);
 if(ok)puts("PASS: reported ped_alpha VS a276344f1111242f + PS 996bb42f448956f2: exact original sources, packed layouts, native clip outputs, 15-variable link and cached Metal pipeline. Synthetic input/attachment descriptors; missing iPhone SRV descriptor and scene pixels not verified.");
 cleanup:reset_state(g);for(unsigned id=8200;id<=8204;id++){Object *o=object(g,id);if(o){CFDictionaryRemoveValue(g->objects,(void*)(uintptr_t)id);destroy(o);}}return ok;
}}
