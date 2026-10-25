# ADR 0002: Public headers, private headers and compiled implementation

Status: Accepted by the user, 2026-10-19. Physical migration implemented under
the user's explicit authorization to refactor production files; unit
evidence is recorded in [traceability](../../docs/traceability.md).
The [source layout guide](design-source-layout.md) describes the implemented tree
and day-to-day build/test conventions, including the final `core/src/` location.

## Issue #98 amendment (2026-10-25)

The user authorized the complete structural migration for this issue. The outer
component is now `core/`. Private includes omit the former namespace folders,
for example `"parser/parser_engine.hpp"`; the C++ `adaptive_json5::detail`
namespace and public `adaptive_json5/` include prefix are unchanged.
The root delegates to `core/CMakeLists.txt` and `tests/CMakeLists.txt`;
`core/src/CMakeLists.txt` owns the production list and `core/tests/CMakeLists.txt`
owns internal units. Public units are registered by topic in `tests/api/`,
`tests/memory/` and `tests/value/`; shared helpers live in
`tests/shared_infrastructure/`.

Internal ADR 0002, architecture, development guidance, implementation sequence,
quality requirements and historical proposal/backlogs live in flat `core/docs/`
with `adr-`, `design-` or `requirements-` prefixes. Public ADR 0001 remains in
`docs/adr/` because it defines consumer portability and component boundaries.
Public API requirements, result design, embedded contract, package usage,
conformance, provenance and the existing traceability inventory remain in `docs/`.
J5-094 through J5-100 and the regression/quality intentions moved to
[core quality requirements](requirements-quality.md); public behavior contracts
remain in [API requirements](../../docs/requirements.md). No normative wording or
Draft status changed. Regenerating source/function traceability is issue #99.

## Decision

Use `includes/` (plural) only for supported consumer-facing headers. Place
implementation-only headers under `core/includes/`. Group public headers,
private headers, implementation files and tests by topic. Use ordinary compiled
`.cpp` definitions for substantial non-template implementations rather than
continuing to grow a header-only parser.
The implementation topic folders live under `core/src/`; `core/` is the component
root containing its `includes/`, `src/` and `tests/` subtrees.

Internal component GoogleTest units live under `core/tests/`. Top-level `tests/`
contains public-header/API units and whole-library behavior units using only
the supported public interface. This user-approved amendment is part of the
layout decision; both groups remain unit tests, not new integration/end-to-end
quality gates.

This decision applies to new work and implementation guidance from now on.
The former `include/adaptive_json5/` tree has been replaced. Do not restore a
parallel header-only copy or duplicate implementation logic in the public tree.

## Target structure

```text
includes/adaptive_json5/           Supported public headers (memory/, value/)
core/
  CMakeLists.txt                  Core target and internal-unit orchestration
  includes/                       Private search root; no namespace folders
    common/
    input/
    lexer/{identifier,number,string}/
    memory/
    parser/
    unicode/
    value/
  src/                            Compiled definitions; explicit CMake source list
    CMakeLists.txt
  tests/                          Internal GoogleTest units by topic
    CMakeLists.txt
  docs/                           Flat internal documentation
    adr-*.md
    design-*.md
    requirements-*.md
tests/
  CMakeLists.txt                  Public-unit and validation orchestration
  api/                            Public API and whole-library units
    CMakeLists.txt
  memory/
    CMakeLists.txt
  value/
    CMakeLists.txt
  shared_infrastructure/          Host-only helpers, never installed
  validation/                     Independently enabled fixture cases
docs/                             Public contracts, usage and conformance
  requirements.md                Public API requirements
  adr/0001-project-scope.md        Public portability/component scope
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
  `"parser/parser_engine.hpp"`, resolved through
  `core/includes/`.
- Public headers must compile without access to `core/includes/` or source-tree
  relative paths. Their transitive dependencies must also be publicly available
  standard facilities or supported public headers.
- Private types and helpers are not supported consumer APIs. Do not install
  `core/includes/` or expose it as a public/transitive CMake include directory.
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
  include directory as its public install root. Add `core/includes/` privately.
  Require C++17 for consumers and apply the repository's configured compiler,
  warning, exception and RTTI policy to compiled sources.
- Unit executables link the compiled target. Units under `core/tests/` may receive
  `core/includes/` as a private test include path. Top-level `tests/` uses only
  supported public headers and the compiled target's public include directories;
  it must not receive the private include root or reach it through relative paths.
  Both groups mirror topics and retain J5 names and traceability.
- Classify tests by what they access, not by how much library behavior they
  exercise. A parser engine/frame/lexer test using internal declarations belongs
  under `core/tests/`. A complete-input test using the public parser entry
  belongs under top-level `tests/api/`. If an existing file
  mixes public-contract and internal checks, split the declarations into the
  appropriate files without duplicating the test bodies or weakening assertions.
- Both test trees are host-only, enabled only by `ADAPTIVE_JSON5_BUILD_TESTS`, and
  registered with CTest through test-only CMake setup. They use the same pinned
  GoogleTest dependency and configured exception/RTTI policy. Source-tree location
  does not make `core/tests/` part of production: list production sources explicitly
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
folders and finish install/export rules. Move internal units to `core/tests/` and
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

## Issue #98 local verification

On 2026-10-25, clean Ninja Debug configurations with native Windows MinGW-w64
GCC 15.2.0 passed 3,068/3,068 GoogleTest cases in each exception mode
(2,147 host units plus 921 validation cases), with RTTI OFF. Both configurations
enabled the independent validation build flag and used the existing offline
GoogleTest checkout. clang-format 18.1.8 passed all 214 C++ files. A clean
Release configuration with both test flags OFF built and installed without
acquiring GoogleTest or installing private headers/test helpers.

Local checks also confirmed all 101 normative J5 rows survive exactly once,
local Markdown link targets resolve, and private headers/public include boundaries
match this layout. Comparing the C++ bodies found only include/format changes,
apart from the source-scanning unit's updated directory strings. These results
are local evidence; Linux/PR gates and issue #99 traceability regeneration are
separate. Requirement rows remain Draft.
