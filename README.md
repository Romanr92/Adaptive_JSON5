# Adaptive_JSON5

Adaptive_JSON5 is a C++17 JSON5 and JSON parser for automotive and embedded software.
It parses into memory you provide, never touches the heap, and reports every failure
as a value, so it runs the same on a bare-metal microcontroller and on a
high-performance computer. It is developed towards ISO 26262 ASIL B(D) and ISO/SAE
21434 CAL 4, with its safety and security work documented in the open.

[![Unit tests](https://github.com/Romanr92/Adaptive_JSON5/actions/workflows/unit-tests.yml/badge.svg)](https://github.com/Romanr92/Adaptive_JSON5/actions/workflows/unit-tests.yml)
[![Validation tests](https://github.com/Romanr92/Adaptive_JSON5/actions/workflows/validation-tests.yml/badge.svg)](https://github.com/Romanr92/Adaptive_JSON5/actions/workflows/validation-tests.yml)
[![Formatting](https://github.com/Romanr92/Adaptive_JSON5/actions/workflows/formatting.yml/badge.svg)](https://github.com/Romanr92/Adaptive_JSON5/actions/workflows/formatting.yml)
[![Examples](https://github.com/Romanr92/Adaptive_JSON5/actions/workflows/examples.yml/badge.svg)](https://github.com/Romanr92/Adaptive_JSON5/actions/workflows/examples.yml)

**Status: in development, pre-1.0, not for production use. Not certified and not
independently assessed: see [Disclaimers](#disclaimers).**

**Contents:** [Highlights](#highlights) · [Quick start](#quick-start) ·
[Usage](#usage) · [Documentation](#documentation) ·
[Building and contributing](#building-and-contributing) · [License](#license) ·
[Disclaimers](#disclaimers)

## Highlights

- **Embedded by design.** No heap in the parser library, no exceptions, no recursion,
  and no operating-system dependency. Every failure is an explicit result value, in
  every compiler exception mode.
- **Complete and exact.** Full JSON5 1.0 and selectable strict JSON, exact number
  handling with correct rounding, and configurable, inclusive limits on input size,
  nesting, memory and work.
- **Tested.** More than 3,400 GoogleTest cases with exceptions OFF and ON and RTTI OFF,
  validation against the third-party JSONTestSuite and json5-tests fixtures, and CI on
  Linux x86-64, Linux ARM64 and Windows (MinGW). Sanitizers, fuzzing, a second compiler
  and static analysis are being added as blocking checks.
- **Safety and security work in the open.** Developed towards ASIL B(D) and CAL 4 as an
  out-of-context component, with assumptions, requirements, FMEA, TARA and manuals for
  integrators: [functional safety](docs/ISO26262-safety/safety-plan.md) and
  [cybersecurity](docs/ISO21434-cybersecurity/cybersecurity-plan.md).
- **Coding standards.** MISRA C++:2023, SEI CERT C++ and BARR-C:2018 as the
  [coding baseline](docs/ISO26262-safety/coding-guidelines.md).
- **All production code is written by me; AI assists with documentation and tests.**

## Quick start

This goes from nothing to a first parsed value. You need CMake 3.25+, Git, a C++17
compiler and network access for the download. Create an empty folder with these two
files.

`CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.25)
project(hello LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
set(ADAPTIVE_JSON5_BUILD_HOSTED ON CACHE BOOL "" FORCE)
FetchContent_Declare(AdaptiveJSON5
    GIT_REPOSITORY https://github.com/Romanr92/Adaptive_JSON5.git
    GIT_TAG 0.2.0)
FetchContent_MakeAvailable(AdaptiveJSON5)

add_executable(hello main.cpp)
target_link_libraries(hello PRIVATE AdaptiveJSON5::hosted)
```

`main.cpp`:

```cpp
#include "adaptive_json5/adaptive_json5.hpp"
#include "adaptive_json5/hosted/parse.hpp"

#include <cstdio>
#include <string_view>

int main()
{
  constexpr std::string_view text = "{ name: 'sensor', port: 8080, }"; // JSON5: unquoted names, trailing comma

  const auto parsed = adaptive_json5::hosted::parse({text.data(), text.size()});
  if (!parsed.has_value())
  {
    char message[128];
    const auto described = adaptive_json5::describe(*parsed.error_if(), {text.data(), text.size()}, message);
    std::puts(described.has_value() ? message : "invalid input");
    return 1;
  }

  const auto port = parsed.value_if()->root().member("port").as_int64();
  if (!port.has_value())
  {
    std::puts("port is missing or is not an integer");
    return 1;
  }

  std::printf("port: %lld\n", static_cast<long long>(*port.value_if()));
  return 0;
}
```

Build and run it from that folder:

```sh
cmake -S . -B build
cmake --build build
./build/hello
```

It prints `port: 8080`. If CMake cannot find a compiler or build tool on Windows, add
`-G Ninja` to the first command (for example with MinGW GCC and Ninja installed).

`GIT_TAG` pins the `0.2.0` release. Use a later release tag to update.

- Every call returns a `result`: check `has_value()` before `value_if()`. Errors are
  values, never exceptions.
- `member()` and `at()` can be chained; a missing member or a wrong type is reported
  by the final read.

### Without the heap

For embedded targets, parse into memory you provide. The reading code stays the same:

1. In `CMakeLists.txt`, remove the `ADAPTIVE_JSON5_BUILD_HOSTED` line and link
   `AdaptiveJSON5::parser` instead of `AdaptiveJSON5::hosted`.
2. In `main.cpp`, remove the `hosted/parse.hpp` include and add this above `main`:

   ```cpp
   static unsigned char memory[16384]; // the document lives here; no heap is used
   ```

3. Replace the parse call with:

   ```cpp
   const auto parsed = adaptive_json5::json5_document::parse({text.data(), text.size()}, memory, sizeof(memory));
   ```

`memory` must stay alive, and must not be used for anything else, while the document
exists.

### Using an installed library

To build against an installed copy instead of downloading during configure, install it
once:

```sh
git clone https://github.com/Romanr92/Adaptive_JSON5.git
cmake -S Adaptive_JSON5 -B lib-build -DCMAKE_BUILD_TYPE=Release -DADAPTIVE_JSON5_BUILD_TESTS=OFF -DADAPTIVE_JSON5_BUILD_HOSTED=ON
cmake --build lib-build
cmake --install lib-build --prefix install
```

In `CMakeLists.txt`, replace the `include(FetchContent)` block, through
`FetchContent_MakeAvailable`, with:

```cmake
find_package(AdaptiveJSON5 0.2 CONFIG REQUIRED)
```

Then configure with the absolute path of that `install` folder:

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/absolute/path/to/install
```

The [user guide](docs/memory-and-limits.md#simple-api-json5_document-current-behavior)
covers buffer sizing, warnings, limits and the advanced API. The
[memory switch](#memory-switch-adaptive_json5_build_hosted) explains the heap option, and
the [example programs](examples/README.md) show reading files, typed values and errors.

## Usage

The library has two levels of API, both in the umbrella header
`adaptive_json5/adaptive_json5.hpp`:

- **Simple API:** `json5_document::parse` parses into a buffer you provide, and
  `json5_value` reads members, elements and numbers. The optional hosted add-on,
  `hosted::parse`, does the same with heap memory for desktop and server tools.
- **Advanced API:** `parse_document` with your own storage callbacks, for full control
  over memory, accounting and limits.

Before parsing, read [memory, limits and failure behavior](docs/memory-and-limits.md).
It covers buffer sizing, what each limit counts, warnings, what happens when a parse
fails, and the advanced API.

### CMake consumption

The source/install package exports the compiled static `AdaptiveJSON5::parser`
target and the supporting `AdaptiveJSON5::build_options` metadata target.
Consumers receive only the public `includes/` search root. The default include is
`adaptive_json5/adaptive_json5.hpp`, which contains the whole embedded API: the
simple `json5_document` and the advanced owned document/value/storage components
with `parse_document`. The optional schema target remains later work.

```cmake
add_subdirectory(path/to/Adaptive_JSON5)
target_link_libraries(my_application PRIVATE AdaptiveJSON5::parser)
```

Tests default OFF as a subproject. A tests-disabled compiled package can be
configured, built and installed without GoogleTest:

```sh
cmake --preset library
cmake --build --preset library
cmake --install build/library --config Release --prefix path/to/install
```

### Memory switch: `ADAPTIVE_JSON5_BUILD_HOSTED`

The CMake option `ADAPTIVE_JSON5_BUILD_HOSTED` decides whether the library may use
the global heap. It is **OFF by default**.

| | OFF (default) | ON |
|---|---|---|
| Libraries | `AdaptiveJSON5::parser` | `AdaptiveJSON5::parser` and `AdaptiveJSON5::hosted` |
| Static library files (GCC/Clang) | `libadaptive_json5_parser.a` | `libadaptive_json5_parser.a` and `libadaptive_json5_hosted.a` |
| Memory | a caller buffer (`json5_document::parse`) or your storage callbacks (`parse_document`) | additionally the heap, through `hosted::parse` |
| Headers installed | all public headers except `adaptive_json5/hosted/` | all public headers |
| Suits | embedded devices and any target without a reliable heap | desktop and server applications |

Both modes return the same `json5_document`, and every limit applies in both.

**Activate it**

1. Configure with the option ON, then build and install:

   ```sh
   cmake -S . -B build/hosted -DADAPTIVE_JSON5_BUILD_HOSTED=ON
   cmake --build build/hosted
   cmake --install build/hosted --prefix path/to/install
   ```

   As a subproject, set it before `add_subdirectory`:

   ```cmake
   set(ADAPTIVE_JSON5_BUILD_HOSTED ON CACHE BOOL "" FORCE)
   add_subdirectory(path/to/Adaptive_JSON5)
   ```

2. Link `AdaptiveJSON5::hosted`. It brings `AdaptiveJSON5::parser` with it:

   ```cmake
   target_link_libraries(my_application PRIVATE AdaptiveJSON5::hosted)
   ```

3. Add the hosted include next to the umbrella header and parse in one call:

   ```cpp
   #include "adaptive_json5/adaptive_json5.hpp"
   #include "adaptive_json5/hosted/parse.hpp"

   const auto parsed = adaptive_json5::hosted::parse({text, size});   // heap storage
   ```

4. Check the installation: `include/adaptive_json5/hosted/parse.hpp` and
   `lib/libadaptive_json5_hosted.a` exist under the prefix, and after
   `find_package(AdaptiveJSON5 CONFIG REQUIRED)` the condition
   `if(TARGET AdaptiveJSON5::hosted)` is true.

**Deactivate it**

1. Configure with `-DADAPTIVE_JSON5_BUILD_HOSTED=OFF`, or remove the `set(...)` line.
   An existing build directory keeps the cached value, so pass `OFF` explicitly or
   configure a fresh directory (`cmake --fresh`).
2. Link `AdaptiveJSON5::parser` instead of `AdaptiveJSON5::hosted`.
3. Remove the hosted include and replace `hosted::parse(input)` with
   `json5_document::parse(input, buffer, sizeof(buffer))`. The result is the same
   `json5_document`, so the code that reads it stays unchanged.

A leftover hosted include then fails at compile time, because the header is not
installed. Building the unit tests compiles the hosted library for its own tests
even when the option is OFF; it is installed only when the option is ON.

**Which to choose:** keep it OFF for embedded targets, for code that must bound its
memory up front, and whenever no reliable heap exists. Turn it ON for desktop and
server tools that prefer one call without managing a buffer.

Use `find_package(AdaptiveJSON5 0.2 CONFIG REQUIRED)` with the installation prefix
in `CMAKE_PREFIX_PATH`. The current version identifies the development package,
not a supported parser release.

## Documentation

**Getting started**

- [Quick start](#quick-start): from nothing to a first parsed value.
- [Example programs](examples/README.md): reading files, typed values and errors.
- [Release packages](docs/release-packages.md): prebuilt static libraries, checksums and
  offline use.

**Using the library**

- [Memory, limits and failure behavior](docs/memory-and-limits.md): buffers, limits,
  errors and the advanced API.
- [Embedded boundary](docs/embedded.md): what the parser needs from a bare-metal target.
- [Memory layout reference](docs/memory-layout.md): sections, heap and stack per
  environment (bare metal and RTOS, Linux, Windows, FreeBSD).

**Functional safety (ISO 26262)**

- [Safety plan](docs/ISO26262-safety/safety-plan.md): roles, tailoring and work products.
- [Safety Manual](docs/ISO26262-safety/safety-manual.md): the checklist for integrators.
- [SEooC assumptions](docs/ISO26262-safety/seooc-assumptions.md),
  [software safety analysis](docs/ISO26262-safety/software-safety-analysis.md),
  [coding guidelines](docs/ISO26262-safety/coding-guidelines.md),
  [verification strategy](docs/ISO26262-safety/verification-strategy.md),
  [tool confidence](docs/ISO26262-safety/tool-confidence.md) and
  [traceability](docs/ISO26262-safety/traceability.md).

**Cybersecurity (ISO/SAE 21434)**

- [Cybersecurity plan](docs/ISO21434-cybersecurity/cybersecurity-plan.md): roles,
  tailoring and work products.
- [Cybersecurity manual](docs/ISO21434-cybersecurity/cybersecurity-manual.md): the
  checklist for integrators.
- [Cybersecurity assumptions](docs/ISO21434-cybersecurity/cybersecurity-assumptions.md),
  [TARA](docs/ISO21434-cybersecurity/tara.md),
  [cybersecurity verification](docs/ISO21434-cybersecurity/cybersecurity-verification.md),
  [cybersecurity case](docs/ISO21434-cybersecurity/cybersecurity-case.md) and
  [interface agreement template](docs/ISO21434-cybersecurity/cybersecurity-interface-agreement.md).
- [Security policy](SECURITY.md): how to report a vulnerability, and supported versions.

**Design**

- [Architecture](core/docs/design-architecture.md) and
  [project scope](docs/adr/0001-project-scope.md) (ADR 0001).
- [Source layout](core/docs/design-source-layout.md) and
  [public/private layout decision](core/docs/adr-0002-public-private-source-layout.md) (ADR 0002).
- [Storage and budget decisions](core/docs/adr-0003-storage-and-budget-decisions.md) (ADR 0003)
  and [simple API, umbrella header and memory switch](docs/adr/0004-simple-api-and-umbrella-header.md) (ADR 0004).
- [C++ coding style](core/docs/design-coding-style.md) and
  [software unit ID registry](core/docs/software-unit-ids.md).

**Requirements and evidence**

- [Requirements](docs/requirements.md): J5 rows, software safety requirements (section K)
  and cybersecurity requirements (section L).
- [Traceability](docs/traceability.md): unit tests per requirement, and the safety
  traceability chain.
- [JSON5 1.0.0 support statement](docs/conformance.md): claim status, limits,
  implementation-defined behaviour, deviations and executed results.
- [Grammar evidence matrix](core/docs/design-grammar-evidence.md),
  [dependencies and provenance](docs/dependencies.md) and [licensing](docs/licensing.md).

**Contributing**

- [Contributing](CONTRIBUTING.md): how changes are made, the implementation sequence and
  the delivery scope.
- [Agent constraints](AGENTS.md) and the adapted repository skills in `.agents/skills/`.

## Building and contributing

Use CMake 3.25+, a compatible C++17 compiler and GoogleTest 1.17.0 for host units.
CMake downloads and builds the pinned host-only GoogleTest dependency on the first test
configuration. From the repository directory:

```powershell
cmake --preset debug-unit-tests
cmake --build --preset debug-unit-tests
ctest --preset debug-unit-tests
```

Run the same units with exceptions ON:

```powershell
cmake --preset debug-unit-tests-exceptions
cmake --build --preset debug-unit-tests-exceptions
ctest --preset debug-unit-tests-exceptions
```

If the selected preset is already configured and built, run only its `ctest` command.
To run one suite, select its label, for example:

```powershell
ctest --preset debug-unit-tests -L "^result_units$" --output-on-failure
```

Add `--verbose` to see each GoogleTest case; CTest counts every individual case,
including parameterized ones. Exceptions and RTTI default OFF. On Windows, run the
commands in a Visual Studio developer terminal when using its compiler. See
[native development](core/docs/design-development.md) for offline GoogleTest,
formatting and toolchain configuration.

Release packaging builds GCC static libraries for Linux x86-64, Linux ARM64, Windows
MinGW x86-64 and a pinned ARM64 Yocto SDK. Each archive contains the `.a` beside its
public `include/` folder, exported CMake targets, licenses and source and toolchain
metadata; see [release packages](docs/release-packages.md). macOS binaries are outside
this packaging scope.

How to contribute, including the implementation sequence, the delivery scope and the
checks every pull request must pass, is described in [CONTRIBUTING.md](CONTRIBUTING.md).

## License

Copyright 2026 Razvan-Stefan Roman.

[Apache-2.0](LICENSE). Incorporated dependencies, Unicode data and fixtures retain
their own applicable notices and recorded versions.

See [licensing and redistribution](docs/licensing.md) and [NOTICE](NOTICE) for
attribution and distribution obligations.

## Disclaimers

### Safety, security and liability

> [!WARNING]
> **Not certified and not independently assessed.** This library is developed by one
> developer with AI assistance. ASIL B(D) and CAL 4 are design targets, not achieved
> levels. An AI review is not an independent review, so no ISO 26262 or ISO/SAE 21434
> claim is valid without your own independent assessment.

I develop this library alone. I want it to follow ISO 26262 and ISO/SAE 21434 properly,
but I have nobody to do the independent work with, so I simulated those steps with an
AI. Even where everything else holds up, a review by an AI is not a valid review. A
qualified human has to go through it. If you use this library in a safety or
security-relevant product, **you do so at your own risk**.

This library is provided as is, without any warranty. I accept no responsibility or
liability for how you use it or for any damage that results, as stated in sections 7
and 8 of the Apache License 2.0.

This library is meant for developers who understand functional safety and
cybersecurity. If you are not sure what that involves, please do not use it in a safety
or security-relevant product, and do not expect support for doing so.

### Standards and trademarks

ISO 26262, ISO/SAE 21434 and MISRA C++ are licensed standards, and SEI CERT C++ and
BARR-C belong to their publishers. This project's documents summarize them in my own
words and do not reproduce them; for any real rating or compliance work, use the
standards themselves.

AUTOSAR is a trademark of the AUTOSAR development partnership, and MICROSAR is a product
of Vector Informatik. Other names, such as Arm, RISC-V, LoongArch, Linux, Windows and
FreeBSD, belong to their respective owners. They are mentioned only as examples or for
technical reference. This project is not affiliated with or endorsed by any of them.

### About the author

I have eight years of experience in automotive software development, and this project
follows the practices I have learned and applied in that time. The safety and
cybersecurity documents reflect that experience. They still do not replace an
independent human review or assessment.

This is a passion project. I love cars, driving and automotive software, and I build
this to see how far my skills, determination and a healthy dose of stubbornness can take
a library like this towards the standards, done properly and in the open.

It is a personal project, developed in my free time. It uses no code, documents, tools
or confidential information from any employer, past or present. It is based on my
general professional experience and public sources, with AI assistance, and it is not
affiliated with or endorsed by any employer.

All eight years of my experience are in embedded C. In my view, from watching the job
market, embedded software work in Germany is shifting towards ADAS, AUTOSAR Adaptive and
high-performance computers, where C++ dominates. That's why I'm learning modern C++
through this project. Feedback on the C++ is very welcome.

No review catches everything: not my own, not the AI reviews and not the automated
tests. If you find a mistake, in the code or in how I've applied the standards, please
open an issue. Corrections are always welcome.
