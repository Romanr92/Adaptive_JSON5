# Contributing

Start with [development setup](docs/development.md), the ordered
[implementation sequence](docs/requirements/json5-implementation-order.md) and [requirements](docs/requirements.md).
Pick a ready issue whose prerequisites are complete. Keep the generic parser
independent of consumer fields, operating systems and optional host tooling.

Follow the [C++ coding style](docs/coding-style.md), including the `m_` prefix
without a trailing underscore for class data members, and the tracked
clang-format rules.

Discuss public API, ownership, exact-number, Unicode and allocation choices in
the relevant issue before building dependent features. Record durable decisions
in `docs/adr/`; preserve stable requirement IDs when contracts change.

For each implementation change:

1. State the behavior and requirement IDs it implements.
2. Add meaningful unit cases for normal behavior, boundaries and explicit errors.
3. Run the unit suite with exceptions OFF and RTTI OFF, then check clang-format 18.
4. Retain named unit mappings/results and update essential API/ownership/error/limit,
   provenance and usage contracts where behavior changes. No per-issue documentation
   report or implementation narrative is required.
5. Submit a focused pull request using the template.

The only automated quality checks are unit tests and formatting. Do not add
integration, end-to-end, sanitizer, fuzz, benchmark, static-analysis or coverage
gates without a later scope decision. Required fixture coverage belongs in unit
tests, with pinned versions, licenses and expected behavior.

Do not mark a Draft requirement Verified just because an issue closes. Record
the named test and executed result first. Do not claim full JSON5 or Draft-07
support while its production or keyword coverage matrix has gaps.

Original contributions use the repository's Apache-2.0 license. Document the
source, immutable version and license of any dependency, generated data or
fixture before adding it. No contribution agreement or external project workflow
is introduced by this repository setup.

Read [licensing guidance](docs/licensing.md) and retain applicable upstream notices
when contributing copied or adapted material. Repository copyright attribution is
recorded in [NOTICE](NOTICE); third-party rights remain with their owners.

The [issue tracking section](docs/requirements/json5-implementation-order.md#delivery-scope-and-issue-tracking) groups consecutive P
steps without reducing technical acceptance. Romanr92 implements production C++;
AI creates independent GoogleTest units and does not fix production files. Full
schema, minimal testing CLI and advanced accounting remain in the selected delivery.
