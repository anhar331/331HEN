# 331 FrameGen — owned-display diagnostic 0.4

This is an isolated native PS5 application (development Title ID PPSA99993). It first runs the existing owned 32×32 GPU blend/readback test, then optionally presents two immutable CPU-authored 1920×1080 images in the sequence A/B/A. Three GPU submissions, release fences and matching VideoOut events must complete before its resources are released. This is an owned-image presentation experiment, not game capture, interpolation or frame generation. The GPU blend output is not what this stage displays.

Version 0.3's compute/readback succeeded in an owner-provided console log. **Version 0.4's new display path has not been tested on PS5.** No console is remotely accessed. The public presentation format/import declarations were measured upstream on firmware 12.02; their availability on the user's 12.70 requires this test. A successful compile or mock cannot establish that compatibility.

## One hardware test

1. Close this app and all games. Keep your current working 331HEN. Back up the previous diagnostic's `eboot.bin` and `sce_sys/param.json`.
2. Copy the new `PPSA99993-app` folder's contents into `/data/homebrew/PPSA99993-app`, replacing the compiled application and metadata. Keep its required `sce_module/libc.prx`. Use your existing native directory-homebrew loader; the `.elf` in the package is for inspection and must not be sent through Payload Manager.
3. Leave an empty regular file `ENABLE_GPU_TEST` beside `eboot.bin`. Add another empty regular file **`ENABLE_DISPLAY_TEST`** in the same directory. Both are required. Ensure `/data/331FrameGen/STOP` is absent. These controls use the native open/fstat/close adapter confirmed by the owner's v0.3 log; symlinks, nonempty files and lookup failures do not enable the test.
4. Launch once. Expected picture: blue background, white border, gold square on the left with letter A; then square on the right with B; then A again. Each is held approximately one second. Red/green/blue bars at the bottom help inspect channel order. The image may disappear after VideoOut closes; this is expected. No FPS counter or interactive controller menu is implemented.
5. Close through the system's **Close Game**. Copy `/data/331FrameGen/diagnostic.log` before launching again; the next launch overwrites it. Report whether the A/B/A images and colors appeared correctly and whether Close Game worked. Required log evidence: compute readback PASS; exactly three `OWNED_DISPLAY_PRESENTED` lines with matching events; `OWNED_DISPLAY_FINISHED result=0 completed_presents=3 surfaces_released=1`; platform close with zero remaining allocations. Record the deployed eboot checksum if possible.
6. Remove `ENABLE_DISPLAY_TEST` afterwards. Removing it restores compute-only behavior; removing both enable markers disables GPU testing. STOP takes precedence. No reboot or new HEN build is required for these file changes.

STOP cannot cancel GPU work already submitted. A missing fence, malformed/mismatched event or uncertain mapping/close causes the test to retain its own memory and park until system Close Game. If it fails or hangs, keep the log and avoid blindly repeating it. This is a new experimental GPU/VideoOut path and could hang or crash on hardware despite passing the host checks.

## Implementation and limits

The application reuses its one live AGC session, opens its own VideoOut handle/event queue and registers only its own two immutable surfaces. It allocates a conservative 128 MiB scanout region with slots 64 MiB apart, plus a 128 KiB command arena; each actual 1080p tiled image occupies 8,847,360 bytes. The SW_64K_R_X footprint and pixel offsets both come from the same pinned Vulkan helper, replacing the different older Gears footprint calculation for this adapter. Images and guards are flushed before registration and are not modified while in use.

Each submission composes the upstream flip and release-fence packet, calls the real AGC submit/suspend imports, waits for the fence, then checks the exact per-frame VideoOut token. The matching event and fence establish completion in the public reference protocol; successful handle closure is additionally required before surface unmapping/release. Unexpected ownership errors retain resources. Generic Vulkan WSI remains disabled. There are no game handles, game frame hooks, kernel patches, firmware offsets, permissions elevation or second kstuff instance.

Logged timestamps are CPU-monotonic event observations. The deliberate one-second sleeps are for visual inspection, not a frame pacing benchmark. No GPU timestamps, measured input latency, generated frames, 60 FPS output or system-wide game FrameGen are claimed. Real GPU-backed image copies, live-game acquisition, image synthesis and game presentation insertion remain unresolved engineering gates.

## Build and host verification

Pinned dependencies: `PINS.json`. SDK 0.44 / Clang 18, the existing `331hen-builder` base, and a separate `331-framegen-builder` image. The clean-room runtime companion is retained. Build:

```
bash tools/bootstrap.sh
docker --host=unix:///var/run/docker.sock build -t 331-framegen-builder .
bash tools/run_container.sh bash -c 'cd upstream/ps5-native-app-boilerplate && bash tools/rebuild-libc.sh'
bash tools/run_container.sh bash -c 'cd upstream/ps5-vulkan && python3 tools/build_psbc.py --target ps5 --jobs 3'
bash tools/run_container.sh python3 tools/build_native.py
bash tools/test_display.sh
bash tools/run_container.sh make -C upstream/ps5-vulkan -f Makefile -f /workspace/331-FrameGen/tools/check.mk check331 LAB_SIBLINGS=/workspace/331-FrameGen/lab/projects
```

`tools/test_display.sh` uses GCC plus ASan/UBSan on Linux. It tests the actual display orchestration and upstream flip/event/videoout helpers with host syscall, command-arena and AGC doubles, including failure/retention scenarios; it does not execute GPU commands. The real command-arena source is tested separately with upstream syscall doubles. See the build report and verification JSON for exact scope. Runtime compiler PSBC/ACO and the public no-offline-library option are used without weakening compiler identity checks. The independently authored logger writes locally; no private network logger is needed.

## Attribution and source

Own additions: Copyright (C) 2026 ANHAR / 331HEN, GPL-3.0-or-later. Display sequencing adapts Manuel Pereira's `ps5-vulkan/native/present_ps5.c`; Gears contributes its public presentation, event and VideoOut helpers. Tile helpers retain Manuel Pereira and BlackBearReloaded attribution. Native foundation, OpenGNM and Mesa/ACO retain their original authors and file-specific licenses. Complete pinned upstream source archives accompany the source package. Existing OnionHEN attribution, HEN releases and kstuff remain untouched.
