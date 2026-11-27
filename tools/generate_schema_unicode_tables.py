#!/usr/bin/env python3
"""Generate the schema component's Unicode property header from pinned, offline UCD inputs.

Tables (Unicode 17.0.0, ADR 0006 issue #187 section):
- General_Category of every assigned code point, and the property value names of ECMA-262
  \\p{...} (single categories and groups such as L, LC, M, N, with their aliases);
- Script of every assigned code point, and the script names and aliases;
- Joining_Type (D, L, R, T, C; U is implicit) and Virama (Canonical_Combining_Class 9) for the
  IDNA2008 CONTEXTJ rules;
- the IDNA2008 derived property (RFC 5892 section 3) of every code point that is PVALID,
  CONTEXTJ or CONTEXTO; all other code points are DISALLOWED or UNASSIGNED.

    python tools/generate_schema_unicode_tables.py            # rewrite the header
    python tools/generate_schema_unicode_tables.py --check    # fail when the header is stale
"""

# SPDX-License-Identifier: Apache-2.0
import argparse
import hashlib
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DATA = ROOT / "tools/unicode-data"
UCD = DATA / "17.0.0"
OUTPUT = ROOT / "core/includes/schema/unicode_properties.hpp"
INPUTS = ("17.0.0/UnicodeData.txt", "17.0.0/PropertyValueAliases.txt", "17.0.0/Scripts.txt",
          "17.0.0/DerivedCoreProperties.txt", "17.0.0/PropList.txt", "17.0.0/DerivedNormalizationProps.txt",
          "17.0.0/HangulSyllableType.txt", "17.0.0/extracted/DerivedJoiningType.txt")
MAX_CODE_POINT = 0x10FFFF

# RFC 5892 section 2.6 exceptions (F) and their derived values.
EXCEPTIONS = {
    0x00DF: "PVALID", 0x03C2: "PVALID", 0x06FD: "PVALID", 0x06FE: "PVALID", 0x0F0B: "PVALID", 0x3007: "PVALID",
    0x00B7: "CONTEXTO", 0x0375: "CONTEXTO", 0x05F3: "CONTEXTO", 0x05F4: "CONTEXTO", 0x30FB: "CONTEXTO",
    0x0640: "DISALLOWED", 0x07FA: "DISALLOWED", 0x302E: "DISALLOWED", 0x302F: "DISALLOWED",
    0x3031: "DISALLOWED", 0x3032: "DISALLOWED", 0x3033: "DISALLOWED", 0x3034: "DISALLOWED",
    0x3035: "DISALLOWED", 0x303B: "DISALLOWED",
}
for _cp in list(range(0x0660, 0x066A)) + list(range(0x06F0, 0x06FA)):
    EXCEPTIONS[_cp] = "CONTEXTO"
# RFC 5892 section 2.7 BackwardCompatible (G) is empty.
LETTER_DIGITS = {"Ll", "Lu", "Lo", "Nd", "Lm", "Mn", "Mc"}  # RFC 5892 section 2.1 (A).
IGNORABLE_BLOCKS = ((0x20D0, 0x20FF), (0x1D100, 0x1D1FF), (0x1D200, 0x1D24F))  # Section 2.4 (D).
IDNA_VALUES = ("PVALID", "CONTEXTJ", "CONTEXTO")
JOINING_TYPES = ("D", "L", "R", "T", "C")


def verify_inputs():
    """Check every pinned input against sources.json."""
    manifest = json.loads((DATA / "sources.json").read_text(encoding="utf-8"))
    for name in INPUTS:
        if name not in manifest:
            raise ValueError("Input not pinned in sources.json: " + name)
    for name, source in manifest.items():
        actual = hashlib.sha256((DATA / name).read_bytes()).hexdigest()
        if actual != source["sha256"]:
            raise ValueError("Pinned source hash mismatch: " + name)
    return manifest


def data_lines(path):
    """Yield the semicolon-separated fields of every non-comment line."""
    for line in path.read_text(encoding="utf-8").splitlines():
        content = line.split("#", 1)[0].strip()
        if content:
            yield [field.strip() for field in content.split(";")]


