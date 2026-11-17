"""Units for the requirements traceability analyzer and the version badge text."""
# SPDX-License-Identifier: Apache-2.0

import contextlib
import copy
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import metric_badges  # noqa: E402
import traceability  # noqa: E402

CONFIGURATIONS = ["gcc", "clang"]
REQUIREMENTS = """# Requirements
| ID / title | Requirement |
| --- | --- |
| J5-001 — First | The library shall do the first thing. |
| J5-002 — Second | The library shall do the second thing. |
| J5-003 — Third | The library shall do the third thing. |
"""
SOURCE = "int first();\nint second_value();\n"
TESTS = """TEST(J5_001_First, Works) {}
TEST_P(J5_002_Second, Accepts) {}
INSTANTIATE_TEST_SUITE_P(Modes, J5_002_Second, ::testing::Values(1, 2));
"""
DOCUMENT = ("# Traceability\n<!-- traceability:begin core -->\n<!-- traceability:end core -->\n"
            "<!-- traceability:begin api -->\n<!-- traceability:end api -->\n")
PASSING = {"J5_001_First.Works": "run", "Modes/J5_002_Second.Accepts/0": "run",
           "Modes/J5_002_Second.Accepts/1": "run"}


def requirement(identifier, title, symbols=None, tests=None, **extra):
    entry = {"id": identifier, "title": title, "status": "Draft", "issue": 1, "section": "core",
             "implementation": [{"files": ["src/code.hpp"], "symbols": symbols}] if symbols else [],
             "tests": [{"file": "tests/units.cpp", "names": tests}] if tests else []}
    entry.update(extra)
    return entry


def registry():
    return {"schema": 1, "requirement_sources": ["docs/requirements.md"], "generated_tables": "docs/traceability.md",
            "issue_url": "https://example.invalid/issues/", "required_configurations": list(CONFIGURATIONS),
            "requirements": [
                requirement("J5-001", "First", ["first"], ["J5_001_First.Works"]),
                requirement("J5-002", "Second", ["second_value"], ["Modes/J5_002_Second.Accepts"]),
                requirement("J5-003", "Third")]}


def junit(cases):
    """CTest-style JUnit; status is run, fail, notrun or skipped."""
    body = []
    for name, status in cases.items():
        child = {"fail": "<failure message=\"\"/>", "skipped": "<skipped/>"}.get(status, "")
        status = "run" if status == "skipped" else status
        body.append(f"<testcase name=\"{name}\" classname=\"{name}\" status=\"{status}\">{child}</testcase>")
    return "<?xml version=\"1.0\"?><testsuite name=\"units\">" + "".join(body) + "</testsuite>"


class Tree:
    """A temporary repository with a registry, requirement document, code, tests and JUnit files."""

    def __init__(self, test, data=None, results=None, source=SOURCE, tests=TESTS):
        temporary = tempfile.TemporaryDirectory()
        test.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.data = registry() if data is None else data
        self.write("docs/requirements.md", REQUIREMENTS)
        self.write("docs/traceability.md", DOCUMENT)
        self.write("src/code.hpp", source)
        self.write("tests/units.cpp", tests)
        self.junit = {}
        for name in CONFIGURATIONS if results is None else results:
            cases = PASSING if results is None else results[name]
            self.write(name + ".xml", cases if isinstance(cases, str) else junit(cases))
            self.junit[name] = str(self.root / (name + ".xml"))

    def write(self, relative, text):
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8", newline="\n")

    def report(self):
        return traceability.analyze(self.data, self.root, self.junit)

    def run_tool(self, *extra):
        self.write("registry.json", json.dumps(self.data))
        arguments = ["--registry", str(self.root / "registry.json"), "--root", str(self.root),
                     "--output-dir", str(self.root / "out"), *extra]
        for name, path in self.junit.items():
            arguments += ["--junit", f"{name}={path}"]
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()) as errors:
            code = traceability.main(arguments)
        return code, errors.getvalue()

    def badge(self, key):
        filename = next(name for badge, name, _ in traceability.BADGES if badge == key)
        return (self.root / "out" / filename).read_text(encoding="utf-8")


def by_id(report, identifier):
    return next(item for item in report["requirements"] if item["id"] == identifier)


