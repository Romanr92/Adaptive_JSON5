#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Report production functions above the complexity limits of the coding-style guide.

Measures every function in core/src, core/includes and includes with lizard and lists
those whose cyclomatic complexity or code-line count (NLOC, without comments or blank
lines) is above the limits in core/docs/design-coding-style.md, "Function size and
complexity". Functions listed in that section's exception table are reported
separately. The report is informative: the exit code is 0 unless --strict is given.
"""

import argparse
from pathlib import Path
import re
import sys

try:
    import lizard
except ImportError:
    sys.exit("lizard is required: python -m pip install --require-hashes -r tools/complexity-requirements.txt")


ROOT = Path(__file__).resolve().parents[1]
GUIDE = ROOT / "core/docs/design-coding-style.md"
SOURCE_DIRECTORIES = ("core/src", "core/includes", "includes")
SUFFIXES = {".cpp", ".hpp"}
# Keep in step with the limits table in the guide.
MAX_CCN = 15
MAX_NLOC = 60


def accepted_exceptions():
    """Return function names from the guide's exception table."""
    text = GUIDE.read_text(encoding="utf-8")
    section = text.split("## Function size and complexity", 1)[1].split("\n## ", 1)[0]
    names = set()
    for line in section.splitlines():
        match = re.match(r"\|\s*`([^`]+)`\s*\|", line)
        if match:
            names.add(match[1])
    return names


def measure():
    """Yield (path, function) for every production function."""
    for directory in SOURCE_DIRECTORIES:
        for path in sorted((ROOT / directory).rglob("*")):
            if path.suffix in SUFFIXES and path.is_file():
                info = lizard.analyze_file(str(path))
                for function in info.function_list:
                    yield path.relative_to(ROOT).as_posix(), function


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--summary", type=Path, help="Also write the Markdown report to this file")
    parser.add_argument("--strict", action="store_true", help="Exit 1 when an unaccepted function is over a limit")
    arguments = parser.parse_args()

    exceptions = accepted_exceptions()
    over, accepted, total = [], [], 0
    for path, function in measure():
        total += 1
        if function.cyclomatic_complexity > MAX_CCN or function.nloc > MAX_NLOC:
            row = (function.cyclomatic_complexity, function.nloc, function.name, path, function.start_line)
            (accepted if function.name in exceptions else over).append(row)
    over.sort(key=lambda row: (-row[0], -row[1], row[2]))
    accepted.sort(key=lambda row: (-row[0], -row[1], row[2]))

    lines = [
        "## Function complexity",
        "",
        f"Limits: cyclomatic complexity at most {MAX_CCN}, at most {MAX_NLOC} code lines "
        f"(lizard {lizard.version}). {total} production functions measured; "
        f"{len(over)} over a limit, {len(accepted)} accepted exceptions.",
        "",
    ]
    for title, rows in (("Over a limit", over), ("Accepted exceptions", accepted)):
        if not rows:
            continue
        lines += [f"### {title}", "", "| CCN | NLOC | Function | Location |", "| ---: | ---: | --- | --- |"]
        for ccn, nloc, name, path, line in rows:
            ccn_text = f"**{ccn}**" if ccn > MAX_CCN else str(ccn)
            nloc_text = f"**{nloc}**" if nloc > MAX_NLOC else str(nloc)
            lines.append(f"| {ccn_text} | {nloc_text} | `{name}` | `{path}:{line}` |")
        lines.append("")
    report = "\n".join(lines)

    print(report)
    if arguments.summary:
        with arguments.summary.open("a", encoding="utf-8") as summary:
            summary.write(report + "\n")
    return 1 if arguments.strict and over else 0


if __name__ == "__main__":
    raise SystemExit(main())
