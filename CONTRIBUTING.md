# Contributing

Start with [development setup](core/docs/design-development.md), the ordered
[implementation sequence](core/docs/requirements-json5-implementation-order.md) and [requirements](docs/requirements.md).
Pick a ready issue whose prerequisites are complete. Keep the generic parser
independent of consumer fields, operating systems and optional host tooling.

Follow the [C++ coding style](core/docs/design-coding-style.md), including the `m_` prefix
without a trailing underscore for class data members, and the tracked
clang-format rules.

Discuss public API, ownership, exact-number, Unicode and allocation choices in
the relevant issue before building dependent features. Record durable decisions
in `docs/adr/`; preserve stable requirement IDs when contracts change.

For each implementation change:

1. State the behavior and requirement IDs it implements.
2. Add meaningful unit cases for normal behavior, boundaries and explicit errors.
3. Run the unit suite with exceptions OFF and RTTI OFF, then check clang-format 18.
4. Retain named unit mappings/results and update essential API/ownership/error/limit,
   provenance and usage contracts where behavior changes. No per-issue documentation
   report or implementation narrative is required.
5. Submit a focused pull request using the template.

The automated quality checks are unit tests, validation tests, formatting, sanitizer
runs, fuzzing, branch coverage, a second compiler (Clang) and static analysis
(clang-tidy and CodeQL), as described in the
[verification strategy](docs/ISO26262-safety/verification-strategy.md) and the
[cybersecurity verification](docs/ISO21434-cybersecurity/cybersecurity-verification.md).
CI also builds the example programs and checks their output, as a non-blocking job.
Do not add integration, end-to-end or benchmark gates without a later scope decision. Required fixture coverage belongs in unit
tests, with pinned versions, licenses and expected behavior.

Do not mark a Draft requirement Verified just because an issue closes. Record
the named test and executed result first. Do not claim full JSON5 or Draft-07
support while its production or keyword coverage matrix has gaps.

Original contributions use the repository's Apache-2.0 license. Document the
source, immutable version and license of any dependency, generated data or
fixture before adding it. No contribution agreement or external project workflow
is introduced by this repository setup.

Read [licensing guidance](docs/licensing.md) and retain applicable upstream notices
when contributing copied or adapted material. Repository copyright attribution is
recorded in [NOTICE](NOTICE); third-party rights remain with their owners.

The [issue tracking section](core/docs/requirements-json5-implementation-order.md#delivery-scope-and-issue-tracking) groups consecutive P
steps without reducing technical acceptance. Romanr92 implements production C++.
AI agents can change build setup, documentation and unit tests, not production
code; unit tests stay independent of the implementation. Full
schema, minimal testing CLI and advanced accounting remain in the selected delivery.

## Project status and delivery details

These details used to be at the top of the README.

**Implementation status.** The compiled C++17 core parses complete JSON5 1.0.0 and strict
JSON documents through `parse_document()`, with exact numeric conversion and comparison,
warnings, line/column lookup and optional source retention (P001–P040). A simple API,
`json5_document` with `json5_value`, parses into a caller buffer and is reachable through
one umbrella header. The P041 technical review is recorded; final claim status and
required PR evidence are in the [JSON5 1.0.0 support statement](docs/conformance.md),
together with versions, limits, implementation-defined behaviour, deviations and executed
results. Requirement rows remain Draft. GoogleTest inventories are in
[traceability](docs/traceability.md), and the per-production view is the
[grammar evidence matrix](core/docs/design-grammar-evidence.md). Draft-07 validation, a
CLI and VS Code support are independently optional components.

**Delivery scope.** The [delivery scope](core/docs/requirements-json5-implementation-order.md#delivery-scope-and-issue-tracking)
retains full schema P043–P055, a minimal testing CLI P056–P059 and full advanced memory
accounting. Existing anchors group consecutive steps with at most three issues per
subsystem. Schema/CLI remain optional for consumers, while required in this delivery
scope. No per-issue documentation report is required; essential public API, limits,
ownership, provenance and usage contracts remain. Ten working days is a checkpoint, not a
credible full-completion promise; the plan states estimate assumptions.

**Implementation sequence.** For core parser work, follow the
[official-specification implementation sequence](core/docs/requirements-json5-implementation-order.md).
P001–P066 are ordered so all prerequisites come earlier; existing J5 IDs remain unchanged
for issue and requirement traceability. Optional schema/editor/CLI work does not block
this core parser sequence. The sequential baseline includes optional schema P042–P055,
CLI/example P056–P059 and VS Code P060–P066. Core parser P001–P041 remains independent.
The [original proposal](core/docs/requirements-proposal-json5-parser-repository.md) is
preserved Draft input, superseded where later user decisions differ.

**Dependencies.** Native development requires no WSL. The parser requires neither
AdaptivePi, an operating-system service, Node.js, VS Code nor network access.
[Embedded requirements](docs/embedded.md) define the bare-metal boundary. AdaptivePi is
inspiration and an optional consumer example ([consumer example](docs/consumer-example.md)).

**Source layout.** The [source layout guide](core/docs/design-source-layout.md) and
[layout decision](core/docs/adr-0002-public-private-source-layout.md) separate public
`includes/` from private `core/includes/`, with files grouped by topic. Compiled
implementation files live under `core/src/<topic>/`. Internal units live under
`core/tests/`; public-header/API and whole-library behavior units stay under top-level
`tests/`. The former `include/` layout has been migrated. Internal declarations are not
installed, and public headers compile without the private include root.

**Examples.** The [examples folder](examples/README.md) contains small programs that
[parse a file](examples/parse_file.cpp), [read a value](examples/read_value.cpp) and
[handle an error](examples/handle_error.cpp). It also holds JSON/JSON5 inputs,
intentional failures, an optional schema and a planned requirement demonstration
manifest. Enable `ADAPTIVE_JSON5_BUILD_EXECUTABLE`, or use the `debug-example` preset,
to build the programs. The command-line executable will be implemented later.

**Package version.** The current CMake package version identifies the development
package, not a supported parser release.
