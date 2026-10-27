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
| 2 | AW1: parser attach without ancestor walk | — | small | done (2026-10-25): split `attach_child` / `attach_to_detached_parent` and `document_access` by developer; `attach_child` note and builder O(depth) comments by agent with user authorization | `InternalFaults/J5_039_AttachmentValidation.DetachedParentEntryRejectsDamagedLinksBeforeChangingChild`, `J5_039_DetachedParentAttach.RejectsAttachedParentThatAttachChildAccepts`, `J5_039_DetachedParentAttach.LinksElementAndNamedMemberWithSpan`; public cycle rejection already covered by `J5_039_Document.RejectsRelationshipChangesAtomically` | passed with exceptions OFF and ON (2026-10-25); full suite 2329/2334, the 5 failures are the pending audit units |
| 3 | L1: literals by byte comparison | — | small | done by developer (2026-10-26): `match_literal_bytes` before the identifier path in `consume_scalar`; non-literals still rejected through `consume_identifier` so errors are unchanged | `J5_046_ScalarLexer.LiteralsDoNotAllocate` (both modes, with boundaries), `J5_046_ScalarLexer.NonFiniteSpellingsDoNotAllocate`; `P028/J5_024_TokenRejection` cases `LiteralEscapedPart`, `LiteralEscapedPoint`, `LiteralUnicodePart`, `TruncatedNull`; `J5_046_ScalarStorage.FailsEveryAllocationAndAllowsRetry` and `EnforcesLimitsAndReleasesBoundaryFailures` updated, because literals no longer have allocation points or a unit limit | passed with exceptions OFF and ON (2026-10-26); full suite 2339/2343, the 4 failures are the pending audit units for steps 4, 5 and 9. Correction (2026-10-26): `LiteralEscapedPart` and `LiteralEscapedPoint` had lost their backslash when written, so that run tested `trueA` and `true.`; restored and passed with the step 4 run |
| 4 | C4: budget-limited scan view | — | medium | done by developer (2026-10-26): `limited_scan` helpers and shared `skip_trivia_within_budget` in `parser_work`; engine `read_value`, `read_name`, trivia and `primitive_parser` scan through the limited view; warning re-scan charged up front; `unknown_literal` carries its range; comment fixes by agent with user authorization | `J5_049_ParserWork.ChargesScanBeforeLexing`, `J5_049_ParserWork.WarningRescanIsCharged`, `J5_049/J5_049_ScanCut.BudgetBelowMinimumFailsWithResourceLimit`, `J5_049/J5_049_ScanCutError.KeepsLexerErrorBeforeCut`, `J5_024_Scalar.ReportsUnknownLiteralRange`; re-baselined work counts in `P037/J5_047_CompleteLimit` (`WorkIncludesTrailingScan` 17→14) and `P032/J5_047_PrimitiveLimit` (`Work*` cases); `J5_044_DiagnosticLocation` `TokenAfter*` cases now expect the unknown-literal range | passed with exceptions OFF and ON (2026-10-26); full suite 2353/2355, the 2 failures are the pending audit units for steps 5 and 9 |
| 5 | S4: string counting walk and exact decode | 4 | medium | done (2026-10-27): `decoded_string_builder::reserve_exact` and removal of the growth path, two-pass `walk_string_body` (string lexer) and `walk_identifier` (identifier lexer), decode pass charged after the token in `read_value`, `read_name` and `consume_primitive`, by developer; `consume_string` guard and comment fixes by agent with user authorization | `J5_046_DecodedStringBuilder.FinishRetainsExactUnits` (restated through the lexer), `J5_046_StringBuilder.ReservationFailureAllowsRetry`, `J5_046_StringBuilder.PairBeyondReservationIsAtomic`, `J5_046_StringBuilder.AppendPastReservationFailsWithoutAllocating`, `J5_049_ParserWork.StringDecodePassIsCharged`, `J5_049_ParserWork.MemberNameDecodePassIsCharged`, `P032/J5_047_PrimitiveLimit` cases `StringWorkDenied`/`StringWorkExact`; builder tests and helper uses reserve first; string and identifier allocation tests expect one exact allocation at the token start | passed with exceptions OFF and ON (2026-10-27); full suite 2355/2356, the 1 failure is the step 9 audit unit |
| 6 | W2 default, adaptive policy, F1 kept (public API) | — | medium | not started | — | — |
| 7 | V3: counting pre-pass, exact records and frames | 1, 3, 4, 5 | large | not started | — | — |
| 8 | 7b: default work 131072 and worst-case evidence | 4, 5, 7 | small | not started | — | — |
| 9 | A2, B1, 1c: numeric pre-sizing, 10^4 chunks, charging | — | medium | not started | pending: `J5_046_BigUnsigned.DigitGrowthIsGeometric` (to restate as one reservation) | fails before the change |

Step 9 is independent and can be done in parallel with steps 1–8.
