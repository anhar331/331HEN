# 331 FrameGen GPU Diagnostic 0.1

Stage: native owned-buffer compute experiment, not system-wide FrameGen. Compiled application ELF and signed plaintext FSELF, using SDK 0.44 / Clang 18 and pinned public GPU/compiler sources. HEN binaries, kstuff, kernel patches, firmware offsets and game files are untouched.

ELF SHA256: f8030a257f011de596f4621a5db03b71d2c834b9f750c41c5c21a483a63805aa

FSELF SHA256: d62e4e7c783232d2916e19ed2c80f420c672f2ddea443d56b0806c95f37ccf53

The owned shader computes a per-channel integer average of two CPU-authored 32x32 images on the GPU candidate path. It uses three storage-buffer descriptors, then waits for completion and validates output, unchanged inputs and guards. No CPU calculation writes the output's active range after poisoning and before fence completion. CPU-monotonic submission-to-fence timing is not an isolated GPU cost or latency measurement.

Completed: native linking, SPIR-V validation, exact PSBC/ACO compilation to gfx1013 (44 code words), numerical reference tests, 10 host backend suites, and valid FSELF static integrity inspection. Eight host suites use ASan/UBSan. Two native ABI syscall-double suites use UBSan because their fixed descriptor aperture conflicts with ASan shadow mappings. Full corresponding backend/compiler sources are supplied in source-archives.

Native PS5 GPU execution, readback, native application launch/close and firmware 12.70 graphics compatibility remain untested. Presentation is intentionally unavailable in this compute-only profile. No frames from a real game are accessed. No interpolation pacing, optical flow, added display frames or increased FPS are implemented.

The private upstream network logger was replaced with an independently authored local-file logger. The public runtime-compiler/no-offline-library path eliminates reliance on historical developer LLPC binaries without relaxing their original hash checks. The independent native-app clean-room runtime reproduced its upstream raw and signed hashes.

Initial build fixes: add missing transfer-layout source dependencies; compile a deliberately unavailable WSI adapter for compute-only scope; provide the build container's CA trust for downloads; apply repository-scoped Git safe-directory settings. Host fixture compiler issues were resolved using GCC O0 sanitizers; native fixed-aperture fixtures use their documented UBSan mode. All final selected suites passed.

This is a native application ELF, not a payload ELF. Never send it through Payload Manager or replace 331HEN. The complete native title directory is PPSA99993-app. GPU execution is disabled without an explicit /app0/ENABLE_GPU_TEST marker and an absent STOP file. See README.md for a single bounded hardware test and failure/resource-retention behavior.
