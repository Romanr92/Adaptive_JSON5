#!/usr/bin/env python3
"""Run host sanitizer/fuzz checks and always preserve a self-contained HTML report."""
# SPDX-License-Identifier: Apache-2.0
import argparse
import hashlib
import html
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import time
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("kind", choices=("sanitizers", "fuzz"))
    parser.add_argument("--compiler", default="clang++-18")
    parser.add_argument("--exceptions", choices=("OFF", "ON"), default="OFF")
    parser.add_argument("--seconds", type=int, default=300)
    parser.add_argument("--jobs", type=int, default=2)
    parser.add_argument("--gtest-source", default="")
    args = parser.parse_args()
    if args.seconds < 1 or args.jobs < 1:
        parser.error("seconds and jobs must be positive")
    build = ROOT / "build" / (args.kind + "-" + args.exceptions.lower())
    report = build / "report"
    report.mkdir(parents=True, exist_ok=True)
    # Do not show stale reports or crash inputs as evidence for this run.
    if (report / "artifacts").exists():
        shutil.rmtree(report / "artifacts")
    (report / "artifacts").mkdir()
    for stale in ("results.xml", "seed-manifest.json"):
        (report / stale).unlink(missing_ok=True)
    for stale in report.glob("step-*.log"):
        stale.unlink()
    steps = []
    env = os.environ.copy()
    env["ASAN_OPTIONS"] = "detect_leaks=1:alloc_dealloc_mismatch=1:halt_on_error=1:abort_on_error=1"
    env["UBSAN_OPTIONS"] = "halt_on_error=1:print_stacktrace=1"
    started = time.time()
    status = 1
    metadata = {"kind": args.kind, "exceptions": args.exceptions, "RTTI": "OFF",
                "compiler": args.compiler, "seconds": args.seconds, "platform": sys.platform,
                "ASAN_OPTIONS": env["ASAN_OPTIONS"], "UBSAN_OPTIONS": env["UBSAN_OPTIONS"]}

    def run(command, timeout=1800):
        command = list(map(str, command))
        log = report / ("step-%02d.log" % len(steps))
        entry = {"command": shlex.join(command), "log": log.name, "returncode": None}
        steps.append(entry)
        print(entry["command"], flush=True)
        with log.open("w", encoding="utf-8") as output:
            try:
                result = subprocess.run(command, cwd=ROOT, env=env, stdout=output,
                                        stderr=subprocess.STDOUT, timeout=timeout, check=False)
                entry["returncode"] = result.returncode
            except subprocess.TimeoutExpired:
                entry["returncode"] = 124
                output.write("\nRuntime-check command exceeded its outer time limit.\n")
        print(log.read_text(encoding="utf-8", errors="replace")[-8000:], flush=True)
        if entry["returncode"] != 0:
            raise RuntimeError("Command failed: " + entry["command"])

    try:
        metadata["revision"] = subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
        metadata["dirty"] = bool(subprocess.check_output(
            ["git", "status", "--porcelain"], cwd=ROOT, text=True).strip())
        run([args.compiler, "--version"])
        units = "ON" if args.kind == "sanitizers" else "OFF"
        configure = ["cmake", "--fresh", "-S", ROOT, "-B", build, "-G", "Ninja",
                     "-DCMAKE_CXX_COMPILER=" + args.compiler, "-DCMAKE_BUILD_TYPE=Debug",
                     "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON", "-DADAPTIVE_JSON5_BUILD_LIBRARY=OFF",
                     "-DADAPTIVE_JSON5_BUILD_TESTS=" + units,
                     "-DADAPTIVE_JSON5_BUILD_VALIDATION_TESTS=" + units,
                     # The optional schema component is checked by the same runs (ADR 0006).
                     "-DADAPTIVE_JSON5_BUILD_SCHEMA=ON",
                     "-DADAPTIVE_JSON5_ENABLE_EXCEPTIONS=" + args.exceptions,
                     "-DADAPTIVE_JSON5_ENABLE_RTTI=OFF", "-DADAPTIVE_JSON5_ENABLE_SANITIZERS=ON",
                     "-DADAPTIVE_JSON5_BUILD_FUZZER=" + ("ON" if args.kind == "fuzz" else "OFF")]
        if args.gtest_source:
            configure.append("-DADAPTIVE_JSON5_GOOGLETEST_SOURCE_DIR=" + str(Path(args.gtest_source).resolve()))
        run(configure)
        run(["cmake", "--build", build, "--parallel", args.jobs])
        if args.kind == "sanitizers":
            run(["ctest", "--test-dir", build, "--parallel", args.jobs,
                 "--output-on-failure", "--no-tests=error", "--output-junit", report / "results.xml"])
        else:
            corpus = build / "corpus"
            corpus.mkdir(exist_ok=True)
            manifest = []
            fixtures = ROOT / "tests" / "fixtures" / "third_party"
            for path in sorted(fixtures.rglob("*")):
                if path.is_file() and path.suffix.lower() in (".json", ".json5", ".txt"):
                    data = path.read_bytes()
                    if len(data) <= 16384:
                        digest = hashlib.sha256(data).hexdigest()
                        (corpus / digest).write_bytes(data)
                        manifest.append({"source": path.relative_to(ROOT).as_posix(), "sha256": digest})
            if not manifest:
                raise RuntimeError("No committed fixture seeds found")
            (report / "seed-manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
            # Keep an evolving corpus locally; CI starts from a clean checkout.
            run([build / "tests" / "fuzz" / "adaptive_json5_fuzzer", corpus,
                 "-max_total_time=" + str(args.seconds), "-timeout=10", "-rss_limit_mb=2048",
                 "-max_len=16384", "-print_final_stats=1", "-seed=132",
                 "-artifact_prefix=" + str(report / "artifacts") + os.sep], timeout=args.seconds + 120)
        status = 0
    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
        metadata["error"] = str(error)
        print(str(error), file=sys.stderr)
    finally:
        junit = report / "results.xml"
        if junit.exists():
            try:
                tree = ET.parse(junit)
                cases = list(tree.iter("testcase"))
                metadata["tests"] = {"total": len(cases),
                    "failed": sum(case.find("failure") is not None or case.find("error") is not None for case in cases),
                    "skipped": sum(case.find("skipped") is not None for case in cases)}
            except (ET.ParseError, OSError) as error:
                metadata["report_error"] = str(error)
                status = 1
        metadata.update(status="PASS" if status == 0 else "FAIL", elapsed_seconds=round(time.time() - started, 2), steps=steps)
        (report / "summary.json").write_text(json.dumps(metadata, indent=2), encoding="utf-8")
        sections = []
        for step in steps:
            content = (report / step["log"]).read_text(encoding="utf-8", errors="replace")
            sections.append("<details><summary>" + html.escape(step["command"]) + " (exit "
                            + str(step["returncode"]) + ")</summary><pre>" + html.escape(content) + "</pre></details>")
        page = """<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Runtime check report</title>
<style>body{font:16px system-ui;margin:2em;max-width:1100px}pre{white-space:pre-wrap;overflow-wrap:anywhere;background:#f3f4f6;padding:1em}summary{cursor:pointer;margin:1em 0}h1{border-bottom:3px solid #888}</style></head><body>
<h1>""" + html.escape(args.kind + ": " + metadata["status"]) + "</h1><pre>" + html.escape(json.dumps(metadata, indent=2)) + "</pre>" + "".join(sections) + "</body></html>"
        (report / "index.html").write_text(page, encoding="utf-8")
        print("Report: " + str(report / "index.html"))
    return status


if __name__ == "__main__":
    sys.exit(main())
