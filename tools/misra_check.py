#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Capture a clean Clang build and report CodeQL MISRA C++:2023 findings."""

import argparse
from collections import Counter
from html import escape
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys
import uuid
import webbrowser
import xml.etree.ElementTree as ET

from metric_badges import result_rule

ROOT = Path(__file__).resolve().parents[1]
DEVIATION_MARKER = re.compile(r"\s*// codeql::misra_deviation_next_line\((DEV-\d+\.\d+)\)\s*")
SOURCE_RULE = re.compile(r"\s*// MISRA rule: ((?:RULE|DIR)-\d+(?:-\d+){1,2}) (cpp/misra/[\w-]+)\s*")
MISRA_RULE_TAG = "external/misra/id/"
JUSTIFICATION = "// MISRA justification: "
SOURCE_SUFFIXES = {".cpp", ".hpp", ".h", ".c", ".cc", ".cxx", ".hh", ".hxx", ".ipp", ".inl"}


def source_deviations():
    """Read source exceptions keyed by source file, code line, and query."""
    deviations = {}
    seen = set()
    for directory in ("core/src", "core/includes", "includes"):
        for path in sorted((ROOT / directory).rglob("*")):
            if path.suffix not in SOURCE_SUFFIXES or not path.is_file():
                continue
            lines = path.read_text(encoding="utf-8").splitlines()
            filename = path.relative_to(ROOT).as_posix()
            for index, line in enumerate(lines):
                match = DEVIATION_MARKER.fullmatch(line)
                if not match:
                    continue
                entry = match[1]
                if entry in seen:
                    raise ValueError(f"{filename}:{index + 1}: duplicate MISRA marker {entry}")

                start = index - 1
                while start >= 0 and lines[start].lstrip().startswith("// "):
                    if lines[start].lstrip().startswith(JUSTIFICATION):
                        break
                    start -= 1
                if start < 0 or not lines[start].lstrip().startswith(JUSTIFICATION):
                    raise ValueError(f"{filename}:{index + 1}: {entry} needs a MISRA justification comment")
                first = lines[start].lstrip()[len(JUSTIFICATION):].strip()
                following = lines[index + 1].strip() if index + 1 < len(lines) else ""
                if not first or not following or following.startswith(("//", "/*")):
                    raise ValueError(f"{filename}:{index + 1}: {entry} needs a justification and a following code line")
                justification = " ".join([first, *(part.lstrip()[3:] for part in lines[start + 1:index])])
                offset = start - 1
                if offset < 0 or not SOURCE_RULE.fullmatch(lines[offset]):
                    raise ValueError(f"{filename}:{index + 1}: {entry} needs a MISRA rule and query")
                while offset >= 0 and (rule_match := SOURCE_RULE.fullmatch(lines[offset])):
                    rule, query = rule_match.groups()
                    key = (filename, index + 2, query)
                    if key in deviations:
                        raise ValueError(f"{filename}:{offset + 1}: duplicate MISRA query {query}")
                    deviations[key] = (rule, entry, justification)
                    offset -= 1
                seen.add(entry)
    return deviations


def index_deviations():
    """Index the XML configuration during CodeQL database creation."""
    dist = os.environ["CODEQL_DIST"]
    database = os.environ["CODEQL_EXTRACTOR_CPP_WIP_DATABASE"]
    executable = Path(dist) / ("codeql.exe" if os.name == "nt" else "codeql")
    config = ET.Element("codingstandards")
    # Keep candidates in the raw SARIF, including branches that skip the
    # query-level deviation check, such as the explicit-destructor result.
    ET.SubElement(config, "report-deviated-alerts").text = "true"
    records = ET.SubElement(config, "deviations")
    for (_, _, query), (rule, entry, justification) in source_deviations().items():
        record = ET.SubElement(records, "deviations-entry")
        for key, value in (
            ("rule-id", rule),
            ("query-id", query),
            ("code-identifier", entry),
            ("justification", justification),
        ):
            ET.SubElement(record, key).text = value
    path = ROOT / "coding-standards.xml"
    output = path.open("xb")
    try:
        with output:
            ET.ElementTree(config).write(output, encoding="utf-8", xml_declaration=True)
        run(str(executable), "database", "index-files", "--include=**/coding-standards.xml",
            f"--working-dir={ROOT}", "--language=xml", database)
    finally:
        path.unlink()


