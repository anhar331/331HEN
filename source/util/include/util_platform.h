#pragma once

/*
 * Shared util-platform helpers used by cheats and other daemons.
 * Prefer these over duplicating sysctl/dynlib/file logic per feature.
 */

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- firmware (Prospero SW version; not kern.sdk_version) --------------- */

/**
 * Firmware major from sceKernelGetProsperoSystemSwVersion (value >> 16).
 * Used for runtime feature gates (e.g. mdbg vs kdirect). Distinct from
 * libhijacker getSystemSwVersion() which reads kern.sdk_version for offsets.
 */
uint32_t util_system_fw_major(void);

/* ---- file I/O ----------------------------------------------------------- */

/** Read entire file into malloc'd buffer (+NUL). Caller frees *buf_out. */
int util_file_read_alloc(const char *path, char **buf_out, size_t *size_out,
                         size_t max_size);

/* ---- game / process context for cheats --------------------------------- */

typedef struct {
  int pid;
  int appid;
  char title_id[16];
  char version[32];
  char platform[16]; /* "ps4" / "ps5" / "unknown" */
  char process_name[64];
} game_context_t;

void util_game_platform_from_title_id(const char *title_id, char *out,
                                      size_t out_size);

/**
 * Resolve installed app version for title_id (param.json / param.sfo).
 * Same discovery paths as BREW_UTIL_GET_GAME_VER.
 */
int util_resolve_game_version(const char *title_id, char *out, size_t out_size);

/**
 * Fill context for the current BigApp (pid/appid/title/process).
 * version is resolved when possible; otherwise "unknown".
 */
int util_get_running_bigapp(game_context_t *out);

/** Process start identity without SystemService (util runs as PTRACE_AUTHID). */
uint64_t util_process_generation(pid_t pid);
int util_verify_game_identity(pid_t pid, int appid, uint64_t generation,
                              const char *title_id);

/*
 * Dynlib module info — layout matches NineS module_info_t / SYS_dl_get_info_2.
 * Kept here so C++ util targets need not pull NineS freebsd-helper headers.
 */
#include <onion/module_info.h>
#define UTIL_MODULE_INFO_NAME_LENGTH MODULE_INFO_NAME_LENGTH
#define UTIL_MODULE_INFO_SANDBOXED_PATH_LENGTH MODULE_INFO_SANDBOXED_PATH_LENGTH
#define UTIL_MODULE_INFO_MAX_SECTIONS MODULE_INFO_MAX_SECTIONS
#define UTIL_FINGERPRINT_LENGTH FINGERPRINT_LENGTH
typedef module_section_t util_module_section_t;
typedef module_info_t util_module_info_t;

/**
 * Find module in pid via NineS get_module_info, then eboot imagebase fallback
 * for eboot.bin / process name when dynlib list is incomplete.
 */
int util_find_module(pid_t pid, const char *module_name,
                     util_module_info_t *out);

/** Scan processes of appid for a module; sets *pid_out on success. */
int util_find_module_in_app(int appid, const char *module_name, pid_t *pid_out,
                            util_module_info_t *out);

#ifdef __cplusplus
}
#endif
