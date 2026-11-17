#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Capture a clean Clang build and report CodeQL MISRA C++:2023 findings."""

import argparse
from collections import Counter
from html import escape
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import uuid
import webbrowser


ROOT = Path(__file__).resolve().parents[1]


def level_badge(level):
    color = level if level in ("error", "warning", "note", "none") else "none"
    return f"<span class='level {color}'>{escape(str(level))}</span>"


def run(*args):
    print("+ " + subprocess.list2cmdline(list(args)), flush=True)
    with subprocess.Popen(args, cwd=ROOT) as process:
        while True:
            try:
                code = process.wait(timeout=30)
                if code:
                    raise subprocess.CalledProcessError(code, args)
                break
            except subprocess.TimeoutExpired:
                print("Still running; waiting for the command to finish...", flush=True)


def write_html(data, destination):
    """Render escaped SARIF findings without third-party Python packages."""
    rows = []
    summary = {}
    for run_data in data.get("runs", []):
        tool = run_data.get("tool", {})
        components = [tool.get("driver", {}), *tool.get("extensions", [])]
        rules = {rule["id"]: rule for component in components
                 for rule in component.get("rules", [])}
        for result in run_data.get("results", []):
            rule_id = result.get("ruleId", "Unknown rule")
            rule = rules.get(rule_id, {})
            level = result.get("level", rule.get("defaultConfiguration", {}).get("level", "warning"))
            description = rule.get("shortDescription", {}).get("text", rule.get("name", ""))
            entry = summary.setdefault(rule_id, {"description": description, "levels": Counter()})
            entry["levels"][level] += 1
            message = result.get("message", {})
            locations = []
            for location in result.get("locations", []):
                physical = location.get("physicalLocation", {})
                uri = physical.get("artifactLocation", {}).get("uri", "Unknown file")
                line = physical.get("region", {}).get("startLine")
                locations.append(f"{uri}:{line}" if line else uri)
            cells = (level, rule_id, "; ".join(locations),
                     message.get("text", message.get("markdown", "")), description)
            rows.append("<tr><td>" + level_badge(level) + "</td>" +
                        "".join(f"<td>{escape(str(cell))}</td>" for cell in cells[1:]) + "</tr>")
    summary_rows = []
    severity_order = {"error": 0, "warning": 1, "note": 2, "none": 3}
    ordered_rules = sorted(
        summary.items(),
        key=lambda item: (
            min(severity_order.get(level, 4) for level in item[1]["levels"]),
            item[0],
        ),
    )
    for rule_id, entry in ordered_rules:
        levels = " &nbsp; ".join(level_badge(level) + f" ({count})"
                                  for level, count in sorted(
                                      entry["levels"].items(),
                                      key=lambda item: (severity_order.get(item[0], 4), item[0]),
                                  ))
        summary_rows.append(f"<tr><td>{escape(str(rule_id))}</td>"
                            f"<td>{escape(str(entry['description']))}</td><td>{levels}</td>"
                            f"<td>{sum(entry['levels'].values())}</td></tr>")
    destination.write_text(
        "<!doctype html><html lang='en'><meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>MISRA and security findings</title>"
        "<style>body{font:16px system-ui;margin:2rem;color:#182230}"
        "table{border-collapse:collapse;width:100%}td,th{padding:.7rem;"
        "border:1px solid #ccd;text-align:left;vertical-align:top}"
        "th{background:#eef2f8}td{white-space:pre-wrap;overflow-wrap:anywhere}"
        "tr:nth-child(even){background:#f8fafc}"
        ".level{display:inline-block;border-radius:.3rem;padding:.15rem .5rem;font-weight:600}"
        ".error{background:#fee2e2;color:#991b1b}.warning{background:#fef3c7;color:#78350f}"
        ".note{background:#dbeafe;color:#1e40af}.none{background:#e5e7eb;color:#374151}</style>"
        f"<h1>MISRA and security findings</h1><p>{len(rows)} findings. "
        "Completed analysis does not establish MISRA compliance.</p>"
        f"<h2>Rules with findings ({len(summary)})</h2>"
        "<p>Counts cover the complete report; filters below apply to individual findings.</p>"
        "<table><thead><tr><th>Rule</th><th>Description</th><th>Levels</th>"
        "<th>Findings</th></tr></thead><tbody>" + "".join(summary_rows) +
        "</tbody></table><h2>Individual findings</h2>"
        "<p>The accompanying SARIF file preserves full analysis details.</p>"
        "<p><label>Level <select id='level'><option value=''>All levels</option>"
        "</select></label> &nbsp; <label>Rule <select id='rule'>"
        "<option value=''>All rules</option></select></label> "
        "&nbsp; <label>File <input id='file' type='search' "
        "placeholder='Part of a file path'></label> "
        "<button id='reset' type='button'>Reset filters</button></p>"
        "<p id='count' role='status' aria-live='polite'></p>"
        "<table id='findings'><thead><tr><th>Level</th><th>Rule ID</th>"
        "<th>Location</th><th>Message</th><th>Rule description</th>"
        "</tr></thead><tbody>" + "".join(rows) +
        "</tbody></table>"
        "<script>"
        "const rows=Array.from(document.querySelectorAll('#findings tbody tr'));"
        "const level=document.getElementById('level');"
        "const rule=document.getElementById('rule');"
        "const file=document.getElementById('file');"
        "const count=document.getElementById('count');"
        "Array.from(new Set(rows.map(r=>r.cells[0].textContent))).sort().forEach(v=>{"
        "const option=document.createElement('option');option.value=v;option.textContent=v;"
        "level.appendChild(option);});"
        "Array.from(new Set(rows.map(r=>r.cells[1].textContent))).sort().forEach(v=>{"
        "const option=document.createElement('option');option.value=v;option.textContent=v;"
        "rule.appendChild(option);});"
        "function filter(){let visible=0;const query=file.value.trim().toLowerCase();"
        "rows.forEach(r=>{const match=(!level.value||r.cells[0].textContent===level.value)"
        "&&(!rule.value||r.cells[1].textContent===rule.value)"
        "&&r.cells[2].textContent.toLowerCase().includes(query);"
        "r.hidden=!match;if(match)visible++;});"
        "count.textContent=visible+' of '+rows.length+' findings shown';}"
        "level.addEventListener('change',filter);rule.addEventListener('change',filter);"
        "file.addEventListener('input',filter);"
        "document.getElementById('reset').addEventListener('click',()=>{"
        "level.value='';rule.value='';file.value='';filter();});filter();"
        "</script></html>", encoding="utf-8",
    )


