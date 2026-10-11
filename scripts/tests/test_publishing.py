import hashlib
import importlib.util
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

spec = importlib.util.spec_from_file_location("publisher", Path(__file__).parents[1] / "publish_release.py")
publisher = importlib.util.module_from_spec(spec)
spec.loader.exec_module(publisher)


class PublishingTests(unittest.TestCase):
    def test_version_allocation(self):
        self.assertEqual(publisher.next_tag(["v0.0.13", "v1.10"]), "v0.0.13-331.1")
        self.assertEqual(publisher.next_tag(["v0.0.13-331.2", "v0.0.13-331.9"]), "v0.0.13-331.10")

    def test_results_must_match_exact_binary_and_remain_untested(self):
        with tempfile.TemporaryDirectory() as d:
            elf = Path(d) / "payload"
            elf.write_bytes(b"compiled fixture")
            results = dict(passed=True, elf_sha256=publisher.checksum(elf), regression_cases=436,
                           patch_sanitizers_passed=True, live_linux_https_passed=True,
                           ps5_runtime_tested=False)
            publisher.require_tests(results, elf)
            for key, value in [("passed", False), ("elf_sha256", "different"), ("regression_cases", 0),
                               ("ps5_runtime_tested", True), ("patch_sanitizers_passed", False)]:
                with self.subTest(key=key), self.assertRaises(RuntimeError):
                    publisher.require_tests({**results, key: value}, elf)

    def test_anonymous_download_integrity(self):
        data = b"public binary"
        class Response:
            status = 200
            def __init__(self): self.remaining = data
            def __enter__(self): return self
            def __exit__(self, *args): pass
            def read(self, size):
                out, self.remaining = self.remaining, b""
                return out
        with patch.object(publisher.urllib.request, "urlopen", side_effect=lambda *a, **k: Response()) as call:
            publisher.verify_public("https://github.com/fixture", hashlib.sha256(data).hexdigest())
            self.assertNotIn("Authorization", call.call_args.args[0].headers)
            with self.assertRaises(RuntimeError):
                publisher.verify_public("https://github.com/fixture", "wrong")

    def test_preview_record_uses_the_published_binary_and_is_never_an_approval(self):
        with tempfile.TemporaryDirectory() as directory:
            elf = Path(directory) / "331HEN.elf"
            elf.write_bytes(b"published compiled fixture")
            record = publisher.preview_record("v0.0.13-331.3", elf)
            self.assertEqual(record["sha256"], publisher.checksum(elf))
            self.assertEqual(record["kind"], "experimental")
            self.assertIs(record["ps5_runtime_tested"], False)
            self.assertNotIn("approved_by", record)
            with self.assertRaises(RuntimeError):
                publisher.preview_record("unrelated-tag", elf)

    def test_private_tracked_path_refused(self):
        with tempfile.TemporaryDirectory() as d:
            with (patch.object(publisher, "ROOT", Path(d)),
                  patch.object(publisher, "tracked_entries", return_value=[("100644", ".env")])):
                with self.assertRaises(RuntimeError):
                    publisher.source_archive(Path(d) / "source.tar.gz")
