/* Copyright (C) 2026 ANHAR / 331HEN. GPL-3.0-or-later.
 * Adapted presentation sequencing from Manuel Pereira's ps5-vulkan:
 * e6d3c3f82f3c8a574e28c174b15b287f9c0ecc6d/native/present_ps5.c.
 * Uses its command arena/format/tile helpers and ps5-agc-gears public helpers.
 * Only owned CPU-authored test surfaces; no game handles, hooks or buffers. */
#include "native_display.h"
#include "display_pattern.h"
#include "command_arena_ps5.h"
#include "presentation_format_ps5.h"
#include "color_detile.h"
#include "ps5_videoout.h"
#include "ps5_present.h"
#include "ps5_agc_writer.h"
#include "ps5_agc.h"
#include "submit_suspend_ps5.h"
#include "ps5_event_adapter.h"
#include "ps5log.h"
#include <string.h>
#include <time.h>
#include <unistd.h>
static const struct ps5_videoout_ops video_ops = {
    sceVideoOutOpen, sceVideoOutClose, sceVideoOutSetFlipRate,
    sceKernelCreateEqueue, sceKernelDeleteEqueue, sceVideoOutAddFlipEvent,
    sceVideoOutDeleteFlipEvent, sceVideoOutSetBufferAttribute2,
    sceVideoOutRegisterBuffers2, sceVideoOutUnregisterBuffers
};
static uint64_t clock_ns(void) {
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC, &t)) return 0;
    return (uint64_t)t.tv_sec * UINT64_C(1000000000) + (uint64_t)t.tv_nsec;
}
static void flush(const void *p, size_t bytes) {
    for (uintptr_t a = (uintptr_t)p & ~(uintptr_t)63, end = (uintptr_t)p + bytes;
         a < end; a += 64)
        __asm__ volatile("clflush (%0)" :: "r"(a) : "memory");
    __asm__ volatile("mfence" ::: "memory");
}
static void retain(const char *stage, int rc) {
    ps5log_printf(PS5LOG_ERR, "OWNED_DISPLAY_RETAIN stage=%s rc=%d", stage, rc);
    ps5log_close("Unknown display ownership: retain buffers until system Close Game");
    for (;;) sleep(1);
}
static int flip(uint32_t **cursor, uint32_t capacity, uint32_t mode,
                int32_t handle, int32_t index, uint32_t flip_mode, uint64_t token) {
    return ps5_agc_writer_set_flip(cursor, capacity, mode, handle, index,
                                    flip_mode, token, sceAgcDcbSetFlip);
}
int fg_run_owned_display(int (*should_stop)(void)) {
    struct ps5_surface_plan plan;
    struct ps5_videoout video = {.handle = -1};
    struct ps5vk_command_arena arena = {0};
    int64_t physical = -1;
    void *address = NULL;
    int allocated = 0, mapped = 0, guards_ready = 0, video_attempted = 0, result = -1;
    unsigned completed = 0;
    uint64_t last_completion = 0;
    if (!should_stop || should_stop()) return 1;
    if (ps5_surface_make_plan(0, &plan) || plan.width != FG_DISPLAY_WIDTH ||
        plan.height != FG_DISPLAY_HEIGHT) return -1;
    plan.format_word = ps5vk_native_video_format(VK_FORMAT_B8G8R8A8_UNORM);
    size_t tile_bytes = ps5vk_color_64k_rx_surface_size(4, plan.width, plan.height);
    /* Gears uses an older tile footprint; use the exact scoped Vulkan
     * SW_64K_R_X size matching the offset helper used below. */
    if (!plan.format_word || tile_bytes == SIZE_MAX ||
        tile_bytes + 256 > PS5_SURFACE_BUFFER_STRIDE) return -1;
    plan.tiled_footprint = tile_bytes;
    int rc = sceKernelAllocateMainDirectMemory(plan.allocation_bytes,
                         PS5_SURFACE_ALIGNMENT, 0x0c, &physical);
    ps5log_printf(PS5LOG_MARK, "OWNED_DISPLAY_ALLOC bytes=%llu rc=%d",
                    (unsigned long long)plan.allocation_bytes, rc);
    if (rc) goto cleanup;
    allocated = 1;
    rc = sceKernelMapDirectMemory(&address, plan.allocation_bytes, 0x33, 0,
                                   physical, PS5_SURFACE_ALIGNMENT);
    if (rc) retain("surface-map", rc);
    mapped = 1;
    if (!address || (uintptr_t)address % PS5_SURFACE_ALIGNMENT ||
        (uintptr_t)address > (UINT64_C(1) << 48) - plan.allocation_bytes)
        retain("surface-map-shape", -1);
    for (unsigned frame = 0; frame < 2; ++frame) {
        unsigned char *pixels = (unsigned char *)address + plan.buffer_offsets[frame];
        memset(pixels, 0, plan.tiled_footprint);
        memset(pixels + plan.tiled_footprint, 0xa5, 256);
        for (unsigned y = 0; y < plan.height; ++y)
            for (unsigned x = 0; x < plan.width; ++x) {
                size_t offset = ps5vk_color_64k_rx_offset(4, x, y, plan.width);
                if (offset > plan.tiled_footprint - sizeof(uint32_t)) goto cleanup;
                uint32_t pixel = fg_display_pixel(frame, x, y);
                memcpy(pixels + offset, &pixel, sizeof(pixel));
            }
        flush(pixels, plan.tiled_footprint + 256);
    }
    guards_ready = 1;
    ps5log_line(PS5LOG_MARK, "OWNED_DISPLAY_IMAGES width=1920 height=1080 cpu_authored=1 format=BGRA8 surfaces=2");
    VkResult created = ps5vk_command_arena_create(&arena);
    if (created == VK_ERROR_DEVICE_LOST) retain("arena-create", created);
    if (created != VK_SUCCESS) goto cleanup;
    if (should_stop()) { result = 1; goto cleanup; }
    video_attempted = 1;
    rc = ps5_videoout_open(&video, &video_ops, &plan, address);
    ps5log_printf(PS5LOG_MARK, "OWNED_DISPLAY_OPEN rc=%d owned_handle=%d", rc, video.handle);
    if (rc) goto cleanup;
    for (unsigned frame = 0; frame < 3; ++frame) {
        if (should_stop()) { result = 1; goto cleanup; }
        unsigned slot = frame % 2;
        uint64_t token = UINT64_C(0x33100000000) + frame + 1;
        volatile uint64_t *label = ps5vk_command_arena_label(&arena);
        if (!label) retain("arena-label", -1);
        *label = 1;
        uint32_t *commands = arena.address;
        struct ps5_present_stream stream = {commands, commands + PS5VK_COMMAND_ARENA_WORDS, commands, 0};
        rc = ps5_present_compose_flip_and_fence(&stream, flip, video.handle,
                            (int32_t)slot, plan.flip_mode, token, (uintptr_t)label);
        if (rc) retain("compose", rc);
        flush(arena.address, PS5VK_COMMAND_ARENA_BYTES);
        uint64_t submitted_at = clock_ns();
        if (!submitted_at) retain("submit-clock", -1);
        struct ps5_agc_submit submit = {commands, (uint32_t)(stream.cursor - commands), 0, {0, 0, 0}};
        struct ps5vk_submit_result submitted = ps5vk_submit_suspend(&submit);
        ps5log_printf(PS5LOG_MARK, "OWNED_DISPLAY_SUBMIT frame=%u slot=%u token=%llu rc=%d suspend_rc=%d cpu_ns=%llu",
               frame, slot, (unsigned long long)token, submitted.submit_rc, submitted.suspend_rc,
               (unsigned long long)submitted_at);
        if (submitted.submit_rc || submitted.suspend_rc)
            retain("submit-or-suspend", submitted.submit_rc ? submitted.submit_rc : submitted.suspend_rc);
        uint64_t start = clock_ns();
        if (!start) retain("fence-clock", -1);
        while (1) {
            flush((const void *)label, sizeof(*label));
            if (__atomic_load_n(label, __ATOMIC_ACQUIRE) == 0) break;
            if (clock_ns() - start > UINT64_C(3000000000)) retain("fence-timeout", -1);
            usleep(1000);
        }
        struct ps5_frame_completion completion;
        ps5_frame_completion_begin(&completion, token, 1);
        struct ps5_event_diagnostics diagnostics = {0};
        uint64_t events[4] = {0};
        start = clock_ns();
        if (!start) retain("event-clock", -1);
        for (;;) {
            struct ps5_event_poll poll = {video.equeue, events, label, 10000, 0,
                clock_ns() - start > UINT64_C(3000000000), sceKernelWaitEqueue,
                sceVideoOutGetEventData, &diagnostics};
            rc = ps5_event_poll_completion(&completion, &poll);
            if (rc == PS5_FRAME_DONE) break;
            if (rc < 0) retain("flip-event", rc);
        }
        uint64_t finished_at = clock_ns();
        ++completed;
        ps5log_printf(PS5LOG_MARK, "OWNED_DISPLAY_PRESENTED frame=%u slot=%u token=%llu matching_event=1 fence=0 cpu_ns=%llu previous_event_interval_ns=%llu",
             frame, slot, (unsigned long long)token, (unsigned long long)finished_at,
             (unsigned long long)(last_completion ? finished_at - last_completion : 0));
        last_completion = finished_at;
        sleep(1); /* Deliberately slow visual inspection, not an FPS benchmark. */
    }
    result = 0;
cleanup:
    if (video_attempted) {
        rc = ps5_videoout_close(&video, &video_ops);
        if (rc) retain("video-close", rc);
        ps5log_printf(PS5LOG_MARK, "OWNED_DISPLAY_CLOSED rc=0 frames=%u", completed);
    }
    if (ps5vk_command_arena_release(&arena) != VK_SUCCESS) retain("arena-release", -1);
    if (mapped && guards_ready) {
        for (unsigned i = 0; i < 2; ++i) {
            unsigned char *guard = (unsigned char *)address + plan.buffer_offsets[i] + plan.tiled_footprint;
            for (unsigned b = 0; b < 256; ++b)
                if (guard[b] != 0xa5) retain("surface-guard", -1);
        }
    }
    if (mapped) {
        rc = sceKernelMunmap(address, plan.allocation_bytes);
        if (rc) retain("surface-unmap", rc);
    }
    if (allocated) {
        rc = sceKernelReleaseDirectMemory(physical, plan.allocation_bytes);
        if (rc) retain("surface-release", rc);
    }
    ps5log_printf(PS5LOG_MARK, "OWNED_DISPLAY_FINISHED result=%d completed_presents=%u surfaces_released=1 game_frames=0 generated_frames=0",
                       result, completed);
    return result;
}
