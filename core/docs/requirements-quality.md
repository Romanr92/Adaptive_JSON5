# Core quality requirements

Status: **Draft requirements**. Split from the public requirements by issue #98;
no normative contract, identifier or verification status changed.
Public behavior and dependencies are defined in [API requirements](../../docs/requirements.md).

## I. Unit verification and release documentation

| ID / title | Requirement | Planned unit verification / documentation | Dependencies | Basis |
| --- | --- | --- | --- | --- |
| J5-094 — Grammar unit regression coverage | Unit tests shall trace to grammar requirements and cover valid, invalid and truncated cases for each production and syntax mode, asserting decoded values and locations rather than acceptance alone. | Production coverage matrix, truncation at every byte of representative tokens, mode-pair cases and the minimum regression checklist below. | J5-009–044 | P |
| J5-095 — Pinned syntax fixtures | The unit suite shall include suitably licensed, pinned external syntax fixtures with documented expected behavior. Differences involving duplicates, precision, Unicode and limits shall be recorded against the specifications rather than treating another implementation as authoritative. | Import selected official JSON5 and JSONTestSuite data into parameterized unit tests; record revisions/notices and classify optional cases. No external interpreter or differential process is required. | J5-007, J5-027, J5-032–038, J5-094 | J5-R; JSONTestSuite is supporting community evidence; P |
| J5-096 — Draft-07 unit conformance fixtures | The optional schema component shall exercise pinned required Draft-07 Test Suite cases as unit tests, with registered remote fixtures supplied offline. Optional cases shall match advertised capabilities; required assertion/reference cases shall not be silently skipped. | Record fixture revision, required/optional selections, unit counts/failures and numeric/Unicode/parity regressions. | J5-053–069, J5-094 | ST; P |
| J5-097 — Deterministic robustness unit cases | Parser/schema unit tests shall cover malformed input, configured boundaries, cleanup and deterministic equivalent-input transformations. Preserve regressions as fixed fixtures; supported failure cases shall return structured errors without crashes, invalid access or uncontrolled loops. | Unit-test both modes, duplicate policies, limits, references and Unicode boundaries; vary legal whitespace/comments/escapes without changing expected values. Fuzzing/sanitizer jobs are outside the current quality scope. | J5-027, J5-046–052, J5-065, J5-094, J5-096 | P |
| J5-098 — Fault-injection evidence | A release shall demonstrate allocation/limit/cancellation failure propagation and cleanup for parser, conversion and schema operations in both supported exception configurations. | Inject failure at each reachable allocation in representative unit cases, count allocated/freed blocks and retained results, test limit/cancellation boundaries and minimal diagnostic fallback in both exception modes. | J5-003, J5-045–051, J5-063, J5-065, J5-094 | P |
| J5-099 — Portable unit build matrix | The same C++17 unit suite shall run natively on Windows/MinGW-w64 GCC and Linux/GCC with exceptions OFF and ON. Record unsupported combinations explicitly; automated quality checks shall comprise unit tests and formatting only. | Record native toolchain versions and unit results from the four platform/exception configurations, together with a clang-format check. | J5-003, J5-068, J5-091, J5-094–098 | P |
| J5-100 — Release completeness | Each release shall accurately document the capabilities actually shipped, API, standards/Unicode versions, requirement/grammar/keyword unit mappings, limits, licenses, package usage and known deviations. Full parser/schema/editor claims shall follow their respective completed contracts and unit evidence; optional examples shall not block a generic library release. | Review capability and requirement mappings against shipped code; record executed unit/format results. No benchmark, deployment, editor-host or certification claim shall be inferred. | J5-008, J5-027, J5-053, J5-082, J5-093–099 (as applicable to shipped components) | P |

## Minimum unit regression checklist

These are test intentions, not implemented tests. Binary fixtures are needed for
raw UTF-8 errors/control bytes; Markdown spellings alone are insufficient.

| Case | Required observation |
| --- | --- |
| JSON5 `{key: 'value',}` and JSON equivalent | Expected owned values; the JSON5-only form fails strict mode. |
| Primitive/array roots and empty/comment-only input | Generic roots supported; no value is a syntax failure; manifest profile rejects non-object roots separately. |
| Comment delimiters inside strings and comments next to tokens | Correct token boundaries; string content is not stripped as a comment. |
| Bare `true`/`null` as keys, escaped Unicode names and invalid digit-first names | IdentifierName behavior and correct decoded names, not arbitrary bare-key acceptance. |
| Non-ASCII IdentifierName categories and Unicode Zs spaces | Grammar support agrees with the documented data version; no ASCII-only claim disguised as full support. |
| `\x41`, `\v`, `\0`, `\a`, `\e`, backslash-space and escaped single quote | Correct decoded units/length; invalid decimal-digit escape forms fail; manifest NUL policy is a separate rejection. |
| Raw tab/NUL and raw CR/LF in strings | Respect the selected grammar and explicit buffer length; do not import a blanket JSON control-byte rule into JSON5. |
| All five line-continuation forms and raw U+2028/U+2029 | Joined content excludes the continuation; raw separators preserve content with the prescribed JSON5 warning. |
| Paired and unpaired escaped surrogate units | Generic parse preserves exact units; checked UTF-8/schema/profile operations report their own unsupported-conversion/domain outcome without substitution. |
| `{a:1, "a":2, "\u0061":3}` | Three members retained generically; decoded duplicate policy reports the colliding occurrences. |
| Equal-looking normalized names and deliberately colliding hashes | Exact decoded equality, no normalization or hash-only comparison. |
| `1`, `1.0`, `1e0`, `0x1`, `+1`, `1e-1`, `1.0000000000000001` | Independent mathematical-integral and spelling queries; schema integer/value semantics correct; no double-rounding acceptance. |
| `9007199254740993`, signed/unsigned bounds and `0x10000000000000001` | No rounding/wrapping; raw token survives; checked destination overflow reported. |
| Huge finite exponent versus explicit Infinity/NaN; negative zero | Grammar acceptance does not imply conversion success; non-finite schema-instance rejection is distinct. |
| Very deep/wide structures, large strings/numbers and cancellation | Named budget/limit failures and bounded, leak-free cleanup. |
| Allocation failure during parse, compiled schema, validation and diagnostic construction | Recoverable error, no published success/partial candidate, no abort-dependent path. |
| Default/unknown instance fields versus unknown schema keywords | Validation does not insert defaults; instance rejection and schema extension handling remain distinct. |
| Draft-07 `$ref` siblings, nested base IDs and recursive schemas | Correct dialect behavior; no hangs or fabricated assertion failure for resolver/resource errors. |
| Decimal `multipleOf`, equivalent numeric enum/const and unordered-object `uniqueItems` | Exact schema number/deep-equality behavior. |
| Editor `1e-1`/`1.0`, extra escapes, decoded duplicates and astral character positions | Runtime/editor agreement in values, diagnostics and selected source ranges. |
| Valid/invalid manifest field and path fixtures | Schema/profile versus filesystem rejection is identified explicitly; no process-control calls. |

## Native quality matrix

| Platform / compiler | Exceptions | Quality evidence |
| --- | --- | --- |
| Windows / MinGW-w64 GCC | OFF and ON | The same C++ unit suite |
| Linux / GCC | OFF and ON | The same C++ unit suite |
| C++ source | N/A | clang-format 18 check |

Editor logic and CLI behavior are covered through unit-testable adapters as those
components are implemented. Unit results do not establish actual editor-host,
cross-compilation, deployment or universal-platform support.
