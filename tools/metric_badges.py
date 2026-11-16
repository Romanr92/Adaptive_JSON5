#!/usr/bin/env python3
"""Generate measured README badges from gcovr summaries or CodeQL SARIF."""
# SPDX-License-Identifier: Apache-2.0
import argparse
from html import escape
import json
import math
from pathlib import Path


def coverage_message(data):
    parts = []
    for key, label in (("line", "lines"), ("function", "functions"), ("branch", "branches")):
        value = data.get(key + "_percent")
        if value is None:
            parts.append(label + " n/a")
        else:
            value = float(value)
            if not math.isfinite(value) or not 0 <= value <= 100:
                raise ValueError("Invalid coverage percentage: " + key)
            parts.append(f"{label} {value:.1f}%")
    return " | ".join(parts)


def misra_message(documents):
    counts = {"critical": 0, "high": 0, "unrated": 0}
    for data in documents:
        for run in data["runs"]:
            components = [run.get("tool", {}).get("driver", {}), *run.get("tool", {}).get("extensions", [])]
            rules = {rule["id"]: rule for component in components for rule in component.get("rules", [])}
            for result in run.get("results", []):
                if result.get("suppressions") or result.get("baselineState") == "absent":
                    continue
                rule = rules.get(result.get("ruleId"), {})
                raw = rule.get("properties", {}).get("security-severity")
                if raw is None:
                    counts["unrated"] += 1
                    continue
                score = float(raw)
                if not math.isfinite(score) or not 0 <= score <= 10:
                    raise ValueError("Invalid security-severity")
                if score >= 9:
                    counts["critical"] += 1
                elif score >= 7:
                    counts["high"] += 1
    return f"critical {counts['critical']} | high {counts['high']} | unrated {counts['unrated']}"


def svg(label, message):
    left = len(label) * 7 + 16
    right = len(message) * 7 + 16
    title = escape(label + ": " + message)
    return f'''<svg xmlns="http://www.w3.org/2000/svg" width="{left+right}" height="24" role="img" aria-label="{title}"><title>{title}</title><rect width="{left}" height="24" fill="#444"/><rect x="{left}" width="{right}" height="24" fill="#17669b"/><g fill="white" text-anchor="middle" font-family="DejaVu Sans,Verdana,sans-serif" font-size="11"><text x="{left/2}" y="16">{escape(label)}</text><text x="{left+right/2}" y="16">{escape(message)}</text></g></svg>'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("kind", choices=("coverage", "misra"))
    parser.add_argument("sources", nargs="+", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    documents = [json.loads(path.read_text(encoding="utf-8")) for path in args.sources]
    message = coverage_message(documents[0]) if args.kind == "coverage" else misra_message(documents)
    label = "unit coverage" if args.kind == "coverage" else "MISRA / CodeQL"
    args.output.write_text(svg(label, message), encoding="utf-8")
    print(message)


if __name__ == "__main__":
    main()
