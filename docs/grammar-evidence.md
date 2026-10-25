# Grammar evidence matrix (P040)

This maps every JSON5 1.0.0 lexical and syntactic production, each ES5.1
alternative it references, and the strict RFC 8259 counterparts to the GoogleTest
suites that exercise them. Suites are listed by name; the individual tests are in
the [traceability inventory](traceability.md). Listing a suite here records that
tests exist. It does not record that they passed, and it does not make any
requirement Verified. Results belong in the implementation sequence.

Columns:

- **Positive**: accepted input with exact decoded content or spans.
- **Negative**: rejected input with code, location and rollback.
- **Boundary**: suffix, lookahead, adjacent tokens, view length and mode.
- **Truncation**: every proper prefix rejected or stopped correctly.
- **Gap**: evidence still missing for P040. A dash means none known.

Component suites test one lexer or engine function. Whole-document suites
(`J5_026_Complete*`, `J5_013_Container*`) go through `parse` or the engine.

## Source text and trivia

| Production | Positive | Negative | Boundary | Truncation | Gap |
| --- | --- | --- | --- | --- | --- |
| JSON5SourceCharacter (UTF-8 input) | `J5_009_Utf8`, `J5_009_Utf8Valid` | `J5_009_Utf8Invalid`, `J5_009_Utf8.RejectsEvery*` | `J5_009_*Encoding` (offsets inside each lexer) | `J5_009_Utf8Truncated` | - (`J5_009_DocumentEncoding` covers both modes through `parse`) |
| WhiteSpace (TAB VT FF SP NBSP BOM Zs) | `J5_011_Whitespace`, `J5_011_WhitespaceRun`, `J5_025_Json5Whitespace` | `J5_009_WhitespaceEncoding`; strict VT, FF, NBSP and BOM in `J5_024_Trivia` | `J5_011_WhitespaceEof`, `J5_024_TriviaBoundary` | n/a (single scalars) | - (`J5_027_ModePair` covers strict rejection and legacy U+200B through `parse`) |
| LineTerminator, LineTerminatorSequence | `J5_011_LineTerminator`, `J5_011_LineTerminatorGrouping` | `J5_009_LineTerminatorEncoding*` | `J5_011_LineTerminatorEof` | CR at end of view | - (`J5_027_ModePair`) |
| Comment, MultiLineComment, SingleLineComment | `J5_012_Comments` | `J5_012_CommentEncoding`, `J5_024_Trivia` (strict rejection) | `J5_012_CommentBoundaries.StopsBeforeInvalidSuffix`, `.RespectsViewLengthAndEmptyCursors` | `J5_012_CommentBoundaries.RejectsEveryUnterminatedPrefix` | - |
| Byte order mark policy (J5-025) | `J5_025_Json5Whitespace` | Strict default rejection | Strict `skip_one`, second BOM | n/a | - (`J5_025_DocumentBom`) |
| JSON5Punctuator `{ } [ ] : ,` | `J5_013_Punctuation.RecognizesAdjacentKindsAndSpans` | `J5_013_Punctuation.LeavesEveryOtherByteAndEmptyInputUnread` | `J5_013_Punctuation.RecognizesOnlyAfterCommentTrivia` | n/a | - |

## Identifiers and literals

