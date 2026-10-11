# 331HEN UI correction build

Built 2026-10-11 in the existing Linux workspace using the successful `331hen-builder` Docker image, PS5 Payload SDK v0.44, Clang 18, CMake and pinned dependencies. Command: `bash scripts/build.sh --release --jobs 3 --skip-dependency-sync`. Build completed successfully.

Output: `/workspace/331HEN/Release/331HEN.elf`, 3,312,272 bytes.
SHA-256: `72f00d524636f8c96746d551fb495c1af319048a6af3703ffe5ae19e6abc197d`.
Upstream OnionHEN v0.0.13 revision: `b23ffe674b2de9f62fe634944c9230ff149d593a`.

## Backup and changes

Working source, generated build outputs and previous release backed up at `/workspace/331HEN-backup/ui-working-snapshot`. The previous ELF SHA-256 is `309186b63813255c68cdac5b1a9f4806899a0f19ae721c6d137c632cf6c8afcd`; the user reports that previous build works on their PS5.

Only four production UI files and the existing welcome notification test changed. ShellUI Settings registration is stored in UTF-8 and UTF-16LE byte arrays in `settings_bundle_patch.cpp`, rather than translations. Both now display `★331HEN Tools`, with two trailing spaces preserving firmware string slot lengths. Patch algorithms, firmware offsets, icons and routing remain unchanged.

Welcome notification uses ANHAR; shared author metadata and credits remain original. Support group/page and its presentation handlers were removed. About has exactly two navigation entries: 331HEN Credits and Projects included in 331HEN. Their existing content is unchanged.

## Verified

- 435 host tests passed, zero failed, including notification text, icon, navigation and firmware route policy tests.
- ELF64 little-endian x86-64 PIE, valid table/segment bounds, alignment and executable entry point.
- Embedded compressed bootstrapper matches this build; it contains the exact previous kstuff ELF.
- kstuff-lite remains v1.10, revision `3b2ab4a16ba43c8ab38daf7ec83cc297014e0e47`, ELF SHA-256 `82cbb1e70f8a562317d6b65269cf6aa0d7d94e22df4f1ac89a260ca0ec261252`.
- Loader and util ELF files, notification/Toolbox icons, LICENSE and kstuff match the previous build byte for byte.
- All remaining first-party source, vendor source, assets, kernel patches, firmware offsets, payload loading and runtime paths match the backup. Translation JSON parses and `git diff --check` passes.
- Credits and Projects page content and original author attribution are unchanged.

## Untested runtime

The updated ELF has not been executed on a PS5. The previous build's successful hardware test is user-reported and does not verify this updated build. No PS5 was remotely accessed or modified. Static checks and host tests do not prove firmware 12.70 runtime stability or rendering.

Build emits an existing SDK linker warning about SHT_LLVM_ADDRSIG and safe identical-code folding; it is nonfatal. Detailed evidence accompanies this report.
