# OS-agnostic embedded integration

J5-101 requires the generic core to run without an operating-system dependency
after compilation into a compatible embedded project. This includes bare-metal
and embedded OS deployments. MICROSAR is one example, not a dedicated vendor
port, SDK or workflow requirement. The same core remains useful in hosted projects.

## Core integration contract

- Explicit in-memory bytes/length input; no files, processes, sockets, environment,
  clocks, locale or OS thread calls in parse/value/conversion operations.
- Errors are explicit result/error values and never thrown, including conversion,
  access, allocation and budget failures. Compiler exception support may be
  enabled in the consumer without changing the library's error API.
- No required exception or RTTI runtime. Maintain minimal allocation-free error
  reporting when richer diagnostics cannot be constructed.
- Caller-supplied fallible storage, configurable budgets and bounded cleanup;
  never assume a default global heap is available or recoverable.
- Specify the required C/C++ runtime subset before choosing containers or
  conversion engines. A device must supply a compatible C++17 compiler and the
  documented runtime/storage facilities; no desktop data model is assumed.
- Record static initialization, alignment, byte order, integer-size assumptions,
  stack/work storage and actual compiler/device limitations.
- Keep optional schema-engine and all CLI/editor/GoogleTest dependencies outside
  the embedded core target. No vendor implementation is imported.

For storage callbacks, limit defaults, failure and retry behavior, and sizing a
memory pool and work budget for a target, see
[memory, limits and failure behavior](memory-and-limits.md).

## Generic CMake integration

Use the consuming project's toolchain file for compiler, linker, archiver,
sysroot and platform flags. Freestanding toolchains that cannot link CMake probe
executables may set `CMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY` there.

```sh
cmake -S . -B build/embedded -DCMAKE_TOOLCHAIN_FILE=path/to/toolchain.cmake -DADAPTIVE_JSON5_BUILD_TESTS=OFF -DADAPTIVE_JSON5_ENABLE_EXCEPTIONS=OFF -DADAPTIVE_JSON5_ENABLE_RTTI=OFF
```

This configures the compiled `AdaptiveJSON5::parser` component target without
host tests. Caller-storage/value/lexical components and the internal P033 engine
and public complete-document `parse_document()` entry exist. Actual embedded
toolchain execution/support must still be established separately.

## Unit evidence and support claims

Host GoogleTest units exercise caller-storage failure/cleanup and parser behavior
with exceptions and RTTI disabled. These host results do not establish embedded
compiler/runtime support. Record that support separately; no device execution,
real-time bound, universal compiler compatibility or certification is inferred
from host units. No vendor-specific acceptance or deployment job is required.
