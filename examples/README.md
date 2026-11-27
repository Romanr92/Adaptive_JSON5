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
intentional and must not be auto-corrected. `manifest.json` (format_version 2) records the
expected parse, validation and exit outcome of each case, machine-checked observations
(`expect.checks`), and a starter map from every J5 requirement to example inputs.

## Command-line executable

`adaptive_json5_examples` (built with `ADAPTIVE_JSON5_BUILD_EXECUTABLE=ON`, source in
[`cli/`](../cli/README.md)) uses the real parser and the optional schema library:

```sh
adaptive_json5_examples run --manifest examples/manifest.json
adaptive_json5_examples inspect --syntax json5 examples/valid/features.json5
adaptive_json5_examples inspect --syntax json examples/valid/strict.json
adaptive_json5_examples inspect --syntax json5 --duplicates reject examples/valid/duplicates.json5
adaptive_json5_examples validate --syntax json5 --schema examples/schema/device.schema.json examples/schema/device-valid.json5
adaptive_json5_examples run --manifest examples/manifest.json --diagnostics json
```

`inspect` shows ordered values with positions, raw number spellings, checked int64,
uint64 and double conversions (value or failure reason) and string warnings. Strict-JSON
output never exports lone surrogates as invalid UTF-8 or NaN/Infinity as number tokens: it
uses `{"$utf16": [...]}` and, for bytes that are not UTF-8, `{"$bytes": [...]}`; NaN and
Infinity keep their spelling as a string and report the double conversion as `non_finite`. `run` evaluates every case like `validate`, compares parse,
validation and exit expectations, runs each structured check, and reports PASS, FAIL,
UNSUPPORTED or ERROR per case and a status per requirement (demonstrated, failed,
unsupported, or not demonstrated by file cases). Expected malformed inputs pass when
they are correctly rejected.

Exit codes: `validate` and `inspect` return 0 for valid input, 1 for invalid input or a
failed schema assertion, 2 for configuration, I/O or operational failures. `run` returns 0
when every selected supported expectation matched, 1 on a mismatch and 2 when the
manifest or an input cannot be processed. Unavailable capabilities (for example schema
cases in a build without the schema component) are reported as UNSUPPORTED and never
count as passing.

Structured checks (`expect.checks[].check`): `same_string`, `string_units`,
`utf8_export`, `warnings`, `member_names`, `duplicate_rejection`, `number_spelling`,
`conversion` and `absent_member`; each keeps its prose as `description`.

A handful of files cannot demonstrate all contracts: ownership after destroying
input, allocation failure, limits, concurrency, all grammar productions/schema
keywords, editor behavior and embedded compatibility need dedicated harnesses,
GoogleTest units or build/documentation evidence. The manifest marks these planned
routes honestly. Extend case-specific assertions and inputs during implementation;
the broad starter mappings are not complete requirement coverage.

The `adaptive_json5_example_files` CMake target lists the data without compiling
or executing production code.
GoogleTest, the example programs and the host demonstration executable must stay
outside embedded parser targets.
