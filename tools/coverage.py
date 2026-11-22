#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Report local GCC coverage for the compiled core and its public/private headers."""

import argparse
from concurrent.futures import ThreadPoolExecutor
import gzip
import importlib.util
import json
import os
import re
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time


ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build" / "coverage"
SOURCE_FILTER = (
    r"^(?:includes/|core/(?:includes|src)/)"
    r".*(?i:\.(?:c|cc|cp|cpp|cxx|c\+\+|h|hh|hp|hpp|hxx|h\+\+|inl|inc|ipp|tpp|tcc|txx|ixx|cppm|cxxm|ccm|c\+\+m))$"
)


def run(command):
    print("+ " + subprocess.list2cmdline([str(arg) for arg in command]), flush=True)
    subprocess.run([str(arg) for arg in command], cwd=ROOT, check=True)


def reset_counters():
    # Restrict removal to runtime data in this helper's dedicated build tree.
    for counter in BUILD.rglob("*.gcda"):
        if counter.resolve().is_relative_to(BUILD.resolve()):
            # Windows briefly locks freshly read files (gcov, antivirus scans).
            for attempt in range(50):
                try:
                    counter.unlink()
                    break
                except PermissionError:
                    if attempt == 49:
                        raise
                    time.sleep(0.1)


def remove_obsolete_notes():
    # A layout change leaves .gcno files for objects no longer in Ninja's graph.
    # gcovr reads those too, mixing old header locations with current profiles.
    targets = subprocess.check_output(
        ["ninja", "-C", str(BUILD), "-t", "targets", "all"],
        cwd=ROOT, text=True)
    current_notes = {
        (BUILD / line.rsplit(": ", 1)[0]).resolve().with_suffix(".gcno")
        for line in targets.splitlines()
        if Path(line.rsplit(": ", 1)[0]).suffix in (".o", ".obj")
    }
    if not current_notes:
        raise OSError("Ninja registered no object files for coverage collection")
    removed = 0
    for note in BUILD.rglob("*.gcno"):
        resolved = note.resolve()
        if resolved.is_relative_to(BUILD.resolve()) and resolved not in current_notes:
            note.unlink()
            removed += 1
    print(f"Removed {removed} obsolete coverage notes.", flush=True)


