/* Copyright (C) 2026 ANHAR. GPL-3.0-or-later. CPU staging verification. */
#ifndef FG_RESULT_SCANOUT_H
#define FG_RESULT_SCANOUT_H
#include "result_view.h"
#include "color_detile.h"
#include <string.h>
/* Verify every original texel at its magnified block's center, directly in
 * the immutable tiled surface. This checks staging, not GPU execution. */
static inline int fg_result_scanout_verify(const struct fg_result_view *v,
        unsigned mode, const void *surface, size_t bytes) {
    struct fg_result_rect r[3]; unsigned n = fg_result_rects(v,mode,r);
    if (!n || !surface || bytes < sizeof(uint32_t)) return 0;
    for (unsigned p=0;p<n;++p)
        for (unsigned y=0;y<32;++y)
            for (unsigned x=0;x<32;++x) {
                unsigned sx=r[p].x+x*r[p].scale+r[p].scale/2;
                unsigned sy=r[p].y+y*r[p].scale+r[p].scale/2;
                size_t off=ps5vk_color_64k_rx_offset(4,sx,sy,FG_DISPLAY_WIDTH);
                if (off>bytes-sizeof(uint32_t)) return 0;
                uint32_t observed;
                memcpy(&observed,(const unsigned char *)surface+off,sizeof(observed));
                if (observed!=fg_rgba_to_bgra(r[p].pixels[y*32+x])) return 0;
            }
    return 1;
}
#endif
