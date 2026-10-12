#!/bin/bash
set -euo pipefail
FG_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# Fixed allowlist; no changes to host Git configuration.
exec docker --host=unix:///var/run/docker.sock run --rm \
 --mount "type=bind,src=$FG_ROOT,dst=/workspace/331-FrameGen" \
 --mount type=bind,src=/etc/ssl/certs/ca-certificates.crt,dst=/etc/ssl/certs/ca-certificates.crt,readonly \
 -e GIT_CONFIG_COUNT=4 \
 -e GIT_CONFIG_KEY_0=safe.directory -e GIT_CONFIG_VALUE_0=/workspace/331-FrameGen/upstream/ps5-vulkan \
 -e GIT_CONFIG_KEY_1=safe.directory -e GIT_CONFIG_VALUE_1=/workspace/331-FrameGen/upstream/ps5-native-app-boilerplate \
 -e GIT_CONFIG_KEY_2=safe.directory -e GIT_CONFIG_VALUE_2=/workspace/331-FrameGen/upstream/ps5-agc-gears \
 -e GIT_CONFIG_KEY_3=safe.directory -e GIT_CONFIG_VALUE_3=/workspace/331-FrameGen/upstream/ps5-vulkan/third_party/psbc-reference \
 -w /workspace/331-FrameGen 331-framegen-builder "$@"
