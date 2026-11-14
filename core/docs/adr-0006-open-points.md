# ADR 0006 open points (temporary)

**Temporary working note for issue #44. Delete this file before the branch is
merged.** Settled points move into
[ADR 0006](adr-0006-schema-component.md); nothing here is a decision.

Found in the 2026-11-14 review of ADR 0006 decisions 1–13.

## Gaps needing a decision

| # | Topic | Gap | Suggested resolution |
| --- | --- | --- | --- |
| O1 | Plain-name `$id` (decisions 8, 10) | Draft-07 location-independent identifiers (`$id: "#foo"`, `$ref: "#foo"`) are not covered; only JSON Pointer fragments are. Required suite cases test this. | Register a plain-name fragment `$id` as an extra lookup key for its node. |
| O2 | `$id` beside `$ref` (decisions 8, 12) | Decision 12 ignores sibling assertions only. Draft-07 also ignores a sibling `$id` for base-URI changes. | State that a `$id` beside `$ref` is ignored at compile time. |
| O3 | Registration errors (decisions 3, 9) | Duplicate URI is "an error" without a reason code; the reason table has no registration phase; a relative registered URI is undefined. | Add a registration column and `invalid_registration` (sub-reasons: duplicate URI, relative URI). |
| O4 | Root base URI (decision 8) | No base URI is defined for a root schema without `$id`, which decides whether relative `$ref`s find registered resources. | Fixed, documented default base URI. |
| O5 | Unpaired surrogates in schema text (decisions 6, 9) | Strict JSON accepts `"\ud800"`; decision 6 covers only the instance. | `invalid_schema` with a domain sub-reason. |

## Tidy-ups

| # | Topic | Change |
| --- | --- | --- |
| O6 | Decision 13 | State the lifetime of diagnostic source ranges (registered schema text, parsed instance document). |
| O7 | Decision 4 | Say where compile's temporary storage comes from. |
| O8 | Decision 6 | Name the budget field the pre-pass is charged to (presumably `max_work_steps`). |
| O9 | ADR status | Header still says "In progress" with all 13 decisions settled. |
| O10 | Issue #44 | Completion checklist lists only unit and formatting jobs; update to all AGENTS.md gates. |
