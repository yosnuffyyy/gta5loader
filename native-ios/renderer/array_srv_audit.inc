/* Original im/VS_Blit + im/PS_BlitTexArray and captured 5632/5633 descriptors.
   Texture bytes, uniforms and fullscreen geometry are deterministic fixtures,
   not captured game imagery. All commands use the production consumer. */
static bool array_audit_upload(NativeGpu *g,unsigned mip,unsigned layer,uint16_t rgb){
 unsigned side=64u>>mip,blocks=side/4,bytes=blocks*blocks*16;
 uint32_t data[12+1024]={5632,mip,layer,0,0,0,side,side,1,blocks*16,blocks,bytes};
 for(unsigned i=0;i<bytes;i+=16){uint8_t *p=(uint8_t*)(data+12)+i;p[0]=p[1]=255;p[8]=rgb&255;p[9]=rgb>>8;}
 return execute_impl(g,13,(uint8_t*)data,12+bytes/4);
}
static bool array_audit_pixels(NativeGpu *g,uint16_t rgb,bool srgb,unsigned layer){
 uint32_t cb[19]={6002,0,64};float values[16]={0};values[8]=values[9]=values[10]=1;values[15]=(float)layer;memcpy(cb+3,values,sizeof(values));
 uint32_t draw[]={0,6,1,0,0,0},copy[17]={6000,0,0,0,0,0,0,0,0,3145728,0,64,1024,0,1024,0,0};
 if(!execute_impl(g,12,(uint8_t*)cb,19)||!execute_impl(g,32,(uint8_t*)draw,6)||!execute_impl(g,42,(uint8_t*)copy,17))return false;
 unsigned expected[3]={((rgb>>11)&31)*255/31,((rgb>>5)&63)*255/63,(rgb&31)*255/31};
 for(unsigned c=0;c<3;c++)if(srgb){float f=expected[c]/255.f;expected[c]=(unsigned)lroundf((f<=.04045f?f/12.92f:powf((f+.055f)/1.055f,2.4f))*255.f);}
 uint8_t pixels[1024];host_load(g->host,pixels,3145728,sizeof(pixels));
 for(unsigned i=0;i<256;i++){const uint8_t *p=pixels+i*4;for(unsigned c=0;c<4;c++)if(abs((int)p[c]-(int)(c==3?255:expected[c]))>2){fprintf(stderr,"Array SRV pixel %u channel %u: %u expected %u\n",i,c,p[c],c==3?255:expected[c]);return false;}}
 return true;
}
static bool audit_array_srv(struct w2c_env *h){@autoreleasepool{
 NativeGpu *g=native_gpu_create(h,NULL,NULL,NULL);if(!g)return false;bool ok=false;unsigned draws=0;g->test_inter_stage_limit=15;
 if(!hardware(g)||!host_range(h,3145728,1024))goto done;
 uint32_t vs[]={51,0,0x1e0ed0bc,0x4e1d5d36,3,1,0,0,7,2,0,2,15,3,0,3,3},ps[]={110,1,0x8c9330c5,0x57bf4a9b,0};
 uint32_t layout[]={6003,3,1,0,6,0,0,0,0,2,0,2,0,12,0,0,3,0,16,0,28,0,0};
 uint32_t raster[]={6004,2,3,1,0,0,0,0,1,0,0,0},smp[15]={6005,3,0,3,3,3};float lod=32;memcpy(smp+14,&lod,4);
 uint32_t target[]={6000,2,16,16,1,1,1,28,0x28,1},vb[]={6001,216,1,0,0},cb[]={6002,64,4,0,0};
 if(!execute_impl(g,14,(uint8_t*)vs,17)||!execute_impl(g,14,(uint8_t*)ps,5)||!execute_impl(g,15,(uint8_t*)layout,23)||!execute_impl(g,16,(uint8_t*)raster,12)||!execute_impl(g,16,(uint8_t*)smp,15)||!execute_impl(g,3,(uint8_t*)target,10)||!execute_impl(g,11,(uint8_t*)vb,5)||!execute_impl(g,11,(uint8_t*)cb,5))goto done;
 uint32_t verts[57]={6001,0,216};float quad[6][2]={{-1,1},{1,1},{-1,-1},{-1,-1},{1,1},{1,-1}};
 for(unsigned i=0;i<6;i++){float v[]={quad[i][0],quad[i][1],0,1,1,1,1,.5f,.5f};memcpy(verts+3+i*9,v,sizeof(v));}
 if(!execute_impl(g,12,(uint8_t*)verts,57))goto done;
 g->shader[0]=51;g->shader[1]=110;g->layout=6003;g->raster=6004;g->targets[0]=6000;g->vb[0][0]=6001;g->vb[0][1]=36;g->cb[1][5][0]=6002;g->cb[1][5][2]=4;g->srv[1][7]=5633;g->smp[1][7]=6005;
 DrawProgram *pp=draw_program(g,object(g,110));DrawBinding *binding=NULL;if(!pp)goto done;
 for(unsigned i=0;i<pp->count;i++)if(pp->bindings[i].binding==39)binding=pp->bindings+i;
 if(!binding||binding->slot!=7||binding->dim!=WGPUTextureViewDimension_2DArray||binding->sample!=WGPUTextureSampleType_Float||binding->ms)goto done;
 uint32_t texture[]={5632,2,64,64,1,5,1,77,8,1};const uint16_t colors[]={0xf800,0x07e0,0x001f,0xffff,0x8410};
 if(!execute_impl(g,3,(uint8_t*)texture,10))goto done;
 for(unsigned mip=0;mip<5;mip++)if(!array_audit_upload(g,mip,0,colors[mip]))goto done;
 if(object(g,5632)->version!=6||object(g,5632)->td.size.depthOrArrayLayers!=1)goto done;
 uint64_t generation=object(g,5632)->generation;
 for(unsigned test=0;test<11;test++){
  uint32_t view[]={5633,0,5632,77,4,0,5,0,0},id=5633;uint16_t expected=colors[0];unsigned layer=0;bool srgb=false;
  if(object(g,id)&&!execute_impl(g,4,(uint8_t*)&id,1))goto done;
  if(test<5){view[5]=test;view[6]=5-test;expected=colors[test];}
  if(test==5){view[3]=78;view[5]=4;view[6]=1;expected=colors[4];srgb=true;}
  if(test==7||test==8){id=5632;if(!execute_impl(g,4,(uint8_t*)&id,1))goto done;texture[6]=test==8?2:1;
   if(!execute_impl(g,3,(uint8_t*)texture,10))goto done;
   for(unsigned face=0;face<texture[6];face++)for(unsigned mip=0;mip<5;mip++)if(!array_audit_upload(g,mip,face,face?0x001f:0x07e0))goto done;
   if(object(g,5632)->generation==generation||object(g,5632)->version!=1+5*texture[6])goto done;
  }
  if(test>=7)expected=test==7?0x07e0:0x001f;
  if(test==8){view[4]=5;view[8]=2;layer=1;}
  if(test==9){view[4]=5;view[7]=1;view[8]=1;}
  if(test==10){view[4]=0;view[6]=UINT32_MAX;view[8]=UINT32_MAX;layer=1;}
  if(!execute_impl(g,17,(uint8_t*)view,9)||!array_audit_pixels(g,expected,srgb,layer))goto done;draws++;
  if(test==6){WGPUTextureView old=binding->view;if(!array_audit_upload(g,0,0,0x001f)||!array_audit_pixels(g,0x001f,false,0)||binding->view!=old||object(g,5632)->version!=7)goto done;draws++;}
 }
 ok=draws==12&&g->draw.creates==1&&g->draw.hits==11&&!g->metrics.pipeline_evictions&&!g->draw.proxy_bytes&&!atomic_load(&g->failed);
 if(ok)puts("PASS: exact 5632/5633 BC3 five-mip SRV and original VS 4e1d5d361e0ed0bc + PS 57bf4a9b8c9330c5: 12 draws / 3072 pixels; mip ranges, sRGB, dynamic array layers, update/cache reuse and resource recreation; 1 pipeline / 11 hits / 0 evictions. Synthetic payloads; no scene or phone FPS claim.");
 done:if(!ok)fprintf(stderr,"FAIL: original array SRV audit after %u draws: %s\n",draws,native_gpu_error(g)?native_gpu_error(g):"pixel/descriptor/cache invariant");release_hardware(g);native_gpu_end(g);return ok;
}}
