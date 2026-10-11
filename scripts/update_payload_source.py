#!/usr/bin/env python3
"""Keep the approved default and explicitly listed experiments in separate families."""
import argparse
import datetime
import hashlib
import json
import os
import re
import subprocess
import tempfile
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REPO = "anhar331/331HEN"
PREFIX = "v0.0.13-331."
BASELINE_TAG = PREFIX + "2"
BASELINE_SHA = "72f00d524636f8c96746d551fb495c1af319048a6af3703ffe5ae19e6abc197d"
SOURCE_URL = "https://raw.githubusercontent.com/anhar331/331HEN/main/payloads.json"
EXPERIMENTAL_FAMILY = "331HEN-Preview"
EXPERIMENTAL_NAME = "331HEN Preview"


def release_number(tag):
    if not re.fullmatch(re.escape(PREFIX) + r"[1-9][0-9]*", tag):
        raise ValueError("Unexpected 331HEN version tag")
    return int(tag[len(PREFIX):])


def saved_filename(tag):
    release_number(tag)
    # Payload Manager compares installed filenames, not the version/checksum.
    # _v is understood by both its frontend and backend family-name logic.
    return "331HEN_" + tag + ".elf"


def experimental_filename(tag):
    release_number(tag)
    return EXPERIMENTAL_FAMILY + "_" + tag + ".elf"


def fetch(url, limit):
    if not url.startswith("https://"):
        raise ValueError("HTTPS is required")
    request = urllib.request.Request(url, headers={"User-Agent": "331HEN-source-verifier"})
    with urllib.request.urlopen(request, timeout=90) as response:
        if response.status != 200:
            raise ValueError("Public download must return HTTP 200")
        data = response.read(limit + 1)
    if len(data) > limit:
        raise ValueError("Download exceeds its size limit")
    return data


def get_release(tag):
    release_number(tag)
    # Public REST metadata and asset downloads deliberately need no credentials.
    return json.loads(fetch(f"https://api.github.com/repos/{REPO}/releases/tags/{tag}", 2 * 1024 * 1024))


def verify_approval(record):
    release_number(record["tag"])
    if not re.fullmatch(r"[0-9a-f]{64}", record["sha256"]):
        raise ValueError("An exact ELF SHA-256 is required")
    if record.get("firmware") != "12.70" or not record.get("evidence") or not record.get("approved_by"):
        raise ValueError("Firmware, approver and actual test evidence are required")
    if record.get("kind") == "preserved-baseline":
        if record["tag"] != BASELINE_TAG or record["sha256"] != BASELINE_SHA:
            raise ValueError("Only the exact preserved existing ELF can use baseline approval")
    elif record.get("kind") == "hardware-tested":
        if record.get("ps5_runtime_tested") is not True:
            raise ValueError("Future default-source updates require a real PS5 test")
    else:
        raise ValueError("Release is not explicitly approved")


def verify_experiment(record):
    release_number(record["tag"])
    if (record.get("kind") != "experimental" or record.get("firmware") != "12.70"
            or record.get("ps5_runtime_tested") is not False
            or not record.get("listed_by") or not record.get("purpose")
            or not re.fullmatch(r"[0-9a-f]{64}", record["sha256"])):
        raise ValueError("Experimental listing needs an explicit untested record and exact ELF hash")


