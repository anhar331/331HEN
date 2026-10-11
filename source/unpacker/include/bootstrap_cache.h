/* Copyright (C) 2026 OnionHEN / LightningMods
 *
 * Persistent decompressed bootstrapper cache under /user/data/331HEN.
 *
 * The cache is an optimization only: callers must still be able to boot
 * from the embedded LZMA blob when the directory is missing, the file is
 * stale, or the write fails.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HEN331_USER_DATA_DIR "/user/data/331HEN"
#define HEN331_CACHED_ELF "/user/data/331HEN/331HEN.elf"

/** Create /user/data/331HEN (and parents). False if it cannot exist. */
bool bootstrap_cache_prepare_dir(void);

/**
 * True when @path exists, is exactly @expected_size bytes, and a chunked
 * digest of the file matches @expected_digest / @digest_size.
 */
bool bootstrap_cache_matches(const char *path, size_t expected_size,
                             const uint8_t *expected_digest, size_t digest_size);

/**
 * Atomically replace @path with @elf via a temp file + rename(). False
 * leaves any previous @path untouched.
 */
bool bootstrap_cache_commit(const char *path, const uint8_t *elf, size_t size);

#ifdef __cplusplus
}
#endif
