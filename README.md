# 331HEN

Official release distribution for **331HEN** PS5 homebrew.

## Downloads

Download verified ELF builds from [Releases](https://github.com/anhar331/331HEN/releases).

- **Stable** releases: hardware-tested builds.
- **Pre-release** builds: experimental; keep a known-working ELF available for rollback.
- Each release should attach `331HEN.elf`, `SHA256SUMS.txt`, and notes on firmware support and changes.

Use the versioned release asset linked by the approved Payload Manager source below. It selects the preserved existing ELF rather than the experimental Game Patches build. GitHub's `latest` endpoint skips pre-releases and is not used for this source.

## Publishing checklist

1. Build in the existing Codex workspace; do not overwrite the known-good ELF.
2. Run build and available static/integration tests.
3. Use an incrementing tag (for example, `v0.0.13-331.1`).
4. Publish untested PS5 builds as **pre-release**.
5. Upload `331HEN.elf` as a binary **release asset** (not a Git-tracked file).
6. Include SHA-256, changes, known limitations, and firmware compatibility.
7. Mark stable only after actual hardware validation.

## Payload Manager

In **Manage Sources → Add a New Source**, paste:

    https://raw.githubusercontent.com/anhar331/331HEN/main/payloads.json

This is the actual JSON catalog for the official [itsPLK PS5 Payload Manager](https://github.com/itsPLK/ps5-payload-manager). The catalog includes a direct, versioned `331HEN.elf` release URL and its SHA-256 checksum. The manager downloads the ELF and validates the hash. A GitHub repository homepage is not a source JSON URL.

The default entry preserves the existing 331HEN build. Untested Game Patches pre-releases do not automatically replace it. A future source update requires an explicit hardware-test approval for the exact published ELF checksum. See [source format and approval workflow](docs/PAYLOAD-MANAGER.md).

The same catalog also lists **331HEN Preview** as a separately requested experimental download. Its saved filename and Payload Manager folder differ from the default, so it does not replace the working payload file. The old TEST catalog entry is retired; historical releases remain archived. This new version is not PS5-tested; use it alone in a separate test session and follow the [hardware-test instructions](docs/game-patches/HARDWARE-TEST.md). Both builds retain the existing 331HEN runtime/configuration paths; separate download folders do not isolate console services or settings.

## Credits

331HEN is derived from OnionHEN. Retain applicable upstream license notices and credits when publishing source or binaries.

## Native Game Patches and repeatable publishing

The current source includes native Game Patches and safe collection updates. Firmware target: 12.70; the experimental implementation is not PS5-tested. Pre-entry patch timing is not guaranteed. See [limitations](docs/game-patches/KNOWN-LIMITATIONS.md) and [one-session hardware test](docs/game-patches/HARDWARE-TEST.md).

Use the **Publish 331HEN pre-release** Actions workflow for future builds. It builds, runs host checks, increments the `v0.0.13-331.N` tag, publishes immutable release assets and verifies anonymous download. See [publishing instructions](docs/RELEASING.md). This workflow always publishes pre-releases.

[Original fork documentation and complete upstream credits](README-FORK.md) · [GPL-3.0 LICENSE](LICENSE) · [Game Patches integration report](docs/game-patches/INTEGRATION-REPORT.md).
