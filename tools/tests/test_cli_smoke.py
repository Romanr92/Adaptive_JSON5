# SPDX-License-Identifier: Apache-2.0
"""Unit tests of tools/cli_smoke.py and the shared CLI smoke case list."""

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import cli_smoke  # noqa: E402


class SmokeCaseListTests(unittest.TestCase):
    """cli/test/smoke_cases.json is consistent with cli/test/expected."""

    def setUp(self):
        import json
        self.cases = json.loads(cli_smoke.CASES.read_text(encoding="utf-8"))["cases"]

    def test_names_are_unique_and_cases_complete(self):
        names = [case["name"] for case in self.cases]
        self.assertEqual(len(names), len(set(names)))
        for case in self.cases:
            self.assertIn(case["exit"], (0, 1, 2), case["name"])
            self.assertTrue(case["args"], case["name"])
            self.assertIn(case.get("requires", "any"), ("any", "schema", "no-schema"), case["name"])

    def test_every_exact_case_has_an_expected_file(self):
        for case in self.cases:
            if "contains" not in case:
                self.assertTrue((cli_smoke.EXPECTED / f"{case['name']}.out").is_file(), case["name"])

    def test_every_input_file_exists(self):
        for case in self.cases:
            for argument in case["args"]:
                if argument.startswith(("examples/", "cli/test/")) and "no-such-file" not in argument:
                    self.assertTrue((cli_smoke.ROOT / argument).is_file(), argument)


class CheckTests(unittest.TestCase):
    """check() reports exit, output and stderr mismatches."""

    def test_matching_and_mismatching_output(self):
        case = {"name": "file_json5_valid", "exit": 0, "args": []}
        expected = cli_smoke.read_expected("file_json5_valid", ".out")
        self.assertEqual(cli_smoke.check(case, 0, expected, "", "0.1.0"), [])
        self.assertEqual(len(cli_smoke.check(case, 1, expected, "", "0.1.0")), 1)
        problems = cli_smoke.check(case, 0, expected + "extra\n", "", "0.1.0")
        self.assertIn("+extra", problems[0])
        self.assertIn("unexpected stderr", cli_smoke.check(case, 0, expected, "boom\n", "0.1.0")[0])

    def test_contains_uses_the_tool_version(self):
        case = {"name": "version", "exit": 0, "args": [], "contains": "adaptive_json5_examples {tool_version}"}
        self.assertEqual(cli_smoke.check(case, 0, "adaptive_json5_examples 9.9.9\n", "", "9.9.9"), [])
        self.assertEqual(len(cli_smoke.check(case, 0, "adaptive_json5_examples 1.0.0\n", "", "9.9.9")), 1)

    def test_normalize_line_endings(self):
        self.assertEqual(cli_smoke.normalize(b"a\r\nb\n"), "a\nb\n")


if __name__ == "__main__":
    unittest.main()
