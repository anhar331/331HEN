# 331 FrameGen — feasibility and implementation gate

Research date: 2026-10-12. Target: retail PS5 firmware 12.70.
Developer/project: ANHAR / 331HEN. Source analyzed: `124382963abdf1298627743b297d0ff25f29eae7`, including the extracted-game discovery correction. OnionHEN v0.0.13 reference: `b23ffe674b2de9f62fe634944c9230ff149d593a`. No physical PS5 was accessed for this investigation.

## Decision

**NO-GO for a system-wide 331.FrameGen interception prototype, engine integration, or experimental FrameGen ELF with the interfaces verified in this investigation.**

This is an evidence-based implementation gate, not a claim that PS5 hardware can never generate frames. Public homebrew graphics projects provide real source-level approaches to GPU compute and presentation of their own images. What remains unverified is access to a running game's completed images, safe image lifetime/synchronization, and insertion into that game's presentation timeline on firmware 12.70. A counter hook or a separate homebrew rendering application does not establish those capabilities.

The requested Phase 2 says to stop if the graphics interception interface cannot be verified. Consequently Phases 2–4 were not implemented; no unknown graphics hooks, new kernel patches, fake control panel, or renamed baseline ELF were produced. Research and verification are delivered instead. A separate owned-image graphics experiment is a plausible next research project, but would not meet the requested system-wide game-frame interception requirement.

## Verified evidence and its limits

“Verified source” below means code was fetched/read at the stated revision. It does not mean its console behavior was reproduced.

| Capability | Evidence | Limit for 331 FrameGen |
| --- | --- | --- |
| Identify a running application/process | Existing daemon and native patch service | Identifies a target; does not identify its final color surfaces or GPU completion |
| PS4 BC GNM submission interception code | `source/libonion_fps/source/fps_bc.cpp` resolves GNM NIDs and installs a counter trampoline | Counts calls. No frame descriptor capture, fence capture, image import or added presentation |
| PS5 AGC counter reads | `source/libonion_fps/source/fps_agc.cpp` reads DCB/global counters through process memory translation | Private addresses/offsets, not a documented texture API; a counter does not reveal a complete image |
| Display counter observation | `source/libonion_fps/source/fps_dce.cpp` opens `/dev/dce` and reads counter at output offset 8 using its existing ioctl | Does not retrieve a framebuffer or schedule an additional flip; repeats may count as display activity |
| Export names in Payload SDK 0.44 | VideoOut, GNM and AGC import stubs exist | Stubs contain symbol declarations, not implemented drivers, typed public ABI contracts or cross-process image access |
| GPU-visible memory and compute in public experimental homebrew | `ps5-vulkan` native allocation, command submission and compute source | Source concerns its own device/resources. It does not demonstrate importing arbitrary game images into another process |
| Owned-image native presentation | `ps5-vulkan/native/present_ps5.c` and `native/wsi_present_ps5.c` | Own VideoOut handle and registered images. Does not replace a retail game's swapchain safely |
| Public shader/compiler and tiled-image research | `ps5-vulkan`, `ps5-agc-gears`, `ps5-opengl`, OpenGNM references | Useful substrate; no verified universal live-game capture/insertion adapter or 12.70 integration evidence |
| Firmware 12.70 kstuff support in existing source | kstuff-lite v1.10 `main.c` contains `FW(1270)` and 12_70 ShellCore patches | Existing homebrew compatibility does not validate private graphics ABIs or resource ownership |

### Existing FPS code is not a frame-generation backend

The BC hook resolves `sceGnmSubmitAndFlipCommandBuffersForWorkload`, `sceGnmSubmitAndFlipCommandBuffers`, and fallback submission-only exports. Its injected code copies a prologue, increments a counter and jumps back. Submission-only fallback calls cannot, by themselves, prove one unique completed frame per call. The hook does not retain arguments describing the game's registered images, detile textures, issue a compute shader, or insert a generated flip. It must not be reused as an image-interception implementation without additional ABI and lifecycle validation.

The native AGC reader uses the existing fixed ring VA `0xFE03B72EC` and a driver-relative counter offset `0x1AAC0`. These are observations embedded in existing monitoring code; they are not proposed new graphics offsets. They are insufficient for image acquisition and must not be extrapolated to other driver layouts.

The daemon samples counters at `kSampleUs = 33333`. Those aggregate samples can support telemetry but are not precise per-frame completion/presentation timestamps suitable for pacing an 8.33 ms 120 Hz schedule. Existing overlay FPS, or a display flip count, must not be relabeled as separately measured real/generated/displayed FPS.

