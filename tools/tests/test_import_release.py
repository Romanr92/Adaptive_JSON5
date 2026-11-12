"""Local regression units for the release import helper and package config (issue #129).

These run CMake configure steps, so they need CMake 3.25+ and a GNU C++ compiler (the
import helper accepts GNU only). They are local packaging checks, not a PR quality gate.
"""

import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tarfile
import tempfile
import unittest


REPOSITORY = Path(__file__).resolve().parents[2]
HELPER = REPOSITORY / "cmake" / "ImportAdaptiveJSON5.cmake"
PROFILE = "linux-x86_64-gcc"
CMAKE = shutil.which("cmake")
GXX = shutil.which("g++")
# Real installs build the current source, so look them up with its own version.
PROJECT_VERSION = re.search(r"project\(AdaptiveJSON5 VERSION ([0-9.]+)",
                            (REPOSITORY / "CMakeLists.txt").read_text(encoding="utf-8"))[1]


def generator_arguments():
    """Pick a generator that works with GNU g++; None when no suitable build tool exists.

    Ninja is preferred. Without it, Windows needs MinGW Makefiles (the default NMake does
    not drive g++); elsewhere the platform default (Unix Makefiles) works.
    """
    if shutil.which("ninja"):
        return ["-G", "Ninja"]
    if os.name == "nt":
        return ["-G", "MinGW Makefiles"] if shutil.which("mingw32-make") else None
    return []


GENERATOR = generator_arguments()
HAVE_TOOLS = bool(CMAKE and GXX and GENERATOR is not None)


def configure(source, build, *definitions):
    """Configure a project with the GNU compiler; return (succeeded, combined output).

    CMake wraps long messages across lines, so whitespace runs are collapsed to one space
    to let tests match whole phrases.
    """
    command = [CMAKE, "-S", str(source), "-B", str(build), *GENERATOR,
               f"-DCMAKE_CXX_COMPILER={GXX}", *definitions]
    completed = subprocess.run(command, capture_output=True, text=True)
    return completed.returncode == 0, " ".join((completed.stdout + completed.stderr).split())


def write_fake_package(root, *, numeric="0.2.0", profile=PROFILE, exported_tag=None,
                       build_info_tag=None):
    """Write a minimal package root with the files the import helper reads.

    exported_tag sets AdaptiveJSON5_RELEASE_TAG in the config (None: not exported, like
    packages built before #129). build_info_tag writes build-info.json (None: no file).
    """
    config_dir = root / "lib" / "cmake" / "AdaptiveJSON5"
    config_dir.mkdir(parents=True)
    lines = [f'set(AdaptiveJSON5_PACKAGE_PROFILE "{profile}")',
             f'set(AdaptiveJSON5_VERSION "{numeric}")']
    if exported_tag is not None:
        lines.append(f'set(AdaptiveJSON5_RELEASE_TAG "{exported_tag}")')
    lines += ["if(NOT TARGET AdaptiveJSON5::parser)",
              "    add_library(AdaptiveJSON5::parser INTERFACE IMPORTED)",
              "endif()",
              "set(AdaptiveJSON5_parser_FOUND TRUE)"]
    (config_dir / "AdaptiveJSON5Config.cmake").write_text("\n".join(lines) + "\n", encoding="utf-8")
    (config_dir / "AdaptiveJSON5ConfigVersion.cmake").write_text(
        f'set(PACKAGE_VERSION "{numeric}")\n'
        "if(PACKAGE_FIND_VERSION VERSION_EQUAL PACKAGE_VERSION)\n"
        "    set(PACKAGE_VERSION_COMPATIBLE TRUE)\n"
        "    set(PACKAGE_VERSION_EXACT TRUE)\n"
        "else()\n"
        "    set(PACKAGE_VERSION_COMPATIBLE FALSE)\n"
        "endif()\n", encoding="utf-8")
    if build_info_tag is not None:
        (root / "build-info.json").write_text(json.dumps({"release_tag": build_info_tag}) + "\n",
                                              encoding="utf-8")


