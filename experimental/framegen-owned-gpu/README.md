# 331 FrameGen — GPU-result display diagnostic 0.5

A separate native PS5 application, development ID PPSA99993. It runs the existing 32×32 owned-image GPU integer blend, waits for its fence, invalidates and checks readback/guards, then optionally displays the actual verified output through CPU staging to owned tiled BGRA scanout. It does not reconstruct the GPU output on the CPU. CPU code converts RGBA to BGRA and performs nearest-neighbor magnification only.

The view sequence is comparison → enlarged result → comparison, approximately one second each. Comparison: input A on the left, actual GPU output in the center with a gold border and GPU label, input B on the right. Enlarged view shows the same output at 800×800. Both surfaces are constructed once and immutable during display. This is a static synthetic GPU-result presentation test, not moving-frame interpolation, game capture, zero-copy transfer or game frame generation.

The owner confirmed visible A/B/A and normal Close Game for v0.4; its log also reports successful compute, three exact flip events, closed VideoOut and zero final allocations. **The new v0.5 bridge has not been tested on PS5.** No independent console access or deployed-hash attestation exists for prior runs. The owner targets firmware 12.70, while the public graphics declarations originate from upstream 12.02 work. Prior successful testing does not prove every future operation or long-term stability.

## One test session

1. Close the diagnostic and all games; keep the working HEN. Back up the current `eboot.bin` and `sce_sys/param.json`.
2. Copy the new `PPSA99993-app` directory contents into `/data/homebrew/PPSA99993-app`, replacing executable/metadata. Keep `sce_module/libc.prx`. Launch through your existing native directory-homebrew loader. This is not a Payload Manager payload; do not send the inspection ELF through that loader.
3. Keep both regular empty files `ENABLE_GPU_TEST` and `ENABLE_DISPLAY_TEST` beside `eboot.bin`, with no filename extension. Ensure `/data/331FrameGen/STOP` is absent. No new control marker, HEN build or reboot is required. Both known app paths are checked using the native open/fstat/close adapter.
4. Launch once. Expect three colored-gradient panels labeled A / GPU / B, then a large single GPU panel, then the three panels again. Gold surrounds the result; RGB bars at the bottom help inspect channel order. This is intentionally a static comparison; no FPS meter or controller UI is implemented. The display may disappear after its own VideoOut handle closes.
5. Close normally with system Close Game. Copy `/data/331FrameGen/diagnostic.log` before another launch overwrites it. Report whether comparison/zoom/comparison appeared and whether Close Game worked. Success evidence: `OWNED_BLEND_READBACK_PASS`; `OWNED_GPU_RESULT_VALIDATED`; two `OWNED_RESULT_SCANOUT_PASS` lines (3072 and 1024 source texels); exactly three matching `OWNED_DISPLAY_PRESENTED` events; `OWNED_DISPLAY_FINISHED result=0 completed_presents=3 surfaces_released=1`; platform close with zero allocations.
6. Remove `ENABLE_DISPLAY_TEST` afterward for compute-only mode, or both markers to disable GPU testing. STOP has priority; lookup failures do not enable work. If the test hangs/fails, retain its log instead of repeatedly relaunching it.

## Resource and data flow

The caller passes its exact three mapped buffers after confirmed compute fence, invalidate, numerical/input validation and guard checks. Display validates the readback again before allocating resources, fingerprints the RGBA result and uses those const pointers directly to fill the two tiled views. Every source texel at its enlarged block's center is checked in the tiled buffer before registration. After the three presents, the source buffers must still validate and have the same output fingerprint.

The v0.4 native submission, suspend, exact release-fence/event-token protocol, VideoOut cleanup, tile format and allocation layout are retained. One AGC session; one owned VideoOut handle/queue; 128 MiB conservative scanout allocation with 64 MiB slot spacing and a 128 KiB command arena. Each tiled 1080p surface occupies 8,847,360 bytes plus guards. All scanout writes finish and are flushed before registration. Confirmed VideoOut closure precedes surface unmap/release. Unknown mapping/submission/completion/close ownership retains its memory and parks until system Close Game. STOP cannot cancel already submitted GPU work or guarantee recovery from a driver hang.

Generic Vulkan WSI remains disabled. There are no game frame handles, game hooks, kernel patches, firmware offsets, permission changes or second kstuff instance. Times are CPU-monotonic observations, not GPU timestamps, FPS or latency. The next unresolved gates include real GPU-side image transfer, moving owned image sequences, synthesis and any audited game adapter. A driver hang/crash remains possible on this new experimental path.

## Build and verification

Use the pinned dependencies in PINS.json and the existing SDK 0.44/Clang 18 environment:

```
bash tools/bootstrap.sh
docker --host=unix:///var/run/docker.sock build -t 331-framegen-builder .
bash tools/run_container.sh bash -c 'cd upstream/ps5-native-app-boilerplate && bash tools/rebuild-libc.sh'
bash tools/run_container.sh bash -c 'cd upstream/ps5-vulkan && python3 tools/build_psbc.py --target ps5 --jobs 3'
bash tools/run_container.sh python3 tools/build_native.py
bash tools/test_display.sh
python3 tests/result_flow_test.py
```

Tests here use Linux GCC with ASan/UBSan and host API/AGC/arena doubles, executing the actual display orchestration and pinned flip/videoout/event/tile helpers. They cover 21 ownership/STOP/error scenarios plus five invalid readback rejections before allocation, channel conversion, panel magnification, tiled staging corruption, every source texel's center/edge and all 2,073,600 unique scanout coordinates. These mocks create CPU reference data and do not attest GPU execution. Static routing checks verify the source's fence→invalidate→validation→mapped output→display sequence and unchanged compiled shader hash. Container integrity and extracted loadable ELF segments are checked separately. Reports distinguish this run from v0.4's prior 10 backend suites and real compiler/arena host tests, which were not rerun because their code is unchanged.

Runtime PSBC/ACO and the public no-offline-library option remain unchanged; no compiler identity checks were weakened. The clean-room runtime companion and independently authored local logger are retained. Existing HEN and kstuff hashes are checked and unchanged. No stable release or Payload Manager catalog is updated for this native application.

## Attribution

Own additions: Copyright (C) 2026 ANHAR / 331HEN, GPL-3.0-or-later. Display sequencing adapts Manuel Pereira's ps5-vulkan presentation source; public Gears helpers implement VideoOut, event completion and flip composition. Tile helpers retain Manuel Pereira and BlackBearReloaded attribution. Native foundation, OpenGNM and Mesa/ACO keep their original authors and file-specific licenses. Complete pinned dependency source archives accompany the SOURCE zip. OnionHEN attribution and licensing remain untouched.
