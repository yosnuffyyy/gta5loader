#include "native_launch.h"
#include "native_host.h"
#include "threading/native_threads.h"
#include "asset_io/native_assets.h"
#include "wasm-rt-impl.h"
#include "wasm-rt-exceptions.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

static int launch(const char *game,const char *saves,void *layer,bool checkpoint,native_progress progress,void *context,char *report,size_t capacity){
 wasm_rt_init();struct w2c_env *h=calloc(1,sizeof(*h));w2c_gta *engine=calloc(1,sizeof(*engine));
 if(!h||!engine){free(h);free(engine);snprintf(report,capacity,"FAIL: native launch allocation");return 1;}
 host_init(h);h->engine=engine;h->progress=progress;h->progress_context=context;struct w2c_wasi__snapshot__preview1 wasi={.env=h};
 volatile bool allocated=false,instantiated=false;NativeThreads *volatile threads=NULL;NativeAssets *volatile assets=NULL;
 int result=1;wasm_rt_trap_t trap=wasm_rt_impl_try();
 if(!trap){
  if(!game||!saves)host_unavailable(h,"select the extracted web folder for native startup");
  h->game_root=open(game,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);h->save_root=open(saves,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);
  if(h->game_root<0||h->save_root<0)host_unavailable(h,"native launch game/save folder unavailable");
  threads=native_threads_begin(h);wasm_rt_allocate_memory_shared(&h->memory,8192,65536,false,65536);allocated=true;wasm2c_gta_instantiate(engine,h,&wasi);instantiated=true;
  if(progress)progress(context,"Starting actual engine constructors for native graphics startup…");
  w2c_gta_0x5F_wasm_call_ctors(engine);
  assets=native_assets_begin(h);native_set_assets(assets);native_boot_prepare(h,layer,checkpoint);
  if(progress)progress(context,checkpoint?"Entering actual graphics-enabled main; native GPU backend and first archive boundary…":"Entering actual graphics-enabled main; reading real archives. Scene integration is experimental.");
  native_main_scene(h);
 }
 char summary[4096];bool reached=native_boot_finish(h,summary,sizeof(summary));
 if(checkpoint&&reached){snprintf(report,capacity,"PASS: actual graphics-enabled engine main handed its shared command ring to native Metal, published input/audio and reached its first archive request. Stopped deliberately before archive bytes.\n%s\nThis is an integration checkpoint, not a game scene.",summary);result=0;}
 else if(!checkpoint&&reached){snprintf(report,capacity,"Native startup stopped by user.\n%s\nA presentation count alone does not verify an actual game scene. Gameplay, audio output and controls are still pending.",summary);result=0;}
 else snprintf(report,capacity,"Native startup stopped (trap %d): %s\n%s\nStage 3 remains in progress; this run has not been visually verified. Prior build: about three seconds of 3D reported on iPhone; continuous rendering and controls remain pending.",trap,h->failure?h->failure:"startup services unavailable",summary);
 native_threads_end(threads);native_assets_end(assets);host_close(h);
 if(instantiated)wasm2c_gta_free(engine);if(allocated){wasm_rt_free_memory_shared(&h->memory);pthread_mutex_destroy(&h->memory.mem_lock);}
 free(engine);free(h);wasm_rt_free_thread();return result;
}
int native_scene_probe(const char *game,const char *saves,void *layer,native_progress progress,void *context,char *report,size_t capacity){return launch(game,saves,layer,false,progress,context,report,capacity);}
int native_graphics_checkpoint(const char *game,const char *saves,native_progress progress,void *context,char *report,size_t capacity){return launch(game,saves,NULL,true,progress,context,report,capacity);}