def accepted_justifications(result):
    return [item["justification"] for item in result.get("suppressions", [])
            if item.get("status") == "accepted" and item.get("justification")]


def misra_rule_ids(rule):
    return sorted(tag.removeprefix(MISRA_RULE_TAG).upper()
                  for tag in rule.get("properties", {}).get("tags", [])
                  if tag.startswith(MISRA_RULE_TAG))


def apply_deviations(report, upload=None):
    """Annotate source-justified results and write an open-only upload."""
    deviations = source_deviations()
    data = json.loads(report.read_text(encoding="utf-8-sig"))
    open_runs = []
    for run_data in data.get("runs", []):
        open_results = []
        for result in run_data.get("results", []):
            locations = result.get("locations", [])
            if len(locations) != 1:
                open_results.append(result)
                continue
            physical = locations[0].get("physicalLocation", {})
            path = physical.get("artifactLocation", {}).get("uri", "").removeprefix("./")
            line = physical.get("region", {}).get("startLine")
            deviation = deviations.get((path, line, result.get("ruleId")))
            if not deviation or deviation[0] not in misra_rule_ids(result_rule(run_data, result)):
                open_results.append(result)
                continue
            _, entry, reason = deviation
            justification = f"{entry}: {reason}"
            if justification not in accepted_justifications(result):
                result.setdefault("suppressions", []).append(
                    {"kind": "inSource", "status": "accepted", "justification": justification}
                )
        open_runs.append({**run_data, "results": open_results} if "results" in run_data else run_data)
    report.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    if upload:
        upload.parent.mkdir(parents=True, exist_ok=True)
        upload.write_text(json.dumps({**data, "runs": open_runs}, ensure_ascii=False, indent=2) + "\n",
                          encoding="utf-8")
    return data


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


def html_start(title):
    return (
        "<!doctype html><html lang='en'><meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        f"<title>{escape(title)}</title>"
        "<style>body{font:16px system-ui;margin:2rem;color:#182230}"
        "table{border-collapse:collapse;width:100%}td,th{padding:.7rem;"
        "border:1px solid #ccd;text-align:left;vertical-align:top}"
        "th{background:#eef2f8}td{white-space:pre-wrap;overflow-wrap:anywhere}"
        "tr:nth-child(even){background:#f8fafc}a{color:#145787}"
        ".level{display:inline-block;border-radius:.3rem;padding:.15rem .5rem;font-weight:600}"
        ".error{background:#fee2e2;color:#991b1b}.warning{background:#fef3c7;color:#78350f}"
        ".note{background:#dbeafe;color:#1e40af}.none{background:#e5e7eb;color:#374151}"
        ".justified{margin-top:.25rem;background:#dcfce7;color:#166534}</style>"
        f"<h1>{escape(title)}</h1>"
    )


