# Dependencies and provenance

The current foundation headers and generated Unicode tables have no external
runtime library dependency. Original repository material uses Apache-2.0;
incorporated Unicode data retains its own license. See [LICENSE](../LICENSE).

| Material | Immutable version | License | Purpose |
| --- | --- | --- | --- |
| GoogleTest | 1.17.0, `52eb8108c5bdec04579160ae17225d66034bd723` | BSD-3-Clause; upstream LICENSE retained in FetchContent checkout | Host-only unit-test framework, never shipped with the parser |
| clang-format | 18.1.8 development package | Apache-2.0 WITH LLVM-exception for upstream LLVM; installed package may include additional notices | C++ formatting only |
| Unicode Character Database | 17.0.0 and historical 3.0.0; exact source hashes in [manifest](../tools/unicode-data/sources.json) | Unicode License V3; full notice retained in LICENSES and generated header | Immutable parser classification tables; original inputs retained for offline regeneration |
| Original supplied proposal | 2026-10-11 snapshot | User-provided project document | Historical Draft input in docs/requirements/proposals |
| Adapted repository skills | Source file SHAs recorded below | AdaptivePi repository Apache-2.0 context; independently rewritten here | Teaching and GoogleTest workflow |

The skill adaptations use the behaviors in
`Romanr92/adaptive-pi/.agents/skills/teach-requirement/SKILL.md` (blob
`1820508840c48a72d98ff162f6718b587d846803`) and
`write-unit-tests/SKILL.md` (blob `557f1e5b782719ecba8d3bad7f5d9e3ce43084a9`).
They remove AdaptivePi release-wide approval gates, ara APIs, WSL/Yocto/deployment
assumptions and CI-only exception-enabled execution. Preserve requirement-based
teaching, no production-code edits, GoogleTest parameterization, documented test
steps and explicit test evidence. No AdaptivePi production code is copied.

Development also uses CMake 3.25+, Python 3.9+ and a compatible C++17 compiler.
Workflow action versions are recorded in their files. Test dependency acquisition uses CMake FetchContent only for enabled host unit
tests, with an offline source override. This follows AdaptivePi’s Dependencies.cmake
pattern, adapted to an immutable revision and this repository’s build policies.

The VS Code workspace settings and snippet are adapted from AdaptivePi’s
`.vscode/` files at commit `1134cbfea9b587649e32d58c9305fe211fd7f4af`.
Native GoogleTest tasks/debugging replace its QEMU/Yocto configuration; formatter
paths, presets, exclusions and the header-guard prefix match this repository.

Before adding further Unicode data, grammar/schema fixtures, a schema engine or editor
dependencies, record its source, immutable version, license, notices and any
regeneration procedure. These future dependencies are not selected by this setup.

The copied formatter rules originate at AdaptivePi revision
`1134cbfea9b587649e32d58c9305fe211fd7f4af`. See [NOTICE](../NOTICE)
and [licensing guidance](licensing.md) for preserved notices and redistribution.

The ordered parser checklist derives language rules from JSON5 1.0.0 (March 2018)
and ECMAScript 5.1 (June 2011). Specification notices are retained in
`LICENSES/JSON5-Specification-MIT.txt` and `LICENSES/ECMA-262-5.1-Document.txt`.
These are document attributions, not parser runtime dependencies.

The generated `include/adaptive_json5/unicode_tables.hpp` uses Unicode 17.0.0
category data and retains Unicode 3.0.0 identifier memberships. Unchanged source
files, URLs, hashes, the license snapshot and the offline regeneration procedure
are recorded in [the Unicode input inventory](../tools/unicode-data/README.md).
This data incorporation does not complete P006's classification functions or
verify whole-parser Unicode semantics.
