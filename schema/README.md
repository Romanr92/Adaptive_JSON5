# Optional schema component

The JSON Schema Draft-07 component is a separate library target,
`AdaptiveJSON5::schema`, built only with `ADAPTIVE_JSON5_BUILD_SCHEMA=ON` (OFF by
default). Its sources follow [ADR 0002](../core/docs/adr-0002-public-private-source-layout.md):
public headers in `includes/adaptive_json5/schema/` (entry header
`adaptive_json5/schema.hpp`), private headers in `core/includes/schema/`, sources in
`core/src/schema/`. Decisions are recorded in
[ADR 0006](../core/docs/adr-0006-schema-component.md). This directory holds no code
and is not a dependency of the parser target.

Status: P043 provides the outcome, budget, option and instance-domain building
blocks. There is no compile or validate entry point yet; no validator is
implemented.

Keep schema errors separate from syntax errors and consumer policies. Runtime
schema resolution must remain explicitly controlled by the caller.
