/* Native filesystem adapter; independent implementation, GPL-3.0-or-later.
 * Public declarations: OpenOrbis libkernel.h/_types/kernel.h; SDK 0.44 exports.
 * No raw lstat syscall, permission elevation or fallback treating errors as absence. */
#ifndef FG_NATIVE_FILE_PROBE_H
#define FG_NATIVE_FILE_PROBE_H
#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
extern int32_t sceKernelOpen(const char *, int32_t, mode_t);
extern int32_t sceKernelFstat(int32_t, struct stat *);
extern int32_t sceKernelClose(int32_t);
#if defined(__PROSPERO__)
_Static_assert(sizeof(struct stat) == 120, "native filesystem stat ABI size");
_Static_assert(offsetof(struct stat, st_mode) == 8, "native stat mode offset");
_Static_assert(offsetof(struct stat, st_size) == 72, "native stat size offset");
#endif
struct fg_native_probe_trace { int32_t open_rc, stat_rc, close_rc; };
static inline int fg_native_errno(int32_t rc) {
    uint32_t code = (uint32_t)rc;
    if ((code & UINT32_C(0xffff0000)) == UINT32_C(0x80020000) &&
        (code & UINT32_C(0xffff)) != 0)
        return (int)(code & UINT32_C(0xffff));
    return EIO;
}
static inline int fg_native_file_probe(const char *path, struct stat *st,
                                       struct fg_native_probe_trace *trace) {
    struct fg_native_probe_trace result = {0};
    result.open_rc = sceKernelOpen(path, O_RDONLY | O_NOFOLLOW | O_NONBLOCK, 0);
    if (result.open_rc < 0) {
        if (trace) *trace = result;
        errno = fg_native_errno(result.open_rc);
        return -1;
    }
    result.stat_rc = sceKernelFstat(result.open_rc, st);
    result.close_rc = sceKernelClose(result.open_rc);
    if (trace) *trace = result;
    if (result.stat_rc != 0 || result.close_rc != 0) {
        errno = fg_native_errno(result.stat_rc ? result.stat_rc : result.close_rc);
        return -1;
    }
    return 0;
}
#endif
