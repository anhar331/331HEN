# Refinement verification

New ELF SHA-256: `71b022ecdc5d5330c3c059e0cf1759306126efc548414fe56afb26a701c0a748` (3,388,144 bytes). Target firmware: 12.70.

The full release project build passed in the existing PS5 SDK0.44 / Clang18 Docker environment. An initial local attempt failed from exhausted task Docker cache storage; deleting obsolete task builder/cache data allowed a complete successful rebuild.

444 host regression cases passed, including duplicate injection and bounded owned-service shutdown. Parser/real executor/service mocks, native picker/toggle/back/error handling and installed-game discovery passed under ASan/UBSan. Installed-game fixture suite executed273 assertions. The shared module syscall ABI test passed. Live Linux libcurl HTTPS fetched the actual GitHub database:508 definitions, pinned snapshot f8e8d5a1b15017fd2ea97558e733eac9fcea21a4. ELF64/x86-64 PIE structure, compressed bootstrapper and embedded original kstuff inspection passed. Publishing policy:15 cases passed; workflow syntax passed.

The final report generator initially rejected an incorrect expected PASS marker count (the installed-game suite prints an assertion summary rather than a PASS marker). The count was corrected; no failing execution test was suppressed. Raw build/test logs remain available and will be attached by the release workflow.

Preserved local Release/331HEN.elf SHA-256:72f00d524636f8c96746d551fb495c1af319048a6af3703ffe5ae19e6abc197d, byte-identical to the pre-refinement backup. Historical Game Patches TEST SHA88a0d3d64cf5ba35fd1e67de51f57599844390c7668b7b08027edc313b46433c is retained in backups and historical releases. Embedded kstuff v1.10/source3b2ab4a remains82cbb1e70f8a562317d6b65269cf6aa0d7d94e22df4f1ac89a260ca0ec261252. No kstuff, kernel patch, firmware offset or existing cheat storage change was made. Shared injector/ptrace cleanup changed deliberately as documented; it is not claimed byte-identical.

The owner reported an initial crash then a successful launch/download with the previous experiment. This update is hardware-untested: the exact crash cause, crash-free stability, firmware/runtime behavior and Bloodborne FPS effects are not verified. Pre-entry patch timing remains unsupported. Alternate installation layouts may be absent; unknown/conflicting versions fail closed. Follow docs/game-patches/HARDWARE-TEST.md for one fresh-session test.

Backup: /workspace/331HEN-backup/pre-refinement-20261011. New local artifact: Release/331HEN-Refined-PREVIEW.elf. Stable defaults remain unchanged. New public release is a pre-release; the obsolete TEST catalog listing is replaced by a separate Preview family only after the uploaded ELF is verified. Earlier public releases are retained.