The DCE read ioctl `0x80308217` is already used by monitoring. This investigation does not infer a framebuffer pointer or undocumented write ioctl from its output, and does not modify DCE state.

### Public native graphics changes the assessment, but not the gate

Pinned `mpereiraesaa/ps5-vulkan` source provides an experimental, non-conformant Vulkan implementation targeting gfx1013. Its queue source allocates/maps its own direct memory, creates command streams and invokes an AGC submission backend. Its native presentation code checks device ownership and resource state, restricts its owned-image path to particular 1080p BGRA surfaces, opens VideoOut, registers its buffers and waits for fence/flip events. This is significantly more than a list of GPU export names.

However, that code does not establish safe takeover of a retail game's VideoOut handle, external image ownership, tiling/compression information, GPU virtual addresses or existing fences. The native `present_ps5.c` describes its own synchronous presentation path; the repository also has an owned WSI implementation. Neither is evidence that a second daemon can import the game's surfaces or safely interleave additional presents.

Its documented companion repositories include `ps5-agc-gears`, `logging_server`, and pinned compiler/header dependencies. These are separate from the working 331HEN build. The public AGC gears README reports tests on firmware 12.02 and explicitly scopes results to the tested hardware/firmware. This does not prove 12.70 compatibility. We did not run those payloads, clone console system modules, or obtain proprietary Sony SDK materials.

OpenOrbis exposes typed PS4 VideoOut declarations, including buffer registration, submit flip and flip events. Those declarations are useful for PS4 analysis; they do not prove PS5 AGC ABI compatibility or game-agnostic access across the PS4 BC boundary.

## Architecture: AFMF versus FSR frame generation

Sony publicly describes the base PS5 as a custom RDNA 2-based GPU with unified 16 GB GDDR6 and 448 GB/s bandwidth. Similar AMD architecture does not imply compatibility with Windows Adrenalin driver extensions or their shader binaries.

AMD describes AFMF as frame generation integrated into AMD Software: Adrenalin Edition, including support for multiple PC graphics APIs. Its engine-independent placement is a driver/platform integration property. No public AFMF PS5 backend or AFMF implementation source was identified in the reviewed references. This investigation does not claim to port proprietary AFMF functionality.

The open FidelityFX frame-generation source is an architectural reference, not a drop-in console daemon. Its documented API consumes depth and motion data as well as color/frame timing resources; HUD-less images or UI composition callbacks provide UI treatment. It contains optical-flow analysis and scene-change handling, but the complete FSR pipeline does not become engine-independent merely because optical-flow shaders are available. The separate swapchain/pacing implementation owns synchronization and ordered real/interpolated presents.

A custom color-only optical-flow solution could theoretically operate without game-specific motion vectors, but would still need validated image acquisition, shader execution and presentation control. It would have weaker occlusion/HUD information and cannot be assumed to reproduce FSR quality. It would require its own measured engineering work.

The pinned FSR analytical documentation specifies HLSL shader requirements and typed UAV/format requirements. The PC implementation's shaders, descriptors, barriers and swapchain semantics require a validated PS5 backend; they cannot be passed unchanged to a symbol stub.

## Missing contracts before Phase 2 can proceed

1. **Per-frame image identity:** determine which registered surface is the final game image, including width, height, pitch, format, HDR encoding, tiling, compression metadata, views and GPU VA. An eboot address or process memory mapping is not sufficient.
2. **Completion and lifetime:** identify a render-completion fence, retain two consecutive completed images, and prevent the game or display engine from recycling them while interpolation reads them. CPU visibility alone does not establish GPU cache coherency or completion.
3. **Verified execution context:** use a supported compute/shader backend with correct GPU permissions, descriptors, barriers and scheduling. Cross-process GPU VAs and handles must not be guessed or treated as interchangeable.
4. **Presentation ownership:** verify the game's real flip path and the legal mechanism to introduce an extra generated buffer without stealing a handle, desynchronizing events, starving the game or replacing its original frame incorrectly.
5. **Separate BC/native adapters:** validate GNM/VideoOut behavior for PS4 BC and AGC/VideoOut behavior for native PS5; common names or counters do not establish common contracts.
6. **12.70 validation:** identify the actual loaded module versions and verify each required ABI on the target firmware. Existing successful kstuff testing cannot substitute for this.

These are technical access/ownership blockers, not an unavailable C++ compiler or a missing XML database. This report does not prescribe new kernel offsets, fabricate function signatures, or install a second GPU engine into 331HEN.

