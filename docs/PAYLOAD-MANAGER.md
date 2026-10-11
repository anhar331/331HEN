# Payload Manager source and approved updates

Source URL to paste in Manage Sources → Add a New Source:

    https://raw.githubusercontent.com/anhar331/331HEN/main/payloads.json

## Official format verified

Official default: https://itsplk.github.io/ps5-payloads-mirror/payloads.json
Official manager source inspected at release `v0.5.2`, commit `e4e72dc9d831dd09ee8fabc222ad1e7f5139050e`:

- [Custom source specification](https://github.com/itsPLK/ps5-payload-manager/blob/e4e72dc9d831dd09ee8fabc222ad1e7f5139050e/CUSTOM_REPOSITORIES.md)
- [Actual payload parser](https://github.com/itsPLK/ps5-payload-manager/blob/e4e72dc9d831dd09ee8fabc222ad1e7f5139050e/src/repository.c)
- [Add-source validation and installation](https://github.com/itsPLK/ps5-payload-manager/blob/e4e72dc9d831dd09ee8fabc222ad1e7f5139050e/src/sources.c)

The mirror uses an array. Custom sources are documented as an object with `name` **before** the `payloads` array. Each payload requires string `name`, `filename`, `url`. Supported optional strings include `description`, `version`, `category`, `checksum` (SHA-256), `source`, `source_direct`, `last_update`. `url` is the binary download; `source_direct` is metadata, not a substitute for `url`. No archive extraction is needed for this ELF.

Our source uses the documented custom wrapper. Both catalog and payload are named `331HEN`. The current C parser scans from each opening brace to the first closing brace, so it reads the wrapper's first `name` for the first item; matching the names avoids a wrong display name. Metadata has no nested objects or brace/escape characters. The actual pinned official C function and JSON helpers are compiled and exercised by `scripts/test_payload_manager_parser.py`, including ASan/UBSan, field comparisons, mirror-array input, empty and missing-URL cases.

## Installed-file filtering and saved filename

The official StorageHub hides an item when its `filename` exactly matches an installed filename. It does **not** compare the JSON version or checksum when deciding to hide it. A nonempty catalog can therefore show **ALL PAYLOADS INSTALLED**. This happened with our original fixed `filename: 331HEN.elf`; the initial parser-only validation did not cover frontend filtering.

The catalog now uses `331HEN_v0.0.13-331.2.elf` as the **saved filename**. The immutable downloaded asset is still named `331HEN.elf`, with the same URL and SHA-256. The updater generates a new saved filename for each future approved tag. The manager's real frontend and backend strip the `_v...` suffix to identify the existing `331HEN` family, so an unversioned or older versioned installation displays an Update item. Once that exact saved version is installed, hiding it is expected.

After this catalog change, press the circular refresh icon in the Repository section of Payload Manager and expand 331HEN. There is no need to delete a payload, restart the PS5 or remove the source to refresh its catalog. The current entry downloads the same preserved baseline; its Update label reflects the distribution filename, not new Game Patches functionality.

`scripts/test_payload_manager_display.py` executes the exact pinned official JavaScript filename normalization, enrichment and available-item filter. It reproduces the original empty display, checks the corrected legacy installation, fresh installation, already-installed exact version and future-version cases. The C parser test additionally exercises the actual backend folder-name function for both old and versioned names. Tests run in the approved-source workflow before committing updates.

If you choose Install/Update, Payload Manager itself asks to replace the existing version, updates its autoload filename and removes old files in its managed payload folder. Changing or refreshing this JSON performs none of those console operations; this fix does not access the PS5 or change the existing local Release ELF.

## Optional separate Game Patches test

At the owner's explicit request, the same source now has a second item, **331HEN Game Patches TEST**. It downloads the already-published `v0.0.13-331.1/331HEN.elf` asset (SHA-256 `88a0d3d64cf5ba35fd1e67de51f57599844390c7668b7b08027edc313b46433c`) and saves it as `331HEN-GamePatches-TEST_v0.0.13-331.1.elf`.

The official frontend and backend normalize that filename to `331HEN-GamePatches-TEST`, while the approved item normalizes to `331HEN`. Installation uses different manager storage folders. The experiment therefore appears as a separate new download against a stable-only installation, with no stable-file replacement or stable autoload filename update. Future experimental filenames remain in their own family. The original approved catalog entry is unchanged.

Experiments are explicitly recorded in `distribution/experimental.json`, separate from `distribution/approved.json`. Refreshes verify both records, actual uploaded assets, SHA256SUMS.txt, exact test reports and ELF structure before writing JSON. The experimental record must identify an untested pre-release, the catalog clearly labels its status, and it cannot pass the hardware-approval gate. New unapproved releases are never inferred into the default entry. Automatic source refresh retains the selected experiment rather than removing it.

Press the circular refresh icon, expand 331HEN, and choose **331HEN Game Patches TEST** to download it directly. The existing working ELF remains available. Use only the experimental HEN in a separate test session, with games closed initially and without a separate ps-patch-system payload; follow HARDWARE-TEST.md. Installing two files does not make them concurrent HEN services or isolate their runtime/configuration paths: both use the existing 331HEN settings and cheat directory, and patches use `/data/331HEN/patches`. PS5 execution of this experimental binary remains unverified.

The actual C parser tests compare both entries and verify their distinct backend folders. Actual frontend tests cover independent visibility/update families in both directions (nine cases), including stable-only and test-only installations. Source-policy tests verify the second entry cannot impersonate stable, normal refresh preserves both entries and the unchanged approved default, and a failed experimental download leaves the working JSON unchanged.

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

## Current refinement preview

The owner requested the old Game Patches TEST catalog item removed. New refinements use 331HEN Preview, saved into the distinct 331HEN-Preview family. The publisher updates that entry only after the compiled release asset and reports are publicly verified. The 331HEN default stays on the exact preserved baseline until a new ELF is tested and explicitly approved. Historical releases/assets are retained; removing the catalog item does not remove files already installed on the console.

This refinement retains native Game Patches, now with installed-game selection and persistent pre-launch settings. It does not guarantee pre-entry application or eliminate every crash. See the integration/limitations reports and single-session hardware-test instructions.
