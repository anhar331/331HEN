/* Copyright (C) 2026 ANHAR. GPL-3.0-or-later. */
#ifndef FG_NATIVE_DISPLAY_H
#define FG_NATIVE_DISPLAY_H
#include "result_view.h"
/* Requires the existing owned AGC session and completed compute work.
 * 0 means three owned presents completed and all display resources closed.
 * -1 means pre-submission unavailability/failure; 1 means STOP canceled.
 * Ambiguous GPU/display ownership parks and retains resources. */
/* Caller keeps the three buffers alive and immutable after fence/invalidate. */
int fg_run_owned_display(const struct fg_result_view *view, int (*should_stop)(void));
#endif
