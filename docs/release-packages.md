# Static library release packages

Publishing a GitHub release runs `release-library.yml` for the released commit.
Published prereleases are included; saving a draft does not publish packages.
The numeric tag version must match `project(AdaptiveJSON5 VERSION ...)`, e.g.
`v0.1.0` or `v0.1.0-rc.1` for CMake version `0.1.0`. Packaging does not establish
parser conformance or ECU runtime verification.

## Targets and contents

| Profile | Target and toolchain |
| --- | --- |
| `linux-x86_64-gcc` | Ubuntu 24.04 x86-64, GCC 13, glibc/libstdc++ |
| `linux-aarch64-gcc` | Ubuntu 24.04 ARM64, GCC 13, glibc/libstdc++ |
| `windows-x86_64-mingw-gcc` | Windows x86-64, MSYS2 UCRT64 MinGW-w64 GCC, UCRT/libstdc++ |
| `adaptivepi-aarch64-yocto5.0.20-sdk0.2.0-gcc` | AdaptivePi v0.2.0 SDK: AArch64 Poky Linux, Cortex-A57 tuning, Yocto 5.0.20 |

macOS is not packaged. Windows binaries are for MinGW, not MSVC. Generic Linux
ARM64 is separate from the AdaptivePi SDK profile. Another ECU operating system,
CPU tuning, libc or vendor SDK needs its own compatible source build/profile.

Each `AdaptiveJSON5-<tag>-<profile>.tar.gz` extracts to:

```text
AdaptiveJSON5-<tag>-<profile>/
  libadaptive_json5_parser.a
  include/adaptive_json5/          # All public .hpp files and topic folders
  lib/cmake/AdaptiveJSON5/        # Relocatable targets and version/build information
  build-info.json
  share/AdaptiveJSON5/            # Licenses, dependency notices and import helper
```

The `.a` sits beside `include/`. Private headers, tests and GoogleTest are
excluded. Adjacent release assets `<archive>.sha256` and `<archive>.json` identify
its checksum and exact source/toolchain/build metadata. The package still needs
the matching C/C++ runtime: `.a` does not incorporate libstdc++, libgcc or libc
automatically. Windows consumers need the matching UCRT64 toolchain/runtime.

Builds use C++17, Release optimization, position-independent code, no LTO,
exceptions OFF and RTTI OFF. Flags remain private to library compilation; the
exported target requires C++17 without changing parent flags or forcing consumer
exception/RTTI settings. Library errors still use explicit result/error values.
Caller-provided storage remains required; no default allocator is introduced.

Metadata records the exact commit, compiler/version/triple, libstdc++ string ABI,
flags, pointer size, MinGW CRT/thread model, source/data versions and SDK checksum.
Runner package updates can change
compiler patch/runtime versions; manifests describe actual builds, not a promise
of byte-identical rebuilds. Binary imports reject different compiler versions,
compiler targets, operating systems, pointer sizes, MinGW CRT/thread models and
libstdc++ string ABIs. These checks do not prove compatibility with an older glibc.
Use the same toolchain distribution/runtime and compatible target flags, or
rebuild from source.

## CMake consumption

After extracting, set `CMAKE_PREFIX_PATH` to the package root and use:

```cmake
find_package(AdaptiveJSON5 0.1.0 EXACT CONFIG REQUIRED COMPONENTS parser)
target_link_libraries(my_application PRIVATE AdaptiveJSON5::parser)
```

The target supplies headers and the archive. Include headers as
`<adaptive_json5/parser.hpp>`; `include/` is not part of their spelling.
The exported `AdaptiveJSON5_VERSION`, `AdaptiveJSON5_SOURCE_REVISION`,
`AdaptiveJSON5_PACKAGE_PROFILE`, `AdaptiveJSON5_COMPILER_VERSION`,
`AdaptiveJSON5_TARGET_TRIPLE`, `AdaptiveJSON5_EXCEPTIONS` and
`AdaptiveJSON5_RTTI` expose package information without a new C++ API.
Required `schema`/unknown components fail. `ADAPTIVE_JSON5_BUILD_SCHEMA=ON`
also fails explicitly; schema and the real host CLI remain future work.

For future AdaptivePi use, vendor `cmake/ImportAdaptiveJSON5.cmake` from a pinned
repository revision into that consumer. This task changes only Adaptive_JSON5;
it does not edit AdaptivePi or implement manifest parsing. After the consumer's
`project(... LANGUAGES CXX)` and toolchain selection:

```cmake
include(cmake/ImportAdaptiveJSON5.cmake)
adaptive_json5_import_release(
    VERSION v0.1.0
    SHA256 "<replace with the reviewed 64-digit archive SHA-256>"
)
target_link_libraries(my_application PRIVATE AdaptiveJSON5::parser)
```

This version illustrates the initial package version, not evidence of an existing
release. Pin the checksum in version-controlled consumer configuration; do not
fetch an unreviewed checksum dynamically or use `latest`. The helper selects the
OS/compiler-target profile, downloads over verified TLS, verifies the checksum
and imports the package. An unknown target fails with a source-build instruction.
`PROFILE` selects an explicit supported profile; imported metadata still checks
compatibility. Source consumers should pin a release or exact commit too.

For the current AdaptivePi profile, source the matching SDK environment before
configuring and set its identity before importing:

```cmake
set(AdaptiveJSON5_EXPECTED_SDK_SHA256
    "abd5aeb0b6243362c8fafc88c33eb501f62354add112a6509d0ec974cec017d6")
```

This digest pins the `Romanr92/adaptive-pi` release `v0.2.0` installer
`poky-glibc-x86_64-adaptive-pi-image-cortexa57-qemuarm64-toolchain-5.0.20.sh`.
The release workflow verifies it before installation. Future SDK changes need
a new profile/checksum; a matching compiler name alone is insufficient.
Cross-compilation does not establish device runtime support or timing bounds.

Verified archives are cached under the consumer build directory. For offline
use add `ARCHIVE /path/to/the.tar.gz` with the same SHA256, or
`ROOT /path/to/extracted/package` for an explicitly trusted local installation.
`CACHE_DIR` selects a shared cache, with a lock for concurrent configure calls.
Choose a short cache path if a Windows toolchain cannot handle long include paths.
Call the helper once before defining consumers; it rejects an already defined target.
The helper never acquires GoogleTest or changes parent flags.

## Building and publishing

Native example, with GCC, CMake 3.25+, Ninja and Python 3.9+:

```sh
python3 tools/package_release.py --version v0.1.0 \
  --platform linux-x86_64-gcc --compiler g++-13 --build-dir build/release-package
```

Use a clean checkout. `--allow-dirty` marks unpublished local development output;
the workflow never uses it. `--expected-commit` checks the release event's commit.
Version/profile strings are passed as process arguments, never shell code.

A manual workflow run builds all four packages as workflow artifacts without
uploading a release. On `release: published`, all builds must succeed before the
publishing job verifies checksums and attaches archives/manifests. Only that job
has `contents: write`. Existing assets are not overwritten on reruns. Keep assets
editable while this after-publication workflow attaches files; immutable-release
publishing would require attaching files before publishing instead.

PR quality checks remain GoogleTest units and formatting. The unit workflow adds
native ARM64 and MinGW executions in both exception modes with RTTI OFF. Consumer
configure/build/install checks are development checks, not a new integration CI
gate. Schema packaging and broader shared J5 obligations remain in their owning
stages. Requirement statuses remain Draft.
