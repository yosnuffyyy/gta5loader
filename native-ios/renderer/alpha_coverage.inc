/* D3D11 does not specify intermediate alpha quantization. Match the pinned
   browser reference's single-sample threshold, with the specified NaN=zero
   rule. MSAA remains native. No draw, color component or shader body is lost.
   Operate on the final entry return, after all original fragment operations. */
static NSString *alpha_coverage_source(NativeGpu *g,NSString *source,bool *changed){
 *changed=false;
 NSArray *entry=draw_matches(@"@fragment\\s*fn\\s+main\\s*\\((?:[^()]|\\([^)]*\\))*\\)\\s*->\\s*([^\\{]+)\\{",source);
 if(!check(g,entry.count==1,"Alpha coverage requires one reflected fragment entry"))return nil;
 NSTextCheckingResult *m=entry[0];NSString *type=[draw_capture(source,m,1) stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet];
 NSString *alpha=nil;NSString *result=type;
 if(![type containsString:@"@"]){NSArray *st=draw_matches([NSString stringWithFormat:@"struct\\s+%@\\s*\\{([^}]*)\\}",[NSRegularExpression escapedPatternForString:type]],source);if(!check(g,st.count==1,"Alpha coverage result structure missing"))return nil;result=draw_capture(source,st[0],1);}
 /* SV_Coverage overrides A2C, including when it happens to be all ones. */
 if([result rangeOfString:@"@builtin(sample_mask)"].location!=NSNotFound)return source;
 NSArray *loc=draw_matches(@"@location\\(0u?\\)\\s*(?:@\\w+\\([^)]*\\)\\s*)?(?:(\\w+)\\s*:\\s*)?(vec4<f32>|vec4f)",result);
 if(!check(g,loc.count==1,"Alpha coverage requires float4 SV_Target0"))return nil;
 NSTextCheckingResult *field=loc[0];alpha=[field rangeAtIndex:1].location==NSNotFound?@"native_a2c_result.w":[NSString stringWithFormat:@"native_a2c_result.%@.w",draw_capture(result,field,1)];
 NSUInteger begin=NSMaxRange(m.range),end=begin;unsigned braces=1;
 for(;end<source.length&&braces;end++){unichar c=[source characterAtIndex:end];if(c=='{')braces++;else if(c=='}')braces--;}
 if(!check(g,!braces&&[source rangeOfString:@"native_a2c_result"].location==NSNotFound,"Alpha coverage entry shape or reserved name invalid"))return nil;
 NSRange body=NSMakeRange(begin,end-begin-1);NSString *original=[source substringWithRange:body];NSArray *returns=draw_matches(@"\\breturn\\s+([^;]+);",original);
 if(!check(g,returns.count>0,"Alpha coverage fragment has no value return"))return nil;
 NSMutableString *patched=[original mutableCopy];
 for(NSTextCheckingResult *r in [returns reverseObjectEnumerator]){NSString *text=[NSString stringWithFormat:@"{ let native_a2c_result = %@; if (!(%@ >= 0.5f)) { discard; } return native_a2c_result; }",draw_capture(original,r,1),alpha];[patched replaceCharactersInRange:r.range withString:text];}
 NSMutableString *text=[source mutableCopy];[text replaceCharactersInRange:body withString:patched];*changed=true;return text;
}
static WGPUShaderModule alpha_coverage_module(NativeGpu *g,DrawProgram *p,NSString *source){
 bool changed=false;NSString *text=alpha_coverage_source(g,source,&changed);if(!text)return NULL;
 if(!changed)return NULL; /* caller tests failed separately; explicit coverage */
 WGPUShaderSourceWGSL code=WGPU_SHADER_SOURCE_WGSL_INIT;code.code=(WGPUStringView){text.UTF8String,WGPU_STRLEN};WGPUShaderModuleDescriptor md=WGPU_SHADER_MODULE_DESCRIPTOR_INIT;md.nextInChain=&code.chain;md.label=(WGPUStringView){p->hash,WGPU_STRLEN};return wgpuDeviceCreateShaderModule(g->device,&md);
}
