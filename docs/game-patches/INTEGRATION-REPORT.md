# Native Game Patches refinement

The native Game Tools page now opens an installed-game picker. The chosen title is matched by strict actual installed APP_VER (PS4) or contentVersion (PS5). Settings are saved before launch and do not patch memory when toggled. The existing native dispatcher supplies a new launch snapshot; util validates its process generation, application and title, compares installed/launched versions, finds bounded executable mappings, and uses the existing memory backend with readback, rollback and cheat-overlap protection. The real executor remains present; this is not an XML-only UI.

The earlier util-side running-game resolver lacked hooks and depended on SystemService APIs after switching to PTRACE credentials. This prevented detection; the revised service uses daemon lifecycle identity instead. A duplicated util dynlib structure also used a 64-bit section size where the native ABI uses 32 bits, shifting later fields and corrupting bounds. A shared portable ABI now prevents that divergence.

Crash-risk changes cover uncertain same-PID ShellUI injection retry, overlapping service replacement, missing remote thread entry, credential backup/restore and owned ptrace cleanup. They do not identify the precise cause of the reported hardware crash.

## Launch timing

Existing SysCore lifecycle events occur after exec. No guaranteed patch-before-entry hook is integrated. Upstream ps-patch-system uses an additional ShellCore inline hook/eh_frame mechanism and server acknowledgement; copying that into this working fork would introduce new critical ShellCore writes/coexistence risks. Startup-only effects are therefore not claimed working; UI status distinguishes persisted selections and verified post-launch writes. Bloodborne CUSA00900 / 01.09 and its real 60 FPS (With Deltatime) definition are host-tested for matching/execution logic, not FPS on PS5.

## kstuff assessment

Embedded v1.10/source 3b2ab4a16ba43c8ab38daf7ec83cc297014e0e47 remains SHA-256 82cbb1e70f8a562317d6b65269cf6aa0d7d94e22df4f1ac89a260ca0ec261252, identical to the preserved working ELF. Official OnionHEN v0.0.13 uses this version. v1.11's old tag 85fb88a contained a 12.70 store_rax_rdi typo, corrected by fe7fef639aafbbdacc73cf9f4f49a45f6798106d and merge 48cd2c4ce62aa9ca0c2cc014c1e512f276ae7c17. Current v1.11 release asset SHA-256 ab9a6cb4d3b1daf139d4d646e402b1cf569071acd64599c936d7a3a6164dc779 declares compatibility, but the old failure does not affect our v1.10. No replacement was made.

References: https://github.com/EchoStretch/kstuff-lite/commit/fe7fef639aafbbdacc73cf9f4f49a45f6798106d , https://github.com/EchoStretch/kstuff-lite/issues/79 , https://github.com/EchoStretch/kstuff-lite/issues/81 . Runtime can select /data/331HEN/kstuff.elf or keep an already-active instance; new logs identify that choice but cannot identify an already active binary version.

Full source, licenses, credits, actual build/test logs and ELF inspection are included in the release. Stable local ELF and historical public releases remain preserved.
