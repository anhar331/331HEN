/* Copyright (C) 2026 OnionHEN / LightningMods */

#include "bootstrap_payload.h"

#include <onion/log.h>

#include <stdint.h>
#include <stdlib.h>

#include "LzmaLib.h"

#define LZMA_CLI_HEADER_SIZE 13

#ifndef HEN331_BOOTSTRAPPER_LZMA
#define HEN331_BOOTSTRAPPER_LZMA "../../bin/bootstrapper.elf.lzma"
#endif
#ifndef HEN331_BOOTSTRAPPER_SIZE
#define HEN331_BOOTSTRAPPER_SIZE "../../bin/bootstrapper.elf.lzma.size"
#endif
#ifndef HEN331_BOOTSTRAPPER_SHA1
#define HEN331_BOOTSTRAPPER_SHA1 "../../bin/bootstrapper.elf.sha1"
#endif

__asm__(".intel_syntax noprefix\n"
        ".section .data\n"
        ".global hen331_compressed\n"
        ".type   hen331_compressed, @object\n"
        ".align  16\n"
        "hen331_compressed:\n"
        ".incbin \"" HEN331_BOOTSTRAPPER_LZMA "\"\n"
        "hen331_compressed_end:\n"
        ".global hen331_compressed_size\n"
        ".type  hen331_compressed_size, @object\n"
        ".align  4\n"
        "hen331_compressed_size:\n"
        ".int    hen331_compressed_end - hen331_compressed\n"
        ".global hen331_decompressed_size\n"
        ".type   hen331_decompressed_size, @object\n"
        ".align  16\n"
        "hen331_decompressed_size:\n"
        ".incbin \"" HEN331_BOOTSTRAPPER_SIZE "\"\n"
        ".byte 0\n"
        ".global hen331_sha1_hex\n"
        ".type   hen331_sha1_hex, @object\n"
        ".align  16\n"
        "hen331_sha1_hex:\n"
        ".incbin \"" HEN331_BOOTSTRAPPER_SHA1 "\"\n"
        ".byte 0\n");

extern uint32_t hen331_compressed_size;
extern uint8_t hen331_compressed[];
extern uint8_t hen331_decompressed_size[];
extern uint8_t hen331_sha1_hex[];

size_t bootstrap_payload_expected_size(void) {
  char *end = NULL;
  const unsigned long n =
      strtoul((const char *)hen331_decompressed_size, &end, 10);
  if (end == (char *)hen331_decompressed_size || n == 0)
    return 0;
  return (size_t)n;
}

bool bootstrap_payload_expected_sha1(uint8_t digest[SHA1_DIGEST_SIZE]) {
  return sha1_parse_hex((const char *)hen331_sha1_hex, digest);
}

bootstrap_payload_status bootstrap_payload_decompress(uint8_t **out,
                                                      size_t *out_size) {
  if (!out || !out_size)
    return BOOTSTRAP_PAYLOAD_INVALID;
  *out = NULL;
  *out_size = 0;

  if (hen331_compressed_size <= LZMA_CLI_HEADER_SIZE)
    return BOOTSTRAP_PAYLOAD_INVALID;

  const size_t expected = bootstrap_payload_expected_size();
  if (expected == 0)
    return BOOTSTRAP_PAYLOAD_INVALID;

  uint8_t *buf = (uint8_t *)malloc(expected);
  if (!buf)
    return BOOTSTRAP_PAYLOAD_NOMEM;

  size_t dest_len = expected;
  size_t src_len = hen331_compressed_size;
  const int res =
      LzmaUncompress(buf, &dest_len, hen331_compressed + LZMA_CLI_HEADER_SIZE,
                     &src_len, hen331_compressed, LZMA_PROPS_SIZE);
  if (res != 0 || dest_len == 0) {
    LOG_ERROR("LzmaUncompress failed: %d", res);
    free(buf);
    return BOOTSTRAP_PAYLOAD_LZMA;
  }

  *out = buf;
  *out_size = dest_len;
  return BOOTSTRAP_PAYLOAD_OK;
}
