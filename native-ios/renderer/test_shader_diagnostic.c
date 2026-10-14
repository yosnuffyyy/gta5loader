#include "shader_diagnostic.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 uint32_t small[2]={1,0},shader[5]={1,0,0x12345678,0xabcdef09,0};
 assert(shader_diagnostic_word(16,2,small,3)==0);assert(shader_diagnostic_word(14,2,small,3)==0);assert(shader_diagnostic_word(14,5,NULL,3)==0);assert(shader_diagnostic_word(14,5,shader,5)==0);assert(shader_diagnostic_word(14,5,shader,2)==0x12345678);assert(shader_diagnostic_word(14,5,shader,3)==0xabcdef09);
 puts("PASS: fatal shader diagnostics reject wrong-kind/truncated/missing descriptors without out-of-bounds reads; valid hashes preserved.");return 0;
}
