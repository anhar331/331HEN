#!/usr/bin/env python3
"""Append-only 331HEN pre-release publishing. Authentication stays in gh."""
import argparse
import hashlib
import io
import json
import re
import shutil
import subprocess
import tarfile
import urllib.request
from pathlib import Path

REPOSITORY = "anhar331/331HEN"
TAG_PREFIX = "v0.0.13-331."
ROOT = Path(__file__).resolve().parent.parent
KSTUFF_SHA256 = "82cbb1e70f8a562317d6b65269cf6aa0d7d94e22df4f1ac89a260ca0ec261252"


def gh(*args):
    return subprocess.check_output(["gh", *args], text=True).strip()


def git(*args, directory=ROOT):
    return subprocess.check_output(["git", "-C", str(directory), *args], text=True).strip()


def checksum(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def next_tag(tags):
    counters = [int(t[len(TAG_PREFIX):]) for t in tags
                if re.fullmatch(re.escape(TAG_PREFIX) + r"[1-9][0-9]*", t)]
    return TAG_PREFIX + str(max(counters, default=0) + 1)


def require_tests(results, elf):
    if (results.get("passed") is not True or results.get("elf_sha256") != checksum(elf)
            or results.get("patch_sanitizers_passed") is not True
            or results.get("live_linux_https_passed") is not True
            or results.get("regression_cases", 0) < 436):
        raise RuntimeError("Passing build/static/sanitizer/integration results must match this exact ELF")
    if results.get("ps5_runtime_tested") is not False:
        raise RuntimeError("This publisher only handles hardware-untested pre-releases")


def tracked_entries(directory):
    raw = subprocess.check_output(["git", "-C", str(directory), "ls-files", "--stage", "-z"])
    for record in raw.split(b"\0"):
        if record:
            header, name = record.split(b"\t", 1)
            mode = header.split(b" ")[0]
            yield mode.decode(), name.decode()


def source_archive(destination):
    """Tracked files and recursive pinned submodules; no .git/config/env/cache."""
    forbidden = {".git", ".cache", ".docker", "Release", ".env"}
    patterns = [
        rb"gh[pousr]_[A-Za-z0-9]{30,}",
        rb"github_pat_[A-Za-z0-9_]{30,}",
        rb"-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----",
        rb"AKIA[0-9A-Z]{16}",
    ]
    with tarfile.open(destination, "w:gz") as archive:
        def add_repository(directory, prefix):
            for mode, name in tracked_entries(directory):
                rel = Path(name)
                if (rel.is_absolute() or ".." in rel.parts or forbidden.intersection(rel.parts)
                        or rel.name.startswith(".env.")):
                    raise RuntimeError(f"Excluded/private path is tracked: {prefix + name}")
                path = directory / rel
                if mode == "160000":
                    if not (path / ".git").exists():
                        raise RuntimeError(f"Initialize recursive submodule: {prefix + name}")
                    add_repository(path, prefix + name + "/")
                    continue
                if not path.is_symlink():
                    data = path.read_bytes()
                    # BearSSL deliberately ships public example TLS keys. Admit
                    # only byte-identical tracked fixtures at its pinned public
                    # revision, never a generic private-key/path exemption.
                    public_fixture = (
                        prefix == "third_party/kstuff-lite/ps5-kstuff/BearSSL/"
                        and name in {"samples/key-ee-ec.pem", "samples/key-ee-rsa.pem",
                                     "samples/key-ica-ec.pem", "samples/key-ica-rsa.pem",
                                     "samples/key-root-ec.pem", "samples/key-root-rsa.pem"}
                        and git("rev-parse", "HEAD", directory=directory)
                        == "79c060eea3eea1257797f15ea1608a9a9923aa6f"
                        and subprocess.check_output(
                            ["git", "-C", str(directory), "show", "HEAD:" + name]) == data
                    )
                    public_fixture = public_fixture or (
                        prefix == "third_party/kstuff-lite/ps5-kstuff/libtomcrypt/"
                        and name == "tests/test.key"
                        and git("rev-parse", "HEAD", directory=directory)
                        == "b96e96cf8b22a931e8e91098ac37bc72f9e2f033"
                        and subprocess.check_output(
                            ["git", "-C", str(directory), "show", "HEAD:" + name]) == data
                    )
                    if not public_fixture and any(re.search(pattern, data) for pattern in patterns):
                        raise RuntimeError(f"Possible credential/private key in tracked source: {prefix + name}")
                archive.add(path, arcname="331HEN-source/" + prefix + name, recursive=False)
        add_repository(ROOT, "")


def verify_public(url, expected):
    # No Authorization header: verify a genuinely anonymous public download.
    request = urllib.request.Request(url, headers={"User-Agent": "331HEN-public-release-verification"})
    digest = hashlib.sha256()
    with urllib.request.urlopen(request, timeout=90) as response:
        if response.status != 200:
            raise RuntimeError("Public asset did not return HTTP 200")
        total = 0
        while chunk := response.read(1024 * 1024):
            total += len(chunk)
            if total > 100 * 1024 * 1024:
                raise RuntimeError("Unexpectedly oversized public asset")
            digest.update(chunk)
    if digest.hexdigest() != expected:
        raise RuntimeError("Publicly downloaded asset hash does not match the local file")


def prepare(elf, tests_dir, tag, commit):
    require_tests(json.loads((tests_dir / "TEST-RESULTS.json").read_text()), elf)
    subprocess.run(["python3", str(ROOT / "tools/validate_elf.py"), str(elf),
                    str(ROOT / "build/bin/bootstrapper.elf"),
                    str(ROOT / ".cache/dependencies/kstuff.elf")],
                   check=True, stdout=subprocess.DEVNULL)
    if checksum(ROOT / ".cache/dependencies/kstuff.elf") != KSTUFF_SHA256:
        raise RuntimeError("Working embedded kstuff changed")
    destination = ROOT / "Release/publishing" / tag
    destination.mkdir(parents=True, exist_ok=False)  # never reuse old release staging
    shutil.copyfile(elf, destination / "331HEN.elf")
    shutil.copyfile(ROOT / "LICENSE", destination / "LICENSE")
    for name in ["INTEGRATION-REPORT.md", "KNOWN-LIMITATIONS.md", "HARDWARE-TEST.md",
                 "CHANGES.md", "SOURCE-NOTICE.md"]:
        shutil.copyfile(ROOT / "docs/game-patches" / name, destination / name)
    for name in ["TEST-RESULTS.json", "STATIC-VALIDATION.json", "BUILD.log",
                 "PATCH-TESTS.log", "REGRESSION-TESTS.log", "LIVE-DOWNLOAD-TESTS.log"]:
        shutil.copyfile(tests_dir / name, destination / name)
    (destination / "FIRMWARE-COMPATIBILITY.md").write_text(
        "# Firmware compatibility\n\nTarget: retail PS5 firmware 12.70.\n\n"
        "Previous working 331HEN/kstuff was PS5-tested by the user. This new Game Patches "
        "ELF has NOT been PS5-tested; it is a pre-release. No new kernel offsets or "
        "patches were added. Existing embedded kstuff SHA-256: " + KSTUFF_SHA256 + ".\n\n"
        "Strict Title ID/version/eboot.bin matching is required. Guaranteed pre-entry "
        "startup timing, mask operations and sprx patching are not supported. "
        "See KNOWN-LIMITATIONS.md and HARDWARE-TEST.md.\n")
    source_archive(destination / "331HEN-source.tar.gz")
    results = json.loads((tests_dir / "TEST-RESULTS.json").read_text())
    (destination / "BUILD-REPORT.md").write_text(
        f"# {tag} build\n\nSource commit: {commit}\n\n"
        f"ELF SHA-256: {checksum(elf)}\n\n"
        "Existing PS5 SDK v0.44 / Clang 18 / Docker release pipeline. Actual newly linked ELF. "
        "ELF structure and embedded bootstrapper/kstuff were verified.\n\n"
        f"{results['regression_cases']} host regression cases passed; parser/real-engine/service "
        "mocks with ASan/UBSan and real Linux HTTPS/collection installation passed. "
        "Raw test/build logs are included. No PS5 runtime test was performed. "
        "The original stable ELF is retained locally and is not this asset.\n")
    notes = (
        f"# 331HEN {tag} — Game Patches TEST\n\n"
        "**Experimental pre-release. Target PS5 firmware 12.70; this updated ELF is "
        "NOT tested on PS5 hardware.** Keep your known-working payload.\n\n"
        "Native Game Patches, independent persisted toggles, exact game/version matching, "
        "real process-memory execution with readback/rollback, cheat overlap protection "
        "and native GitHub collection updates. Bloodborne CUSA00900 / 01.09 definitions "
        "are bundled unchanged.\n\n"
        "**Limitations:** guaranteed patch-before-entry launch timing is absent; "
        "mask/cave and sprx operations are unsupported. Memory readback is not proof of "
        "FPS or startup effect. See KNOWN-LIMITATIONS.md.\n\n"
        f"Checks: {results['regression_cases']} existing host regression cases, new "
        "sanitized core/service mocks, live Linux HTTPS installation and ELF/embedded "
        "payload integrity passed. Existing kstuff is byte-identical. No firmware "
        "offset or kernel patch changes.\n\n"
        f"Source commit: {commit}\n\n"
        f"331HEN.elf SHA-256: `{checksum(elf)}`\n\n"
        "Download 331HEN.elf and SHA256SUMS.txt from this release. Follow HARDWARE-TEST.md "
        "for a single test session with the game initially closed. Full corresponding "
        "source, upstream GPL-3.0 LICENSE, patch credits and reference notices are included.\n"
    )
    (destination / "RELEASE-NOTES.md").write_text(notes)
    assets = sorted(p for p in destination.iterdir() if p.is_file())
    (destination / "SHA256SUMS.txt").write_text(
        "".join(checksum(p) + "  " + p.name + "\n" for p in assets))
    return destination


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--elf", type=Path, required=True)
    parser.add_argument("--tests-dir", type=Path, required=True)
    parser.add_argument("--prepare-only", action="store_true")
    args = parser.parse_args()
    # Refuse publishing uncommitted/private local state or to an unrelated repo.
    if git("status", "--porcelain", "--untracked-files=normal"):
        raise RuntimeError("Commit the reviewed source/workflow first; working tree must be clean")
    remote = git("remote", "get-url", "origin")
    if remote not in [f"https://github.com/{REPOSITORY}.git", f"https://github.com/{REPOSITORY}",
                      f"git@github.com:{REPOSITORY}.git"]:
        raise RuntimeError("Origin must be the existing anhar331/331HEN repository")
    repo = json.loads(gh("api", f"repos/{REPOSITORY}"))
    # Installation GITHUB_TOKEN responses can omit user-role permissions.
    # The release API enforces contents:write; an absent role field is not an
    # authorization denial and must not reject an otherwise valid workflow.
    if repo["private"] or repo["full_name"] != REPOSITORY:
        raise RuntimeError("The existing public anhar331/331HEN destination is required")
    tags = gh("api", f"repos/{REPOSITORY}/tags", "--paginate", "--jq", ".[].name").splitlines()
    releases = gh("api", f"repos/{REPOSITORY}/releases", "--paginate", "--jq", ".[].tag_name").splitlines()
    tag = next_tag(tags + releases)
    commit = git("rev-parse", "HEAD")
    remote_commit = json.loads(gh("api", f"repos/{REPOSITORY}/commits/{commit}"))
    if remote_commit["sha"] != commit:
        raise RuntimeError("Push the exact source commit before publishing")
    stable = ROOT / "Release/331HEN.elf"
    stable_before = checksum(stable) if stable.exists() else None
    destination = prepare(args.elf.resolve(), args.tests_dir.resolve(), tag, commit)
    if args.prepare_only:
        print(destination)
        return
    assets = sorted(str(p) for p in destination.iterdir() if p.is_file())
    # No --clobber, update, deletion, forced tag or stable promotion exists here.
    subprocess.run(["gh", "release", "create", tag, *assets, "--repo", REPOSITORY,
                    "--target", commit, "--prerelease", "--latest=false",
                    "--title", f"331HEN {tag} — Game Patches TEST",
                    "--notes-file", str(destination / "RELEASE-NOTES.md")], check=True)
    release = json.loads(gh("api", f"repos/{REPOSITORY}/releases/tags/{tag}"))
    if release["draft"] or not release["prerelease"]:
        raise RuntimeError("Release must be a published pre-release")
    public_checks = []
    for name in ["331HEN.elf", "SHA256SUMS.txt"]:
        asset = next(a for a in release["assets"] if a["name"] == name)
        verify_public(asset["browser_download_url"], checksum(destination / name))
        public_checks.append({"asset": name, "url": asset["browser_download_url"],
                              "sha256_matches": True, "anonymous_http_200": True})
    if stable_before is not None and checksum(stable) != stable_before:
        raise RuntimeError("Stable local ELF was modified")
    verification = {"release_url": release["html_url"], "tag": tag, "source_commit": commit,
                    "prerelease": True, "public_downloads": public_checks,
                    "stable_local_preserved": True}
    (destination / "PUBLIC-VERIFICATION.json").write_text(json.dumps(verification, indent=2) + "\n")
    # GITHUB_TOKEN-created releases do not trigger a second release workflow.
    # Reverify only the recorded approved channel; this new untested release is
    # absent from approvals and cannot replace the preserved default source.
    subprocess.run(["python3", str(ROOT / "scripts/update_payload_source.py")], check=True)
    summary = Path(__import__("os").environ.get("GITHUB_STEP_SUMMARY", "/dev/null"))
    with summary.open("a") as stream:
        stream.write(f"Published [{tag}]({release['html_url']}) as an untested PS5 pre-release. "
                     "Anonymous ELF/checksum downloads verified.\n")
    print(json.dumps(verification, indent=2))


if __name__ == "__main__":
    main()