def verify_asset(release, record, *, experimental=False):
    if experimental:
        verify_experiment(record)
        if release.get("prerelease") is not True:
            raise ValueError("The experimental listing must reference a pre-release")
    else:
        verify_approval(record)
    if release.get("draft") or release.get("tag_name") != record["tag"]:
        raise ValueError("Approved release must already be published")
    assets = {a["name"]: a for a in release["assets"] if a.get("state") == "uploaded"}
    for name in ("331HEN.elf", "SHA256SUMS.txt", "LICENSE", "331HEN-source.tar.gz",
                 "FIRMWARE-COMPATIBILITY.md", "CHANGES.md", "TEST-RESULTS.json"):
        if name not in assets:
            raise ValueError("Required release asset missing: " + name)
    url = assets["331HEN.elf"]["browser_download_url"]
    expected = f"https://github.com/{REPO}/releases/download/{record['tag']}/331HEN.elf"
    if url != expected:
        raise ValueError("ELF must be the exact immutable asset in this repository")
    data = fetch(url, 100 * 1024 * 1024)
    if hashlib.sha256(data).hexdigest() != record["sha256"]:
        raise ValueError("Published ELF differs from the approved hardware-test hash")
    sums_url = assets["SHA256SUMS.txt"]["browser_download_url"]
    if sums_url != expected.rsplit("/", 1)[0] + "/SHA256SUMS.txt":
        raise ValueError("Checksum asset belongs to a different release")
    entries = [line.split() for line in fetch(sums_url, 1024 * 1024).decode().splitlines()]
    if entries.count([record["sha256"], "331HEN.elf"]) != 1:
        raise ValueError("SHA256SUMS.txt does not identify the approved ELF exactly once")
    tests = json.loads(fetch(assets["TEST-RESULTS.json"]["browser_download_url"], 1024 * 1024))
    if tests.get("elf_sha256") != record["sha256"]:
        raise ValueError("Published test report does not identify this exact ELF")
    if not (tests.get("passed") is True or (record["kind"] == "preserved-baseline"
                                           and tests.get("static_elf_checked") is True)):
        raise ValueError("Published checks did not pass")
    if experimental and tests.get("ps5_runtime_tested") is not False:
        raise ValueError("The experimental test report must state its actual untested status")
    with tempfile.TemporaryDirectory() as directory:
        elf = Path(directory) / "331HEN.elf"
        elf.write_bytes(data)
        subprocess.run(["python3", str(ROOT / "tools/validate_elf.py"), str(elf)],
                       check=True, stdout=subprocess.DEVNULL)
    return url


def catalog(release, record, url):
    # Current upstream's flat C scanner reads the wrapper's first name for the
    # first payload: using the same name gives the correct result in both the
    # official scanner and a normal JSON parser. Keep name before payloads.
    description = ("Preserved existing 331HEN build; PS5 12.70; excludes the untested Game Patches experiment."
                   if record["kind"] == "preserved-baseline" else
                   "331HEN for PS5 12.70; exact ELF approved after owner-reported PS5 hardware testing.")
    return {"name": "331HEN", "payloads": [{
        "name": "331HEN", "filename": saved_filename(record["tag"]), "url": url,
        "source": release["html_url"], "source_direct": url,
        "description": description, "last_update": release["published_at"][:10],
        "version": record["tag"], "category": "Homebrew Enablers",
        "checksum": record["sha256"]}]}


def experimental_item(release, record, url):
    return {"name": EXPERIMENTAL_NAME, "filename": experimental_filename(record["tag"]),
            "url": url, "source": release["html_url"], "source_direct": url,
            "description": "EXPERIMENTAL refined 331HEN; PS5 12.70; NOT PS5-tested. Separate payload; keep your working version and use a separate session.",
            "last_update": release["published_at"][:10], "version": record["tag"],
            "category": "Experimental Payloads", "checksum": record["sha256"]}


def validate_catalog(document):
    if list(document) != ["name", "payloads"] or document["name"] != "331HEN":
        raise ValueError("Custom catalog requires name before payloads")
    if not 1 <= len(document["payloads"]) <= 3:
        raise ValueError("Catalog needs one approved default and at most one separate experiment")
    limits = {"name": 128, "filename": 256, "url": 1024, "source": 1024,
              "source_direct": 1024, "description": 1024, "last_update": 64,
              "version": 64, "checksum": 65, "category": 128}
    for index, item in enumerate(document["payloads"]):
        if item.get("name") == "331HEN Autoloader Installer":
            try:
                from scripts.installer_source import validate_item
            except ModuleNotFoundError:
                from installer_source import validate_item
            validate_item(item)
            continue
        name = document["name"] if index == 0 else EXPERIMENTAL_NAME
        filename = saved_filename(item["version"]) if index == 0 else experimental_filename(item["version"])
        if item.get("name") != name or item.get("filename") != filename:
            raise ValueError("Approved and experimental entries must have separate saved filename families")
        release_number(item["version"])
        expected = f"https://github.com/{REPO}/releases/download/{item['version']}/331HEN.elf"
        if item.get("url") != expected or item.get("source_direct") != expected:
            raise ValueError("Use immutable published release URLs")
        if not re.fullmatch(r"[0-9a-f]{64}", item["checksum"]):
            raise ValueError("Missing SHA-256")
        if index and (item.get("category") != "Experimental Payloads"
                      or "NOT PS5-tested" not in item.get("description", "")):
            raise ValueError("Experimental status must be visible")
        for value in item.values():
            if not isinstance(value, str) or any(c in value for c in '{}"\\'):
                raise ValueError("Metadata must be compatible with upstream's flat C scanner")
        if any(len(item.get(key, "").encode()) >= size for key, size in limits.items()):
            raise ValueError("Metadata exceeds an official RepoPayload field size")


