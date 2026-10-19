/* Exact fatal shader sources from the pinned, SHA-verified grass effect.
   Geometry, constants and attachment formats are deterministic test inputs;
   the phone's full material/geometry descriptors were not retained. */
static Object *coverage_buffer(NativeGpu *g,uint32_t id,unsigned bytes,WGPUBufferUsage usage){
 Object *o=create(g,id,11);if(!o)return NULL;o->size=bytes;o->shadow=calloc(1,bytes);WGPUBufferDescriptor d=WGPU_BUFFER_DESCRIPTOR_INIT;d.size=bytes;d.usage=usage|WGPUBufferUsage_CopyDst;o->buffer=wgpuDeviceCreateBuffer(g->device,&d);return o->shadow&&o->buffer?o:NULL;
}
static Object *coverage_target(NativeGpu *g,uint32_t id,WGPUTextureFormat format_,unsigned samples){Object *o=create(g,id,3);if(!o)return NULL;o->td=(WGPUTextureDescriptor)WGPU_TEXTURE_DESCRIPTOR_INIT;o->td.dimension=WGPUTextureDimension_2D;o->td.size=(WGPUExtent3D){16,16,1};o->td.format=format_;o->td.mipLevelCount=1;o->td.sampleCount=samples;o->td.usage=WGPUTextureUsage_RenderAttachment|(samples==1&&!draw_depth(format_)?WGPUTextureUsage_CopySrc:0);o->texture=wgpuDeviceCreateTexture(g->device,&o->td);return o->texture?o:NULL;}
static bool coverage_pair_fixture(NativeGpu *g){@autoreleasepool{
 bool ok=false;WGPUBuffer read=NULL;reset_state(g);
 const uint64_t hashes[]={UINT64_C(0x90bb870460e2835c),UINT64_C(0x0030c87264713865)};
 for(unsigned stage=0;stage<2;stage++){Object *o=create(g,8500+stage,14);if(!o)goto cleanup;o->words=stage?5:25;o->meta=calloc(o->words,4);if(!o->meta)goto cleanup;o->meta[0]=o->id;o->meta[1]=stage;o->meta[2]=(uint32_t)hashes[stage];o->meta[3]=hashes[stage]>>32;o->meta[4]=stage?0:5;for(unsigned i=0;!stage&&i<5;i++){o->meta[5+i*4]=i+1;o->meta[7+i*4]=i;o->meta[8+i*4]=15;}g->shader[stage]=o->id;}
 Object *layout=create(g,8502,15);if(!layout)goto cleanup;layout->words=37;layout->meta=calloc(37,4);if(!layout->meta)goto cleanup;layout->meta[0]=layout->id;layout->meta[1]=5;const unsigned formats[]={2,6,2,2,16};unsigned stride=0;
 for(unsigned i=0;i<5;i++){unsigned bytes=0;draw_vertex_format(formats[i],&bytes);uint32_t *e=layout->meta+2+i*7;e[0]=i+1;e[2]=formats[i];e[4]=stride;stride+=bytes;}g->layout=layout->id;
 Object *vb=coverage_buffer(g,8503,stride*6,WGPUBufferUsage_Vertex);if(!vb)goto cleanup;float quad[6][2]={{-1,1},{1,1},{-1,-1},{-1,-1},{1,1},{1,-1}};
 for(unsigned i=0;i<6;i++){float v[17]={quad[i][0],quad[i][1],.25f,1,0,0,1,0,0,0,0,0,0,0,0,.5f,.5f};memcpy(vb->shadow+i*stride,v,stride);}queue_write_buffer(g,vb->buffer,0,vb->shadow,vb->size);g->vb[0][0]=vb->id;g->vb[0][1]=stride;
 /* Preserve the fatal indexed-instanced DRAW payload, including count48.
    Repeated test-quad indices provide known pixels; scene geometry is absent. */
 Object *ib=coverage_buffer(g,8504,96,WGPUBufferUsage_Index);if(!ib)goto cleanup;for(unsigned i=0;i<48;i++)((uint16_t*)ib->shadow)[i]=i%6;queue_write_buffer(g,ib->buffer,0,ib->shadow,96);g->ib[0]=ib->id;g->ib[1]=57;
 const unsigned sizes[]={192,16,20480,368,736,368};Object *cb[6];for(unsigned i=0;i<6;i++){cb[i]=coverage_buffer(g,8510+i,sizes[i],WGPUBufferUsage_Uniform);if(!cb[i])goto cleanup;}
 float *mat=(float*)cb[0]->shadow;mat[32]=mat[37]=mat[42]=mat[47]=1;
 mat=(float*)cb[2]->shadow;mat[0]=mat[5]=mat[10]=1;uint32_t white=UINT32_MAX;memcpy(mat+3,&white,4);mat[11]=1000000;
 mat=(float*)cb[3]->shadow;mat[64]=mat[88]=mat[91]=1;
 const unsigned stages[]={0,0,0,0,1,1},slots[]={1,4,5,10,3,10};for(unsigned i=0;i<6;i++)g->cb[stages[i]][slots[i]][0]=cb[i]->id;
 Object *tex=create(g,8520,3),*srv=create(g,8521,17);if(!tex||!srv)goto cleanup;tex->td=(WGPUTextureDescriptor)WGPU_TEXTURE_DESCRIPTOR_INIT;tex->td.dimension=WGPUTextureDimension_2D;tex->td.size=(WGPUExtent3D){1,1,1};tex->td.format=WGPUTextureFormat_RGBA8Unorm;tex->td.mipLevelCount=tex->td.sampleCount=1;tex->td.usage=WGPUTextureUsage_TextureBinding|WGPUTextureUsage_CopyDst;tex->texture=wgpuDeviceCreateTexture(g->device,&tex->td);tex->dxgi=28;srv->words=9;srv->meta=calloc(9,4);if(!tex->texture||!srv->meta)goto cleanup;uint32_t desc[]={8521,0,8520,28,4,0,1,0,1};memcpy(srv->meta,desc,sizeof(desc));srv->source_generation=tex->generation;g->srv[1][2]=srv->id;
 WGPUTexelCopyTextureInfo ti=WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;ti.texture=tex->texture;WGPUTexelCopyBufferLayout tl=WGPU_TEXEL_COPY_BUFFER_LAYOUT_INIT;tl.bytesPerRow=4;tl.rowsPerImage=1;WGPUExtent3D one={1,1,1};uint8_t pixel[]={200,100,50,255};queue_write_texture(g,&ti,pixel,4,&tl,&one);
 Object *blend=create(g,8522,16);if(!blend)goto cleanup;blend->words=68;blend->meta=calloc(68,4);if(!blend->meta)goto cleanup;blend->meta[0]=blend->id;blend->meta[2]=1;blend->meta[11]=15;g->blend[0]=blend->id;
 WGPUBufferDescriptor rd=WGPU_BUFFER_DESCRIPTOR_INIT;rd.size=16384;rd.usage=WGPUBufferUsage_CopyDst|WGPUBufferUsage_MapRead;read=wgpuDeviceCreateBuffer(g->device,&rd);if(!read)goto cleanup;
 const float alphas[]={0,.25f,.499f,.5f,.75f,1,-1,2};unsigned cases=0;
 for(unsigned samples=1;samples<=4;samples+=3){
  for(unsigned i=0;i<4;i++)if(!coverage_target(g,8530+i,WGPUTextureFormat_RGBA8Unorm,samples))goto cleanup;
  if(!coverage_target(g,8534,WGPUTextureFormat_Depth32Float,samples)||!coverage_target(g,8535,WGPUTextureFormat_RGBA8Unorm,1))goto cleanup;
  for(unsigned a=0;a<8;a++){
   for(unsigned i=0;i<4;i++)g->targets[i*4]=8530+i;g->targets[32]=8534;
   ((float*)cb[5]->shadow)[88]=1;((float*)cb[5]->shadow)[90]=1;((float*)cb[5]->shadow)[91]=alphas[a];
   uint32_t clear_words[7]={8530,0,0,0,0,0};for(unsigned i=0;i<4;i++){clear_words[0]=8530+i;if(!clear(g,(uint8_t*)clear_words,false))goto cleanup;}
   uint32_t payload[]={3,48,1,0,0,0};if(!draw_execute(g,(uint8_t*)payload)||!draw_flush(g))goto cleanup;
   for(unsigned target=0;target<4;target++){Object *out=object(g,samples==1?8530+target:8535);
   if(samples==4){uint32_t resolve[]={8535,0,8530+target,0};if(!resolve_resource(g,(uint8_t*)resolve))goto cleanup;}
   WGPUCommandEncoder e=encoder(g);WGPUTexelCopyTextureInfo src=WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;src.texture=out->texture;WGPUTexelCopyBufferInfo dst=WGPU_TEXEL_COPY_BUFFER_INFO_INIT;dst.buffer=read;dst.layout.offset=target*4096;dst.layout.bytesPerRow=256;dst.layout.rowsPerImage=16;WGPUExtent3D extent={16,16,1};wgpuCommandEncoderCopyTextureToBuffer(e,&src,&dst,&extent);if(!submit(g,e))goto cleanup;}
   atomic_store(&g->callback_done,false);atomic_store(&g->map_active,true);WGPUBufferMapCallbackInfo map=WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;map.mode=WGPUCallbackMode_AllowProcessEvents;map.callback=mapped;map.userdata1=g;wgpuBufferMapAsync(read,WGPUMapMode_Read,0,16384,map);if(!wait_callback(g))goto cleanup;const uint8_t *pixels=wgpuBufferGetConstMappedRange(read,0,16384);if(!pixels)goto cleanup;
   for(unsigned target=0;target<4;target++)for(unsigned y=0;y<16;y++)for(unsigned x=0;x<16;x++){const uint8_t *p=pixels+target*4096+y*256+x*4;bool on=alphas[a]>=.5f;int rgba[4][4]={{200,100,50,(int)lroundf(fminf(1,fmaxf(0,alphas[a]))*255)},{128,128,255,255},{0,0,250,255},{0,0,0,127}};for(unsigned c=0;c<4;c++){int want=on?rgba[target][c]:0;if((samples==1||alphas[a]<=0||alphas[a]>=1)?abs((int)p[c]-want)>1:p[c]>rgba[target][c]+1){char why[190];snprintf(why,sizeof(why),"Fatal grass A2C samples%u alpha%.3f MRT%u pixel%u,%u channel%u got%u expected%d",samples,alphas[a],target,x,y,c,p[c],want);fail(g,why);goto cleanup;}}}
   wgpuBufferUnmap(read);atomic_store(&g->map_active,false);cases++;
  }
  reset_state(g);for(unsigned id=8530;id<=8535;id++){Object *o=object(g,id);CFDictionaryRemoveValue(g->objects,(void*)(uintptr_t)id);destroy(o);}g->shader[0]=8500;g->shader[1]=8501;g->layout=8502;g->vb[0][0]=8503;g->vb[0][1]=stride;g->ib[0]=8504;g->ib[1]=57;g->srv[1][2]=8521;g->blend[0]=8522;for(unsigned i=0;i<6;i++)g->cb[stages[i]][slots[i]][0]=cb[i]->id;
 }
 ok=cases==16&&!atomic_load(&g->failed);if(ok)puts("PASS: fatal grass VS90bb870460e2835c/PS0030c87264713865: 16 actual DRAW cases, 16384 pixels, four MRTs, single-sample threshold and native4x coverage endpoints; original shaders and synthetic material/geometry. No vehicle collision gameplay claimed.");
 cleanup:if(atomic_exchange(&g->map_active,false)&&read)wgpuBufferUnmap(read);if(read)wgpuBufferRelease(read);draw_flush(g);reset_state(g);for(unsigned id=8500;id<=8535;id++){Object *o=object(g,id);if(o){CFDictionaryRemoveValue(g->objects,(void*)(uintptr_t)id);destroy(o);}}return ok;
}}