def code_points(field):
    """Return the inclusive range of a 'XXXX' or 'XXXX..YYYY' field."""
    first, _, last = field.partition("..")
    return int(first, 16), int(last or first, 16)


def unicode_data():
    """Return per-code-point General_Category and Canonical_Combining_Class (assigned only)."""
    category = {}
    combining = {}
    pending = None
    for fields in data_lines(UCD / "UnicodeData.txt"):
        value = int(fields[0], 16)
        name, gc, ccc = fields[1], fields[2], int(fields[3])
        if name.endswith(", First>"):
            pending = value
            continue
        first = pending if name.endswith(", Last>") else value
        pending = None
        for cp in range(first, value + 1):
            category[cp] = gc
            if ccc:
                combining[cp] = ccc
    return category, combining


def property_values(path, wanted=None):
    """Return {code point: value} from a 'range ; value' file (optionally one property only)."""
    values = {}
    for fields in data_lines(path):
        if wanted is not None and fields[1] != wanted:
            continue
        first, last = code_points(fields[0])
        for cp in range(first, last + 1):
            values[cp] = fields[1] if wanted is None else True
    return values


def aliases(prefix):
    """Return the PropertyValueAliases rows of one property: [short, long, *others] and the group comment."""
    rows = []
    for line in (UCD / "PropertyValueAliases.txt").read_text(encoding="utf-8").splitlines():
        content, _, comment = line.partition("#")
        fields = [field.strip() for field in content.split(";")]
        if len(fields) >= 3 and fields[0] == prefix:
            rows.append((fields[1:], comment.strip()))
    return rows


def ranges_of(values, keep=None):
    """Merge {code point: value} into sorted (first, last, value) runs of equal value."""
    runs = []
    for cp in sorted(values):
        value = values[cp]
        if keep is not None and value not in keep:
            continue
        if runs and runs[-1][1] == cp - 1 and runs[-1][2] == value:
            runs[-1] = (runs[-1][0], cp, value)
        else:
            runs.append((cp, cp, value))
    return runs


def derive_idna(category):
    """RFC 5892 section 3: the derived property of every code point."""
    default_ignorable = property_values(UCD / "DerivedCoreProperties.txt", "Default_Ignorable_Code_Point")
    white_space = property_values(UCD / "PropList.txt", "White_Space")
    noncharacter = property_values(UCD / "PropList.txt", "Noncharacter_Code_Point")
    join_control = property_values(UCD / "PropList.txt", "Join_Control")
    unstable = property_values(UCD / "DerivedNormalizationProps.txt", "Changes_When_NFKC_Casefolded")
    hangul = property_values(UCD / "HangulSyllableType.txt")
    derived = {}
    for cp in range(MAX_CODE_POINT + 1):
        if cp in EXCEPTIONS:
            value = EXCEPTIONS[cp]
        elif cp not in category and cp not in noncharacter:
            value = "UNASSIGNED"  # Section 2.11 (J).
        elif cp == 0x2D or 0x30 <= cp <= 0x39 or 0x61 <= cp <= 0x7A:
            value = "PVALID"  # LDH (K).
        elif cp in join_control:
            value = "CONTEXTJ"  # (H).
        elif cp in unstable:
            value = "DISALLOWED"  # Unstable (B).
        elif cp in default_ignorable or cp in white_space or cp in noncharacter:
            value = "DISALLOWED"  # IgnorableProperties (C).
        elif any(first <= cp <= last for first, last in IGNORABLE_BLOCKS):
            value = "DISALLOWED"  # IgnorableBlocks (D).
        elif hangul.get(cp) in ("L", "V", "T"):
            value = "DISALLOWED"  # OldHangulJamo (I).
        elif category.get(cp) in LETTER_DIGITS:
            value = "PVALID"  # LetterDigits (A).
        else:
            value = "DISALLOWED"
        derived[cp] = value
    return derived


