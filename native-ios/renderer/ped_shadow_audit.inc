static bool audit_ped_shadow(NativeGpu *g){@autoreleasepool{
 bool ok=false;reset_state(g);Object *shaders[2]={0};const uint64_t hashes[]={UINT64_C(0xa276344f1111242f),UINT64_C(0x996bb42f448956f2)};
 for(unsigned st=0;st<2;st++){Object *o=shaders[st]=create(g,8300+st,14);if(!o)goto cleanup;o->words=st?5:33;o->meta=calloc(o->words,4);if(!o->meta)goto cleanup;uint32_t *m=o->meta;m[0]=o->id;m[1]=st;m[2]=(uint32_t)hashes[st];m[3]=hashes[st]>>32;m[4]=st?0:7;for(unsigned i=0;!st&&i<7;i++){m[5+i*4]=i+1;m[7+i*4]=i;m[8+i*4]=15;}g->shader[st]=o->id;}
 Object *input=create(g,8302,15);if(!input)goto cleanup;input->words=51;input->meta=calloc(input->words,4);if(!input->meta)goto cleanup;input->meta[0]=input->id;input->meta[1]=7;const unsigned formats[]={6,2,2,16,6,2,2};unsigned stride=0;for(unsigned i=0;i<7;i++){unsigned bytes=0;draw_vertex_format(formats[i],&bytes);uint32_t *e=input->meta+2+i*7;e[0]=i+1;e[2]=formats[i];e[4]=stride;stride+=bytes;}g->layout=input->id;g->vb[0][0]=8303;g->vb[0][1]=stride;
 Object *target=create(g,8304,3);if(!target)goto cleanup;target->td=(WGPUTextureDescriptor)WGPU_TEXTURE_DESCRIPTOR_INIT;target->td.dimension=WGPUTextureDimension_2D;target->td.size=(WGPUExtent3D){1,1,1};target->td.mipLevelCount=target->td.sampleCount=1;target->td.format=WGPUTextureFormat_RGBA8Unorm;target->td.usage=WGPUTextureUsage_RenderAttachment;target->texture=wgpuDeviceCreateTexture(g->device,&target->td);if(!target->texture)goto cleanup;g->targets[0]=target->id;
 uint32_t creation[]={708,2,4,4,1,1,1,71,8,1},upload[]={708,0,0,0,0,0,4,4,1,8,1,8,0xf800,0},cb[]={8305,496,4,0,0};if(!create_texture(g,(uint8_t*)creation)||!upload_texture(g,(uint8_t*)upload)||!create_buffer(g,(uint8_t*)cb))goto cleanup;Object *srv=create(g,709,17);if(!srv)goto cleanup;uint32_t captured[]={709,0,708,71,4,0,1,0,0};srv->words=9;srv->meta=malloc(sizeof(captured));if(!srv->meta)goto cleanup;memcpy(srv->meta,captured,sizeof(captured));srv->source_generation=object(g,708)->generation;g->cb[1][6][0]=8305;
 DrawProgram *vp=draw_program(g,shaders[0]),*pp=draw_program(g,shaders[1]);if(!vp||!pp||!pp->shadow_plan||pp->shadow_plan->count!=4)goto cleanup;for(unsigned i=0;i<4;i++){ShadowGuard *q=pp->shadow_plan->guards+i;if(q->count<2||q->conditions[0].slot!=6||q->conditions[1].slot!=6||q->conditions[0].word!=63+16*i||q->conditions[1].word!=63+16*i)goto cleanup;}
 unsigned masks[]={0,1,2,4,8,15,1,0};uint64_t evictions=g->metrics.shadow_evictions;for(unsigned test=0;test<8;test++){for(unsigned j=0;j<4;j++)g->srv[1][pp->shadow_plan->guards[j].binding-32]=masks[test]&(1u<<j)?709:0;DrawProgram *v=shadow_program(g,pp);if(!v||v->inactive_mask!=masks[test]||!draw_pipeline(g,shaders[0],shaders[1],vp,v))goto cleanup;}
 /* Original no-light control must independently certify binding 46 even
    when the stale shadow-type selector says cube. */
 {Object *buffer=object(g,8305);uint32_t one=0x3f800000;memcpy(buffer->shadow+63*4,&one,4);g->srv[1][14]=709;DrawProgram *v=shadow_program(g,pp);if(!v||!(v->inactive_mask&1u)||!draw_pipeline(g,shaders[0],shaders[1],vp,v))goto cleanup;}
 ok=!atomic_load(&g->failed)&&g->metrics.shadow_evictions>=evictions+2;
 if(ok)puts("PASS: exact ped_alpha pair and captured 709/708 descriptor: four certified light guards, nine pipeline/cache configurations including original zero-light outer guard, original native clips/15-variable link, bounded variant eviction/recreation. Synthetic uniforms/layout; scene pixels not claimed.");
 cleanup:reset_state(g);for(unsigned id=8300;id<=8305;id++){Object *o=object(g,id);if(o){CFDictionaryRemoveValue(g->objects,(void*)(uintptr_t)id);destroy(o);}}for(unsigned id=708;id<=709;id++){Object *o=object(g,id);if(o){CFDictionaryRemoveValue(g->objects,(void*)(uintptr_t)id);destroy(o);}}return ok;
}}

