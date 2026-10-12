#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
FG_VK=upstream/ps5-vulkan
FG_GEARS=upstream/ps5-agc-gears
cc -std=c11 -D_DEFAULT_SOURCE -O1 -g -Wall -Wextra -Werror -Wno-error=cpp -fsanitize=address,undefined \
 -Isrc -I"$FG_VK/native" -I"$FG_VK/src" -I"$FG_VK/include" -I"$FG_VK/third_party/vulkan-headers/include" \
 -I"$FG_GEARS/include" -I"$FG_GEARS/src" -Ilab/projects/logging_server/client \
 tests/display_test.c src/native_display.c "$FG_VK/src/color_detile.c" \
 "$FG_GEARS/src/ps5_surface.c" "$FG_GEARS/src/ps5_videoout.c" "$FG_GEARS/src/ps5_present.c" \
 "$FG_GEARS/src/ps5_agc_writer.c" "$FG_GEARS/src/ps5_gpu_span.c" "$FG_GEARS/src/ps5_event_adapter.c" \
 "$FG_GEARS/src/ps5_frame_completion.c" -o tests/display_test
ASAN_OPTIONS=detect_leaks=1 ./tests/display_test