class MetricTests(unittest.TestCase):
    def test_counts_every_applicable_requirement_including_unmapped_ones(self):
        report = Tree(self).report()  # 2 of 3 is 66.67%, shown rounded down
        self.assertEqual(report["errors"], [])
        self.assertEqual(report["metrics"]["applicable"], 3)
        for key in ("implementation", "tests", "passing"):
            self.assertEqual(report["metrics"][key], {"count": 2, "percent": 66.6})
        self.assertEqual(by_id(report, "J5-003")["gaps"], ["no code mapped", "no tests mapped"])
        self.assertIs(by_id(report, "J5-003")["tests_passing"], False)

    def test_excluded_requirement_leaves_the_denominator_only_with_a_reason(self):
        data = registry()
        data["requirements"][2].update(status="Obsolete", applicable=False, reason="Replaced by J5-001")
        report = Tree(self, data).report()
        self.assertEqual((report["metrics"]["applicable"], report["metrics"]["excluded"]), (2, 1))
        self.assertEqual(report["metrics"]["passing"], {"count": 2, "percent": 100.0})
        del data["requirements"][2]["reason"]
        self.assertIn("J5-003: \"applicable\": false needs a written \"reason\"", Tree(self, data).report()["errors"])

    def test_obsolete_requirement_must_be_excluded_explicitly(self):
        data = registry()
        data["requirements"][2]["status"] = "Obsolete"
        self.assertTrue(any("Obsolete" in error for error in Tree(self, data).report()["errors"]))

    def test_draft_and_deferred_requirements_stay_in_the_denominator(self):
        data = registry()
        data["requirements"][2]["status"] = "Deferred"
        self.assertEqual(Tree(self, data).report()["metrics"]["applicable"], 3)


class LinkTests(unittest.TestCase):
    def test_missing_file_and_symbol_are_stale_and_unmapped(self):
        data = registry()
        data["requirements"][0]["implementation"] = [{"files": ["src/code.hpp", "src/gone.cpp"],
                                                      "symbols": ["first", "removed_function"]}]
        report = Tree(self, data).report()
        item = by_id(report, "J5-001")
        self.assertFalse(item["code_mapped"])
        self.assertEqual(item["implementation"][0]["problems"],
                         ["file not found: src/gone.cpp", "symbol not found: removed_function"])
        self.assertEqual(report["metrics"]["stale_links"], 2)
        self.assertEqual(report["metrics"]["implementation"]["count"], 1)
        self.assertEqual(report["metrics"]["passing"]["count"], 1)

    def test_symbol_must_match_a_whole_identifier(self):
        data = registry()
        data["requirements"][1]["implementation"][0]["symbols"] = ["second"]
        self.assertFalse(by_id(Tree(self, data).report(), "J5-002")["code_mapped"])

    def test_qualified_symbol_is_found_by_its_last_name(self):
        data = registry()
        data["requirements"][0]["implementation"][0]["symbols"] = ["widget::first"]
        self.assertTrue(by_id(Tree(self, data).report(), "J5-001")["code_mapped"])

    def test_path_outside_the_repository_is_not_followed(self):
        data = registry()
        data["requirements"][0]["implementation"][0]["files"] = ["../outside.hpp"]
        self.assertFalse(by_id(Tree(self, data).report(), "J5-001")["code_mapped"])

    def test_undeclared_test_renamed_instantiation_and_missing_test_file_are_stale(self):
        data = registry()
        data["requirements"][0]["tests"][0]["names"] = ["J5_001_First.Renamed"]
        data["requirements"][1]["tests"][0]["names"] = ["Other/J5_002_Second.Accepts"]
        data["requirements"][2]["tests"] = [{"file": "tests/gone.cpp", "names": ["J5_003_Third.Works"]}]
        report = Tree(self, data).report()
        self.assertEqual(by_id(report, "J5-001")["tests"][0]["problem"], "not declared in tests/units.cpp")
        self.assertEqual(by_id(report, "J5-002")["tests"][0]["problem"],
                         "instantiation Other not declared in tests/units.cpp")
        self.assertEqual(by_id(report, "J5-003")["tests"][0]["problem"], "file not found: tests/gone.cpp")
        self.assertEqual(report["metrics"]["tests"]["count"], 0)

    def test_malformed_and_repeated_test_names_are_reported(self):
        data = registry()
        data["requirements"][0]["tests"][0]["names"] = ["J5_001_First.Works", "J5_001_First.Works", "NoDot"]
        problems = [test["problem"] for test in by_id(Tree(self, data).report(), "J5-001")["tests"]]
        self.assertEqual(problems, ["", "listed twice", "not a Suite.Test name"])