def atomic_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(value, indent=2) + "\n")
    temporary.replace(path)


def refresh(approvals, output, experiments=(), installers=()):
    records = approvals["approvals"]
    if not records or len({r["tag"] for r in records}) != len(records):
        raise ValueError("Approvals must be nonempty with unique tags")
    for record in records:
        verify_approval(record)
    record = max(records, key=lambda r: release_number(r["tag"]))
    if output.exists():
        previous = json.loads(output.read_text())["payloads"][0]
        if release_number(previous["version"]) > release_number(record["tag"]):
            raise ValueError("Refusing to downgrade the approved source")
        if previous["version"] == record["tag"] and previous["checksum"] != record["sha256"]:
            raise ValueError("An approved version cannot change binary hashes")
    release = get_release(record["tag"])
    url = verify_asset(release, record)
    document = catalog(release, record, url)
    if len(experiments) > 1:
        raise ValueError("Only one explicitly selected Game Patches experiment is supported")
    for experiment in experiments:
        verify_experiment(experiment)
        experimental_release = get_release(experiment["tag"])
        experimental_url = verify_asset(experimental_release, experiment, experimental=True)
        document["payloads"].append(experimental_item(experimental_release, experiment, experimental_url))
    document["payloads"].extend(installers)
    validate_catalog(document)
    atomic_json(output, document)  # Never change a working source before verification.
    return document


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--approve-tag")
    parser.add_argument("--sha256")
    parser.add_argument("--firmware")
    parser.add_argument("--evidence")
    parser.add_argument("--confirmed-ps5-test", action="store_true")
    args = parser.parse_args()
    approval_path = ROOT / "distribution/approved.json"
    approvals = json.loads(approval_path.read_text())
    experiments = json.loads((ROOT / "distribution/experimental.json").read_text())["experiments"]
    if args.approve_tag:
        actor = os.environ.get("GITHUB_ACTOR")
        if not args.confirmed_ps5_test or not actor:
            raise ValueError("Explicit hardware-test confirmation and GitHub workflow actor are required")
        if any(r["tag"] == args.approve_tag for r in approvals["approvals"]):
            raise ValueError("Approval history cannot be overwritten")
        record = {"tag": args.approve_tag, "sha256": args.sha256,
                  "firmware": args.firmware, "evidence": args.evidence,
                  "kind": "hardware-tested", "ps5_runtime_tested": True,
                  "approved_by": actor, "approved_at": datetime.datetime.now(datetime.timezone.utc).isoformat()}
        verify_approval(record)
        approvals["approvals"].append(record)
    try:
        from scripts.installer_source import entries
    except ModuleNotFoundError:
        from installer_source import entries
    document = refresh(approvals, ROOT / "payloads.json", experiments, installers=entries())
    if args.approve_tag:
        atomic_json(approval_path, approvals)
    message = f"Verified approved source: {SOURCE_URL} ({document['payloads'][0]['version']})"
    print(message)
    if os.environ.get("GITHUB_STEP_SUMMARY"):
        with open(os.environ["GITHUB_STEP_SUMMARY"], "a") as stream:
            stream.write(message + "\n")


if __name__ == "__main__":
    main()
