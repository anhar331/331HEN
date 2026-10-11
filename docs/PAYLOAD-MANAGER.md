# Payload Manager source and approved updates

Source URL to paste in Manage Sources → Add a New Source:

    https://raw.githubusercontent.com/anhar331/331HEN/main/payloads.json

## Official format verified

Official default: https://itsplk.github.io/ps5-payloads-mirror/payloads.json
Official manager source inspected at commit `e4e72dc9d831dd09ee8fabc222ad1e7f5139050e`:

- [Custom source specification](https://github.com/itsPLK/ps5-payload-manager/blob/e4e72dc9d831dd09ee8fabc222ad1e7f5139050e/CUSTOM_REPOSITORIES.md)
- [Actual payload parser](https://github.com/itsPLK/ps5-payload-manager/blob/e4e72dc9d831dd09ee8fabc222ad1e7f5139050e/src/repository.c)
- [Add-source validation and installation](https://github.com/itsPLK/ps5-payload-manager/blob/e4e72dc9d831dd09ee8fabc222ad1e7f5139050e/src/sources.c)

The mirror uses an array. Custom sources are documented as an object with `name` **before** the `payloads` array. Each payload requires string `name`, `filename`, `url`. Supported optional strings include `description`, `version`, `category`, `checksum` (SHA-256), `source`, `source_direct`, `last_update`. `url` is the binary download; `source_direct` is metadata, not a substitute for `url`. No archive extraction is needed for this ELF.

Our source uses the documented custom wrapper. Both catalog and payload are named `331HEN`. The current C parser scans from each opening brace to the first closing brace, so it reads the wrapper's first `name` for the first item; matching the names avoids a wrong display name. Metadata has no nested objects or brace/escape characters. The actual pinned official C function and JSON helpers are compiled and exercised by `scripts/test_payload_manager_parser.py`, including ASan/UBSan, field comparisons, mirror-array input, empty and missing-URL cases.

## Preserved baseline

Initial default: `v0.0.13-331.2`, SHA-256 `72f00d524636f8c96746d551fb495c1af319048a6af3703ffe5ae19e6abc197d`. This is the exact existing local `Release/331HEN.elf`, not the Game Patches experiment (`88a0d3...`). Source is retained separately on `preserved-working-source` and attached in full, with original GPL-3.0 license and developer attribution.

The owner reports the existing 331HEN project working on firmware 12.70. Codex did not perform a PS5 test or independently confirm the exact checksum on hardware. Archived build reports retain their original runtime-unverified status. The GitHub release remains conservatively a pre-release; it is selected only as the expressly requested preserved baseline. This does not promote Game Patches to stable or claim new hardware testing.

The one-time **Publish exact preserved 331HEN baseline** workflow recovers a pending empty draft after the Cloud upload endpoint rejected authentication (HTTP 401), using the runner's short-lived GITHUB_TOKEN. It reproduces the preserved source and refuses any binary whose SHA differs from the existing working ELF. It cannot replace assets or modify an already published release.

## Future releases

1. Run **Publish 331HEN pre-release**, as documented in RELEASING.md. It creates a new incrementing release and verifies downloads. This alone does **not** change the approved default source.
2. After actual PS5 testing, open **Update approved Payload Manager source** → Run workflow. Enter the published release tag, the exact tested ELF SHA-256, firmware `12.70`, actual test results, and check the confirmation that this binary was tested. Leave tag blank to reverify the current source without approval.
3. The workflow verifies public release metadata, all required assets, anonymously downloads the exact ELF and SHA256SUMS.txt, verifies hash and ELF structure, tests source policy and the official C parser, then commits approval and JSON together. It also verifies the exact public raw JSON URL.

On release publication the source-refresh workflow runs automatically, but only recorded approved versions can be selected. The pre-release publisher also verifies the current approved source directly, because releases made with GITHUB_TOKEN do not trigger another workflow. Future versions enter the feed automatically during their hardware-approval workflow; unapproved newer releases are ignored.

`distribution/approved.json` is append-only approval history. Only the exact fixed existing baseline can use the preserved-baseline exception. All future entries require an actual hardware-test declaration tied to hash, firmware, actor and evidence. Older versions, changed hashes, draft/missing assets, failed downloads or checksum mismatches cannot replace the working source. Releases and tags are never overwritten or deleted. The workflow does not relabel a previous pre-release as stable.

Raw GitHub and asset downloads must be reachable from the PS5's network. Cloud validation tests public HTTP, real parser compatibility and ELF integrity; it does not test your PS5's network or installation.
