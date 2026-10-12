/* GPL-3.0-or-later. Host orchestration doubles; no PS5 execution. */
#include "native_display.h"
#include "result_scanout.h"
#include "command_arena_ps5.h"
#include "color_detile.h"
#include "ps5_videoout.h"
#include "ps5_agc.h"
#include "ps5_agc_driver.h"
#include "ps5log.h"
#include <assert.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
static jmp_buf park;
enum {OK,ALLOC_FAIL,MAP_FAIL,OPEN_FAIL,QUEUE_FAIL,REGISTER_FAIL,
      BUSY,UNREGISTER_FAIL,SUBMIT_FAIL,SUSPEND_FAIL,FENCE_TIMEOUT,
      WRONG_TOKEN,DECODE_FAIL,EVENT_TIMEOUT,GUARD_FAIL,CLOSE_FAIL,SOURCE_FAIL};
static int scenario,stop_at,stop_calls,submits,presents,closes,unmaps,releases,retained;
static uint64_t now,token;
static void *surface,*command;
static volatile uint64_t *fence;
static unsigned slot;
static uint64_t surface_digest;
static uint32_t input_a[FG_PIXELS], input_b[FG_PIXELS], gpu_output[FG_PIXELS];
static struct fg_result_view view={input_a,input_b,gpu_output,FG_PIXELS};
static unsigned allocations;
static uint64_t digest(void *p,size_t n){unsigned char*b=p;uint64_t d=0;for(size_t i=0;i<n;i++)d=d*131+b[i];return d;}
static uint64_t images_digest(void){size_t n=ps5vk_color_64k_rx_surface_size(4,1920,1080)+256;return digest(surface,n)^digest((char*)surface+PS5_SURFACE_BUFFER_STRIDE,n);}
int clock_gettime(clockid_t id,struct timespec*t){(void)id;now+=UINT64_C(1000000000);t->tv_sec=now/1000000000;t->tv_nsec=0;return 0;}
unsigned sleep(unsigned seconds){(void)seconds;return 0;}
int usleep(useconds_t u){(void)u;return 0;}
void ps5log_printf(int l,const char*f,...){(void)l;(void)f;}
void ps5log_line(int l,const char*s){(void)l;(void)s;}
void ps5log_close(const char*s){assert(strstr(s,"Unknown"));retained=1;longjmp(park,1);}
static int stop(void){return stop_at&&++stop_calls>=stop_at;}
int sceKernelAllocateMainDirectMemory(size_t n,size_t a,int type,int64_t*off){allocations++;assert(n==PS5_SURFACE_ALLOCATION_BYTES&&a==PS5_SURFACE_ALIGNMENT&&type==12);if(scenario==ALLOC_FAIL)return -1;assert(!posix_memalign(&surface,a,n));*off=123;return 0;}
int sceKernelMapDirectMemory(void**p,size_t n,int prot,int flags,int64_t off,size_t a){assert(n==PS5_SURFACE_ALLOCATION_BYTES&&prot==0x33&&!flags&&off==123&&a==PS5_SURFACE_ALIGNMENT);if(scenario==MAP_FAIL)return -1;*p=surface;return 0;}
int sceKernelMunmap(void*p,size_t n){assert(p==surface&&n==PS5_SURFACE_ALLOCATION_BYTES);assert(scenario==OPEN_FAIL||closes==1||!presents);unmaps++;return 0;}
int sceKernelReleaseDirectMemory(int64_t o,size_t n){assert(o==123&&n==PS5_SURFACE_ALLOCATION_BYTES&&unmaps==1);releases++;free(surface);surface=NULL;return 0;}
VkResult ps5vk_command_arena_create(struct ps5vk_command_arena*a){assert(!posix_memalign(&command,65536,PS5VK_COMMAND_ARENA_BYTES));memset(command,0,PS5VK_COMMAND_ARENA_BYTES);a->address=command;fence=(volatile uint64_t*)((char*)command+PS5VK_COMMAND_ARENA_BYTES-64);return VK_SUCCESS;}
VkResult ps5vk_command_arena_release(struct ps5vk_command_arena*a){if(a->address){assert(!presents||closes==1);free(command);command=NULL;a->address=NULL;}return VK_SUCCESS;}
volatile uint64_t*ps5vk_command_arena_label(struct ps5vk_command_arena*a){assert(a->address==command);return fence;}
int sceVideoOutOpen(int32_t u,int32_t b,int32_t i,const void*p){assert(u==255&&!b&&!i&&!p);return scenario==OPEN_FAIL?-1:7;}
int sceVideoOutClose(int32_t h){assert(h==7);if(scenario==CLOSE_FAIL)return -1;closes++;return 0;}
int sceVideoOutSetFlipRate(int32_t h,int32_t r){assert(h==7&&r==0);return 0;}
int sceKernelCreateEqueue(void**q,const char*n){assert(n);if(scenario==QUEUE_FAIL)return -1;*q=(void*)1;return 0;}
int sceKernelDeleteEqueue(void*q){assert(q==(void*)1);return 0;}
int sceVideoOutAddFlipEvent(void*q,int32_t h,void*u){assert(q==(void*)1&&h==7&&!u);return 0;}
int sceVideoOutDeleteFlipEvent(void*q,int32_t h){assert(q==(void*)1&&h==7);return 0;}
void sceVideoOutSetBufferAttribute2(void*a,uint64_t f,uint32_t t,uint32_t w,uint32_t h,uint64_t o,uint32_t r,uint64_t r1){assert(a&&f==UINT64_C(0x8000000000000000)&&!t&&w==1920&&h==1080&&!o&&!r&&!r1);}
int sceVideoOutRegisterBuffers2(int32_t h,int32_t set,int32_t start,void*buf,int32_t count,void*attr,int32_t opt,void*r){assert(h==7&&!set&&!start&&count==2&&attr&&!opt&&!r);struct ps5_video_buffer*b=buf;assert(b[0].data==surface&&b[1].data==(char*)surface+PS5_SURFACE_BUFFER_STRIDE);uint32_t observed;size_t off=ps5vk_color_64k_rx_offset(4,727,327,1920);memcpy(&observed,(char*)b[0].data+off,4);assert(observed==0xff100004);off=ps5vk_color_64k_rx_offset(4,572,192,1920);memcpy(&observed,(char*)b[1].data+off,4);assert(observed==0xff100004);assert(fg_result_scanout_verify(&view,0,b[0].data,ps5vk_color_64k_rx_surface_size(4,1920,1080)));assert(fg_result_scanout_verify(&view,1,b[1].data,ps5vk_color_64k_rx_surface_size(4,1920,1080)));surface_digest=images_digest();return scenario==REGISTER_FAIL?-1:0;}
int sceVideoOutUnregisterBuffers(int32_t h,int32_t s){assert(h==7&&!s);if(scenario==BUSY)return (int)PS5_VIDEOOUT_RESOURCE_BUSY;return scenario==UNREGISTER_FAIL?-1:0;}
uint32_t*sceAgcDcbSetFlip(void*ptr,uint32_t h,int32_t idx,uint32_t mode,int64_t arg){struct ps5_agc_command_buffer*w=ptr;assert(h==7&&idx==(int)(submits%2)&&mode==1);slot=idx;token=(uint64_t)arg;assert(token==UINT64_C(0x33100000000)+(unsigned)submits+1);*w->up++=0x12345678;return w->up;}
int32_t sceAgcDriverSubmitDcb(void*description){struct ps5_agc_submit*s=description;assert(s->count==9&&s->words[0]==0x12345678&&s->words[1]==0xc0064900);uintptr_t addr=(uintptr_t)s->words[4]|((uintptr_t)s->words[5]<<32);assert(addr==(uintptr_t)fence&&*fence==1);assert(images_digest()==surface_digest);assert(slot==submits%2u);submits++;if(scenario==SUBMIT_FAIL)return -1;if(scenario!=FENCE_TIMEOUT)*fence=0;if(scenario==SOURCE_FAIL&&submits==3)gpu_output[0]^=1;if(scenario==GUARD_FAIL&&submits==3){size_t n=ps5vk_color_64k_rx_surface_size(4,1920,1080);((unsigned char*)surface)[n]=0;}return 0;}
int32_t sceAgcSuspendPoint(void){return scenario==SUSPEND_FAIL?-1:0;}
int sceKernelWaitEqueue(void*q,void*e,int n,int*out,unsigned*t){assert(q==(void*)1&&e&&n==1&&*t==10000);if(scenario==EVENT_TIMEOUT){*out=0;return -1;}*out=1;return 0;}
int sceVideoOutGetEventData(const void*e,int64_t*arg){assert(e);if(scenario==DECODE_FAIL)return -1;*arg=(int64_t)(scenario==WRONG_TOKEN?token+1:token);presents++;return 0;}
static void run(int which,int when_stop,int expected,int expect_retain){scenario=which;stop_at=when_stop;allocations=0;stop_calls=submits=presents=closes=unmaps=releases=retained=0;now=0;surface=command=NULL;if(!setjmp(park)){int rc=fg_run_owned_display(&view,stop);fprintf(stderr,"scenario=%d stop=%d result=%d expected=%d\n",which,when_stop,rc,expected);assert(!expect_retain&&rc==expected);}else {assert(expect_retain);fprintf(stderr,"scenario=%d retained=1 no-surface-free=1\n",which);}assert(retained==expect_retain);if(expect_retain)assert(!unmaps&&!releases);if(which==OK&&!when_stop)assert(submits==3&&presents==3&&closes==1&&releases==1);/* Simulate process teardown only after recording retained ownership. */free(surface);free(command);surface=command=NULL;if(which==SOURCE_FAIL)gpu_output[0]^=1;}
static void pattern(void){
 assert(fg_rgba_to_bgra(0xff0000ff)==0xffff0000);
 assert(fg_rgba_to_bgra(0xffff0000)==0xff0000ff);
 assert(fg_rgba_to_bgra(0xff00ff00)==0xff00ff00);
 assert(fg_rgba_to_bgra(0x3d123456)==0x3d563412);
 assert(fg_result_view_pixel(&view,0,0,0)==0xffeeeeee);
 assert(fg_result_view_pixel(&view,0,120,320)==0xff000000);
 assert(fg_result_view_pixel(&view,0,720,320)==0xff100004);
 assert(fg_result_view_pixel(&view,0,1320,320)==0xff200008);
 assert(fg_result_view_pixel(&view,1,560,180)==0xff100004);
 assert(!fg_result_view_pixel(&view,2,0,0)&&!fg_result_view_pixel(&view,0,1920,0));
 size_t n=ps5vk_color_64k_rx_surface_size(4,1920,1080);
 unsigned char*seen=calloc(n/4,1);uint32_t*tiled=calloc(1,n);assert(seen&&tiled);
 for(unsigned y=0;y<1080;y++)for(unsigned x=0;x<1920;x++){
  size_t o=ps5vk_color_64k_rx_offset(4,x,y,1920);assert(!(o%4)&&o<=n-4&&!seen[o/4]);seen[o/4]=1;
 }
 for(unsigned mode=0;mode<2;mode++){
  for(unsigned y=0;y<1080;y++)for(unsigned x=0;x<1920;x++){
   size_t o=ps5vk_color_64k_rx_offset(4,x,y,1920);
   tiled[o/4]=fg_result_view_pixel(&view,mode,x,y);
  }
  assert(fg_result_scanout_verify(&view,mode,tiled,n));
  struct fg_result_rect rects[3];unsigned count=fg_result_rects(&view,mode,rects);
  for(unsigned p=0;p<count;p++)for(unsigned y=0;y<32;y++)for(unsigned x=0;x<32;x++){
   unsigned sx=rects[p].x+x*rects[p].scale+rects[p].scale-1;
   unsigned sy=rects[p].y+y*rects[p].scale+rects[p].scale-1;
   size_t o=ps5vk_color_64k_rx_offset(4,sx,sy,1920);
   uint32_t src=rects[p].pixels[y*32+x];
   uint32_t expected=(src&0xff00ff00)|((src&255)<<16)|((src>>16)&255);
   assert(tiled[o/4]==expected);
  }
  /* Wrong channel order or one altered texel must fail staging validation. */
  size_t o=ps5vk_color_64k_rx_offset(4,mode?572:727,mode?192:327,1920);
  tiled[o/4]^=1;assert(!fg_result_scanout_verify(&view,mode,tiled,n));
 }
 assert(!fg_result_scanout_verify(&view,0,tiled,3));
 free(seen);free(tiled);
}
static void bad_views(void){
 stop_at=0;allocations=0;assert(!surface);
 gpu_output[17]^=1;assert(fg_run_owned_display(&view,stop)==-1&&!allocations);gpu_output[17]^=1;
 input_a[0]^=1;assert(fg_run_owned_display(&view,stop)==-1&&!allocations);input_a[0]^=1;
 struct fg_result_view invalid=view;invalid.gpu_output=NULL;assert(fg_run_owned_display(&invalid,stop)==-1&&!allocations);
 invalid=view;invalid.pixels=FG_PIXELS-1;assert(fg_run_owned_display(&invalid,stop)==-1&&!allocations);
 assert(fg_run_owned_display(NULL,stop)==-1&&!allocations);
}
int main(void){for(unsigned i=0;i<FG_PIXELS;i++){input_a[i]=fg_pixel(i,0);input_b[i]=fg_pixel(i,1);gpu_output[i]=fg_blend(input_a[i],input_b[i]);}pattern();run(OK,0,0,0);run(OK,1,1,0);run(OK,2,1,0);run(OK,3,1,0);run(OK,4,1,0);run(ALLOC_FAIL,0,-1,0);run(MAP_FAIL,0,-1,1);run(OPEN_FAIL,0,-1,0);run(QUEUE_FAIL,0,-1,0);run(REGISTER_FAIL,0,-1,0);run(BUSY,0,0,0);run(UNREGISTER_FAIL,0,-1,1);run(SUBMIT_FAIL,0,-1,1);run(SUSPEND_FAIL,0,-1,1);run(FENCE_TIMEOUT,0,-1,1);run(WRONG_TOKEN,0,-1,1);run(DECODE_FAIL,0,-1,1);run(EVENT_TIMEOUT,0,-1,1);run(GUARD_FAIL,0,-1,1);run(CLOSE_FAIL,0,-1,1);run(SOURCE_FAIL,0,-1,1);bad_views();puts("PASS: RGBA/BGRA staging, all 4096 panel texels at magnified centers and edges, corruption detection, 2073600 unique tiled addresses; 21 display and 5 invalid-readback scenarios; host doubles, not GPU execution");}
