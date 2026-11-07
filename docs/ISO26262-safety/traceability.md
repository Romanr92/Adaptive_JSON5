# Traceability

| Item | Value |
| --- | --- |
| Work product | Traceability (ISO 26262-6, clause 7 and clause 9; ISO 26262-8, clause 6) |
| Issue | [#138](https://github.com/Romanr92/Adaptive_JSON5/issues/138) |
| Status | Draft. The structure is in place; most links are still "missing". |

The traceability itself is kept in one place, next to the existing requirement
traceability, so that it cannot drift apart from it:

- **[Safety traceability (ISO 26262)](../traceability.md#safety-traceability-iso-26262)**
  in `docs/traceability.md`: one row per software safety requirement, tracing
  SEooC assumption → SSR → J5 rows → software units → unit tests → FMEA rows.
- **[Requirement traceability](../traceability.md)**, the rest of `docs/traceability.md`:
  each J5 row with its source files, functions and named unit tests.
- **[Software unit ID registry](../../core/docs/software-unit-ids.md)** in
  `core/docs/`: every `SWU-<MODULE>-<number>` ID, its function and file, and whether
  it is used or deleted. Each production function carries its ID in an `@par ID` line
  in its Doxygen block.

## The chain

| Link | Where it is defined |
| --- | --- |
| SEooC assumptions A-01 to A-08 | [`seooc-assumptions.md`](seooc-assumptions.md) |
| Software safety requirements SSR-01 to SSR-08 | [`docs/requirements.md`, section K](../requirements.md#k-software-safety-requirements-iso-26262) |
| J5 requirement rows | [`docs/requirements.md`](../requirements.md) |
| Software units (`SWU-…`) | `@par ID` in the Doxygen of each function; [registry](../../core/docs/software-unit-ids.md) |
| Unit tests (`SuiteName.TestName`) | `tests/`, `core/tests/`; inventory in [`docs/traceability.md`](../traceability.md) |
| FMEA rows (IN-, LX-, PA-, VM-, ME-, DG-, AP-) | [`software-safety-analysis.md`](software-safety-analysis.md) |

## How it is kept up to date

The chain is completed during the verification step of the `guide-requirement`
skill (`.agents/skills/guide-requirement/SKILL.md`). Each time the unit tests for a
requirement are written or named, the agent records the full chain for every SSR the
requirement serves, and writes "missing" for any link that does not exist yet. New
functions take the next free ID from the registry, following the
[C++ coding style](../../core/docs/design-coding-style.md).

The milestone safety review (see the [safety plan](safety-plan.md), section 6) checks
that no row still says "missing" for a released feature.
