# Issue consolidation plan — retained full scope

Updated 2026-10-13 from GitHub state and default branch `9e239e97d42303975e7ebeae889d2ee175c51d71`.

## Delivery scope and responsibilities

Retain full JSON5 and strict JSON, complete JSON Schema engine P043–P055, deliberately minimal testing CLI P056–P059, and full advanced memory accounting. Schema and CLI remain independently optional build components for consumers; they are mandatory in this selected delivery scope. Editor P060–P066 and consumer #70/#71 remain intact in the backlog. They are not included in the schedule below; if the complete repository backlog is meant, their effort must be added.

Romanr92 writes all production C++ and public C++ headers. AI writes independent GoogleTest units against complete contracts; AI may maintain authorized build/planning material and must not patch production C++ to make tests pass. The developer resolves implementation failures and reviews/merges implementation PRs.

Remove repetitive per-issue documentation reports, implementation narratives and generic Documentation checkboxes. Keep essential public API, ownership/lifetime, limits/defaults/accounting, cancellation, error, thread-safety, command/package usage, known deviations, support/version/provenance and licensing contracts. Named unit results remain evidence without a separate report for every issue. Required CI stays one GoogleTest job with exceptions OFF/RTTI OFF and one formatting job; no new integration/e2e/fuzz/sanitizer/benchmark gate.

## At most three active issues per subsystem

Reuse existing anchors and preserve assignments, milestones and labels. Each P group covers consecutive requirements. Internal P prerequisites remain unchanged and are followed in order; grouping does not make every internal step ready at once. Full P behavior, original unit acceptance and active J5 technical rows are embedded in each grouped issue.