def build():
    """Return every table and name list of the header."""
    category, combining = unicode_data()
    categories = []
    groups = {}
    for fields, comment in aliases("gc"):
        if "|" in comment:
            groups[fields[0]] = (fields, [member.strip() for member in comment.split("|")])
        else:
            categories.append(fields)
    index = {fields[0]: number for number, fields in enumerate(categories)}
    gc_names = []
    for fields in categories:
        for name in fields:
            gc_names.append((name, 1 << index[fields[0]]))
    for fields, members in groups.values():
        mask = 0
        for member in members:
            mask |= 1 << index[member]
        for name in fields:
            gc_names.append((name, mask))
    gc_ranges = [(first, last, index[value]) for first, last, value in ranges_of(category)]

    scripts = [fields for fields, _ in aliases("sc")]
    script_index = {fields[1]: number for number, fields in enumerate(scripts)}
    script_names = [(name, number) for number, fields in enumerate(scripts) for name in fields]
    script_values = {cp: script_index[name] for cp, name in property_values(UCD / "Scripts.txt").items()}
    script_ranges = ranges_of(script_values)

    joining = property_values(UCD / "extracted/DerivedJoiningType.txt")
    joining_ranges = [(first, last, JOINING_TYPES.index(value))
                      for first, last, value in ranges_of(joining, set(JOINING_TYPES))]
    virama_ranges = [(first, last, 0) for first, last, _ in ranges_of({cp: 9 for cp, ccc in combining.items()
                                                                        if ccc == 9})]
    derived = derive_idna(category)
    idna_ranges = [(first, last, IDNA_VALUES.index(value))
                   for first, last, value in ranges_of(derived, set(IDNA_VALUES))]
    return {
        "categories": [fields[0] for fields in categories],
        "gc_names": sorted(gc_names),
        "gc_ranges": gc_ranges,
        "script_count": len(scripts),
        "script_names": sorted(script_names),
        "script_ranges": script_ranges,
        "joining_ranges": joining_ranges,
        "virama_ranges": virama_ranges,
        "idna_ranges": idna_ranges,
        "derived": derived,
    }


def table(lines, name, brief, element, rows, note):
    """Append one constexpr array, one row per line."""
    lines += ["", "    /**", f"     * @brief {brief}", "     * @par Requirements",
              "     * J5-061; J5-062; ADR 0006 (issue #187).", f"     * @note {note}", "     */",
              f"    inline constexpr std::array<{element}, {len(rows)}> {name} =", "    {{"]
    lines.extend("      " + row + "," for row in rows)
    lines.append("    }};")


