#!/usr/bin/env python3
"""Check the requirements registry and report requirement -> code -> tests -> result.

Reads docs/requirements-registry.json, checks every link against the working tree and,
when JUnit XML is given for the required configurations, against executed test results.
Writes traceability.json, traceability.html and three badges. Standard library only.

Exit codes: 0 registry consistent; 1 registry problems or stale links; 2 unusable input.
Test failures do not change the exit code: the test jobs own that.
"""
# SPDX-License-Identifier: Apache-2.0
import argparse
from html import escape
import json
import math
from pathlib import Path
import re
import sys
import xml.etree.ElementTree as ElementTree

sys.path.insert(0, str(Path(__file__).resolve().parent))
from metric_badges import svg  # noqa: E402

SCHEMA = 1
STATUSES = ("Draft", "Implemented", "Verified", "Deferred", "Obsolete")
SECTIONS = ("core", "api")
ID_PATTERN = re.compile(r"J5-\d{3}")
TEST_PATTERN = re.compile(r"(?:([A-Za-z0-9_]+)/)?([A-Za-z0-9_]+)\.([A-Za-z0-9_]+)")
DEFINITION_PATTERN = re.compile(r"\| *(J5-\d{3}) — ([^|]+?) *\|")
BADGES = (
    ("implementation", "traceability-implementation.svg", "requirements mapped to code"),
    ("tests", "traceability-tests.svg", "requirements mapped to tests"),
    ("passing", "traceability-passing.svg", "requirements with passing tests"),
)
UNKNOWN_COLOR = "#777"
TABLE_HEADER = ("| ID | Contract | Status | Owning issue | Source files and functions | Unit tests |\n"
                "| --- | --- | --- | --- | --- | --- |\n")


class InputError(Exception):
    """The registry or an input file cannot be used at all."""


def load_registry(path):
    try:
        data = json.loads(Path(path).read_text(encoding="utf-8"))
    except (OSError, ValueError) as error:
        raise InputError(f"Cannot read registry {path}: {error}") from error
    if not isinstance(data, dict) or data.get("schema") != SCHEMA:
        raise InputError(f"Registry {path} must be an object with \"schema\": {SCHEMA}")
    if not isinstance(data.get("requirements"), list):
        raise InputError("Registry \"requirements\" must be a list")
    return data


def is_strings(value):
    return isinstance(value, list) and all(isinstance(item, str) and item for item in value)


def safe_path(root, relative):
    """Return the file below root, or None for a missing file or a path leaving the tree."""
    if not relative or "\\" in relative or relative.startswith("/") or ".." in relative.split("/"):
        return None
    path = Path(root, relative)
    return path if path.is_file() else None


def structure_errors(registry):
    """Problems that make the whole registry untrustworthy; metrics are then unknown."""
    errors = []
    for key in ("requirement_sources", "required_configurations"):
        if not is_strings(registry.get(key)) or not registry[key]:
            errors.append(f"\"{key}\" must be a non-empty list of strings")
    seen = set()
    for index, entry in enumerate(registry["requirements"]):
        where = f"requirements[{index}]"
        if not isinstance(entry, dict) or not isinstance(entry.get("id"), str):
            errors.append(f"{where}: must be an object with a string \"id\"")
            continue
        where = entry["id"]
        if not ID_PATTERN.fullmatch(entry["id"]):
            errors.append(f"{where}: ID must look like J5-123")
        if entry["id"] in seen:
            errors.append(f"{where}: duplicate ID")
        seen.add(entry["id"])
        if not isinstance(entry.get("title"), str) or not entry["title"]:
            errors.append(f"{where}: \"title\" must be a non-empty string")
        if entry.get("status") not in STATUSES:
            errors.append(f"{where}: \"status\" must be one of {', '.join(STATUSES)}")
        if entry.get("section") not in SECTIONS:
            errors.append(f"{where}: \"section\" must be one of {', '.join(SECTIONS)}")
        if not isinstance(entry.get("issue"), int) or isinstance(entry.get("issue"), bool):
            errors.append(f"{where}: \"issue\" must be an issue number")
        applicable = entry.get("applicable", True)
        if not isinstance(applicable, bool):
            errors.append(f"{where}: \"applicable\" must be true or false")
        elif not applicable and not (isinstance(entry.get("reason"), str) and entry["reason"].strip()):
            errors.append(f"{where}: \"applicable\": false needs a written \"reason\"")
        if entry.get("status") == "Obsolete" and applicable is True:
            errors.append(f"{where}: an Obsolete requirement must set \"applicable\": false with a reason")
        implementation = entry.get("implementation")
        if not isinstance(implementation, list):
            errors.append(f"{where}: \"implementation\" must be a list")
        else:
            for item in implementation:
                if not (isinstance(item, dict) and is_strings(item.get("files")) and item["files"]
                        and is_strings(item.get("symbols", []))
                        and isinstance(item.get("note", ""), str)):
                    errors.append(f"{where}: each implementation entry needs \"files\" and may have "
                                  "\"symbols\" and a \"note\"")
        tests = entry.get("tests")
        if not isinstance(tests, list):
            errors.append(f"{where}: \"tests\" must be a list")
        else:
            for item in tests:
                if not (isinstance(item, dict) and isinstance(item.get("file"), str)
                        and is_strings(item.get("names")) and item["names"]):
                    errors.append(f"{where}: each tests entry needs a \"file\" and \"names\"")
    return errors


