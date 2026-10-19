# Compiled parser components and internal units

Ordinary implementations are grouped by topic under `src/` (repository path
`src/src/`) and compiled
into `AdaptiveJSON5::parser`. Shared private declarations live in
`includes/adaptive_json5/detail/`; internal GoogleTest units live in `tests/`.
Neither private headers nor internal units are installed or exposed through the
consumer include path. Production source lists are explicit and exclude tests.

Public-header/API and whole-library behavior units remain in top-level `tests/`.
The core performs no host I/O; caller storage and explicit errors remain its
runtime boundary. The public `parse_document()` entry provides complete-document
parsing; the internal P033 engine remains private.

See [ADR 0002](../docs/adr/0002-public-private-source-layout.md) for layout and
representation decisions and the [source layout guide](../docs/source-layout.md)
for current paths and build/test commands. The user's authorization permitted the mechanical
production refactor; it does not change the normal human-owned implementation
workflow for subsequent features.
