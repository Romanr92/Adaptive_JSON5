# Requirement traceability

Current ownership follows [the consolidation plan](issue-consolidation-plan.md).
The owner is the final completion anchor for a shared contract; stage-specific
implementation/tests remain in the P groups listed there. All existing named
tests and Draft status are retained. No per-issue report is required; named units
and actual executed evidence must still support any Verified claim.


Every requirement has an owning implementation issue. Issues #2 onward follow
P002–P066; completion issues #68–#72 retain build/runtime/consumer/release work.
Issue #1's declaration/contracts work and P002/P003 are complete at component
scope; broader J5 contracts remain Draft. The P004 cursor is still pending.
 All functional contracts
remain **Draft**; none is declared fully implemented or verified by this setup.
The configuration, P002 result/error and input/options units provide partial evidence
for their owning requirements. Test existence, a passing execution and complete
requirement coverage are distinct.

Keep exact `SuiteName.TestName` declarations in the Unit Tests inventory, separated
by `; ` when several units cover one requirement. Do not add generated parameter
case suffixes. Record actual execution in [verification notes](verification.md).

| ID | Contract | Status | Owning issue | Unit Tests |
| --- | --- | --- | --- | --- |
| J5-001 | Standalone C++ library | Draft | [#68](https://github.com/Romanr92/Adaptive_JSON5/issues/68) | `J5_001_BuildConfiguration.UsesCxx17OrNewer` |
| J5-002 | Build components | Draft | [#68](https://github.com/Romanr92/Adaptive_JSON5/issues/68) |  |
| J5-003 | Exception-independent API | Draft | [#69](https://github.com/Romanr92/Adaptive_JSON5/issues/69) | `J5_003_BuildConfiguration.AppliesSelectedExceptionMode`; `J5_003_Result.StatusSuccessHasNoError`; `J5_003_Result.ValueSuccessHasNoError`; `J5_003_Result.MutableAccessUpdatesStoredValue`; `J5_003_Result.CopyConstructionPreservesAlternatives`; `J5_003_Result.MoveConstructionPreservesAlternatives`; `J5_003_Result.SupportsMoveOnlyValueOwnership`; `J5_003_Result.SameTypeAlternativesRemainDistinct`; `J5_003_Result.StatusCopyAndMovePreserveFailure`; `J5_003_Result.StatusAssignmentTransitionsBothWays`; `J5_003_Result.BalancesSuccessAlternativeLifetimes`; `J5_003_Result.BalancesErrorAlternativeLifetimes` |
| J5-004 | Input boundary | Draft | [#38](https://github.com/Romanr92/Adaptive_JSON5/issues/38) | `J5_004_InputView.DefaultsToEmptyBuffer`; `J5_004_InputView.AllowsNonNullEmptyBuffer`; `J5_004_InputView.DescribesReadOnlyNonTerminatedPrefix`; `J5_004_InputView.RetainsEmbeddedZeroInByteCount`; `J5_004_InputView.BorrowsCallerStorageWithoutCopying`; `J5_004_Cursor.DefaultConstructedIsAtEnd`; `J5_004_Cursor.EmptyNonNullViewIsAtEnd`; `J5_004_Cursor.ConsumesExactLengthWithoutTerminatingNul`; `J5_004_Cursor.ReadsAcrossEmbeddedZeroBytes`; `J5_004_Cursor.PeekLookaheadBoundsAgainstRemaining`; `J5_004_Cursor.AdvanceClampsAtEnd` |
| J5-005 | Syntax selection | Draft | [#38](https://github.com/Romanr92/Adaptive_JSON5/issues/38) | `J5_005_ParseOptions.DefaultsToJson5`; `J5_005_ParseOptions.SelectsJson5Explicitly`; `J5_005_ParseOptions.SelectsStrictJsonExplicitly` |
| J5-006 | Unicode data baseline | Draft | [#5](https://github.com/Romanr92/Adaptive_JSON5/issues/5) |  |
| J5-007 | License and provenance | Draft | [#68](https://github.com/Romanr92/Adaptive_JSON5/issues/68) |  |
| J5-008 | Public contracts | Draft | [#72](https://github.com/Romanr92/Adaptive_JSON5/issues/72) |  |
| J5-009 | UTF-8 source | Draft | [#5](https://github.com/Romanr92/Adaptive_JSON5/issues/5) |  |
| J5-010 | JSON whitespace | Draft | [#38](https://github.com/Romanr92/Adaptive_JSON5/issues/38) | `Modes/J5_024_Trivia.ComposesOrReportsLocatedFailure`; `J5_024_TriviaBoundary.HandlesEmptyAndBoundedViews` |
| J5-011 | JSON5 whitespace | Draft | [#5](https://github.com/Romanr92/Adaptive_JSON5/issues/5) |  |
| J5-012 | Comments | Draft | [#9](https://github.com/Romanr92/Adaptive_JSON5/issues/9) | `Spellings/J5_012_Comments.PreservesExactSpanAndSuffix`; `J5_012_CommentBoundaries.RejectsEveryUnterminatedPrefix`; `J5_012_CommentBoundaries.RespectsViewLengthAndEmptyCursors`; `Malformed/J5_012_CommentEncoding.ReportsOriginalOffsetAndRollsBack`; `J5_012_CommentBoundaries.StopsBeforeInvalidSuffix`; `Modes/J5_024_Trivia.ComposesOrReportsLocatedFailure`; `J5_024_TriviaBoundary.HandlesEmptyAndBoundedViews` |
| J5-013 | Structural tokens | Draft | [#9](https://github.com/Romanr92/Adaptive_JSON5/issues/9) | `J5_013_Punctuation.RecognizesAdjacentKindsAndSpans`; `J5_013_Punctuation.LeavesEveryOtherByteAndEmptyInputUnread`; `J5_013_Punctuation.RecognizesOnlyAfterCommentTrivia` |
| J5-014 | Quoted strings | Draft | [#13](https://github.com/Romanr92/Adaptive_JSON5/issues/13) | `J5_014_Opening.RecognizesOrRejectsAtOriginalOffset`; `J5_014_OpeningBoundary.LeavesNonquotesAndEmptyViewsUnread`; `J5_014_Character.PreservesScalarSpanAndSuffix`; `J5_014_Controls.DistinguishesAllRawControlsByMode`; `J5_014_Delimiters.MatchesOnlyOpeningQuoteAndHandlesEmptyStrings`; `J5_014_EscapeBoundary.HandsOffWithoutDecodingEscape`; `J5_014_Encoding.RejectsMalformedInputWithoutAdvancing`; `J5_014_Truncation.RespectsEveryExplicitLengthPrefix`; `J5_014_Controls.RejectsCrLfWithoutConsumingEitherByte` |
| J5-015 | JSON escapes | Draft | [#13](https://github.com/Romanr92/Adaptive_JSON5/issues/13) | `J5_015_SimpleEscape.DecodesExactUnitOrRejectsByMode`; `J5_015_SimpleEscapeBoundary.RejectsEofWithoutAdvancing`; `J5_015_SimpleEscapeBoundary.AcceptsExactLengthSuffixWithoutTerminator`; `J5_015_SimpleEscapeBoundary.SeparatesEscapedQuoteFromClosingQuote`; `J5_015_UnicodeEscape.DecodesExactUnitInBothModes`; `J5_015_UnicodeBoundary.RejectsEveryTruncatedPrefix`; `J5_015_UnicodeFailure.RejectsEveryDigitPositionAtomically`; `J5_015_UnicodeBoundary.RejectsTruncatedUtf8AtEveryDigitPosition`; `J5_015_UnicodeBoundary.PreservesOtherIntroducersAndTrailingBytes` |
| J5-016 | JSON5 escapes | Draft | [#13](https://github.com/Romanr92/Adaptive_JSON5/issues/13) | `J5_016_ZeroDigit.RejectsEveryDecimalLookaheadAtomically`; `J5_016_ZeroEscape.AcceptsNondigitLookaheadWithoutConsumingIt`; `J5_016_ZeroEscape.HonorsBoundedLookaheadAndStillRequiresClosingQuote`; `J5_016_SimpleEscapeHandoff.LeavesEveryOtherByteUnread`; `J5_016_HexDigit.MapsAllAsciiHexSpellings`; `J5_016_HexDigit.RejectsNonhexAndUnicodeLookalikes`; `J5_016_HexEscape.DecodesExactUnitAndPreservesSuffix`; `J5_016_HexFailure.ReportsLocatedErrorAndRollsBack`; `J5_016_HexEscapeBoundary.RejectsEveryTruncatedPrefix`; `J5_016_HexEscapeBoundary.LeavesEveryOtherIntroducerUnread`; `J5_016_HexEscapeBoundary.StopsAfterExactlyTwoDigits`; `J5_016_NonEscape.PreservesScalarOrRejectsStrictMode`; `J5_016_NonEscapeDigit.RejectsWithoutAdvancing`; `J5_016_NonEscapeHandoff.DefersSpecialFormsAndLineTerminators`; `J5_016_NonEscapeEncoding.ReportsEncodingAndRollsBack`; `J5_016_NonEscapeBoundary.RejectsEofAndEveryTruncatedCharacterPrefix`; `J5_016_NonEscapeHandoff.CannotAcceptMalformedHexOrUnicodeAsLetters`; `J5_016_NonEscapeBoundary.RetainsSupplementaryScalarForStorage` |
| J5-017 | Continuations and separators | Draft | [#5](https://github.com/Romanr92/Adaptive_JSON5/issues/5) | `J5_017_Continuation.ConsumesExactSequenceOrRejectsStrictMode`; `J5_017_Continuation.JoinsContentAndPreservesFollowingSpace`; `J5_017_ContinuationBoundary.BoundsCrLookahead`; `J5_017_ContinuationBoundary.LeavesAdditionalTerminatorsUnread`; `J5_017_ContinuationBoundary.LeavesNonterminatorsUnread`; `J5_017_ContinuationEncoding.ReportsEncodingWithoutAdvancing`; `J5_017_ContinuationBoundary.RejectsEofAndTruncatedSeparatorPrefixes`; `J5_017_ContinuationBoundary.LeavesMalformedSuffixAfterCompleteCr`; `J5_017_StringWarning.ReportsOriginalSpanWithoutChangingContent`; `J5_017_StringWarning.DoesNotWarnOnEscapedOrContinuedSeparators`; `J5_017_StringWarningBoundary.IgnoresOtherCharactersAndSyntaxPieces`; `J5_017_StringWarningBoundary.DescribesEachAdjacentSeparator` |
| J5-018 | Surrogate fidelity | Draft | [#13](https://github.com/Romanr92/Adaptive_JSON5/issues/13) | `J5_018_Scalar.PreservesBoundaryRepresentations`; `J5_018_InvalidScalar.RejectsWithoutChangingContent`; `J5_018_Unpaired.PreservesUnitsButRejectsExportAtomically`; `J5_018_Decoder.HandlesOffsetsAndInvalidIndices`; `J5_018_UnicodePair.PreservesUnitsAndChecksConversion`; `J5_018_UnicodeEscape.PreservesLoneSurrogatesWithoutReplacement` |
| J5-019 | Unquoted member names | Draft | [#5](https://github.com/Romanr92/Adaptive_JSON5/issues/5) |  |
| J5-020 | Escaped identifiers | Draft | [#22](https://github.com/Romanr92/Adaptive_JSON5/issues/22) |  |
| J5-021 | JSON numbers | Draft | [#25](https://github.com/Romanr92/Adaptive_JSON5/issues/25) |  |
| J5-022 | JSON5 numbers | Draft | [#25](https://github.com/Romanr92/Adaptive_JSON5/issues/25) |  |
| J5-023 | Primitive literals | Draft | [#22](https://github.com/Romanr92/Adaptive_JSON5/issues/22) |  |
| J5-024 | Token boundaries | Draft | [#9](https://github.com/Romanr92/Adaptive_JSON5/issues/9) | `Modes/J5_024_Trivia.ComposesOrReportsLocatedFailure`; `J5_024_TriviaBoundary.DoesNotJoinSeparatedTokens`; `J5_024_TriviaBoundary.HandlesEmptyAndBoundedViews` |
| J5-025 | BOM policy | Draft | [#5](https://github.com/Romanr92/Adaptive_JSON5/issues/5) |  |
| J5-026 | Complete input | Draft | [#38](https://github.com/Romanr92/Adaptive_JSON5/issues/38) |  |
| J5-027 | Language completeness | Draft | [#25](https://github.com/Romanr92/Adaptive_JSON5/issues/25) |  |
| J5-028 | Value types | Draft | [#29](https://github.com/Romanr92/Adaptive_JSON5/issues/29) |  |
| J5-029 | Length-aware strings | Draft | [#13](https://github.com/Romanr92/Adaptive_JSON5/issues/13) | `J5_029_StringStorage.HandlesEmptyWithoutProvider`; `J5_029_StringStorage.PreservesNulControlsAndExplicitLength`; `J5_029_Utf8Export.RejectsEveryShortCapacityWithoutWriting`; `J5_029_Utf8Export.WritesMixedWidthsWithoutTerminator` |
| J5-030 | Ordered arrays | Draft | [#29](https://github.com/Romanr92/Adaptive_JSON5/issues/29) |  |
| J5-031 | Observable object members | Draft | [#29](https://github.com/Romanr92/Adaptive_JSON5/issues/29) |  |
| J5-032 | Duplicate policy | Draft | [#33](https://github.com/Romanr92/Adaptive_JSON5/issues/33) |  |
| J5-033 | Name equality | Draft | [#22](https://github.com/Romanr92/Adaptive_JSON5/issues/22) |  |
| J5-034 | Exact number storage | Draft | [#29](https://github.com/Romanr92/Adaptive_JSON5/issues/29) |  |
| J5-035 | Checked integer conversion | Draft | [#38](https://github.com/Romanr92/Adaptive_JSON5/issues/38) |  |
| J5-036 | Value and spelling queries | Draft | [#38](https://github.com/Romanr92/Adaptive_JSON5/issues/38) |  |
| J5-037 | Floating conversion | Draft | [#38](https://github.com/Romanr92/Adaptive_JSON5/issues/38) |  |
| J5-038 | Special numbers | Draft | [#29](https://github.com/Romanr92/Adaptive_JSON5/issues/29) |  |
| J5-039 | Checked access | Draft | [#29](https://github.com/Romanr92/Adaptive_JSON5/issues/29) |  |
| J5-040 | Document lifetime | Draft | [#69](https://github.com/Romanr92/Adaptive_JSON5/issues/69) | `J5_040_Storage.ReleasePreservesAllocationDescription`; `J5_040_OwnedBlock.DefaultOwnsNothing`; `J5_040_OwnedBlock.ReleasesOnceOnDestruction`; `J5_040_OwnedBlock.MoveTransfersOwnershipWithoutDoubleRelease`; `J5_040_OwnedBlock.ZeroByteRequestSucceedsWithoutProviderCall`; `J5_040_StringStorage.OwnsInputAcrossMovesAndDestruction`; `J5_040_StringBuilder.TransfersStorageAndSupportsReuse`; `J5_040_StringBuilder.ReleasesUnfinishedContent`; `J5_040_OwnedBlock.MoveAssignmentPreservesProviderAndSelfMove` |
| J5-041 | Source spans | Draft | [#69](https://github.com/Romanr92/Adaptive_JSON5/issues/69) | `J5_041_Cursor.SpansSubrangeUpToCurrentOffset` |
| J5-042 | Pure memory parsing | Draft | [#69](https://github.com/Romanr92/Adaptive_JSON5/issues/69) |  |
| J5-043 | Structured errors | Draft | [#38](https://github.com/Romanr92/Adaptive_JSON5/issues/38) | `J5_043_LocatedResult.PreservesCategoryAndOffset`; `J5_043_UnlocatedResult.DistinguishesMissingLocationFromByteZero` |
| J5-044 | Diagnostic locations | Draft | [#38](https://github.com/Romanr92/Adaptive_JSON5/issues/38) | `J5_044_Cursor.PreservesExactByteOffsetForDiagnostics` |
| J5-045 | Failure-safe reporting | Draft | [#69](https://github.com/Romanr92/Adaptive_JSON5/issues/69) | `J5_045_Result.ReportsAllocationFailureWithoutRichDiagnostics` |
| J5-046 | Fallible allocation | Draft | [#69](https://github.com/Romanr92/Adaptive_JSON5/issues/69) | `J5_046_CheckedSize/CheckedAddUnits.ReportsExactSumOrResourceLimit`; `J5_046_CheckedSize/CheckedMultiplyUnits.ReportsExactProductOrResourceLimit`; `J5_046_Storage.DefaultHasNoProvider`; `J5_046_Storage.ReportsFailureAsNullWithoutOutstandingStorage`; `J5_046_Storage.BalancesReleaseAfterInjectedFailure`; `J5_046_OwnedBlock/OwnedBlockAllocationPointUnits.ReportsFailureAndBalancesRelease`; `J5_046_OwnedBlock/OwnedBlockAlignmentUnits.RejectsInvalidAlignmentBeforeProviderCall`; `J5_046_OwnedBlock.RejectsMissingCallbacks`; `J5_046_OwnedBlock.AllocatesArrayWithExactSize`; `J5_046_OwnedBlock.ArrayOverflowReportsResourceLimitWithoutProviderCall`; `J5_046_StringStorage.RejectsInvalidAndUnrepresentableRequests`; `J5_046_GrowthFailure.PreservesContentAndAllowsRetry`; `J5_046_StringBuilder.PairAllocationFailureIsAtomic`; `J5_046_StringBuilder.EnforcesExactUnitLimits`; `J5_046_StringBuilder.RejectsMissingProvider` |
| J5-047 | Configurable limits | Draft | [#69](https://github.com/Romanr92/Adaptive_JSON5/issues/69) |  |
| J5-048 | Bounded traversal | Draft | [#69](https://github.com/Romanr92/Adaptive_JSON5/issues/69) |  |
| J5-049 | Work budgets | Draft | [#69](https://github.com/Romanr92/Adaptive_JSON5/issues/69) |  |
| J5-050 | Concurrency | Draft | [#69](https://github.com/Romanr92/Adaptive_JSON5/issues/69) |  |
| J5-051 | Atomic results and reuse | Draft | [#69](https://github.com/Romanr92/Adaptive_JSON5/issues/69) | `J5_046_CheckedSize/CheckedAddUnits.ReportsExactSumOrResourceLimit`; `J5_046_CheckedSize/CheckedMultiplyUnits.ReportsExactProductOrResourceLimit`; `J5_051_OwnedBlock.ProviderRemainsUsableAfterFailedRequest` |
| J5-052 | Deterministic semantics | Draft | [#69](https://github.com/Romanr92/Adaptive_JSON5/issues/69) |  |
| J5-053 | Schema dialect | Draft | [#53](https://github.com/Romanr92/Adaptive_JSON5/issues/53) |  |
| J5-054 | Schema compilation | Draft | [#53](https://github.com/Romanr92/Adaptive_JSON5/issues/53) |  |
| J5-055 | Identifiers and references | Draft | [#44](https://github.com/Romanr92/Adaptive_JSON5/issues/44) |  |
| J5-056 | Reference semantics | Draft | [#44](https://github.com/Romanr92/Adaptive_JSON5/issues/44) |  |
| J5-057 | Assertion coverage | Draft | [#48](https://github.com/Romanr92/Adaptive_JSON5/issues/48) |  |
| J5-058 | Annotations and defaults | Draft | [#53](https://github.com/Romanr92/Adaptive_JSON5/issues/53) |  |
| J5-059 | Exact numeric validation | Draft | [#48](https://github.com/Romanr92/Adaptive_JSON5/issues/48) |  |
| J5-060 | String and pattern semantics | Draft | [#48](https://github.com/Romanr92/Adaptive_JSON5/issues/48) |  |
| J5-061 | Structural equality | Draft | [#48](https://github.com/Romanr92/Adaptive_JSON5/issues/48) |  |
| J5-062 | Extension keywords | Draft | [#53](https://github.com/Romanr92/Adaptive_JSON5/issues/53) |  |
| J5-063 | Validation outcomes | Draft | [#53](https://github.com/Romanr92/Adaptive_JSON5/issues/53) |  |
| J5-064 | Schema diagnostics | Draft | [#53](https://github.com/Romanr92/Adaptive_JSON5/issues/53) |  |
| J5-065 | Evaluation budgets | Draft | [#69](https://github.com/Romanr92/Adaptive_JSON5/issues/69) |  |
| J5-066 | Compiled-schema lifetime | Draft | [#53](https://github.com/Romanr92/Adaptive_JSON5/issues/53) |  |
| J5-067 | Pure evaluation | Draft | [#53](https://github.com/Romanr92/Adaptive_JSON5/issues/53) |  |
| J5-068 | Component independence | Draft | [#53](https://github.com/Romanr92/Adaptive_JSON5/issues/53) |  |
| J5-069 | Optional assertions | Draft | [#53](https://github.com/Romanr92/Adaptive_JSON5/issues/53) |  |
| J5-070 | Validation CLI | Draft | [#57](https://github.com/Romanr92/Adaptive_JSON5/issues/57) |  |
| J5-071 | Machine diagnostics | Draft | [#57](https://github.com/Romanr92/Adaptive_JSON5/issues/57) |  |
| J5-072 | Editor packaging | Draft | [#61](https://github.com/Romanr92/Adaptive_JSON5/issues/61) |  |
| J5-073 | External schema association | Draft | [#64](https://github.com/Romanr92/Adaptive_JSON5/issues/64) |  |
| J5-074 | Grammar agreement | Draft | [#61](https://github.com/Romanr92/Adaptive_JSON5/issues/61) |  |
| J5-075 | Location mapping | Draft | [#61](https://github.com/Romanr92/Adaptive_JSON5/issues/61) |  |
| J5-076 | Live document versions | Draft | [#61](https://github.com/Romanr92/Adaptive_JSON5/issues/61) |  |
| J5-077 | Incomplete-input assistance | Draft | [#61](https://github.com/Romanr92/Adaptive_JSON5/issues/61) |  |
| J5-078 | Schema completion | Draft | [#64](https://github.com/Romanr92/Adaptive_JSON5/issues/64) |  |
| J5-079 | Hover and schema diagnostics | Draft | [#66](https://github.com/Romanr92/Adaptive_JSON5/issues/66) |  |
| J5-080 | Consumer-policy diagnostics | Draft | [#66](https://github.com/Romanr92/Adaptive_JSON5/issues/66) |  |
| J5-081 | Offline operation | Draft | [#64](https://github.com/Romanr92/Adaptive_JSON5/issues/64) |  |
| J5-082 | Editor unit coverage | Draft | [#66](https://github.com/Romanr92/Adaptive_JSON5/issues/66) |  |
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
| J5-094 | Grammar unit regression coverage | Draft | [#38](https://github.com/Romanr92/Adaptive_JSON5/issues/38) |  |
| J5-095 | Pinned syntax fixtures | Draft | [#38](https://github.com/Romanr92/Adaptive_JSON5/issues/38) |  |
| J5-096 | Draft-07 unit conformance fixtures | Draft | [#53](https://github.com/Romanr92/Adaptive_JSON5/issues/53) |  |
| J5-097 | Deterministic robustness unit cases | Draft | [#38](https://github.com/Romanr92/Adaptive_JSON5/issues/38) |  |
| J5-098 | Fault-injection evidence | Draft | [#38](https://github.com/Romanr92/Adaptive_JSON5/issues/38) |  |
| J5-099 | Portable unit build matrix | Draft | [#72](https://github.com/Romanr92/Adaptive_JSON5/issues/72) |  |
| J5-100 | Release completeness | Draft | [#72](https://github.com/Romanr92/Adaptive_JSON5/issues/72) |  |
| J5-101 | OS-agnostic embedded integration | Draft | [#69](https://github.com/Romanr92/Adaptive_JSON5/issues/69) | `J5_101_BuildConfiguration.AppliesSelectedRttiMode` |