## Permissions and stability

The existing stack can inspect processes, resolve libraries and use privileged memory operations. Those privileges are not proof that another process's VideoOut handle or GPU resources can be accessed safely. A future in-process adapter would need correct loader permissions and verified module/surface ABI, while a system-level adapter would need validated ownership/authorization at the graphics-service boundary.

kstuff is retained byte-for-byte. It enables existing loading/patch functionality; it is not an AFMF driver or a general display compositor extension. No kernel/GPU driver replacement is justified by the available evidence.

Misidentifying a surface, changing display buffers still in use, submitting invalid commands, mishandling fences or freeing in-flight resources can cause corruption, hangs, GPU timeouts or kernel panic. Additional hooks can also conflict with the existing BC counter hook. An eventual prototype must audit instruction relocation, active-thread safety and hook ownership before installation; the existence of a trampoline is not a safety proof.

Future isolation requirements: separate experimental module, excluded from ordinary release builds, OFF by default, one instance per process generation, explicit hook ownership, no mutation when unsupported, a kill switch that stops accepting work, and fence-safe teardown. A GPU timeout may prevent prompt reclamation; a kill switch must not promise recovery from every GPU hang. UI controls are gated on a working backend, not on mock statistics.

## Memory, bandwidth, performance and latency

The following are calculated lower bounds for three **uncompressed** full-resolution surfaces: two inputs and one output. They exclude tiling padding, alignment, fences, staging, history, optical-flow pyramids, intermediate vectors, UI separation and driver allocations. They are not measurements or a claim of available spare memory.

| Resolution | Three RGBA8 surfaces | Three RGBA16F surfaces |
| --- | ---: | ---: |
| 1920 × 1080 | 23.73 MiB | 47.46 MiB |
| 2560 × 1440 | 42.19 MiB | 84.38 MiB |
| 3840 × 2160 | 94.92 MiB | 189.84 MiB |

At 4K RGBA8, a single pass reading two images and writing one moves about 99.53 MB per generated image: approximately 2.99 GB/s at 30 generated images/s or 5.97 GB/s at 60. Multi-pass flow/warping and cache behavior add traffic. Peak advertised memory bandwidth is shared with the game's workload and is not a timing guarantee. CPU readback/upload would add transfers and synchronization and is not proposed as a production workaround.

AMD's pinned analytical FSR documentation separately reports example memory use of 124/214/457 MB at 1080p/1440p/4K on its stated PC test GPU/API, excluding swapchain overhead. These are AMD's PC examples, not PS5 measurements or PS5 allocation targets.

30→60 requires useful output intervals around 16.67 ms; 60→120 around 8.33 ms, plus a display/output mode capable of the requested rate. GPU work, fences and pacing must fit the schedule while the game continues rendering. Generating a frame adds work; it does not double simulation rate or input responsiveness. A fully busy GPU may reduce the real frame rate when extra compute is added.

Interpolation between two completed real frames requires the later frame, creating a lookahead/buffering tradeoff. Real-frame periods are 33.33 ms at 30 FPS and 16.67 ms at 60 FPS, but these are not measured added latency. Exact input-to-photon delay requires an instrumented input/display test; no reliable latency-reduction capability is established here.

Without engine depth/motion/HUD layers, expected failure cases include disocclusion, thin geometry, particle motion, subtitles/HUD ghosting, camera cuts, menus and motion discontinuities. Future policy must disable generation on scene changes or unstable timing using measured criteria, without inventing compatibility claims.

## Implementation and verification status

| Requested status | Result |
| --- | --- |
| Source analysis | Completed for the files/revisions cited; not an exhaustive proof about all PS5 software |
| 331.FrameGen prototype compiled | **Not implemented**: interception gate unmet |
| GPU interception of a game verified | **No** |
| Frame insertion into a game verified | **No** |
| Frame generation verified | **No** |
| PS5 hardware tested by this investigation | **No** |
| Existing 331HEN baseline build/tests | See `docs/framegen/VERIFICATION.json`; separate from FrameGen capability |

Exact new tracked files: this report, `docs/framegen/REFERENCES.json`, and `docs/framegen/VERIFICATION.json`. No runtime source, menus, configuration, game files, firmware offsets or kernel patches are changed for FrameGen. Saved ELF and kstuff hashes are checked before/after baseline verification. No `331HEN-FrameGen-EXPERIMENTAL.elf` is generated or published. Build logs and evidence are retained under `.cache/framegen-research-tests` and `Release/FrameGen-Research`.