class RegistryTests(unittest.TestCase):
    def assert_error(self, data, fragment):
        report = Tree(self, data).report()
        self.assertTrue(any(fragment in error for error in report["errors"]), report["errors"])
        return report

    def test_duplicate_id_makes_metrics_unknown(self):
        data = registry()
        data["requirements"].append(copy.deepcopy(data["requirements"][0]))
        report = self.assert_error(data, "J5-001: duplicate ID")
        self.assertIsNone(report["metrics"])
        for key, _, _ in traceability.BADGES:
            self.assertEqual(traceability.badge_text(report, key), "unknown")

    def test_malformed_entries_are_reported_not_raised(self):
        data = registry()
        data["requirements"][0].update(id="REQ-1", status="Done", section="other", issue="12")
        data["requirements"][1]["implementation"] = [{"files": [], "symbols": ["x"]}]
        data["requirements"][2]["tests"] = [{"file": "tests/units.cpp"}]
        data["requirements"].append("J5-004")
        data["required_configurations"] = []
        report = self.assert_error(data, "REQ-1: ID must look like J5-123")
        for fragment in ("\"status\" must be one of", "\"section\" must be one of", "\"issue\" must be an issue",
                         "each implementation entry needs", "each tests entry needs",
                         "requirements[3]: must be an object", "\"required_configurations\" must be"):
            self.assertTrue(any(fragment in error for error in report["errors"]), fragment)

    def test_requirement_missing_from_registry_or_documents_or_retitled(self):
        data = registry()
        data["requirements"][2] = requirement("J5-009", "Unknown")
        data["requirements"][0]["title"] = "Renamed"
        report = self.assert_error(data, "J5-003: defined in the requirement documents but missing")
        self.assertTrue(any("J5-009: not defined" in error for error in report["errors"]))
        self.assertTrue(any("J5-001: title differs" in error for error in report["errors"]))
        self.assertEqual(traceability.badge_text(report, "implementation"), "unknown")

    def test_missing_requirement_document_is_an_error(self):
        data = registry()
        data["requirement_sources"] = ["docs/absent.md"]
        self.assert_error(data, "Requirement source docs/absent.md does not exist")

    def test_unreadable_or_wrong_schema_registry_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory, "registry.json")
            for text in ("{not json", "[]", "{\"schema\": 2, \"requirements\": []}", "{\"schema\": 1}"):
                path.write_text(text, encoding="utf-8")
                with self.assertRaises(traceability.InputError):
                    traceability.load_registry(path)
            with self.assertRaises(traceability.InputError):
                traceability.load_registry(Path(directory, "absent.json"))