def defined_requirements(root, sources):
    """IDs and titles from the '| J5-123 — Title |' rows of the requirement documents."""
    defined, errors = {}, []
    for source in sources:
        path = safe_path(root, source)
        if path is None:
            errors.append(f"Requirement source {source} does not exist")
            continue
        for line in path.read_text(encoding="utf-8").splitlines():
            match = DEFINITION_PATTERN.match(line)
            if match:
                if match.group(1) in defined:
                    errors.append(f"{match.group(1)}: defined more than once in the requirement documents")
                defined[match.group(1)] = match.group(2)
    return defined, errors


def junit_results(path):
    """Map 'Suite.Test' (with its instantiation prefix, if any) to the outcomes of its cases."""
    try:
        root = ElementTree.parse(path).getroot()
    except (OSError, ElementTree.ParseError) as error:
        raise InputError(f"{error}") from error
    results = {}
    for case in root.iter("testcase"):
        name = case.get("name", "")
        if "." not in name and case.get("classname"):
            name = case.get("classname") + "." + name  # GoogleTest's own XML
        if "." not in name:
            continue
        if case.find("failure") is not None or case.find("error") is not None or case.get("status") == "fail":
            outcome = "failed"
        elif (case.find("skipped") is not None or case.get("status") in ("notrun", "disabled")
              or case.get("result") in ("skipped", "suppressed")):
            outcome = "skipped"
        else:
            outcome = "passed"
        suite, test = name.split(".", 1)
        test = test.split("/", 1)[0]  # value-parameterized case suffix
        suite = re.sub(r"/\d+$", "", suite)  # typed-test type index
        keys = {suite + "." + test}
        if "/" in suite:  # a test named without its instantiation covers all instantiations
            keys.add(suite.split("/", 1)[1] + "." + test)
        for key in keys:
            results.setdefault(key, []).append(outcome)
    if not results:
        raise InputError("no test cases in the file")
    return results


def combined(outcomes):
    if not outcomes:
        return "missing"
    for outcome in ("failed", "skipped"):
        if outcome in outcomes:
            return outcome
    return "passed"