def write_html(data, destination):
    """Render escaped SARIF findings without third-party Python packages."""
    rows = []
    justified_rows = []
    summary = {}
    open_levels = Counter()
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
            message = result.get("message", {})
            locations = []
            for location in result.get("locations", []):
                physical = location.get("physicalLocation", {})
                uri = physical.get("artifactLocation", {}).get("uri", "Unknown file")
                line = physical.get("region", {}).get("startLine")
                locations.append(f"{uri}:{line}" if line else uri)
            specific_rules = misra_rule_ids(result_rule(run_data, result) or rule)
            if justifications := accepted_justifications(result):
                cells = (", ".join(specific_rules) or "—", rule_id,
                         "; ".join(locations), "; ".join(justifications))
                justified_rows.append("<tr><td>" + level_badge(level) +
                                      "<br><span class='level justified'>Justified</span></td>" +
                                      "".join(f"<td>{escape(str(cell))}</td>" for cell in cells) + "</tr>")
                continue
            entry = summary.setdefault(rule_id, {"description": description, "levels": Counter(),
                                                 "misra_rules": set()})
            entry["levels"][level] += 1
            entry["misra_rules"].update(specific_rules)
            open_levels[level] += 1
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
    severity_totals = " &nbsp; ".join(level_badge(level) + f" ({open_levels[level]})"
                                     for level in ("error", "warning", "note"))
    rule_options = []
    for query_id, entry in sorted(summary.items()):
        numbers = ", ".join(sorted(entry["misra_rules"]))
        label = f"{numbers} — {query_id}" if numbers else query_id
        rule_options.append(f"<option value='{escape(query_id)}'>{escape(label)}</option>")
    justified = destination.with_name(f"{destination.stem}-justified.html")
    destination.write_text(
        html_start("MISRA and security findings") +
        f"<nav><a href='{escape(justified.name)}'>Justified findings ({len(justified_rows)})</a></nav>"
        f"<p>{len(rows)} open findings. "
        "Completed analysis does not establish MISRA compliance.</p>"
        f"<h2>Rules with findings ({len(summary)})</h2>"
        "<p>Counts cover open findings. Filters below apply to individual findings.</p>"
        "<table><thead><tr><th>Rule</th><th>Description</th><th>Levels</th>"
        "<th>Findings</th></tr></thead><tbody>" + "".join(summary_rows) +
        "</tbody></table><h2>Individual findings</h2>"
        f"<p>Open findings by severity: {severity_totals}</p>"
        "<p>The accompanying SARIF file preserves full analysis details.</p>"
        "<p><label>Level <select id='level'><option value=''>All levels</option>"
        "</select></label> &nbsp; <label>Rule <select id='rule'>"
        "<option value=''>All rules</option>" + "".join(rule_options) + "</select></label> "
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
    justified.write_text(
        html_start("Justified findings") +
        f"<nav><a href='{escape(destination.name)}'>Open findings ({len(rows)})</a></nav>"
        f"<p>{len(justified_rows)} justified findings.</p>"
        "<table><thead><tr><th>Severity</th><th>MISRA rule</th><th>CodeQL query ID</th>"
        "<th>Location</th><th>Justification</th></tr></thead><tbody>"
        + "".join(justified_rows) + "</tbody></table></html>",
        encoding="utf-8",
    )
    return justified


def open_report(report, open_browser=True):
    data = json.loads(report.read_text(encoding="utf-8-sig"))
    html = report.with_suffix(".html")
    justified = write_html(data, html)
    print(f"HTML report: {html}")
    print(f"Justified findings: {justified}")
    if open_browser and not webbrowser.open(html.resolve().as_uri()):
        print("Could not open a browser automatically; open the HTML report manually.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--report", help="Convert a SARIF file, or 'latest', without rerunning analysis")
    mode.add_argument("--index-deviations", action="store_true",
                      help="Index source justifications inside an active CodeQL database build")
    mode.add_argument("--prepare-sarif", type=Path,
                      help="Annotate source deviations in a new SARIF and generate the HTML report")
    parser.add_argument("--upload-sarif", type=Path,
                        help="With --prepare-sarif, write a separate SARIF containing open results")
    parser.add_argument("--no-open", action="store_true", help="Generate HTML without opening a browser")
    args = parser.parse_args()
    if args.upload_sarif and not args.prepare_sarif:
        parser.error("--upload-sarif requires --prepare-sarif")
    if args.prepare_sarif:
        if args.upload_sarif and args.upload_sarif.resolve() == args.prepare_sarif.resolve():
            parser.error("The upload path must differ from the audit SARIF path.")
        try:
            apply_deviations(args.prepare_sarif, args.upload_sarif)
            open_report(args.prepare_sarif, open_browser=not args.no_open)
        except (OSError, ValueError) as error:
            parser.error(str(error))
        return 0
    if args.index_deviations:
        try:
            index_deviations()
        except KeyError as error:
            parser.error(f"Run --index-deviations during CodeQL database creation (missing {error}).")
        except (OSError, ValueError) as error:
            parser.error(str(error))
        return 0
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
    index_command = [sys.executable, "tools/misra_check.py", "--index-deviations"]
    quote_command = subprocess.list2cmdline if os.name == "nt" else shlex.join
    run("codeql", "database", "create", database, "--language=c-cpp",
        "--source-root=.", "--threads=0",
        f"--command={quote_command(index_command)}",
        f"--command=cmake --build {build} --clean-first --parallel {cores}")
    print("Downloading query packs and running analysis; HTML opens after completion.", flush=True)
    print(f"Analysis logs: {ROOT / database / 'log'}", flush=True)
    run("codeql", "database", "analyze", database,
        "codeql/misra-cpp-coding-standards", "codeql/cpp-queries",
        "--download", "--verbose", "--threads=0", "--format=sarifv2.1.0", f"--output={report}")
    data = apply_deviations(ROOT / report)
    findings = sum(not accepted_justifications(result) for item in data.get("runs", [])
                   for result in item.get("results", []))
    print(f"Analysis complete: {findings} open findings. Report: {ROOT / report}")
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
