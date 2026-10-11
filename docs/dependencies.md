# Dependencies and provenance

The current repository has no production C++ code or runtime dependency. Original
repository material uses Apache-2.0; see [LICENSE](../LICENSE).

| Material | Immutable version | License | Purpose |
| --- | --- | --- | --- |
| GoogleTest | 1.17.0, `52eb8108c5bdec04579160ae17225d66034bd723` | BSD-3-Clause; upstream LICENSE retained in explicit checkout | Host-only unit-test framework, never shipped with the parser |
| clang-format | 18.1.8 development package | LLVM licensing; supplied by development installation | C++ formatting only |
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
Workflow action versions are recorded in their files. Test dependency acquisition
is an explicit checkout step, not an implicit CMake or parser download.

Before adding Unicode tables, grammar/schema fixtures, a schema engine or editor
dependencies, record its source, immutable version, license, notices and any
regeneration procedure. These future dependencies are not selected by this setup.