class ResultTests(unittest.TestCase):
    def outcome(self, results):
        tree = Tree(self, results=results)
        return tree.report()

    def test_failing_test_in_one_configuration_is_not_passing(self):
        failing = dict(PASSING, **{"J5_001_First.Works": "fail"})
        report = self.outcome({"gcc": PASSING, "clang": failing})
        item = by_id(report, "J5-001")
        self.assertIs(item["tests_passing"], False)
        self.assertEqual(item["tests"][0]["results"], {"gcc": "passed", "clang": "failed"})
        self.assertIn("J5_001_First.Works failed with clang", item["gaps"])
        self.assertEqual(report["metrics"]["passing"]["count"], 1)
        self.assertEqual(report["metrics"]["tests"]["count"], 2)

    def test_skipped_and_not_run_tests_are_not_passing(self):
        for status in ("skipped", "notrun", "disabled"):
            with self.subTest(status=status):
                report = self.outcome({"gcc": dict(PASSING, **{"J5_001_First.Works": status}), "clang": PASSING})
                self.assertIs(by_id(report, "J5-001")["tests_passing"], False)
                self.assertEqual(by_id(report, "J5-001")["tests"][0]["results"]["gcc"], "skipped")

    def test_test_absent_from_results_is_missing_not_passing(self):
        absent = {name: status for name, status in PASSING.items() if not name.startswith("J5_001")}
        report = self.outcome({"gcc": absent, "clang": PASSING})
        self.assertEqual(by_id(report, "J5-001")["tests"][0]["results"]["gcc"], "missing")
        self.assertIs(by_id(report, "J5-001")["tests_passing"], False)

    def test_every_parameterized_case_must_pass(self):
        one_failing = dict(PASSING, **{"Modes/J5_002_Second.Accepts/1": "fail"})
        report = self.outcome({"gcc": one_failing, "clang": PASSING})
        self.assertEqual(by_id(report, "J5-002")["tests"][0]["results"]["gcc"], "failed")

    def test_name_without_instantiation_covers_all_instantiations(self):
        tree = Tree(self)
        tree.data["requirements"][1]["tests"][0]["names"] = ["J5_002_Second.Accepts"]
        self.assertIs(by_id(tree.report(), "J5-002")["tests_passing"], True)

    def test_typed_suite_index_and_googletest_xml_are_understood(self):
        xml = ("<testsuites><testsuite name=\"J5_001_First/0\">"
               "<testcase name=\"Works\" classname=\"J5_001_First/0\" status=\"run\" result=\"completed\"/>"
               "<testcase name=\"Later\" classname=\"J5_001_First/0\" status=\"run\" result=\"skipped\"/>"
               "</testsuite></testsuites>")
        with tempfile.TemporaryDirectory() as directory:
            Path(directory, "gtest.xml").write_text(xml, encoding="utf-8")
            results = traceability.junit_results(Path(directory, "gtest.xml"))
        self.assertEqual(results, {"J5_001_First.Works": ["passed"], "J5_001_First.Later": ["skipped"]})

    def test_missing_configuration_makes_passing_unknown_not_zero(self):
        report = self.outcome({"gcc": PASSING})
        self.assertEqual(report["metrics"]["passing"], {"count": None, "percent": None})
        self.assertEqual(traceability.badge_text(report, "passing"), "unknown")
        self.assertEqual(traceability.badge_text(report, "tests"), "66.6% (2/3)")
        self.assertIsNone(by_id(report, "J5-001")["tests_passing"])
        self.assertIs(by_id(report, "J5-003")["tests_passing"], False)
        self.assertEqual(report["configurations"]["clang"], {"available": False, "detail": "no results provided"})

    def test_known_failure_is_reported_even_while_another_configuration_is_missing(self):
        report = self.outcome({"gcc": dict(PASSING, **{"J5_001_First.Works": "fail"})})
        self.assertIs(by_id(report, "J5-001")["tests_passing"], False)

    def test_malformed_or_empty_results_make_the_configuration_unavailable(self):
        for text in ("<testsuite><testcase", "<testsuite name=\"units\"/>"):
            with self.subTest(text=text):
                report = self.outcome({"gcc": PASSING, "clang": text})
                self.assertFalse(report["configurations"]["clang"]["available"])
                self.assertEqual(traceability.badge_text(report, "passing"), "unknown")

    def test_results_for_an_unrequired_configuration_are_an_error(self):
        tree = Tree(self)
        tree.junit["msvc"] = tree.junit["gcc"]
        self.assertTrue(any("msvc" in error for error in tree.report()["errors"]))


