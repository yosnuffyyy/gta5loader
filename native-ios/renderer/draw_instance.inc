/* Slow path only for indirect commands that need D3D ID/divisor rebasing.
   Read 16/20 bytes with the existing bounded staging buffer; normal indirect
   commands remain entirely on the GPU. No vertex data is copied. */
static bool draw_indirect_decode(NativeGpu *g,const uint8_t *payload,unsigned kind,uint32_t args[5]){
 Object *o=object(g,gpu_word(payload,1));uint32_t offset=gpu_word(payload,2),bytes=kind==4?20:16;
 if(!check(g,o&&o->buffer&&(o->misc&0x10)&&offset%4==0&&offset<=o->size&&bytes<=o->size-offset,"Invalid indirect draw arguments"))return false;
 if(!draw_flush(g)||!readback_buffer(g,bytes))return false;WGPUCommandEncoder e=encoder(g);
 wgpuCommandEncoderCopyBufferToBuffer(e,o->buffer,offset,g->readback_buffer,0,bytes);const uint8_t *data=readback_map(g,e,bytes);
 bool ok=data!=NULL;if(ok){memcpy(args,data,bytes);g->draw.indirect_decodes++;}readback_unmap(g);return ok;
}
