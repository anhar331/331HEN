import copy
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
        bad = copy.deepcopy(valid); bad["payloads"][0]["url"] = "https://github.com/anhar331/331HEN"
        with self.assertRaises(ValueError): source.validate_catalog(bad)
        bad = copy.deepcopy(valid); bad["payloads"][0]["description"] = "bad } metadata"
        with self.assertRaises(ValueError): source.validate_catalog(bad)

    def test_missing_or_draft_asset_refused(self):
        for release in [self.release, {**self.release, "draft": True}]:
            with self.assertRaises(ValueError): source.verify_asset(release, self.record)


if __name__ == "__main__": unittest.main()
