"""Local packaging regression units; no additional PR quality gate."""

import importlib.util
from pathlib import Path
import unittest


spec = importlib.util.spec_from_file_location(
    "package_release", Path(__file__).resolve().parents[1] / "package_release.py")
package_release = importlib.util.module_from_spec(spec)
spec.loader.exec_module(package_release)


class ReleaseTagTests(unittest.TestCase):
    def test_numeric_tag_used_by_published_release(self):
        self.assertEqual(package_release.release_version("0.1.0"), "0.1.0")

    def test_prefixed_tag_remains_supported(self):
        self.assertEqual(package_release.release_version("v0.1.0"), "0.1.0")

    def test_prerelease_tags_with_either_prefix(self):
        for tag in ("0.1.0-rc.1", "v0.1.0-rc.1", "1.2.3-beta-2"):
            with self.subTest(tag=tag):
                self.assertEqual(package_release.release_version(tag), tag.lstrip("v").split("-")[0])

    def test_unversioned_malformed_and_path_tags_are_rejected(self):
        for tag in ("", "latest", "main", "0.1", "0.1.0/other", "../0.1.0",
                    "0.1.0-", "0.1.0\n", "vv0.1.0", "0.1.0;echo"):
            with self.subTest(tag=tag):
                with self.assertRaises(ValueError):
                    package_release.release_version(tag)


if __name__ == "__main__":
    unittest.main()
