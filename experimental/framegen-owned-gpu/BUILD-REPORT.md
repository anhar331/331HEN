# Build report — owned-display diagnostic v0.4

Compiled from source in /workspace/331-FrameGen, using the pinned public graphics/compiler/native sources and SDK 0.44 / Clang 18 in 331-framegen-builder. Native executable type is a PS5 application, not an ELF payload. Firmware target for the owner is 12.70; public graphics ABI/format evidence is upstream 12.02. New display execution on 12.70 remains untested.

ELF SHA-256: 9c169d80fb3c100a094ee5657df275326cf33c37a0fbf7056e65a29b3404a8fa

Signed plaintext native eboot SHA-256: 93cc5a571ab78083d04cae502c6d77d039087a4e2d006e1f1f61ef7e63f09d56

## Verified here

- Full native ELF build, FSELF signing and companion/metadata packaging passed. Compiler uses -Wall -Wextra -Werror. Shader SPIR-V validation passed.
- FSELF container integrity valid (12 segments). ELF64 little-endian x86-64. Its loadable segments match the extracted signed container.
- 20 display orchestration scenarios passed with ASan/UBSan and the real application display source plus real public flip, event, completion, VideoOut and tile helpers. Native APIs/AGC submit and command arena are mocked here. Tests cover A/B/A, STOP before allocation/open/submission and between frames, allocation/map/open/event queue/register errors, busy unregister deferred to close, unknown unregister/close errors, failed submit/suspend, missing fence, wrong token, decode errors, event timeout and guard corruption. Uncertain ownership causes retention, not surface free.
- Manual pattern color assertions and all 2,073,600 tile coordinates checked for alignment, bounds and non-overlap. Registered image pixels and immutability checked in the orchestrator mock.
- 12 control and 11 native filesystem adapter scenarios passed with ASan/UBSan, including empty-file activation, native errno mapping, descriptor closure and fail-closed STOP handling.
- 10 existing backend host contract suites rerun and passed (compute checker/commands, dispatch, descriptors, memory, queue, coherence, cache, native prepare and alignment). Eight ASan/UBSan; last two UBSan due fixed address aperture.
- Real upstream command-arena implementation's host syscall test passed with ASan/UBSan. The exact blend shader compiled with real pinned host PSBC/ACO: gfx1013, 44 code words, three descriptors, local size 64x1x1. No mock substituted for this compiler.
- Preserved hashes checked for stable local 331HEN.elf, both existing HEN preview/test artifacts and embedded kstuff. No HEN executable, catalog, game patch, kernel code or offset changed.

## Correction found by tests

The older Gears footprint and newer SW_64K_R_X helper differ for 1080p. The first host orchestration test caught an early return that prevented all presentation. The adapter now explicitly selects the exact footprint from the same helper that produces its tile offsets (8,847,360 bytes); existing upstream/Gears source is not changed. The corrected application was rebuilt and tested.

## Runtime integration

One existing AGC session is reused after verified compute completion. Owned VideoOut handle and queue, two immutable CPU-authored scanout surfaces, 128 MiB conservative direct-memory region plus 128 KiB command arena. Real GPU flip/fence packets and strict event tokens, three submissions only. Handle closure precedes surface unmapping. Ambiguous mapping/submission/completion/close ownership parks with buffers retained. Graphics format, native imports and scanout close protocol require hardware verification. Generic Vulkan WSI remains disabled.

## Untested and limitations

No PS5 runtime test of v0.4 occurred. Owner v0.3 evidence concerns one successful 32x32 compute/readback run, not display. No game capture, motion estimation, generated frames, interpolation, game presentation insertion, game compatibility, GPU timing, FPS or latency measurement. CPU-authored A/B/A display is deliberately slow and separately gated by ENABLE_DISPLAY_TEST. A graphics driver hang/crash remains possible despite mock validation. No stable HEN release or Payload Manager source was updated.

One hardware test and required log evidence: README.md / INSTALL-AR.md. Source additions and full pinned upstream archives are provided in the separate SOURCE zip with original licenses/attributions. Build and test logs are included.
