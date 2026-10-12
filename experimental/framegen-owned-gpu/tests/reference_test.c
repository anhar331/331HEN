#include "owned_reference.h"
#include <assert.h>
#include <stdio.h>
int main(void){uint32_t a[FG_PIXELS],b[FG_PIXELS],o[FG_PIXELS];for(unsigned i=0;i<FG_PIXELS;i++){a[i]=fg_pixel(i,0);b[i]=fg_pixel(i,1);o[i]=fg_blend(a[i],b[i]);}assert(fg_verify(a,b,o));assert(fg_blend(0xffffffffu,0)==0x7f7f7f7fu);assert(fg_blend(0xff000000u,0xff000000u)==0xff000000u);o[511]^=1;assert(!fg_verify(a,b,o));o[511]^=1;a[19]^=1;assert(!fg_verify(a,b,o));puts("PASS: exact 1024-pixel reference, channel isolation, overflow bounds, wrong output and mutated inputs; CPU only");return 0;}
