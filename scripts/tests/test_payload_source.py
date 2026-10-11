import copy
import hashlib
import importlib.util
import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

spec = importlib.util.spec_from_file_location("source", Path(__file__).parents[1] / "update_payload_source.py")
source = importlib.util.module_from_spec(spec)
spec.loader.exec_module(source)


class SourceTests(unittest.TestCase):
    def setUp(self):
        self.record = dict(tag=source.BASELINE_TAG, sha256=source.BASELINE_SHA,
                           firmware="12.70", kind="preserved-baseline", approved_by="anhar331",
                           evidence="Preserved existing working payload")
        self.release = dict(tag_name=source.BASELINE_TAG, draft=False, prerelease=True,
                            html_url="https://github.com/anhar331/331HEN/releases/tag/" + source.BASELINE_TAG,
                            published_at="2026-10-11T17:00:00Z", assets=[])

    def test_exact_baseline_exception_cannot_admit_experimental_build(self):
        source.verify_approval(self.record)
        for change in [{"tag": "v0.0.13-331.1"}, {"sha256": "a" * 64}, {"firmware": "13.60"}, {"kind": "unknown"}]:
            with self.subTest(change=change), self.assertRaises(ValueError):
                source.verify_approval({**self.record, **change})

    def test_future_versions_require_actual_test_declaration(self):
        record = {**self.record, "tag": "v0.0.13-331.3", "kind": "hardware-tested"}
        with self.assertRaises(ValueError): source.verify_approval(record)
        source.verify_approval({**record, "ps5_runtime_tested": True})

    def test_older_or_different_hash_cannot_replace_current_source(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "payloads.json"
            for tag, sha in [("v0.0.13-331.3", source.BASELINE_SHA), (source.BASELINE_TAG, "b" * 64)]:
                path.write_text(json.dumps({"payloads": [{"version": tag, "checksum": sha}]}))
                before = path.read_bytes()
                with self.assertRaises(ValueError): source.refresh({"approvals": [self.record]}, path)
                self.assertEqual(path.read_bytes(), before)

    def test_failure_preserves_working_json(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "payloads.json"
            path.write_text(json.dumps({"payloads": [{"version": source.BASELINE_TAG, "checksum": source.BASELINE_SHA}]}))
            before = path.read_bytes()
            with patch.object(source, "get_release", side_effect=OSError("Network unavailable")):
                with self.assertRaises(OSError): source.refresh({"approvals": [self.record]}, path)
            self.assertEqual(path.read_bytes(), before)

    def test_unapproved_newest_release_is_never_selected(self):
        with tempfile.TemporaryDirectory() as directory, patch.object(source, "get_release", return_value=self.release) as get:
            url = "https://github.com/anhar331/331HEN/releases/download/" + source.BASELINE_TAG + "/331HEN.elf"
            with patch.object(source, "verify_asset", return_value=url):
                result = source.refresh({"approvals": [self.record]}, Path(directory) / "payloads.json")
            get.assert_called_once_with(source.BASELINE_TAG)
            self.assertEqual(result["payloads"][0]["version"], source.BASELINE_TAG)

    def test_custom_schema_and_immutable_urls(self):
        url = "https://github.com/anhar331/331HEN/releases/download/" + source.BASELINE_TAG + "/331HEN.elf"
        valid = source.catalog(self.release, self.record, url)
        source.validate_catalog(valid)
        self.assertEqual(valid["payloads"][0]["filename"], "331HEN_v0.0.13-331.2.elf")
        self.assertEqual(source.saved_filename("v0.0.13-331.3"), "331HEN_v0.0.13-331.3.elf")
        bad = copy.deepcopy(valid); bad["payloads"][0]["filename"] = "331HEN.elf"
        with self.assertRaises(ValueError): source.validate_catalog(bad)
        bad = copy.deepcopy(valid); bad["payloads"][0]["url"] = "https://github.com/anhar331/331HEN"
        with self.assertRaises(ValueError): source.validate_catalog(bad)
        bad = copy.deepcopy(valid); bad["payloads"][0]["description"] = "bad } metadata"
        with self.assertRaises(ValueError): source.validate_catalog(bad)

    def test_missing_or_draft_asset_refused(self):
        for release in [self.release, {**self.release, "draft": True}]:
            with self.assertRaises(ValueError): source.verify_asset(release, self.record)

    def test_public_asset_checksums_reports_and_elf_verification(self):
        data = b"ELF fixture inspected by mocked static validator"
        sha = hashlib.sha256(data).hexdigest()
        record = {**self.record, "tag": "v0.0.13-331.3", "kind": "hardware-tested",
                  "sha256": sha, "ps5_runtime_tested": True}
        base = "https://github.com/anhar331/331HEN/releases/download/" + record["tag"] + "/"
        names = ["331HEN.elf", "SHA256SUMS.txt", "LICENSE", "331HEN-source.tar.gz",
                 "FIRMWARE-COMPATIBILITY.md", "CHANGES.md", "TEST-RESULTS.json"]
        release = {**self.release, "tag_name": record["tag"], "assets": [
            {"name": n, "state": "uploaded", "browser_download_url": base + n} for n in names]}
        contents = {base + "331HEN.elf": data, base + "SHA256SUMS.txt": (sha + "  331HEN.elf\n").encode(),
                    base + "TEST-RESULTS.json": json.dumps({"passed": True, "elf_sha256": sha}).encode()}
        with patch.object(source, "fetch", side_effect=lambda url, limit: contents[url]), patch.object(source.subprocess, "run") as validate:
            self.assertEqual(source.verify_asset(release, record), base + "331HEN.elf")
            self.assertEqual(validate.call_count, 1)
            for change in [{base + "331HEN.elf": b"corrupt"},
                           {base + "SHA256SUMS.txt": b"wrong checksum\n"},
                           {base + "TEST-RESULTS.json": b'{"passed":true,"elf_sha256":"wrong"}'}]:
                old = contents.copy(); contents.update(change)
                with self.subTest(change=change), self.assertRaises(ValueError): source.verify_asset(release, record)
                contents.clear(); contents.update(old)

    def test_experiment_cannot_use_stable_saved_filename_or_hide_status(self):
        record = dict(tag="v0.0.13-331.1", sha256="a" * 64, kind="experimental",
                      firmware="12.70", ps5_runtime_tested=False, listed_by="anhar331", purpose="Separate test")
        source.verify_experiment(record)
        url = "https://github.com/anhar331/331HEN/releases/download/" + record["tag"] + "/331HEN.elf"
        document = source.catalog(self.release, self.record,
                                  "https://github.com/anhar331/331HEN/releases/download/" + source.BASELINE_TAG + "/331HEN.elf")
        document["payloads"].append(source.experimental_item(self.release, record, url))
        source.validate_catalog(document)
        self.assertEqual(document["payloads"][1]["filename"], "331HEN-GamePatches-TEST_v0.0.13-331.1.elf")
        for change in [{"filename": source.saved_filename(record["tag"])}, {"category": "Homebrew Enablers"},
                       {"description": "Stable build"}, {"name": "331HEN"}]:
            bad = copy.deepcopy(document); bad["payloads"][1].update(change)
            with self.subTest(change=change), self.assertRaises(ValueError): source.validate_catalog(bad)
        with self.assertRaises(ValueError): source.verify_approval(record)
        with self.assertRaises(ValueError): source.verify_experiment({**record, "ps5_runtime_tested": True})

    def test_explicit_listing_preserves_default_and_survives_refresh(self):
        experiment = dict(tag="v0.0.13-331.1", sha256="a" * 64, kind="experimental", firmware="12.70",
                          ps5_runtime_tested=False, listed_by="anhar331", purpose="Separate test")
        release = {**self.release, "tag_name": experiment["tag"]}
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "payloads.json"
            with patch.object(source, "get_release", side_effect=lambda tag: self.release if tag == self.record["tag"] else release), \
                 patch.object(source, "verify_asset", side_effect=lambda rel, rec, **kw:
                              "https://github.com/anhar331/331HEN/releases/download/" + rec["tag"] + "/331HEN.elf"):
                initial = source.refresh({"approvals": [self.record]}, output)
                for _ in range(2):
                    combined = source.refresh({"approvals": [self.record]}, output, [experiment])
                    self.assertEqual(combined["payloads"][0], initial["payloads"][0])
                    self.assertEqual(len(combined["payloads"]), 2)

    def test_failed_experimental_download_preserves_entire_source(self):
        experiment = dict(tag="v0.0.13-331.1", sha256="a" * 64, kind="experimental", firmware="12.70",
                          ps5_runtime_tested=False, listed_by="anhar331", purpose="Separate test")
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "payloads.json"
            output.write_text(json.dumps({"payloads": [{"version": self.record["tag"], "checksum": self.record["sha256"]}]}))
            before = output.read_bytes()
            with patch.object(source, "get_release", side_effect=[self.release, OSError("Experimental download failed")]), \
                 patch.object(source, "verify_asset", return_value="https://github.com/anhar331/331HEN/releases/download/" + self.record["tag"] + "/331HEN.elf"):
                with self.assertRaises(OSError): source.refresh({"approvals": [self.record]}, output, [experiment])
            self.assertEqual(before, output.read_bytes())


if __name__ == "__main__": unittest.main()
