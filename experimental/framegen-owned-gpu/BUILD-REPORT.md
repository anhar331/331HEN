# Build report — GPU-result display diagnostic 0.5

Compiled from source in /workspace/331-FrameGen with SDK 0.44 / Clang 18, unchanged pinned public graphics/compiler/native sources and the existing 331-framegen-builder container. Native PS5 application PPSA99993, not a Payload Manager ELF payload. Firmware target: owner 12.70; public graphics interfaces originate from upstream 12.02. The new bridge remains untested on PS5.

Native ELF SHA-256: `ce8039abca230c2e0a138a7b88d0faceaa6f9ec4a2dbab69a90ae8247480e24e`

Compiled/signed native eboot SHA-256: `385b5720ea45089f67c47dad00a15eb55a21c73c0ad470cad0421408b7b836c8`

## Change from the successful owner-tested 0.4

The shader, GPU submission, fence/readback validation, public graphics interfaces, scanout allocation, flip/event protocol, guards and release ordering are retained. Display receives the caller's exact input and GPU-output pointers after fence/invalidate/validation. CPU code magnifies and converts these pixels into tiled BGRA views: A / GPU result / B, zoomed GPU result, then comparison. It never replaces the GPU output with a CPU-computed blend. This is an intentional CPU-staging bridge, not GPU-only/zero-copy scanout. Immutable source data is validated before allocation and after presentation; each source texel's staging is checked before VideoOut registration.

The existing working v0.4 source, tools/tests, native executable, metadata and inspection ELF are backed up under backups/v0.4; the earlier downloadable ZIP remains unchanged. The supplied v0.4 owner log and visual/Close Game confirmation are recorded separately. No HEN, cheat, patch, kernel or kstuff code changes.

## Verified in this build

- Full native ELF/FSELF compilation and signing passed with -Wall -Wextra -Werror. SPIR-V validation passed; shader SHA-256 remains `9706ef9b8d76dde8451bfd5eafab0d18f411563914a0985768db8c8ce6eeb2f5`.
- FSELF signed-plaintext integrity valid, 12 segments. ELF64 little-endian x86-64; all five PT_LOAD segments match extraction from the FSELF. Whole-file equality is not required for non-loadable trailers.
- ASan/UBSan: 21 actual display orchestration scenarios using host native API/AGC/arena doubles plus the real pinned flip/event/VideoOut/tile helpers, including STOP, readback mutation after staging and unknown ownership/error paths. Five bad readback cases reject before allocation: altered output, altered input, null output, incorrect pixel count, null view.
- Independent known red/green/blue/alpha conversion assertions, expected panel samples, every 4,096 source texel center and magnified block edge across the two views, tiled staging corruption detection and all 2,073,600 aligned/in-bounds/non-overlapping scanout coordinates passed. Host inputs/results are CPU reference fixtures; these tests do not execute a GPU.
- Source routing check passed: fence→invalidate→numerical validation→actual mapped GPU output→display; the display source does not synthesize fg_blend/fg_pixel output. Static routing is not hardware attestation.
- Existing 12 control and 11 native filesystem mock scenarios rerun and passed with ASan/UBSan. Marker/STOP paths and native adapter unchanged.
- Preserved SHA-256 check passed for the original local 331HEN.elf, two previous HEN builds and embedded kstuff.

Ten unchanged backend contract suites, the real command-arena host syscall test and exact PSBC/ACO gfx1013 shader compiler host test passed in v0.4. They were not repeated for this data-routing/display-only change. Their previous logs/results are retained; current GPU compile/dispatch behavior is untested on hardware until the owner runs v0.5.

## Hardware status and remaining gates

Owner v0.4 log reports successful 1,024-pixel compute/readback, three matching display events, closed VideoOut and zero final allocations; owner also confirmed visible A/B/A and Close Game. No independent console access or deployed binary hash attestation. Those are static CPU-authored scanout surfaces, so they do not establish the new result bridge's correctness. Version 0.5 PS5 testing has not occurred.

No game capture, game presentation takeover, motion estimation, moving image sequence, game FrameGen, GPU-side image copy, GPU timestamp, FPS or input-latency measurement. CPU-staged output and roughly one-second view holds are deliberate. A graphics driver hang/crash remains possible; unknown completion/ownership retains resources until system Close Game. No stable HEN release or Payload Manager catalog was changed.

One test: README.md / INSTALL-AR.md. Expect comparison/zoom/comparison and the new OWNED_GPU_RESULT_VALIDATED plus two OWNED_RESULT_SCANOUT_PASS lines before three matching OWNED_DISPLAY_PRESENTED events and clean release. Sources, original licensing and full pinned dependency archives are provided separately.
