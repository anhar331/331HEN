/* Host ISA compilation test only, not GPU execution. GPL-3.0-or-later. */
#include "ps5vk_compiler.h"
#include "vk_descriptor.h"
#include "owned_shader.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
int main(void){struct VkPipelineLayout_T layout={.set_count=1};for(unsigned b=0;b<PS5VK_MAX_BINDINGS;b++)layout.sets[0].binding[b].first=b<3?b:3;for(unsigned b=0;b<3;b++){layout.sets[0].binding[b].count=1;layout.sets[0].binding[b].stages=VK_SHADER_STAGE_COMPUTE_BIT;layout.sets[0].type[b]=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;}layout.sets[0].count=3;struct ps5vk_compiled_program p={0};uint32_t* code=0;VkResult r=ps5vk_runtime_compile_compute(owned_shader,sizeof(owned_shader)/4,"main",&layout,0,&p,&code);assert(r==VK_SUCCESS&&code&&p.code_words&&p.gfx==1013&&p.descriptor_count==3&&p.local_size[0]==64&&p.local_size[1]==1&&p.local_size[2]==1);printf("PASS: exact owned shader compiled to gfx1013; code_words=%zu descriptors=%u local_size=64,1,1; GPU execution not tested\n",p.code_words,p.descriptor_count);free(code);return 0;}
