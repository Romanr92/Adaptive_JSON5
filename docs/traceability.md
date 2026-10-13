# Requirement traceability

Every requirement has an owning implementation issue. All functional contracts
remain **Draft**; none is declared fully implemented or verified by this setup.
The configuration, P002 result/error and input/options units provide partial evidence
for their owning requirements. Test existence, a passing execution and complete
requirement coverage are distinct.

Keep exact `SuiteName.TestName` declarations in the Unit Tests inventory, separated
by `; ` when several units cover one requirement. Do not add generated parameter
case suffixes. Record actual execution in [verification notes](verification.md).

| ID | Contract | Status | Owning issue | Unit Tests |
| --- | --- | --- | --- | --- |
| J5-001 | Standalone C++ library | Draft | [#1](https://github.com/Romanr92/Adaptive_JSON5/issues/1) | `J5_001_BuildConfiguration.UsesCxx17OrNewer` |
| J5-002 | Build components | Draft | [#2](https://github.com/Romanr92/Adaptive_JSON5/issues/2) |  |
| J5-003 | Exception-independent API | Draft | [#1](https://github.com/Romanr92/Adaptive_JSON5/issues/1) | `J5_003_BuildConfiguration.AppliesSelectedExceptionMode`; `J5_003_Result.StatusSuccessHasNoError`; `J5_003_Result.ValueSuccessHasNoError`; `J5_003_Result.MutableAccessUpdatesStoredValue`; `J5_003_Result.CopyConstructionPreservesAlternatives`; `J5_003_Result.MoveConstructionPreservesAlternatives`; `J5_003_Result.SupportsMoveOnlyValueOwnership`; `J5_003_Result.SameTypeAlternativesRemainDistinct`; `J5_003_Result.StatusCopyAndMovePreserveFailure`; `J5_003_Result.StatusAssignmentTransitionsBothWays`; `J5_003_Result.BalancesSuccessAlternativeLifetimes`; `J5_003_Result.BalancesErrorAlternativeLifetimes` |
| J5-004 | Input boundary | Draft | [#1](https://github.com/Romanr92/Adaptive_JSON5/issues/1) | `J5_004_InputView.DefaultsToEmptyBuffer`; `J5_004_InputView.AllowsNonNullEmptyBuffer`; `J5_004_InputView.DescribesReadOnlyNonTerminatedPrefix`; `J5_004_InputView.RetainsEmbeddedZeroInByteCount`; `J5_004_InputView.BorrowsCallerStorageWithoutCopying`; `J5_004_Cursor.DefaultConstructedIsAtEnd`; `J5_004_Cursor.EmptyNonNullViewIsAtEnd`; `J5_004_Cursor.ConsumesExactLengthWithoutTerminatingNul`; `J5_004_Cursor.ReadsAcrossEmbeddedZeroBytes`; `J5_004_Cursor.PeekLookaheadBoundsAgainstRemaining`; `J5_004_Cursor.AdvanceClampsAtEnd` |
| J5-005 | Syntax selection | Draft | [#1](https://github.com/Romanr92/Adaptive_JSON5/issues/1) | `J5_005_ParseOptions.DefaultsToJson5`; `J5_005_ParseOptions.SelectsJson5Explicitly`; `J5_005_ParseOptions.SelectsStrictJsonExplicitly` |
| J5-006 | Unicode data baseline | Draft | [#3](https://github.com/Romanr92/Adaptive_JSON5/issues/3) |  |
| J5-007 | License and provenance | Draft | [#2](https://github.com/Romanr92/Adaptive_JSON5/issues/2) |  |
| J5-008 | Public contracts | Draft | [#1](https://github.com/Romanr92/Adaptive_JSON5/issues/1) |  |
| J5-009 | UTF-8 source | Draft | [#3](https://github.com/Romanr92/Adaptive_JSON5/issues/3) |  |
| J5-010 | JSON whitespace | Draft | [#3](https://github.com/Romanr92/Adaptive_JSON5/issues/3) |  |
| J5-011 | JSON5 whitespace | Draft | [#3](https://github.com/Romanr92/Adaptive_JSON5/issues/3) |  |
| J5-012 | Comments | Draft | [#4](https://github.com/Romanr92/Adaptive_JSON5/issues/4) |  |
| J5-013 | Structural tokens | Draft | [#4](https://github.com/Romanr92/Adaptive_JSON5/issues/4) |  |
| J5-014 | Quoted strings | Draft | [#5](https://github.com/Romanr92/Adaptive_JSON5/issues/5) |  |
| J5-015 | JSON escapes | Draft | [#5](https://github.com/Romanr92/Adaptive_JSON5/issues/5) |  |
| J5-016 | JSON5 escapes | Draft | [#5](https://github.com/Romanr92/Adaptive_JSON5/issues/5) |  |
| J5-017 | Continuations and separators | Draft | [#5](https://github.com/Romanr92/Adaptive_JSON5/issues/5) |  |
| J5-018 | Surrogate fidelity | Draft | [#5](https://github.com/Romanr92/Adaptive_JSON5/issues/5) |  |
| J5-019 | Unquoted member names | Draft | [#6](https://github.com/Romanr92/Adaptive_JSON5/issues/6) |  |
| J5-020 | Escaped identifiers | Draft | [#6](https://github.com/Romanr92/Adaptive_JSON5/issues/6) |  |
| J5-021 | JSON numbers | Draft | [#7](https://github.com/Romanr92/Adaptive_JSON5/issues/7) |  |
| J5-022 | JSON5 numbers | Draft | [#7](https://github.com/Romanr92/Adaptive_JSON5/issues/7) |  |
| J5-023 | Primitive literals | Draft | [#7](https://github.com/Romanr92/Adaptive_JSON5/issues/7) |  |
| J5-024 | Token boundaries | Draft | [#7](https://github.com/Romanr92/Adaptive_JSON5/issues/7) |  |
| J5-025 | BOM policy | Draft | [#3](https://github.com/Romanr92/Adaptive_JSON5/issues/3) |  |
| J5-026 | Complete input | Draft | [#8](https://github.com/Romanr92/Adaptive_JSON5/issues/8) |  |
| J5-027 | Language completeness | Draft | [#8](https://github.com/Romanr92/Adaptive_JSON5/issues/8) |  |
| J5-028 | Value types | Draft | [#9](https://github.com/Romanr92/Adaptive_JSON5/issues/9) |  |
| J5-029 | Length-aware strings | Draft | [#9](https://github.com/Romanr92/Adaptive_JSON5/issues/9) |  |
| J5-030 | Ordered arrays | Draft | [#9](https://github.com/Romanr92/Adaptive_JSON5/issues/9) |  |
| J5-031 | Observable object members | Draft | [#9](https://github.com/Romanr92/Adaptive_JSON5/issues/9) |  |
| J5-032 | Duplicate policy | Draft | [#10](https://github.com/Romanr92/Adaptive_JSON5/issues/10) |  |
| J5-033 | Name equality | Draft | [#10](https://github.com/Romanr92/Adaptive_JSON5/issues/10) |  |
| J5-034 | Exact number storage | Draft | [#11](https://github.com/Romanr92/Adaptive_JSON5/issues/11) |  |
| J5-035 | Checked integer conversion | Draft | [#11](https://github.com/Romanr92/Adaptive_JSON5/issues/11) |  |
| J5-036 | Value and spelling queries | Draft | [#11](https://github.com/Romanr92/Adaptive_JSON5/issues/11) |  |
| J5-037 | Floating conversion | Draft | [#11](https://github.com/Romanr92/Adaptive_JSON5/issues/11) |  |
| J5-038 | Special numbers | Draft | [#11](https://github.com/Romanr92/Adaptive_JSON5/issues/11) |  |
| J5-039 | Checked access | Draft | [#9](https://github.com/Romanr92/Adaptive_JSON5/issues/9) |  |
| J5-040 | Document lifetime | Draft | [#9](https://github.com/Romanr92/Adaptive_JSON5/issues/9) | `J5_040_Storage.ReleasePreservesAllocationDescription`; `J5_040_OwnedBlock.DefaultOwnsNothing`; `J5_040_OwnedBlock.ReleasesOnceOnDestruction`; `J5_040_OwnedBlock.MoveTransfersOwnershipWithoutDoubleRelease`; `J5_040_OwnedBlock.ZeroByteRequestSucceedsWithoutProviderCall` |
| J5-041 | Source spans | Draft | [#12](https://github.com/Romanr92/Adaptive_JSON5/issues/12) | `J5_041_Cursor.SpansSubrangeUpToCurrentOffset` |
| J5-042 | Pure memory parsing | Draft | [#9](https://github.com/Romanr92/Adaptive_JSON5/issues/9) |  |
| J5-043 | Structured errors | Draft | [#12](https://github.com/Romanr92/Adaptive_JSON5/issues/12) | `J5_043_LocatedResult.PreservesCategoryAndOffset`; `J5_043_UnlocatedResult.DistinguishesMissingLocationFromByteZero` |
| J5-044 | Diagnostic locations | Draft | [#12](https://github.com/Romanr92/Adaptive_JSON5/issues/12) | `J5_044_Cursor.PreservesExactByteOffsetForDiagnostics` |
| J5-045 | Failure-safe reporting | Draft | [#12](https://github.com/Romanr92/Adaptive_JSON5/issues/12) | `J5_045_Result.ReportsAllocationFailureWithoutRichDiagnostics` |
| J5-046 | Fallible allocation | Draft | [#13](https://github.com/Romanr92/Adaptive_JSON5/issues/13) | `J5_046_CheckedSize/CheckedAddUnits.ReportsExactSumOrResourceLimit`; `J5_046_CheckedSize/CheckedMultiplyUnits.ReportsExactProductOrResourceLimit`; `J5_046_Storage.DefaultHasNoProvider`; `J5_046_Storage.ReportsFailureAsNullWithoutOutstandingStorage`; `J5_046_Storage.BalancesReleaseAfterInjectedFailure`; `J5_046_OwnedBlock/OwnedBlockAllocationPointUnits.ReportsFailureAndBalancesRelease`; `J5_046_OwnedBlock/OwnedBlockAlignmentUnits.RejectsInvalidAlignmentBeforeProviderCall`; `J5_046_OwnedBlock.RejectsMissingCallbacks`; `J5_046_OwnedBlock.AllocatesArrayWithExactSize`; `J5_046_OwnedBlock.ArrayOverflowReportsResourceLimitWithoutProviderCall` |
| J5-047 | Configurable limits | Draft | [#14](https://github.com/Romanr92/Adaptive_JSON5/issues/14) |  |
| J5-048 | Bounded traversal | Draft | [#14](https://github.com/Romanr92/Adaptive_JSON5/issues/14) |  |
| J5-049 | Work budgets | Draft | [#14](https://github.com/Romanr92/Adaptive_JSON5/issues/14) |  |
| J5-050 | Concurrency | Draft | [#15](https://github.com/Romanr92/Adaptive_JSON5/issues/15) |  |
| J5-051 | Atomic results and reuse | Draft | [#13](https://github.com/Romanr92/Adaptive_JSON5/issues/13) | `J5_046_CheckedSize/CheckedAddUnits.ReportsExactSumOrResourceLimit`; `J5_046_CheckedSize/CheckedMultiplyUnits.ReportsExactProductOrResourceLimit`; `J5_051_OwnedBlock.ProviderRemainsUsableAfterFailedRequest` |
| J5-052 | Deterministic semantics | Draft | [#15](https://github.com/Romanr92/Adaptive_JSON5/issues/15) |  |
| J5-053 | Schema dialect | Draft | [#16](https://github.com/Romanr92/Adaptive_JSON5/issues/16) |  |
| J5-054 | Schema compilation | Draft | [#16](https://github.com/Romanr92/Adaptive_JSON5/issues/16) |  |
| J5-055 | Identifiers and references | Draft | [#17](https://github.com/Romanr92/Adaptive_JSON5/issues/17) |  |
| J5-056 | Reference semantics | Draft | [#17](https://github.com/Romanr92/Adaptive_JSON5/issues/17) |  |
| J5-057 | Assertion coverage | Draft | [#18](https://github.com/Romanr92/Adaptive_JSON5/issues/18) |  |
| J5-058 | Annotations and defaults | Draft | [#21](https://github.com/Romanr92/Adaptive_JSON5/issues/21) |  |
| J5-059 | Exact numeric validation | Draft | [#19](https://github.com/Romanr92/Adaptive_JSON5/issues/19) |  |
| J5-060 | String and pattern semantics | Draft | [#20](https://github.com/Romanr92/Adaptive_JSON5/issues/20) |  |
| J5-061 | Structural equality | Draft | [#19](https://github.com/Romanr92/Adaptive_JSON5/issues/19) |  |
| J5-062 | Extension keywords | Draft | [#21](https://github.com/Romanr92/Adaptive_JSON5/issues/21) |  |
| J5-063 | Validation outcomes | Draft | [#22](https://github.com/Romanr92/Adaptive_JSON5/issues/22) |  |
| J5-064 | Schema diagnostics | Draft | [#22](https://github.com/Romanr92/Adaptive_JSON5/issues/22) |  |
| J5-065 | Evaluation budgets | Draft | [#23](https://github.com/Romanr92/Adaptive_JSON5/issues/23) |  |
| J5-066 | Compiled-schema lifetime | Draft | [#23](https://github.com/Romanr92/Adaptive_JSON5/issues/23) |  |
| J5-067 | Pure evaluation | Draft | [#23](https://github.com/Romanr92/Adaptive_JSON5/issues/23) |  |
| J5-068 | Component independence | Draft | [#16](https://github.com/Romanr92/Adaptive_JSON5/issues/16) |  |
| J5-069 | Optional assertions | Draft | [#21](https://github.com/Romanr92/Adaptive_JSON5/issues/21) |  |
| J5-070 | Validation CLI | Draft | [#24](https://github.com/Romanr92/Adaptive_JSON5/issues/24) |  |
| J5-071 | Machine diagnostics | Draft | [#24](https://github.com/Romanr92/Adaptive_JSON5/issues/24) |  |
| J5-072 | Editor packaging | Draft | [#25](https://github.com/Romanr92/Adaptive_JSON5/issues/25) |  |
| J5-073 | External schema association | Draft | [#25](https://github.com/Romanr92/Adaptive_JSON5/issues/25) |  |
| J5-074 | Grammar agreement | Draft | [#26](https://github.com/Romanr92/Adaptive_JSON5/issues/26) |  |
| J5-075 | Location mapping | Draft | [#26](https://github.com/Romanr92/Adaptive_JSON5/issues/26) |  |
| J5-076 | Live document versions | Draft | [#26](https://github.com/Romanr92/Adaptive_JSON5/issues/26) |  |
| J5-077 | Incomplete-input assistance | Draft | [#27](https://github.com/Romanr92/Adaptive_JSON5/issues/27) |  |
| J5-078 | Schema completion | Draft | [#27](https://github.com/Romanr92/Adaptive_JSON5/issues/27) |  |
| J5-079 | Hover and schema diagnostics | Draft | [#28](https://github.com/Romanr92/Adaptive_JSON5/issues/28) |  |
| J5-080 | Consumer-policy diagnostics | Draft | [#28](https://github.com/Romanr92/Adaptive_JSON5/issues/28) |  |
| J5-081 | Offline operation | Draft | [#25](https://github.com/Romanr92/Adaptive_JSON5/issues/25) |  |
| J5-082 | Editor unit coverage | Draft | [#34](https://github.com/Romanr92/Adaptive_JSON5/issues/34) |  |
| J5-083 | Manifest profile boundary | Draft | [#29](https://github.com/Romanr92/Adaptive_JSON5/issues/29) |  |
| J5-084 | Versioned schema artifact | Draft | [#29](https://github.com/Romanr92/Adaptive_JSON5/issues/29) |  |
| J5-085 | Field and policy coverage | Draft | [#29](https://github.com/Romanr92/Adaptive_JSON5/issues/29) |  |
| J5-086 | Unknown versus dynamic names | Draft | [#29](https://github.com/Romanr92/Adaptive_JSON5/issues/29) |  |
| J5-087 | Numeric-policy configuration | Draft | [#30](https://github.com/Romanr92/Adaptive_JSON5/issues/30) |  |
| J5-088 | String and path boundaries | Draft | [#30](https://github.com/Romanr92/Adaptive_JSON5/issues/30) |  |
| J5-089 | Defaults and arguments | Draft | [#30](https://github.com/Romanr92/Adaptive_JSON5/issues/30) |  |
| J5-090 | Result/error mapping | Draft | [#30](https://github.com/Romanr92/Adaptive_JSON5/issues/30) |  |
| J5-091 | Native portability | Draft | [#32](https://github.com/Romanr92/Adaptive_JSON5/issues/32) |  |
| J5-092 | Reusable package | Draft | [#32](https://github.com/Romanr92/Adaptive_JSON5/issues/32) |  |
| J5-093 | Versioned consumption | Draft | [#32](https://github.com/Romanr92/Adaptive_JSON5/issues/32) |  |
| J5-094 | Grammar unit regression coverage | Draft | [#33](https://github.com/Romanr92/Adaptive_JSON5/issues/33) |  |
| J5-095 | Pinned syntax fixtures | Draft | [#33](https://github.com/Romanr92/Adaptive_JSON5/issues/33) |  |
| J5-096 | Draft-07 unit conformance fixtures | Draft | [#34](https://github.com/Romanr92/Adaptive_JSON5/issues/34) |  |
| J5-097 | Deterministic robustness unit cases | Draft | [#33](https://github.com/Romanr92/Adaptive_JSON5/issues/33) |  |
| J5-098 | Fault-injection evidence | Draft | [#33](https://github.com/Romanr92/Adaptive_JSON5/issues/33) |  |
| J5-099 | Portable unit build matrix | Draft | [#35](https://github.com/Romanr92/Adaptive_JSON5/issues/35) |  |
| J5-100 | Release completeness | Draft | [#35](https://github.com/Romanr92/Adaptive_JSON5/issues/35) |  |
| J5-101 | OS-agnostic embedded integration | Draft | [#31](https://github.com/Romanr92/Adaptive_JSON5/issues/31) | `J5_101_BuildConfiguration.AppliesSelectedRttiMode` |