def check_requirement(entry, root, cache):
    def text(relative):
        if relative not in cache:
            path = safe_path(root, relative)
            cache[relative] = None if path is None else path.read_text(encoding="utf-8", errors="replace")
        return cache[relative]

    gaps, implementation, tests = [], [], []
    for item in entry["implementation"]:
        problems = [f"file not found: {name}" for name in item["files"] if text(name) is None]
        for symbol in item.get("symbols", []):
            leaf = re.escape(symbol.split("::")[-1])
            pattern = re.compile(r"(?<![A-Za-z0-9_])" + leaf + r"(?![A-Za-z0-9_])")
            if not any(text(name) and pattern.search(text(name)) for name in item["files"]):
                problems.append(f"symbol not found: {symbol}")
        implementation.append({"files": item["files"], "symbols": item.get("symbols", []),
                               "note": item.get("note", ""), "problems": problems})
        gaps.extend("stale code link, " + problem for problem in problems)
    seen = set()
    for item in entry["tests"]:
        source = text(item["file"])
        for name in item["names"]:
            problem = ""
            match = TEST_PATTERN.fullmatch(name)
            if name in seen:
                problem = "listed twice"
            elif not match:
                problem = "not a Suite.Test name"
            elif source is None:
                problem = f"file not found: {item['file']}"
            else:
                instance, suite, test = match.groups()
                if not re.search(r"\b(?:TYPED_)?TEST(?:_F|_P)?\s*\(\s*" + suite + r"\s*,\s*" + test + r"\s*\)",
                                 source):
                    problem = f"not declared in {item['file']}"
                elif instance and not re.search(
                        r"\bINSTANTIATE_(?:TYPED_)?TEST_SUITE_P\s*\(\s*" + instance + r"\s*,\s*" + suite + r"\b",
                        source):
                    problem = f"instantiation {instance} not declared in {item['file']}"
            seen.add(name)
            tests.append({"id": name, "file": item["file"], "problem": problem, "results": {}})
            if problem:
                gaps.append(f"stale test link, {name}: {problem}")
    if not implementation:
        gaps.append("no code mapped")
    if not tests:
        gaps.append("no tests mapped")
    return implementation, tests, gaps


def analyze(registry, root, junit_paths):
    """Return the report. junit_paths maps a configuration name to its JUnit XML path."""
    errors = structure_errors(registry)
    report = {"schema": SCHEMA, "errors": errors, "configurations": {}, "metrics": None, "requirements": []}
    if errors:
        return report
    defined, source_errors = defined_requirements(root, registry["requirement_sources"])
    errors.extend(source_errors)
    listed = {entry["id"]: entry for entry in registry["requirements"]}
    for identifier in sorted(set(defined) - set(listed)):
        errors.append(f"{identifier}: defined in the requirement documents but missing from the registry")
    for identifier, entry in listed.items():
        if identifier not in defined:
            errors.append(f"{identifier}: not defined in the requirement documents")
        elif defined[identifier] != entry["title"]:
            errors.append(f"{identifier}: title differs from the requirement documents "
                          f"(\"{entry['title']}\" / \"{defined[identifier]}\")")
    for name in junit_paths:
        if name not in registry["required_configurations"]:
            errors.append(f"JUnit results given for {name}, which is not a required configuration")

    results = {}
    for name in registry["required_configurations"]:
        if name not in junit_paths:
            report["configurations"][name] = {"available": False, "detail": "no results provided"}
            continue
        try:
            results[name] = junit_results(junit_paths[name])
            report["configurations"][name] = {"available": True, "detail": f"{len(results[name])} tests"}
        except InputError as error:
            report["configurations"][name] = {"available": False, "detail": f"unusable results: {error}"}
    evidence = len(results) == len(registry["required_configurations"])

    cache, stale = {}, 0
    counts = {"applicable": 0, "excluded": 0, "implementation": 0, "tests": 0, "passing": 0}
    for entry in registry["requirements"]:
        implementation, tests, gaps = check_requirement(entry, root, cache)
        stale += sum(1 for gap in gaps if gap.startswith("stale"))
        applicable = entry.get("applicable", True)
        code_mapped = bool(implementation) and not any(item["problems"] for item in implementation)
        test_mapped = bool(tests) and not any(test["problem"] for test in tests)
        not_passed = False
        for test in tests:
            for name in registry["required_configurations"]:
                outcome = combined(results[name].get(test["id"])) if name in results else "unknown"
                test["results"][name] = outcome
                if outcome not in ("passed", "unknown"):
                    not_passed = True
                    gaps.append(f"{test['id']} {outcome} with {name}")
        # False on any known gap, None (unknown) while a required configuration has no results.
        passing = False if not (code_mapped and test_mapped) or not_passed else (True if evidence else None)
        if applicable:
            counts["applicable"] += 1
            counts["implementation"] += code_mapped
            counts["tests"] += test_mapped
            counts["passing"] += bool(passing)
        else:
            counts["excluded"] += 1
        report["requirements"].append({
            "id": entry["id"], "title": entry["title"], "status": entry["status"], "issue": entry["issue"],
            "section": entry["section"], "applicable": applicable, "reason": entry.get("reason", ""),
            "implementation": implementation, "tests": tests, "code_mapped": code_mapped,
            "test_mapped": test_mapped, "tests_passing": passing, "gaps": gaps})

    def metric(key, known=True):
        if not known or not counts["applicable"]:
            return {"count": None, "percent": None}
        # Rounded down, so 100% appears only when every applicable requirement qualifies.
        return {"count": counts[key], "percent": math.floor(1000 * counts[key] / counts["applicable"]) / 10}

    report["metrics"] = {"applicable": counts["applicable"], "excluded": counts["excluded"],
                         "stale_links": stale, "implementation": metric("implementation"),
                         "tests": metric("tests"), "passing": metric("passing", evidence)}
    return report


