# Temporary: ADR 0003 implementation order

> **Temporary document.** It tracks implementing the decisions in
> [ADR 0003](adr-0003-storage-and-budget-decisions.md) and **will be deleted once
> all steps are implemented**. Permanent records are ADR 0003, the
> [user guide](../../docs/memory-and-limits.md) and
> [traceability](../../docs/traceability.md).

Workflow for each step:

1. Walkthrough.
2. The developer writes the production C++.
3. Optional review.
4. Units for the step.
5. Run with exceptions OFF and ON, run clang-format, and record the results in
   traceability.

Implementation, unit existence and executed results are tracked separately.

| Step | Decision | Depends on | Size | Implementation | Units | Executed |
|---|---|---|---|---|---|---|
| 1 | K1: single string-unit counter in `resource_budget` | — | small | done by developer (2026-10-25): `string_unit_allowance`; engine counter removed | `P031/J5_047_StringAllowance.ReportsMinimumOfPerStringAndRemainingTotal`, `J5_047_StringAllowance.TracksReservationsAndReleases`, `P037/J5_047_CompleteLimit` case `AggregateAcrossNamesAndValues` | passed with exceptions OFF and ON (2026-10-25); full suite 2314/2319, the 5 failures are the pending audit units |
| 2 | AW1: parser attach without ancestor walk | — | small | not started | — | — |
| 3 | L1: literals by byte comparison | — | small | not started | pending: `J5_046_ScalarLexer.LiteralsDoNotAllocate` | fails before the change |
| 4 | C4: budget-limited scan view | — | medium | not started | pending: `J5_049_ParserWork.ChargesScanBeforeLexing`, `J5_049_ParserWork.WarningRescanIsCharged` | fail before the change |
| 5 | S4: string counting walk and exact decode | 4 | medium | not started | pending: `J5_046_DecodedStringBuilder.FinishRetainsExactUnits` | fails before the change |
| 6 | W2 default, adaptive policy, F1 kept (public API) | — | medium | not started | — | — |
| 7 | V3: counting pre-pass, exact records and frames | 1, 3, 4, 5 | large | not started | — | — |
| 8 | 7b: default work 131072 and worst-case evidence | 4, 5, 7 | small | not started | — | — |
| 9 | A2, B1, 1c: numeric pre-sizing, 10^4 chunks, charging | — | medium | not started | pending: `J5_046_BigUnsigned.DigitGrowthIsGeometric` (to restate as one reservation) | fails before the change |

Step 9 is independent and can be done in parallel with steps 1–8.
