# Third-party conformance fixtures

These files are unmodified copies of external test corpora, used only as input
data for host GoogleTest units. They are not compiled into, linked with or
installed with the parser. They keep their own MIT licenses; the project's
Apache-2.0 license does not apply to them. `.gitattributes` marks this tree
`-text` so line endings and bytes are never normalized.

| Folder | Upstream | Commit | Archive SHA256 | License | Included |
| --- | --- | --- | --- | --- | --- |
| `json5-tests/` | https://github.com/json5/json5-tests | `ceb24d4080137d70833f86c25659c1331b80a387` | `30d041035ce29dc4f01d1836b7885c89689fc1bdee4c384bc2bc113c737c0a28` | MIT, Copyright (c) 2012-2016 Aseem Kishore, and others (`json5-tests/LICENSE.md`, `LICENSES/json5-tests-MIT.txt`) | The complete repository |
| `JSONTestSuite/` | https://github.com/nst/JSONTestSuite | `1ef36fa01286573e846ac449e8683f8833c5b26a` | `32ab2fb37cc26679dbd9164dbb7053bcef10134643d53cb0cef45de7680dcdcc` | MIT, Copyright (c) 2016 Nicolas Seriot (`JSONTestSuite/LICENSE`, `LICENSES/JSONTestSuite-MIT.txt`) | `test_parsing/`, `LICENSE`, `README.md` only; the benchmarked parsers and results are omitted |
| `JSON-Schema-Test-Suite/` | https://github.com/json-schema-org/JSON-Schema-Test-Suite (release `Test-JSON-Schema-Acceptance-1.041`) | `245a45abc90190c0bff04d97e6d2b541b4bb9f12` | `b60bdf58fd414e9b86c8be70b03e3b2d596b8bc3bc822a2083d86041cea4b20e` | MIT, Copyright (c) 2012 Julian Berman (`JSON-Schema-Test-Suite/LICENSE`, `LICENSES/JSON-Schema-Test-Suite-MIT.txt`) | `tests/draft7/`, `remotes/`, `LICENSE`, `README.md` only; other drafts, output and annotation tests are omitted |

Archives are `https://github.com/<owner>/<repo>/archive/<commit>.tar.gz`. Every
included file is byte-identical to the archive.

## Expectation conventions

- json5-tests: `.json` is valid JSON and JSON5, `.json5` is valid JSON5 only,
  `.js` and `.txt` must be rejected. Files under `todo/` are upstream work in
  progress and `misc/` holds non-fixture material.
- JSONTestSuite `test_parsing/`: `y_` must be accepted, `n_` must be rejected and
  `i_` is implementation-defined.

The upstream i_ classification is preserved. Separately, the project's
`c_project_decisions` table in `tests/validation/conformance_tests.cpp` pins all
35 i_ outcomes in each syntax mode to the [support statement](../../../docs/conformance.md).
The inventory unit rejects missing, duplicate or obsolete classifications, and
project-outcome units assert rejection codes/offsets or warning counts/spans.
JSON5 n_ cases remain clean-completion cases because many are valid JSON5.

- JSON-Schema-Test-Suite `tests/draft7/`: each test's `valid` is the expected verdict.
  Required files are all run; optional files run when they need only advertised
  capabilities (the asserted `format` subset, bignum, ECMAScript and non-BMP regex,
  unknown keywords, `$id`). Skipped optional files: `content.json` (content assertion is
  unsupported), `cross-draft.json` (needs 2019-09 processing), and the `format` files
  `idn-email`, `idn-hostname`, `iri`, `iri-reference`, `uri-template` (not asserted).
  `remotes/` (draft-07-compatible files) are registered offline under
  `http://localhost:1234/`, and the Draft-07 meta-schema under its own URI.
  At this revision: 747 required and 568 optional tests run.

## Recorded deviations from upstream labels

The specification decides the expected outcome, not the corpus. Where an
upstream label contradicts it, the validation test overrides that one case:

| File | Mode | Upstream label | Required outcome | Reason |
| --- | --- | --- | --- | --- |
| `json5-tests/comments/irregular-block-comment.json` | strict | valid JSON (`.json`) | reject | Contains a block comment; RFC 8259 has no comments. Accepted in JSON5 mode. |
| `JSON-Schema-Test-Suite/tests/draft7/optional/format/ecmascript-regex.json` test 0/0 (`"\a"`, `format: regex`) | Draft-07 | invalid | valid | `` is invalid only with the ECMA-262 `u` flag; the pattern engine follows Annex B (owner decision, ADR 0006 #48 section), where an unknown identity escape matches the letter. Optional test. |

## CMake acquisition

`cmake/TestFixtures.cmake` runs only when validation tests are enabled. It resolves
each set in this order:

1. `ADAPTIVE_JSON5_JSON5_TESTS_SOURCE_DIR` or `ADAPTIVE_JSON5_JSONTESTSUITE_SOURCE_DIR`,
   when set, for offline checkouts.
2. The committed folder here.
3. Otherwise FetchContent downloads the pinned archive, checks the SHA256 and
   extracts it into the build tree `_deps`. The source tree is not modified.

Test targets receive the roots through `adaptive_json5_use_conformance_fixtures()`
as `ADAPTIVE_JSON5_JSON5_TESTS_DIR` and `ADAPTIVE_JSON5_JSONTESTSUITE_DIR`
(the latter points at `test_parsing`).

To update a set, replace the folder with the new commit's files, then update the
commit, archive URL and SHA256 here, in `cmake/TestFixtures.cmake` and in
`docs/dependencies.md`.
