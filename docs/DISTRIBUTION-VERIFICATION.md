# Payload Manager distribution verification — 2026-10-11

Follow-up display correction: the first catalog used a fixed saved filename and did not test StorageHub's installed-file filtering. The user reported **ALL PAYLOADS INSTALLED** after adding the source. Inspection reproduced the exact cause: the manager hides an exact filename match, without checking version or hash. The source now saves as `331HEN_v0.0.13-331.2.elf`; asset URL, release, binary and hash are unchanged. The real official JavaScript logic passed five installed/update-display cases, and the real C folder-name function maps both filename formats to `331HEN`. See [refresh and update behavior](PAYLOAD-MANAGER.md#installed-file-filtering-and-saved-filename). Earlier results below concern parser, HTTP and ELF validation, not hardware UI verification.

Public JSON URL:

    https://raw.githubusercontent.com/anhar331/331HEN/main/payloads.json

Published preserved release: https://github.com/anhar331/331HEN/releases/tag/v0.0.13-331.2

Direct asset: https://github.com/anhar331/331HEN/releases/download/v0.0.13-331.2/331HEN.elf

ELF size: **3,312,272 bytes**. SHA-256:

    72f00d524636f8c96746d551fb495c1af319048a6af3703ffe5ae19e6abc197d

## Verified results

- Public raw JSON returned HTTP 200, parsed successfully, matched the committed catalog, and returned `Access-Control-Allow-Origin: *`.
- Following its exact `url` anonymously returned HTTP 200 and an ELF byte-identical to the preserved local `Release/331HEN.elf`. JSON `checksum`, SHA256SUMS.txt and computed SHA-256 agree.
- ELF64 little-endian x86-64 PIE structure, segment bounds/alignment/entry point, compressed bootstrapper and original embedded kstuff checks passed. kstuff SHA-256 remains `82cbb1e70f8a562317d6b65269cf6aa0d7d94e22df4f1ac89a260ca0ec261252`.
- [Isolated full SDK/Docker rebuild and successful release upload](https://github.com/anhar331/331HEN/actions/runs/38149775749) reproduced the exact preserved binary SHA-256. The compiled experimental source and local stable ELF were not replaced.
- Official Payload Manager `v0.5.2`/commit `e4e72dc9d831dd09ee8fabc222ad1e7f5139050e` C parser and its real JSON helpers were compiled with ASan/UBSan. The actual catalog, mirror-array layout, required field values, empty source and missing-URL cases passed locally and in Actions.
- **12 Python publishing/source-policy tests passed**, covering approvals, exact baseline-only exception, future hardware-test requirement, downgrade/hash-change prevention, missing/draft assets, network-failure preservation, incorrect binaries/checksums/test reports, and ignoring newer unapproved releases. Workflow syntax and `git diff --check` passed.
- [Approved-source workflow, including public raw JSON verification](https://github.com/anhar331/331HEN/actions/runs/38150211305) passed. No new hardware approval was fabricated; the workflow was run with tag empty to verify the existing approved baseline.
- Public release assets independently downloaded: ELF, SHA256SUMS.txt, test results, firmware compatibility, LICENSE, corresponding source, changes and fresh static validation. Their downloaded hashes agree with SHA256SUMS.txt.
- All **20 pre-existing local Release files** match the working backup byte-for-byte. The prior experimental release `v0.0.13-331.1` remains available and its ELF still hashes to `88a0d3d64cf5ba35fd1e67de51f57599844390c7668b7b08027edc313b46433c`.
- All **4,776 preserved regular source files** match the public corresponding source archive byte-for-byte. Its source branch also retains one original unused upstream icon; the exact reproduced ELF hash establishes it does not change the executable.
- No main-branch payload C/C++, kernel, offsets, runtime paths, cheat files, kstuff or UI/resource source changed for this distribution fix. GPL-3.0 licensing and original attribution remain included. Source exporting uses the existing credential/private-path checks and exact pinned public crypto-fixture exceptions.

## Runtime and release status

The archived preserved build reports **435 host cases passed, zero failed**; these are prior recorded results, not a new hardware test. The owner reports their existing 331HEN project works on PS5 firmware 12.70. Codex did not execute the ELF on PS5 or independently confirm its exact checksum on hardware.

The release therefore remains conservatively marked pre-release. The default source expressly selects the unchanged preserved existing build; it does not promote the untested Game Patches experiment to stable. The source workflow requires a real owner-confirmed test of an exact published hash for every future version. It commits approved metadata and JSON together after verification, without overwriting/deleting releases or replacing assets.

Cloud uploads to `uploads.github.com` returned HTTP 401 even though repository operations worked. Publishing was completed through the isolated GitHub Actions runner with its short-lived GITHUB_TOKEN. An initial run found that the public by-tag API hides draft releases; draft discovery was corrected using authenticated listing, and the subsequent build/upload and source-verification runs succeeded. Future distribution uses the working Actions workflow; manual transfers are unnecessary.

Payload Manager installation/navigation and the PS5's own network access have not been tested on hardware. Public HTTP, official parser compatibility and ELF integrity are verified; they do not prove console runtime behavior. See [source/update instructions](PAYLOAD-MANAGER.md).
