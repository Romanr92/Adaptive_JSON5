# SPDX-License-Identifier: Apache-2.0
"""Unit tests of tools/generate_schema_unicode_tables.py (issue #187, ADR 0006)."""

import csv
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import generate_schema_unicode_tables as generator  # noqa: E402


class DerivedIdnaPropertyTests(unittest.TestCase):
    """The RFC 5892 derivation on Unicode 17.0.0 agrees with IANA's published 12.0.0 table."""

    @classmethod
    def setUpClass(cls):
        generator.verify_inputs()
        cls.category, cls.combining = generator.unicode_data()
        cls.derived = generator.derive_idna(cls.category)

    def test_matches_iana_12_for_every_code_point_assigned_in_12(self):
        mismatches = []
        path = generator.DATA / "idna-12.0.0/idna-tables-properties.csv"
        with path.open(encoding="utf-8") as table:
            for row in csv.DictReader(table):
                if row["Property"] == "UNASSIGNED":
                    continue
                first, last = generator.code_points(row["Codepoint"].replace("-", ".."))
                mismatches += [cp for cp in range(first, last + 1) if self.derived[cp] != row["Property"]]
        self.assertEqual(mismatches, [])

    def test_rfc_5892_exceptions_and_classes(self):
        self.assertEqual(self.derived[0x002D], "PVALID")      # LDH hyphen.
        self.assertEqual(self.derived[0x0041], "DISALLOWED")  # Upper case is unstable.
        self.assertEqual(self.derived[0x00B7], "CONTEXTO")    # MIDDLE DOT.
        self.assertEqual(self.derived[0x200C], "CONTEXTJ")    # ZERO WIDTH NON-JOINER.
        self.assertEqual(self.derived[0x302E], "DISALLOWED")  # Hangul single dot tone mark.
        self.assertEqual(self.derived[0x0378], "UNASSIGNED")


class HeaderTests(unittest.TestCase):
    """The checked-in header is the generator's current output."""

    def test_header_is_up_to_date(self):
        manifest = generator.verify_inputs()
        expected = generator.render(generator.build(), manifest)
        self.assertEqual(generator.OUTPUT.read_text(encoding="utf-8"), expected,
                         "run python tools/generate_schema_unicode_tables.py")

    def test_category_groups(self):
        tables = generator.build()
        names = dict(tables["gc_names"])
        index = {name: number for number, name in enumerate(tables["categories"])}
        letters = sum(1 << index[name] for name in ("Lu", "Ll", "Lt", "Lm", "Lo"))
        self.assertEqual(names["L"], letters)
        self.assertEqual(names["Letter"], letters)
        self.assertEqual(names["digit"], 1 << index["Nd"])
        self.assertNotIn("", names)


if __name__ == "__main__":
    unittest.main()
