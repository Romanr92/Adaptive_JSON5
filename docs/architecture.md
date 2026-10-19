# Architecture and implementation boundaries

For a source-based map of the current lexer, owned document, budgets and P033
helpers, see [Current parser architecture](current-architecture.md), including
Mermaid class diagrams and interaction flows. The preparation-stage descriptions
below retain the original contracts; their statements about which components
exist are historical, not a current implementation inventory.

The repository contains CMake development metadata, GoogleTest infrastructure
and the developer's result/error, input-view and syntax-options headers. No
parsing operation is implemented.
The implementation issues define contracts before selecting storage and
allocation details. The following boundaries guide that work.

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

## Input and syntax-options contract

`input_view` and `parse_options` define the public input/configuration boundary
for Issue #1, J5-004 and J5-005. They describe input and syntax selection; they
do not implement parsing, bounded cursor reads or grammar acceptance.

- `input_view::data` is a borrowed `const char*`; `size` is the exact byte count.
  No terminating NUL is required. A zero byte is part of the supplied input,
  not a sentinel or permission to truncate it. Its syntax validity is determined
  later by the selected language and its position.
- A nonzero size requires a non-null pointer to at least that many readable
  bytes. The caller owns the storage and keeps it alive and unchanged while an
  operation reads it. Portable C++ cannot validate arbitrary dangling pointers
  or inaccessible caller memory; these are buffer-lifetime preconditions.
- A zero-size description can use a null or non-null pointer. Default
  construction yields `{nullptr, 0}`. Empty input is a valid buffer description,
  but the future parser reports missing-value `syntax` failure rather than success.
- A reader consumes only the supplied byte range, without modifying it or
  reading beyond it. `strlen`, NUL scanning and mutable input access are not
  part of the parsing boundary. Complete parsing must consume one root followed
  only by permitted trivia; prefix acceptance is not a success contract.
- `parse_options{}` selects `syntax_mode::json5`. Callers can explicitly select
  `json5` or `strict_json`. No additional nonstandard extension mode is exposed;
  the declaration does not establish implementation of either language.
- The result's state is selected explicitly with `success(...)` or
  `failure(...)`. Neither value-bearing results nor `status_result` offer public
  default construction. Input/options defaults do not imply a successful parse.

Declaration-level units cover empty descriptions, const non-NUL-terminated
buffers, explicit prefix lengths, embedded zero bytes, borrowed storage and
default/explicit syntax selections. Parsing immutable/truncated inputs, byte
validation, complete consumption and mode-dependent syntax are verified by
subsequent parser units. Invalid-length pointers are not dereferenced in these
declaration tests.

## Recoverable-error contract

This section specifies the public error behavior for Issue #1 and J5-003,
J5-043, J5-045 and J5-051. P002 implements the result/error containers; the
operation-specific behavior below is a contract for subsequent implementation.

Library operations return explicit `result<T, error>` or `status_result`
success/error outcomes. Enabling compiler exceptions does not introduce a
throwing API, select a different recovery path or change the documented outcome.
The same input, options, storage behavior and resource limits must produce the
same success/failure classification and error information in exceptions OFF/ON
builds. Zed's build/launch actions select OFF; this does not change the library
contract when a compatible consumer enables compiler exceptions.

| Recoverable failure | Minimal category | Implementation/verification boundary |
| --- | --- | --- |
| Invalid grammar, missing value or incomplete input | `syntax` | Parser behavior and grammar units are pending |
| Malformed UTF-8 input | `encoding` | Decoder and parser units are pending |
| Duplicate name rejected by an explicitly selected policy | `duplicate_policy` | Duplicate-policy behavior is pending |
| Wrong value type or unavailable value/container access | `wrong_type_or_access` | Value/access operations are pending |
| Requested numeric destination cannot represent the value | `numeric_conversion` | Conversion operations are pending |
| Configured resource budget or limit is exhausted | `resource_limit` | Resource accounting and boundary units are pending |
| Caller-provided fallible storage cannot fulfill an allocation | `allocation_failure` | Storage implementation and failure-injection units are pending |

Ordinary invalid input and the recoverable failures above must not throw,
assert or abort. Failed operations publish no successful partial document or
conversion value, release their temporary resources and leave reusable contexts
in a documented valid state. Cleanup and reuse behavior must be verified when
the corresponding operations exist; the result container alone does not prove it.

Minimal errors contain a stable code and optional original zero-based byte
location. A failure at byte zero is distinguished from a failure without a
location by `has_byte_offset`; absent locations store an unused zero offset.
End-of-input diagnostics use the valid end offset. Constructing and returning
minimal allocation/resource errors must require no additional successful
allocation. Rich messages, paths and diagnostic ranges are optional additions
and cannot replace or obscure the minimal error if their construction fails.

Result inspection is non-throwing: `has_value()` reports the active state,
`value_if()` returns a value pointer or null, and `error_if()` returns an error
pointer or null. No throwing value accessor or wrong-alternative termination
path is part of this API. Pointers refer to result-owned storage and must not
outlive it. Custom result payload/error types must meet the documented
non-throwing constraints; minimal error types must additionally honor the
allocation-free reporting contract. Type traits alone do not establish that
arbitrary custom operations never allocate.

Storage callbacks and required runtime facilities must fulfill their documented
contracts. Unsupported throwing callbacks or runtime termination paths are not
represented as recoverable library outcomes. The core must use fallible storage
operations; exception handling around an infallible global-heap operation is not
an alternative implementation of the allocation-failure contract.

P002's 40 result cases and three configuration cases passed locally with
exceptions OFF/ON and RTTI OFF; see [verification notes](verification.md).
These establish result-container behavior and selected compiler settings.
Invalid-input, allocation-exhaustion, conversion/access and resource-limit
runtime recovery are verified by later units as their implementations arrive.