def write_consumer(directory):
    """Write a consumer project that imports a package and prints the exported tag."""
    directory.mkdir(parents=True)
    (directory / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.25)\n"
        "project(consumer LANGUAGES CXX)\n"
        f'include("{HELPER.as_posix()}")\n'
        # For the redirect test: CMake consults a package redirect before PATHS.
        "if(DEFINED TEST_REDIRECT)\n"
        '    set(target "${TEST_REDIRECT}/lib/cmake/AdaptiveJSON5")\n'
        '    file(WRITE "${CMAKE_FIND_PACKAGE_REDIRECTS_DIR}/adaptivejson5-config.cmake"\n'
        '        "include([[${target}/AdaptiveJSON5Config.cmake]])")\n'
        '    file(WRITE "${CMAKE_FIND_PACKAGE_REDIRECTS_DIR}/adaptivejson5-config-version.cmake"\n'
        '        "include([[${target}/AdaptiveJSON5ConfigVersion.cmake]])")\n'
        "endif()\n"
        "adaptive_json5_import_release(VERSION \"${TEST_VERSION}\" PROFILE " + PROFILE + " ${TEST_SOURCE})\n"
        'message(STATUS "RELEASE_TAG=[${AdaptiveJSON5_RELEASE_TAG}]")\n', encoding="utf-8")


@unittest.skipUnless(HAVE_TOOLS, "needs cmake, a GNU g++ and Ninja (or MinGW Makefiles on Windows) on PATH")
class ImportReleaseTagTests(unittest.TestCase):
    """adaptive_json5_import_release compares the exact release tag (REPO-15)."""

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="aj5-import-")
        self.work = Path(self.temporary.name)
        self.consumer = self.work / "consumer"
        write_consumer(self.consumer)
        self.runs = 0

    def tearDown(self):
        self.temporary.cleanup()

    def import_root(self, version, root, *definitions):
        """Import a package root with the given VERSION; return (succeeded, output)."""
        self.runs += 1
        return configure(self.consumer, self.work / f"build{self.runs}",
                         f"-DTEST_VERSION={version}", f"-DTEST_SOURCE=ROOT;{root.as_posix()}",
                         *definitions)

    def import_archive(self, version, archive, sha256, cache):
        """Import an archive through a shared cache; return (succeeded, output)."""
        self.runs += 1
        return configure(self.consumer, self.work / f"build{self.runs}", f"-DTEST_VERSION={version}",
                         f"-DTEST_SOURCE=ARCHIVE;{archive.as_posix()};SHA256;{sha256};CACHE_DIR;{cache.as_posix()}")

    def fake_root(self, name, **package):
        root = self.work / name
        write_fake_package(root, **package)
        return root

    def test_matching_tag_succeeds_and_is_exported(self):
        for tag in ("0.2.0", "0.2.0-rc.1", "v0.2.0"):
            with self.subTest(tag=tag):
                root = self.fake_root(f"pkg-{tag}", exported_tag=tag, build_info_tag=tag)
                succeeded, output = self.import_root(tag, root)
                self.assertTrue(succeeded, output)
                self.assertIn(f"RELEASE_TAG=[{tag}]", output)

    def test_different_tag_of_same_numeric_version_fails_on_root(self):
        cases = [("0.2.0-rc.1", "0.2.0-rc.2"),  # Different pre-release.
                 ("0.2.0", "0.2.0-rc.9"),       # Final package, pre-release requested.
                 ("0.2.0-rc.9", "0.2.0"),       # Pre-release package, final requested.
                 ("0.2.0", "v0.2.0"),           # The v prefix is part of the tag.
                 ("v0.2.0", "0.2.0")]
        for packaged, requested in cases:
            with self.subTest(packaged=packaged, requested=requested):
                root = self.fake_root(f"pkg-{packaged}-{requested}", exported_tag=packaged,
                                      build_info_tag=packaged)
                succeeded, output = self.import_root(requested, root)
                self.assertFalse(succeeded, output)
                self.assertIn(f"release tag {packaged}", output)
                self.assertIn(f"requested VERSION {requested}", output)

    def test_package_without_exported_tag_falls_back_to_build_info(self):
        root = self.fake_root("pkg-old", exported_tag=None, build_info_tag="0.2.0")
        succeeded, output = self.import_root("0.2.0", root)
        self.assertTrue(succeeded, output)
        self.assertIn("RELEASE_TAG=[0.2.0]", output)
        succeeded, output = self.import_root("0.2.0-rc.9", root)
        self.assertFalse(succeeded, output)
        self.assertIn("release tag 0.2.0 ", output)

    def test_stale_cached_tag_does_not_hide_the_fallback(self):
        # A cache entry left by an earlier configure must not stand in for the package's tag.
        root = self.fake_root("pkg-cache", exported_tag=None, build_info_tag="0.2.0")
        stale = "-DAdaptiveJSON5_RELEASE_TAG=0.2.0-rc.9"
        succeeded, output = self.import_root("0.2.0", root, stale)
        self.assertTrue(succeeded, output)
        self.assertIn("RELEASE_TAG=[0.2.0]", output)
        succeeded, output = self.import_root("0.2.0-rc.9", root, stale)
        self.assertFalse(succeeded, output)
        self.assertIn("release tag 0.2.0 ", output)

    def test_redirected_config_is_rejected(self):
        # The redirect loads another package with the same version and profile; the pinned
        # root's build-info.json must not vouch for it.
        root = self.fake_root("pkg-pinned", exported_tag=None, build_info_tag="0.2.0")
        other = self.fake_root("pkg-other", exported_tag=None)
        succeeded, output = self.import_root("0.2.0", root, f"-DTEST_REDIRECT={other.as_posix()}")
        self.assertFalse(succeeded, output)
        self.assertIn("not from the pinned package", output)

    def test_package_without_any_tag_is_rejected(self):
        for name, package in (("no-file", {"exported_tag": None}),
                              ("empty-export", {"exported_tag": "", "build_info_tag": "0.2.0"}),
                              ("empty-file", {"exported_tag": None, "build_info_tag": ""})):
            with self.subTest(case=name):
                root = self.fake_root(f"pkg-{name}", **package)
                succeeded, output = self.import_root("0.2.0", root)
                self.assertFalse(succeeded, output)
                self.assertIn("has no release tag", output)

    def test_wrong_profile_and_numeric_version_still_fail(self):
        root = self.fake_root("pkg-profile", profile="linux-aarch64-gcc", exported_tag="0.2.0")
        succeeded, output = self.import_root("0.2.0", root)
        self.assertFalse(succeeded, output)
        self.assertIn("package profile differs", output)
        root = self.fake_root("pkg-numeric", numeric="0.3.0", exported_tag="0.2.0")
        succeeded, output = self.import_root("0.2.0", root)
        self.assertFalse(succeeded, output)

    def test_reused_cache_checks_the_tag(self):
        # The cache is keyed by the archive checksum; a second configure with the same
        # checksum reuses the extracted package without looking at its folder name.
        tag = "0.2.0-rc.1"
        name = f"AdaptiveJSON5-{tag}-{PROFILE}"
        package = self.fake_root(name, exported_tag=tag, build_info_tag=tag)
        archive = self.work / f"{name}.tar.gz"
        with tarfile.open(archive, "w:gz") as bundle:
            bundle.add(package, arcname=name)
        sha256 = hashlib.sha256(archive.read_bytes()).hexdigest()
        cache = self.work / "cache"
        succeeded, output = self.import_archive(tag, archive, sha256, cache)
        self.assertTrue(succeeded, output)
        for requested in ("0.2.0-rc.2", "0.2.0"):
            with self.subTest(requested=requested):
                succeeded, output = self.import_archive(requested, archive, sha256, cache)
                self.assertFalse(succeeded, output)
                self.assertIn(f"requested VERSION {requested}", output)


