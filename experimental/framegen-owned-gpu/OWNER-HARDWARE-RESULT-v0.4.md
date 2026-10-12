# Owner hardware evidence — owned-display v0.4

Evidence received 2026-10-12: owner-provided diagnostic.log, SHA-256 `c5e8c9793e8951eed3cccf7081a4ab335d5e4e1ab2583693b4fce697f9508e54`. No independent console access or deployed eboot hash attestation. The project target is owner-reported PS5 firmware 12.70; this log does not itself identify firmware.

The log records native marker lookup, successful AGC init and one owned compute/readback check of 1,024 pixels (unchanged inputs and intact guards). CPU submission-to-fence observation is 4,255,280 ns; this is not GPU-only timing.

The subsequent owned display sequence completed three submissions in slots 0/1/0. All have rc=0, suspend_rc=0, fence=0 and a matching event for tokens 3508988280833/834/835. VideoOut close returned rc=0, three presents completed, surfaces_released=1, and platform close returned rc=0 with allocations_bytes=0. No logged failure or timeout. The owner answered yes when asked whether A/B/A was visually visible and system Close Game worked.

This verifies one owner-observed run of immutable CPU-authored owned-image display and clean closure, scoped to that evidence. It does not verify GPU-result display in v0.5, zero-copy image transfer, generated game frames, graphics interception, frame pacing, input latency or long-term stability.
