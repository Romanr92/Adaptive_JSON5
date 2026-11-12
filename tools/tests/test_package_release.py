"""Local packaging regression units; no additional PR quality gate."""

import importlib.util
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock


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


class ReleaseTagExportTests(unittest.TestCase):
    """The exact tag reaches CMake, so packages export AdaptiveJSON5_RELEASE_TAG (#129)."""

    class StopAfterConfigure(Exception):
        """Raised by the fake configure step so the tool never builds anything."""

    def run_until_configured(self, tag, configured_tag):
        """Run main() with fake git/CMake calls; return the configure command and error."""
        commands = []
        temporary = tempfile.TemporaryDirectory(prefix="aj5-package-")
        self.addCleanup(temporary.cleanup)
        build = Path(temporary.name) / "build"
        build.mkdir()
        info = {"compiler_id": "GNU", "system": "Linux", "target_triple": "x86_64-pc-linux-gnu",
                "glibcxx_cxx11_abi": "1", "source_revision": "abc123"}
        # None models a source older than the tag export, which records no release_tag.
        if configured_tag is not None:
            info["release_tag"] = configured_tag
        (build / "build-info.json").write_text(json.dumps(info))

        def fake_run(*args, cwd=None):
            return {"rev-parse": "abc123", "status": ""}.get(args[1], "") if args[0] == "git" else ""

        def fake_subprocess_run(command, check=False):
            commands.append(command)
            if "--build" in command:
                raise self.StopAfterConfigure()

        argv = ["package_release.py", "--platform", "linux-x86_64-gcc", "--version", tag,
                "--build-dir", str(build)]
        with mock.patch.object(package_release, "run", fake_run), \
                mock.patch.object(package_release.subprocess, "run", fake_subprocess_run), \
                mock.patch.object(sys, "argv", argv), \
                mock.patch.object(sys, "stderr", io.StringIO()) as errors:
            try:
                package_release.main()
            except self.StopAfterConfigure:
                pass
            except SystemExit:
                return commands[0], errors.getvalue()
        return commands[0], ""

    def test_configure_receives_the_exact_tag(self):
        configure, error = self.run_until_configured("0.2.0-rc.1", "0.2.0-rc.1")
        self.assertIn("-DADAPTIVE_JSON5_RELEASE_TAG=0.2.0-rc.1", configure)
        self.assertEqual(error, "")

    def test_configured_tag_must_equal_the_requested_tag(self):
        _, error = self.run_until_configured("0.2.0-rc.1", "0.2.0")
        self.assertIn("CMake release tag differs from --version", error)

    def test_recovery_source_without_the_tag_export_is_still_packaged(self):
        # Recovering an existing release packages its old tagged source with these tools.
        configure, error = self.run_until_configured("0.2.0", None)
        self.assertIn("-DADAPTIVE_JSON5_RELEASE_TAG=0.2.0", configure)
        self.assertEqual(error, "")


if __name__ == "__main__":
    unittest.main()
