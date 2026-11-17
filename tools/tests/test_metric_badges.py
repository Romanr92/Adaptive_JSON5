"""Regression tests for measured coverage and CodeQL security-severity badges."""
# SPDX-License-Identifier: Apache-2.0

import contextlib
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import metric_badges  # noqa: E402


def rule(identifier, score=None):
    properties = {} if score is None else {"security-severity": score}
    return {"id": identifier, "properties": properties}


def document(scores):
    return {"runs": [{"tool": {"driver": {"rules": [rule(str(i), score) for i, score in enumerate(scores)]}},
                      "results": [{"ruleId": str(i)} for i in range(len(scores))]}]}


class CoverageTests(unittest.TestCase):
    def test_formats_independent_values_and_retains_combined_message(self):
        data = {"line_percent": 12.34, "function_percent": 56.78, "branch_percent": 90}
        self.assertEqual(metric_badges.coverage_values(data),
                         {"lines": "12.3%", "functions": "56.8%", "branches": "90.0%"})
        self.assertEqual(metric_badges.coverage_message(data),
                         "lines 12.3% | functions 56.8% | branches 90.0%")

    def test_missing_coverage_is_not_reported_as_zero(self):
        self.assertEqual(metric_badges.coverage_values({"line_percent": 0}),
                         {"lines": "0.0%", "functions": "n/a", "branches": "n/a"})

    def test_rejects_invalid_percentages(self):
        for value in (-0.1, 100.1, "nan", "inf", "invalid"):
            with self.subTest(value=value), self.assertRaises(ValueError):
                metric_badges.coverage_values({"branch_percent": value})


class MisraTests(unittest.TestCase):
    def test_counts_security_thresholds_including_zero_and_missing(self):
        scores = ["0", "0.1", "3.9", "4.0", "6.9", "7.0", "8.9", "9.0", "10", None]
        self.assertEqual(metric_badges.misra_counts([document(scores)]),
                         {"critical": 2, "high": 2, "medium": 2, "low": 2, "unrated": 2})

    def test_counts_all_documents_and_runs_and_keeps_combined_metrics(self):
        first = document(["4.5", "9.2"])
        first["runs"].extend(document(["7.5"])["runs"])
        self.assertEqual(metric_badges.misra_message([first, document([None, "1.2"])]),
                         "critical 1 | high 1 | medium 1 | low 1 | unrated 1")

    def test_problem_severity_is_not_a_security_severity(self):
        data = document([None, None, None])
        for result, level in zip(data["runs"][0]["results"], ("error", "warning", "note")):
            result["level"] = level
        self.assertEqual(metric_badges.misra_counts([data]),
                         {"critical": 0, "high": 0, "medium": 0, "low": 0, "unrated": 3})

    def test_excludes_suppressed_and_absent_findings(self):
        data = document(["9.8", "7.5", "4.5"])
        results = data["runs"][0]["results"]
        results[0]["suppressions"] = [{"kind": "external", "status": "accepted"}]
        results[1]["baselineState"] = "absent"
        self.assertEqual(metric_badges.misra_counts([data]),
                         {"critical": 0, "high": 0, "medium": 1, "low": 0, "unrated": 0})

    def test_resolves_driver_indices_and_extension_component_references(self):
        run = {"tool": {"driver": {"name": "CodeQL", "rules": [rule("duplicate", "9.8")]},
                        "extensions": [{"name": "misra", "rules": [rule("duplicate", "4.5")]},
                                       {"name": "security", "guid": "security-guid",
                                        "rules": [rule("unique", "7.5")]}]},
               "results": [{"ruleIndex": 0},
                           {"rule": {"index": 0, "toolComponent": {"index": 0}}},
                           {"rule": {"id": "unique", "toolComponent": {"name": "security"}}},
                           {"rule": {"index": 0, "toolComponent": {"guid": "security-guid"}}},
                           {"ruleId": "unique"}]}
        self.assertEqual(metric_badges.misra_counts([{"runs": [run]}]),
                         {"critical": 1, "high": 3, "medium": 1, "low": 0, "unrated": 0})

    def test_unresolvable_rules_are_unrated_instead_of_using_another_component(self):
        run = {"tool": {"driver": {"rules": [rule("driver", "9.8")]},
                        "extensions": [{"rules": [rule("extension", "4.5")]}]},
               "results": [{"ruleId": "missing"}, {"ruleIndex": 5},
                           {"ruleId": "extension", "ruleIndex": 0},
                           {"rule": {"index": 0, "toolComponent": {"index": 8}}}]}
        self.assertEqual(metric_badges.misra_counts([{"runs": [run]}])["unrated"], 4)

    def test_rejects_malformed_security_scores_before_publishing(self):
        for value in (-0.1, 10.1, "nan", "inf", "invalid"):
            with self.subTest(value=value), self.assertRaises(ValueError):
                metric_badges.misra_counts([document([value])])