def extract_counters(gcov, inputs, destination, jobs):
    """Extract independent profiles in parallel, without launching gcovr per unit."""
    destination.mkdir()

    def extract(item):
        index, counter = item
        command = [gcov, "--json-format", "--branch-probabilities",
                   "--branch-counts", "--stdout", str(counter.resolve())]
        result = subprocess.run(command, cwd=ROOT, capture_output=True)
        if result.returncode:
            raise OSError(f"gcov failed for {counter}: "
                          + result.stderr.decode(errors="replace"))
        # Keep each executable's counts separate until gcovr merges them with
        # Python integers. Combining GCC profiles first can overflow gcov counts.
        with gzip.open(destination / f"{index:04}.gcov.json.gz", "wb") as output:
            output.write(result.stdout)

    with ThreadPoolExecutor(max_workers=jobs) as workers:
        list(workers.map(extract, enumerate(inputs)))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default="g++", help="GCC C++ compiler (default: g++)")
    parser.add_argument("--gcov", help="Matching gcov executable (default: beside the compiler)")
    parser.add_argument("--jobs", type=int, default=os.cpu_count() or 1,
                        help="Parallel build/test/gcov workers (default: all logical CPUs)")
    parser.add_argument("--gtest-source", type=Path, help="Existing GoogleTest 1.17.0 source checkout")
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    if importlib.util.find_spec("gcovr") is None:
        parser.error("gcovr is missing; install with: python -m pip install gcovr==8.6")

    compiler = shutil.which(args.compiler)
    if not compiler:
        parser.error("GCC compiler not found: " + args.compiler)
    suffix = ".exe" if os.name == "nt" else ""
    gcov = args.gcov or str(Path(compiler).parent / ("gcov" + suffix))
    if not shutil.which(gcov):
        parser.error("matching gcov not found; supply --gcov PATH")
    gcov_version = subprocess.check_output([gcov, "--version"], text=True)
    version = re.search(r"\)\s+(\d+)\.", gcov_version)
    if version is None or int(version.group(1)) < 14:
        parser.error("JSON coverage collection requires GCC/gcov 14 or newer "
                     "(gcovr requires gcov JSON format 2); select matching --compiler and --gcov")
    for tool in ("cmake", "ctest", "ninja"):
        if not shutil.which(tool):
            parser.error(tool + " is required on PATH")

    # Reuse the existing checkout rather than having a second FetchContent
    # build try to repopulate the shared tools/gtest directory.
    gtest_source = args.gtest_source
    if gtest_source is None and (ROOT / "tools/gtest/CMakeLists.txt").is_file():
        gtest_source = ROOT / "tools/gtest"

    try:
        run([
            "cmake", "--fresh", "-S", ROOT, "-B", BUILD, "-G", "Ninja",
            "-DCMAKE_CXX_COMPILER=" + compiler,
            "-DCMAKE_BUILD_TYPE=Debug",
            # The concurrency units execute the core on several threads even
            # during sequential collection. Non-atomic counters lose updates
            # and can make gcov infer negative branch counts.
            "-DCMAKE_CXX_FLAGS=--coverage -fprofile-update=atomic -O0 -g",
            "-DCMAKE_EXE_LINKER_FLAGS=--coverage",
            "-DADAPTIVE_JSON5_BUILD_TESTS=ON",
            # Validation tests are a separate gate and never contribute to coverage.
            "-DADAPTIVE_JSON5_BUILD_VALIDATION_TESTS=OFF",
            # Include the optional schema component and its units (ADR 0006).
            "-DADAPTIVE_JSON5_BUILD_SCHEMA=ON",
            "-DADAPTIVE_JSON5_BUILD_LIBRARY=OFF",
            "-DADAPTIVE_JSON5_BUILD_EXECUTABLE=OFF",
            "-DADAPTIVE_JSON5_ENABLE_EXCEPTIONS=OFF",
            "-DADAPTIVE_JSON5_ENABLE_RTTI=OFF",
            "-DADAPTIVE_JSON5_GOOGLETEST_SOURCE_DIR="
            + (str(gtest_source.resolve()) if gtest_source else ""),
        ])
        compiler_files = list((BUILD / "CMakeFiles").glob("*/CMakeCXXCompiler.cmake"))
        if not any('set(CMAKE_CXX_COMPILER_ID "GNU")' in path.read_text(encoding="utf-8")
                   for path in compiler_files):
            parser.error("this helper supports GCC only; select a GCC compiler with --compiler")

        # GoogleTest discovery runs newly linked executables during the build.
        # Clear profiles from older binaries before discovery can merge them
        # with instrumentation whose checksum changed after recompilation.
        reset_counters()
        remove_obsolete_notes()
        run(["cmake", "--build", BUILD, "--parallel", args.jobs])

        # Reset only runtime counters in this dedicated build so repeated reports
        # describe the current unit run, rather than accumulating older results.
        reset_counters()

        # Each discovered GoogleTest case is a separate CTest process. All linked
        # executables share the static core's .gcda files, whose concurrent writes
        # lose counts on the Windows GCC host. Run the tests and collect
        # deterministic counters with one process per registered
        # executable, sequentially. Do not discover stale binaries by globbing.
        discovered = json.loads(subprocess.check_output(
            ["ctest", "--test-dir", str(BUILD), "--show-only=json-v1"],
            cwd=ROOT, text=True))
        executables = sorted({test["command"][0] for test in discovered["tests"]})
        if not executables:
            raise OSError("CTest registered no unit executables for coverage collection")
        for executable in executables:
            if not Path(executable).resolve().is_relative_to(BUILD.resolve()):
                raise OSError("Unit executable is outside the coverage build: " + executable)
        report = BUILD / "report"
        report.mkdir(exist_ok=True)
        # Raw profiles are temporary; gcovr parses and merges them in one pass
        # instead of starting a separate report process for every executable.
        with tempfile.TemporaryDirectory(prefix="raw-", dir=BUILD) as temporary:
            raw = Path(temporary)
            print("Extracting uncovered baseline once.", flush=True)
            reset_counters()
            extract_counters(gcov, sorted(BUILD.rglob("*.gcno")), raw / "baseline", args.jobs)
            for index, executable in enumerate(executables):
                print(f"Collecting unit executable {index + 1}/{len(executables)}.", flush=True)
                reset_counters()
                run([executable, "--gtest_brief=1"])
                counters = sorted(BUILD.rglob("*.gcda"))
                if not counters:
                    raise OSError("Unit executable produced no coverage counters: " + executable)
                extract_counters(gcov, counters, raw / f"unit-{index:03}", args.jobs)
            # Include uninstrumented source files as well as the zero-count
            # baseline, preserving uncovered lines, functions and branches.
            run([
                sys.executable, "-m", "gcovr", raw,
                "--root", ROOT, "--filter", SOURCE_FILTER,
                "--gcov-use-existing-files", "-j", args.jobs,
                "--include", SOURCE_FILTER,
                "--json-summary", report / "summary.json",
                "--html-details", report / "index.html",
                "--html-single-page", "--html-self-contained",
                "--no-html-syntax-highlighting", "--print-summary",
            ])
        # Remove only artifacts from the older multi-file report, after the
        # replacement has been generated successfully.
        for pattern in ("coverage-*.json", "coverage.json", "index.*.html", "index.css", "index.js"):
            for stale in report.glob(pattern):
                stale.unlink()
        print("Report: " + str(report / "index.html"))
        return 0
    except (OSError, subprocess.CalledProcessError) as error:
        print("Coverage report failed: " + str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
