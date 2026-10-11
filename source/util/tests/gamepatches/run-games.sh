#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/../../../.."
TASK_GAMES_BUILD="${GP_TEST_BUILD:-/tmp/331hen-patch-tests}"
mkdir -p "$TASK_GAMES_BUILD"
TASK_GAMES_SANITIZE=(-fsanitize=address,undefined -fno-omit-frame-pointer)
c++ -std=c++20 -Wall -Wextra -Werror -g -O1 \
  "${TASK_GAMES_SANITIZE[@]}" -Isource/util/include -Ithird_party/cjson \
  source/util/source/patches/games.cpp third_party/cjson/cJSON.cpp \
  source/util/tests/gamepatches/games_test.cpp \
  -o "$TASK_GAMES_BUILD/games_test"
"$TASK_GAMES_BUILD/games_test"