def badge_text(report, key):
    metrics = report["metrics"]
    if report["errors"] or metrics is None or metrics[key]["percent"] is None:
        return "unknown"
    return f"{metrics[key]['percent']:.1f}% ({metrics[key]['count']}/{metrics['applicable']})"


def write_badges(report, directory):
    for key, filename, label in BADGES:
        message = badge_text(report, key) if report else "unknown"
        color = UNKNOWN_COLOR if message == "unknown" else "#17669b"
        Path(directory, filename).write_text(svg(label, message, color), encoding="utf-8", newline="\n")


def table(registry, section):
    """One generated docs/traceability.md table, in the registry's order."""
    rows = []
    for entry in registry["requirements"]:
        if entry["section"] != section:
            continue
        code = []
        for item in entry["implementation"]:
            parts = [f"`{symbol}`" for symbol in item.get("symbols", [])]
            if item.get("note"):
                parts.append(item["note"])
            files = " / ".join(f"`{name}`" for name in item["files"])
            code.append(files + (": " + ", ".join(parts) if parts else ""))
        tests = [f"`{item['file']}`: " + "; ".join(f"`{name}`" for name in item["names"])
                 for item in entry["tests"]]
        issue = f"[#{entry['issue']}]({registry.get('issue_url', '')}{entry['issue']})"
        cells = (entry["id"], entry["title"], entry["status"], issue, "<br>".join(code), "<br>".join(tests))
        rows.append("| " + " | ".join(cells) + " |\n")
    return TABLE_HEADER + "".join(rows)


def generated_document(registry, text):
    """Return text with each '<!-- traceability:begin NAME -->' block regenerated."""
    for section in SECTIONS:
        begin, end = f"<!-- traceability:begin {section} -->\n", f"<!-- traceability:end {section} -->\n"
        if text.count(begin) != 1 or text.count(end) != 1 or text.index(begin) > text.index(end):
            raise InputError(f"Expected exactly one '{begin.strip()}' ... '{end.strip()}' block")
        text = text[:text.index(begin) + len(begin)] + table(registry, section) + text[text.index(end):]
    return text


RESULT_CLASS = {"passed": "ok", "failed": "bad", "skipped": "bad", "missing": "bad", "unknown": "unk"}
STYLE = """
body{font:14px/1.45 system-ui,sans-serif;margin:24px;color:#1b1f24}
h1{font-size:22px}h2{font-size:16px;margin-top:28px}
table{border-collapse:collapse;width:100%}th,td{border:1px solid #d0d7de;padding:6px 8px;vertical-align:top;text-align:left}
th{background:#f3f5f7;position:sticky;top:0}code{font:12px ui-monospace,Consolas,monospace;overflow-wrap:anywhere}
.tiles{display:flex;gap:12px;flex-wrap:wrap}.tile{border:1px solid #d0d7de;border-radius:6px;padding:10px 14px;min-width:190px}
.tile b{display:block;font-size:20px}.ok{color:#116329}.bad{color:#b3261e}.unk{color:#6e7781}
.chip{display:inline-block;border:1px solid currentColor;border-radius:10px;padding:0 6px;margin:1px 2px 1px 0;font-size:11px}
.problem{color:#b3261e;font-weight:600}.muted{color:#6e7781}ul{margin:0;padding-left:18px}
.controls{margin:14px 0}.controls input[type=search]{padding:5px 8px;width:280px}
"""
SCRIPT = """
const rows=[...document.querySelectorAll('#matrix tbody tr')];
const query=document.getElementById('query'),gaps=document.getElementById('gaps'),shown=document.getElementById('shown');
function apply(){const text=query.value.toLowerCase();let count=0;
for(const row of rows){const visible=(!gaps.checked||row.dataset.gap==='1')&&row.textContent.toLowerCase().includes(text);
row.hidden=!visible;count+=visible;}shown.textContent=count+' of '+rows.length+' requirements shown';}
query.addEventListener('input',apply);gaps.addEventListener('change',apply);apply();
"""


