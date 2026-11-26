# ADR 0009: Command-line tool decisions (P056–P059)

Status: Decided, 2026-11-25 and 2026-11-26 (issue #57). Production C++ is written by the
human developer; this record does not claim implementation or verification. Requirement
statuses are kept in the requirements registry.

Context: issue #57, P056–P059 of the
[implementation sequence](requirements-json5-implementation-order.md). Related contracts
in [requirements](../../docs/requirements.md): J5-002, J5-007, J5-070, J5-071, J5-092,
J5-093, J5-094.

| # | Topic | Decision |
| --- | --- | --- |
| 1 | Shape | One host executable `adaptive_json5_examples` with `validate`, `inspect`, `run`, `capabilities`, `version` and `help` |
| 2 | Layout | Everything under `cli/`: private headers `cli/includes/cli/`, sources `cli/src/`, GoogleTest units `cli/test/` |
| 3 | I/O seams | All input, output and memory go through an injectable `environment` (reader, sink, storage callbacks) |
| 4 | Memory | Heap allowed (host only), through `malloc`-backed storage callbacks; no allocating std containers in the tool logic |
| 5 | Exit codes | validate/inspect 0/1/2; run 0 all supported matched, 1 mismatch, 2 operational |
| 6 | Diagnostics | Readable text and a versioned strict-JSON format with tagged values |
| 7 | Manifest | `examples/manifest.json` format_version 2 with machine-checked observations |
| 8 | Packaging | `ADAPTIVE_JSON5_BUILD_EXECUTABLE`, independent version, install component `cli`, presets `cli` and `cli-no-schema` |
| 9 | Verification | GoogleTest units, CTest smoke tests on the real executable, a local Python runner; MISRA, coverage and complexity cover `cli/` |

## Decision 1: one executable with commands

One small testing executable carries the validation CLI (P056–P057) and the
demonstration runner (P058), as the issue scope asks ("a deliberately small testing
executable"). Commands share argument parsing, readers and the diagnostic writer.
Rejected: a separate validation binary and examples binary (twice the packaging for the
same seams).

## Decision 2: layout (owner, 2026-11-25)

The tool lives entirely in `cli/`. The pure-memory unit test keeps scanning only `core/`
and `includes/`, so the host adapter may use `<cstdio>`. The units are in `cli/test/`
(owner decision D5; AGENTS.md lists it as a test location). The logic library
`adaptive_json5_cli_core` is built with the unit tests or the executable; it is never part
of the parser or schema libraries. Rejected: logic in `core/src/cli` with the adapters in
`cli/` (the owner chose one tree for the whole tool).

## Decision 3 and 4: seams and memory (owner, 2026-11-25)

`input_source` reads a file or standard input (`-`) into a bounded buffer and reports
`not_found`, `read_error` or `too_large`; `output_sink` writes to standard output or
error; `storage_callbacks` supply memory. `main.cpp` wires the real `<cstdio>` adapters
and `malloc`. The tool runs only on a PC, so heap use is allowed, but the logic allocates
only through the callbacks and writes output without allocating: an allocation failure is
always reported (exit 2; strict JSON with `--diagnostics json`) and can be injected in
units. Rejected: `std::string`/`std::vector` (with exceptions OFF an allocation failure
aborts the process, so the required out-of-memory fallback could not be guaranteed).

## Decision 5: exit codes (J5-070)

`validate` and `inspect`: 0 valid; 1 syntax, encoding, duplicate or secure-mode error, a
failed schema assertion, or an instance outside the schema domain; 2 argument,
configuration (including an invalid or unreadable schema), I/O, limit or allocation
failure. `run`: 0 when every selected supported expectation matched (intentionally invalid
inputs included), 1 on a mismatch, 2 when the manifest or an input cannot be processed. An
unavailable capability (a schema case in a build without the schema component) is
reported as UNSUPPORTED and never counts as passing. The tool has no cancellation source
of its own; a cancelled schema run would map to category `limit`, exit 2, and is reachable
only through the library.

## Decision 6: diagnostics (J5-071, owner D2)

Text output prints `<input>: <message>` with the library's `line: L, column: C` text; I/O
and operational errors go to standard error. `--diagnostics json` writes one object:
`format` (`adaptive-json5-diagnostics`, `-inspection`, `-run` or `-capabilities`),
`version` 1, the command and input, the diagnostics (severity, category, message, line,
column, byte offset, schema keyword and instance and schema pointers) and `result` and
`exit`. Values strict JSON cannot carry losslessly are tagged objects:
`{"$utf16": [...]}` for strings with unpaired surrogates and `{"$bytes": [...]}` for text that is not UTF-8 (for example a file
path). `{"$number": "Infinity"}` is reserved for non-finite numbers but not emitted: a
number is written as its spelling (a string) and `to_double` reports NaN and Infinity as a
`non_finite` failure, so no non-finite double reaches the output. Rejected: escaped lone surrogates and `"NaN"` strings (invalid Unicode for many
readers, and indistinguishable from real text).

## Decision 7: manifest observations (owner D3)

The manifest's prose observations became structured checks evaluated by `run`
(`same_string`, `string_units`, `utf8_export`, `warnings`, `member_names`,
`duplicate_rejection`, `number_spelling`, `conversion`, `absent_member`); the prose stays
as each check's `description`. Requirement rows report demonstrated, failed, unsupported
or "not demonstrated by file cases". Manifests are read with limits sized for them (1 MB),
independent of `--max-*`. Rejected: printing observations unchecked.

## Decision 8: packaging (P059)

The tool has its own version (`ADAPTIVE_JSON5_CLI_VERSION`, 0.1.0) in a configured header
with the library version and source revision; `version` and `capabilities` report them.
`cmake --install <build> --component cli` installs only `bin/adaptive_json5_examples`.
Presets: `cli` (with the schema component) and `cli-no-schema`. Parser and embedded
consumers never build it (`ADAPTIVE_JSON5_BUILD_EXECUTABLE` is OFF by default and fails
for cross builds).

## Decision 9: verification (owner, 2026-11-25 and 2026-11-26)

- GoogleTest units in `cli/test` with a fake host (in-memory files, captured output,
  failure-injecting storage), run in the unit-test builds.
- Smoke tests on the real executable: `cli/test/smoke_cases.json` lists the cases (the
  example files, the manifest and the tool's own inputs in `cli/test/expected/inputs`);
  CTest (`ctest --preset cli` / `cli-no-schema`, label `cli_smoke`) compares exit codes and
  exact output with `cli/test/expected`. The informative `cli` workflow runs both presets.
- `tools/cli_smoke.py` runs every built variant locally on the same cases and prints each
  case's output with a green PASSED or red FAILED verdict; it does not use CTest or
  GoogleTest.
- The complexity report, the MISRA C++:2023 scan and the coverage gate include `cli/`
  (owner, 2026-11-26).
