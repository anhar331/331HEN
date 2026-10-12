/* Copyright (C) 2026 ANHAR / 331HEN. GPL-3.0-or-later. */
#ifndef FG_CONTROL_H
#define FG_CONTROL_H
#include <errno.h>
#include <sys/stat.h>
enum fg_control_state { FG_CONTROL_DISABLED, FG_CONTROL_ENABLED,
                        FG_CONTROL_STOPPED, FG_CONTROL_STOP_ERROR };
struct fg_marker_probe { int result, error, valid; };
struct fg_control_result {
    enum fg_control_state state;
    int stop_error;
    const char *selected_path;
    struct fg_marker_probe markers[2];
};
typedef int (*fg_stat_fn)(const char *, struct stat *);
/* STOP always takes precedence, including unexpected access failures.
 * Explicit enable requires a regular empty file; symlinks are not accepted. */
static inline struct fg_control_result fg_control_check(
        const char *stop_path, const char *mounted, const char *installed,
        fg_stat_fn stat_file) {
    struct fg_control_result out = {0};
    struct stat st;
    errno = 0;
    if (stat_file(stop_path, &st) == 0) {
        out.state = FG_CONTROL_STOPPED;
        return out;
    }
    out.stop_error = errno;
    if (errno != ENOENT && errno != ENOTDIR) {
        out.state = FG_CONTROL_STOP_ERROR;
        return out;
    }
    const char *paths[2] = {mounted, installed};
    for (unsigned i = 0; i < 2; ++i) {
        errno = 0;
        out.markers[i].result = stat_file(paths[i], &st);
        out.markers[i].error = out.markers[i].result ? errno : 0;
        out.markers[i].valid = out.markers[i].result == 0 &&
                              S_ISREG(st.st_mode) && st.st_size == 0;
        if (out.markers[i].valid && !out.selected_path)
            out.selected_path = paths[i];
    }
    out.state = out.selected_path ? FG_CONTROL_ENABLED : FG_CONTROL_DISABLED;
    return out;
}
#endif
