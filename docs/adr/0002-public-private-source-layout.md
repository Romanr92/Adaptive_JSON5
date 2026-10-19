# ADR 0002: Public headers, private headers and compiled implementation

Status: Accepted by the user, 2026-10-19. Physical migration implemented under
the user's explicit authorization to refactor production files; execution
evidence is recorded in [verification notes](../verification.md).
The [source layout guide](../source-layout.md) describes the implemented tree
and day-to-day build/test conventions, including the final `src/src/` location.

## Decision

Use `includes/` (plural) only for supported consumer-facing headers. Place
implementation-only headers under `src/includes/`. Group public headers,
private headers, implementation files and tests by topic. Use ordinary compiled
`.cpp` definitions for substantial non-template implementations rather than
continuing to grow a header-only parser.
The implementation topic folders live under `src/src/`; `src/` is the component
root containing its `includes/`, `src/` and `tests/` subtrees.

Internal component GoogleTest units live under `src/tests/`. Top-level `tests/`
contains public-header/API units and whole-library behavior units using only
the supported public interface. This user-approved amendment is part of the
layout decision; both groups remain unit tests, not new integration/end-to-end
quality gates.

This decision applies to new work and implementation guidance from now on.
The former `include/adaptive_json5/` tree has been replaced. Do not restore a
parallel header-only copy or duplicate implementation logic in the public tree.

## Target structure

```text
includes/
  adaptive_json5/
    parser.hpp                    # Future complete-document entry point
    input.hpp
    options.hpp
    result.hpp
    error.hpp
    memory/
      storage.hpp
      owned_block.hpp
      accounted_storage.hpp
      resource_limits.hpp
    value/
      document.hpp
      value_kind.hpp
      value_view.hpp
      value_iterator.hpp
      member_lookup.hpp
      decoded_string.hpp
      decoded_name_equal.hpp
      owned_number.hpp
      number_kind.hpp
      string_warning.hpp
      utf8_export.hpp
      value_index.hpp

src/
  includes/
    adaptive_json5/
      detail/
        common/
          checked_size.hpp
        input/
          cursor.hpp
        unicode/
          unicode_range.hpp
          unicode_tables.hpp
        lexer/
          trivia.hpp
          punctuation.hpp
          string/
          identifier/
          number/
        parser/
          parser_engine.hpp
          parser_frame.hpp
          parser_stack.hpp
          parser_dispatch.hpp
          primitive_parser.hpp
        memory/
          resource_budget.hpp
          work_budget.hpp
        value/
          document_links.hpp
          value_record.hpp
          container_links.hpp
          member_lookup.hpp
  src/                            # Compiled implementation topic folders
    input/
    unicode/
    lexer/
      string/
      identifier/
      number/
    parser/
      parser_engine.cpp
      parser_dispatch.cpp
      primitive_parser.cpp
    memory/
    value/
      document.cpp
      value_view.cpp
      owned_number.cpp
  tests/                          # Internal component units
    common/
    input/
    unicode/
    lexer/
      string/
      identifier/
      number/
    parser/
    memory/
    value/

tests/                            # Public API and whole-library behavior units
  api/
  input/
  parser/
  memory/
  value/
  library/
```

This is the agreed organization, not an exhaustive file inventory or evidence
that these APIs have been implemented. Add files to the matching topic as
needed; do not create empty directories or one private header per function just
to reproduce the diagram. A helper needed by only one implementation file can
remain local to that `.cpp`. Optional schema, CLI, editor and examples retain
their separate component boundaries.

## Header and implementation rules

- Consumers include paths such as `<adaptive_json5/value/document.hpp>`;
  `includes/` is an include-search root, not part of the include spelling.
- Implementation files may additionally include paths such as
  `"adaptive_json5/detail/parser/parser_engine.hpp"`, resolved through
  `src/includes/`.
- Public headers must compile without access to `src/includes/` or source-tree
  relative paths. Their transitive dependencies must also be publicly available
  standard facilities or supported public headers.
- Private types and helpers are not supported consumer APIs. Do not install
  `src/includes/` or expose it as a public/transitive CMake include directory.
- Public declarations retain API, ownership/lifetime, error and limit contracts.
  Move implementation-only includes and substantial non-template function bodies
  to the corresponding `.cpp` files. Preserve signatures, namespaces and
  `noexcept`; remove ordinary free-function `inline` where a single out-of-line
  definition replaces the header definition.