| Production | Positive | Negative | Boundary | Truncation | Gap |
| --- | --- | --- | --- | --- | --- |
| JSON5Identifier, IdentifierName, IdentifierStart, IdentifierPart | `J5_019_Identifier`, `J5_033_IdentifierContent` | `J5_019_IdentifierBoundary.RejectsInvalidStartsWithoutAllocation`, `J5_019_IdentifierEncoding` | `J5_019_IdentifierBoundary.PreservesTerminatorAndOriginalSpan` | `J5_019_IdentifierEncoding` | - |
| UnicodeLetter, UnicodeCombiningMark, UnicodeDigit, UnicodeConnectorPunctuation, ZWNJ, ZWJ | `J5_019_UnicodeClassifier`, `J5_006_UnicodeTableMembership` | `J5_019_UnicodeClassifier` | Unicode 3.0 retained memberships | n/a | - |
| `\` UnicodeEscapeSequence in identifiers | `J5_020_IdentifierEscapes` | `J5_020_IdentifierFailure` | `J5_020_IdentifierStorage.EnforcesDecodedLimitsAtEscapeOffsets` | `J5_020_IdentifierBoundary.RejectsEveryTruncatedEscapePrefix` | - |
| JSON5Null, JSON5Boolean; reserved words as values | `J5_023_IdentifierClassification`, `J5_024_Scalar.ReturnsExactAlternativesAndSpans` | `J5_024_TokenRejection` | `J5_024_Scalar.AppliesModeSpecificBoundariesToEveryAlternative` | `J5_026_CompleteTruncation` | - |

## Strings

| Production | Positive | Negative | Boundary | Truncation | Gap |
| --- | --- | --- | --- | --- | --- |
| JSON5String, DoubleStringCharacters, SingleStringCharacters | `J5_014_StringLexer`, `J5_014_Character` | `J5_014_StringFailure`, `J5_014_Encoding`, `J5_014_Opening` (single quotes in strict mode) | `J5_014_StringBoundary`, `J5_014_Delimiters`, `J5_014_OpeningBoundary` | `J5_014_StringBoundary.RejectsEveryCombinedStringPrefixAtomically`, `J5_014_Truncation` | - |
| Raw controls and line terminators in strings | `J5_014_Controls.DistinguishesAllRawControlsByMode` | `J5_014_Controls.RejectsCrLfWithoutConsumingEitherByte` | Raw LS/PS: `J5_017_StringWarning*`, `J5_017_StringLexer.StrictSeparatorsDoNotWarn` | n/a | - |
| LineContinuation | `J5_017_Continuation` | Strict rejection in `J5_017_Continuation.ConsumesExactSequenceOrRejectsStrictMode` | `J5_017_ContinuationBoundary` | `J5_017_ContinuationBoundary.RejectsEofAndTruncatedSeparatorPrefixes` | - |
| SingleEscapeCharacter | `J5_015_SimpleEscape` | Strict `'` and `\v` rejection in `J5_015_SimpleEscape` | `J5_015_SimpleEscapeBoundary`, `J5_016_SimpleEscapeHandoff` | `J5_015_SimpleEscapeBoundary.RejectsEofWithoutAdvancing` | - |
| NonEscapeCharacter | `J5_016_NonEscape` | `J5_016_NonEscapeDigit`, `J5_016_NonEscapeEncoding` | `J5_016_NonEscapeHandoff`, `J5_016_NonEscapeBoundary.RetainsSupplementaryScalarForStorage` | `J5_016_NonEscapeBoundary.RejectsEofAndEveryTruncatedCharacterPrefix` | - |
| `0` [lookahead ∉ DecimalDigit] | `J5_016_ZeroEscape` | `J5_016_ZeroDigit` | `J5_016_ZeroEscape.HonorsBoundedLookaheadAndStillRequiresClosingQuote` | covered by the bounded-lookahead case | - |
| HexEscapeSequence, HexDigit | `J5_016_HexEscape`, `J5_016_HexDigit.MapsAllAsciiHexSpellings` | `J5_016_HexFailure`, `J5_016_HexDigit.RejectsNonhexAndUnicodeLookalikes` | `J5_016_HexEscapeBoundary.StopsAfterExactlyTwoDigits` | `J5_016_HexEscapeBoundary.RejectsEveryTruncatedPrefix` | - |
| UnicodeEscapeSequence, surrogate units | `J5_015_UnicodeEscape`, `J5_018_UnicodePair`, `J5_018_UnicodeEscape` | `J5_015_UnicodeFailure`, `J5_018_Unpaired` (export) | `J5_015_UnicodeBoundary`, `J5_018_Utf8Boundary` | `J5_015_UnicodeBoundary.RejectsEveryTruncatedPrefix` | - |

## Numbers

