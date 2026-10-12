# GPU Diagnostic 0.2 — explicit enable-path correction

The owner's v0.1 log reported DISABLED before GPU initialization. It could not distinguish a missing /app0 marker from an active STOP file. No GPU execution result can be inferred from that log.

Version 0.2 probes both /app0/ENABLE_GPU_TEST and /data/homebrew/PPSA99993-app/ENABLE_GPU_TEST, requiring a regular empty file. STOP has priority; unexpected STOP lookup failures disable initialization. Each lookup has its own path, result, errno and regular-empty status in the log. No automatic enable, GPU shader/backend changes, game hooks or HEN/kstuff modifications. The shader hash remains identical.

Full native compilation/linking and FSELF integrity inspection passed. Twelve real/mocked-filesystem control scenarios passed under ASan/UBSan, including the installed-path fallback, STOP precedence and unreadable STOP errors. Earlier ten backend suites and exact shader compilation remain results for the unchanged backend; they were not rerun for this control-only correction. Version 0.2 has NOT been PS5-tested.

Close the diagnostic before replacing files. Copy the new application's eboot.bin and sce_sys/param.json to the same existing directory; leave the empty ENABLE_GPU_TEST marker and original companion module in place. Run once and retrieve /data/331FrameGen/diagnostic.log. If STOP is intentionally present, the test stays off; do not remove it blindly. This is still a native application, not a Payload Manager payload.