- Keep template definitions and definitions required for consumer constant
  evaluation available in public headers when they belong to the public API.
  Small inline operations may remain there. Private templates stay in private
  headers. Separation is not a rule that every function needs a `.cpp`.
- Review generated Unicode data together with its generator before relocating
  declarations/definitions; preserve pinned data versions and provenance.
- If a public signature exposes a span, warning descriptor, number classification
  or other supporting type, give that type a public home. Do not make a consumer
  include a private lexer header to use the public API.

## Public representation boundary

The migration keeps `document`'s existing by-value storage layout and
allocation-free construction. The `detail::value_storage` class declaration is
included directly in the public `value/document.hpp`; record/payload definitions
and storage method bodies are private. The declaration is representation support,
not a supported direct consumer API. Public views forward-declare private record
types and resolve them only in compiled implementations. No public header
includes a private header. No PImpl allocation or fixed opaque storage was added.

Classify dependencies before migration. Representation declarations may remain
inside supported public headers where appropriate, or the human developer may
choose an opaque representation with an explicit storage/lifetime contract.
Do not silently introduce PImpl/global-heap allocation, infallible construction,
fixed opaque storage sizes or new ABI promises merely to hide implementation.
Any representation change must preserve caller-supplied fallible storage,
bounded cleanup, explicit nonthrowing errors and documented view invalidation.

## Build, installation and tests

- Introduce a compiled parser target exported as `AdaptiveJSON5::parser`.
  A static library is the initial recommendation; this ADR does not require
  shared-library or binary-compatibility support.
- Expose `includes/` as the target's public build include root and the installed
  include directory as its public install root. Add `src/includes/` privately.
  Require C++17 for consumers and apply the repository's configured compiler,
  warning, exception and RTTI policy to compiled sources.
- Unit executables link the compiled target. Units under `src/tests/` may receive
  `src/includes/` as a private test include path. Top-level `tests/` uses only
  supported public headers and the compiled target's public include directories;
  it must not receive the private include root or reach it through relative paths.
  Both groups mirror topics and retain J5 names and traceability.
- Classify tests by what they access, not by how much library behavior they
  exercise. A parser engine/frame/lexer test using internal declarations belongs
  under `src/tests/`. A complete-input test using the future public parser entry
  belongs under top-level `tests/parser/` or `tests/library/`. If an existing file
  mixes public-contract and internal checks, split the declarations into the
  appropriate files without duplicating the test bodies or weakening assertions.
- Both test trees are host-only, enabled only by `ADAPTIVE_JSON5_BUILD_TESTS`, and
  registered with CTest through test-only CMake setup. They use the same pinned
  GoogleTest dependency and configured exception/RTTI policy. Source-tree location
  does not make `src/tests/` part of production: list production sources explicitly
  and never collect test sources through a recursive source glob. Do not install
  either test tree or link their executables/framework into the consumer library.
- Build the core when library consumers or enabled unit tests need it. The
  current unit presets set `ADAPTIVE_JSON5_BUILD_LIBRARY=OFF`; that must not prevent
  compiling a target required by units. Installation remains controlled by the
  library/package configuration.
- Install/export the compiled target and public headers, preserving the public
  topic paths. GoogleTest remains host-only and is neither linked nor acquired
  by tests-disabled consumer builds.

## Migration and evidence

Migrate in small groups: establish the compiled target, separate declarations
and definitions, resolve public-header dependencies, then move files into topic
folders and finish install/export rules. Move internal units to `src/tests/` and
retain public-interface units under `tests/` as their API boundary is established.
Update includes, test source lists,
documentation links and generators alongside the affected moves. Keep behavior
changes separate where practical and use existing units to check each group.

Run applicable GoogleTest units with exceptions OFF/ON and RTTI OFF, plus
clang-format, under the existing quality scope. No new automated quality gate
is authorized by this layout decision. Previous test results describe the
pre-migration implementation until rerun against the migrated code.

Romanr92 normally authors production C++ and public headers. The user explicitly
authorized AI to perform this structural refactor; that authorization does not
extend to implementing subsequent parser features. AI may continue to maintain
build setup, tests, documentation and guidance under the normal repository scope.
This refactor does not implement P037 or change J5 Draft/Verified status.
