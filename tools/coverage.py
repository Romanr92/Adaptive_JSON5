#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Build and run host units with GCC coverage, then write a local gcovr report."""

import argparse
import importlib.util
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build" / "coverage"
SOURCE_FILTER = (
    r"(?!(?:tests|build(?:-[^/]+)?|Build|out|install|third_party|vendor|generated|node_modules|\.git)/"
    r"|tools/gtest/).*(?i:\.(?:c|cc|cp|cpp|cxx|c\+\+|h|hh|hp|hpp|hxx|h\+\+|inl|inc|ipp|tpp|tcc|txx|ixx|cppm|cxxm|ccm|c\+\+m))$"
)


def run(command):
    print("+ " + subprocess.list2cmdline([str(arg) for arg in command]), flush=True)
    subprocess.run([str(arg) for arg in command], cwd=ROOT, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default="g++", help="GCC C++ compiler (default: g++)")
    parser.add_argument("--gcov", help="Matching gcov executable (default: beside the compiler)")
    parser.add_argument("--jobs", type=int, default=12, help="Parallel build jobs (default: 12)")
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
            "cmake", "-S", ROOT, "-B", BUILD, "-G", "Ninja",
            "-DCMAKE_CXX_COMPILER=" + compiler,
            "-DCMAKE_BUILD_TYPE=Debug",
            "-DCMAKE_CXX_FLAGS=--coverage -O0 -g",
            "-DCMAKE_EXE_LINKER_FLAGS=--coverage",
            "-DADAPTIVE_JSON5_BUILD_TESTS=ON",
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

        run(["cmake", "--build", BUILD, "--parallel", args.jobs])

        # Reset only runtime counters in this dedicated build so repeated reports
        # describe the current unit run, rather than accumulating older results.
        for counter in BUILD.rglob("*.gcda"):
            if counter.resolve().is_relative_to(BUILD.resolve()):
                counter.unlink()

        run(["ctest", "--test-dir", BUILD, "--output-on-failure", "--no-tests=error"])
        report = BUILD / "report"
        report.mkdir(exist_ok=True)
        run([
            sys.executable, "-m", "gcovr", BUILD,
            "--root", ROOT, "--filter", SOURCE_FILTER,
            # gcovr parses this argument as a shell-style command, even on Windows.
            "--gcov-executable", shlex.quote(Path(shutil.which(gcov)).resolve().as_posix()),
            "--json", report / "coverage.json",
        ])
        # Read only this run's data, then discover all requested source files
        # from the root, including files with no compiler instrumentation.
        run([
            sys.executable, "-m", "gcovr", ROOT,
            "--root", ROOT, "--filter", SOURCE_FILTER,
            "--json-add-tracefile", report / "coverage.json",
            "--include", SOURCE_FILTER,
            "--html-details", report / "index.html", "--print-summary",
        ])
        print("Report: " + str(report / "index.html"))
        return 0
    except (OSError, subprocess.CalledProcessError) as error:
        print("Coverage report failed: " + str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
