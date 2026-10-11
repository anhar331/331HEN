# Current refinement changes

- Native Game Patches now selects an installed game before launch. ON/OFF choices are persistent; selecting a patch does not write into a running process.
- Strict installed APP_VER/contentVersion metadata and package/executable installation evidence replace the running-game-only selector. Cached metadata alone does not count as installation.
- Daemon lifecycle sends PID, app ID, Title ID, launched version and exact process-generation strings. Util validates process identity without SystemService BigApp calls under PTRACE_AUTHID.
- Corrected SYS_dl_get_info_2 section ABI (32-bit size and protection, 16-byte record). Module lookup now checks syscall errors, handle counts and string bounds.
- Prevented retry injection into the same ShellUI PID after an uncertain/failed attempt; a late ready marker is accepted.
- Bootstrapper waits for its previously owned services to exit and refuses overlapping replacements. Injector refuses missing pthread_create or unavailable credential backup, propagates detach/restore failures; owned ptrace attach cleanup was tightened.
- Startup logs distinguish embedded kstuff, custom override and already-active kstuff. Embedded v1.10, kernel patches/offsets, cheat storage and existing branding/credits are preserved.
- New installed-game, offline selection/runtime, native navigation and module ABI tests accompany existing host regression tests.

These fixes address demonstrated source defects and crash risks. They do not establish the actual cause of the owner's hardware crash or prove that the new ELF is stable.
