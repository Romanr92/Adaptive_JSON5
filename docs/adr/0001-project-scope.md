# ADR 0001: Portable library and development scope

Status: Accepted for repository preparation; updated 2026-10-12.

This ADR records the preparation-stage baseline. Its portable runtime/component
boundaries remain applicable; implementation-status statements in its consequences
are historical. [ADR 0002](0002-public-private-source-layout.md) and the
[current architecture](../architecture.md) describe the compiled implementation
and public/private file separation now in use.

## Context

The supplied proposal describes 100 Draft contracts for a standalone reader,
optional schema validator and tooling. The repository started with Apache-2.0,
a short README and a C++ ignore file. The user requested development preparation
and implementation issues, then specified unit tests and formatting only, and a
generic C++ library with native development rather than WSL.

## Decision

- Target compatible C++17 projects on Windows, Linux and macOS, plus OS-agnostic embedded integration after compilation into a compatible project. Supported
  compiler/platform combinations must be recorded from actual results.
- Develop the JSON5 reader independently, with selectable strict JSON. Keep
  schema, CLI and editor components independently buildable and optional.
- Preserve Apache-2.0. The proposal's suggested MIT license does not replace the
  repository's existing license. Record all incorporated material separately.
- Use CMake 3.25, exported namespaced targets and explicit exception settings.
  Exceptions default OFF for this library, without changing parent project flags.
- Automated checks comprise unit tests and clang-format only. Deterministic
  upstream corpus cases and allocation/cancellation tests can be unit fixtures.
- Treat AdaptivePi as inspiration and an optional consumer example. Its code,
  agent policies, SDK, filesystem, service model and release governance are not
  dependencies. No WSL, Yocto, QEMU or ARM64 deployment is required.
- Preserve the original proposal Markdown as historical Draft input; add J5-101 for the later embedded requirement.
- Agents may edit CMake, GoogleTest tests, documentation and requirements, but shall not write production C++ or public C++ headers. Record this in AGENTS.md and adapt the two repository skills from AdaptivePi.
- Keep the 100 original stable requirement IDs and record adaptations explicitly. The
  contracts remain Draft until reviewed; repository preparation is not proof of
  implementation or conformance.

## Consequences

The scaffold can build GoogleTest infrastructure and test compiler settings before any
production C++ or parser API is implemented. The current exported CMake target is development metadata only. It must not claim to parse or validate documents yet.
Unit evidence does not establish editor-host, installation, deployment,
performance or universal C++ platform support. Those are not added as quality
gates by this setup. A later optional component may be released separately and
must accurately describe its supported scope.