/* Exact 0.23 phone state: active light count=1, selector=2, original black
   2D BC1 view. A required cube cannot be reconstructed from this descriptor.
   Preserve a negative regression rather than falsifying a successful draw. */
typedef struct { unsigned cb3_rows,cb6_rows; bool count,type,history; } ShadowCaptureCheck;
static void audit_shadow_capture(void *context,const char *text){
 ShadowCaptureCheck *c=context;
 if(strncmp(text,"Shadow fatal CB3 words ",23)==0)c->cb3_rows++;
 if(strncmp(text,"Shadow fatal CB6 words ",23)==0)c->cb6_rows++;
 if(strstr(text,"Shadow fatal CB3 words 8: 00000001"))c->count=true;
 if(strstr(text,"Shadow fatal CB6 words 56:")&&strstr(text,"40000000"))c->type=true;
 if(strstr(text,"Shadow transition history:"))c->history=true;
}
static bool audit_active_shadow(struct w2c_env *h,unsigned reference){@autoreleasepool{
 NativeGpu *g=native_gpu_create(h,NULL,NULL,NULL);if(!g)return false;bool ok=false;g->test_inter_stage_limit=15;g->reference_shadow_compat=reference;
 native_progress saved_progress=h->progress;void *saved_context=h->progress_context;ShadowCaptureCheck capture_check={0};h->progress=audit_shadow_capture;h->progress_context=&capture_check;
 if(!hardware(g))goto cleanup;reset_state(g);
 const uint64_t hashes[]={reference?UINT64_C(0xa276344f1111242f):UINT64_C(0xac2dd56e1111be56),reference==2?UINT64_C(0x6e9619cde3dac275):UINT64_C(0x996bb42f448956f2)};Object *shaders[2]={0};
 for(unsigned st=0;st<2;st++){Object *o=shaders[st]=create(g,st?4000:3993,14);if(!o)goto cleanup;o->words=st?5:5+(reference?7:5)*4;o->meta=calloc(o->words,4);if(!o->meta)goto cleanup;o->meta[0]=o->id;o->meta[1]=st;o->meta[2]=(uint32_t)hashes[st];o->meta[3]=hashes[st]>>32;o->meta[4]=st?0:reference?7:5;for(unsigned i=0;!st&&i<(reference?7:5);i++){o->meta[5+i*4]=i+1;o->meta[7+i*4]=i;o->meta[8+i*4]=15;}g->shader[st]=o->id;}
 Object *input=create(g,21529,15);if(!input)goto cleanup;input->words=2+(reference?7:5)*7;input->meta=calloc(input->words,4);if(!input->meta)goto cleanup;input->meta[0]=input->id;input->meta[1]=reference?7:5;const unsigned formats[]={6,2,16,6,2},reference_formats[]={6,2,2,16,6,2,2};unsigned stride=0;for(unsigned i=0;i<(reference?7:5);i++){unsigned format=(reference?reference_formats:formats)[i],bytes=0;draw_vertex_format(format,&bytes);uint32_t *e=input->meta+2+i*7;e[0]=i+1;e[2]=format;e[4]=stride;stride+=bytes;}if(stride!=(reference?96u:64u))goto cleanup;g->layout=input->id;g->vb[0][0]=14637;g->vb[0][1]=stride;
 uint32_t creation[]={708,2,4,4,1,1,1,71,8,1},upload[]={708,0,0,0,0,0,4,4,1,8,1,8,0,0},cb3[]={8306,976,4,0,0},cb6[]={8307,528,4,0,0};
 if(!create_texture(g,(uint8_t*)creation)||!upload_texture(g,(uint8_t*)upload)||!create_buffer(g,(uint8_t*)cb3)||!create_buffer(g,(uint8_t*)cb6))goto cleanup;
 uint32_t count=1,type=0x40000000;memcpy(object(g,8306)->shadow+32,&count,4);memcpy(object(g,8307)->shadow+252,&type,4);g->cb[1][3][0]=8306;g->cb[1][6][0]=8307;
 Object *srv=create(g,709,17);if(!srv)goto cleanup;uint32_t captured[]={709,0,708,71,4,0,1,0,0};srv->words=9;srv->meta=malloc(sizeof(captured));if(!srv->meta)goto cleanup;memcpy(srv->meta,captured,sizeof(captured));srv->source_generation=object(g,708)->generation;
 Object *target=create(g,10464,3);if(!target)goto cleanup;target->td=(WGPUTextureDescriptor)WGPU_TEXTURE_DESCRIPTOR_INIT;target->td.dimension=WGPUTextureDimension_2D;target->td.size=(WGPUExtent3D){1,1,1};target->td.mipLevelCount=target->td.sampleCount=1;target->td.format=WGPUTextureFormat_RGBA8Unorm;target->td.usage=WGPUTextureUsage_RenderAttachment;target->texture=wgpuDeviceCreateTexture(g->device,&target->td);g->targets[0]=target->id;
 DrawProgram *vp=draw_program(g,shaders[0]),*pp=draw_program(g,shaders[1]);if(!vp||!pp||!pp->shadow_plan||pp->shadow_plan->guards[0].count!=3||!draw_pipeline(g,shaders[0],shaders[1],vp,pp))goto cleanup;
 /* Full original shader, not the reduced pixel fixture: repeatedly change
    the captured lanes, nonzero CB offsets, and buffer object generations.
    The original program must be selected again for every required sample. */
 {uint32_t cube[]={8310,2,4,4,1,1,6,40,0x48,1},view[]={8311,0,8310,40,9,0,1,0,6};
  if(!execute_impl(g,3,(uint8_t*)cube,10)||!execute_impl(g,17,(uint8_t*)view,9))goto cleanup;
  DrawProgram *inactive=NULL;uint64_t hits=g->metrics.shadow_hits;
  for(unsigned cycle=0;cycle<3;cycle++){
   uint32_t first=cycle==1?2:0;g->cb[1][3][1]=g->cb[1][6][1]=first;
   if(cycle==2){uint32_t id=8306;if(!execute_impl(g,4,(uint8_t*)&id,1)||!create_buffer(g,(uint8_t*)cb3))goto cleanup;id=8307;if(!execute_impl(g,4,(uint8_t*)&id,1)||!create_buffer(g,(uint8_t*)cb6))goto cleanup;}
   const uint32_t counts[]={0,1,1,1,1,0},types[]={0x40000000,0,0x40400000,0x40000000,0x7fc00000,0x40000000};
   for(unsigned i=0;i<6;i++){uint32_t a[]={8306,first*16+32,4,counts[i]},b[]={8307,first*16+252,4,types[i]};
    if(!execute_impl(g,12,(uint8_t*)a,4)||!execute_impl(g,12,(uint8_t*)b,4))goto cleanup;
    bool proven=i<3||i==5;g->srv[1][14]=proven?709:8311;DrawProgram *v=shadow_program(g,pp);
    if(!v||v->inactive_mask!=(proven?1u:0u)||(!proven&&v!=pp)||!draw_pipeline(g,shaders[0],shaders[1],vp,v))goto cleanup;
    if(proven){if(inactive&&v!=inactive)goto cleanup;inactive=v;}
   }
  }
  if(g->metrics.shadow_creates!=1||g->metrics.shadow_hits-hits!=11||g->metrics.shadow_evictions)goto cleanup;
  printf("PASS: full original %s/%s pair: 18 alternating required/inactive/ambiguous binding selections; one certified variant reused only after fresh uniform proofs; CB offsets and object generation changes preserved; real six-face resource for required samples. Pipeline validation, not scene pixels.\n",reference?"a276344f1111242f":"ac2dd56e1111be56",reference==2?"6e9619cde3dac275":"996bb42f448956f2");
 }
 {uint32_t a[]={8306,32,4,1},b[]={8307,252,4,0x40000000};g->cb[1][3][1]=g->cb[1][6][1]=0;if(!execute_impl(g,12,(uint8_t*)a,4)||!execute_impl(g,12,(uint8_t*)b,4))goto cleanup;}
 for(unsigned j=0;j<3;j++)if(shadow_proven_inactive(pp->shadow_plan->guards[0].conditions[j],shadow_uniform_bits(g,pp,pp->shadow_plan->guards[0].conditions[j])))goto cleanup;
 g->srv[1][14]=709;g->last_op=32;g->last_words=6;uint32_t payload[]={1,672,1,0,0,0};memcpy(g->last_payload,payload,sizeof(payload));
 {
  uint32_t captured_sampler[]={3511,3,20,3,3,1,0,1,1,0,0,0,0,0,1094713344};
  if(!execute_impl(g,16,(uint8_t*)captured_sampler,15))goto cleanup;g->smp[1][14]=3511;
 }
 if(reference){
  const uint32_t active_counts[]={1,2,3,4,5,INT32_MAX};ok=true;
  for(unsigned i=0;i<6&&ok;i++){
   uint32_t update[]={8306,32,4,active_counts[i]};if(!execute_impl(g,12,(uint8_t*)update,4)){ok=false;break;}
   DrawProgram *selected=shadow_program(g,pp);unsigned offset=0;
   ok=selected==pp&&draw_prepare_proxies(g,pp)&&draw_uniforms(g)&&draw_group(g,pp,&offset)&&!native_gpu_error(g);
  }
  ok=ok&&g->reference_shadow_uses==6&&g->reference_shadow_allocations==1;
  if(ok)printf("PASS: original a276344f1111242f/%s pipeline and full reflected binding group validate with exact0.27 sampler, active counts1/2/3/4/5/INT32_MAX, selector2 and optional reference empty cube. Original shader unchanged; synthetic remaining constants, no scene-pixel claim.\n",reference==2?"6e9619cde3dac275":"996bb42f448956f2");
  goto cleanup;
 }
 ok=!draw_execute(g,(uint8_t*)payload)&&native_gpu_error(g)&&strstr(native_gpu_error(g),"SRV range outside texture")&&strstr(native_gpu_error(g),"ac2dd56e1111be56")&&strstr(native_gpu_error(g),"raw40000000")&&strstr(native_gpu_error(g),"raw00000001")&&!g->draw.draws&&!g->draw.proxy_bytes&&object(g,708)->td.size.depthOrArrayLayers==1&&object(g,708)->version==2;
 ok=ok&&capture_check.cb3_rows==30&&capture_check.cb6_rows==16&&capture_check.count&&capture_check.type&&capture_check.history;
 if(ok)puts("PASS: exact physical active binding reproduced: VS ac2dd56e1111be56 + PS 996bb42f448956f2 pipeline validates; count 1 / selector 2, captured black BC1 708/709 rejects six-layer sampling. Bounded history and both original constant blocks verified through the report callback. No inactivity proof, draw, layer fabrication or crash-resolution claim.");
 cleanup:release_hardware(g);native_gpu_end(g);h->progress=saved_progress;h->progress_context=saved_context;return ok;
}}
