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
P052–P055 add logic units, annotations, `format` assertions, the meta-schema check
and conformance against the official Draft-07 test suite. Limits and storage are described in
[memory and limits](../docs/memory-and-limits.md#schema-component-limits-optional-adr-0006).

## Capabilities (P055)

| Area | Support |
| --- | --- |
| Dialect | Draft-07 only (`schema_dialect::draft_07`); other `$schema` values are `unsupported_dialect` |
| Keywords | Every Draft-07 validation keyword; `$ref`, `$id` (including plain-name fragments) and `definitions`, resolved offline among registered resources |
| Compilation | Strict-JSON schemas, checked against the embedded Draft-07 meta-schema and the built-in shape and pattern checks |
| Annotations | Opt-in (`validation_options::collect_annotations`), valid branches only; instances are never changed |
| `format` | Annotation by default; `compile_options::assert_format` asserts date-time, date, time, email, hostname, ipv4, ipv6, uri, uri-reference, json-pointer, relative-json-pointer and regex (`format_assertion_supported()`) |
| `contentEncoding`, `contentMediaType` | Annotations only; `compile_options::assert_content` returns `unsupported_option` |
| `pattern` | Own linear-time ECMA-262 engine with Annex B escapes; lookaround, named groups and back-references are `unsupported_pattern` |
| Numbers and instances | Exact decimal comparison and divisibility; non-finite numbers and unpaired surrogates are `unsupported_instance` |
| Conformance | Pinned JSON-Schema-Test-Suite draft7, run by the `schema-conformance` workflow; recorded deviations are in `tests/fixtures/third_party/README.md` |

Keep schema errors separate from syntax errors and consumer policies. Runtime
schema resolution must remain explicitly controlled by the caller.