def html_report(report, registry):
    out = ["<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">",
           "<title>Requirements traceability</title><style>", STYLE, "</style></head><body>",
           "<h1>Requirements traceability</h1>",
           "<p>Requirement → code → tests → executed result. A mapping records where a requirement is "
           "implemented and tested; passing tests are evidence, not a review. Neither makes a requirement "
           "Verified, and nothing here is a safety or security approval.</p>"]
    if report["errors"]:
        out.append("<h2 class=\"bad\">Registry problems: percentages are unknown until these are fixed</h2><ul>")
        out.extend(f"<li>{escape(error)}</li>" for error in report["errors"])
        out.append("</ul>")
    metrics = report["metrics"]
    if metrics:
        out.append("<div class=\"tiles\">")
        for key, _, label in BADGES:
            out.append(f"<div class=\"tile\"><b>{escape(badge_text(report, key))}</b>{escape(label)}</div>")
        out.append(f"<div class=\"tile\"><b>{metrics['applicable']}</b>applicable requirements "
                   f"({metrics['excluded']} excluded with a reason)</div>")
        out.append(f"<div class=\"tile\"><b>{metrics['stale_links']}</b>stale links</div></div>")
    out.append("<h2>Required configurations</h2><table><thead><tr><th>Configuration</th><th>Test results</th>"
               "</tr></thead><tbody>")
    for name, state in report["configurations"].items():
        css = "ok" if state["available"] else "bad"
        out.append(f"<tr><td><code>{escape(name)}</code></td><td class=\"{css}\">{escape(state['detail'])}</td></tr>")
    out.append("</tbody></table><h2>Matrix</h2><div class=\"controls\"><input id=\"query\" type=\"search\" "
               "placeholder=\"Filter by ID, file, test…\"> <label><input id=\"gaps\" type=\"checkbox\"> "
               "gaps only</label> <span id=\"shown\" class=\"muted\"></span></div>"
               "<table id=\"matrix\"><thead><tr><th>Requirement</th><th>Status</th><th>Code</th><th>Tests and "
               "results</th><th>Result</th></tr></thead><tbody>")
    for item in report["requirements"]:
        code = []
        for entry in item["implementation"]:
            line = " / ".join(f"<code>{escape(name)}</code>" for name in entry["files"])
            details = [f"<code>{escape(symbol)}</code>" for symbol in entry["symbols"]]
            if entry["note"]:
                details.append(escape(entry["note"]))
            if details:
                line += ": " + ", ".join(details)
            line += "".join(f" <span class=\"problem\">{escape(problem)}</span>" for problem in entry["problems"])
            code.append(f"<li>{line}</li>")
        tests = []
        for test in item["tests"]:
            chips = "".join(f"<span class=\"chip {RESULT_CLASS[outcome]}\">{escape(name)}: {outcome}</span>"
                            for name, outcome in test["results"].items())
            problem = f" <span class=\"problem\">{escape(test['problem'])}</span>" if test["problem"] else ""
            tests.append(f"<li><code>{escape(test['id'])}</code> <span class=\"muted\">{escape(test['file'])}"
                         f"</span>{problem}<br>{chips}</li>")
        if not item["applicable"]:
            result = f"<span class=\"muted\">not applicable: {escape(item['reason'])}</span>"
        elif item["tests_passing"]:
            result = "<span class=\"ok\">mapped, all tests passing</span>"
        elif item["tests_passing"] is None:
            result = "<span class=\"unk\">mapped, results unknown</span>"
        else:
            result = "<span class=\"bad\">gap</span>"
        gap = "0" if item["tests_passing"] or not item["applicable"] else "1"
        gaps = "".join(f"<li>{escape(text)}</li>" for text in item["gaps"])
        issue = f"<a href=\"{escape(registry.get('issue_url', ''))}{item['issue']}\">#{item['issue']}</a>"
        out.append(f"<tr data-gap=\"{gap}\"><td><b>{escape(item['id'])}</b><br>{escape(item['title'])}<br>{issue}"
                   f"</td><td>{escape(item['status'])}</td><td><ul>{''.join(code)}</ul></td>"
                   f"<td><ul>{''.join(tests)}</ul></td><td>{result}<ul>{gaps}</ul></td></tr>")
    out.extend(["</tbody></table><script>", SCRIPT, "</script></body></html>\n"])
    return "".join(out)


