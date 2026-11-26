# Command-line tool

`adaptive_json5_examples` is the optional host tool for P056–P059 (issue #57, ADR 0009).
It uses the real parser and, when built, the optional schema library; it never
reimplements parsing. It is host-only and never part of the parser or schema libraries.

| Path | Contents |
| --- | --- |
| `includes/cli/` | Private headers: I/O seams (`cli_io.hpp`), options, commands, reporter |
| `src/` | The logic library `adaptive_json5_cli_core`, plus `main.cpp` and the real file and console adapters (`host_io.cpp`) |
| `test/` | GoogleTest units (`adaptive_json5_cli_unit_tests`) with a fake host: in-memory files, captured output, failure-injecting storage |

Commands: `validate`, `inspect`, `run`, `capabilities`, `version`, `help`. See the
[examples README](../examples/README.md) for usage, output formats and exit codes.

All input, output and memory go through the injectable `environment` (reader, sink,
storage callbacks). The tool logic allocates only through those callbacks and writes
output without allocating, so an allocation failure is always reported (exit 2,
strict-JSON diagnostics with `--diagnostics json`).
