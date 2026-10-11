# Architecture and implementation boundaries

The current scaffold contains CMake development metadata and GoogleTest infrastructure, with no production C++ or parsing API. The
implementation issues define contracts before selecting storage and allocation
details. The following boundaries guide that work.

| Component | Planned responsibility | Dependency boundary |
| --- | --- | --- |
| `include/adaptive_json5`, `src` | JSON5/strict JSON, owned values, exact numbers, errors and resource accounting | C++17 only; no filesystem, network or application model |
| `schema` | Optional Draft-07 compilation, registered references and bounded validation | Parser/value API; explicit schema registry; no implicit I/O |
| `cli` | Native host input/output and stable exit/diagnostic protocol | Optional parser/schema adapters; host-only |
| `editor` | VS Code language support, schema assistance and live document versions | Host-only providers/backend; never a C++ runtime dependency |
| `examples` | Small generic consumers and optional external profile/schema examples | Public API only; no imported consumer runtime |
| `tests` | Deterministic units and pinned fixture data | Build optional; no runtime consumer dependency |

The reader accepts explicit bytes and length. A successful result owns its data;
invalid input returns a structured error. Preserve decoded UTF-16 units losslessly
and distinguish checked UTF-8 conversion. Preserve exact number spelling/value
before any requested destination conversion. Preserve duplicate members by
default and expose an explicit rejection policy.

The schema boundary rejects unsupported instance domains separately from syntax.
Exact numeric and deep-equality behavior must agree across runtime and editor
adapters. A provisional editor tree must never be a successful runtime parse.
CLI I/O and schema loading occur outside pure parse/evaluate operations.

## Decisions still to make

- Fallible allocator interface, owned document layout and view lifetimes.
- Stable result/error API and minimal reporting under allocation failure.
- Lossless string representation and exact finite-number operations.
- Named budget accounting/defaults and bounded cleanup/traversal.
- Unicode table source/version and reproducible generation.
- Independent or integrated schema engine, regex behavior and provenance.
- Editor backend/provider approach and supported host versions.

Record these decisions in focused ADRs as their foundation issues are implemented.
No framework, dependency engine, public parse signature or consumer policy is
silently selected by this scaffold.

Embedded portability is a core requirement: see [J5-101 and port boundary](embedded.md). Keep hosted standard-library/OS assumptions out of the production design; caller storage and the required runtime subset must be reviewed before selecting containers or schema dependencies.
