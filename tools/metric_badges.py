#!/usr/bin/env python3
"""Generate measured README badges from gcovr summaries or CodeQL SARIF."""
# SPDX-License-Identifier: Apache-2.0
import argparse
from html import escape
import json
import math
from pathlib import Path
import re


COVERAGE = (("line", "lines"), ("function", "functions"), ("branch", "branches"))
SEVERITIES = ("critical", "high", "medium", "low", "unrated")


def coverage_values(data):
    values = {}
    for key, label in COVERAGE:
        value = data.get(key + "_percent")
        if value is None:
            values[label] = "n/a"
        else:
            value = float(value)
            if not math.isfinite(value) or not 0 <= value <= 100:
                raise ValueError("Invalid coverage percentage: " + key)
            values[label] = f"{value:.1f}%"
    return values


def coverage_message(data):
    return " | ".join(f"{label} {value}" for label, value in coverage_values(data).items())


def result_rule(run, result):
    """Resolve driver/extension descriptors without mixing their rule indices."""
    tool = run.get("tool", {})
    driver = tool.get("driver", {})
    extensions = tool.get("extensions", [])
    reference = result.get("rule", {})
    component_reference = reference.get("toolComponent")
    components = [driver, *extensions]
    if component_reference is not None:
        if "index" in component_reference:
            index = component_reference["index"]
            components = [extensions[index]] if isinstance(index, int) and 0 <= index < len(extensions) else []
        else:
            components = [component for component in components if all(
                component.get(key) == component_reference[key]
                for key in ("name", "guid") if key in component_reference)]
    index = reference.get("index", result.get("ruleIndex"))
    identifier = reference.get("id", result.get("ruleId"))
    if component_reference is not None and len(components) != 1:
        return {}
    if index is not None:
        # An unqualified index refers to the driver, not an extension.
        component = components[0] if component_reference is not None else driver
        rules = component.get("rules", [])
        if isinstance(index, int) and 0 <= index < len(rules):
            rule = rules[index]
            if identifier is None or rule.get("id") == identifier:
                return rule
        return {}
    matches = [rule for component in components for rule in component.get("rules", [])
               if identifier is not None and rule.get("id") == identifier]
    return matches[0] if len(matches) == 1 else {}


def misra_counts(documents):
    counts = dict.fromkeys(SEVERITIES, 0)
    for data in documents:
        for run in data["runs"]:
            for result in run.get("results", []):
                if result.get("suppressions") or result.get("baselineState") == "absent":
                    continue
                rule = result_rule(run, result)
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
                elif score >= 4:
                    counts["medium"] += 1
                elif score > 0:
                    counts["low"] += 1
                else:
                    # GitHub treats 0.0 as having no security severity.
                    # https://docs.github.com/en/code-security/reference/code-scanning/sarif-files/sarif-support
                    counts["unrated"] += 1
    return counts


def misra_message(documents):
    return " | ".join(f"{severity} {count}" for severity, count in misra_counts(documents).items())


def version_message(text):
    """Latest published release tag, or 'unreleased' when there is none."""
    tag = text.strip()
    if not tag:
        return "unreleased"
    if not re.fullmatch(r"v?\d+\.\d+\.\d+(?:[-+][0-9A-Za-z.+-]+)?", tag):
        raise ValueError("Unexpected release tag: " + tag)
    return tag


def svg(label, message, color="#17669b"):
    left = len(label) * 7 + 16
    right = len(message) * 7 + 16
    title = escape(label + ": " + message)
    return f'''<svg xmlns="http://www.w3.org/2000/svg" width="{left+right}" height="24" role="img" aria-label="{title}"><title>{title}</title><rect width="{left}" height="24" fill="#444"/><rect x="{left}" width="{right}" height="24" fill="{color}"/><g fill="white" text-anchor="middle" font-family="DejaVu Sans,Verdana,sans-serif" font-size="11"><text x="{left/2}" y="16">{escape(label)}</text><text x="{left+right/2}" y="16">{escape(message)}</text></g></svg>'''


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("kind", choices=("coverage", "misra", "version"))
    parser.add_argument("sources", nargs="+", type=Path,
                        help="gcovr summary, SARIF files, or a text file holding the latest release tag")
    parser.add_argument("--output", type=Path, help="Combined badge (retained for existing consumers)")
    parser.add_argument("--output-dir", type=Path, help="Directory for separate, short metric badges")
    args = parser.parse_args(argv)
    if args.output is None and args.output_dir is None:
        parser.error("at least one of --output or --output-dir is required")
    if args.kind == "version":
        label, message = "version", version_message(args.sources[0].read_text(encoding="utf-8"))
        badges = {"version.svg": (label, message)}
    else:
        documents = [json.loads(path.read_text(encoding="utf-8")) for path in args.sources]
        if args.kind == "coverage":
            values = coverage_values(documents[0])
            message = " | ".join(f"{key} {value}" for key, value in values.items())
            badges = {f"coverage-{key}.svg": (key, value) for key, value in values.items()}
        else:
            counts = misra_counts(documents)
            message = " | ".join(f"{key} {value}" for key, value in counts.items())
            badges = {f"misra-{key}.svg": (key, str(value)) for key, value in counts.items()}
        label = "unit coverage" if args.kind == "coverage" else "MISRA / CodeQL"
    if args.output is not None:
        args.output.write_text(svg(label, message), encoding="utf-8")
    if args.output_dir is not None:
        args.output_dir.mkdir(parents=True, exist_ok=True)
        for filename, (badge_label, badge_message) in badges.items():
            (args.output_dir / filename).write_text(svg(badge_label, badge_message), encoding="utf-8")
    print(message)


if __name__ == "__main__":
    main()
