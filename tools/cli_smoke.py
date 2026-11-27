#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Run the built adaptive_json5_examples on the smoke cases and check its output.

Reads cli/test/smoke_cases.json (the same cases CTest and the `cli` workflow run), runs the
executable once per case from the repository root, and compares the exit code and the exact
stdout/stderr with cli/test/expected/<name>.out and .err (or checks that stdout contains a
fragment). Each case prints its command and the CLI's output, then PASSED in green or FAILED
in red with a diff. Schema cases are skipped when the executable reports that it was built
without the schema component.

    python tools/cli_smoke.py                       # every build found: build/cli and build/cli-no-schema
    python tools/cli_smoke.py --build cli-no-schema # one preset's build folder
    python tools/cli_smoke.py --exe path/to/adaptive_json5_examples
    python tools/cli_smoke.py --case file_json5_valid --quiet
    python tools/cli_smoke.py --update               # rewrite expected files after a review

Exit code: 0 when every selected case passed, 1 when any failed, 2 for usage errors.
"""

import argparse
import difflib
import json
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CASES = ROOT / "cli" / "test" / "smoke_cases.json"
EXPECTED = ROOT / "cli" / "test" / "expected"
NAME = "adaptive_json5_examples"
# Preset build folders run by default (with and without the schema component), and the
# folders searched when none of them exists.
PRESET_BUILDS = ("cli", "cli-no-schema")
FALLBACK_SEARCH = ("build/debug-example", "build")

GREEN = "\033[32m"
RED = "\033[31m"
YELLOW = "\033[33m"
BOLD = "\033[1m"
RESET = "\033[0m"


def use_color(stream):
    """Colors only on a terminal; NO_COLOR disables them (https://no-color.org)."""
    if os.environ.get("NO_COLOR") or not stream.isatty():
        return False
    if os.name == "nt":
        os.system("")  # Enables ANSI escape processing in the Windows console.
    return True


class Painter:
    def __init__(self, enabled):
        self.enabled = enabled

    def __call__(self, color, text):
        return f"{color}{text}{RESET}" if self.enabled else text


def executable_in(folder):
    """Return the newest adaptive_json5_examples under folder, or None."""
    names = (NAME + ".exe", NAME) if os.name == "nt" else (NAME,)
    if not folder.is_dir():
        return None
    for name in names:
        matches = [m for m in folder.rglob(name) if m.is_file()]
        if matches:
            return max(matches, key=lambda p: p.stat().st_mtime)
    return None


def find_executables(explicit, build):
    """Return the executables to test: --exe, --build, every preset build, or a fallback."""
    if explicit:
        path = Path(explicit)
        return [path] if path.is_file() else []
    if build:
        found = executable_in(ROOT / "build" / build)
        return [found] if found else []
    found = [e for e in (executable_in(ROOT / "build" / name) for name in PRESET_BUILDS) if e]
    if found:
        return found
    for folder in FALLBACK_SEARCH:
        fallback = executable_in(ROOT / folder)
        if fallback:
            return [fallback]
    return []


def normalize(data):
    """Decode as UTF-8 and use \\n line endings, as the CTest check does."""
    return data.decode("utf-8", errors="replace").replace("\r\n", "\n")


def run(executable, args):
    completed = subprocess.run([str(executable), *args], cwd=ROOT, capture_output=True, check=False)
    return completed.returncode, normalize(completed.stdout), normalize(completed.stderr)


def tool_info(executable):
    """Return (tool version, schema available) from the version command."""
    _, out, _ = run(executable, ["version"])
    version = ""
    schema = False
    for line in out.splitlines():
        if line.startswith(NAME + " "):
            version = line[len(NAME) + 1:].strip()
        if line.strip() == "schema on":
            schema = True
    return version, schema


def read_expected(name, suffix):
    path = EXPECTED / f"{name}{suffix}"
    return normalize(path.read_bytes()) if path.is_file() else None


def diff(expected, observed, label):
    lines = difflib.unified_diff(expected.splitlines(keepends=True), observed.splitlines(keepends=True),
                                 fromfile=f"expected {label}", tofile=f"observed {label}")
    return "".join(lines)


def check(case, code, out, err, tool_version):
    """Return a list of failure descriptions (empty when the case passed)."""
    problems = []
    if code != case["exit"]:
        problems.append(f"exit code {code}, expected {case['exit']}")
    if "contains" in case:
        fragment = case["contains"].replace("{tool_version}", tool_version)
        if fragment not in out:
            problems.append(f"stdout does not contain: {fragment}")
        return problems
    want_out = read_expected(case["name"], ".out")
    if want_out is None:
        problems.append(f"missing expected file cli/test/expected/{case['name']}.out")
    elif out != want_out:
        problems.append(diff(want_out, out, "stdout"))
    want_err = read_expected(case["name"], ".err")
    if want_err is not None and err != want_err:
        problems.append(diff(want_err, err, "stderr"))
    if want_err is None and err and case["exit"] != 2:
        problems.append("unexpected stderr:\n" + err)
    return problems


def update(case, out, err):
    """Rewrite the expected files of a case from the observed output."""
    if "contains" in case:
        return
    (EXPECTED / f"{case['name']}.out").write_bytes(out.encode("utf-8"))
    err_path = EXPECTED / f"{case['name']}.err"
    if err:
        err_path.write_bytes(err.encode("utf-8"))
    elif err_path.exists():
        err_path.unlink()


def show_output(paint, out, err):
    for text, label in ((out, "stdout"), (err, "stderr")):
        if text:
            print(paint(BOLD, f"  {label}:"))
            for line in text.rstrip("\n").split("\n"):
                print("    " + line)


def run_build(executable, cases, options, paint):
    """Run the cases against one executable; return (passed, failed, skipped)."""
    tool_version, schema = tool_info(executable)
    print(paint(BOLD, f"executable: {executable}"))
    print(f"tool version {tool_version or '?'}, schema component {'on' if schema else 'off'}\n")
    passed = failed = skipped = 0
    for case in cases:
        requires = case.get("requires")
        if (requires == "schema" and not schema) or (requires == "no-schema" and schema):
            skipped += 1
            print(paint(YELLOW, f"SKIPPED {case['name']}") + f" (needs {requires})")
            continue
        code, out, err = run(executable, case["args"])
        if options.update:
            update(case, out, err)
        problems = check(case, code, out, err, tool_version)
        if problems or not options.quiet:
            print(paint(BOLD, case["name"]) + f": {NAME} " + " ".join(case["args"]))
            show_output(paint, out, err)
            print(f"  exit code: {code}")
        if problems:
            failed += 1
            print(paint(RED, f"FAILED {case['name']}"))
            for problem in problems:
                print(paint(RED, "  " + problem.rstrip("\n").replace("\n", "\n  ")))
        else:
            passed += 1
            print(paint(GREEN, f"PASSED {case['name']}"))
        if problems or not options.quiet:
            print()
    summary = f"{passed} passed, {failed} failed, {skipped} skipped (schema {'on' if schema else 'off'})"
    print(paint(GREEN if failed == 0 else RED, paint(BOLD, summary)) + "\n")
    return passed, failed, skipped


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--exe", help="path to the built adaptive_json5_examples")
    parser.add_argument("--build", choices=PRESET_BUILDS, help="test only build/<preset>")
    parser.add_argument("--case", action="append", help="run only this case (repeatable)")
    parser.add_argument("--quiet", action="store_true", help="do not print the CLI output of passing cases")
    parser.add_argument("--update", action="store_true",
                        help="rewrite cli/test/expected from the observed output (review the diff afterwards)")
    options = parser.parse_args()

    paint = Painter(use_color(sys.stdout))
    executables = find_executables(options.exe, options.build)
    if not executables:
        print(paint(RED, "adaptive_json5_examples not found; build it first (cmake --preset cli && "
                         "cmake --build --preset cli --target adaptive_json5_examples, or the cli-no-schema "
                         "preset) or pass --exe"))
        return 2
    cases = json.loads(CASES.read_text(encoding="utf-8"))["cases"]
    if options.case:
        unknown = set(options.case) - {c["name"] for c in cases}
        if unknown:
            print(paint(RED, "unknown case: " + ", ".join(sorted(unknown))))
            return 2
        cases = [c for c in cases if c["name"] in options.case]
    totals = [run_build(executable, cases, options, paint) for executable in executables]
    failed = sum(t[1] for t in totals)
    if len(totals) > 1:
        overall = (f"all builds: {sum(t[0] for t in totals)} passed, {failed} failed, "
                   f"{sum(t[2] for t in totals)} skipped")
        print(paint(GREEN if failed == 0 else RED, paint(BOLD, overall)))
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
