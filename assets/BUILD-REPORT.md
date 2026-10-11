# 331HEN Autoloader 0.6.1-331.1

Official latest upstream release checked: v0.6.1, source commit 4351659f2f90dda5393eb8c52500ef2330aeb72a.
Product: 331HEN Autoloader. Linux cloud build using existing 331hen-builder image and PS5 Payload SDK 0.44. Build command: scripts/build-cloud.sh. libmicrohttpd taken from existing SDK homebrew library directory.

Binary: 331HEN-Autoloader-Installer_v0.6.1-331.1.elf; 5702912 bytes; SHA256 0e6e34bdf6dc583db09639689655f1fb88a68fe1d20e4dd950d5bd5c277d5cd0.
Compiled from source; not a renamed upstream ELF. Original compiled baseline preserved under backup/.

Changes: application title, installer/UI/page branding and notification labels; supplied icon artwork prepared through the image tool because the displayed attachment's original Windows file was unavailable to the executor. The prepared PNG is visually reviewed but is NOT asserted byte-identical to the original user file. Master PNG retained in assets/331-webkit-master.png and embedded into assets/icon.svg. Existing icon pipeline generates 512x512 icon0.png and seven ICO sizes. Installer logo uses the same SVG.

Preserved: PLK author attribution, GPL-3.0 license, WKAL00001 app ID, localhost port 18181, deeplink, exploit logic, firmware offsets, ELF loader and unified-autoloader pinned versions. Existing 331HEN project source and ELF files unchanged. Submodules show no source diffs.

Verification: official baseline and branded builds passed; four upstream JS suites passed; native installer helper tests passed for TEST00001 and TEST00002; Poops standalone page 92/92 structural checks passed; Relapse standalone page 92/92 passed; ELF static validator passed (x86-64 ELF64 little endian PIE, 16KiB segments); git diff whitespace check passed. Logs and validator output accompany this report.

Hardware: NO PS5 test performed for this installer. Upstream lists firmware 12.70 with a dedicated offset table. The owner's previous 331HEN/Bloodborne test does not test this newly branded autoloader.

Limitations: exploit reliability requires hardware testing. This installs/updates the existing WebKit Autoloader title WKAL00001, not a second independent app. It stores both exploit pages locally. Poops can operate without an active network interface; Relapse needs a connected local interface, without requiring Internet. Manual autoload.txt overrides Payload Manager fallback. No forced autoload or new HEN/kstuff is embedded by this branding. Existing Payload Manager auto-load settings determine which HEN runs. Do not configure duplicate HENs or a separate extra kstuff.

Distribution: local downloadable artifact only at this stage. No GitHub Release or source JSON changes made in this task.