class ToolTests(unittest.TestCase):
    def test_writes_reproducible_report_matrix_and_badges(self):
        tree = Tree(self)
        self.assertEqual(tree.run_tool("--write-docs")[0], 0)
        first = {path.name: path.read_bytes() for path in (tree.root / "out").iterdir()}
        self.assertEqual(sorted(first), ["traceability-implementation.svg", "traceability-passing.svg",
                                         "traceability-tests.svg", "traceability.html", "traceability.json"])
        self.assertEqual(tree.run_tool()[0], 0)
        self.assertEqual(first, {path.name: path.read_bytes() for path in (tree.root / "out").iterdir()})
        self.assertIn("66.6% (2/3)", tree.badge("passing"))
        page = first["traceability.html"].decode("utf-8")
        for text in ("J5-003", "src/code.hpp", "Modes/J5_002_Second.Accepts", "clang: passed", "no tests mapped"):
            self.assertIn(text, page)

    def test_generated_tables_are_written_then_checked(self):
        tree = Tree(self)
        code, errors = tree.run_tool()
        self.assertEqual(code, 1)
        self.assertIn("differs from the registry", errors)
        self.assertIn("unknown", tree.badge("implementation"))
        self.assertEqual(tree.run_tool("--write-docs")[0], 0)
        document = (tree.root / "docs/traceability.md").read_text(encoding="utf-8")
        self.assertIn("| J5-001 | First | Draft | [#1](https://example.invalid/issues/1) | `src/code.hpp`: `first` | "
                      "`tests/units.cpp`: `J5_001_First.Works` |\n", document)
        self.assertIn("<!-- traceability:begin api -->\n| ID | Contract |", document)
        self.assertEqual(tree.run_tool()[0], 0)

    def test_document_without_markers_is_unusable_input(self):
        tree = Tree(self)
        tree.write("docs/traceability.md", "# Traceability\n")
        self.assertEqual(tree.run_tool()[0], 2)
        self.assertIn("unknown", tree.badge("passing"))

    def test_stale_link_fails_the_tool_but_still_reports_measured_values(self):
        tree = Tree(self, source="int second_value();\n")
        code, errors = tree.run_tool("--write-docs")
        self.assertEqual(code, 1)
        self.assertIn("J5-001: stale code link, symbol not found: first", errors)
        self.assertIn("33.3% (1/3)", tree.badge("implementation"))

    def test_failing_tests_do_not_fail_the_tool(self):
        tree = Tree(self, results={"gcc": dict(PASSING, **{"J5_001_First.Works": "fail"}), "clang": PASSING})
        self.assertEqual(tree.run_tool("--write-docs")[0], 0)
        self.assertIn("33.3% (1/3)", tree.badge("passing"))

    def test_unreadable_registry_writes_unknown_badges(self):
        tree = Tree(self)
        (tree.root / "bad.json").write_text("{", encoding="utf-8")
        with contextlib.redirect_stderr(io.StringIO()):
            code = traceability.main(["--registry", str(tree.root / "bad.json"), "--output-dir",
                                      str(tree.root / "out")])
        self.assertEqual(code, 2)
        self.assertIn("unknown", tree.badge("tests"))

    def test_bad_junit_argument_is_unusable_input(self):
        tree = Tree(self)
        tree.junit = {}
        self.assertEqual(tree.run_tool("--junit", "gcc")[0], 2)

    def test_unknown_badges_fill_only_missing_files(self):
        tree = Tree(self)
        out = tree.root / "out"
        out.mkdir()
        (out / "traceability-tests.svg").write_text("measured", encoding="utf-8")
        self.assertEqual(traceability.main(["--unknown-badges", str(out)]), 0)
        self.assertEqual(tree.badge("tests"), "measured")
        self.assertIn("unknown", tree.badge("passing"))
        self.assertIn(traceability.UNKNOWN_COLOR, tree.badge("implementation"))

    def test_summary_lists_totals_configurations_and_gaps(self):
        tree = Tree(self, results={"gcc": dict(PASSING, **{"J5_001_First.Works": "fail"})})
        tree.run_tool("--write-docs", "--summary", str(tree.root / "summary.md"))
        summary = (tree.root / "summary.md").read_text(encoding="utf-8")
        for text in ("| requirements mapped to code | 66.6% (2/3) |", "| requirements with passing tests | unknown |",
                     "| `clang` | no results provided |", "- J5-001: J5_001_First.Works failed with gcc",
                     "Not a merge requirement"):
            self.assertIn(text, summary)
        self.assertNotIn("no tests mapped", summary)

    def test_html_escapes_registry_text(self):
        data = registry()
        data["requirements"][2]["implementation"] = [{"files": ["src/code.hpp"], "note": "<script>alert(1)</script>"}]
        page = traceability.html_report(Tree(self, data).report(), data)
        self.assertNotIn("<script>alert(1)</script>", page)
        self.assertIn("&lt;script&gt;alert(1)&lt;/script&gt;", page)


class VersionBadgeTests(unittest.TestCase):
    def test_release_tag_is_shown_as_published(self):
        for tag in ("0.2.0", "v0.4.0", "1.0.0-rc.1", " 0.2.0\n"):
            with self.subTest(tag=tag):
                self.assertEqual(metric_badges.version_message(tag), tag.strip())

    def test_no_release_is_unreleased(self):
        self.assertEqual(metric_badges.version_message("\n"), "unreleased")

    def test_unexpected_text_is_rejected(self):
        for text in ("Not Found", "<svg>", "0.2"):
            with self.subTest(text=text), self.assertRaises(ValueError):
                metric_badges.version_message(text)


if __name__ == "__main__":
    unittest.main()
