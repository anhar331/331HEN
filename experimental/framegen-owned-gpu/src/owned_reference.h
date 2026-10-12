/* Copyright (C) 2026 ANHAR / 331HEN. GPL-3.0-or-later. */
#ifndef OWNED_REFERENCE_H
#define OWNED_REFERENCE_H
#include <stdint.h>
#include <stddef.h>
enum { FG_PIXELS=1024, FG_GUARD=64, FG_BUFFER_BYTES=4096, FG_MEMORY_BYTES=8192 };
static inline uint32_t fg_pixel(unsigned i,unsigned frame) {
 unsigned x=i%32,y=i/32;
 return 0xff000000u|((x+frame*4)%32)*8u|((y*8u)<<8)|(((x+y+frame)%32)*8u<<16);
}
static inline uint32_t fg_blend(uint32_t a,uint32_t b) {
 uint32_t out=0;for(unsigned k=0;k<4;k++){unsigned s=k*8;out|=((((a>>s)&255u)+((b>>s)&255u))/2u)<<s;}return out;
}
static inline int fg_verify(const uint32_t *a,const uint32_t *b,const uint32_t *o) {
 for(unsigned i=0;i<FG_PIXELS;i++)if(a[i]!=fg_pixel(i,0)||b[i]!=fg_pixel(i,1)||o[i]!=fg_blend(a[i],b[i]))return 0;
 return 1;
}
#endif