| Subsystem | Retained issue | Scope |
| --- | --- | --- |
| Unicode/source | [#5](https://github.com/Romanr92/Adaptive_JSON5/issues/5) | P005–P008 |
| Lexer | [#9](https://github.com/Romanr92/Adaptive_JSON5/issues/9) | P009–P012 |
| Strings | [#13](https://github.com/Romanr92/Adaptive_JSON5/issues/13) | P013–P017 |
| Strings | [#18](https://github.com/Romanr92/Adaptive_JSON5/issues/18) | P018–P021 |
| Lexer | [#22](https://github.com/Romanr92/Adaptive_JSON5/issues/22) | P022–P024 |
| Lexer | [#25](https://github.com/Romanr92/Adaptive_JSON5/issues/25) | P025–P028 |
| Values/parser | [#29](https://github.com/Romanr92/Adaptive_JSON5/issues/29) | P029–P032 |
| Values/parser | [#33](https://github.com/Romanr92/Adaptive_JSON5/issues/33) | P033–P036 |
| Values/parser | [#38](https://github.com/Romanr92/Adaptive_JSON5/issues/38) | P037–P042 |
| Schema | [#44](https://github.com/Romanr92/Adaptive_JSON5/issues/44) | P043–P046 |
| Schema | [#48](https://github.com/Romanr92/Adaptive_JSON5/issues/48) | P047–P051 |
| Schema | [#53](https://github.com/Romanr92/Adaptive_JSON5/issues/53) | P052–P055 |
| CLI | [#57](https://github.com/Romanr92/Adaptive_JSON5/issues/57) | P056–P059 |
| Editor | [#61](https://github.com/Romanr92/Adaptive_JSON5/issues/61) | P060–P062 |
| Editor | [#64](https://github.com/Romanr92/Adaptive_JSON5/issues/64) | P063–P064 |
| Editor | [#66](https://github.com/Romanr92/Adaptive_JSON5/issues/66) | P065–P066 |
| Build/release | [#68](https://github.com/Romanr92/Adaptive_JSON5/issues/68) | Finish the C++17 library target and install the public headers |
| Memory/runtime | [#69](https://github.com/Romanr92/Adaptive_JSON5/issues/69) | Full advanced accounting and existing runtime contracts |
| Consumer | [#70](https://github.com/Romanr92/Adaptive_JSON5/issues/70) | Finish the optional versioned consumer schema example |
| Consumer | [#71](https://github.com/Romanr92/Adaptive_JSON5/issues/71) | Finish optional consumer numeric/string policies and result mapping |
| Build/release | [#72](https://github.com/Romanr92/Adaptive_JSON5/issues/72) | Keep essential API, limits, provenance and truthful release evidence |

Foundation #1/#2/#3/#4 remains closed for its actual declaration/result/storage/cursor evidence. #3 is not proof of complete memory accounting. The active P005 branch `5-p005-unicode-source-decoding` remains attached to #5. Unicode 1, lexer 3, strings 2, values/parser 3, schema 3, CLI 1, editor 3, memory/runtime 1, consumer 2 and build/release 2 meet the cap: 21 active implementation/completion issues.

Full advanced accounting in #69 is cross-cutting completion of #29's P031 guards and operation-specific criteria in strings/parser/schema/CLI groups, not a duplicate implementation work stream. It cannot close until all shipped core/schema paths satisfy allocation, owned-byte/work budget, failure-safe diagnostics, bounded cleanup, atomic reuse and concurrency/storage lifetime contracts. #3 stays closed for the primitive foundation.

The CLI is a thin adapter over real library APIs with simple argument dispatch, injectable bounded readers/reporters and required run/inspect/validate behavior. Preserve complete exit mappings, safe strict-JSON output, capability reporting and optional target installation. Demonstration run success means expected outcomes match, including intentionally invalid input; direct validation success means valid input. A small implementation does not weaken these acceptance criteria.

## Status checks and administrative consolidation

Before changes, all 26 still-open child issues to consolidate were unassigned and had zero comments. Repository PR inventory had no open PRs; the only issue-named active branch was P005, whose anchor is retained. Their timelines showed planning cross-references, labels/milestones and renames, with no active implementation evidence. Re-read issue state/body/assignment/comments and branch/PR status immediately before mutations; skip conflicting updates rather than overwrite new work.

Close child issues only after their target body is updated and verified. Add a redirect while preserving original text; use administrative `not_planned` closure, explicitly stating that the requirement remains pending in its target. No issue is closed as completed by this consolidation. Prior consolidation closures are likewise not implementation proof.

| Child issue | Current owner | Step |
| --- | --- | --- |
| #67 | #66 | P066 |
| #65 | #64 | P064 |
| #63 | #61 | P062 |
| #62 | #61 | P061 |
| #60 | #57 | P059 |
| #59 | #57 | P058 |
| #58 | #57 | P057 |
| #56 | #53 | P055 |
| #55 | #53 | P054 |
| #54 | #53 | P053 |
| #52 | #48 | P051 |
| #51 | #48 | P050 |
| #50 | #48 | P049 |
| #49 | #48 | P048 |
| #47 | #44 | P046 |
| #46 | #44 | P045 |
| #45 | #44 | P044 |
| #43 | #38 | P042 |
| #42 | #38 | P041 |
| #41 | #38 | P040 |
| #40 | #38 | P039 |
| #39 | #38 | P038 |
| #37 | #33 | P036 |
| #35 | #33 | P035 |
| #34 | #33 | P034 |
| #32 | #29 | P032 |

The full pre-change live issue snapshot is [issue-backlog-before-consolidation-2026-10-13.json](issue-backlog-before-consolidation-2026-10-13.json). The earlier original baseline [issue-backlog-before-p-sequence.json](issue-backlog-before-p-sequence.json) is retained unchanged. Original J5 IDs, technical contracts, required unit cases and P dependency order are preserved. Historical issue numbers can refer to different scopes; use the snapshot name/date and stable P/J5 IDs.

## Ten-working-day feasibility

**Full retained scope is not a credible ten-working-day commitment from the inspected implementation baseline.** Main contains result/error, input/options, storage/checked arithmetic/owned-block primitives and a bounded cursor. It does not yet contain the full parser, schema validator or executable. PRs #73/#75/#76 merged foundational implementation; #74 reorganized the backlog. Closed consolidated tasks are not completed code.

Planning estimate: approximately **25–45 working days** for one experienced C++ implementer at 6–8 focused hours/day, with AI preparing and maintaining tests concurrently, no major API redesign and no new device/vendor port. This is an engineering judgment, not measured velocity or a deadline promise.

| Remaining work package | Human working days |
| --- | --- |
| Unicode/lexing/strings, owned values, exact numbers, containers, document API and strict JSON | 7–12 |
| Incremental shared advanced memory/accounting integration and failure recovery | 3–6 |
| Complete Draft-07 engine, references, assertions, compilation, conformance and schema-specific budgets | 10–18 |
| Minimal complete P056–P059 CLI wrapper | 1–3 |
| Build/package completion, cross-component fixes and essential release/evidence records | 2–5 |

The package ranges total 23–44 days; use a rounded 25–45-day planning range to reflect uncertain interfaces and integration. Schema-specific allocation/work checks are included in the schema range rather than counted again as shared memory work. AI test-authoring overlaps implementation, but the human still writes production C++, resolves defects and reviews tests. Ten days provides only about 60–80 focused implementation hours under these assumptions.

Use the first ten working days as a checkpoint for implemented/tested parser increments and the shared storage/budget design, with schema/CLI progress only as prerequisites allow. Review actual elapsed implementation time, passing named unit evidence and remaining acceptance after each group. Do not label this checkpoint full completion or silently defer retained features.

The critical path is stable ownership/limits → full grammar/exact operations → owned schema references/equality/bounded evaluator → all assertions/meta-schema/compiler → conformance and CLI validation. Biggest risks are exact arithmetic, fallible growing containers, bounded destruction/partial cleanup, budget-enforceable regex, recursive reference progress, annotation semantics and meta-schema bootstrapping. Consolidation and removing repetitive writing reduce administration, not these implementation dependencies.

If editor and consumer features must also finish in the same milestone, provision approximately 6–12 additional days for editor and 2–5 for consumer work, with further integration uncertainty. Their existing acceptance remains tracked; this estimate does not remove them.

## Verification of the plan

Validate 66 P steps exactly once, unchanged P/J5 requirement and unit strings, unchanged prerequisite order, all 101 stable J5 rows, all issue mappings and at most three anchors per subsystem. Preserve named existing tests and historical results without claiming new test execution. Re-fetch GitHub bodies/states after each update and verify the final 21 active implementation/completion anchors. This planning change adds no production C++ or tests.