def open_report(report, open_browser=True):
    data = json.loads(report.read_text(encoding="utf-8-sig"))
    html = report.with_suffix(".html")
    write_html(data, html)
    print(f"HTML report: {html}")
    if open_browser and not webbrowser.open(html.resolve().as_uri()):
        print("Could not open a browser automatically; open the HTML report manually.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--report", help="Convert a SARIF file, or 'latest', without rerunning analysis")
    parser.add_argument("--no-open", action="store_true", help="Generate HTML without opening a browser")
    args = parser.parse_args()
    if args.report:
        if args.report == "latest":
            reports = list((ROOT / "build" / "misra").glob("*/results.sarif"))
            if not reports:
                parser.error("No completed SARIF report found under build/misra. Run analysis first.")
            report = max(reports, key=lambda path: path.stat().st_mtime)
        else:
            report = Path(args.report).resolve()
        try:
            open_report(report, open_browser=not args.no_open)
        except (OSError, ValueError) as error:
            parser.error(f"Cannot open report: {error}. Use --report latest or an existing SARIF path.")
        return 0
    for tool in ("cmake", "ninja", "codeql"):
        if not shutil.which(tool):
            print(f"Missing {tool} on PATH; see README setup instructions.", file=sys.stderr)
            return 2

    cores = str(os.cpu_count() or 1)
    # Keep earlier databases/results intact and avoid stale incremental captures.
    work = ROOT / "build" / "misra" / uuid.uuid4().hex[:12]
    work.mkdir(parents=True)
    build = (work / "build").relative_to(ROOT).as_posix()
    database = (work / "database").relative_to(ROOT).as_posix()
    report = (work / "results.sarif").relative_to(ROOT).as_posix()
    run("codeql", "version")
    run("cmake", "--preset", "clang", "-B", build,
        "-DADAPTIVE_JSON5_BUILD_LIBRARY=ON",
        "-DADAPTIVE_JSON5_BUILD_EXECUTABLE=OFF",
        "-DADAPTIVE_JSON5_BUILD_TESTS=OFF",
        "-DADAPTIVE_JSON5_BUILD_VALIDATION_TESTS=OFF")
    run("codeql", "database", "create", database, "--language=c-cpp",
        "--source-root=.", "--threads=0",
        f"--command=cmake --build {build} --clean-first --parallel {cores}")
    print("Downloading query packs and running analysis; HTML opens after completion.", flush=True)
    print(f"Analysis logs: {ROOT / database / 'log'}", flush=True)
    run("codeql", "database", "analyze", database,
        "codeql/misra-cpp-coding-standards", "codeql/cpp-queries",
        "--download", "--verbose", "--threads=0", "--format=sarifv2.1.0", f"--output={report}")
    data = json.loads((ROOT / report).read_text(encoding="utf-8-sig"))
    findings = sum(len(item.get("results", [])) for item in data.get("runs", []))
    print(f"Analysis complete: {findings} findings. Report: {ROOT / report}")
    open_report(ROOT / report, open_browser=not args.no_open)
    print("Review findings; successful execution is not MISRA compliance.")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except subprocess.CalledProcessError as error:
        sys.exit(error.returncode)
    except KeyboardInterrupt:
        sys.exit(130)