## Public ownership, value and policy contracts

This section completes Issue #1's public-contract specification scope for
J5-008. The semantic obligations come from the active requirements. Except for
the existing input/options and result/error declarations, the value, storage,
conversion and parser APIs described below are planned, not implemented.

### Ownership and views

Under J5-028–031 and J5-040–042, a successful parsed document owns its values,
decoded strings/member names and original number spellings independently of
the input buffer. Caller input can be released or overwritten after parsing
returns without changing a successful document. Source text retained for
tooling must have explicit ownership and an accounted, configurable storage cost.
Arrays and object-member enumeration retain source order; names and strings
are length-aware, including embedded zero units.

Input views borrow caller storage only during the reading operation. Pointers
returned by result inspection borrow the result's active alternative and must
not outlive that storage. For future document views, the owning-document
lifetime, move/destruction effects and any invalidation rules must be defined
before those views are exposed. No pointers into temporary parsing buffers may
escape as document data. Address stability across document moves is **Draft**;
it is not inferred from the existing result's move constructor.

### Duplicate policy

J5-031–033 specify preserve-all as the generic default. Every member remains
observable in source order, including repeated decoded names. An explicit
reject-duplicates policy reports the colliding occurrences and returns failure
before publishing a successful document. Names compare by exact decoded
content, without normalization, case folding or hash-only equality. Convenience
lookup must explicitly report ambiguity rather than silently choosing or
concealing a preserved duplicate.

The duplicate-policy declarations and lookup signatures are **Draft**. Current
`parse_options` contains only syntax selection; no duplicate-policy field or
working duplicate parser is present yet.

### Numbers, conversions and checked access

J5-034–039 require owned original number spelling and an exact finite-value
representation. Mathematical integrality is separate from lexical spelling;
eager conversion through `double` must not determine integer acceptance.
Integer conversion reports wrong type, non-integral value and destination
overflow explicitly, without rounding or wrapping. Booleans and non-finite
values are not silently treated as integers.

Floating conversion is explicit and locale-independent, preserves the stored
number and documents rounding, overflow and underflow outcomes. Finite tokens
that overflow a requested destination remain distinct from explicit Infinity
or NaN, and negative-zero sign/spelling is retained. The exact representation,
destination-specific policies and detailed conversion-reason API are **Draft**;
the current minimal `numeric_conversion` category alone does not specify every
conversion sub-outcome.

J5-018 and J5-029 require lossless decoded UTF-16 units, including unpaired
surrogates, and checked UTF-8 export that fails rather than replacing unsupported
units. Wrong-type access, absent members, ambiguous duplicates and out-of-range
indices return explicit outcomes. These string/value access and conversion APIs
are pending; current checked access applies to result alternatives only.

### Storage, limits and cleanup

J5-046–049 and J5-101 require caller-supplied fallible storage and explicit
budgets for library-owned allocations, with no implicit global-heap or OS
dependency. Storage must remain available until all associated owned objects
have been destroyed and their allocations released. Allocator declarations,
alignment/capacity accounting and the exact runtime subset are **Draft** and
are developed in the storage foundation.

The named limits cover input bytes, nesting, nodes/members, decoded string
units, numeric token/exponent lengths, owned allocation bytes and diagnostic
count. Limits and option combinations are validated before processing;
arithmetic is checked, and data is never silently truncated to fit a budget.
Parsing, conversion, comparison and error cleanup must remain within stack/work
budgets, including cleanup of partially built deep data. Exhaustion reports
`resource_limit` or `allocation_failure` as appropriate, rather than a syntax
failure. Long operations use documented cancellation or deterministic work
budgets; their exact public representation remains **Draft**.

Concrete limit defaults and precise charging/reset rules are **Draft**. No
unlimited or arbitrary numeric default is implied, and the current
`parse_options` has no limit fields. These choices must be settled and tested
before the corresponding processing API is advertised as supported.

### Thread-safety and deterministic behavior

J5-050–052 require independent parser contexts/documents to operate concurrently
without shared mutable global state. Sharing a mutable work context requires
caller synchronization unless a later API explicitly supports it. A shared
storage provider must satisfy its own concurrency contract; the core does not
add OS locks or threads to make an unsafe provider safe.

Existing result/input/options objects follow ordinary C++ synchronization rules:
read-only inspection must not race with mutation, moving, destruction or changes
to borrowed bytes/payloads. Mutable `value_if()` access supplies no synchronization.
Future same-document concurrent access and context reuse/invalidation details
are **Draft** until their declarations and implementation are available.

Parsing and conversion do not depend on process locale, clocks, environment or
hidden global settings. Identical input/options, storage outcomes and documented
data versions produce equivalent values and stable primary error categories.
Concurrency and deterministic processing units follow those implementations;
current result units do not prove parser concurrency or device execution.

## Decisions still to make

- Fallible allocator interface, owned document layout and view lifetimes.
- Rich diagnostics layered on the implemented minimal result/error foundation.
- Lossless string representation and exact finite-number operations.
- Named budget accounting/defaults and bounded cleanup/traversal.
- Unicode table source/version and reproducible generation.
- Independent or integrated schema engine, regex behavior and provenance.
- Editor backend/provider approach and supported host versions.

Record these decisions in focused ADRs as their foundation issues are implemented.
No framework, dependency engine, public parse signature or consumer policy is
silently selected by this scaffold.

Embedded portability is a core requirement: see [J5-101 and port boundary](embedded.md). Keep hosted standard-library/OS assumptions out of the production design; caller storage and the required runtime subset must be reviewed before selecting containers or schema dependencies.
