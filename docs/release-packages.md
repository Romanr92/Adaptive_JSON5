# Static library release packages

Publishing a GitHub release runs `release-library.yml` for the released commit.
Published prereleases are included; saving a draft does not publish packages.
The numeric tag version must match `project(AdaptiveJSON5 VERSION ...)`, e.g.
`0.2.0`, `v0.2.0`, `0.2.0-rc.1` or `v0.2.0-rc.1` for CMake version `0.2.0`.
The optional `v` prefix is preserved in archive names and download URLs; use the
exact tag when importing a package. Packaging does not establish
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
  include/adaptive_json5/          # Public .hpp files, except the optional hosted/ header
  lib/cmake/AdaptiveJSON5/        # Relocatable targets and version/build information
  build-info.json
  share/AdaptiveJSON5/            # Licenses, dependency notices and import helper
```

Packages contain the embedded parser only. They are built with `ADAPTIVE_JSON5_BUILD_HOSTED=OFF`,
so `AdaptiveJSON5::hosted` and `hosted/parse.hpp` are not included; build from source to
use the hosted option. The `.a` sits beside `include/`. Private headers, tests and GoogleTest are
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
The separate `packaging_revision` identifies the packaging tool/import helper
revision, which may be newer than the immutable library source during recovery.
Runner package updates can change
compiler patch/runtime versions; manifests describe actual builds, not a promise
of byte-identical rebuilds. Binary imports reject different compiler versions,
compiler targets, operating systems, pointer sizes, MinGW CRT/thread models and
libstdc++ string ABIs. These checks do not prove compatibility with an older glibc.
Use the same toolchain distribution/runtime and compatible target flags, or
rebuild from source.

## ABI policy

Decided in [ADR 0007](adr/0007-public-api-storage-abi-and-index-identity.md).
The library promises **no binary compatibility between releases**, including patch
releases. When you upgrade, rebuild the library and every piece of your C++ code
that includes its headers. A release can change the size or layout of public
objects, the storage callback signatures and the error enumerations. There is no
versioned inline namespace yet. This will be reviewed once the API is stable.

This rule covers binary compatibility only. Source changes between releases are
listed in the release notes.

Code that includes the library and the library itself must agree on:

- the compiler and its C++ standard library ABI (for example the libstdc++
  string ABI), recorded in the package metadata above;
- the target architecture, pointer size and calling convention;
- packing and alignment flags (`-fpack-struct`, `-malign-*` and similar), which
  change the layout of public objects;
- the release: public types such as `value_index`, `storage_callbacks` and the
  document objects can change between releases.

Settings that only select an algorithm inside the compiled library do not change
the ABI. `ADAPTIVE_JSON5_EISEL_LEMIRE` selects how decimal numbers are converted and
changes no public declaration. The test, schema, executable and hosted build flags
add or remove components without changing the core library's public types. The
library itself is built with exceptions and RTTI OFF and reports failures through
result values. A consumer built with exceptions or RTTI ON can call the library,
because no library exception or `typeid` crosses the API. The C++ runtime must still
match, as listed above.

## CMake consumption

After extracting, set `CMAKE_PREFIX_PATH` to the package root and use:

```cmake
find_package(AdaptiveJSON5 0.2.0 EXACT CONFIG REQUIRED COMPONENTS parser)
target_link_libraries(my_application PRIVATE AdaptiveJSON5::parser)
```

The target supplies headers and the archive. Include headers as
`<adaptive_json5/parser.hpp>`; `include/` is not part of their spelling.
The exported `AdaptiveJSON5_VERSION`, `AdaptiveJSON5_RELEASE_TAG`,
`AdaptiveJSON5_SOURCE_REVISION`, `AdaptiveJSON5_PACKAGE_PROFILE`,
`AdaptiveJSON5_COMPILER_VERSION`, `AdaptiveJSON5_TARGET_TRIPLE`,
`AdaptiveJSON5_EXCEPTIONS` and `AdaptiveJSON5_RTTI` expose package information
without a new C++ API. `AdaptiveJSON5_RELEASE_TAG` is the exact tag a release
package was built from (for example `0.2.0` or `0.2.0-rc.1`); it is empty for a
source build.

Components: `parser` and `build_options` are always present. `hosted` is found
only when the optional hosted library is installed, which needs a source build with
`ADAPTIVE_JSON5_BUILD_HOSTED=ON`; release packages never contain it, so
`COMPONENTS hosted` fails for them with a message saying so:

```cmake
find_package(AdaptiveJSON5 0.2.0 EXACT CONFIG REQUIRED COMPONENTS parser hosted)
target_link_libraries(my_tool PRIVATE AdaptiveJSON5::hosted)
```

The optional `schema` component (ADR 0006) exists only in source builds with
`ADAPTIVE_JSON5_BUILD_SCHEMA=ON`; release packages do not contain it yet, so a
required `schema` component fails there, as do unknown components. Its P043
building blocks (outcome, budget and option types) are not a validator yet. The
real host CLI remains future work.

For future AdaptivePi use, vendor `cmake/ImportAdaptiveJSON5.cmake` from a pinned
repository revision into that consumer. This task changes only Adaptive_JSON5;
it does not edit AdaptivePi or implement manifest parsing. After the consumer's
`project(... LANGUAGES CXX)` and toolchain selection:

```cmake
include(cmake/ImportAdaptiveJSON5.cmake)
adaptive_json5_import_release(
    VERSION 0.2.0
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

**The release tag must match exactly.** `VERSION` is compared, character for
character, with the tag the package was built from: `v0.2.0` and `0.2.0` are
different releases, as are `0.2.0-rc.1` and `0.2.0-rc.2`, and `0.2.0-rc.9` does not
accept a final `0.2.0` package. The check runs for `ROOT`, `ARCHIVE`, downloaded and
already cached packages alike. The helper reads `AdaptiveJSON5_RELEASE_TAG`, or, for
packages built before it was exported, `release_tag` from the package's
`build-info.json`. A package without a release tag, such as a source install passed as
`ROOT`, is rejected: import source builds with `find_package` instead. The helper
passes `AdaptiveJSON5_RELEASE_TAG` to the caller.

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
python3 tools/package_release.py --version 0.2.0 \
  --platform linux-x86_64-gcc --compiler g++-13 --build-dir build/release-package
```

Use a clean checkout. `--allow-dirty` marks unpublished local development output;
the workflow never uses it. `--expected-commit` checks the release event's commit.
Version/profile strings are passed as process arguments, never shell code.

A manual workflow run normally builds all four packages as workflow artifacts
without uploading a release. Leaving **publish** unchecked intentionally skips
the publish job. On `release: published`, all builds must succeed before the
publishing job verifies checksums and attaches archives/manifests. Only that job
has `contents: write`. Existing assets are not overwritten on reruns. Keep assets
editable while this after-publication workflow attaches files; immutable-release
publishing would require attaching files before publishing instead.

### Recover an existing release

If an older release failed because it used a tag without `v`, rerunning that old
run still uses its original workflow/tooling. After merging the fix, open
**Actions > release-library > Run workflow**, choose `main`, enter the exact
existing release tag (e.g. `0.1.0`), and check **publish**. This explicitly attaches
packages to that existing release. The workflow checks out its immutable tagged
library source separately and runs the corrected packaging tools from the selected
workflow revision. Library source and packaging-tool revisions are both recorded;
the corrected import helper ships with the package. The tag is not moved.
A source older than the release-tag export (before #129) does not define
`AdaptiveJSON5_RELEASE_TAG`; its package still records `release_tag` in
`build-info.json`, which the import helper reads instead.

All four builds must pass before this recovery publish job runs. Already attached
assets are not overwritten. A failed dependency build also causes publish to be
skipped, so inspect the first failed build rather than treating the skipped
publish job as the underlying failure.

PR quality checks remain GoogleTest units and formatting. The unit workflow adds
native ARM64 and MinGW executions in both exception modes with RTTI OFF. Consumer
configure/build/install checks are development checks, not a new integration CI
gate. The packaging tool and import helper have local regression tests (CMake 3.25+,
a GNU `g++`, and Ninja or, on Windows without it, MinGW Makefiles):

```sh
python3 tools/tests/test_package_release.py
python3 tools/tests/test_import_release.py
```
 Schema packaging and broader shared J5 obligations remain in their owning
stages. Requirement statuses remain Draft.
