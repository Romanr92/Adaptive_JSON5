#!/usr/bin/env python3
"""Build or launch the repository's native targets from Zed without shell chaining."""

# SPDX-License-Identifier: Apache-2.0
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys


ROOT = Path(__file__).resolve().parent.parent

ACTIONS = (
    ("Build Unit Test", "build-tests"),
    ("Launch Unit Test", "launch-tests"),
    ("Build Official JSON & JSON5 Validation Tests", "build-validation"),
    ("Launch Official JSON & JSON5 Validation Tests", "launch-validation"),
    ("Build Executable", "build-exe"),
    ("Launch Executable", "launch-exe"),
)


def run(arguments):
    """Preserve the command's exit code and stop the workflow on failure."""
    print("Running: " + " ".join(str(arg) for arg in arguments), flush=True)
    try:
        completed = subprocess.run(arguments, cwd=ROOT, check=False)
    except OSError as exc:
        print(f"Cannot start command: {exc}", file=sys.stderr)
        raise SystemExit(2) from exc
    if completed.returncode:
        raise SystemExit(completed.returncode)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "action",
        choices=tuple(action for _, action in ACTIONS),
    )
    options = parser.parse_args()

    test_presets = {
        "build-tests": "debug-unit-tests",
        "launch-tests": "debug-unit-tests",
        "build-validation": "validation-tests",
        "launch-validation": "validation-tests",
    }
    if options.action in test_presets:
        preset = test_presets[options.action]
        cache = ROOT / "build" / preset / "CMakeCache.txt"
        cache_text = cache.read_text(encoding="utf-8") if cache.is_file() else ""
        broken_nmake = (
            "CMAKE_GENERATOR:INTERNAL=NMake Makefiles" in cache_text
            and shutil.which("nmake") is None
        )
        if not cache.is_file() or broken_nmake:
            configure = ["cmake", "--preset", preset]
            if broken_nmake:
                if not shutil.which("ninja"):
                    print("NMake is unavailable. Install Ninja or launch Zed from a Visual Studio developer terminal.",
                          file=sys.stderr)
                    return 2
                # Changing generators requires a fresh cache, not deleting the build tree.
                configure.extend(["--fresh", "-G", "Ninja"])
            elif shutil.which("ninja") and not os.environ.get("CMAKE_GENERATOR"):
                configure.extend(["-G", "Ninja"])

            # Ordinary Windows editor sessions may have GCC but no initialized MSVC.
            # Respect an explicitly selected compiler or toolchain.
            if (sys.platform == "win32" and shutil.which("cl") is None
                    and shutil.which("g++") and not os.environ.get("CXX")
                    and not os.environ.get("CMAKE_TOOLCHAIN_FILE")):
                configure.append("-DCMAKE_CXX_COMPILER=" + shutil.which("g++"))
            # Reuse the shared checkout when configuring a new build directory.
            gtest_source = ROOT / "tools/gtest"
            if (gtest_source / "CMakeLists.txt").is_file():
                configure.append(f"-DADAPTIVE_JSON5_GOOGLETEST_SOURCE_DIR={gtest_source}")
            run(configure)
        # The native build system tracks source/header dependencies and does no work when current.
        run(["cmake", "--build", "--preset", preset])
        if options.action.startswith("launch-"):
            run(["ctest", "--preset", preset, "--verbose"])
        return 0

    # Do not mistake the existing example-data target for a compiled executable.
    if not (ROOT / "examples/main.cpp").is_file():
        print("The example executable is not implemented yet (examples/main.cpp is absent).", file=sys.stderr)
        return 2

    # Launch can reuse an existing configuration; the build system handles later CMake changes.
    if options.action != "launch-exe" or not (ROOT / "build/debug-example/CMakeCache.txt").is_file():
        run(["cmake", "--preset", "debug-example"])
    run(["cmake", "--build", "--preset", "debug-example", "--target", "adaptive_json5_examples"])
    if options.action == "launch-exe":
        output = ROOT / "build/debug-example/examples"
        name = "adaptive_json5_examples.exe" if sys.platform == "win32" else "adaptive_json5_examples"
        # Multi-configuration generators place the Debug binary in a subdirectory.
        executable = next((path for path in (output / "Debug" / name, output / name) if path.is_file()), None)
        if executable is None:
            print(f"Built executable was not found under {output}.", file=sys.stderr)
            return 2
        run([str(executable)])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
