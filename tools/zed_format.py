#!/usr/bin/env python3
"""Format Zed's stdin buffer with this checkout's pinned clang-format rules."""

# SPDX-License-Identifier: Apache-2.0
from pathlib import Path
import re
import shutil
import subprocess
import sys


ROOT = Path(__file__).resolve().parent.parent


def main():
    if len(sys.argv) != 2:
        print("Expected the edited buffer path.", file=sys.stderr)
        return 2

    candidates = list(filter(None, (shutil.which("clang-format-23"), shutil.which("clang-format"))))

    for executable in candidates:
        try:
            version = subprocess.run(
                [executable, "--version"], check=True, capture_output=True, text=True
            ).stdout.strip()
        except (OSError, subprocess.CalledProcessError):
            continue
        if re.search(r"\bversion 23\.1\.6(?:\s|$)", version):
            break
    else:
        print(
            "clang-format 23.1.6 is required on PATH: on Windows run 'scoop install llvm@23.1.6'; "
            "on Linux use the official LLVM 23.1.6 release (see core/docs/design-development.md).",
            file=sys.stderr,
        )
        return 2

    # Format the unsaved buffer through stdin/stdout; never read or rewrite its file.
    command = [
        executable,
        "--style=file:" + str(ROOT / ".clang-format"),
        "--fallback-style=none",
        "--assume-filename=" + sys.argv[1],
    ]
    try:
        return subprocess.run(command, cwd=ROOT, check=False).returncode
    except OSError as error:
        print("Cannot start clang-format: " + str(error), file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
