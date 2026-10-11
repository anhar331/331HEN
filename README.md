# 331HEN

Official release distribution for **331HEN** PS5 homebrew.

## Downloads

Download verified ELF builds from [Releases](https://github.com/anhar331/331HEN/releases).

- **Stable** releases: hardware-tested builds.
- **Pre-release** builds: experimental; keep a known-working ELF available for rollback.
- Each release should attach `331HEN.elf`, `SHA256SUMS.txt`, and notes on firmware support and changes.

When a stable release exists, its asset can be accessed via:
`https://github.com/anhar331/331HEN/releases/latest/download/331HEN.elf`

This URL will not work until a stable GitHub Release with that asset has been published. GitHub's `latest` endpoint skips pre-releases.

## Publishing checklist

1. Build in the existing Codex workspace; do not overwrite the known-good ELF.
2. Run build and available static/integration tests.
3. Use an incrementing tag (for example, `v0.0.13-331.1`).
4. Publish untested PS5 builds as **pre-release**.
5. Upload `331HEN.elf` as a binary **release asset** (not a Git-tracked file).
6. Include SHA-256, changes, known limitations, and firmware compatibility.
7. Mark stable only after actual hardware validation.

## Payload Manager

Payload Manager source formats vary. A GitHub repository or Releases URL may not be a valid **Manage Sources** entry. Verify the manager's expected source manifest format before adding a feed; do not assume this repository is directly compatible.

## Credits

331HEN is derived from OnionHEN. Retain applicable upstream license notices and credits when publishing source or binaries.
