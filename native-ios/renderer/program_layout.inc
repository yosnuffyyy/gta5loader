static bool program_layout(NativeGpu *g,DrawProgram *p){
 WGPUBindGroupLayoutEntry entries[DRAW_BINDINGS];for(unsigned i=0;i<p->count;i++){DrawBinding *b=p->bindings+i;entries[i]=(WGPUBindGroupLayoutEntry)WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;entries[i].binding=b->binding;entries[i].visibility=p->stage==2?WGPUShaderStage_Compute:p->stage?WGPUShaderStage_Fragment:WGPUShaderStage_Vertex;
  if(b->kind==1){entries[i].buffer.type=WGPUBufferBindingType_Uniform;entries[i].buffer.hasDynamicOffset=b->binding==0;entries[i].buffer.minBindingSize=b->binding==0?p->pack_size:0;}
  else if(b->kind==2||b->kind==3)entries[i].sampler.type=b->kind==3?WGPUSamplerBindingType_Comparison:WGPUSamplerBindingType_Filtering;
  else if(b->kind==5||b->kind==6)entries[i].buffer.type=b->kind==5?WGPUBufferBindingType_ReadOnlyStorage:WGPUBufferBindingType_Storage;
  else if(b->kind==7){entries[i].storageTexture.access=b->access;entries[i].storageTexture.format=b->format;entries[i].storageTexture.viewDimension=b->dim;}
  else {entries[i].texture.sampleType=b->sample;entries[i].texture.viewDimension=b->dim;entries[i].texture.multisampled=b->ms;}
 }
 WGPUBindGroupLayoutDescriptor ld=WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;ld.entryCount=p->count;ld.entries=entries;p->layout=wgpuDeviceCreateBindGroupLayout(g->device,&ld);return check(g,p->layout&&!atomic_load(&g->failed),"Shader binding layout creation failed");
}