## Answers and next engineering steps

- Universal frame generation through the currently verified interfaces: **not established; NO-GO now**.
- Without game-specific XML patches: theoretically possible with a platform adapter, but no verified implementation here. Driver/API version adapters may still be required.
- Both PS4 and PS5: no demonstrated combined solution; their submission paths need separate validation.
- Confirmed interception methods: existing source-level BC submission counter hook, native memory counter reads and DCE counter observation. None supplies two complete GPU game frames and controlled insertion.
- Implemented/compiled: feasibility/evidence documentation and verification of the existing baseline; no FrameGen runtime.

To reopen the gate, first validate a public graphics backend **inside an isolated, owned-image homebrew diagnostic** on 12.70, using a reviewed reproducible build and documented licensing. Demonstrate shader dispatch, fence completion, owned-image copying and original/extra presentations. This proves only the substrate, not game support.

Then identify a permitted, typed presentation-registration and flip adapter for one known module version, validate its ABI and thread/resource lifecycle, and capture two completed game surfaces without modifying their presentation. Require image hashes/visual captures tied to real fence timestamps, format/tiling validation and bounded resource retention. Do not submit arbitrary commands to a retail game's queue while this is unproven.

Only after those tests succeed should engineering attempt an extra known test image with unchanged original ordering and measured pacing, followed by simple interpolation and then optical flow/occlusion/UI handling. PS4 BC should be investigated separately from native PS5. Add Toolbox controls only after a working backend exists. No blind game-hook hardware procedure is recommended at this stage.

## References

Pinned URLs and download hashes are recorded in `framegen/REFERENCES.json`. Public claims remain attributed to their publishers.

- [Sony PS5 hardware specifications](https://blog.playstation.com/2020/03/18/unveiling-new-details-of-playstation-5-hardware-technical-specs/).
- [AMD AFMF overview](https://www.amd.com/en/products/software/adrenalin/afmf.html).
- [Payload SDK 0.44 VideoOut stubs](https://github.com/ps5-payload-dev/sdk/blob/f5d40f02770e9b0001dd9e18056eca70b3da8b40/sce_stubs/libSceVideoOut.c), [AGC driver stubs](https://github.com/ps5-payload-dev/sdk/blob/f5d40f02770e9b0001dd9e18056eca70b3da8b40/sce_stubs/libSceAgcDriver.c).
- [OnionHEN v0.0.13 FPS BC hook](https://github.com/aydencharles/onionHEN/blob/b23ffe674b2de9f62fe634944c9230ff149d593a/source/libonion_fps/source/fps_bc.cpp).
- [FidelityFX analytical frame generation](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/blob/60f4ea81909200d8542eca14dccb2628b763a9a3/Kits/FidelityFX/docs/techniques/frame-interpolation.md), [frame-generation API](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/blob/60f4ea81909200d8542eca14dccb2628b763a9a3/Kits/FidelityFX/docs/techniques/frame-interpolation-api.md), [swapchain/pacing](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/blob/60f4ea81909200d8542eca14dccb2628b763a9a3/Kits/FidelityFX/docs/techniques/frame-interpolation-swap-chain.md).
- [ps5-vulkan source and limitations](https://github.com/mpereiraesaa/ps5-vulkan/tree/e6d3c3f82f3c8a574e28c174b15b287f9c0ecc6d), particularly native queue/presentation source, API.md, BUILDING.md and VALIDATION.md.
- [ps5-agc-gears source and scoped hardware claims](https://github.com/mpereiraesaa/ps5-agc-gears/tree/1ae1f9182abd2770c131b97419034fb85173c2dc).
- [ps5-opengl source/provenance reference](https://github.com/blackbearreloaded/ps5-opengl/tree/7f9bfabdddb187a11e4401058eba8c9e55194d0a).
- [OpenOrbis PS4 VideoOut header](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/blob/0a1aaf9dd4a92695538bdeb09fb056d06dd11725/include/orbis/VideoOut.h).
- [Existing kstuff-lite v1.10 source](https://github.com/EchoStretch/kstuff-lite/tree/3b2ab4a16ba43c8ab38daf7ec83cc297014e0e47).

331HEN retains OnionHEN developer attribution and GPL-3.0 licensing. Public references have their own licenses (including GPL-compatible homebrew sources and per-file FidelityFX notices). No third-party implementation code, Sony modules, private credentials or game binaries are incorporated by this research change. Any future reuse must preserve the exact applicable notices and source obligations.