class CommandTests(unittest.TestCase):
    def generate(self, kind, sources, both=True):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        root = Path(temporary.name)
        arguments = [kind]
        for index, source in enumerate(sources):
            path = root / f"source-{index}.json"
            path.write_text(json.dumps(source), encoding="utf-8")
            arguments.append(str(path))
        arguments += ["--output-dir", str(root / "badges")]
        if both:
            arguments += ["--output", str(root / "combined.svg")]
        with contextlib.redirect_stdout(io.StringIO()):
            metric_badges.main(arguments)
        return root

    def assert_badge(self, path, label, value):
        element = ET.fromstring(path.read_text(encoding="utf-8"))
        self.assertEqual(element.attrib["aria-label"], f"{label}: {value}")
        self.assertEqual(element.attrib["height"], "24")
        self.assertLess(int(element.attrib["width"]), 200)
        texts = [item.text for item in element.findall(".//{http://www.w3.org/2000/svg}text")]
        self.assertEqual(texts, [label, value])

    def test_cli_writes_three_short_coverage_badges_and_legacy_output(self):
        root = self.generate("coverage", [{"line_percent": 12.3, "branch_percent": 45.6}])
        self.assertEqual({path.name for path in (root / "badges").iterdir()},
                         {"coverage-lines.svg", "coverage-functions.svg", "coverage-branches.svg"})
        self.assert_badge(root / "badges/coverage-lines.svg", "lines", "12.3%")
        self.assert_badge(root / "badges/coverage-functions.svg", "functions", "n/a")
        self.assert_badge(root / "badges/coverage-branches.svg", "branches", "45.6%")
        self.assertIn("lines 12.3% | functions n/a | branches 45.6%",
                      (root / "combined.svg").read_text(encoding="utf-8"))

    def test_cli_writes_all_severities_with_dynamic_counts(self):
        root = self.generate("misra", [document(["4.1", "4.1", "4.1"]), document(["7.1", None])])
        self.assertEqual({path.name for path in (root / "badges").iterdir()},
                         {f"misra-{name}.svg" for name in metric_badges.SEVERITIES})
        for severity, count in {"critical": 0, "high": 1, "medium": 3, "low": 0, "unrated": 1}.items():
            self.assert_badge(root / f"badges/misra-{severity}.svg", severity, str(count))
        self.assertIn("medium 3", (root / "combined.svg").read_text(encoding="utf-8"))

    def test_cli_accepts_separate_badges_without_combined_output(self):
        root = self.generate("misra", [document([])], both=False)
        self.assertFalse((root / "combined.svg").exists())
        self.assert_badge(root / "badges/misra-medium.svg", "medium", "0")

    def test_svg_escapes_accessible_title_and_visible_text(self):
        element = ET.fromstring(metric_badges.svg("a<&", "b>&"))
        self.assertEqual(element.attrib["aria-label"], "a<&: b>&")
        self.assertEqual(element.find("{http://www.w3.org/2000/svg}title").text, "a<&: b>&")


if __name__ == "__main__":
    unittest.main()
