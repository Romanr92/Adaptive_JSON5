# Requirement traceability

Every requirement has an owning implementation issue. Issues #2 onward now follow
P002–P066; final completion issues #68–#72 retain the build/runtime/consumer/release
work outside those steps. P002 is complete as a component; broader J5 contracts
remain Draft. Issue #1 remains the public-contract umbrella. All functional contracts
remain **Draft**; none is declared fully implemented or verified by this setup.
The configuration units and P002 result/error units provide partial evidence
for their owning requirements. Test existence, a passing execution and complete
requirement coverage are distinct.

Keep exact `SuiteName.TestName` declarations in the Unit Tests inventory, separated
by `; ` when several units cover one requirement. Do not add generated parameter
case suffixes. Record actual execution in [verification notes](verification.md).

| ID | Contract | Status | Owning issue | Unit Tests |
| --- | --- | --- | --- | --- |
| J5-001 | Standalone C++ library | Draft | [#68](https://github.com/Romanr92/Adaptive_JSON5/issues/68) | `J5_001_BuildConfiguration.UsesCxx17OrNewer` |
| J5-002 | Build components | Draft | [#68](https://github.com/Romanr92/Adaptive_JSON5/issues/68) |  |
| J5-003 | Exception-independent API | Draft | [#2](https://github.com/Romanr92/Adaptive_JSON5/issues/2) | `J5_003_BuildConfiguration.AppliesSelectedExceptionMode`; `J5_003_Result.StatusSuccessHasNoError`; `J5_003_Result.ValueSuccessHasNoError`; `J5_003_Result.MutableAccessUpdatesStoredValue`; `J5_003_Result.CopyConstructionPreservesAlternatives`; `J5_003_Result.MoveConstructionPreservesAlternatives`; `J5_003_Result.SupportsMoveOnlyValueOwnership`; `J5_003_Result.SameTypeAlternativesRemainDistinct`; `J5_003_Result.StatusCopyAndMovePreserveFailure`; `J5_003_Result.StatusAssignmentTransitionsBothWays`; `J5_003_Result.BalancesSuccessAlternativeLifetimes`; `J5_003_Result.BalancesErrorAlternativeLifetimes` |
| J5-004 | Input boundary | Draft | [#4](https://github.com/Romanr92/Adaptive_JSON5/issues/4) |  |
| J5-005 | Syntax selection | Draft | [#43](https://github.com/Romanr92/Adaptive_JSON5/issues/43) |  |
| J5-006 | Unicode data baseline | Draft | [#6](https://github.com/Romanr92/Adaptive_JSON5/issues/6) |  |
| J5-007 | License and provenance | Draft | [#68](https://github.com/Romanr92/Adaptive_JSON5/issues/68) |  |
| J5-008 | Public contracts | Draft | [#72](https://github.com/Romanr92/Adaptive_JSON5/issues/72) |  |
| J5-009 | UTF-8 source | Draft | [#5](https://github.com/Romanr92/Adaptive_JSON5/issues/5) |  |
| J5-010 | JSON whitespace | Draft | [#43](https://github.com/Romanr92/Adaptive_JSON5/issues/43) |  |
| J5-011 | JSON5 whitespace | Draft | [#7](https://github.com/Romanr92/Adaptive_JSON5/issues/7) |  |
| J5-012 | Comments | Draft | [#9](https://github.com/Romanr92/Adaptive_JSON5/issues/9) |  |
| J5-013 | Structural tokens | Draft | [#12](https://github.com/Romanr92/Adaptive_JSON5/issues/12) |  |
| J5-014 | Quoted strings | Draft | [#14](https://github.com/Romanr92/Adaptive_JSON5/issues/14) |  |
| J5-015 | JSON escapes | Draft | [#15](https://github.com/Romanr92/Adaptive_JSON5/issues/15) |  |
| J5-016 | JSON5 escapes | Draft | [#15](https://github.com/Romanr92/Adaptive_JSON5/issues/15) |  |
| J5-017 | Continuations and separators | Draft | [#7](https://github.com/Romanr92/Adaptive_JSON5/issues/7) |  |
| J5-018 | Surrogate fidelity | Draft | [#13](https://github.com/Romanr92/Adaptive_JSON5/issues/13) |  |
| J5-019 | Unquoted member names | Draft | [#6](https://github.com/Romanr92/Adaptive_JSON5/issues/6) |  |
| J5-020 | Escaped identifiers | Draft | [#23](https://github.com/Romanr92/Adaptive_JSON5/issues/23) |  |
| J5-021 | JSON numbers | Draft | [#25](https://github.com/Romanr92/Adaptive_JSON5/issues/25) |  |
| J5-022 | JSON5 numbers | Draft | [#25](https://github.com/Romanr92/Adaptive_JSON5/issues/25) |  |
| J5-023 | Primitive literals | Draft | [#24](https://github.com/Romanr92/Adaptive_JSON5/issues/24) |  |
| J5-024 | Token boundaries | Draft | [#11](https://github.com/Romanr92/Adaptive_JSON5/issues/11) |  |
| J5-025 | BOM policy | Draft | [#8](https://github.com/Romanr92/Adaptive_JSON5/issues/8) |  |
| J5-026 | Complete input | Draft | [#38](https://github.com/Romanr92/Adaptive_JSON5/issues/38) |  |
| J5-027 | Language completeness | Draft | [#28](https://github.com/Romanr92/Adaptive_JSON5/issues/28) |  |
| J5-028 | Value types | Draft | [#30](https://github.com/Romanr92/Adaptive_JSON5/issues/30) |  |
| J5-029 | Length-aware strings | Draft | [#13](https://github.com/Romanr92/Adaptive_JSON5/issues/13) |  |
| J5-030 | Ordered arrays | Draft | [#30](https://github.com/Romanr92/Adaptive_JSON5/issues/30) |  |
| J5-031 | Observable object members | Draft | [#30](https://github.com/Romanr92/Adaptive_JSON5/issues/30) |  |
| J5-032 | Duplicate policy | Draft | [#37](https://github.com/Romanr92/Adaptive_JSON5/issues/37) |  |
| J5-033 | Name equality | Draft | [#23](https://github.com/Romanr92/Adaptive_JSON5/issues/23) |  |
| J5-034 | Exact number storage | Draft | [#29](https://github.com/Romanr92/Adaptive_JSON5/issues/29) |  |
| J5-035 | Checked integer conversion | Draft | [#39](https://github.com/Romanr92/Adaptive_JSON5/issues/39) |  |
| J5-036 | Value and spelling queries | Draft | [#39](https://github.com/Romanr92/Adaptive_JSON5/issues/39) |  |
| J5-037 | Floating conversion | Draft | [#39](https://github.com/Romanr92/Adaptive_JSON5/issues/39) |  |
| J5-038 | Special numbers | Draft | [#29](https://github.com/Romanr92/Adaptive_JSON5/issues/29) |  |
| J5-039 | Checked access | Draft | [#30](https://github.com/Romanr92/Adaptive_JSON5/issues/30) |  |
| J5-040 | Document lifetime | Draft | [#3](https://github.com/Romanr92/Adaptive_JSON5/issues/3) |  |
| J5-041 | Source spans | Draft | [#4](https://github.com/Romanr92/Adaptive_JSON5/issues/4) |  |
| J5-042 | Pure memory parsing | Draft | [#69](https://github.com/Romanr92/Adaptive_JSON5/issues/69) |  |
| J5-043 | Structured errors | Draft | [#2](https://github.com/Romanr92/Adaptive_JSON5/issues/2) | `J5_043_LocatedResult.PreservesCategoryAndOffset`; `J5_043_UnlocatedResult.DistinguishesMissingLocationFromByteZero` |
| J5-044 | Diagnostic locations | Draft | [#4](https://github.com/Romanr92/Adaptive_JSON5/issues/4) |  |
| J5-045 | Failure-safe reporting | Draft | [#2](https://github.com/Romanr92/Adaptive_JSON5/issues/2) | `J5_045_Result.ReportsAllocationFailureWithoutRichDiagnostics` |
| J5-046 | Fallible allocation | Draft | [#3](https://github.com/Romanr92/Adaptive_JSON5/issues/3) |  |
| J5-047 | Configurable limits | Draft | [#31](https://github.com/Romanr92/Adaptive_JSON5/issues/31) |  |
| J5-048 | Bounded traversal | Draft | [#31](https://github.com/Romanr92/Adaptive_JSON5/issues/31) |  |
| J5-049 | Work budgets | Draft | [#31](https://github.com/Romanr92/Adaptive_JSON5/issues/31) |  |
| J5-050 | Concurrency | Draft | [#69](https://github.com/Romanr92/Adaptive_JSON5/issues/69) |  |
| J5-051 | Atomic results and reuse | Draft | [#3](https://github.com/Romanr92/Adaptive_JSON5/issues/3) |  |
| J5-052 | Deterministic semantics | Draft | [#69](https://github.com/Romanr92/Adaptive_JSON5/issues/69) |  |
| J5-053 | Schema dialect | Draft | [#44](https://github.com/Romanr92/Adaptive_JSON5/issues/44) |  |
| J5-054 | Schema compilation | Draft | [#55](https://github.com/Romanr92/Adaptive_JSON5/issues/55) |  |
| J5-055 | Identifiers and references | Draft | [#45](https://github.com/Romanr92/Adaptive_JSON5/issues/45) |  |
| J5-056 | Reference semantics | Draft | [#47](https://github.com/Romanr92/Adaptive_JSON5/issues/47) |  |
| J5-057 | Assertion coverage | Draft | [#48](https://github.com/Romanr92/Adaptive_JSON5/issues/48) |  |
| J5-058 | Annotations and defaults | Draft | [#54](https://github.com/Romanr92/Adaptive_JSON5/issues/54) |  |
| J5-059 | Exact numeric validation | Draft | [#46](https://github.com/Romanr92/Adaptive_JSON5/issues/46) |  |
| J5-060 | String and pattern semantics | Draft | [#50](https://github.com/Romanr92/Adaptive_JSON5/issues/50) |  |
| J5-061 | Structural equality | Draft | [#46](https://github.com/Romanr92/Adaptive_JSON5/issues/46) |  |
| J5-062 | Extension keywords | Draft | [#54](https://github.com/Romanr92/Adaptive_JSON5/issues/54) |  |
| J5-063 | Validation outcomes | Draft | [#44](https://github.com/Romanr92/Adaptive_JSON5/issues/44) |  |
| J5-064 | Schema diagnostics | Draft | [#45](https://github.com/Romanr92/Adaptive_JSON5/issues/45) |  |
| J5-065 | Evaluation budgets | Draft | [#44](https://github.com/Romanr92/Adaptive_JSON5/issues/44) |  |
| J5-066 | Compiled-schema lifetime | Draft | [#44](https://github.com/Romanr92/Adaptive_JSON5/issues/44) |  |
| J5-067 | Pure evaluation | Draft | [#45](https://github.com/Romanr92/Adaptive_JSON5/issues/45) |  |
| J5-068 | Component independence | Draft | [#44](https://github.com/Romanr92/Adaptive_JSON5/issues/44) |  |
| J5-069 | Optional assertions | Draft | [#54](https://github.com/Romanr92/Adaptive_JSON5/issues/54) |  |
| J5-070 | Validation CLI | Draft | [#57](https://github.com/Romanr92/Adaptive_JSON5/issues/57) |  |
| J5-071 | Machine diagnostics | Draft | [#58](https://github.com/Romanr92/Adaptive_JSON5/issues/58) |  |
| J5-072 | Editor packaging | Draft | [#61](https://github.com/Romanr92/Adaptive_JSON5/issues/61) |  |
| J5-073 | External schema association | Draft | [#64](https://github.com/Romanr92/Adaptive_JSON5/issues/64) |  |
| J5-074 | Grammar agreement | Draft | [#62](https://github.com/Romanr92/Adaptive_JSON5/issues/62) |  |
| J5-075 | Location mapping | Draft | [#62](https://github.com/Romanr92/Adaptive_JSON5/issues/62) |  |
| J5-076 | Live document versions | Draft | [#63](https://github.com/Romanr92/Adaptive_JSON5/issues/63) |  |
| J5-077 | Incomplete-input assistance | Draft | [#63](https://github.com/Romanr92/Adaptive_JSON5/issues/63) |  |
| J5-078 | Schema completion | Draft | [#65](https://github.com/Romanr92/Adaptive_JSON5/issues/65) |  |
| J5-079 | Hover and schema diagnostics | Draft | [#66](https://github.com/Romanr92/Adaptive_JSON5/issues/66) |  |
| J5-080 | Consumer-policy diagnostics | Draft | [#66](https://github.com/Romanr92/Adaptive_JSON5/issues/66) |  |
| J5-081 | Offline operation | Draft | [#64](https://github.com/Romanr92/Adaptive_JSON5/issues/64) |  |
| J5-082 | Editor unit coverage | Draft | [#67](https://github.com/Romanr92/Adaptive_JSON5/issues/67) |  |
| J5-083 | Manifest profile boundary | Draft | [#70](https://github.com/Romanr92/Adaptive_JSON5/issues/70) |  |
| J5-084 | Versioned schema artifact | Draft | [#70](https://github.com/Romanr92/Adaptive_JSON5/issues/70) |  |
| J5-085 | Field and policy coverage | Draft | [#70](https://github.com/Romanr92/Adaptive_JSON5/issues/70) |  |
| J5-086 | Unknown versus dynamic names | Draft | [#70](https://github.com/Romanr92/Adaptive_JSON5/issues/70) |  |
| J5-087 | Numeric-policy configuration | Draft | [#71](https://github.com/Romanr92/Adaptive_JSON5/issues/71) |  |
| J5-088 | String and path boundaries | Draft | [#71](https://github.com/Romanr92/Adaptive_JSON5/issues/71) |  |
| J5-089 | Defaults and arguments | Draft | [#71](https://github.com/Romanr92/Adaptive_JSON5/issues/71) |  |
| J5-090 | Result/error mapping | Draft | [#71](https://github.com/Romanr92/Adaptive_JSON5/issues/71) |  |
| J5-091 | Native portability | Draft | [#68](https://github.com/Romanr92/Adaptive_JSON5/issues/68) |  |
| J5-092 | Reusable package | Draft | [#68](https://github.com/Romanr92/Adaptive_JSON5/issues/68) |  |
| J5-093 | Versioned consumption | Draft | [#68](https://github.com/Romanr92/Adaptive_JSON5/issues/68) |  |
| J5-094 | Grammar unit regression coverage | Draft | [#41](https://github.com/Romanr92/Adaptive_JSON5/issues/41) |  |
| J5-095 | Pinned syntax fixtures | Draft | [#41](https://github.com/Romanr92/Adaptive_JSON5/issues/41) |  |
| J5-096 | Draft-07 unit conformance fixtures | Draft | [#56](https://github.com/Romanr92/Adaptive_JSON5/issues/56) |  |
| J5-097 | Deterministic robustness unit cases | Draft | [#41](https://github.com/Romanr92/Adaptive_JSON5/issues/41) |  |
| J5-098 | Fault-injection evidence | Draft | [#41](https://github.com/Romanr92/Adaptive_JSON5/issues/41) |  |
| J5-099 | Portable unit build matrix | Draft | [#72](https://github.com/Romanr92/Adaptive_JSON5/issues/72) |  |
| J5-100 | Release completeness | Draft | [#72](https://github.com/Romanr92/Adaptive_JSON5/issues/72) |  |
| J5-101 | OS-agnostic embedded integration | Draft | [#69](https://github.com/Romanr92/Adaptive_JSON5/issues/69) | `J5_101_BuildConfiguration.AppliesSelectedRttiMode` |
