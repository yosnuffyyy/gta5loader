#include "inter_stage_layout.h"
typedef struct {WGPUShaderModule vertex,fragment;} LinkedModules;
/* Reflect only the VS result and PS main parameters. Never relabel vertex
   inputs, fragment outputs, buffers/textures, builtin fields, or private code. */
static NSRange inter_stage_range(NSString *text,bool vertex){
 NSString *pattern=vertex?@"@vertex\\s*fn\\s+main\\s*\\((?:[^()]|\\([^)]*\\))*\\)\\s*->\\s*([^\\{]+)\\{":@"@fragment\\s*fn\\s+main\\s*\\(((?:[^()]|\\([^)]*\\))*)\\)\\s*->";
 NSArray *matches=draw_matches(pattern,text);if(matches.count!=1)return NSMakeRange(NSNotFound,0);NSTextCheckingResult *m=matches[0];
 if(!vertex)return [m rangeAtIndex:1];NSString *type=[draw_capture(text,m,1) stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet];
 if([type containsString:@"@builtin"])return [m rangeAtIndex:1];
 NSArray *structs=draw_matches([NSString stringWithFormat:@"struct\\s+%@\\s*\\{([^}]*)\\}",[NSRegularExpression escapedPatternForString:type]],text);
 return structs.count==1?[structs[0] rangeAtIndex:1]:NSMakeRange(NSNotFound,0);
}
static bool inter_stage_mask(NativeGpu *g,NSString *text,NSRange range,uint32_t *mask){
 if(!check(g,range.location!=NSNotFound,"Inter-stage entry interface reflection failed"))return false;*mask=0;NSString *body=[text substringWithRange:range];
 for(NSTextCheckingResult *m in draw_matches(@"@location\\((\\d+)u?\\)",body)){unsigned n=draw_capture(body,m,1).intValue;if(!check(g,n<32&&!(*mask&(1u<<n)),"Inter-stage location exceeds ABI or is duplicated"))return false;*mask|=1u<<n;}return true;
}
static NSString *inter_stage_relabel(NSString *source,NSRange range,const InterStagePlan *p){
 NSMutableString *body=[[source substringWithRange:range] mutableCopy];for(NSTextCheckingResult *m in [draw_matches(@"@location\\((\\d+)(u?)\\)",body) reverseObjectEnumerator]){unsigned n=draw_capture(body,m,1).intValue;if(n<32&&(p->changed&(1u<<n)))[body replaceCharactersInRange:m.range withString:[NSString stringWithFormat:@"@location(%u%@)",p->location[n],draw_capture(body,m,2)]];}
 NSMutableString *text=[source mutableCopy];[text replaceCharactersInRange:range withString:body];return text;
}
static bool draw_link_modules(NativeGpu *g,NSString *vs,NSString *ps,uint32_t flat,LinkedModules *out){
 *out=(LinkedModules){0};NSRange vr=inter_stage_range(vs,true),fr=ps?inter_stage_range(ps,false):NSMakeRange(0,0);uint32_t vmask=0,fmask=0;
 if(!inter_stage_mask(g,vs,vr,&vmask)||(ps&&!inter_stage_mask(g,ps,fr,&fmask)))return false;
 InterStagePlan plan;if(!inter_stage_plan(vmask,fmask,g->inter_stage_limit,&plan)){char why[220];snprintf(why,sizeof(why),"Inter-stage interface cannot fit or is missing required outputs: VS mask 0x%08x PS mask 0x%08x device limit %u",vmask,fmask,g->inter_stage_limit);fail(g,why);return false;}
 NSString *vertex=vs,*fragment=ps;
 if(flat){NSMutableString *body=[[vs substringWithRange:vr] mutableCopy];for(NSTextCheckingResult *m in [draw_matches(@"@location\\((\\d+)u?\\)(\\s*\\n\\s*\\w+\\s*:)",body) reverseObjectEnumerator]){unsigned n=draw_capture(body,m,1).intValue;if(n<32&&(flat&(1u<<n)))[body replaceCharactersInRange:m.range withString:[NSString stringWithFormat:@"@location(%uu) @interpolate(flat)%@",n,draw_capture(body,m,2)]];}NSMutableString *text=[vs mutableCopy];[text replaceCharactersInRange:vr withString:body];vertex=text;vr=inter_stage_range(vertex,true);}
 if(plan.changed){vertex=inter_stage_relabel(vertex,vr,&plan);if(ps&&(plan.changed&fmask))fragment=inter_stage_relabel(ps,fr,&plan);}
 if(![vertex isEqualToString:vs]){WGPUShaderSourceWGSL src=WGPU_SHADER_SOURCE_WGSL_INIT;src.code=(WGPUStringView){vertex.UTF8String,WGPU_STRLEN};WGPUShaderModuleDescriptor md=WGPU_SHADER_MODULE_DESCRIPTOR_INIT;md.nextInChain=&src.chain;md.label=(WGPUStringView){"linked vertex varying locations",WGPU_STRLEN};out->vertex=wgpuDeviceCreateShaderModule(g->device,&md);if(!out->vertex||atomic_load(&g->failed))return false;}
 if(ps&&![fragment isEqualToString:ps]){WGPUShaderSourceWGSL src=WGPU_SHADER_SOURCE_WGSL_INIT;src.code=(WGPUStringView){fragment.UTF8String,WGPU_STRLEN};WGPUShaderModuleDescriptor md=WGPU_SHADER_MODULE_DESCRIPTOR_INIT;md.nextInChain=&src.chain;md.label=(WGPUStringView){"linked fragment varying locations",WGPU_STRLEN};out->fragment=wgpuDeviceCreateShaderModule(g->device,&md);if(!out->fragment||atomic_load(&g->failed))return false;}
 if(plan.changed){g->draw.inter_stage_links++;if(g->host->progress&&g->draw.inter_stage_links<=6){char line[240];snprintf(line,sizeof(line),"Native inter-stage link: negotiated limit %u; VS/PS locations 0x%08x/0x%08x; relabeled high locations 0x%08x. Types/interpolation/builtins and all outputs retained.",g->inter_stage_limit,vmask,fmask,plan.changed);g->host->progress(g->host->progress_context,line);}}return !atomic_load(&g->failed);
}
static void linked_modules_free(LinkedModules *m){if(m->vertex)wgpuShaderModuleRelease(m->vertex);if(m->fragment)wgpuShaderModuleRelease(m->fragment);*m=(LinkedModules){0};}
