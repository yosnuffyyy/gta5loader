/* Uses production structures/helpers without a GPU shortcut. Exhaustively
   alter each exact preparation input; eviction and dead handles cannot hit. */
static bool core_fixture(void){
 NativeGpu *g=calloc(1,sizeof(*g));if(!g)return false;g->core_v2=true;
 DrawProgram vp={0},pp={0};g->object_generation=9;g->draw.resource_epoch=7;g->draw.pipelines[0].pipeline=(WGPURenderPipeline)(uintptr_t)1;
 CorePipelineInputs input=core_pipeline_inputs(g,&vp,&pp);core_pipeline_remember(g,&input,&g->draw.pipelines[0]);bool ok=core_pipeline_hit(g,&input)!=NULL;
 unsigned cases=1;for(size_t i=0;i<sizeof(input);i++){CorePipelineInputs other=input;((unsigned char*)&other)[i]^=1;ok=ok&&core_pipeline_hit(g,&other)==NULL;cases++;}
 g->core_pipeline_epoch++;ok=ok&&!core_pipeline_hit(g,&input);cases++;g->core_pipeline_epoch--;
 g->draw.pipelines[0].pipeline=NULL;ok=ok&&!core_pipeline_hit(g,&input);cases++;
 /* Runtime specialization is part of identity, not captured startup constants. */
 for(unsigned mask=0;mask<32;mask++){pp.inactive_mask=mask;CorePipelineInputs active=core_pipeline_inputs(g,&vp,&pp);ok=ok&&(active.masks[1]==mask);cases++;}
 printf("%s: CPU core preparation contracts: %u exact-input, eviction, lifetime and specialization checks; no draw or GPU result fabricated.\n",ok?"PASS":"FAIL",cases);free(g);return ok;
}
