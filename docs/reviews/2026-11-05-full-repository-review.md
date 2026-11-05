# Full repository review — Adaptive JSON5

| Item | Value |
| --- | --- |
| Review date | 2026-11-05 |
| Baseline | `64510bef5486bc5825ee6eaf3fb0079d8918fa04` (`main`, version 0.2.0) |
| Review branch | `claude/keen-edison-0aacft` |
| Reviewer | Claude Code (AI agent) |
| Status | **Complete** — all plan steps 0–9 done; [follow-up re-check](#follow-up--re-check-of-newer-branches-2026-11-06) on 2026-11-06 |
| Re-checked refs | `origin/main` `903aedc` (= tag `0.2.0`), `72-release-packaging` `0b8957e`, `44-schema-1-…-p043p046` `f103fc2` |

**Revision note (2026-11-06).** Re-checked the branches pushed after the review (see
[follow-up](#follow-up--re-check-of-newer-branches-2026-11-06)). Tag `0.2.0` contains
exactly the reviewed production code, so every code finding applies to the release.
Status updates were added to STD-06, FS-04, FS-09, DOC-13 and REPO-06; new findings
STD-15, DOC-17 and REPO-14 were added. Totals are now 81 (5 High, 25 Medium, 38 Low,
13 Info).

**Revision note (2026-11-05).** After the first version, the owner confirmed that the
project follows **MISRA with a fail-fast principle** (early return on the first
failure). STD-06 was lowered from Medium to Info, and STD-07, FS-04 and the
BARR-C note were reworded. Totals changed from 26 to 25 Medium and from 11 to 12
Info.

This document records review findings only. Per `AGENTS.md`, the reviewer does
not create or change production C++ or public headers; every production finding
is a recommendation for the maintainer (Romanr92). No finding in this document
claims a compliance, certification or "verified" status.

---

## Review plan

The review is executed in the order below. Each step updates the progress
tracker and writes its findings to the "Step N results" section with the same number.

### Progress tracker

| Step | Area | Status | Results |
| --- | --- | --- | --- |
| 0 | Method, severity scale and finding format | Done | [Step 0](#step-0--method) |
| 1 | Preparation: source of truth, baseline build and test evidence | Done | [Baseline evidence](#step-1-results--baseline-evidence) |
| 2 | Logic review of production code | Done | [LOG-01 … LOG-10](#step-2-results--logic-findings) |
| 3 | Coding standards: MISRA C++:2023, AUTOSAR C++14, CERT C++, BARR-C:2018 and related automotive guidelines | Done | [STD-01 … STD-15](#step-3-results--coding-standards-findings-misra-autosar-cert-barr-c) |
| 4 | ISO 26262 (functional safety) | Done | [FS-01 … FS-10](#step-4-results--iso-26262-findings) |
| 5 | ISO/SAE 21434 (cybersecurity) | Done | [CS-01 … CS-07](#step-5-results--isosae-21434-findings) |
| 6 | Architecture | Done | [ARC-01 … ARC-08](#step-6-results--architecture-findings) |
| 7 | Documentation | Done | [DOC-01 … DOC-17](#step-7-results--documentation-findings) |
| 8 | Full repository (build, CI, tests, tools, licensing, housekeeping) | Done | [REPO-01 … REPO-14](#step-8-results--full-repository-findings) |
| 9 | Consolidation, open questions and further improvements | Done | [Summary](#step-9-results--summary), [Questions](#step-9-results--questions-no-answer-needed-during-the-review), [Improvements](#step-9-results--further-improvements) |
| F | Follow-up: re-check of branches pushed after the review | Done (2026-11-06) | [Follow-up](#follow-up--re-check-of-newer-branches-2026-11-06) |

### Step 0 — Method

- **Constraints.** Read-only for production code (`core/src/`, `core/includes/`,
  `includes/`). Standards are applied from rule knowledge; no licensed MISRA,
  AUTOSAR or CERT checker is available, so standards findings are reviewer
  observations, not tool-certified deviations. BARR-C:2018 is a C standard and is
  applied by analogy where its intent carries over to C++17.
- **Local analysis tools.** Compiler warnings beyond the project defaults and any
  locally available analyzer are run once as review evidence only. They are not
  added as repository gates (the user-approved scope in `AGENTS.md` excludes
  static-analysis, coverage, sanitizer and fuzz gates).
- **Severity scale.**
  - **Critical** — incorrect result, memory-safety defect or crash reachable from
    input; or a claim in the documents that is false and safety/security relevant.
  - **High** — defect or gap that a safety/security assessment would block on.
  - **Medium** — rule deviation or design weakness with a realistic impact;
    needs a fix or a recorded, justified deviation.
  - **Low** — maintainability, consistency or clarity issue.
  - **Info** — observation or positive note; no action required.
- **Finding IDs.** `LOG-nn` logic, `STD-nn` coding standards, `FS-nn` ISO 26262,
  `CS-nn` ISO/SAE 21434, `ARC-nn` architecture, `DOC-nn` documentation,
  `REPO-nn` full repository.
- **Finding format.** ID · severity · location (`path:line`) · observation ·
  evidence/impact · recommendation.

### Step 1 — Preparation and baseline evidence

1. Read the source-of-truth set: `README.md`, `AGENTS.md`, `docs/requirements.md`,
   `docs/traceability.md`, ADR 0001–0004, the P001–P066 implementation order,
   `core/docs/design-*.md`, `core/docs/requirements-quality.md` and both skills
   under `.agents/skills/`.
2. Record toolchain versions and the CMake presets.
3. Configure, build and run the host unit tests with exceptions OFF and RTTI OFF
   (the mandated configuration), and the exceptions-ON variant used by CI.
4. Build and run the validation tests (`ADAPTIVE_JSON5_BUILD_VALIDATION_TESTS`).
5. Run the formatting check (`tools/format.py`).
6. Record the executed results (pass/fail counts, warnings) in the evidence section.

### Step 2 — Logic review of production code

Review every production translation unit and header in dependency order. For
each area check JSON5/ECMAScript 5.1 conformance, boundary values, integer
overflow and underflow, off-by-one errors, invalid and truncated UTF-8,
surrogate handling, error propagation and error position accuracy,
resource-limit enforcement, object lifetime and dangling views, aliasing,
determinism and numeric correctness (exactness, rounding, range).

1. Foundations — `common/checked_size`, `input/cursor`, `input/text_position`.
2. Unicode — UTF-8/UTF-16 decoding, generated tables and the generator script.
3. Lexer — whitespace, line terminators, comments, trivia, punctuation,
   identifiers (including escaped identifiers), strings and every escape form,
   decimal/hexadecimal/non-finite numbers, token boundaries, scalar and
   member-name lexers.
4. Memory — resource and work budgets, buffer arena, accounted storage, owned
   block, storage owner, document storage, decoded string builder, alignment.
5. Parser — engine, explicit stack and frames, dispatch, container open/close,
   duplicate-member policy, attachment, primitive parser, work accounting and the
   two-pass flow.
6. Value model — value records and storage, document, links, view, iterator,
   member lookup, number analysis/conversion/comparison, big unsigned and integer
   magnitude, floating conversion, decoded strings, UTF-8 export, name equality,
   warnings, retained source.
7. Public API — `result.hpp`/`error.hpp`, options, `parser.hpp`, document and
   value wrappers, hosted one-call parse, diagnostic formatting, umbrella header.

### Step 3 — Coding standards

Sweep the production code against MISRA C++:2023 (primary), AUTOSAR C++14,
SEI CERT C++, BARR-C:2018 (by analogy) and the relevant HIC++/JSF AV intents.
Checklist:

1. Language subset and undefined/unspecified/implementation-defined behavior.
2. Dynamic memory, exceptions, RTTI, recursion, `goto`, unions, variadics.
3. Casts (`reinterpret_cast`, C-style, `const_cast`), pointer arithmetic,
   implicit narrowing and signed/unsigned conversions.
4. Standard library use (`<cstdio>`, `<cstdlib>`, `strtod`, locale, `<cmath>`),
   floating-point comparisons and conversions.
5. Function size and cyclomatic complexity, nesting depth, single point of exit
   where required, `switch` completeness, default labels, fall-through.
6. Declarations: `const`/`constexpr`, `noexcept`, `[[nodiscard]]`, initialization,
   `explicit`, special-member rules (rule of zero/five), object lifetime.
7. Preprocessor and header hygiene: guards, macros, include order.
8. Naming, magic numbers, comments, formatting (BARR-C intents).
9. Compiler warning sweep with an extended flag set as supporting evidence.

### Step 4 — ISO 26262 (functional safety)

Assess the repository as a candidate Safety Element out of Context (SEooC)
software component, ISO 26262-6 and -8 focus:

1. Safety scope: ASIL claim, assumptions of use, safety manual, SEooC assumptions.
2. Software safety requirements: attributes and quality criteria (Part 8 clause 6),
   verification of requirements, bi-directional traceability.
3. Architectural design principles (Part 6 clause 7) and unit design and
   implementation principles (Part 6 clause 8, Tables 6 and 8).
4. Error detection and handling mechanisms, defensive programming, freedom from
   interference (memory, timing/execution, shared state).
5. Unit verification (Part 6 clause 9): methods, test-case derivation, structural
   coverage, back-to-back and fault-injection expectations.
6. Supporting processes: configuration and change management, tool confidence
   (compilers, generators, CI), qualification of software components (Part 8
   clause 12), documentation.

### Step 5 — ISO/SAE 21434 (cybersecurity)

1. Item/component context and attack surface (untrusted JSON5 input, file and
   CLI paths, release artifacts).
2. TARA-lite for the parser: threats, damage scenarios, existing controls.
3. Cybersecurity requirements and how limits, budgets and error handling
   implement them (denial of service, memory safety, information disclosure).
4. Supply chain: dependency pinning, FetchContent, GitHub Actions pinning and
   permissions, release integrity (checksums, signatures, SBOM).
5. Vulnerability management and post-development activities (security policy,
   disclosure, monitoring).
6. Verification evidence for security claims (negative tests, robustness tests).

### Step 6 — Architecture

1. Layering against ADR 0002 and `core/docs/design-architecture.md`.
2. Public/private boundary, include direction and dependency graph.
3. Ownership, lifetime and storage model (ADR 0003), embedded versus hosted split.
4. Public API shape (ADR 0004): ergonomics, consistency, error model, ABI and
   versioning.
5. Build options and optional components (schema, CLI, editor) isolation.

### Step 7 — Documentation

1. Accuracy of documents against the code and CMake.
2. Requirement status consistency and traceability completeness.
3. Stale, duplicated or contradictory content, broken links and anchors.
4. README quickstart and example correctness.
5. ADR status and decision records.

### Step 8 — Full repository

1. CMake: options, presets, install/export, packaging, FetchContent pinning.
2. CI workflows: triggers, matrices, permissions, action pinning, caching.
3. Tests: placement rules (`core/tests/` versus `tests/`), naming, duplication,
   requirement coverage, determinism, test-only helpers.
4. Tools: Python scripts (format, coverage, packaging, Unicode generator).
5. Fixtures, licensing and provenance (`NOTICE`, `LICENSES/`).
6. Editor configuration, issue/PR templates, backlog JSON files, housekeeping.

### Step 9 — Consolidation

1. Summary table of findings by area and severity.
2. Prioritized recommendations.
3. Questions that do not need answers during the review.
4. Further improvements.

---

## Step 1 results — Baseline evidence

Executed on 2026-11-05 in the review container against the baseline commit.

| Item | Value |
| --- | --- |
| Host | Linux x86-64 (Ubuntu 24.04 container) |
| Compiler | GCC 13.3.0 (`g++`), clang/clang-tidy available for review-only analysis |
| CMake / generator | CMake 3.28.3, Ninja |
| Formatter | clang-format 18 |

| Check (preset) | Exceptions | RTTI | Result |
| --- | --- | --- | --- |
| `debug-unit-tests` | OFF | OFF | **2514 / 2514 passed**; `J5_052_NumericLocale.ResultsIgnoreProcessLocale` reported *Skipped* (no alternative locale installed) |
| `debug-unit-tests-exceptions` | ON | OFF | **2514 / 2514 passed**; same locale case skipped |
| `validation-tests` | OFF | OFF | **921 / 921 passed** |
| `validation-tests-exceptions` | ON | OFF | **921 / 921 passed** |
| `python3 tools/format.py --check` | — | — | **Passed** for 257 C++ files |
| Project warning flags (GCC/Clang) | — | — | Only `-Wparentheses -Werror` (see STD-01) |

The skipped locale case is counted as "passed" by CTest's summary line; it is not
an executed locale comparison (the repository's own conformance page already says
so). No source file was changed to produce these results.

## Step 2 results — Logic findings

Scope covered: every production translation unit and header under `core/src/`,
`core/includes/` and `includes/`, read in the Step 2 order. Behavioral claims
marked *probe* were confirmed with throw-away programs compiled in the reviewer's
scratch area against the baseline `libadaptive_json5_parser.a`; nothing was added
to the repository.

**Areas reviewed without a logic finding** (positive notes):

- UTF-8 decoding (`core/src/unicode/utf8.cpp`) implements the RFC 3629 table
  exactly (C2–DF, E0/A0, ED/9F, F0/90, F4/8F boundaries) and never advances on
  failure. UTF-16 encode/decode and surrogate pairing are correct.
- JSON5 §5.1 escapes (`\x`, `\u`, `\0` with no following digit, `\1`–`\9`
  rejection, non-escape characters, all five line-continuation forms) and the
  strict-JSON escape subset are implemented as specified.
- Number grammar (decimal, leading/trailing point, exponent, hex, signed
  `Infinity`/`NaN`, leading-zero rejection, token boundaries) matches JSON5 §6 and
  RFC 8259 §6 in the respective modes.
- The two-pass engine is iterative (no recursion), bounded by explicit depth,
  node, member, string, number, diagnostic and work budgets, and publishes no
  partial document. Container state transitions in the count pass and the build
  pass agree.
- Checked arithmetic (`checked_add`, `checked_multiply`, subtraction-form bound
  checks) is used consistently on every size computation reviewed.
- Exact numeric work (integrality analysis, integer conversion, correctly rounded
  binary floating conversion with round-half-even and subnormal handling) was
  traced by hand on boundary examples; no defect found.
- Error messages produced by `format_error`/`describe` never echo input bytes.

| ID | Severity | Location | Observation | Evidence / impact | Recommendation |
| --- | --- | --- | --- | --- | --- |
| LOG-01 | **High** | `core/src/value/json5_value.cpp:303-318`; `core/src/memory/buffer_arena.cpp:50-64`; `core/src/value/floating_conversion.cpp:256-351` | `json5_value::as_double()` takes its big-integer scratch from the document's own storage. For a caller-buffer document (`json5_document::parse(text, buffer, size)`), `buffer_arena` only rewinds when *every* allocation is released, which never happens while the document is alive. Every `as_double()` call therefore consumes arena space permanently; `to_double` reserves two scratch integers for every finite non-zero value, including `8080` or `1.5`. | *Probe:* 4 KiB buffer, document `{ port: 8080, ratio: 1.5 }`: read #163 of `root().member("ratio").as_double()` fails with `allocation_failure` while `current_bytes()` (649) and `peak_bytes()` (761) are unchanged. A device that polls a configuration value will start failing after a bounded number of reads, and the documented sizing method (`peak_bytes()` + margin, `docs/memory-and-limits.md`) cannot detect it. | Make the arena reclaim LIFO releases (if `address + bytes == m_begin + m_used`, roll `m_used` back; the two scratch integers are already released in reverse order), or give `as_double` a bounded stack scratch / fast path for short significands, or take scratch from a separate caller-supplied provider. Add a unit test that reads `as_double()` in a loop on a small buffer and asserts no exhaustion. |
| LOG-02 | Medium | `includes/adaptive_json5/json5_value.hpp` (`as_double`, `const`); `includes/adaptive_json5/value/document.hpp:209` | `json5_value::as_double()` is a `const` member but mutates shared state of the owning document (accounting counters and the arena bump pointer) through the scratch callbacks. `document.hpp` promises that "const operations change no state, so several threads may read one document concurrently"; `json5_document`/`json5_value` state no thread-safety rule. | Two threads calling `as_double()` on values of the same `json5_document` race on `accounted_storage::m_current_bytes/m_peak_bytes` and `buffer_arena::m_used/m_live` (undefined behavior; possible corrupt accounting or overlapping scratch blocks). | Either make `as_double()` independent of document storage (see LOG-01) or document that a `json5_document` must not be read concurrently when `as_double()` is used. Add the thread-safety statement to `json5_document.hpp`. |
| LOG-03 | Medium | `core/src/parser/parser_engine.cpp:126-148, 684-698`; `core/src/memory/resource_budget.cpp` | Exceeding `max_depth`, `max_nodes`, `max_members` (and `max_input_bytes`) returns `resource_limit` **without a byte offset**, and no `resource_limit` error says *which* limit was exceeded. | *Probe:* `[[[[1]]]]` with `max_depth = 3`, `[1,2,3,4]` with `max_nodes = 3`, `{a:1,b:2}` with `max_members = 1` all yield `has_byte_offset = false`, message "resource limit exceeded". J5-044 asks for an offset on input diagnostics; `docs/memory-and-limits.md` tells users to "raise the specific limit", which they cannot identify. | Locate these failures at `m_source.offset()` (the token that would exceed the limit) and add an optional `resource_limit_reason` (input, depth, nodes, members, string, total string, number bytes, exponent digits, owned bytes, diagnostics, work) to `error`, mirroring `syntax_reason`/`numeric_reason`. |
| LOG-04 | Low | `core/src/lexer/number/non_finite_lexer.cpp:60-100`; `docs/memory-and-limits.md:214-215` | Unknown words in value position that start with `N` or `I` (`None`, `Nan`, `Index`) are routed to the non-finite lexer and reported at the first mismatching byte with no `source_range`. Other unknown words (`nul`, `trueish`) get a whole-word range. | *Probe:* `[None]` → offset 2, no range; `[nul]` → offset 1, range 1+3. Contradicts the documented contract "source_range covers the whole word". | On a non-finite spelling mismatch, fall back to `measure_identifier` and report `unknown_literal` with the whole-word range, as `consume_scalar` does. Add `None`/`Index`/`Nan` cases to the diagnostics units. |
| LOG-05 | Low | `core/src/value/json5_value.cpp:150-214` | `json5_value::member()` reports a missing member, a duplicated (ambiguous) member and a non-object receiver all as the same `wrong_type_or_access` without any sub-reason. The advanced `lookup_member` distinguishes `absent`/`unique`/`ambiguous`, but the simple API discards it. | *Probe:* `member("a")` on `{a:1, a:2}` and `member("zz")` both return code 4. J5-039 requires *explicit* reporting of absent member versus ambiguous duplicate versus wrong type. | Add an access reason (e.g. `access_reason::{wrong_type, absent_member, duplicate_member, index_out_of_range}`) to `error`, or document that callers must fall back to `values().lookup_member()` to tell them apart. |
| LOG-06 | Low | `core/src/parser/parser_work.cpp:85-100`; many `error::without_location(error_code::wrong_type_or_access)` sites in `core/src/parser/` | (a) Broken internal invariants (count/build pass disagreement, impossible states) are reported as `wrong_type_or_access` — the same code as caller misuse — so field reports cannot separate "library bug" from "bad call". (b) `limited_scan_error` converts **any unlocated** failure into `resource_limit` when the scan was cut by the work budget, so an `allocation_failure` from the warning-table reservation inside a cut scan is misreported. | Category confusion weakens J5-043 and the retry guidance (`allocation_failure` → grow pool vs `resource_limit` → raise a limit). | Introduce an `internal_error` code (or reason) for invariant violations, and restrict the near-cut reclassification to `syntax`/`encoding` errors. |
| LOG-07 | Low | `core/src/memory/accounted_storage.cpp:75-82` | `deallocate_callback` subtracts `bytes` from `m_current_bytes` without checking for underflow. A provider or caller error that releases a block twice or with a wrong size wraps the counter, after which every allocation fails with `resource_limit`. | Silent state corruption instead of a detectable fault; relevant for ISO 26262 defensive-programming expectations (see FS-05). | Saturate at zero and latch an error state (or assert in a debug configuration); add a unit for a mismatched release. |
| LOG-08 | Low | `core/src/memory/owned_block.cpp:126-134` | A non-power-of-two or zero alignment is reported as `resource_limit`. It is an invalid argument, not an exhausted limit. | Documented in the header, but inconsistent with the error-code table in `docs/memory-and-limits.md` (`wrong_type_or_access` = invalid argument). | Report `wrong_type_or_access` for invalid alignment. |
| LOG-09 | Info | `core/src/lexer/whitespace.cpp`; `core/src/lexer/identifier/identifier_lexer.cpp` | Each whitespace character is UTF-8 decoded twice (line-terminator check, then classification); each string and identifier is walked three to four times (count-pass measure, build-pass measure, build-pass decode, optional warning notification). All walks are charged to the work budget, so this is bounded, but it is the dominant CPU cost on small targets. | Performance only; no correctness impact. | Consider a single-decode whitespace loop and reusing the count-pass measure in the build pass (see Further improvements). |
| LOG-10 | Info | `docs/conformance.md:135` | JSON5 whitespace accepts the union of Unicode 17.0 `Zs` and Unicode 3.0 `Zs` (e.g. U+200B, which is `Cf` today). This is a documented, deliberate choice. | Interoperability: documents accepted here may be rejected by parsers using only current Unicode data. | Keep documented; consider exposing it in the support statement's deviations table rather than only in the choices table. |

## Step 3 results — Coding standards findings (MISRA, AUTOSAR, CERT, BARR-C)

**Basis and limits.** `core/docs/design-coding-style.md` selects MISRA C++:2023 and
SEI CERT C++ (2016 edition) as the coding baseline and requires that rule numbers be
checked against the authoritative text. The licensed MISRA and AUTOSAR texts were not
available to the reviewer, so every rule number below is a **candidate mapping to be
confirmed** against the licensed text before it is recorded as a deviation. No
compliance claim is made or implied.

**Evidence gathered for this step** (review-only, nothing added to CI):

| Evidence | Result |
| --- | --- |
| Pattern sweep (`throw`/`try`/`catch`, `dynamic_cast`/`typeid`, `goto`, `union`, variadics, `malloc`/`free`, `assert`/`abort`/`exit`, `virtual`, `std::function`, preprocessor conditionals) | None in production code. Every `#define` is an include guard. |
| Recursion (`clang-tidy misc-no-recursion`) | No recursion found. |
| GCC 13 with `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wold-style-cast -Wcast-align=strict -Wuseless-cast -Wdouble-promotion -Wnull-dereference -Wformat=2 -Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wimplicit-fallthrough -Wundef -Wswitch-enum -Wfloat-equal -Wzero-as-null-pointer-constant -Wextra-semi` (library + hosted) | 65 of 71 translation units clean. Diagnostics: 32 × `-Wswitch-enum` (enumerators handled by `default`), 2 × `-Wuseless-cast` (`core/src/memory/pointer_alignment.cpp:18,21`), 1 × `-Wunused-local-typedefs` from `-Wall` (`core/src/value/json5_value.cpp:226`). No conversion, sign-conversion, shadowing or old-style-cast diagnostics. |
| clang-tidy (`bugprone-*`, `cert-*`, `cppcoreguidelines-*`, `hicpp-*`, `misc-*`, `performance-*`, `portability-*`, selected readability) over `core/src/**/*.cpp` | Main themes: 55 pointer-arithmetic, 34 `unchecked-optional-access` (all inspected samples are false positives behind `result::has_value()` checks the tool cannot see), 14 cognitive-complexity > 25, 6 `reinterpret_cast`, 3 uninitialized locals, 3 `owning-memory`, 3 `signed-bitwise`. No `cert-*` or `bugprone-*` defect beyond these. |
| Lizard metrics (383 functions) | Mean cyclomatic complexity 4.85; 46 functions > 10, 22 > 15, 13 > 20; 36 functions > 50 NLOC, 10 > 100 NLOC; 10 functions with > 5 parameters; 211 functions with more than one `return`. Highest: `convert_to_floating` (CCN 54, 239 NLOC, 31 returns), `read_integer_magnitude` (36), `analyse_number` (36), `consume_decimal` (32), `parser_engine::read_value` (31). |

**Positive notes.** No exceptions, RTTI, recursion, `goto`, unions or variadics;
`noexcept` and `[[nodiscard]]` on effectively every function; no implicit narrowing
or sign conversions; `char` bytes are converted through `unsigned char` before
numeric use; checked size arithmetic; all switches have explicit handling;
placement-new storage is aligned and sized; consistent naming (`m_`, `c_`,
snake_case); full Doxygen headers with requirement trace tags.

| ID | Severity | Location | Observation | Candidate rule mapping (to confirm) | Recommendation |
| --- | --- | --- | --- | --- | --- |
| STD-01 | **Medium** | `CMakeLists.txt:62-63`; `core/docs/design-coding-style.md` ("Conditions and compiler warnings") | GCC/Clang project targets build with only `-Wparentheses -Werror`, while MSVC uses `/W4 /WX`. `-Wall` alone already reports an unused typedef that the current gate cannot see. The policy is a recorded user decision, but it leaves the GCC path (the only path CI runs) with almost no compiler diagnostics. | MISRA C++:2023 Dir 0.3.x / general "compiler warnings" expectation; ISO 26262-6 Table 1 (use of language subsets, tool support) | Adopt at least `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wold-style-cast` for project targets. The sweep above shows the code is already almost clean (fix the unused typedef and two useless casts; decide on `-Wswitch-enum`). |
| STD-02 | Medium | `core/src/value/value_storage.cpp` (placement new at `slot_address`, `->~value_record()`); `core/src/parser/parser_stack.cpp:187`; `core/src/memory/buffer_document_storage.cpp:50,86` | Placement `new`, explicit destructor calls and raw storage reinterpretation (`std::launder`) implement the flat record and frame arrays. Technically sound (alignment and capacity are checked) but these are the constructs MISRA restricts most. | MISRA C++:2023 Rule 21.6.3 (advanced memory management), Rule 21.6.1; AUTOSAR A18-5-10; CERT MEM54-CPP (complied) | Record a formal deviation (scope: the three storage owners; rationale: caller-provided fallible storage required by J5-101; compensating measures: alignment/size checks, unit tests). |
| STD-03 | Medium | `core/src/hosted/parse.cpp:29,39,68,82` | Non-placement `new (std::nothrow)`/`delete` and `::operator new/delete` are used in the optional hosted library. | MISRA C++:2023 Rule 21.6.1/21.6.2; AUTOSAR A18-5-2 | Keep it isolated to `AdaptiveJSON5::hosted` (already the case) and record the deviation; exclude the hosted library from any safety-relevant configuration. |
| STD-04 | Medium | `core/src/memory/pointer_alignment.cpp:17,29`; `core/src/memory/json5_document.cpp:47-48`; `core/src/parser/parser_engine.cpp:62`; `core/src/value/json5_value.cpp:343` | Six `reinterpret_cast`s: pointer ↔ `std::uintptr_t` for alignment and overlap checks (implementation-defined mapping), and `char*` ↔ `unsigned char*` for byte views (generally permitted). | MISRA C++:2023 Rule 8.2.5, Rule 8.2.7; AUTOSAR A5-2-4; clang-tidy `performance-no-int-to-ptr` | Keep the byte-view casts (permitted exception). For alignment, prefer `std::align` (already the documented model in `buffer_document_storage.cpp:49`) and document the integer-pointer mapping assumption as a deviation. |
| STD-05 | Medium | Callback trampolines: `accounted_storage.cpp:46,75`, `buffer_arena.cpp:6,32`, `parser_engine.cpp:473,511`, `buffer_document_storage.cpp:50`, `hosted/parse.cpp:68` | `void* context` is converted back to an object pointer with `static_cast<T*>(context)`; the C-style callback API (`storage_callbacks`, `string_warning_sink`) depends on it. | MISRA C++:2023 Rule 8.2.6; AUTOSAR M5-2-8 | Record as a deviation for the callback ABI, or move internal callers to a typed interface (template or small abstract provider) and keep `void*` only at the public C-compatible boundary. |
| STD-06 | Info | Whole production code (211 / 383 functions); `core/docs/design-coding-style.md` | Early returns on failure (`if (!x.has_value()) return failure(...)`) are the dominant style. The owner confirmed (2026-11-05) that the project follows **MISRA with a fail-fast principle**, so this is intentional. MISRA C++:2023 and AUTOSAR C++14 contain no single-exit rule, so it is **not a MISRA deviation**. Residual gap: the fail-fast principle is not written in the coding-style guide (a repository-wide search finds no mention). | Conforms to MISRA C++:2023. It departs only from the *older/stricter* single-exit rules — MISRA C++:2008 6-6-5, JSF AV 113, BARR-C:2018 §6.2 (a "preferred practice") — and from ISO 26262-6 Table 6 item 1a (see FS-04). | Record the fail-fast principle in `design-coding-style.md` (what may return early, that every early return passes the error through unchanged, that resources are released by RAII owners). The same text then serves as the ISO 26262 Table 6 1a rationale. <br>**Update 2026-11-06:** documented in `design-coding-style.md` ("Control flow and error handling") on branch `44-schema-1-…-p043p046` (`f103fc2`), **not yet merged**. Accuracy remarks on that text are in DOC-17. |
| STD-07 | Medium | Functions listed in the metrics row above | 46 functions exceed cyclomatic complexity 10 and 10 exceed 100 lines; `convert_to_floating` has CCN 54 and 239 lines. (Its 31 `return` statements are fail-fast exits and are not counted as a problem here; see STD-06.) | HIS metrics (v(G) ≤ 10, ≤ 50 statements; the HIS "one return" metric is not applied under the fail-fast principle); BARR-C:2018 §6.2 (≈100 lines); ISO 26262-6 Table 6 item 1h/“restricted size and complexity” | Split the largest functions along their natural phases (e.g. `convert_to_floating`: analysis → scratch sizing → load → scale → divide → round), and record accepted thresholds in the coding-style guide. |
| STD-08 | Low | `includes/adaptive_json5/error.hpp:60-62`; `string_warning.hpp:32-33`; private token/span structs | Several aggregates (`error`, `string_warning`, `decoded_character`, span structs, `unicode_range`) have members without default initializers; `error e;` leaves `code`, `byte_offset` and `has_byte_offset` indeterminate. Three locals are declared without initializer (`punctuation.cpp:32`, `string_boundary.cpp:34`, `string_warning.cpp:133`). | MISRA C++:2023 (initialization rules, Rule 11.6.x); AUTOSAR A8-5-0; CERT EXP53-CPP | Add default member initializers to public aggregates (e.g. `error_code code{error_code::syntax}` or a value-initialized default) and initialize the three locals. |
| STD-09 | Low | `includes/adaptive_json5/*.hpp` (22 of 38 public headers), most private headers | Public headers use unprefixed include guards such as `INPUT_HPP_`, `OPTIONS_HPP_`, `PARSER_HPP_`, `DOCUMENT_HPP_`, `STORAGE_HPP_`; 16 public headers use `ADAPTIVE_JSON5_*`. A consumer header with the same common guard silently suppresses the library header (or vice versa). | BARR-C:2018 §4.2 (header guards); MISRA C++:2023 Rule 19.2.1 (guards present – complied) | Prefix every installed header guard with `ADAPTIVE_JSON5_` and the path (the coding-style guide currently prescribes the bare filename). |
| STD-10 | Low | `core/src/value/big_unsigned.cpp`, `floating_conversion.cpp`, `number_comparison.cpp:573-642`, `core/includes/value/floating_format.hpp:22-26` | Basic types `unsigned int` and `int` are used for arithmetic (radix, chunks, signs, float format constants) instead of fixed-width types; ranges are small and correct on 16-bit-`int` targets, but this is not documented. | AUTOSAR A3-9-1; BARR-C:2018 §5.2 (fixed-width types) | Use `std::uint_least16_t`/`std::int_least32_t` (or document the minimum-range reasoning) for portability evidence. |
| STD-11 | Low | 537 `auto` declarations | `auto` is used for nearly every local, mostly for `result<...>` returned by calls (permitted case). A few deduce non-obvious arithmetic types. | AUTOSAR A7-1-5 | Acceptable for call results; spell out the type where arithmetic types are deduced. |
| STD-12 | Low | 32 `-Wswitch-enum` sites (e.g. `parser_container_close.cpp:35`, `parser_attachment.cpp:48`, `parser_dispatch.cpp`) | Switches over `parser_state`/`punctuation_kind` handle several enumerators through `default`, so adding a state will not be diagnosed. | MISRA C++:2023 Rule 9.4.2 (switch structure); AUTOSAR M6-4-6 | List every enumerator explicitly and keep `default` only as an unreachable guard, or enable `-Wswitch-enum` for the parser directory. |
| STD-13 | Info | `includes/adaptive_json5/result.hpp`, `std::variant`, `std::optional`, `std::string_view`, `std::launder`, inline variables, `if` with initializer | The code uses C++17 language and library features. AUTOSAR C++14 targets C++14 and does not cover them; MISRA C++:2023 (C++17) is therefore the correct primary baseline, as the coding-style guide already states. | AUTOSAR A1-1-1 (C++14) | Do not claim AUTOSAR C++14 conformance; map AUTOSAR rules only where they add to MISRA C++:2023. |
| STD-14 | Info | `core/src/lexer/whitespace.cpp:75,82`, `trivia.cpp:56,91,118`, `line_terminator.cpp:36,79`, `unicode.hpp:61,123,129`, `whitespace.hpp:58`, `utf16.cpp:27`, `decimal_lexer.cpp:157`, `identifier_lexer.cpp:118` | Comment typos ("succeded", "togather", "it's" for "its", "explicitely", "afterword", "Contine", "tojen", "scalers", "the after"). | BARR-C:2018 §2 (comments) | Fix in the next production edit. |
| STD-15 | Low | `core/includes/parser/parser_stack.hpp:162`; calls at `core/src/parser/parser_container_open.cpp:89`, `parser_container_close.cpp:156`, `parser_stack.cpp:201` | *Added 2026-11-06.* `parser_stack::pop()` returns `bool` but is not `[[nodiscard]]`, and all three callers ignore the result. Each call is safe in context (a frame is known to exist), but it contradicts the new rule "never ignore a returned result" (branch `44-schema-1-…`) and is the only ignored return value found in production code. | MISRA C++:2023 Rule 0.1.2 (value returned by a function shall be used); AUTOSAR A0-1-2 | Make `pop()` return `void` (callers already guarantee a frame) or mark it `[[nodiscard]]` and check it. |

**BARR-C:2018 (applied by analogy).** Braces on every block, one statement per line,
named constants instead of magic numbers (11 residual `readability-magic-numbers`
hits), include guards on every header, `default` in every switch, no `goto`, and a
120-column limit enforced by clang-format are all in place. Gaps are STD-07 (function
length), STD-09 (guard naming) and STD-10 (fixed-width types). BARR-C's *preferred*
single exit (§6.2) is consciously replaced by the project's fail-fast principle
(STD-06).

**CERT C++.** No violations found for ERR (no exceptions), MEM51/MEM54 (matched
allocation/deallocation, aligned placement new), DCL50 (no variadics), DCL59 (no
unnamed namespaces in headers), EXP54 (lifetimes) or STR50 (bounded copies). Open
items: EXP53-CPP (STD-08), INT30-C wrap in `accounted_storage` release (LOG-07) and a
data race reachable through the simple API (LOG-02, CERT CON43-C / C++ data-race rules).

## Step 4 results — ISO 26262 findings

**Context.** The repository makes **no functional-safety claim**: no ASIL, item,
safety goal, safety requirement, safety manual or SEooC statement exists
(repository-wide search for `ASIL`, `26262`, `SEooC`, `safety manual` found
nothing outside third-party GoogleTest documentation). The assessment below is
therefore a **gap analysis**: what an integrator would need before using the
library in a safety-related item, primarily per ISO 26262-6 (software) and -8
(supporting processes), treating the library as a Safety Element out of Context
(ISO 26262-10 clause 9) or as a software component to be qualified (ISO 26262-8
clause 12).

**Plan coverage (Step 4 items → findings):** 4.1 safety scope, ASIL, SEooC → FS-01;
4.2 software safety requirements and traceability → FS-02; 4.3 architectural and
unit design principles → FS-03, FS-04; 4.4 error detection, defensive programming,
freedom from interference → FS-05, FS-06; 4.5 unit verification and structural
coverage → FS-07 and the coverage table below; 4.6 supporting processes
(configuration/change management, tool confidence, component qualification,
guidelines) → FS-08, FS-09, FS-10.

**Structural coverage evidence** (review run of `tools/coverage.py`, GCC 13 gcov +
gcovr, unit tests only, x86-64 Debug, exceptions OFF, RTTI OFF):

| Metric | Result |
| --- | --- |
| Statement (line) coverage | 95.7 % (5766 / 6027) |
| Function coverage | 98.5 % (921 / 935) |
| Branch coverage (object-code branches) | 90.0 % (2821 / 3134) |
| MC/DC | Not measured (no tool configured; GCC 13 has no condition coverage) |
| Lowest branch coverage | `core/src/hosted/parse.cpp` 50 %, `parser_engine.cpp` 75 % (268/356), `floating_conversion.cpp` 76 %, `parser_duplicate.cpp` 78 % |

**Positive notes.** The design already follows several ISO 26262-6 Table 3/Table 6
principles: no recursion (1j), no unconditional jumps (1i), no global variables (1e),
no implicit conversions (1g, `-Wconversion` clean), no shadowed names (1d,
`-Wshadow` clean), explicit error results instead of exceptions, bounded resources
with explicit budgets, no use of the global heap in the core, iterative parsing with
bounded frames, and allocation-failure sweeps that fail *every* allocation point
(`J5_098_DocumentFaultSweep`, `J5_098_StrictFaultSweep`,
`J5_046_CompleteAllocation`, `J5_045_MinimalFallback`).

| ID | Severity | Clause | Observation | Impact | Recommendation |
| --- | --- | --- | --- | --- | --- |
| FS-01 | **High** | -10 cl. 9 (SEooC), -8 cl. 12, -6 cl. 6 | No safety scope: no intended ASIL, no assumptions of use (input trust level, single-threaded use per document, caller-supplied storage guarantees, timing budget), no software safety requirements and no safety manual. | An integrator cannot reuse the library in an ASIL context without re-deriving everything; the existing evidence cannot be credited. | Decide the intended use (QM only, or SEooC up to a target ASIL). If SEooC: write the assumptions of use, derive software safety requirements from them, and publish a safety manual listing integration obligations (storage, budgets, concurrency, error handling, known anomalies). |
| FS-02 | **High** | -8 cl. 6.4 (requirement attributes, verification of requirements), -8 cl. 6.4.3 | All 101 requirement rows are `Draft`; none has been reviewed/approved. Rows are not atomic (e.g. J5-003 combines compile mode, error transport and three failure classes), carry no ASIL/safety attribute and no verification status. 45 rows have no unit test (all optional-component rows plus J5-002, J5-007, J5-091, J5-093, J5-096, J5-099, J5-100). | Requirements cannot serve as a verified baseline; "Draft" plus passing tests is not requirement verification. | Split rows into atomic, verifiable statements; add attributes (ASIL/QM, status, verification method, verification result reference); run and record a requirements review; baseline them per release. |
| FS-03 | Medium | -6 cl. 7 (architectural design), -9 cl. 7/8 (safety analysis, DFA) | `core/docs/design-architecture.md` explains components but marks several diagrams as historical sketches; there is no per-component interface specification with resource usage and error behavior, no dynamic-behavior description and no software-level safety analysis (e.g. SW FMEA: wrong value accepted, valid input rejected, hang, memory corruption, wrong error category) or dependent-failure analysis for shared storage. | Architecture-level safety mechanisms cannot be justified or traced. | Add a current architecture view (static and dynamic), an interface table, and a SW FMEA + DFA. LOG-01/LOG-02 are examples the analysis would have exposed (shared scratch storage between document and reads). |
| FS-04 | Medium | -6 cl. 8, Table 6 | Unit design principles: 1a (one entry/one exit) is deliberately replaced by the fail-fast principle (211 functions return early on failure, STD-06) — acceptable, but ISO 26262 requires the rationale to be written down; 1f (restricted pointers) partly — raw pointers, `void*` contexts (STD-05); 1h (no hidden data/control flow) partly — warning-sink callbacks set engine flags (`m_string_reserved`, `m_warning_record_failed`) that are checked after the lexer returns; 1c (initialization) mostly, with exceptions in STD-08. | Each deviation needs a documented rationale for ASIL B and above. | Record the rationale per Table 6 entry in the unit-design guideline (for 1a: the fail-fast principle — errors propagate immediately and unchanged, ownership is released by RAII, so early exits cannot skip cleanup); replace the flag-based sink protocol with explicit return values where possible. <br>**Update 2026-11-06:** the 1a rationale is now written on branch `44-schema-1-…` (`f103fc2`, unmerged), but it does not yet state that RAII owners release resources on every early return (DOC-17 e). Items 1f and 1h are unchanged. |
| FS-05 | Medium | -6 cl. 7.4.12, Table 4 (error detection), -6 cl. 8 (defensive implementation) | Error-detection mechanisms exist (range checks, plausibility, pass-agreement checks) but: internal invariant violations are reported as a caller error (LOG-06); accounting underflow is silent (LOG-07); there is no integrity check of a parsed document after construction (records and payloads in caller memory can be corrupted by other software without detection); limit errors lack location and reason (LOG-03). | Faults are detected but cannot always be told apart or localized, which weakens fault reaction design at system level. | Add an `internal_error` category; latch accounting faults; optionally offer a document integrity check (e.g. link/count validation pass, already partly present in `document_links.cpp`) that a safety consumer can run before use. |
| FS-06 | Medium | -6 cl. 7.4.8 / -9 cl. 6 (freedom from interference), J5-101 | Memory: storage separation is good, but LOG-01/LOG-02 show interference between a document and its readers through shared scratch storage. Timing: the work budget bounds *work units*, not time; no WCET figure or cost-per-unit measurement exists. Stack: no stack-usage figure is documented, although J5-101 requires it; a review probe shows `sizeof(parser_engine)` = 968 bytes, `sizeof(error)` = 104 bytes and `sizeof(json5_value)` = 144 bytes on x86-64, and every `result<T>` carries an inline `error`. | Integrators cannot dimension stack or execution-time budgets, a prerequisite for FFI arguments. | Fix LOG-01/LOG-02; publish worst-case stack usage per entry point (e.g. GCC `-fstack-usage` plus call-graph analysis) and a method to convert work units to time on a target; consider shrinking `error` (e.g. a compact reason union) to reduce stack. |
| FS-07 | **High** | -6 cl. 9.4.4 / 9.4.5 (test environment, structural coverage), Tables 7, 8, 9 | (a) The tested configuration differs from the delivered one: all unit and validation tests build `Debug` (`-O0`), while `tools/package_release.py:81-83` builds release archives with `-DCMAKE_BUILD_TYPE=Release` and tests OFF; optimization-dependent behavior is never exercised. (b) Structural coverage is not a gate and is not recorded per release; branch coverage is 90 % and MC/DC is not measured (ASIL D: MC/DC highly recommended). (c) Test oracles can hide defects: `J5_037_Json5Value.ReadsDoublesUsingDocumentScratch` checks `current_bytes()`, which cannot detect LOG-01. (d) Unit tests execute on x86-64 and AArch64 Linux hosts and Windows, but never on a representative embedded target. | Verification evidence does not cover the shipped binaries or the embedded environment that J5-101 targets. | Run the unit suite on the Release configuration used for packaging (at least once per release, ideally per PR); record coverage per release and justify uncovered branches; add MC/DC measurement (e.g. GCC 14 `-fcondition-coverage` or a qualified tool) if ASIL C/D is targeted; add an arena-capacity oracle to the `as_double` test. The project-scope exclusion of coverage gates (AGENTS.md) would need a user decision for any safety use. |
| FS-08 | Medium | -8 cl. 11 (confidence in software tools) | No tool classification exists. Tools that can introduce or fail to detect errors in delivered artifacts: GCC/MinGW/Yocto SDK compilers, CMake, `tools/generate_unicode_tables.py` (generates the production header `core/includes/unicode/unicode_tables.hpp`), `tools/package_release.py`, clang-format (rewrites production source), GoogleTest/gcovr (verification tools). | Without TI/TD/TCL evaluation the tool chain cannot be relied upon in a safety case. | Write a tool-criteria evaluation; for the Unicode generator, the existing table units (`core/tests/unicode/unicode_tables_tests.cpp`) are a candidate detection measure — reference them explicitly. |
| FS-09 | Medium | -8 cl. 7 (configuration management), cl. 8 (change management) | Git, pull requests, pinned dependencies and immutable fixture revisions are good practice. Gaps: no release baseline tag for 0.2.0 in the clone, no CHANGELOG/release notes with known anomalies, unit tests run on `pull_request` only (not on `main` after merge), and the README quick start pins the head of a PR (`4d855d2`, "empty commit to create pull request") rather than a release. | Traceability from a delivered version to its verification evidence is weak. | Tag releases, publish release notes with a known-anomalies list (LOG-01 would be one), run the gates on `push` to `main`, and pin documentation to release tags. <br>**Update 2026-11-06:** partly addressed — tag `0.2.0` exists at `903aedc` and `docs/conformance.md` now records CI results for `64510be`; the README pin moves to `0.2.0` on `72-release-packaging` (unmerged). Still open: release notes with known anomalies (LOG-01 ships in 0.2.0) and gates on `push` to `main`. |
| FS-10 | Low | -6 cl. 5 (modelling and coding guidelines) | The coding guideline (MISRA C++:2023 + CERT C++) is selected but there is no deviation register, no checker and no record of which rules were assessed (STD section). | Guideline adoption cannot be evidenced. | Maintain a deviation register (STD-02 … STD-06 are initial entries) and decide whether a qualified checker is required for the target ASIL. |

## Step 5 results — ISO/SAE 21434 findings

**Context.** The repository makes no cybersecurity claim and has no cybersecurity
documentation. For a vehicle program the library would be a component developed
out of context, so the relevant ISO/SAE 21434 expectations are: organizational and
project cybersecurity management (clauses 5–6), continual activities such as
vulnerability monitoring and management (clause 8), assumptions and requirements
derived from a TARA (clauses 9 and 15), product development and verification
(clause 10), and post-development support (clauses 12–14). Its primary attack
surface is **untrusted JSON5/JSON input** parsed on an ECU or backend; the
secondary surface is the **supply chain** of its source and release archives.

**Plan coverage (Step 5 items → findings):** 5.1 component context and attack
surface → this context paragraph; 5.2 TARA-lite → the TARA table below; 5.3
cybersecurity requirements and controls (DoS, memory safety, disclosure) → TARA
table, CS-03, CS-05, CS-07; 5.4 supply chain (pinning, FetchContent, actions,
release integrity, SBOM) → CS-04 (with REPO-05); 5.5 vulnerability management and
post-development → CS-01, CS-06; 5.6 verification evidence for security claims →
robustness evidence table and CS-02.

**Robustness evidence gathered by the reviewer** (review-only; none of it is a
repository gate):

| Evidence | Result |
| --- | --- |
| All 3435 unit and validation cases built with `-fsanitize=address,undefined -fno-sanitize-recover=all` (GCC 13, `-O1`) | **No UBSan report and no ASan/LeakSanitizer report in production code.** 59 failures are `alloc-dealloc-mismatch` reports caused by test-only global `operator new/delete` replacements (REPO-03); 1 locale case skipped. |
| libFuzzer + ASan + UBSan (clang 18) harness over `parse_document` (both syntax modes, both duplicate policies, retention, BOM and warning-storage options, tight work budget, depth 2000), full value-tree walk with `to_double`/`to_float`/integer conversions/`export_utf8`, `format_error`, and the simple API incl. `describe`; seed corpus = committed JSON5 and JSONTestSuite fixtures | **No crash, timeout, out-of-memory or sanitizer report.** 4 workers × 601 s: 21,486,466 executions (5.29–5.52 M per worker, about 10 k exec/s each), 2344 covered edges, 13,509 features, final corpus 1899 inputs, peak RSS 523 MB per worker (fuzzer bookkeeping). The harness and corpus stayed in the reviewer's scratch area and were not committed. |

**TARA-lite for the parser** (asset → threat → existing control → residual risk):

| Asset / property | Threat scenario | Existing controls | Residual risk / finding |
| --- | --- | --- | --- |
| Integrity and confidentiality of process memory | Crafted input triggers out-of-bounds read/write or integer wrap | Bounds-checked cursor, checked size arithmetic, transactional lexers, no recursion, no global state; sanitizer and fuzz evidence above | Low; no continuous robustness testing (CS-02) |
| Availability of the consuming service/ECU | Deep nesting, huge strings/numbers, many members, pathological duplicates, long comments | Configurable limits with conservative defaults, work budget charged per scanned byte, quadratic duplicate check bounded by the work budget, scratch bound for numeric conversion | **LOG-01** lets attacker-chosen numbers exhaust a caller buffer during later reads (CS-03); no time bound per work unit (FS-06) |
| Integrity of configuration semantics | Duplicate-key or Unicode confusion makes this parser and another component interpret the same file differently | Every duplicate is preserved and the simple API refuses ambiguous lookups; exact UTF-16 equality without normalization; lone surrogates preserved and rejected on UTF-8 export | Iteration-based consumers may still pick the first or last duplicate; embedded NUL survives `as_string` (CS-05) |
| Confidentiality of input | Diagnostics leak document content | Messages never echo input bytes; source retention is opt-in | None found |
| Integrity of delivered library | Tampered dependency, action, toolchain or release archive | Pinned GoogleTest SHA (host only), pinned fixtures, SHA-pinned actions in release jobs, SHA-256 files and SDK digest check | Mixed pinning, floating runners/toolchains, no signatures/provenance/SBOM (CS-04) |

| ID | Severity | Clause | Observation | Recommendation |
| --- | --- | --- | --- | --- |
| CS-01 | **High** | 5.4, 6.4, 8, 13 | No cybersecurity scope or process artifacts: no `SECURITY.md`/vulnerability-disclosure channel, no cybersecurity requirements or claims, no out-of-context assumptions (input trust, deployment, update path), no cybersecurity case, no support/end-of-life statement. | Add `SECURITY.md` (reporting channel, supported versions, response targets); write out-of-context assumptions and cybersecurity requirements from the TARA-lite above; keep a cybersecurity case per release. |
| CS-02 | Medium | 10.4.2 (verification methods: fuzz testing, vulnerability scanning) | The project scope (`AGENTS.md`) excludes fuzz, sanitizer and static-analysis gates. For a parser of untrusted input these are the standard verification methods, and the review runs show they are cheap and currently clean. | Ask the owner to allow at least a scheduled, non-blocking sanitizer + fuzz workflow (like the existing non-blocking `examples` job) and to commit the fuzz harness and corpus under `tests/`. |
| CS-03 | Medium | 10.4.1 (design weaknesses), 15.6 (attack feasibility) | LOG-01 is attacker-amplifiable: a document with long numeric spellings increases the scratch consumed per `as_double()`, so a malicious configuration makes later reads fail sooner (availability). LOG-02 adds a data race when reads are shared between threads. | Fix LOG-01/LOG-02; add a regression test with long numbers on a small buffer. |
| CS-04 | Medium | 6.4.x (supplier/OTS components), 12 (production), 10.4.1 | Supply chain: actions pinned by tag in three workflows and by SHA elsewhere; `ubuntu-latest`; unpinned `apt-get install g++-13` and MSYS2 `update: true` in release builds; release integrity relies on SHA-256 files uploaded to the same release (detects corruption, not tampering); no SBOM and no signed provenance. | Pin all actions and runner images; record exact compiler package versions; publish GitHub artifact attestations (SLSA provenance) or Sigstore signatures and an SPDX/CycloneDX SBOM with each release; enable Dependabot for GitHub Actions. |
| CS-05 | Low | 9.3 (assumptions), 10.4.1 | Default `duplicate_name_policy::preserve_all` and iteration APIs allow consumer code to resolve duplicates differently from other parsers; `as_string()` returns bytes that may contain NUL, which C-string consumers truncate. | Recommend `reject_duplicates` in the user guide for security-relevant configuration; document the NUL behavior in `json5_value.hpp` and offer an optional NUL-rejecting read. |
| CS-06 | Low | 8.5–8.6 (vulnerability analysis/management), 13 | No dependency or advisory monitoring, no known-anomaly list per release, no process to issue fixes for released versions. | Add release notes with a known-anomalies section, enable GitHub security advisories and Dependabot alerts. |
| CS-07 | Info | 10.4.1 | The optional hosted library uses the global heap; the core does not. | State in the security guidance that safety/security-relevant deployments should use the caller-buffer or callback storage path with explicit limits. |

## Step 6 results — Architecture findings

**Positive notes.** The ADR 0002 boundary holds: no public header includes the
private tree (`grep` over `includes/`), the private include directory is `PRIVATE`
on both libraries, production sources are explicit lists, and test code is never in
production source lists. The core/hosted split keeps the global heap out of the
embedded target. ADR 0003 records each storage/budget decision with problem,
rationale, consequences and rejected alternatives, which is above average for a
project of this size. Binary packages refuse to load with a mismatched compiler,
target triple, thread model, libstdc++ ABI or MinGW CRT (`cmake/AdaptiveJSON5Config.cmake.in`).

| ID | Severity | Location | Observation | Impact | Recommendation |
| --- | --- | --- | --- | --- | --- |
| ARC-01 | Medium | `core/src/value/json5_value.cpp`, `core/src/memory/json5_document.cpp:57-64` | The simple API couples *reading* to the document's *allocator*: `json5_document::root()` hands every `json5_value` the document's accounted callbacks as scratch. This is the root cause of LOG-01 (arena exhaustion) and LOG-02 (hidden mutation behind `const`). | A read-only view type has write side effects on shared state. | Make value reads allocation-free (bounded stack scratch or a fast path for doubles with ≤ 19 significant digits and small exponents, falling back to an explicit caller-supplied scratch only for long spellings), or give `json5_document` a separate, reclaimable scratch region. |
| ARC-02 | Medium | `core/src/parser/parser_engine.cpp` (`count_*` vs `read_*`/`consume_*` methods) | The two-pass design (ADR 0003 decision 7) duplicates the grammar state machine: `count_value`/`count_name`/`count_separator`/`count_close` mirror `read_value`/`read_name`/`consume_separator`/`finish_container`. Agreement is checked only at run time ("passes disagree" → internal error). | Every grammar or limit change must be made twice; a divergence becomes a field failure rather than a compile error. | Drive both passes from one templated walker with a counting policy and a building policy, so the grammar exists once. |
| ARC-03 | Medium | `includes/adaptive_json5/value/document.hpp`, `memory/owned_block.hpp`, `memory/document_storage_owner.hpp` | The public surface (38 headers) exposes construction internals: document mutators (`make_*`, `append_*`, `set_source_span`, `set_root`), `owned_block`/`allocate_block`, `detail::value_storage`, `detail::document_storage_owner`. A consumer can mutate a parsed `parsed_document::values` (for example append to an exact-reserved document, which then fails with the internal-error code). | Large API/ABI commitment for 0.x; invariants of parsed documents are not protected by the type system. | Split a read-only `document` view from a `document_builder`, move `owned_block`/`document_storage_owner` under an explicitly unsupported `detail/` folder, and document which headers are stable. |
| ARC-04 | Low | `includes/adaptive_json5/error.hpp` | `error` carries four `std::optional` members (duplicate spans, numeric reason, source range, syntax reason) → 104 bytes on x86-64, embedded inline in every `result<T>`. LOG-03 and LOG-05 would add more reasons. | Stack and copy cost on small targets; the structure grows with every new reason. | Use one compact reason field (category-specific enum stored in a small integer) plus at most one span pair, keeping duplicate locations as the only two-span case. |
| ARC-05 | Low | `includes/adaptive_json5/memory/storage.hpp` | `storage_callbacks` has two allocation entry points (`allocate` returning `nullptr`, `allocate_result` returning `result<void*>`) with precedence rules. | Two paths to test and document for every provider. | Keep one (the `result` form) in the next breaking release, with an adapter for pointer-returning allocators. |
| ARC-06 | Low | `cmake/AdaptiveJSON5Config.cmake.in:77-79` | The package config sets `AdaptiveJSON5_parser_FOUND` and `AdaptiveJSON5_build_options_FOUND` but not `AdaptiveJSON5_hosted_FOUND`, so `find_package(AdaptiveJSON5 CONFIG REQUIRED COMPONENTS hosted)` fails even when the hosted library is installed; consumers must use `if(TARGET AdaptiveJSON5::hosted)`. | Unidiomatic component discovery. | Set `AdaptiveJSON5_hosted_FOUND` when the target exists. |
| ARC-07 | Low | `CMakeLists.txt:121` | Package compatibility is `SameMinorVersion`, but there is no ABI policy (no inline versioned namespace, no statement on which build options change the ABI beyond the recorded exceptions/RTTI values). | Mixing two 0.2.x builds with different options is not detected. | Document the ABI policy; consider an inline namespace (e.g. `adaptive_json5::v0_2`) once the API stabilizes. |
| ARC-08 | Info | `CMakeLists.txt:12-15`, `cli/README.md`, `schema/README.md`, `editor/README.md` | `ADAPTIVE_JSON5_BUILD_SCHEMA` fails configuration ("not available yet") and `ADAPTIVE_JSON5_BUILD_EXECUTABLE` builds the example programs, not a CLI. The optional components are placeholders. | Option names promise more than they deliver. | Rename `ADAPTIVE_JSON5_BUILD_EXECUTABLE` to `ADAPTIVE_JSON5_BUILD_EXAMPLES` (keep the old name as an alias) until the CLI exists. |

## Step 7 results — Documentation findings

**Checks performed.** Relative links and anchors in all 37 tracked project Markdown
files (one broken anchor found); README quick start extracted and compiled against
the baseline in both memory modes (heap and caller buffer) — both print
`port: 8080` as documented; documents compared against code behavior established in
Step 2.

**Positive notes.** The documentation is unusually explicit about the difference
between implementation, executed tests and requirement verification;
`docs/conformance.md` honestly marks the 0.1.0-era review as historical and lists
the 0.2.0 release evidence as pending; `docs/memory-and-limits.md` is a clear,
user-oriented guide; every public function carries a Doxygen contract with
requirement tags; the ADRs record rejected alternatives.

| ID | Severity | Location | Observation | Recommendation |
| --- | --- | --- | --- | --- |
| DOC-01 | Medium | `docs/memory-and-limits.md` ("Parse into a buffer" → *Sizing*); `includes/adaptive_json5/json5_value.hpp` | The buffer-sizing method (`peak_bytes()` plus a margin) and the `json5_value` contract ("owns nothing and allocates nothing, except `as_double()`, which uses scratch memory from the document's own storage") do not say that this scratch is never returned to a caller buffer while the document lives (LOG-01). | Fix LOG-01, or document the per-call consumption and the resulting read limit until it is fixed. |
| DOC-02 | Medium | `includes/adaptive_json5/json5_document.hpp`, `json5_value.hpp` | No thread-safety statement for the simple API, while `document.hpp:209` promises lock-free concurrent const reads for `document` (LOG-02). | Add an explicit statement to both headers and to the user guide. |
| DOC-03 | Medium | `docs/embedded.md:22-23`; J5-101 | The embedded page says to "record static initialization, alignment, byte order, integer-size assumptions, stack/work storage and actual compiler/device limitations", but records none of them (no stack figure, no static-initialization statement, no data-model assumptions). | Fill in the figures (stack per entry point, no static constructors, required C++ runtime subset, assumed `CHAR_BIT == 8`, two's-complement, `std::uintptr_t` availability, alignment needs). |
| DOC-04 | Low | `docs/memory-and-limits.md:237-238` | "Invalid or over-limit input fails before any allocation … every limit in the table above [is] found in the first pass." `max_owned_bytes` can only fail when allocating (second pass), and `max_work_units` can also fail in the second pass because both passes are charged. | Narrow the sentence to the limits that the count pass actually checks. |
| DOC-05 | Low | `docs/memory-and-limits.md:214-215` | Unknown-word diagnostics are documented as always covering the whole word; words beginning with `N`/`I` do not (LOG-04). | Fix LOG-04 or document the exception. |
| DOC-06 | Low | `docs/memory-and-limits.md` error table ("`resource_limit` … Raise the specific limit") | Users cannot identify the specific limit from the error (LOG-03). | Add the limit reason, then reference it here. |
| DOC-07 | Low | `docs/conformance.md:156` | Broken anchor `requirements.md#minimum-unit-regression-checklist`; the checklist moved to `core/docs/requirements-quality.md` in issue #98. | Point the link to `../core/docs/requirements-quality.md#minimum-unit-regression-checklist`. |
| DOC-08 | Low | `docs/traceability.md:26-70` | The "Layout and mapping rules" section and the "Core requirements" heading are duplicated (headings at lines 26/49 and 47/70); the first "Core requirements" heading is empty. | Remove the duplicate block. |
| DOC-09 | Low | `AGENTS.md:8`; `docs/dependencies.md:14` | Both point to the proposal under `docs/requirements/proposals/`, which does not exist; the file is `core/docs/requirements-proposal-json5-parser-repository.md`. | Update the path. |
| DOC-10 | Low | `docs/requirements.md:12`; `core/docs/requirements-quality.md` (J5-099) | Both still say automated quality checks are "unit tests and formatting only", while `AGENTS.md`, the README and CI include validation tests as a separate gate plus the non-blocking examples job. | Align the wording with the current scope decision. |
| DOC-11 | Low | `docs/adr/0001-project-scope.md:21` vs `docs/requirements.md` (J5-001, J5-091) and `README.md:317` | ADR 0001 targets "Windows, Linux and macOS"; the requirements name only Windows and Linux, and macOS is listed as unexecuted/unsupported. | Amend ADR 0001 (or add a superseding note) to match the requirements. |
| DOC-12 | Low | `core/docs/design-architecture.md:137` | The "Complete picture now" diagram still shows "Remaining: P034/P035 acceptance, duplicate policy and unified public root entry", all of which are implemented. | Remove or relabel the node as historical. |
| DOC-13 | Low | `README.md` ("Quick start", `GIT_TAG 4d855d2…`) | The quick start pins the head commit of a PR titled "empty commit to create pull request (#118)", whose `CMakeLists.txt` still declares version 0.1.0, while the README's installed-package path asks for `find_package(AdaptiveJSON5 0.2 …)`. The conformance page already lists this as pending. | Switch to the `0.2.0` tag as soon as it exists. <br>**Update 2026-11-06:** fixed on branch `72-release-packaging` (`0b8957e`, README pins `GIT_TAG 0.2.0`), **pending merge**. |
| DOC-14 | Low | `includes/adaptive_json5/result.hpp:230,245` | In `result<void, E>`, the Doxygen brief of both `failure()` and `error_if()` reads "Construct failure with a payload"; `error_if()` is an accessor. | Correct the brief for `error_if()`. |
| DOC-15 | Info | `README.md:356-363`, `AGENTS.md:89-96`, `docs/requirements.md:226-232` | The same "official-specification implementation sequence" paragraph pair is repeated verbatim in three documents; the README's copy is appended after the License section. | Keep it in one place (the implementation-order document) and link to it. |
| DOC-16 | Info | Whole `docs/` tree | There is no CHANGELOG/release notes and no known-anomalies list; the conformance page carries release scope and limits instead. | Add release notes per tag with a known-anomalies section (also needed for FS-09 and CS-06). |
| DOC-17 | Low | `core/docs/design-coding-style.md` "Control flow and error handling" on branch `44-schema-1-…-p043p046` (`f103fc2`, unmerged) | *Added 2026-11-06.* The new section correctly records the fail-fast convention, but several rules are stated more absolutely than the code behaves: (a) "propagate the first error unchanged" — `limited_scan_error` (`core/src/parser/parser_work.cpp:85-100`) deliberately turns errors near a work-budget cut into `resource_limit` (ADR 0003 decision 5; see LOG-06); (b) "a failed operation leaves its inputs and the object as they were" — the internal `parser_engine` is single-use and is discarded as a whole on failure without rolling back its counters, as `docs/memory-and-limits.md` states; (c) "charge before work" — the string decode pass is charged after it ran (`parser_engine.cpp:247`), and limited scans bound work by shortening the view and charge afterwards; (d) "never ignore a returned result" — see STD-15; (e) the section does not say that RAII owners release resources on every early return, which is the key argument for the ISO 26262 Table 6 1a rationale (FS-04). The same file still says quality checks are "unit tests and formatting" (lines 56 and 80, DOC-10) and still prescribes unprefixed header guards (STD-09). | State the intended exceptions (single-use parser state, limited-scan charging, near-cut reclassification) or adjust the rules; add one sentence on RAII cleanup; fix the two stale quality-check sentences while editing the file. |

## Step 8 results — Full repository findings

**Checks performed.** CMake files and presets, all five workflows, test layout and
include boundaries, test infrastructure under sanitizers (ASan + UBSan run of all
3435 unit and validation cases, review-only), Python tools, fixtures, licensing
files, editor configuration and repository hygiene.

**Positive notes.** Least-privilege workflow permissions (`contents: read`, `write`
only on the publish job), `persist-credentials: false`, workflow inputs passed via
environment variables rather than inline expressions, SHA-256 verification of the
Yocto SDK and of release archives, pinned GoogleTest revision with an offline
override, committed and pinned third-party fixtures with licenses, a clean
public/private test boundary (no file under `tests/` includes a private header),
test code roughly 1.2× the size of production code, and a clean ASan/UBSan run of
production code (see CS evidence).

| ID | Severity | Location | Observation | Recommendation |
| --- | --- | --- | --- | --- |
| REPO-01 | Medium | `.github/workflows/unit-tests.yml:3-4`, `validation-tests.yml`, `formatting.yml` | Quality gates trigger on `pull_request` only. `main` is never re-verified after merge (e.g. after a merge commit or a base update), and no scheduled run detects tool-chain drift (MSYS2 GCC floats, `ubuntu-latest` moves). | Add `push: branches: [main]` and a weekly `schedule` trigger. |
| REPO-02 | Medium | `unit-tests.yml` (all jobs), `validation-tests.yml`; `tools/package_release.py:81-83` | Every test job builds `Debug`; release archives are built `Release` with tests OFF (see FS-07). | Add a `Release` (or `RelWithDebInfo`) unit-test leg, ideally on the same configuration the packager uses. |
| REPO-03 | Low | `tests/api/pure_memory_tests.cpp:24-66`; `tests/api/diagnostic_format_tests.cpp:45-63` | These units replace the global `operator new`/`operator delete` with `malloc`/`free` but not every overload. Under ASan, memory allocated by a non-replaced overload (e.g. GoogleTest's `std::get_temporary_buffer` path) is freed through the replaced sized `operator delete`, giving 59 `alloc-dealloc-mismatch` failures in the review's sanitizer run. In normal builds they pass. | Replace all overloads consistently (plain, nothrow, aligned, sized, array forms) so the suite is sanitizer-clean; this keeps a future sanitizer run (even a local one) meaningful. |
| REPO-04 | Low | `cmake/Dependencies.cmake:5-9,17-20` | When unit tests are enabled the module sets `FETCHCONTENT_UPDATES_DISCONNECTED` in the including scope, `FORCE`s `BUILD_GMOCK`/`INSTALL_GTEST`/`gtest_force_shared_crt` into the cache, and downloads GoogleTest into the *source* tree (`tools/gtest`). In a parent project that enables this subproject's tests, this overrides the parent's GoogleTest settings and writes into a possibly read-only source directory; parallel build directories share one checkout. | Scope the settings (e.g. `block()`/function scope), avoid `FORCE` on generic GoogleTest options, and default `SOURCE_DIR` to the binary directory. |
| REPO-05 | Low | `.github/workflows/*.yml` | `ubuntu-latest` is used in four workflows; `actions/checkout@v7` and `actions/setup-python@v7` are tag-pinned in `unit-tests.yml` (first job), `formatting.yml` and `examples.yml`, while other jobs pin SHAs. `pip install clang-format==18.1.8` has no hash pin. | Pin runner images and all actions by SHA consistently; use `--require-hashes` (or a hashed requirements file) for Python tools. Covered further in CS-04. |
| REPO-06 | Low | `tools/tests/test_package_release.py`; `tools/generate_unicode_tables.py` | The packaging script's Python tests are not run by any workflow, and no job checks that `core/includes/unicode/unicode_tables.hpp` regenerates byte-identically from `tools/unicode-data/`. | Add a lightweight `tools` job (pure Python, no new C++ gate) or document both as manual release steps. <br>**Update 2026-11-06:** confirmed by the release itself — the first 0.2.0 packaging run failed its header inventory check because `tools/package_release.py` expected the optional `hosted/` header; the fix is on `72-release-packaging` (`6caab0d`, unmerged). Running the packaging tests/script on pull requests would have caught it before the tag. |
| REPO-07 | Low | `tests/value/numeric_locale_tests.cpp:91,163` | The locale-independence unit (J5-052) is skipped on every Linux runner because no comma-decimal locale is installed; it executes only on the Windows job. | Install a comma-decimal locale (e.g. `de_DE.UTF-8`) in the Linux jobs. |
| REPO-08 | Low | `.gitattributes` | Only `tests/fixtures/third_party/** -text` is declared; there is no `* text=auto eol=lf` normalization although development happens on Windows and Linux. | Add text normalization to avoid CRLF churn and formatter differences. |
| REPO-09 | Low | 171 of 172 production C++ files | No per-file copyright/SPDX header (only one file has `SPDX-License-Identifier`); tests have none. Apache-2.0 does not require it, but automotive supplier audits and SBOM tooling commonly expect it. | Add `// SPDX-License-Identifier: Apache-2.0` and a copyright line to each source file. |
| REPO-10 | Low | `.github/pull_request_template.md` | The validation checklist names unit tests (exceptions OFF/RTTI OFF) and formatting, but not the validation-test gate or the exceptions-ON leg that CI requires. | Align the checklist with the required checks. |
| REPO-11 | Info | `CMakeLists.txt:122-135` | Inconsistent indentation inside the `install(...)`/hosted `if` block. | Reformat (cmake-format or the `.neocmakelint.toml` rules). |
| REPO-12 | Info | `core/docs/requirements-issue-backlog-before-consolidation-2026-10-13.json` (158 kB), `core/docs/requirements-issue-backlog-before-p-sequence.json` (71 kB) | Historical backlog snapshots live beside the current backlog and implementation-order JSON. | Move snapshots to an `archive/` folder or rely on git history. |
| REPO-13 | Info | `.zed/user-config/AGENTS.md`, `.zed/user-config/settings.json` | Personal editor configuration (including a commit-message prompt) is committed. | Keep project-wide editor settings only, or document why user-level settings are shared. |
| REPO-14 | Info | Branch `44-schema-1-component-owned-references-exact-equality-and-bounded-evaluation-p043p046` | *Added 2026-11-06.* The coding-style decision (a docs-only change) is the only commit on the schema P043–P046 feature branch, which is based on `64510be` and one commit behind `main`; the commit subject has a typo ("conding"). Merging it with the schema work ties a project-wide convention to an unrelated feature review. | Land the coding-style change in its own small documentation PR (and fix the subject), then rebase the schema branch on `main`. |

## Step 9 results — Summary

**Overall assessment.** The production code is careful, consistent and well tested
for a 0.x library: no exceptions, RTTI, recursion or global state; checked
arithmetic throughout; clean under an extended GCC warning set apart from three
cosmetic diagnostics; 95.7 % line / 90.0 % branch coverage from unit tests alone;
and no sanitizer or fuzzing defect in production code. The review found **one
high-severity functional defect** (LOG-01: repeated `as_double()` reads exhaust a
caller-buffer document) with a related concurrency hazard (LOG-02). Most other
High/Medium items are **process gaps** that matter only if the library is used in
an ISO 26262 or ISO/SAE 21434 context, which the repository does not currently
claim.

| Area | High | Medium | Low | Info | Total |
| --- | ---: | ---: | ---: | ---: | ---: |
| Step 2 — Logic (LOG) | 1 | 2 | 5 | 2 | 10 |
| Step 3 — Coding standards (STD) | 0 | 6 | 6 | 3 | 15 |
| Step 4 — ISO 26262 (FS) | 3 | 6 | 1 | 0 | 10 |
| Step 5 — ISO/SAE 21434 (CS) | 1 | 3 | 2 | 1 | 7 |
| Step 6 — Architecture (ARC) | 0 | 3 | 4 | 1 | 8 |
| Step 7 — Documentation (DOC) | 0 | 3 | 12 | 2 | 17 |
| Step 8 — Full repository (REPO) | 0 | 2 | 8 | 4 | 14 |
| **Total** | **5** | **25** | **38** | **13** | **81** |

**Prioritized recommendations**

1. **Fix LOG-01/LOG-02** (with ARC-01, CS-03, DOC-01, DOC-02): make `as_double()`
   independent of the document's arena or make the arena reclaim LIFO releases; add a
   regression test that loops `as_double()` on a small buffer and checks arena
   capacity, not only `current_bytes()`; state the simple API's thread-safety rule.
   This is production code, so it is for the maintainer to implement.
2. **Test what is shipped** (FS-07, REPO-01, REPO-02): add a Release unit-test leg and
   run the gates on `push` to `main` and on a schedule.
3. **Decide the safety and security scope** (FS-01, CS-01): QM-only versus SEooC; add
   `SECURITY.md`, assumptions of use and a known-anomalies list per release.
4. **Turn on standard compiler warnings** (STD-01): the code is already almost clean.
5. **Improve failure diagnostics** (LOG-03, LOG-05, LOG-06, ARC-04): limit reason and
   location, access reason, and a distinct internal-error category, ideally with a
   more compact `error` layout.
6. **Baseline the requirements** (FS-02): atomic rows with attributes and a recorded
   review, so that status can move beyond Draft.
7. **Allow optional robustness jobs** (CS-02): a scheduled, non-blocking sanitizer and
   fuzz workflow; the review's harness found no defect in about ten minutes and could
   be committed as a test-only target if the owner agrees.
8. **Harden the supply chain** (CS-04, REPO-05): SHA-pin all actions and runner
   images, publish provenance/attestations and an SBOM with each release.
9. **Start a MISRA deviation register and reduce complexity hot spots** (STD-02 …
   STD-07, FS-10), beginning with `convert_to_floating`, `read_integer_magnitude`,
   `analyse_number` and `consume_decimal`.
10. **Batch the documentation fixes** (DOC-03 … DOC-14), especially the embedded
    figures that J5-101 asks for.

## Step 9 results — Questions (no answer needed during the review)

These questions came up during the review. None blocks this document; the answers
would refine severities and priorities in a follow-up.

1. Is the library intended for QM use only, or as an SEooC for a target ASIL? This
   decides whether MC/DC, a qualified MISRA checker and a safety manual are needed,
   and how formally the fail-fast rationale must be recorded (FS-01, FS-04, FS-07).
2. In the intended vehicle deployments, is JSON5 input trusted (built into the
   image) or untrusted (received over the air, from a backend or a tester)? This sets
   the depth of the ISO/SAE 21434 work (CS-01).
3. Was the GCC/Clang warning set limited to `-Wparentheses` deliberately, for example
   because the same helper also configures the GoogleTest targets (STD-01)?
4. Before 1.0, would an API change to `json5_value::as_double()` (separate scratch or
   no scratch) be acceptable, or must the signature stay fixed (LOG-01)?
5. Should the document builder API (`document::make_*`, `append_*`, `set_root`,
   `set_source_span`) remain a supported public API (ARC-03)?
6. Is the quadratic `reject_duplicates` cost acceptable long term, or would a hashed
   pre-check with exact equality confirmation (allowed by J5-033) be welcome?
7. ~~Will `0.2.0` be tagged from the reviewed baseline `64510be`, and will the README
   quick start then pin that tag (DOC-13, FS-09)?~~ *Answered 2026-11-06:* tagged at
   `903aedc` (only `docs/conformance.md` differs from `64510be`); the README pin is on
   `72-release-packaging`. New question: will 0.2.0 get release notes that list LOG-01
   as a known anomaly, or will a 0.2.1 with the fix follow?
8. Is the AdaptivePi Yocto AArch64 profile planned as the first target for on-device
   test execution (FS-07 d)?
9. MSVC flags are configured but never executed in CI. Is MSVC (or Clang) support
   planned, or should those branches be removed until it is?
10. Who reviews and approves requirement rows to move them out of Draft, and is a
    review record planned (FS-02)?
11. Should `.zed/user-config/` stay in the repository (REPO-13)?

## Step 9 results — Further improvements

Improvements that are not defects and are outside the findings above:

- **Performance on small targets:** single-decode whitespace loop; reuse the
  count-pass measurement in the build pass instead of measuring strings and
  identifiers again (LOG-09).
- **One grammar walker:** generate the count and build passes from one templated
  walker with counting and building policies (ARC-02).
- **Allocation-free doubles:** a Clinger/Eisel-Lemire style fast path for spellings
  with ≤ 19 significant digits and small exponents, using big-integer scratch only as
  a fallback (removes most of LOG-01's exposure and speeds up reads).
- **Faster duplicate rejection:** hashed bucket pre-check with exact UTF-16 equality
  confirmation, keeping the work budget.
- **Integrity check API:** an optional `validate(document)` that re-checks links,
  counts and payload invariants, useful to safety consumers before use (FS-05).
- **Stack and timing data:** generate `-fstack-usage` reports in the release job and
  publish a per-entry-point stack table plus a method to calibrate time per work unit
  (FS-06, DOC-03).
- **Generated traceability:** produce `docs/traceability.md` from the test names and
  requirement tags with a script, so duplicated or stale sections cannot recur
  (DOC-08).
- **Release notes template** with known anomalies, supported versions and evidence
  links (DOC-16, CS-06, FS-09).
- **Developer presets** for Release units and for local sanitizer runs (no CI gate
  implied), and a pinned `tools/requirements.txt` for gcovr/clang-format.
- **Optional NUL-rejecting string read** for consumers that pass strings to C APIs
  (CS-05).
- **ABI hygiene:** inline versioned namespace and a written ABI policy (ARC-07).
- **Naming:** rename `ADAPTIVE_JSON5_BUILD_EXECUTABLE` to
  `ADAPTIVE_JSON5_BUILD_EXAMPLES` with a compatibility alias (ARC-08).
- **Licensing hygiene:** SPDX headers in all source files (REPO-09) and
  `* text=auto eol=lf` in `.gitattributes` (REPO-08).
- **MC/DC:** evaluate GCC 14 `-fcondition-coverage` locally to see the gap before
  deciding on any safety target (FS-07).

---

## Follow-up — re-check of newer branches (2026-11-06)

After the review was published, three refs moved. Each was compared with the reviewed
baseline `64510be`.

| Ref | Commits after `64510be` | Content | Effect on this review |
| --- | --- | --- | --- |
| `origin/main` = tag **`0.2.0`** (`903aedc`) | 1 | `docs/conformance.md` only: records the CI results for `64510be` (2514/2514 units on Linux x86-64, Linux aarch64 and Windows MinGW in both exception modes; 921/921 validation; formatting; examples 3/3) and marks the 0.2.0 review complete. | **No production, header, CMake or test change**, so the shipped 0.2.0 code is exactly the reviewed code and every LOG/STD/ARC finding applies to the release. The recorded CI results agree with the reviewer's own baseline run. The recorded configurations are all `Debug` (FS-07 a unchanged). |
| `72-release-packaging` (`0b8957e`, unmerged) | 2 on top of `main` | README quick start pins `GIT_TAG 0.2.0`; packages built with `ADAPTIVE_JSON5_BUILD_HOSTED=OFF`; `tools/package_release.py` no longer expects the `hosted/` header after the first packaging run failed its inventory check; release-package and conformance docs updated. | Resolves DOC-13 once merged; confirms REPO-06 (the packaging defect surfaced only at release time); FS-09 partly addressed. Packages remain built `Release` without tests (FS-07 a). |
| `44-schema-1-component-owned-references-exact-equality-and-bounded-evaluation-p043p046` (`f103fc2`, unmerged) | 1 on top of `64510be` | Adds "Control flow and error handling" to `core/docs/design-coding-style.md`: fail-fast early returns, errors as values, first error propagated unchanged, validate before changing state, charge before work, checked arithmetic, no recursion or hidden allocation, braces, `const`, private members first. | Closes the documentation gap in STD-06 and most of the FS-04 1a rationale once merged. New findings DOC-17 (rules stricter than the code), STD-15 (ignored `pop()` result) and REPO-14 (docs change on a feature branch). |

**Status of findings touched by the newer refs**

| Finding | Status on 2026-11-06 |
| --- | --- |
| STD-06 fail-fast not documented | Addressed on `44-schema-1-…`, pending merge |
| FS-04 Table 6 1a rationale | Mostly addressed on `44-schema-1-…`; add the RAII cleanup sentence (DOC-17 e) |
| FS-09 configuration/change management | Partly addressed: `0.2.0` tag and recorded results exist; release notes with known anomalies and `push` gates still open |
| DOC-13 README pins a PR commit | Fixed on `72-release-packaging`, pending merge |
| REPO-06 packaging script not exercised before release | Confirmed by the failed first packaging run; fix pending on `72-release-packaging` |
| DOC-07 broken anchor in `conformance.md` | Still open on `main` (`docs/conformance.md:158`) |
| DOC-10 "unit tests and formatting only" | Still open, and still present twice in the updated coding-style guide |
| STD-09 unprefixed header guards | Still open; the coding-style guide still prescribes `ERROR_HPP_`-style guards |
| LOG-01 … LOG-10, STD-01 … STD-05, STD-07 … STD-14, ARC-01 … ARC-08 | Unchanged — no production code changed; all apply to the `0.2.0` release |
| All other FS, CS, DOC and REPO findings | Unchanged |

**New findings from the re-check:** STD-15 (Low), DOC-17 (Low), REPO-14 (Info), listed in
their step tables above. Updated totals: **81 findings — 5 High, 25 Medium, 38 Low,
13 Info.**

**Recommendation for the next step.** Merge `72-release-packaging`, then publish 0.2.0
release notes that list LOG-01 (and LOG-02) as known anomalies with the workaround
"use `values()` with `to_double()` and your own scratch storage, or the hosted
library, for repeated floating-point reads". Land the coding-style text as its own
documentation PR with the DOC-17 adjustments.
