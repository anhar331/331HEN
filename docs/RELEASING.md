# Repeatable 331HEN pre-releases

Destination: https://github.com/anhar331/331HEN (existing public repository).

Origin is this repository; upstream remains aydencharles/onionHEN. Source and original developer/license notices are retained. Release binaries are assets, not Git-tracked files.

## Future updates

1. Implement and review the source changes, commit them and push to this repository.
2. Open Actions → **Publish 331HEN pre-release** → Run workflow on the intended branch. From Codex, use:

       gh workflow run publish-release.yml --repo anhar331/331HEN --ref main

3. The workflow builds using the existing Docker/SDK pipeline, runs regression, real-engine mock/sanitizer, live Linux HTTPS and ELF/embed checks, then allocates the next unused v0.0.13-331.N tag.
4. It publishes 331HEN.elf, SHA256SUMS.txt, full recursive source, GPL LICENSE, firmware compatibility, changelog, test results, logs, limitations and one-session hardware instructions.
5. It anonymously downloads the public ELF and checksum asset, verifies their hashes and returns the release URL in the Actions summary. No manual file transfer is required for distribution.

Payload Manager uses the public raw [payloads.json](https://raw.githubusercontent.com/anhar331/331HEN/main/payloads.json), not this repository homepage. Publishing an experimental release does not replace the approved source. After a real PS5 test of the exact published checksum, use **Update approved Payload Manager source** to record approval and automatically update the JSON. See [the source policy and official parser verification](PAYLOAD-MANAGER.md).

The GitHub Actions GITHUB_TOKEN is used only for publishing. It is not passed to the Docker build, source archive or console payload. For local Codex publishing, use the existing gh authentication; never store tokens in source. The source exporter includes only tracked files/initialized pinned recursive submodules and rejects credential/private-key patterns and private paths.

## Existing Cloud build

After the build succeeds, run:

    bash scripts/test-release.sh
    python3 -m unittest discover -s scripts/tests -v
    git add . && git commit -m "Describe the reviewed update"
    git push origin main
    python3 scripts/publish_release.py --elf build/bin/331HEN.elf --tests-dir .cache/release-tests

The publisher requires a clean committed source tree and the exact source commit on GitHub. Passing test results must match the exact ELF hash. It stages the upload as Release/publishing/<new-tag>/331HEN.elf; the known-working Release/331HEN.elf is not replaced. The build-container.sh --build-only option skips the older wrapper's stable-file copy.

## Version and preservation policy

The fork base stays OnionHEN v0.0.13 and its existing in-console banner. Distribution tags increment independently as v0.0.13-331.1, .2, .3, etc. Both tags and releases, including drafts, reserve their numbers. Runs are serialized; collisions fail rather than overwrite.

There is no clobber/upload-replacement, release deletion, forced tag update or stable promotion path in this publisher. Failed runs may leave a new reserved draft; subsequent runs allocate a higher number. Older releases/assets remain untouched.

This workflow **always creates pre-releases**, even after host tests pass. It cannot mark a build stable. A future stable release requires real PS5 testing of the exact compiled hash and explicit recorded hardware results, followed by a separately authorized new release; never relabel/overwrite a previous release.

## Compatibility boundary

Target firmware: retail PS5 12.70. Existing kstuff is preserved. New native patches are hardware-untested. Guaranteed pre-entry timing, mask/cave operations and sprx patching are not implemented; see the integration/limitations reports. Successful compilation and memory mocks do not prove game effect or PS5 runtime stability.
