# Owner-provided GPU diagnostic result — v0.3

On 2026-10-12 the owner supplied the native diagnostic.log after running the application on their PS5 (firmware 12.70 according to session context; firmware is not encoded in this log). Codex inspected the uploaded log; Codex did not access or operate the console.

The log reports successful native control-file opens/fstat/close, absent STOP, regular empty enable markers, AGC module loading/initialization, pipeline creation, one compute submission and GPU completion token. All 1,024 output pixels matched the CPU reference, input buffers were unchanged, and guards remained intact. All allocations were released and platform close returned success. This supports successful owned-buffer GPU compute/readback in this owner test.

CPU-monotonic submission-to-fence duration: 4260332 ns (4.260332 ms). This includes software, driver, logging and wait overhead. It is NOT an isolated GPU shader cost, FPS benchmark, input latency measurement, or evidence of performance at 1080p/4K. The tested workload consists of two CPU-authored 32×32 synthetic images, one arithmetic blend and one output.

No displayed frames or game frames were processed. Frame generation, game interception, frame insertion and presentation remain unimplemented/unverified. The log does not attest the deployed ELF/FSELF hashes. System Close Game and continued console responsiveness after this log were not independently confirmed. No HEN version is promoted by this diagnostic result.

Evidence SHA-256: 77860fb05c11af3388b61d8b06f2db23c0a55d5ed67b070073afe2f05f301d54
