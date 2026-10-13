#!/usr/bin/env python3
"""Build or launch the repository's native targets from Zed without shell chaining."""

# SPDX-License-Identifier: Apache-2.0
import argparse
from pathlib import Path
import subprocess
import sys


ROOT = Path(__file__).resolve().parent.parent

ACTIONS = (
    ("Build Unit Test", "build-tests"),
    ("Launch Unit Test", "launch-tests"),
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

    if options.action in ("build-tests", "launch-tests"):
        preset = "debug-unit-tests"
        # The native build system tracks source/header dependencies and does no work when current.
        run(["cmake", "--build", "--preset", preset])
        if options.action == "launch-tests":
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
