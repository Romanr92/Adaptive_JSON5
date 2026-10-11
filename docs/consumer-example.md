# Optional consumer example

J5-083–090 describe a later, independently usable manifest profile inspired by
AdaptivePi. This is an optional example of generic parsing plus external schema
and consumer policy, not core parser behavior. No AdaptivePi code, build policy,
service manager, SDK or deployment workflow is imported. The example may be
implemented after the generic components it uses.

The original proposal snapshot follows as contextual input for that example.
Its consumer-specific unresolved choices require reconciliation only if the
example is advertised as compatible with an actual AdaptivePi contract.

## Proposal contract snapshot


This is a consumer-compatibility checklist, **not newly agreed AdaptivePi
wording**. It makes the file useful when copied to the new repository. Reconcile
it with the current consumer file before publishing the example schema.

| Area | Current requirement-derived contract | Where enforced |
| --- | --- | --- |
| Root/version | Object root; required `schema_version` with integer value 1. Numeric spelling is unresolved. | Profile plus schema; a spelling rule needs an additional token check. |
| Fixed root members | Required `schema_version`, `application_id`, `executable`, `arguments`, `working_directory`, `restart_policy`; optional `environment`, `service_unit`. | Required/types/additional-property schema rules. |
| Identity | String matching `[A-Za-z][A-Za-z0-9_]*`; schema regex must be anchored to check the whole value. | Schema. |
| Arguments | Ordered array of strings; empty array and empty strings valid; embedded NUL invalid; excludes executable argument zero. | Schema/value checks; launch-vector assembly in AdaptivePi. |
| Environment | Optional object of string values, defaulting to empty at the consumer; names match `[A-Za-z_][A-Za-z0-9_]*`; values can be empty but cannot contain NUL. | Schema `propertyNames`/value rules plus consumer default. |
| Restart | Object containing exactly `mode` and `max_attempts`; mode `never` or `on_failure`; count integral in 0–100 and never a boolean; `never` requires 0. | Schema types/ranges/conditional rule plus any agreed spelling policy. |
| Service unit | Optional string matching `[A-Za-z][A-Za-z0-9_-]*\.service`; omitted value becomes `adaptive-pi-<application_id>.service`; no NUL. | Anchored schema pattern and consumer-derived default. |
| Executable/work paths | Nonempty relative strings under `bin/` and `work/`, no NUL or `.`/`..` components. Exact allowed path spellings remain consumer-owned. | Schema/profile for lexical rules. |
| Filesystem | Canonical/symlink-resolved paths stay within package root; executable is an executable regular file; work path is an existing directory. | AdaptivePi package validation, not the generic parser or editor schema. |
| Duplicate keys | Reject within each object after decoding; dynamic environment names are also checked. | Parser/profile; JSON Schema alone cannot recover duplicate members. |
| Unknown members | Fixed schema objects reject unknown fields; environment uses dynamic-name rules. This interpretation requires MAN-001 review. | Schema/profile; clarification remains proposed. |
| Ownership/errors | Complete owned result, no temporary-buffer references, explicit errors and no published partial configuration. | Library contracts and consumer adapter. |

The current AdaptivePi requirements specify JSON5 1.0.0 manifests named
`manifest.json5`. MAN-001–015 remain In Review; the format and filename edits
do not establish agreement to the full manifest contracts.
The generic library does not supervise services, install packages or implement
filesystem security. Final C++ consumer-model allocation also needs an agreed
failure strategy; the parser's fallible allocator does not make `std::string` or
other unrelated consumer allocations recoverable automatically.

