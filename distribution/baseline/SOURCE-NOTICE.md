# Source, license and credits

331HEN is a modified fork of OnionHEN v0.0.13. The original GPL-3.0 LICENSE is included unchanged, as are upstream copyright notices, author metadata and contributor acknowledgments.
Original authors include 麒麟/0xp0co, LightningMods and the contributors credited in the source README. kstuff-lite credits EchoStretch, sleirsgoevy and its contributors. Other vendored dependencies retain their own notices/licenses.

`331HEN-source.tar.gz` contains the modified first-party source, build files, branding sources, vendored dependencies and pinned recursive kstuff sources. No source-only archive is substituted for the separately delivered compiled ELF.
The original repository revision is b23ffe674b2de9f62fe634944c9230ff149d593a. Upstream source: https://github.com/aydencharles/onionHEN/tree/v0.0.13
SDK source: https://github.com/ps5-payload-dev/sdk/tree/v0.44
PS5 library build recipes: https://github.com/illusionyy/ps5-pacbrew-repo
kstuff source: https://github.com/EchoStretch/kstuff-lite/tree/3b2ab4a16ba43c8ab38daf7ec83cc297014e0e47
Keystone source: https://github.com/keystone-engine/keystone

To rebuild with Docker, curl, unzip and tar available, run `./scripts/build-container.sh` from the extracted source. This fetches the SDK, target libraries and kstuff with pinned checksums. The compiled payload is produced at `build/bin/331HEN.elf` and copied into `Release`.
