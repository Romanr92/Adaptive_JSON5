# Unicode table inputs

The Unicode data files and Unicode 17.0.0 ReadMe are unchanged upstream inputs.
Their source URLs and SHA-256 hashes are recorded in [sources.json](sources.json).
The Unicode License V3 snapshot retrieved on 2026-10-13 is preserved unchanged
under [LICENSES](../../LICENSES/Unicode-License-V3.txt). The generated header also
embeds the complete copyright and permission notice.

Regenerate the authorized production table header offline from the repository root:

```sh
python tools/generate_unicode_tables.py
```

The generator verifies input hashes, expands paired UCD First/Last records,
unions category memberships, and merges adjacent intervals. No Python, network,
Unicode file reader, locale or dynamic allocation enters the compiled tables.

Identifier-start categories are Lu/Ll/Lt/Lm/Lo/Nl; additional continuation
categories are Mn/Mc/Nd/Pc. Each identifier table combines Unicode 3.0.0 and
17.0.0 memberships to preserve ES5's historical category obligations while
supporting later additions. `$`, `_`, ZWNJ and ZWJ remain explicit additions in
the classifier, rather than artificial Unicode category memberships.

`c_space_separator_ranges` contains Unicode 17.0.0 Zs. The separate
`c_unicode_3_space_separator_ranges` retains Unicode 3.0.0 Zs, including U+200B,
for the later whitespace implementation to apply the historical baseline
explicitly. Generation does not implement the whitespace policy or classifiers.

## Schema component tables (issue #187)

`python tools/generate_schema_unicode_tables.py` builds
`core/includes/schema/unicode_properties.hpp` for the optional schema library only (ADR 0006,
issue #187 section): General_Category and Script of every code point with their ECMA-262
`\p{…}` names and aliases, Joining_Type, Virama (Canonical_Combining_Class 9) and the
IDNA2008 derived property. Inputs are the unchanged UCD 17.0.0 files `UnicodeData.txt`,
`PropertyValueAliases.txt`, `Scripts.txt`, `DerivedCoreProperties.txt`, `PropList.txt`,
`DerivedNormalizationProps.txt`, `HangulSyllableType.txt` and
`extracted/DerivedJoiningType.txt`, pinned in [sources.json](sources.json).

IANA publishes derived IDNA2008 tables only up to Unicode 12.0.0, so the generator computes
the derived property with the RFC 5892 section 3 algorithm. The unchanged IANA table
`idna-12.0.0/idna-tables-properties.csv` (retrieved 2026-11-27) is used only by
`tools/tests/test_generate_schema_unicode_tables.py`, which checks that the derivation gives
IANA's value for every code point assigned in Unicode 12.0.0. `--check` fails when the
checked-in header is stale. `.gitattributes` keeps these files byte-exact (`-text`) so the
SHA-256 checks also hold on Windows checkouts.