def render(tables, manifest):
    """Return the header text."""
    license_text = (ROOT / "LICENSES/Unicode-License-V3.txt").read_text(encoding="utf-8")
    guard = "ADAPTIVE_JSON5_CORE_SCHEMA_UNICODE_PROPERTIES_HPP_"
    lines = ["// Generated by tools/generate_schema_unicode_tables.py; do not edit table entries.",
             "// Unicode data copyright 1991-2026 Unicode, Inc.", "// SPDX-License-Identifier: Unicode-3.0",
             "/*", *license_text.splitlines(), "*/", "", f"#ifndef {guard}", f"#define {guard}", "",
             "#include <array>", "#include <cstdint>", "#include <string_view>", "",
             "namespace adaptive_json5::detail", "{"]
    for name in INPUTS:
        lines += ["  // " + manifest[name]["url"], "  // SHA-256: " + manifest[name]["sha256"]]
    lines += ["",
              "  /**",
              "   * @brief Inclusive code point range with one property value.",
              "   * @par Requirements",
              "   * J5-061; J5-062; ADR 0006 (issue #187).",
              "   */",
              "  struct unicode_value_range",
              "  {",
              "      std::uint32_t first;",
              "      std::uint32_t last;",
              "      std::uint8_t value;",
              "  };",
              "",
              "  /**",
              "   * @brief A property value name and its value (a category bit mask or a script number).",
              "   * @par Requirements",
              "   * J5-061; ADR 0006 (issue #187).",
              "   */",
              "  struct unicode_value_name",
              "  {",
              "      std::string_view name;",
              "      std::uint32_t value;",
              "  };",
              "",
              "  /**",
              "   * @brief IDNA2008 derived property values stored in c_idna_ranges (RFC 5892 section 3).",
              "   * @par Requirements",
              "   * J5-062; ADR 0006 (issue #187).",
              "   */",
              "  enum class idna_property : std::uint8_t",
              "  {",
              "    pvalid = 0,",
              "    contextj = 1,",
              "    contexto = 2",
              "  };",
              "",
              "  /**",
              "   * @brief Joining_Type values stored in c_joining_type_ranges (U is implicit).",
              "   * @par Requirements",
              "   * J5-062; ADR 0006 (issue #187).",
              "   */",
              "  enum class joining_type : std::uint8_t",
              "  {",
              *[f"    {name.lower()}_type = {number}," for number, name in enumerate(JOINING_TYPES)],
              "  };",
              "",
              '  inline constexpr std::string_view c_schema_unicode_version = "17.0.0";',
              f"  inline constexpr std::uint32_t c_script_count = {tables['script_count']}U;",
              "",
              "  // Keep one entry per line for reproducible generation and review.",
              "  // clang-format off"]
    category_comment = ", ".join(f"{number} {name}" for number, name in enumerate(tables["categories"]))
    table(lines, "c_general_category_ranges", "General_Category of every assigned code point (value: category number).",
          "unicode_value_range", [f"{{0x{f:06X}, 0x{l:06X}, {v}U}}" for f, l, v in tables["gc_ranges"]],
          "Sorted, disjoint; unlisted code points are Cn. Numbers: " + category_comment + ".")
    table(lines, "c_general_category_names", "General_Category value names and aliases with their category bit masks.",
          "unicode_value_name", [f'{{"{n}", 0x{m:08X}U}}' for n, m in tables["gc_names"]],
          "Sorted by name for binary search; group names (L, LC, M, N, P, S, Z, C) set several bits.")
    table(lines, "c_script_ranges", "Script of every code point listed in Scripts.txt (value: script number).",
          "unicode_value_range", [f"{{0x{f:06X}, 0x{l:06X}, {v}U}}" for f, l, v in tables["script_ranges"]],
          "Sorted, disjoint; unlisted code points have the script Unknown (Zzzz).")
    table(lines, "c_script_names", "Script names and aliases with their script numbers.",
          "unicode_value_name", [f'{{"{n}", {v}U}}' for n, v in tables["script_names"]], "Sorted by name for binary search.")
    table(lines, "c_joining_type_ranges", "Joining_Type D, L, R, T and C (value: joining_type).",
          "unicode_value_range", [f"{{0x{f:06X}, 0x{l:06X}, {v}U}}" for f, l, v in tables["joining_ranges"]],
          "Sorted, disjoint; unlisted code points are U (non-joining).")
    table(lines, "c_virama_ranges", "Code points with Canonical_Combining_Class 9 (Virama).",
          "unicode_value_range", [f"{{0x{f:06X}, 0x{l:06X}, 0U}}" for f, l, _ in tables["virama_ranges"]],
          "Sorted, disjoint.")
    table(lines, "c_idna_ranges", "IDNA2008 derived property PVALID, CONTEXTJ or CONTEXTO (value: idna_property).",
          "unicode_value_range", [f"{{0x{f:06X}, 0x{l:06X}, {v}U}}" for f, l, v in tables["idna_ranges"]],
          "Sorted, disjoint; unlisted code points are DISALLOWED or UNASSIGNED (RFC 5892 section 3).")
    lines += ["", "  // clang-format on", "} // namespace adaptive_json5::detail", "", f"#endif /* {guard} */", ""]
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--output", type=Path, default=OUTPUT)
    parser.add_argument("--check", action="store_true", help="fail when the header differs from a fresh build")
    arguments = parser.parse_args()
    manifest = verify_inputs()
    tables = build()
    text = render(tables, manifest)
    if arguments.check:
        current = arguments.output.read_text(encoding="utf-8") if arguments.output.is_file() else ""
        if current != text:
            print(f"{arguments.output} is stale; run tools/generate_schema_unicode_tables.py")
            return 1
        return 0
    arguments.output.parent.mkdir(parents=True, exist_ok=True)
    with arguments.output.open("w", encoding="utf-8", newline="\n") as output:
        output.write(text)
    for key in ("gc_ranges", "gc_names", "script_ranges", "script_names", "joining_ranges", "virama_ranges",
                "idna_ranges"):
        print(f"{key}: {len(tables[key])}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
