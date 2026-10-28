# Source layout and compiled library

Implemented on 2026-10-19. This guide describes the current structure;
[ADR 0002](adr-0002-public-private-source-layout.md) records the decision and
[traceability](../../docs/traceability.md) lists the unit tests.

## Directory map

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

All paths above are relative to the repository root. In particular, compiled
implementation files belong in **`core/src/<topic>/`**, not `src/<topic>/`.
The former singular `include/` tree has been removed. Add topic directories
when they contain useful files; empty folders are not required.

## Public and private includes

Consumers link `AdaptiveJSON5::parser` and receive `includes/` as their public
include-search root. For example:

```cpp
#include <adaptive_json5/value/document.hpp>
#include <adaptive_json5/memory/storage.hpp>
```

Implementation files and internal units additionally receive `core/includes/`
privately and can include:

```cpp
#include "parser/parser_engine.hpp"
```

Do not put `includes/` or `core/includes/` into the include spelling. Public
headers and public-interface tests must not depend on the private search root,
private headers or relative paths into implementation directories. Shared
test-only support is separate from the library interface and is not installed.

## Where declarations and definitions belong

| Content | Location |
| --- | --- |
| Supported API types, declarations and usage contracts | `includes/adaptive_json5/` and its topic folders |
| Public template definitions and required compile-time definitions | Public headers |
| Implementation-only types, shared helpers and private templates | `core/includes/<topic>/` |
| Ordinary non-template function and substantial method definitions | `core/src/<topic>/*.cpp` |
| Helper used by a single translation unit | Local to that `.cpp` when practical |
| Units accessing internal declarations | `core/tests/<topic>/` |
| Units exercising only supported headers and library behavior | `tests/<topic>/` |

Keep existing namespaces, explicit error results, `noexcept`, ownership and
budget contracts when separating declarations and definitions. Do not duplicate
an implementation in both a header and a `.cpp`. Functions kept for templates
or compile-time use are intentional exceptions to out-of-line implementation.

The public document retains its by-value storage layout and allocation-free
construction. Its header contains a supporting `detail::value_storage`
declaration, while record/payload definitions and implementation bodies are
private. Direct use of that representation support is not a supported consumer
API. This separation added neither PImpl allocation nor a global-heap fallback.

## Build and installation

`AdaptiveJSON5::parser` is a compiled static library; consumers must link it.
`AdaptiveJSON5::build_options` remains supporting C++17 metadata. Production
sources are listed explicitly in `core/src/CMakeLists.txt`, excluding both test
trees. Add implementation sources there when introducing a new `.cpp`.

The core is built when library building or enabled units require it. Installation
is controlled by the library option and exports the compiled target and public
headers, preserving their topic paths. Private headers, tests and GoogleTest
are not installed. Tests-disabled builds neither acquire nor build GoogleTest.

Both test trees use the same pinned host-only GoogleTest dependency, project
compiler settings and CTest registration helper in `cmake/UnitTests.cmake`.
Their source lists are maintained in `core/tests/CMakeLists.txt` and
`tests/{api,memory,value}/CMakeLists.txt`. Split mixed internal/public test files without dropping
assertions or changing their J5 names. Both groups remain unit tests under the
existing quality scope.

Top-level public-library units also compile every supported header separately,
without the private include root. The runtime public-library unit exercises
owned document construction, moves, access and UTF-8 export through public APIs.
The public `parse_document()` entry provides complete-document parsing; the
internal P033 engine remains private.

## Running units with all logical processors

After configuring the two presets as described in
[native development](design-development.md), use PowerShell:

```powershell
$unitJobs = [Environment]::ProcessorCount
cmake --build --preset debug-unit-tests --parallel $unitJobs
ctest --preset debug-unit-tests --parallel $unitJobs --output-on-failure
cmake --build --preset debug-unit-tests-exceptions --parallel $unitJobs
ctest --preset debug-unit-tests-exceptions --parallel $unitJobs --output-on-failure
```

The refactor passed **1422/1422 cases in each exception mode**, with RTTI OFF,
using 32 logical processors on the local host. All 157 current C++ files passed
clang-format 18.1.8, and the tests-disabled Release build/install succeeded.
These are local results, not CI, embedded-device or full-parser conformance
claims; J5 statuses remain unchanged.

## Implementation guidance and reviews

Implementation guidance uses this compiled architecture. Each coherent change identifies
the public/internal boundary, matching declaration and implementation files, and
any new entry in the compiled target's CMake source list. Where both files change,
present separately labeled header/source blocks rather than putting all bodies in
a header. Existing declarations can be implemented by changing only the `.cpp`.

Explain complete-type requirements, forward declarations, qualified method
definitions, default arguments, compilation and linking as needed. Review the
header and implementation together. Preserve fallible storage and ownership;
do not introduce extra allocation or an abstraction solely to conceal a type.
The [requirement guidance skill](../../.agents/skills/guide-requirement/SKILL.md) records
the guidance workflow and the narrow template/compile-time exceptions.
