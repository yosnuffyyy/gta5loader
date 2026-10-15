/* Conservative proof over the pinned translator's structured uniform guards.
   Only a single direct compare-sample use of a depth cube can be removed;
   unknown syntax, aliases, extra uses, storage writes or complex blocks keep
   the original required binding and its normal validation failures. */
static NSUInteger shadow_close(NSString *s,NSUInteger start,unichar open,unichar close){
 unsigned level=0,comment=0;bool line=false;for(NSUInteger i=start;i<s.length;i++){unichar c=[s characterAtIndex:i],next=i+1<s.length?[s characterAtIndex:i+1]:0;
  if(line){if(c=='\n')line=false;continue;}if(comment){if(c=='/'&&next=='*'){comment++;i++;}else if(c=='*'&&next=='/'){comment--;i++;}continue;}
  if(c=='/'&&next=='/'){line=true;i++;continue;}if(c=='/'&&next=='*'){comment=1;i++;continue;}if(c==open)level++;else if(c==close&&!--level)return i;
 }return NSNotFound;
}
static void shadow_reflect(DrawProgram *p,NSString *text,NSArray *members){
 bool cube=false;for(unsigned i=0;i<p->count;i++)if(p->bindings[i].kind==4&&p->bindings[i].dim==WGPUTextureViewDimension_Cube&&p->bindings[i].sample==WGPUTextureSampleType_Depth)cube=true;if(!cube||p->stage!=1||[text containsString:@"//"]||[text containsString:@"/*"]||[text containsString:@"\""])return;for(unsigned i=0;i<p->count;i++)if(p->bindings[i].kind>=5)return;
 if(draw_matches(@"\\b(?:var|let|const)(?:<[^>]*>)?\\s+cbpack_v\\b",text).count!=1||draw_matches(@"\\bcbpack_v\\s*:",text).count!=1)return;
 NSString *pat=@"(r\\d+\\.[xyzw]) = bitcast<f32>\\(select\\(0u, 4294967295u, !\\(\\(cbpack_v\\.(\\w+)\\.(\\w+)\\[(\\d+)u?\\]\\.([xyzw]) == (0|3)\\.0f\\)\\)\\)\\);\\s*if \\(\\(bitcast<u32>\\(\\1\\) != 0u\\)\\) \\{";
 NSString *intPat=@"(r\\d+\\.[xyzw]) = bitcast<f32>\\(select\\(0u, 4294967295u, \\(([0-3])i (>=|<) bitcast<i32>\\(cbpack_v\\.(\\w+)\\.(\\w+)\\[(\\d+)u?\\]\\.([xyzw])\\)\\)\\)\\);\\s*if \\(\\(bitcast<u32>\\(\\1\\) != 0u\\)\\) \\{";
 struct {NSUInteger from,to;ShadowCondition condition;} regions[32];unsigned region_count=0;
 for(unsigned mode=0;mode<2;mode++)for(NSTextCheckingResult *m in draw_matches(mode?intPat:pat,text)){
  unsigned memberGroup=mode?4:2,fieldGroup=mode?5:3,rowGroup=mode?6:4,componentGroup=mode?7:5;
  NSDictionary *member=nil;for(NSDictionary *v in members)if([v[@"name"] isEqualToString:draw_capture(text,m,memberGroup)])member=v;if(!member)continue;
  NSString *fieldPattern=[NSString stringWithFormat:@"struct\\s+%@\\s*\\{\\s*%@\\s*:\\s*array<vec4<\\s*f32\\s*>,\\s*%uu?>\\s*,?\\s*\\}",[NSRegularExpression escapedPatternForString:member[@"type"]],[NSRegularExpression escapedPatternForString:draw_capture(text,m,fieldGroup)],[member[@"size"] unsignedIntValue]/16];if(draw_matches(fieldPattern,text).count!=1)continue;
  uint64_t row=draw_capture(text,m,rowGroup).longLongValue;if(row>=([member[@"size"] unsignedIntValue]/16))continue;uint64_t word=row*4ull+[@"xyzw" rangeOfString:draw_capture(text,m,componentGroup)].location;if(word>=([member[@"size"] unsignedIntValue]/4))continue;
  NSUInteger from=NSMaxRange(m.range)-1,to=shadow_close(text,from,'{','}');if(to==NSNotFound)continue;
  if(region_count==32)break;ShadowCondition c={.slot=[member[@"slot"] unsignedIntValue],.word=(uint32_t)word,.equals=draw_capture(text,m,mode?2:6).floatValue,.true_arm=true,.predicate=mode?([draw_capture(text,m,3) isEqualToString:@">="]?SHADOW_INT_LE:SHADOW_INT_GT):SHADOW_FLOAT_NE};regions[region_count].from=from;regions[region_count].to=to;regions[region_count++].condition=c;
  NSUInteger at=to+1;while(at<text.length&&[NSCharacterSet.whitespaceAndNewlineCharacterSet characterIsMember:[text characterAtIndex:at]])at++;
  if(at+4<=text.length&&[[text substringWithRange:NSMakeRange(at,4)] isEqualToString:@"else"]){at+=4;while(at<text.length&&[NSCharacterSet.whitespaceAndNewlineCharacterSet characterIsMember:[text characterAtIndex:at]])at++;if(at<text.length&&[text characterAtIndex:at]=='{'&&region_count<32){NSUInteger end=shadow_close(text,at,'{','}');if(end!=NSNotFound){c.true_arm=false;regions[region_count].from=at;regions[region_count].to=end;regions[region_count++].condition=c;}}}
 }
 ShadowPlan plan={0};for(unsigned i=0;i<p->count&&plan.count<SHADOW_GUARDS;i++){DrawBinding *b=p->bindings+i;if(b->kind!=4||b->dim!=WGPUTextureViewDimension_Cube||b->sample!=WGPUTextureSampleType_Depth||b->ms)continue;
  NSArray *decl=draw_matches([NSString stringWithFormat:@"@group\\(1u?\\)\\s*@binding\\(%uu?\\)\\s*var\\s+(\\w+)\\s*:\\s*texture_depth_cube\\s*;",b->binding],text);if(decl.count!=1)continue;NSTextCheckingResult *d=decl[0];NSString *name=draw_capture(text,d,1);
  NSArray *uses=draw_matches([NSString stringWithFormat:@"\\b%@\\b",[NSRegularExpression escapedPatternForString:name]],text);if(uses.count!=2)continue;NSTextCheckingResult *use=uses[1];if(use.range.location<NSMaxRange(d.range))continue;
  NSString *callPrefix=[@"textureSampleCompareLevel(" stringByAppendingString:name];NSRange call=[text rangeOfString:callPrefix options:NSBackwardsSearch range:NSMakeRange(0,NSMaxRange(use.range))];if(call.location==NSNotFound||call.location+call.length!=NSMaxRange(use.range))continue;
  if(call.location){unichar before=[text characterAtIndex:call.location-1];if(before=='_'||[NSCharacterSet.alphanumericCharacterSet characterIsMember:before])continue;}
  NSUInteger open=call.location+[@"textureSampleCompareLevel" length],end=shadow_close(text,open,'(',')');if(end==NSNotFound)continue;
  ShadowGuard guard={.binding=b->binding,.decl_start=(uint32_t)d.range.location,.decl_length=(uint32_t)d.range.length,.call_start=(uint32_t)call.location,.call_length=(uint32_t)(end+1-call.location)};
  for(unsigned j=0;j<region_count&&guard.count<SHADOW_CONDITIONS;j++)if(regions[j].from<call.location&&regions[j].to>end&&(regions[j].condition.true_arm||regions[j].condition.predicate!=SHADOW_FLOAT_NE))guard.conditions[guard.count++]=regions[j].condition;
  if(guard.count)plan.guards[plan.count++]=guard;
 }
 if(plan.count){p->shadow_plan=malloc(sizeof(plan));if(p->shadow_plan)*p->shadow_plan=plan;}
}
