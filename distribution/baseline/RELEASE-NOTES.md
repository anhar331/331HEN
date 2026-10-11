# Preserved 331HEN — Payload Manager distribution

Uploads the **existing unchanged** local known-working `Release/331HEN.elf`. This is **not** the Game Patches experimental ELF in v0.0.13-331.1. No payload source changes or relinking were necessary for this distribution fix.

Target: PS5 **12.70**. The owner reports the existing 331HEN project working; Codex has not performed a hardware test. Original build reports retain their untested-runtime status. This release remains conservatively a **pre-release** pending hardware confirmation of the exact published checksum; no experimental build is promoted to stable.

ELF SHA-256: `72f00d524636f8c96746d551fb495c1af319048a6af3703ffe5ae19e6abc197d`

Matching source: `preserved-working-source`, commit `c52ce44351e7c28915d920a16223fc8e82bea35f`. Attached `331HEN-source.tar.gz` contains corresponding source; all 4,776 regular files compared byte-for-byte against the preserved source snapshot. GPL-3.0 licensing and original credits retained.

Archived build: PS5 Payload SDK v0.44, Clang 18, Docker; **435 host cases passed, zero failed**. Fresh ELF64/x86-64 PIE structure, compressed bootstrapper and original embedded kstuff checks passed. kstuff SHA-256: `82cbb1e70f8a562317d6b65269cf6aa0d7d94e22df4f1ac89a260ca0ec261252`.

Previous release and local stable ELF remain untouched. Payload Manager will select this preserved baseline. Future feed updates require explicit approval of an exact hardware-tested ELF hash.
