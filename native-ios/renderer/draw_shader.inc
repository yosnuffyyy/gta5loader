/* Original local WGSL/constant-table loader. Reads only a shader's pack range;
   no network access, whole-pack cache or game-file duplication at runtime. */
static NSRegularExpression *draw_regex(NSString *pattern){return [NSRegularExpression regularExpressionWithPattern:pattern options:0 error:NULL];}
static NSString *draw_capture(NSString *s,NSTextCheckingResult *m,unsigned i){return [s substringWithRange:[m rangeAtIndex:i]];}
static NSArray<NSTextCheckingResult*> *draw_matches(NSString *pattern,NSString *s){return [draw_regex(pattern) matchesInString:s options:0 range:NSMakeRange(0,s.length)];}
static int draw_file(int root,const char *path){
 if(root<0||!path||!*path||strlen(path)>2048)return -1;
 int dir=dup(root);if(dir<0)return -1;char text[2049];strcpy(text,path);char *part=text;
 for(;;){char *slash=strchr(part,'/');if(slash)*slash=0;
  if(!*part||!strcmp(part,".")||!strcmp(part,"..")){close(dir);return -1;}
  for(const unsigned char *c=(void*)part;*c;c++)if(*c<32||*c>126||*c==':'||*c=='\\'){close(dir);return -1;}
  int next=openat(dir,part,O_RDONLY|O_NOFOLLOW|O_CLOEXEC|(slash?O_DIRECTORY:0));close(dir);if(next<0)return -1;
  if(!slash){struct stat st;if(fstat(next,&st)||!S_ISREG(st.st_mode)){close(next);return -1;}return next;}
  dir=next;part=slash+1;
 }
}
static NSData *draw_read(int root,const char *path,uint64_t offset,uint64_t bytes,bool whole){
 int fd=draw_file(root,path);if(fd<0)return nil;struct stat st;NSData *result=nil;
 if(!fstat(fd,&st)&&st.st_size>=0){uint64_t size=(uint64_t)st.st_size;if(whole)bytes=size;
  if(bytes&&bytes<=16*1024*1024&&offset<=size&&bytes<=size-offset&&offset<=INT64_MAX){
   NSMutableData *data=[NSMutableData dataWithLength:(NSUInteger)bytes];size_t done=0;
   while(done<bytes){ssize_t n=pread(fd,(uint8_t*)data.mutableBytes+done,(size_t)bytes-done,(off_t)(offset+done));if(n<0&&errno==EINTR)continue;if(n<=0)break;done+=(size_t)n;}
   if(done==bytes)result=data;
  }
 }close(fd);return result;
}
static id draw_json(NSData *data){return data?[NSJSONSerialization JSONObjectWithData:data options:NSJSONReadingFragmentsAllowed error:NULL]:nil;}
static bool draw_uint(id n,uint64_t *out){if(![n isKindOfClass:NSNumber.class]||CFGetTypeID((__bridge CFTypeRef)n)==CFBooleanGetTypeID())return false;double d=[n doubleValue];if(!isfinite(d)||d<0||d>9007199254740991.0||floor(d)!=d)return false;*out=(uint64_t)d;return true;}
static NSString *draw_source(NativeGpu *g,const char *hash,id *constants){
 *constants=nil;
 if(g->draw.fixture){if(!strcmp(hash,"028921879df627f5"))return @(draw_fixture_vs);if(!strcmp(hash,"943b0ad9542193ff"))return @(draw_fixture_ps);}
 if(!g->draw.index){
  const char *dirs[]={"b/8b0b5899ed/shaders","shaders"};
  for(unsigned i=0;i<2&&!g->draw.index;i++){char path[128];snprintf(path,sizeof(path),"%s/index.json",dirs[i]);id index=draw_json(draw_read(g->host->game_root,path,0,0,true));
   if([index isKindOfClass:NSDictionary.class]){g->draw.index=CFBridgingRetain(index);snprintf(g->draw.directory,sizeof(g->draw.directory),"%s",dirs[i]);}
  }
 }
 NSDictionary *index=(__bridge NSDictionary*)g->draw.index;id rec=index[@(hash)];
 if(!check(g,[rec isKindOfClass:NSDictionary.class]&&[rec[@"ok"] isKindOfClass:NSNumber.class]&&[rec[@"ok"] boolValue],"Local shader index missing shader or marks translation unavailable"))return nil;
 NSData *source=nil;NSArray *range=rec[@"p"];
 if([range isKindOfClass:NSArray.class]&&range.count==4){uint64_t pack,offset,wl,cl;
  if(!check(g,draw_uint(range[0],&pack)&&draw_uint(range[1],&offset)&&draw_uint(range[2],&wl)&&draw_uint(range[3],&cl)&&wl<=1024*1024&&cl<=256*1024&&wl&&offset<=INT64_MAX-wl-cl,"Invalid shader pack range"))return nil;
  NSArray *packs=index[@"_packs"];NSString *file=nil;if([packs isKindOfClass:NSArray.class]&&pack<packs.count&&[packs[pack] isKindOfClass:NSDictionary.class])file=packs[pack][@"file"];
  if(![file isKindOfClass:NSString.class])file=[NSString stringWithFormat:@"pack%llu.bin",(unsigned long long)pack];
  NSString *path=[NSString stringWithFormat:@"%s/%@",g->draw.directory,file];NSData *data=draw_read(g->host->game_root,path.UTF8String,offset,wl+cl,false);
  if(data){source=[data subdataWithRange:NSMakeRange(0,(NSUInteger)wl)];if(cl)*constants=draw_json([data subdataWithRange:NSMakeRange((NSUInteger)wl,(NSUInteger)cl)]);}
 }
 if(!source){char path[160];snprintf(path,sizeof(path),"%s/%s.wgsl",g->draw.directory,hash);source=draw_read(g->host->game_root,path,0,0,true);if([rec[@"consts"] isKindOfClass:NSNumber.class]&&[rec[@"consts"] boolValue]){snprintf(path,sizeof(path),"%s/%s.consts.json",g->draw.directory,hash);*constants=draw_json(draw_read(g->host->game_root,path,0,0,true));}}
 if(!check(g,source&&source.length<=1024*1024,"Local shader source unavailable or exceeds 1 MiB"))return nil;
 return [[NSString alloc] initWithData:source encoding:NSUTF8StringEncoding];
}
static void draw_program_free(DrawProgram *p){if(!p)return;for(unsigned i=0;i<DRAW_UBO_SLOTS;i++)if(p->groups[i])wgpuBindGroupRelease(p->groups[i]);for(unsigned i=0;i<p->count;i++){if(p->bindings[i].view)wgpuTextureViewRelease(p->bindings[i].view);if(p->bindings[i].dummy){wgpuTextureDestroy(p->bindings[i].dummy);wgpuTextureRelease(p->bindings[i].dummy);}}if(p->constants){wgpuBufferDestroy(p->constants);wgpuBufferRelease(p->constants);}if(p->layout)wgpuBindGroupLayoutRelease(p->layout);if(p->module)wgpuShaderModuleRelease(p->module);free(p->source);free(p);}
static DrawProgram *draw_program(NativeGpu *g,Object *o){
 if(!check(g,o&&o->kind==14&&o->meta[1]<2,"Draw requires a vertex/fragment shader"))return NULL;if(o->program)return o->program;
 DrawProgram *p=calloc(1,sizeof(*p));if(!check(g,p!=NULL,"Shader program allocation"))return NULL;o->program=p;p->stage=o->meta[1];snprintf(p->hash,sizeof(p->hash),"%08x%08x",o->meta[3],o->meta[2]);
 @autoreleasepool {
  id constants=nil;NSString *source=draw_source(g,p->hash,&constants);if(!check(g,source!=nil,"Invalid UTF-8 shader source"))return NULL;
  NSMutableString *text=[source mutableCopy];NSString *decl=@"@group\\((\\d+)u?\\)\\s*@binding\\((\\d+)u?\\)\\s*var<uniform>\\s+(\\w+)\\s*:\\s*(\\w+)\\s*;";
  NSMutableArray *members=[NSMutableArray array];NSArray *matches=draw_matches(decl,text);
  for(NSTextCheckingResult *m in [matches reverseObjectEnumerator]){unsigned group=draw_capture(text,m,1).intValue,slot=draw_capture(text,m,2).intValue;if(slot==15)continue;
   if(!check(g,group==p->stage&&slot<16&&members.count<DRAW_MEMBERS,"Unsupported uniform group/slot count"))return NULL;
   NSString *name=draw_capture(text,m,3),*type=draw_capture(text,m,4);NSArray *st=draw_matches([NSString stringWithFormat:@"struct\\s+%@\\s*\\{([^}]*)\\}",type],source);
   if(!check(g,st.count==1,"Uniform structure definition missing"))return NULL;
   NSArray *arr=draw_matches(@"array<[^>]*>,\\s*(\\d+)u?>",draw_capture(source,st[0],1));
   if(!check(g,arr.count==1,"Uniform structure is outside translator array ABI"))return NULL;
   NSString *body=draw_capture(source,st[0],1);uint64_t size=(uint64_t)draw_capture(body,arr[0],1).longLongValue*16;
   if(!check(g,size&&size<=65536,"Uniform structure exceeds 64 KiB"))return NULL;
   [members addObject:@{@"slot":@(slot),@"name":name,@"type":type,@"size":@(size)}];[text deleteCharactersInRange:m.range];
  }
  [members sortUsingComparator:^NSComparisonResult(id a,id b){return [a[@"slot"] compare:b[@"slot"]];}];NSMutableString *definition=[NSMutableString stringWithString:@"struct cbpack_t {\n"];
  for(NSDictionary *m in members){unsigned i=p->member_count++;p->members[i].slot=[m[@"slot"] unsignedIntValue];p->members[i].offset=p->pack_size;p->members[i].size=[m[@"size"] unsignedIntValue];p->pack_size+=p->members[i].size;
   NSString *pattern=[NSString stringWithFormat:@"\\b%@\\b",m[@"name"]];[draw_regex(pattern) replaceMatchesInString:text options:0 range:NSMakeRange(0,text.length) withTemplate:[@"cbpack_v." stringByAppendingString:m[@"name"]]];
   [definition appendFormat:@"  %@ : %@,\n",m[@"name"],m[@"type"]];
  }
  if(!check(g,p->pack_size<=65536,"Packed uniforms exceed 64 KiB"))return NULL;
  if(members.count){[definition appendFormat:@"}\n@group(%uu) @binding(0u) var<uniform> cbpack_v : cbpack_t;\n",p->stage];NSTextCheckingResult *at=[draw_regex(@"\n(?:@\\w+|fn)\\s") firstMatchInString:text options:0 range:NSMakeRange(0,text.length)];[text insertString:[@"\n" stringByAppendingString:definition] atIndex:at?at.range.location:text.length];}
  p->vertex_id=[text rangeOfString:@"vertex_index"].location!=NSNotFound;p->instance_id=[text rangeOfString:@"instance_index"].location!=NSNotFound;
  if(p->stage==1){NSArray *out=draw_matches(@"@fragment\\s*fn\\s+main\\s*\\((?:[^()]|\\([^)]*\\))*\\)\\s*->\\s*([^{]+)\\{",text);if(out.count){NSString *type=draw_capture(text,out[0],1);NSArray *loc=draw_matches(@"@location\\((\\d+)u?\\)",type);if(!loc.count){NSArray *st=draw_matches([NSString stringWithFormat:@"struct\\s+%@\\s*\\{([^}]*)\\}",[type stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet]],text);if(st.count){type=draw_capture(text,st[0],1);loc=draw_matches(@"@location\\((\\d+)u?\\)",type);}}for(NSTextCheckingResult *m in loc){unsigned n=draw_capture(type,m,1).intValue;if(!check(g,n<8,"Fragment output location exceeds eight"))return NULL;p->outputs|=1u<<n;}}
   for(NSTextCheckingResult *m in draw_matches(@"@location\\((\\d+)u?\\)\\s*@interpolate\\(flat[^)]*\\)",text)){unsigned n=draw_capture(text,m,1).intValue;if(n<32)p->flat_inputs|=1u<<n;}
  }
  NSString *bindings=@"@group\\((\\d+)u?\\)\\s*@binding\\((\\d+)u?\\)\\s*var(?:<([^>]*)>)?\\s+(\\w+)\\s*:\\s*([^;]+);";
  for(NSTextCheckingResult *m in draw_matches(bindings,text)){unsigned group=draw_capture(text,m,1).intValue,binding=draw_capture(text,m,2).intValue;
   if(!check(g,group==p->stage&&p->count<DRAW_BINDINGS,"Unsupported shader binding group/count"))return NULL;DrawBinding *b=&p->bindings[p->count++];b->binding=binding;
   NSString *space=[m rangeAtIndex:3].location==NSNotFound?@"":draw_capture(text,m,3);NSString *type=[draw_capture(text,m,5) stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet];
   if([space isEqualToString:@"uniform"]){b->kind=1;if(!check(g,binding==0||binding==15,"Unexpected unpacked uniform binding"))return NULL;}
   else if([type isEqualToString:@"sampler"]||[type isEqualToString:@"sampler_comparison"]){b->kind=[type hasSuffix:@"comparison"]?3:2;b->slot=binding-16;if(!check(g,binding>=16&&binding<32,"Sampler binding outside ABI"))return NULL;}
   else if([type hasPrefix:@"texture_"]&&![type hasPrefix:@"texture_storage_"]){b->kind=4;b->slot=binding-32;if(!check(g,binding>=32&&binding<160,"Texture binding outside ABI"))return NULL;b->sample=[type containsString:@"<u32>"]?WGPUTextureSampleType_Uint:[type containsString:@"<i32>"]?WGPUTextureSampleType_Sint:[type hasPrefix:@"texture_depth_"]?WGPUTextureSampleType_Depth:WGPUTextureSampleType_Float;b->ms=[type containsString:@"multisampled_"];if(b->ms)b->sample=WGPUTextureSampleType_UnfilterableFloat;
    b->dim=[type containsString:@"cube_array"]?WGPUTextureViewDimension_CubeArray:[type containsString:@"cube"]?WGPUTextureViewDimension_Cube:[type containsString:@"2d_array"]?WGPUTextureViewDimension_2DArray:[type containsString:@"3d"]?WGPUTextureViewDimension_3D:[type containsString:@"1d"]?WGPUTextureViewDimension_1D:WGPUTextureViewDimension_2D;
   }else {fail(g,"Shader storage/proxy binding is not yet ported");return NULL;}
  }
  for(unsigned i=1;i<p->count;i++){DrawBinding v=p->bindings[i];unsigned j=i;while(j&&p->bindings[j-1].binding>v.binding){p->bindings[j]=p->bindings[j-1];j--;}p->bindings[j]=v;}
  WGPUBindGroupLayoutEntry entries[DRAW_BINDINGS];for(unsigned i=0;i<p->count;i++){DrawBinding *b=p->bindings+i;entries[i]=(WGPUBindGroupLayoutEntry)WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;entries[i].binding=b->binding;entries[i].visibility=p->stage?WGPUShaderStage_Fragment:WGPUShaderStage_Vertex;
   if(b->kind==1){entries[i].buffer.type=WGPUBufferBindingType_Uniform;entries[i].buffer.hasDynamicOffset=b->binding==0;entries[i].buffer.minBindingSize=b->binding==0?p->pack_size:16;}
   else if(b->kind==2||b->kind==3)entries[i].sampler.type=b->kind==3?WGPUSamplerBindingType_Comparison:WGPUSamplerBindingType_Filtering;
   else {entries[i].texture.sampleType=b->sample;entries[i].texture.viewDimension=b->dim;entries[i].texture.multisampled=b->ms;}
  }
  WGPUBindGroupLayoutDescriptor ld=WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;ld.entryCount=p->count;ld.entries=entries;p->layout=wgpuDeviceCreateBindGroupLayout(g->device,&ld);
  if([constants isKindOfClass:NSArray.class]){NSArray *values=constants;if(!check(g,values.count<=16384,"Special constants exceed 256 KiB"))return NULL;size_t bytes=values.count?values.count*16:16;uint32_t *data=calloc(1,bytes);if(!check(g,data!=NULL,"Special constant storage allocation"))return NULL;for(unsigned i=0;i<values.count;i++){uint64_t v;if(!draw_uint(values[i],&v)||v>UINT32_MAX){free(data);fail(g,"Invalid special constant table value");return NULL;}data[i*4]=(uint32_t)v;}WGPUBufferDescriptor bd=WGPU_BUFFER_DESCRIPTOR_INIT;bd.size=bytes;bd.usage=WGPUBufferUsage_Uniform|WGPUBufferUsage_CopyDst;p->constants=wgpuDeviceCreateBuffer(g->device,&bd);if(p->constants)wgpuQueueWriteBuffer(g->queue,p->constants,0,data,bytes);free(data);}
  p->source=strdup(text.UTF8String);if(!check(g,p->source!=NULL,"Shader source copy allocation"))return NULL;WGPUShaderSourceWGSL wgsl=WGPU_SHADER_SOURCE_WGSL_INIT;wgsl.code=(WGPUStringView){p->source,WGPU_STRLEN};WGPUShaderModuleDescriptor md=WGPU_SHADER_MODULE_DESCRIPTOR_INIT;md.nextInChain=&wgsl.chain;md.label=(WGPUStringView){p->hash,WGPU_STRLEN};p->module=wgpuDeviceCreateShaderModule(g->device,&md);
  if(!check(g,p->module&&p->layout&&!atomic_load(&g->failed),"Shader module/layout validation failed"))return NULL;
 }return p;
}
