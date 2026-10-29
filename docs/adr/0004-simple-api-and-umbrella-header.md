# ADR 0004: Simple API, umbrella header and memory switch

Status: Accepted by the user, 2026-10-28. Not yet implemented; tracked in #93.

## Context

Consumers of the library take the public headers and the static library, include
them in their software and parse JSON5 documents. Today that needs several headers
and three caller-owned objects: storage callbacks, an `accounted_storage` that must
outlive every document, and `parse_options`
([memory, limits and failure behavior](../memory-and-limits.md)). Reading a value
then needs `value_view`, member lookup by decoded UTF-16 name and the conversion
functions. This is the right foundation for embedded and advanced use, but too much
for the common case.

Constraints that any simpler API must keep:

- The core library requires no implicit global heap and runs on embedded targets
  (J5-101). The heap-based one-call `parse` from #94 therefore lives in a separate,
  optional library, `AdaptiveJSON5::hosted`.
- Every failure is an explicit result, never an exception, in every compiler
  exception mode.
- Documents keep exact values, every duplicate member and every warning.

## Decision

### 1. One umbrella header that includes everything

`includes/adaptive_json5/adaptive_json5.hpp` includes every supported public header,
including the new simple API below, and nothing from the private include tree. A
consumer needs one `#include`. Advanced features (custom storage, limits, warning
policy, exact comparison and conversion options) are reachable through the same
include; the documentation says which parts are advanced.

The optional hosted header `adaptive_json5/hosted/parse.hpp` is **not** included,
because it is installed only when the hosted library is built (decision 4). Hosted
users add that include.

Rejected:

- **Simple API only in the umbrella:** a smaller surface and faster builds, but
  advanced use would need extra includes for every feature.
- **Including the hosted header when it is installed (`__has_include`):** what the
  umbrella declares would depend on the installation, and in a build tree the header
  exists even when its library is not linked.

### 2. A small class for the common case

A public class `json5_document` (header `adaptive_json5/json5_document.hpp`) owns a
parsed document, its warnings and its storage, and a lightweight `json5_value`
navigates it. The first version offers:

- **Parse and errors:** `json5_document::parse(text, buffer, options = {})`; readable
  error and warning text with line and column, built on `format_error`,
  `format_warning` and `locate`.
- **Navigation:** `root()`; member by UTF-8 name; element by index; size; kind checks.
  A member name that occurs more than once in an object is reported as an error
  instead of choosing one (J5-031).
- **Scalar reads:** `as_bool`, `as_int64`, `as_uint64`, `as_double`, and `as_string`
  writing UTF-8 into a caller buffer. Numeric scratch memory comes from the
  document's own storage.
- **Warnings:** count and access to every retained warning, such as raw U+2028/U+2029
  in strings (JSON5 specification section 5.2).

Every operation is `noexcept`, `[[nodiscard]]` where it returns a value, and
returns a `result`; a failed step in a chain passes its error through.

Rejected: **free functions** over the existing types. A thinner layer, but users
would still manage storage and ownership types themselves.

### 3. Memory: a caller buffer in the core, the heap as an option

- **Caller buffer (core library):** a public `buffer_arena`
  (`adaptive_json5/memory/buffer_arena.hpp`) turns a caller-supplied byte buffer into
  a storage provider. Its control block (the accounting and a bump pointer) sits at
  the start of the buffer, the rest is allocation space. It uses no heap and no
  global state, counts releases and resets when everything is released. A buffer
  that is too small reports `allocation_failure`. `json5_document::parse(text,
  buffer)` uses it.
- **Heap (hosted library):** `hosted::parse(text, options = {})` returns the same
  `json5_document`, with a heap block as its storage. One class serves both, so
  users learn one type whichever memory they use. `hosted_document` from #94 is
  replaced; there has been no release with it.
- `json5_document` holds its storage through a small owner that is either the arena
  in the caller's buffer or the heap block; moving a document never moves its
  accounting.
- Both paths build the document through one private friend,
  `detail::json5_document_access` (`core/includes/memory/`), which parses through
  the owner's accounting. The public header only forward-declares it, so it names no
  hosted function and gives users no way to construct a document from raw storage.

Rejected:

- **Heap only through the hosted library:** two libraries and a heap are then
  required, which rules out targets without a heap.
- **Heap inside the core:** breaks J5-101.
- **Separate document types for buffer and heap:** two overlapping APIs to learn and
  document.

### 4. The memory switch `ADAPTIVE_JSON5_BUILD_HOSTED`

The CMake option `ADAPTIVE_JSON5_BUILD_HOSTED` (default OFF) builds
`libadaptive_json5_hosted.a` (`AdaptiveJSON5::hosted`) and installs its header.

| | OFF (default) | ON |
|---|---|---|
| Libraries | `AdaptiveJSON5::parser` | `AdaptiveJSON5::parser` and `AdaptiveJSON5::hosted` |
| Memory | caller buffer or caller storage callbacks | additionally the heap through `hosted::parse` |
| Headers installed | all public headers except `hosted/` | all public headers |
| Suits | embedded and any target without a reliable heap | desktop and server applications |

The README documents the switch with a tutorial to activate and deactivate it: the
CMake command line, use as a subproject, the libraries to link, the extra include,
how to check the installation, and that an existing build directory keeps its cached
value.

### 5. Scope

All of the above is delivered in #93, whose description is updated to these
acceptance criteria. The JSON5 specification is not affected: nothing changes what
is accepted or rejected.

## Consequences

- The common case becomes one include, one library and a few calls, without the
  caller managing storage objects.
- The core gains two public components (`buffer_arena`, `json5_document` with
  `json5_value`) and a private UTF-8 member-name comparison that does not allocate.
- #94's `hosted_document` is replaced by `json5_document`; its units and the user
  guide section are restated.
- The umbrella header slows compilation for code that includes it; fine-grained
  headers remain available.
- Units: buffer arena (exhaustion, alignment, reset, accounting limit), document,
  values, messages, hosted parse; one test translation unit that compiles with only
  the umbrella include; a coverage test that fails when a public header (other than
  `hosted/`) is missing from the umbrella.
- Documentation: README with the umbrella as the default include, a short example
  and the memory switch tutorial; user guide section; traceability.

## Implementation order

1. This ADR.
2. `buffer_arena` and its units.
3. `json5_document` with buffer parsing and the storage owner.
4. `json5_value`: navigation, UTF-8 member lookup, scalar reads.
5. Error and warning text, warning access.
6. Hosted parse returns `json5_document`; `hosted_document` removed (after #108 is
   merged).
7. The umbrella header with its compile and coverage units.
8. README (default include, example, memory switch tutorial), user guide and
   traceability.
