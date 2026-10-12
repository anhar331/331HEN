/* Copyright (C) 2026 ANHAR. GPL-3.0-or-later.
 * View a completed, invalidated GPU readback. Never synthesize its result. */
#ifndef FG_RESULT_VIEW_H
#define FG_RESULT_VIEW_H
#include "owned_reference.h"
#include "display_pattern.h"
struct fg_result_view {
    const uint32_t *input_a, *input_b, *gpu_output;
    size_t pixels;
};
struct fg_result_rect { unsigned x, y, scale; const uint32_t *pixels; };
static inline int fg_result_view_valid(const struct fg_result_view *v) {
    return v && v->pixels == FG_PIXELS && v->input_a && v->input_b &&
           v->gpu_output && fg_verify(v->input_a, v->input_b, v->gpu_output);
}
/* Source uint bytes: R,G,B,A. Scanout uint bytes: B,G,R,A. */
static inline uint32_t fg_rgba_to_bgra(uint32_t p) {
    return (p & UINT32_C(0xff00ff00)) | ((p & 255u) << 16) | ((p >> 16) & 255u);
}
static inline uint32_t fg_result_fingerprint(const uint32_t *p) {
    uint32_t h = UINT32_C(2166136261);
    for (unsigned i = 0; i < FG_PIXELS; ++i)
        for (unsigned byte = 0; byte < 4; ++byte)
            h = (h ^ ((p[i] >> (byte * 8)) & 255u)) * UINT32_C(16777619);
    return h;
}
static inline unsigned fg_result_rects(const struct fg_result_view *v, unsigned mode,
                                        struct fg_result_rect rects[3]) {
    if (!v || !rects || mode > 1 || v->pixels != FG_PIXELS ||
        !v->input_a || !v->input_b || !v->gpu_output) return 0;
    if (mode == 1) {
        rects[0] = (struct fg_result_rect){560, 180, 25, v->gpu_output};
        return 1;
    }
    rects[0] = (struct fg_result_rect){120, 320, 15, v->input_a};
    rects[1] = (struct fg_result_rect){720, 320, 15, v->gpu_output};
    rects[2] = (struct fg_result_rect){1320, 320, 15, v->input_b};
    return 3;
}
static inline int fg_result_glyph(unsigned c, unsigned x, unsigned y) {
    /* A,B,G,P,U, five columns, seven rows. */
    const unsigned rows[5][7] = {
        {14,17,17,31,17,17,17},{30,17,17,30,17,17,30},
        {14,17,16,23,17,17,14},{30,17,17,30,16,16,16},
        {17,17,17,17,17,17,14}
    };
    return c < 5 && x < 5 && y < 7 && (rows[c][y] & (1u << (4-x)));
}
static inline uint32_t fg_result_view_pixel(const struct fg_result_view *v,
                                            unsigned mode, unsigned x, unsigned y) {
    struct fg_result_rect r[3]; unsigned n = fg_result_rects(v, mode, r);
    if (!n || x >= FG_DISPLAY_WIDTH || y >= FG_DISPLAY_HEIGHT) return 0;
    if (x < 16 || y < 16 || x >= FG_DISPLAY_WIDTH-16 || y >= FG_DISPLAY_HEIGHT-16)
        return UINT32_C(0xffeeeeee);
    for (unsigned p = 0; p < n; ++p) {
        unsigned size = 32 * r[p].scale;
        if (x >= r[p].x && x < r[p].x+size && y >= r[p].y && y < r[p].y+size) {
            unsigned i = ((y-r[p].y)/r[p].scale)*32 + (x-r[p].x)/r[p].scale;
            return fg_rgba_to_bgra(r[p].pixels[i]);
        }
        if (x >= r[p].x-4 && x < r[p].x+size+4 && y >= r[p].y-4 && y < r[p].y+size+4)
            return mode == 1 || p == 1 ? UINT32_C(0xfff6d03b) : UINT32_C(0xffeeeeee);
    }
    /* GPU label centered above its panel; A and B label the original inputs. */
    unsigned label_y = mode ? 80 : 160;
    if (y >= label_y && y < label_y+70) {
        const unsigned letters[5] = {0,2,3,4,1};
        const unsigned starts[5] = {335,875,935,995,1535};
        for (unsigned p = mode ? 1 : 0; p < (mode ? 4u : 5u); ++p)
            if (x >= starts[p] && x < starts[p]+50 &&
                fg_result_glyph(letters[p],(x-starts[p])/10,(y-label_y)/10))
                return UINT32_C(0xffffffff);
    }
    if (y >= 1000 && y < 1040 && x >= 360 && x < 1560) {
        const uint32_t bars[3] = {UINT32_C(0xffff0000),UINT32_C(0xff00ff00),UINT32_C(0xff0000ff)};
        return bars[(x-360)/400];
    }
    return UINT32_C(0xff082550);
}
#endif
