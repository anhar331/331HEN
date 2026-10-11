#!/usr/bin/env python3
"""One-time recovery: upload the exact preserved ELF from GitHub's runner."""
import argparse
import json
import shutil
import subprocess
from pathlib import Path

import publish_release as publisher
from update_payload_source import BASELINE_SHA, BASELINE_TAG, REPO

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    source = args.source.resolve()
    expected_commit = json.loads((root / "distribution/baseline/TEST-RESULTS.json").read_text())["source_commit"]
    if publisher.git("rev-parse", "HEAD", directory=source) != expected_commit:
        raise ValueError("Checkout must match the preserved corresponding source commit")
    elf = source / "build/bin/331HEN.elf"
    if publisher.checksum(elf) != BASELINE_SHA:
        raise ValueError("Rebuilt binary differs from the existing working ELF: refusing to upload it")
    destination = root / "Release/baseline-ci"
    destination.mkdir(parents=True, exist_ok=False)
    for report in (root / "distribution/baseline").iterdir():
        shutil.copyfile(report, destination / report.name)
    shutil.copyfile(elf, destination / "331HEN.elf")
    shutil.copyfile(source / "LICENSE", destination / "LICENSE")
    shutil.copyfile(root / ".cache/preserved-build.log", destination / "REPRODUCIBLE-BUILD.log")
    result = subprocess.check_output(["python3", str(root / "tools/validate_elf.py"), str(elf),
                                     str(source / "build/bin/bootstrapper.elf"),
                                     str(source / ".cache/dependencies/kstuff.elf")], text=True)
    (destination / "CURRENT-ELF-VALIDATION.json").write_text(result)
    publisher.ROOT = source
    publisher.source_archive(destination / "331HEN-source.tar.gz")
    (destination / "SHA256SUMS.txt").write_text("".join(
        publisher.checksum(p) + "  " + p.name + "\n" for p in sorted(destination.iterdir()) if p.is_file()))
    # GitHub's public by-tag endpoint hides drafts. Authenticated listing can
    # find the existing pending draft without inventing a second release.
    pages = json.loads(publisher.gh("api", f"repos/{REPO}/releases", "--paginate", "--slurp"))
    matches = [release for page in pages for release in page if release["tag_name"] == BASELINE_TAG]
    if len(matches) != 1:
        raise ValueError("Exactly one existing baseline draft is required")
    release = matches[0]
    if not release["draft"]:
        raise ValueError("Cannot alter an already-published release")
    if not release["prerelease"]:
        raise ValueError("Preserved release must remain conservatively marked pre-release")
    if release["assets"]:
        raise ValueError("Recovery expects an empty draft; refuse asset replacement")
    # Upload once, sequentially, using the runner's short-lived GITHUB_TOKEN.
    for asset in sorted(destination.iterdir()):
        publisher.gh("release", "upload", BASELINE_TAG, str(asset), "--repo", REPO)
    publisher.gh("release", "edit", BASELINE_TAG, "--repo", REPO, "--draft=false", "--prerelease", "--latest=false")
    release = json.loads(publisher.gh("api", f"repos/{REPO}/releases/tags/{BASELINE_TAG}"))
    for name in ["331HEN.elf", "SHA256SUMS.txt"]:
        asset = next(a for a in release["assets"] if a["name"] == name)
        publisher.verify_public(asset["browser_download_url"], publisher.checksum(destination / name))
    print(release["html_url"])


if __name__ == "__main__":
    main()
