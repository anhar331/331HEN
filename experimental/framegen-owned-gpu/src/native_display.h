/* Copyright (C) 2026 ANHAR. GPL-3.0-or-later. */
#ifndef FG_NATIVE_DISPLAY_H
#define FG_NATIVE_DISPLAY_H
/* Requires the existing owned AGC session and completed compute work.
 * 0 means three owned presents completed and all display resources closed.
 * -1 means pre-submission unavailability/failure; 1 means STOP canceled.
 * Ambiguous GPU/display ownership parks and retains resources. */
int fg_run_owned_display(int (*should_stop)(void));
#endif