def main(arguments=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--registry", type=Path, default=Path("docs/requirements-registry.json"))
    parser.add_argument("--root", type=Path, default=Path("."), help="repository root for the linked files")
    parser.add_argument("--junit", action="append", default=[], metavar="CONFIGURATION=FILE",
                        help="JUnit XML of one required configuration; repeat for each")
    parser.add_argument("--output-dir", type=Path, help="write traceability.json, .html and the badges here")
    parser.add_argument("--write-docs", action="store_true", help="regenerate the tables in the traceability document")
    parser.add_argument("--unknown-badges", type=Path, metavar="DIR",
                        help="only write an 'unknown' badge for each badge missing from DIR, then exit")
    args = parser.parse_args(arguments)

    if args.unknown_badges:
        args.unknown_badges.mkdir(parents=True, exist_ok=True)
        for _, filename, label in BADGES:
            if not Path(args.unknown_badges, filename).is_file():
                Path(args.unknown_badges, filename).write_text(svg(label, "unknown", UNKNOWN_COLOR),
                                                               encoding="utf-8", newline="\n")
        return 0
    try:
        junit_paths = {}
        for item in args.junit:
            name, separator, path = item.partition("=")
            if not separator or not name or not path or name in junit_paths:
                raise InputError(f"--junit needs one CONFIGURATION=FILE per configuration, got {item}")
            junit_paths[name] = path
        registry = load_registry(args.registry)
        report = analyze(registry, args.root, junit_paths)
        if not structure_errors(registry) and registry.get("generated_tables"):
            document = Path(args.root, registry["generated_tables"])
            try:
                current = document.read_text(encoding="utf-8")
                expected = generated_document(registry, current)
            except OSError as error:
                raise InputError(f"Cannot read {document}: {error}") from error
            if args.write_docs and expected != current:
                document.write_text(expected, encoding="utf-8", newline="\n")
                print(f"Updated {document}")
            elif expected != current:
                report["errors"].append(f"{registry['generated_tables']} differs from the registry; run "
                                        "python tools/traceability.py --write-docs")
    except InputError as error:
        print(f"error: {error}", file=sys.stderr)
        if args.output_dir:
            args.output_dir.mkdir(parents=True, exist_ok=True)
            write_badges(None, args.output_dir)
        return 2

    if args.output_dir:
        args.output_dir.mkdir(parents=True, exist_ok=True)
        Path(args.output_dir, "traceability.json").write_text(
            json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")
        Path(args.output_dir, "traceability.html").write_text(html_report(report, registry), encoding="utf-8",
                                                              newline="\n")
        write_badges(report, args.output_dir)
        print(f"Report: {Path(args.output_dir, 'traceability.html')}")
    for error in report["errors"]:
        print(f"error: {error}", file=sys.stderr)
    stale = 0
    for item in report["requirements"]:
        for gap in item["gaps"]:
            if gap.startswith("stale"):
                stale += 1
                print(f"error: {item['id']}: {gap}", file=sys.stderr)
    for name, state in report["configurations"].items():
        print(f"{name}: {state['detail']}")
    for key, _, label in BADGES:
        print(f"{label}: {badge_text(report, key)}")
    return 1 if report["errors"] or stale else 0


if __name__ == "__main__":
    sys.exit(main())
