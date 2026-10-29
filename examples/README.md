# JSON5 examples

## Example programs

Three small programs use the simple `json5_document` API through public headers only.
Each keeps its parsed document in a caller buffer, so none of them needs the heap.

| Program | Input | Shows |
|---|---|---|
| `parse_file.cpp` | `data/parse_file.json5` | Read a file and parse it |
| `read_value.cpp` | `data/read_value.json5` | Read typed values, by member name and array index |
| `handle_error.cpp` | `data/handle_error.json5` | Report a parse error with its line and column |

Each program holds its input path as a `constexpr` constant relative to the repository
root, so run them from there. `data/handle_error.json5` is intentionally invalid.

```sh
cmake --preset debug-example
cmake --build --preset debug-example
build/debug-example/examples/adaptive_json5_example_parse_file
build/debug-example/examples/adaptive_json5_example_read_value
build/debug-example/examples/adaptive_json5_example_handle_error
```

Check all three against their expected output with `ctest --preset debug-example`. Each
test requires exit code 0 and output that matches `expected/<program>.txt` exactly, except
for the byte count of `parse_file`, which changes with internal layouts. CI runs the same
tests in the `examples` workflow.

## JSON5 demonstration fixtures

These original project examples are Apache-2.0 repository material. Valid JSON5
and strict JSON, intentional invalid inputs and optional Draft-07 schema examples
are stored here. File extensions label the intended syntax; malformed files are
intentional and must not be auto-corrected. `manifest.json` records planned parse,
validation and inspection outcomes, plus a map covering every J5 requirement.
The command-line executable below does not exist yet, and fixture presence is not
verified conformance.

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
or executing production code. Future executable work is tracked separately.
GoogleTest, the example programs and the host demonstration executable must stay
outside embedded parser targets.
