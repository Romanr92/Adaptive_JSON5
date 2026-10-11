# JSON5 demonstration fixtures

These original project examples are Apache-2.0 repository material. Valid JSON5
and strict JSON, intentional invalid inputs and optional Draft-07 schema examples
are stored here. File extensions label the intended syntax; malformed files are
intentional and must not be auto-corrected. `manifest.json` records planned parse,
validation and inspection outcomes, plus a map covering every J5 requirement.
No executable exists yet, and fixture presence is not verified conformance.

## Planned command-line executable

The future optional host target is `adaptive_json5_examples`. It uses the real
parser and optional schema library; it must not reimplement parsing. Proposed
commands (not available yet):

```sh
adaptive_json5_examples run --manifest examples/manifest.json
adaptive_json5_examples inspect --syntax json5 examples/valid/features.json5
adaptive_json5_examples inspect --syntax json examples/valid/strict.json
adaptive_json5_examples inspect --syntax json5 --duplicates reject examples/valid/duplicates.json5
adaptive_json5_examples validate --syntax json5 --schema examples/schema/device.schema.json examples/schema/device-valid.json5
adaptive_json5_examples run --manifest examples/manifest.json --diagnostics json
```

`inspect` shows ordered owned values, raw number spellings, checked conversions,
source spans, warnings and explicit result/error categories. Never export lone
surrogates as invalid UTF-8 or NaN/Infinity as illegal strict-JSON number tokens;
use tagged escaped diagnostic representations. `run` compares observed outcomes
with expectations and reports pass/fail/unsupported per case and requirement.
Expected malformed inputs count as passing demonstrations when correctly rejected.
Exit 0 means every selected supported case matched, 1 a mismatch/invalid direct
input, and 2 configuration/I/O/resource failure. Unavailable optional capabilities
must be reported explicitly, never silently counted as passing.

A handful of files cannot demonstrate all contracts: ownership after destroying
input, allocation failure, limits, concurrency, all grammar productions/schema
keywords, editor behavior and embedded compatibility need dedicated harnesses,
GoogleTest units or build/documentation evidence. The manifest marks these planned
routes honestly. Extend case-specific assertions and inputs during implementation;
the broad starter mappings are not complete requirement coverage.

The `adaptive_json5_example_files` CMake target lists the data without compiling
or executing production code. Future executable work is tracked separately. No
new CI category is introduced: automated quality checks remain unit tests and
formatting. GoogleTest and the host demonstration executable must stay outside
embedded parser targets.
