#!/usr/bin/env python3
"""Check or apply the repository's C++ formatting with clang-format 18."""

import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
EXCLUDED = {".git", "build", "out", "install", "generated", "vendor", "third_party", "node_modules"}
SUFFIXES = {".h", ".hpp", ".c", ".cpp", ".cc", ".cxx"}


def source_files():
    for directory, subdirectories, filenames in os.walk(ROOT):
        subdirectories[:] = sorted(
            name for name in subdirectories
            if name not in EXCLUDED and not name.startswith("build-")
        )
        for name in sorted(filenames):
            path = Path(directory) / name
            if path.suffix in SUFFIXES:
                yield path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="Report files needing formatting")
    parser.add_argument("--fix", action="store_true", help="Format source files in place")
    parser.add_argument("--clang-format", help="Path to a clang-format 18 executable")
    arguments = parser.parse_args()
    if arguments.check and arguments.fix:
        parser.error("choose only one of --check and --fix")

    requested = arguments.clang_format or os.environ.get("CLANG_FORMAT")
    executable = requested or shutil.which("clang-format-18") or shutil.which("clang-format")
    if not executable:
        parser.error("clang-format 18 was not found; install it or use --clang-format PATH")

    try:
        version = subprocess.run(
            [executable, "--version"], check=True, capture_output=True, text=True
        ).stdout
        if not re.search(r"\bversion 18\.", version):
            parser.error("clang-format major version 18 is required: " + version.strip())

        files = list(source_files())
        if not files:
            parser.error("no C++ source files were found")
        failed = False
        for path in files:
            command = [executable, "--style=file", "--fallback-style=none"]
            if arguments.fix:
                command.append("-i")
            else:
                command.extend(["--dry-run", "--Werror"])
            result = subprocess.run(command + [str(path)], cwd=ROOT, check=False)
            failed = failed or result.returncode != 0
        if failed:
            return 1
        action = "Formatted" if arguments.fix else "Formatting passed for"
        print(f"{action} {len(files)} C++ files.")
        return 0
    except (OSError, subprocess.CalledProcessError) as error:
        print(f"Formatting could not run: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
