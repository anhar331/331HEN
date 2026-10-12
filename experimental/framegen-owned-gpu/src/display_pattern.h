/* Owned synthetic BGRA test images. Copyright (C) 2026 ANHAR. GPL-3.0-or-later. */
#ifndef FG_DISPLAY_PATTERN_H
#define FG_DISPLAY_PATTERN_H
#include <stdint.h>
enum { FG_DISPLAY_WIDTH = 1920, FG_DISPLAY_HEIGHT = 1080 };
static inline uint32_t fg_display_pixel(unsigned frame, unsigned x, unsigned y) {
    if (frame > 1 || x >= FG_DISPLAY_WIDTH || y >= FG_DISPLAY_HEIGHT)
        return 0;
    if (x < 16 || y < 16 || x >= FG_DISPLAY_WIDTH - 16 || y >= FG_DISPLAY_HEIGHT - 16)
        return UINT32_C(0xffeeeeee);
    if (y >= 940 && y < 1040 && x >= 360 && x < 1560) {
        const uint32_t bars[3] = {UINT32_C(0xffff0000), UINT32_C(0xff00ff00), UINT32_C(0xff0000ff)};
        return bars[(x - 360) / 400];
    }
    unsigned box_x = frame ? 1340 : 340;
    if (x >= box_x && x < box_x + 240 && y >= 390 && y < 630)
        return UINT32_C(0xfff6d03b);
    if (x >= 910 && x < 1010 && y >= 100 && y < 240) {
        const unsigned glyph[2][7] = {{14,17,17,31,17,17,17},{30,17,17,30,17,17,30}};
        unsigned gx = (x - 910) / 20, gy = (y - 100) / 20;
        if (glyph[frame][gy] & (1u << (4 - gx)))
            return UINT32_C(0xffffffff);
    }
    return UINT32_C(0xff082550);
}
#endif
