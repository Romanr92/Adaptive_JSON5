# Optional schema component

The JSON Schema Draft-07 component is a separate library target,
`AdaptiveJSON5::schema`, built only with `ADAPTIVE_JSON5_BUILD_SCHEMA=ON` (OFF by
default). Its sources follow [ADR 0002](../core/docs/adr-0002-public-private-source-layout.md):
public headers in `includes/adaptive_json5/schema/` (entry header
`adaptive_json5/schema.hpp`), private headers in `core/includes/schema/`, sources in
`core/src/schema/`. Decisions are recorded in
[ADR 0006](../core/docs/adr-0006-schema-component.md). This directory holds no code
and is not a dependency of the parser target.

Status: P043–P051 are implemented: registry, compilation with `$ref` resolution,
exact equality and every Draft-07 assertion and applicator keyword (types, values,
numbers, strings and patterns, arrays, objects, combinators and conditions).
Meta-schema checking (P052) and annotations (P053) follow. Limits and storage are described in
[memory and limits](../docs/memory-and-limits.md#schema-component-limits-optional-adr-0006).

Keep schema errors separate from syntax errors and consumer policies. Runtime
schema resolution must remain explicitly controlled by the caller.
