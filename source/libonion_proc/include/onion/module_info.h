/* Copyright (C) 2025 OnionHEN / LightningMods
 * Shared SYS_dl_get_info_2 userspace ABI. No kernel offsets are defined here.
 */
#pragma once
#include <stdint.h>

#define MODULE_INFO_NAME_LENGTH 128
#define MODULE_INFO_SANDBOXED_PATH_LENGTH 1024
#define MODULE_INFO_MAX_SECTIONS 4
#define FINGERPRINT_LENGTH 20

typedef struct {
  uint64_t vaddr;
  uint32_t size;
  uint32_t prot;
} module_section_t;

typedef struct {
  char filename[MODULE_INFO_NAME_LENGTH];
  uint64_t handle;
  uint8_t unknown0[32];
  uint64_t init;
  uint64_t fini;
  uint64_t eh_frame_hdr;
  uint64_t eh_frame_hdr_sz;
  uint64_t eh_frame;
  uint64_t eh_frame_sz;
  module_section_t sections[MODULE_INFO_MAX_SECTIONS];
  uint8_t unknown7[1176];
  uint8_t fingerprint[FINGERPRINT_LENGTH];
  uint32_t unknown8;
  char libname[MODULE_INFO_NAME_LENGTH];
  uint32_t unknown9;
  char sandboxed_path[MODULE_INFO_SANDBOXED_PATH_LENGTH];
  uint64_t sdk_version;
} module_info_t;
