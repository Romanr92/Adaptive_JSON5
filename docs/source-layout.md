# Source layout and compiled library

Implemented on 2026-10-19. This guide describes the current structure;
[ADR 0002](adr/0002-public-private-source-layout.md) records the decision and
[verification notes](verification.md) record the executed results.

## Directory map

```text
includes/                         Supported consumer headers only
  adaptive_json5/
    error.hpp
    input.hpp
    options.hpp
    result.hpp
    memory/                       Caller storage, owned blocks, limits/accounting
    value/                        Documents, views, strings, number spelling

src/                              Core component root
  includes/
    adaptive_json5/detail/        Private implementation declarations
      common/
      input/
      unicode/
      lexer/
        identifier/
        number/
        string/
      memory/
      parser/
      value/
  src/                            Compiled implementation files
    lexer/
      identifier/
      number/
      string/
    memory/
    parser/
    unicode/
    value/
  tests/                          Internal GoogleTest units
    common/
    input/
    unicode/
    lexer/
      identifier/
      number/
      string/
    memory/
    parser/
    value/

tests/                            Public-interface/library GoogleTest units
  api/
  input/
  library/
  memory/
  value/
  support/                        Shared host-only test support

cmake/                            Target, dependency and test-registration setup
```

All paths above are relative to the repository root. In particular, compiled
implementation files belong in **`src/src/<topic>/`**, not `src/<topic>/`.
The former singular `include/` tree has been removed. Add topic directories
when they contain useful files; empty folders are not required.

## Public and private includes

Consumers link `AdaptiveJSON5::parser` and receive `includes/` as their public
include-search root. For example:

```cpp
#include <adaptive_json5/value/document.hpp>
#include <adaptive_json5/memory/storage.hpp>
```

Implementation files and internal units additionally receive `src/includes/`
privately and can include:

```cpp
#include "adaptive_json5/detail/parser/parser_engine.hpp"
```

Do not put `includes/` or `src/includes/` into the include spelling. Public
headers and public-interface tests must not depend on the private search root,
private headers or relative paths into implementation directories. Shared
test-only support is separate from the library interface and is not installed.

## Where declarations and definitions belong

| Content | Location |
| --- | --- |
| Supported API types, declarations and usage contracts | `includes/adaptive_json5/` and its topic folders |
| Public template definitions and required compile-time definitions | Public headers |
| Implementation-only types, shared helpers and private templates | `src/includes/adaptive_json5/detail/<topic>/` |
| Ordinary non-template function and substantial method definitions | `src/src/<topic>/*.cpp` |
| Helper used by a single translation unit | Local to that `.cpp` when practical |
| Units accessing internal declarations | `src/tests/<topic>/` |
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
sources are listed explicitly in root `CMakeLists.txt`, excluding both test
trees. Add implementation sources there when introducing a new `.cpp`.

The core is built when library building or enabled units require it. Installation
is controlled by the library option and exports the compiled target and public
headers, preserving their topic paths. Private headers, tests and GoogleTest
are not installed. Tests-disabled builds neither acquire nor build GoogleTest.

Both test trees use the same pinned host-only GoogleTest dependency, project
compiler settings and CTest registration helper in `cmake/UnitTests.cmake`.
Their source lists are maintained in `src/tests/CMakeLists.txt` and
`tests/CMakeLists.txt`. Split mixed internal/public test files without dropping
assertions or changing their J5 names. Both groups remain unit tests under the
existing quality scope.

Top-level public-library units also compile every supported header separately,
without the private include root. The runtime public-library unit exercises
owned document construction, moves, access and UTF-8 export through public APIs.
A complete public parsing entry remains P037 work; the internal P033 engine is
not advertised as that entry point.

## Running units with all logical processors

After configuring the two presets as described in
[native development](development.md), use PowerShell:

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
