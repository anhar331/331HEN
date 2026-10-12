/* 331 owned-image GPU diagnostic. Copyright (C) 2026 ANHAR. GPL-3.0-or-later.
 * No game capture, no optical flow, no VideoOut takeover and no HEN changes. */
#include <vulkan/vulkan_core.h>
#include "owned_reference.h"
#include "owned_shader.h"
#include "ps5log.h"
#include "control.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
static uint64_t now_ns(void){struct timespec t={0};clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000000000u+(uint64_t)t.tv_nsec;}
static void retain(const char* reason){ps5log_line(PS5LOG_ERR,reason);ps5log_close("Resources retained until system Close Game; no unsafe in-flight free");for(;;)sleep(1);}
static int stop(void){struct stat st;errno=0;if(lstat("/data/331FrameGen/STOP",&st)==0)return 1;return errno!=ENOENT&&errno!=ENOTDIR;}
#define CHECK(call) do{VkResult r=(call);if(r!=VK_SUCCESS){ps5log_printf(PS5LOG_ERR,"FAIL operation=%s result=%d",#call,r);goto cleanup;}}while(0)
int main(void){
 if(fg_log_open())_exit(0);
 ps5log_line(PS5LOG_MARK,"331_FRAMEGEN_DIAGNOSTIC version=0.2 source=owned-synthetic-images game_capture=0 presentation=0 frame_generation=0");
 const char *enable_paths[2]={"/app0/ENABLE_GPU_TEST","/data/homebrew/PPSA99993-app/ENABLE_GPU_TEST"};
 struct fg_control_result control=fg_control_check("/data/331FrameGen/STOP",enable_paths[0],enable_paths[1],lstat);
 ps5log_printf(PS5LOG_MARK,"CONTROL_STOP state=%d errno=%d",control.state,control.stop_error);
 if(control.state==FG_CONTROL_STOPPED||control.state==FG_CONTROL_STOP_ERROR){ps5log_close("DISABLED: STOP present or STOP lookup failed; no GPU initialization");for(;;)sleep(1);}
 for(unsigned i=0;i<2;i++)ps5log_printf(PS5LOG_MARK,"CONTROL_ENABLE path=%s result=%d errno=%d regular_empty=%d",enable_paths[i],control.markers[i].result,control.markers[i].error,control.markers[i].valid);
 if(control.state!=FG_CONTROL_ENABLED){ps5log_close("DISABLED: no regular empty enable marker in either known application path");for(;;)sleep(1);}
 ps5log_printf(PS5LOG_MARK,"CONTROL_ENABLED marker=%s",control.selected_path);
 VkInstance instance=0;VkDevice device=0;VkShaderModule shader=0;VkDescriptorSetLayout sl=0;VkPipelineLayout layout=0;VkPipeline pipeline=0;VkDescriptorPool dp=0;VkCommandPool cp=0;VkFence fence=0;
 VkBuffer buffers[3]={0};VkDeviceMemory memories[3]={0};uint32_t* mapped[3]={0};VkMappedMemoryRange ranges[3]={{0}};int pending=0,success=0;
 VkInstanceCreateInfo ici={.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};CHECK(vkCreateInstance(&ici,0,&instance));
 uint32_t count=1;VkPhysicalDevice physical=0;CHECK(vkEnumeratePhysicalDevices(instance,&count,&physical));if(count!=1)goto cleanup;
 float priority=1;VkDeviceQueueCreateInfo qci={.sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,.queueCount=1,.pQueuePriorities=&priority};VkDeviceCreateInfo dci={.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,.queueCreateInfoCount=1,.pQueueCreateInfos=&qci};CHECK(vkCreateDevice(physical,&dci,0,&device));VkQueue queue;vkGetDeviceQueue(device,0,0,&queue);
 VkDescriptorSetLayoutBinding bindings[3]={{0}};for(unsigned i=0;i<3;i++)bindings[i]=(VkDescriptorSetLayoutBinding){.binding=i,.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,.descriptorCount=1,.stageFlags=VK_SHADER_STAGE_COMPUTE_BIT};
 VkDescriptorSetLayoutCreateInfo slci={.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,.bindingCount=3,.pBindings=bindings};CHECK(vkCreateDescriptorSetLayout(device,&slci,0,&sl));
 VkPipelineLayoutCreateInfo plci={.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,.setLayoutCount=1,.pSetLayouts=&sl};CHECK(vkCreatePipelineLayout(device,&plci,0,&layout));
 VkShaderModuleCreateInfo smci={.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,.codeSize=sizeof(owned_shader),.pCode=owned_shader};CHECK(vkCreateShaderModule(device,&smci,0,&shader));
 VkComputePipelineCreateInfo pci={.sType=VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,.layout=layout,.stage={.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,.stage=VK_SHADER_STAGE_COMPUTE_BIT,.module=shader,.pName="main"}};uint64_t compile_start=now_ns();CHECK(vkCreateComputePipelines(device,0,1,&pci,0,&pipeline));ps5log_printf(PS5LOG_MARK,"PIPELINE_CREATED cpu_compile_elapsed_ns=%llu",(unsigned long long)(now_ns()-compile_start));
 VkDescriptorPoolSize dps={VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,3};VkDescriptorPoolCreateInfo dpci={.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,.maxSets=1,.poolSizeCount=1,.pPoolSizes=&dps};CHECK(vkCreateDescriptorPool(device,&dpci,0,&dp));VkDescriptorSet set;VkDescriptorSetAllocateInfo dsai={.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,.descriptorPool=dp,.descriptorSetCount=1,.pSetLayouts=&sl};CHECK(vkAllocateDescriptorSets(device,&dsai,&set));
 VkDescriptorBufferInfo infos[3];VkWriteDescriptorSet writes[3];
 for(unsigned b=0;b<3;b++){
  VkBufferCreateInfo bci={.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,.size=FG_BUFFER_BYTES,.usage=VK_BUFFER_USAGE_STORAGE_BUFFER_BIT};CHECK(vkCreateBuffer(device,&bci,0,&buffers[b]));VkMemoryRequirements req;vkGetBufferMemoryRequirements(device,buffers[b],&req);if(!req.alignment||256%req.alignment||req.size>FG_MEMORY_BYTES-256){ps5log_line(PS5LOG_ERR,"Unsupported buffer alignment/size");goto cleanup;}
  VkMemoryAllocateInfo mai={.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,.allocationSize=FG_MEMORY_BYTES};CHECK(vkAllocateMemory(device,&mai,0,&memories[b]));CHECK(vkBindBufferMemory(device,buffers[b],memories[b],256));CHECK(vkMapMemory(device,memories[b],0,VK_WHOLE_SIZE,0,(void**)&mapped[b]));
  for(unsigned i=0;i<FG_MEMORY_BYTES/4;i++)mapped[b][i]=0xcdcdcdcdu;
  for(unsigned i=0;i<FG_PIXELS;i++)mapped[b][FG_GUARD+i]=b<2?fg_pixel(i,b):0xdeadbeefu;
  ranges[b]=(VkMappedMemoryRange){.sType=VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,.memory=memories[b],.size=VK_WHOLE_SIZE};infos[b]=(VkDescriptorBufferInfo){.buffer=buffers[b],.range=FG_BUFFER_BYTES};writes[b]=(VkWriteDescriptorSet){.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,.dstSet=set,.dstBinding=b,.descriptorCount=1,.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,.pBufferInfo=&infos[b]};
 }
 CHECK(vkFlushMappedMemoryRanges(device,3,ranges));vkUpdateDescriptorSets(device,3,writes,0,0);
 VkCommandPoolCreateInfo cpci={.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};CHECK(vkCreateCommandPool(device,&cpci,0,&cp));VkCommandBuffer cb;VkCommandBufferAllocateInfo cbai={.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,.commandPool=cp,.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY,.commandBufferCount=1};CHECK(vkAllocateCommandBuffers(device,&cbai,&cb));VkCommandBufferBeginInfo cbi={.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};CHECK(vkBeginCommandBuffer(cb,&cbi));
 VkMemoryBarrier before={.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER,.srcAccessMask=VK_ACCESS_HOST_WRITE_BIT,.dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT};vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&before,0,0,0,0);vkCmdBindPipeline(cb,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline);vkCmdBindDescriptorSets(cb,VK_PIPELINE_BIND_POINT_COMPUTE,layout,0,1,&set,0,0);vkCmdDispatch(cb,16,1,1);
 VkMemoryBarrier after={.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER,.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT,.dstAccessMask=VK_ACCESS_HOST_READ_BIT};vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&after,0,0,0,0);CHECK(vkEndCommandBuffer(cb));VkFenceCreateInfo fci={.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};CHECK(vkCreateFence(device,&fci,0,&fence));
 if(stop()){ps5log_line(PS5LOG_MARK,"STOP before submit");goto cleanup;}
 VkSubmitInfo submit={.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,.commandBufferCount=1,.pCommandBuffers=&cb};uint64_t submit_start=now_ns();pending=1;CHECK(vkQueueSubmit(queue,1,&submit,fence));CHECK(vkWaitForFences(device,1,&fence,VK_TRUE,5000000000ULL));pending=0;uint64_t finished=now_ns();CHECK(vkInvalidateMappedMemoryRanges(device,3,ranges));
 if(!fg_verify(mapped[0]+FG_GUARD,mapped[1]+FG_GUARD,mapped[2]+FG_GUARD)){ps5log_line(PS5LOG_ERR,"READBACK_MISMATCH");goto cleanup;}
 for(unsigned b=0;b<3;b++)for(unsigned i=0;i<FG_MEMORY_BYTES/4;i++)if((i<FG_GUARD||i>=FG_GUARD+FG_PIXELS)&&mapped[b][i]!=0xcdcdcdcdu){ps5log_line(PS5LOG_ERR,"GUARD_MISMATCH");goto cleanup;}
 success=1;ps5log_printf(PS5LOG_MARK,"OWNED_BLEND_READBACK_PASS pixels=1024 input_unchanged=1 guards=1 cpu_submit_to_fence_ns=%llu gpu_timestamp=0 displayed_frames=0 game_frames=0",(unsigned long long)(finished-submit_start));
 cleanup:
 if(pending)retain("SUBMIT_OWNERSHIP_UNKNOWN: no resource release");
 if(device){if(fence)vkDestroyFence(device,fence,0);if(cp)vkDestroyCommandPool(device,cp,0);if(dp)vkDestroyDescriptorPool(device,dp,0);for(unsigned i=0;i<3;i++){if(mapped[i])vkUnmapMemory(device,memories[i]);if(buffers[i])vkDestroyBuffer(device,buffers[i],0);if(memories[i])vkFreeMemory(device,memories[i],0);}if(pipeline)vkDestroyPipeline(device,pipeline,0);if(shader)vkDestroyShaderModule(device,shader,0);if(layout)vkDestroyPipelineLayout(device,layout,0);if(sl)vkDestroyDescriptorSetLayout(device,sl,0);vkDestroyDevice(device,0);}if(instance)vkDestroyInstance(instance,0);ps5log_printf(PS5LOG_MARK,"DIAGNOSTIC_FINISHED readback_pass=%d hardware_result_not_assumed=1",success);ps5log_close("Close this diagnostic with system Close Game");for(;;)sleep(1);
}