@unittest.skipUnless(HAVE_TOOLS, "needs cmake, a GNU g++ and Ninja (or MinGW Makefiles on Windows) on PATH")
class InstalledPackageTests(unittest.TestCase):
    """Real installs of this repository: the exported tag and the hosted component (ARC-06)."""

    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory(prefix="aj5-install-")
        work = Path(cls.temporary.name)
        cls.with_hosted = cls.install(work / "hosted", "-DADAPTIVE_JSON5_BUILD_HOSTED=ON",
                                      "-DADAPTIVE_JSON5_RELEASE_TAG=0.2.0-rc.1")
        cls.without_hosted = cls.install(work / "plain", "-DADAPTIVE_JSON5_BUILD_HOSTED=OFF")
        cls.work = work

    @classmethod
    def tearDownClass(cls):
        cls.temporary.cleanup()

    @staticmethod
    def install(directory, *definitions):
        """Build and install the library only; return the install prefix."""
        build = directory / "build"
        prefix = directory / "prefix"
        succeeded, output = configure(REPOSITORY, build, "-DCMAKE_BUILD_TYPE=Debug",
                                      "-DADAPTIVE_JSON5_BUILD_TESTS=OFF",
                                      "-DADAPTIVE_JSON5_BUILD_VALIDATION_TESTS=OFF",
                                      "-DADAPTIVE_JSON5_BUILD_EXECUTABLE=OFF",
                                      "-DADAPTIVE_JSON5_BUILD_LIBRARY=ON",
                                      # The import helper expects <root>/lib/cmake/AdaptiveJSON5.
                                      "-DCMAKE_INSTALL_LIBDIR=lib", *definitions)
        if not succeeded:
            raise RuntimeError(output)
        subprocess.run([CMAKE, "--build", str(build)], check=True, capture_output=True)
        subprocess.run([CMAKE, "--install", str(build), "--prefix", str(prefix)], check=True,
                       capture_output=True)
        return prefix

    def find(self, prefix, name, find_arguments):
        """Configure a consumer calling find_package with the given arguments."""
        consumer = self.work / f"consumer-{name}"
        consumer.mkdir()
        (consumer / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.25)\n"
            "project(consumer LANGUAGES CXX)\n"
            f"find_package(AdaptiveJSON5 {PROJECT_VERSION} EXACT CONFIG {find_arguments})\n"
            'message(STATUS "FOUND=[${AdaptiveJSON5_FOUND}] HOSTED=[${AdaptiveJSON5_hosted_FOUND}] '
            'RELEASE_TAG=[${AdaptiveJSON5_RELEASE_TAG}]")\n'
            "if(AdaptiveJSON5_hosted_FOUND AND NOT TARGET AdaptiveJSON5::hosted)\n"
            '    message(FATAL_ERROR "hosted found without its target")\n'
            "endif()\n", encoding="utf-8")
        return configure(consumer, self.work / f"build-{name}", f"-DCMAKE_PREFIX_PATH={prefix.as_posix()}")

    def test_hosted_install_is_found_as_a_component(self):
        succeeded, output = self.find(self.with_hosted, "hosted", "REQUIRED COMPONENTS parser hosted")
        self.assertTrue(succeeded, output)
        self.assertIn("HOSTED=[TRUE]", output)

    def test_release_tag_is_exported(self):
        succeeded, output = self.find(self.with_hosted, "tag", "REQUIRED COMPONENTS parser")
        self.assertTrue(succeeded, output)
        self.assertIn("RELEASE_TAG=[0.2.0-rc.1]", output)

    def test_source_build_exports_an_empty_tag(self):
        succeeded, output = self.find(self.without_hosted, "source-tag", "REQUIRED COMPONENTS parser")
        self.assertTrue(succeeded, output)
        self.assertIn("RELEASE_TAG=[]", output)

    def test_missing_hosted_fails_clearly_when_required(self):
        succeeded, output = self.find(self.without_hosted, "missing", "REQUIRED COMPONENTS parser hosted")
        self.assertFalse(succeeded, output)
        self.assertIn("ADAPTIVE_JSON5_BUILD_HOSTED=ON", output)

    def test_hosted_is_reported_per_installation(self):
        # An earlier lookup of a hosted install imports AdaptiveJSON5::hosted; a later lookup
        # of a parser-only install in the same directory must still report hosted as missing.
        consumer = self.work / "consumer-two-installs"
        consumer.mkdir()
        (consumer / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.25)\n"
            "project(consumer LANGUAGES CXX)\n"
            f'find_package(AdaptiveJSON5 CONFIG REQUIRED COMPONENTS parser hosted PATHS "{self.with_hosted.as_posix()}" NO_DEFAULT_PATH)\n'
            'message(STATUS "FIRST=[${AdaptiveJSON5_hosted_FOUND}]")\n'
            "unset(AdaptiveJSON5_DIR CACHE)\n"
            f'find_package(AdaptiveJSON5 CONFIG REQUIRED COMPONENTS parser OPTIONAL_COMPONENTS hosted PATHS "{self.without_hosted.as_posix()}" NO_DEFAULT_PATH)\n'
            'message(STATUS "SECOND=[${AdaptiveJSON5_hosted_FOUND}] FROM=[${AdaptiveJSON5_CONFIG}]")\n',
            encoding="utf-8")
        succeeded, output = configure(consumer, self.work / "build-two-installs")
        self.assertTrue(succeeded, output)
        self.assertIn("FIRST=[TRUE]", output)
        self.assertIn("SECOND=[FALSE]", output)
        self.assertIn(self.without_hosted.name, output.split("FROM=[", 1)[1])

    def test_import_helper_rejects_a_source_install(self):
        # A source install has profile "source" and no tag; the helper must point to find_package
        # instead of reporting a profile mismatch.
        consumer = self.work / "consumer-helper-source"
        write_consumer(consumer)
        succeeded, output = configure(consumer, self.work / "build-helper-source",
                                      f"-DTEST_VERSION={PROJECT_VERSION}",
                                      f"-DTEST_SOURCE=ROOT;{self.without_hosted.as_posix()}")
        self.assertFalse(succeeded, output)
        self.assertIn("is a source build, not a release package", output)
        self.assertNotIn("package profile differs", output)

    def test_missing_hosted_is_reported_when_optional(self):
        succeeded, output = self.find(self.without_hosted, "optional",
                                      "REQUIRED COMPONENTS parser OPTIONAL_COMPONENTS hosted")
        self.assertTrue(succeeded, output)
        self.assertIn("FOUND=[1]", output)
        self.assertIn("HOSTED=[FALSE]", output)


if __name__ == "__main__":
    unittest.main()
