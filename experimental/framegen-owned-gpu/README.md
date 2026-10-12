# 331 FrameGen — owned-image GPU diagnostic 0.2

This is an isolated native PS5 **application**, not a 331HEN replacement or a Payload Manager ELF payload. Its only GPU workload computes a per-channel integer average of two CPU-authored 32×32 RGBA images into a third buffer. It checks 1,024 output pixels, unchanged inputs and surrounding guards after a completion fence. This is a numerical GPU substrate experiment, not motion estimation, game capture or frame generation.

No VideoOut handle is opened; presentation is explicitly unavailable. No game is hooked, no original game files are changed, and no kstuff/kernel modifications are included. GPU execution is OFF by default. Version 0.2 logs each control lookup separately and honors STOP before checking either marker; unexpected STOP lookup errors disable initialization. A successful build or host compiler test does not prove native GPU operation on firmware 12.70.

## Outputs

- `Release/PPSA99993-app/eboot.bin`: real compiled native FSELF and complete title directory.
- `Release/331-FrameGen-GPU-Diagnostic.elf`: the same native application's converted ELF for inspection; **do not send it through Payload Manager**.
- `Release/BUILD-MANIFEST.json`, `BUILD-REPORT.md`, `SHA256SUMS.txt`.
- `logs`: compiler, build, host contracts, reference and static inspection results.

PPSA99993 is a development identity, not a Sony assignment. Do not overwrite an existing application with that ID.

## One bounded hardware test session (untested on PS5)

1. Keep the existing tested HEN. Close all games. This diagnostic does not require a new HEN build or game patch.
2. Use the existing native directory-homebrew loader (the same kind of loader used for extracted titles). Copy `PPSA99993-app` intact to `/data/homebrew/PPSA99993-app`; the companion `sce_module/libc.prx` is required. Do not install it over another title. Loader-specific registration may be necessary; this package is not an installer.
3. For an optional loader-only check, launch without an enable marker. No AGC init or GPU commands are issued; it records DISABLED then waits for system Close Game. No picture is expected.
4. For the actual compute test, place an empty file named `ENABLE_GPU_TEST` in the application directory, where it normally appears as `/app0/ENABLE_GPU_TEST`. Version 0.2 also checks the exact installed location `/data/homebrew/PPSA99993-app/ENABLE_GPU_TEST` when the loader does not expose the mount. Both paths require a regular empty file. Ensure `/data/331FrameGen/STOP` is absent. Launch the diagnostic once.
5. It performs one compute dispatch (16 groups of 64 threads). It waits up to five seconds for the Vulkan fence, validates readback and guards, releases resources on confirmed completion, then waits for system **Close Game**. There is no rendered screen or controller UI.
6. Close the app through the system. Retrieve `/data/331FrameGen/diagnostic.log` through existing FTP. Required success evidence: `OWNED_BLEND_READBACK_PASS`, intact inputs/guards and `DIAGNOSTIC_FINISHED readback_pass=1`, accompanied by the exact ELF/FSELF hashes. Confirm the system remains responsive and the app closes normally. No result is assumed in advance.
7. Remove the enable marker after the test. An existing `/data/331FrameGen/STOP` prevents initialization/submission; it cannot abort already submitted GPU work. On unknown submission ownership or a timeout the code retains resources instead of freeing in-flight memory. Do not repeat a failed test blindly; retain its log.

The log is overwritten only for this diagnostic's next launch; retrieve it before launching again. Failure of AGC initialization, memory allocation, shader compilation or readback is a failed/unavailable diagnostic, not evidence of FrameGen support. A graphics-driver failure can still hang or crash an untested console; neither a fence timeout nor a stop file guarantees recovery from a GPU hang.

## Build and verification

The build uses the existing `331hen-builder` SDK 0.44 / Clang 18 environment as its base and a separate `331-framegen-builder` image. Required additional tools are glslang, SPIR-V Tools, Mako, PyYAML and wget. Public upstream sources are pinned in `PINS.json`; run `bash tools/bootstrap.sh` in a fresh checkout, then:

```
docker --host=unix:///var/run/docker.sock build -t 331-framegen-builder .
bash tools/run_container.sh bash -c 'cd upstream/ps5-native-app-boilerplate && bash tools/rebuild-libc.sh'
bash tools/run_container.sh bash -c 'cd upstream/ps5-vulkan && python3 tools/build_psbc.py --target ps5 --jobs 3'
bash tools/run_container.sh python3 tools/build_native.py
bash tools/run_container.sh make -C upstream/ps5-vulkan -f Makefile -f /workspace/331-FrameGen/tools/check.mk check331 LAB_SIBLINGS=/workspace/331-FrameGen/lab/projects
```

The foundation's clean-room runtime is rebuilt twice and checked against its upstream digest. No proprietary Sony modules or shader packages are required. The upstream private network logger is replaced with an independently authored local-file logger. Runtime PSBC/ACO compilation and the upstream `PS5VK_NO_OFFLINE_LIBRARY` path avoid dependency on the developer's historical LLPC executable/control hash; no compiler identity check was weakened or fake GPU implementation substituted.

Eight backend contract suites use ASan/UBSan. Native command aperture and allocation-policy syscall doubles use UBSan: their fixed descriptor aperture overlaps ASan's shadow gap. These tests model ownership, errors, coherence and encoding; they do not execute a PS5 GPU. The exact SPIR-V is also compiled on the host through the pinned PSBC/ACO adapter to gfx1013 code with three storage descriptors and local size 64×1×1. CPU reference tests and shader validation are separate from GPU execution.

The recorded duration is CPU-monotonic submission-to-fence time, including driver/wait overhead. It is not an isolated GPU timestamp, FrameGen cost, displayed FPS or input latency measurement.

## Remaining engineering gates

Actual compute execution and clean closure must first be verified on 12.70. Then investigate GPU-side image copies and **owned** VideoOut presentation with validated formats/fences. Only after those steps may an audited live-game image acquisition/presentation adapter be considered. The system-wide FrameGen NO-GO decision remains in force: no game frames, inserted frames, optical flow or PS4/PS5 universal compatibility are verified here.

## Licensing and attribution

Own diagnostic code: Copyright (C) 2026 ANHAR / 331HEN, GPL-3.0-or-later. Upstream PS5 graphics/compiler/native foundation retain their original authors and file-specific licenses. See PINS.json and the corresponding source archives. Manuel Pereira's ps5-vulkan/ps5-agc-gears, BlackBearReloaded's native-app foundation, OpenGNM, Mesa/ACO and their contributors are technical foundations, not code credited solely to this project. Existing OnionHEN credits and GPL licensing remain untouched in 331HEN.