| Production | Positive | Negative | Boundary | Truncation | Gap |
| --- | --- | --- | --- | --- | --- |
| JSON5Number signs `+` and `-` | `J5_021_Decimal`, `J5_022_NonFinite`, `J5_022_Hexadecimal` | Strict `+`, `.5` and `5.` rejection in `J5_021_Decimal` | `J5_024_ScalarTrivia.DoesNotJoinSeparatedSpellings` | `J5_021_DecimalBoundary.RespectsEveryExplicitLengthPrefix` | - |
| DecimalLiteral (`1`, `1.`, `.1`, `1.5`), DecimalIntegerLiteral, ExponentPart, SignedInteger | `J5_021_Decimal`, `J5_021_DecimalRange` | `J5_021_Decimal` (leading zeros, lone `.` and signs, empty or malformed exponents) | `J5_021_DecimalBoundary` | `J5_021_DecimalBoundary.RespectsEveryExplicitLengthPrefix` | - |
| HexIntegerLiteral | `J5_022_Hexadecimal`, `J5_022_HexadecimalRange` | `J5_022_Hexadecimal`, `J5_022_HexadecimalEncoding` | `J5_022_HexadecimalBoundary` | `J5_022_HexadecimalBoundary.RespectsEveryExplicitLengthPrefix` | - |
| `Infinity`, `NaN` | `J5_022_NonFinite` | `J5_022_NonFinite`, `J5_022_NonFiniteEncoding` | `J5_022_NonFiniteBoundary` | `J5_022_NonFiniteBoundary.ChecksEveryBoundedPrefix` | - |
| Number storage and exact spelling | `J5_034_OwnedNumber`, `J5_034_OwnedNumberRange` | `J5_034_InvalidOwnedNumber` | n/a | n/a | - |

## Syntactic grammar

| Production | Positive | Negative | Boundary | Truncation | Gap |
| --- | --- | --- | --- | --- | --- |
| JSON5Text (one value with trivia) | `J5_026_CompleteRoot`, `J5_026_CompleteLength` | `J5_026_CompleteRejection`, `J5_026_EmptyRoot` | `J5_013_ContainerPrefix` | `J5_026_CompleteTruncation` | - |
| JSON5Value | `J5_026_CompleteRoot.AcceptsEveryTypeAndSurroundingTrivia`, `J5_028_*` | `J5_026_PrimitiveInput` | `J5_013_ContainerModes` | `J5_013_ContainerTruncation` | - |
| JSON5Object, JSON5MemberList, JSON5Member (trailing comma) | `J5_013_ObjectGrammar`, `J5_031_ParsedObjectMembers` | `J5_013_ObjectGrammar`, `J5_013_ContainerFailure` | `J5_013_ObjectGrammar` (strict and nested trailing commas) | `J5_013_ContainerTruncation` | - |
| JSON5MemberName (identifier or string) | `J5_020_ParsedMemberName`, `J5_023_IdentifierClassification` | Strict identifier names in `J5_013_ContainerModes` | `J5_032_DuplicatePolicy` | `J5_013_ContainerTruncation` | - |
| JSON5Array, JSON5ElementList (trailing comma) | `J5_013_ArrayGrammar`, `J5_030_ParsedArrayOrder` | `J5_013_ArrayGrammar`, `J5_013_ContainerFailure` | `J5_013_ArrayGrammar` (strict and nested trailing commas) | `J5_013_ContainerTruncation` | - |
| Nesting and dispatch | `J5_013_ContainerEngine` | `J5_013_ParserDispatch.DistinguishesInvalidStateFromMissingInput` | `J5_013_ParserDispatch.SelectsActionsForEveryStateAndMode` | `J5_026_CompleteTruncation.RejectsEveryUnfinishedNestedPrefix` | - |

## Whole-grammar evidence

These items cover P040 as a whole rather than one production:

1. **External corpora.** Written: `J5_095_Json5TestsCorpus` and
   `J5_095_JsonTestSuiteCorpus` in `tests/validation` (separate validation-test
   gate). Upstream label deviations are recorded in the fixture README.
2. **Equivalence.** Written: `J5_097_Equivalence` compares ten JSON5 spellings
   (quotes, names, trailing commas, comments, Unicode whitespace, escapes,
   continuations, number spellings) against a strict reference document.
3. **Resource failures.** Written: `J5_098_DocumentFaultSweep` fails each
   allocation of a representative document, with and without source retention,
   and checks `allocation_failure` with nothing left allocated.
4. **Strict-mode negatives through `parse`.** Written: `J5_027_ModePair` pairs
   JSON5 acceptance with the strict error offset for every JSON5-only production,
   including legacy U+200B. `J5_009_DocumentEncoding` covers malformed UTF-8 in
   both modes and `J5_094_DocumentTruncation` rejects every proper prefix of a
   representative document in each mode.
5. **BOM rows.** Written: `J5_025_DocumentBom`.

All five suites are in `tests/parser/grammar_evidence_tests.cpp`
(`grammar_evidence_units`). Strict error offsets follow the parser's convention:
rejected number spellings report the token start, escapes report the escaped
character and line continuations report the line terminator.
