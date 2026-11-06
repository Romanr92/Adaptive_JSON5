# Full repository review — Adaptive JSON5

| Item | Value |
| --- | --- |
| Review date | 2026-11-05 – 2026-11-06 |
| Baseline | `64510bef5486bc5825ee6eaf3fb0079d8918fa04` (`main`, version 0.2.0) |
| Review branch | `claude/keen-edison-0aacft` |
| Document | `doc/review/2026-11-06-full-repository-review-0.2.0.md` (first drafted as `docs/reviews/2026-11-05-full-repository-review.md`); companion guide: [`how-to-run-a-full-repository-review.md`](how-to-run-a-full-repository-review.md) |
| Reviewer | Claude Code (AI agent) |
| Status | **Complete** — all plan steps 0–9 done; [follow-up re-check](#follow-up--re-check-of-newer-branches-2026-11-06) on 2026-11-06; [proposed GitHub issues](#proposed-github-issues-copy-ready) and [order of work](#order-of-work-scope-documents-haratara-versus-review-findings) |
| Re-checked refs | `origin/main` `903aedc` (= tag `0.2.0`), `72-release-packaging` `0b8957e`, `44-schema-1-…-p043p046` `f103fc2` |
| Compared with | Independent ChatGPT review `full-repository-review-2026-11-06.md` (its baseline `1455359` is older than `0.2.0`); see [comparison](#comparison-with-the-independent-chatgpt-review-2026-11-06) |

**Revision note (2026-11-06, publication check).** Before publishing, every `path:line`
reference was re-checked against `64510be`. Corrected: STD-05 callback-cast lines,
ARC-01 (`json5_document.cpp:122-127`), LOG-01 (`buffer_arena.cpp:46-64`), ARC-06
(`AdaptiveJSON5Config.cmake.in:73-74`) and DOC-18 (`.zed/README.md:38`). The follow-up
status was re-checked on 2026-11-06: `main` is still `903aedc` (tag `0.2.0`), and
`72-release-packaging` and `f103fc2` are still unmerged. The document moved to
`doc/review/` and a step-by-step guide for future reviews was added beside it.

**Revision note (2026-11-06, comparison).** Compared with an independent ChatGPT review
of the older baseline `1455359`. Every item in it was re-checked at `0.2.0`. Valid items
missing here were copied in: LOG-11, ARC-09, DOC-18, DOC-19, REPO-15, a standards
applicability table, BARR-C tailoring details, a CERT/MISRA edition note, a TARA row on
configuration authenticity, five evidence rows and new questions and improvements. Items
no longer valid at `0.2.0` were not copied; the
[comparison](#comparison-with-the-independent-chatgpt-review-2026-11-06) lists each
disposition. Totals are now 86 (5 High, 27 Medium, 40 Low, 14 Info).

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
| 2 | Logic review of production code | Done | [LOG-01 … LOG-11](#step-2-results--logic-findings) |
| 3 | Coding standards: MISRA C++:2023, AUTOSAR C++14, CERT C++, BARR-C:2018 and related automotive guidelines | Done | [STD-01 … STD-15](#step-3-results--coding-standards-findings-misra-autosar-cert-barr-c) |
| 4 | ISO 26262 (functional safety) | Done | [FS-01 … FS-10](#step-4-results--iso-26262-findings) |
| 5 | ISO/SAE 21434 (cybersecurity) | Done | [CS-01 … CS-07](#step-5-results--isosae-21434-findings) |
| 6 | Architecture | Done | [ARC-01 … ARC-09](#step-6-results--architecture-findings) |
| 7 | Documentation | Done | [DOC-01 … DOC-19](#step-7-results--documentation-findings) |
| 8 | Full repository (build, CI, tests, tools, licensing, housekeeping) | Done | [REPO-01 … REPO-15](#step-8-results--full-repository-findings) |
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
- **Line references.** Every `path:line` refers to the baseline `64510be`, whose
  production code is identical to tag `0.2.0`. Documentation files may have moved by a few
  lines on later commits; references to later refs say so explicitly.

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
| Each of the 38 public headers compiled alone (`-fsyntax-only`, C++17, public include root only, `-Wall -Wextra`) | OFF | OFF | **38 / 38 passed** (added 2026-11-06) |
| `python3 -m unittest discover -s tools/tests` | — | — | **4 / 4 passed** (added 2026-11-06) |
| Offline `tools/generate_unicode_tables.py` regeneration | — | — | **Byte-identical** to `core/includes/unicode/unicode_tables.hpp` (SHA-256 `2d7a18ca…f3ae1`) (added 2026-11-06) |
| `library` preset: Release build and install, tests OFF | OFF | OFF | Succeeded; installed CMake exports and headers reference neither GoogleTest nor the private include tree; `hosted/` header not installed by default (added 2026-11-06) |
| `tools/package_release.py --platform linux-x86_64-gcc --version 0.2.0` on tag `0.2.0` | OFF | OFF | **Fails** with "Installed public-header inventory differs from supported headers" (REPO-06); succeeds with the script from `72-release-packaging` (added 2026-11-06) |

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
| LOG-01 | **High** | `core/src/value/json5_value.cpp:303-318`; `core/src/memory/buffer_arena.cpp:46-64`; `core/src/value/floating_conversion.cpp:256-351` | `json5_value::as_double()` takes its big-integer scratch from the document's own storage. For a caller-buffer document (`json5_document::parse(text, buffer, size)`), `buffer_arena` only rewinds when *every* allocation is released, which never happens while the document is alive. Every `as_double()` call therefore consumes arena space permanently; `to_double` reserves two scratch integers for every finite non-zero value, including `8080` or `1.5`. | *Probe:* 4 KiB buffer, document `{ port: 8080, ratio: 1.5 }`: read #163 of `root().member("ratio").as_double()` fails with `allocation_failure` while `current_bytes()` (649) and `peak_bytes()` (761) are unchanged. A device that polls a configuration value will start failing after a bounded number of reads, and the documented sizing method (`peak_bytes()` + margin, `docs/memory-and-limits.md`) cannot detect it. | Make the arena reclaim LIFO releases (if `address + bytes == m_begin + m_used`, roll `m_used` back; the two scratch integers are already released in reverse order), or give `as_double` a bounded stack scratch / fast path for short significands, or take scratch from a separate caller-supplied provider. Add a unit test that reads `as_double()` in a loop on a small buffer and asserts no exhaustion. |
| LOG-02 | Medium | `includes/adaptive_json5/json5_value.hpp` (`as_double`, `const`); `includes/adaptive_json5/value/document.hpp:209` | `json5_value::as_double()` is a `const` member but mutates shared state of the owning document (accounting counters and the arena bump pointer) through the scratch callbacks. `document.hpp` promises that "const operations change no state, so several threads may read one document concurrently"; `json5_document`/`json5_value` state no thread-safety rule. | Two threads calling `as_double()` on values of the same `json5_document` race on `accounted_storage::m_current_bytes/m_peak_bytes` and `buffer_arena::m_used/m_live` (undefined behavior; possible corrupt accounting or overlapping scratch blocks). | Either make `as_double()` independent of document storage (see LOG-01) or document that a `json5_document` must not be read concurrently when `as_double()` is used. Add the thread-safety statement to `json5_document.hpp`. |
| LOG-03 | Medium | `core/src/parser/parser_engine.cpp:126-148, 684-698`; `core/src/memory/resource_budget.cpp` | Exceeding `max_depth`, `max_nodes`, `max_members` (and `max_input_bytes`) returns `resource_limit` **without a byte offset**, and no `resource_limit` error says *which* limit was exceeded. | *Probe:* `[[[[1]]]]` with `max_depth = 3`, `[1,2,3,4]` with `max_nodes = 3`, `{a:1,b:2}` with `max_members = 1` all yield `has_byte_offset = false`, message "resource limit exceeded". J5-044 asks for an offset on input diagnostics; `docs/memory-and-limits.md` tells users to "raise the specific limit", which they cannot identify. | Locate these failures at `m_source.offset()` (the token that would exceed the limit) and add an optional `resource_limit_reason` (input, depth, nodes, members, string, total string, number bytes, exponent digits, owned bytes, diagnostics, work) to `error`, mirroring `syntax_reason`/`numeric_reason`. |
| LOG-04 | Low | `core/src/lexer/number/non_finite_lexer.cpp:60-100`; `docs/memory-and-limits.md:214-215` | Unknown words in value position that start with `N` or `I` (`None`, `Nan`, `Index`) are routed to the non-finite lexer and reported at the first mismatching byte with no `source_range`. Other unknown words (`nul`, `trueish`) get a whole-word range. | *Probe:* `[None]` → offset 2, no range; `[nul]` → offset 1, range 1+3. Contradicts the documented contract "source_range covers the whole word". | On a non-finite spelling mismatch, fall back to `measure_identifier` and report `unknown_literal` with the whole-word range, as `consume_scalar` does. Add `None`/`Index`/`Nan` cases to the diagnostics units. |
| LOG-05 | Low | `core/src/value/json5_value.cpp:150-214` | `json5_value::member()` reports a missing member, a duplicated (ambiguous) member and a non-object receiver all as the same `wrong_type_or_access` without any sub-reason. The advanced `lookup_member` distinguishes `absent`/`unique`/`ambiguous`, but the simple API discards it. | *Probe:* `member("a")` on `{a:1, a:2}` and `member("zz")` both return code 4. J5-039 requires *explicit* reporting of absent member versus ambiguous duplicate versus wrong type. | Add an access reason (e.g. `access_reason::{wrong_type, absent_member, duplicate_member, index_out_of_range}`) to `error`, or document that callers must fall back to `values().lookup_member()` to tell them apart. |
| LOG-06 | Low | `core/src/parser/parser_work.cpp:85-100`; many `error::without_location(error_code::wrong_type_or_access)` sites in `core/src/parser/` | (a) Broken internal invariants (count/build pass disagreement, impossible states) are reported as `wrong_type_or_access` — the same code as caller misuse — so field reports cannot separate "library bug" from "bad call". (b) `limited_scan_error` converts **any unlocated** failure into `resource_limit` when the scan was cut by the work budget, so an `allocation_failure` from the warning-table reservation inside a cut scan is misreported. | Category confusion weakens J5-043 and the retry guidance (`allocation_failure` → grow pool vs `resource_limit` → raise a limit). | Introduce an `internal_error` code (or reason) for invariant violations, and restrict the near-cut reclassification to `syntax`/`encoding` errors. |
| LOG-07 | Low | `core/src/memory/accounted_storage.cpp:75-82` | `deallocate_callback` subtracts `bytes` from `m_current_bytes` without checking for underflow. A provider or caller error that releases a block twice or with a wrong size wraps the counter, after which every allocation fails with `resource_limit`. | Silent state corruption instead of a detectable fault; relevant for ISO 26262 defensive-programming expectations (see FS-05). | Saturate at zero and latch an error state (or assert in a debug configuration); add a unit for a mismatched release. |
| LOG-08 | Low | `core/src/memory/owned_block.cpp:126-134` | A non-power-of-two or zero alignment is reported as `resource_limit`. It is an invalid argument, not an exhausted limit. | Documented in the header, but inconsistent with the error-code table in `docs/memory-and-limits.md` (`wrong_type_or_access` = invalid argument). | Report `wrong_type_or_access` for invalid alignment. |
| LOG-09 | Info | `core/src/lexer/whitespace.cpp`; `core/src/lexer/identifier/identifier_lexer.cpp` | Each whitespace character is UTF-8 decoded twice (line-terminator check, then classification); each string and identifier is walked three to four times (count-pass measure, build-pass measure, build-pass decode, optional warning notification). All walks are charged to the work budget, so this is bounded, but it is the dominant CPU cost on small targets. | Performance only; no correctness impact. | Consider a single-decode whitespace loop and reusing the count-pass measure in the build pass (see Further improvements). |
| LOG-10 | Info | `docs/conformance.md:135` | JSON5 whitespace accepts the union of Unicode 17.0 `Zs` and Unicode 3.0 `Zs` (e.g. U+200B, which is `Cf` today). This is a documented, deliberate choice. | Interoperability: documents accepted here may be rejected by parsers using only current Unicode data. | Keep documented; consider exposing it in the support statement's deviations table rather than only in the choices table. |
| LOG-11 | Medium | `core/src/value/json5_value.cpp:176-178` (`member()`); `core/src/value/value_view.cpp:291-307` (`element_at`/`member_at`); `core/src/value/member_lookup.cpp:35`; `decoded_name_equal.cpp`; `utf8_export.cpp` | *Added 2026-11-06 from the comparison (ChatGPT G07, sharpened).* `json5_value::member()` loops over `member_at(position)`, and each `member_at` walks the sibling list from the first member, so one lookup costs O(n²) record steps for an object with n members; it always scans every member to detect duplicates. `at(i)` costs O(i). No post-parse operation (lookup, name comparison, UTF-8 export) takes a work budget or cancellation context (`member_lookup.cpp:35`: "P031 adds configurable work accounting"), so the parse-time work budget does not bound later reads. | *Probe:* object with 1024 members (the default `max_members`), library built `-O2`: about **2.5 ms per `member()` call** on an x86-64 host, whichever key is requested (about 27 ms with the Debug library). A configuration within default limits can therefore make every lookup expensive on small targets. | Walk the sibling chain once in `member()` (carry the current `value_view` instead of calling `member_at(position)`), making lookup O(n); document per-operation costs; decide whether post-parse operations need their own work budget. |

## Step 3 results — Coding standards findings (MISRA, AUTOSAR, CERT, BARR-C)

**Basis and limits.** `core/docs/design-coding-style.md` selects MISRA C++:2023 and
SEI CERT C++ (2016 edition) as the coding baseline and requires that rule numbers be
checked against the authoritative text. The licensed MISRA and AUTOSAR texts were not
available to the reviewer, so every rule number below is a **candidate mapping to be
confirmed** against the licensed text before it is recorded as a deviation. No
compliance claim is made or implied.

Record the exact edition and any published technical corrigenda of MISRA C++:2023, and the
CERT C++ errata, used for an assessment, so that it can be reproduced (added from the
comparison).

**Applicability of automotive standards** (added from the comparison). "All automotive
standards" is not a finite checklist without a product, customer contract, target and
safety role. For this generic library:

| Standard / framework | Applicability to this repository | Where covered |
| --- | --- | --- |
| MISRA C++:2023 (C++17) | Primary coding baseline selected by the project | This step |
| SEI CERT C++ (2016 + errata) | Selected by the project as secondary baseline | This step (CERT paragraph) |
| AUTOSAR C++14 | Targets C++14; supplementary only, unless a customer contract requires a separate assessment | STD-13 |
| BARR-C:2018 | C99 standard; usable by analogy only after explicit tailoring | BARR-C paragraphs below |
| HIS source-code metrics | Common OEM/supplier metric set; used for complexity evidence | STD-07 |
| ISO 26262:2018 | Applies only for safety-related use; reviewed as a gap analysis | Step 4 |
| ISO/SAE 21434:2021 | Applies to cybersecurity engineering of the consuming item; reviewed as a gap analysis | Step 5 |
| Automotive SPICE | Process capability model; cannot be rated from a repository snapshot; the contractual model version must be chosen first | Not assessed |
| UN R155 / R156, ISO 24089 | Vehicle cybersecurity and software-update management; relevant only when the library is integrated into a vehicle product; no update management is supplied here | Not applicable to the library alone |
| ISO 21448 (SOTIF) | Concerns intended driving functionality; applies to an integrating function, not to a generic parser | Not applicable |
| IATF 16949 / ISO 9001 | Organizational quality management; no conclusion possible from source review | Not assessed |
| IEEE 1003.13 PSE51/PSE52 | Conditional platform profile already acknowledged in the coding-style guide; the core stays OS-agnostic | Not assessed |
| AUTOSAR APIs/manifests, hardware/EMC/network standards | Apply only to selected adapters or products; the core is not an `ara::*` runtime or manifest validator | Not applicable |

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
| STD-05 | Medium | Callback trampolines: `core/src/memory/accounted_storage.cpp:24,76`, `buffer_arena.cpp:20,49`, `core/src/parser/parser_engine.cpp:580,624`, `buffer_document_storage.cpp:50`, `core/src/hosted/parse.cpp:68` | `void* context` is converted back to an object pointer with `static_cast<T*>(context)`; the C-style callback API (`storage_callbacks`, `string_warning_sink`) depends on it. | MISRA C++:2023 Rule 8.2.6; AUTOSAR M5-2-8 | Record as a deviation for the callback ABI, or move internal callers to a typed interface (template or small abstract provider) and keep `void*` only at the public C-compatible boundary. |
| STD-06 | Info | Whole production code (211 / 383 functions); `core/docs/design-coding-style.md` | Early returns on failure (`if (!x.has_value()) return failure(...)`) are the dominant style. The owner confirmed (2026-11-05) that the project follows **MISRA with a fail-fast principle**, so this is intentional. MISRA C++:2023 and AUTOSAR C++14 contain no single-exit rule, so it is **not a MISRA deviation**. Residual gap: the fail-fast principle is not written in the coding-style guide (a repository-wide search finds no mention). | Conforms to MISRA C++:2023. It departs only from the *older/stricter* single-exit rules — MISRA C++:2008 6-6-5, JSF AV 113, BARR-C:2018 §6.2 (a "preferred practice") — and from ISO 26262-6 Table 6 item 1a (see FS-04). | Record the fail-fast principle in `design-coding-style.md` (what may return early, that every early return passes the error through unchanged, that resources are released by RAII owners). The same text then serves as the ISO 26262 Table 6 1a rationale. <br>**Update 2026-11-06:** documented in `design-coding-style.md` ("Control flow and error handling") on branch `44-schema-1-…-p043p046` (`f103fc2`), **not yet merged**. Accuracy remarks on that text are in DOC-17. |
| STD-07 | Medium | Functions listed in the metrics row above | 46 functions exceed cyclomatic complexity 10 and 10 exceed 100 lines; `convert_to_floating` has CCN 54 and 239 lines. (Its 31 `return` statements are fail-fast exits and are not counted as a problem here; see STD-06.) | HIS metrics (v(G) ≤ 10, ≤ 50 statements; the HIS "one return" metric is not applied under the fail-fast principle); BARR-C:2018 §6.2 (≈100 lines); ISO 26262-6 Table 6 item 1h/“restricted size and complexity” | Split the largest functions along their natural phases (e.g. `convert_to_floating`: analysis → scratch sizing → load → scale → divide → round), and record accepted thresholds in the coding-style guide. |
| STD-08 | Low | `includes/adaptive_json5/error.hpp:60-62`; `string_warning.hpp:32-33`; private token/span structs | Several aggregates (`error`, `string_warning`, `decoded_character`, span structs, `unicode_range`) have members without default initializers; `error e;` leaves `code`, `byte_offset` and `has_byte_offset` indeterminate. Three locals are declared without initializer (`punctuation.cpp:32`, `string_boundary.cpp:34`, `string_warning.cpp:133`). | MISRA C++:2023 (initialization rules, Rule 11.6.x); AUTOSAR A8-5-0; CERT EXP53-CPP | Add default member initializers to public aggregates (e.g. `error_code code{error_code::syntax}` or a value-initialized default) and initialize the three locals. |
| STD-09 | Low | `includes/adaptive_json5/*.hpp` (22 of 38 public headers), most private headers | Public headers use unprefixed include guards such as `INPUT_HPP_`, `OPTIONS_HPP_`, `PARSER_HPP_`, `DOCUMENT_HPP_`, `STORAGE_HPP_`; 16 public headers use `ADAPTIVE_JSON5_*`. A consumer header with the same common guard silently suppresses the library header (or vice versa). | BARR-C:2018 §4.2 (header guards); MISRA C++:2023 Rule 19.2.1 (guards present – complied) | Prefix every installed header guard with `ADAPTIVE_JSON5_` and the path (the coding-style guide currently prescribes the bare filename). <br>**Reproduced 2026-11-06 (comparison):** a translation unit that includes `adaptive_json5/adaptive_json5.hpp` compiles normally but fails with `-DOPTIONS_HPP_` (`'syntax_mode' has not been declared` in `owned_number.hpp`). |
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

**BARR-C tailoring** (added from the comparison). BARR-C:2018 targets C99, so it cannot be
applied literally: its 80-column limit (§1.2) differs from the project's 120 columns, its
four-space indentation (§3.4) from the project's two spaces (`.clang-format` `IndentWidth: 2`),
and its C99 language choice (§1.1) and restriction of the `auto` keyword (§1.7) do not transfer
to C++17 type deduction. Its brace rules (§1.3) are met. Record the selected subset and the
precedence over MISRA C++:2023 before claiming BARR-C conformance.

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
| Availability of the consuming service/ECU | Deep nesting, huge strings/numbers, many members, pathological duplicates, long comments | Configurable limits with conservative defaults, work budget charged per scanned byte, quadratic duplicate check bounded by the work budget, scratch bound for numeric conversion | **LOG-01** lets attacker-chosen numbers exhaust a caller buffer during later reads (CS-03); no time bound per work unit (FS-06) <br>*Added 2026-11-06:* LOG-11 — every `member()` lookup is O(n²) in the object's member count and unbudgeted. |
| Integrity of configuration semantics | Duplicate-key or Unicode confusion makes this parser and another component interpret the same file differently | Every duplicate is preserved and the simple API refuses ambiguous lookups; exact UTF-16 equality without normalization; lone surrogates preserved and rejected on UTF-8 export | Iteration-based consumers may still pick the first or last duplicate; embedded NUL survives `as_string` (CS-05) |
| Authenticity and authorization of configuration | An attacker replaces or edits a configuration file or a document received over a channel | None in the parser, by design: accepting the syntax does not authenticate the content (added from the comparison) | Integrator responsibility: authenticate the source (signature, secure storage, access control) before parsing; state it as an assumption of use (CS-01) |
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
| ARC-01 | Medium | `core/src/value/json5_value.cpp`, `core/src/memory/json5_document.cpp:122-127` | The simple API couples *reading* to the document's *allocator*: `json5_document::root()` hands every `json5_value` the document's accounted callbacks as scratch. This is the root cause of LOG-01 (arena exhaustion) and LOG-02 (hidden mutation behind `const`). | A read-only view type has write side effects on shared state. | Make value reads allocation-free (bounded stack scratch or a fast path for doubles with ≤ 19 significant digits and small exponents, falling back to an explicit caller-supplied scratch only for long spellings), or give `json5_document` a separate, reclaimable scratch region. |
| ARC-02 | Medium | `core/src/parser/parser_engine.cpp` (`count_*` vs `read_*`/`consume_*` methods) | The two-pass design (ADR 0003 decision 7) duplicates the grammar state machine: `count_value`/`count_name`/`count_separator`/`count_close` mirror `read_value`/`read_name`/`consume_separator`/`finish_container`. Agreement is checked only at run time ("passes disagree" → internal error). | Every grammar or limit change must be made twice; a divergence becomes a field failure rather than a compile error. | Drive both passes from one templated walker with a counting policy and a building policy, so the grammar exists once. |
| ARC-03 | Medium | `includes/adaptive_json5/value/document.hpp`, `memory/owned_block.hpp`, `memory/document_storage_owner.hpp` | The public surface (38 headers) exposes construction internals: document mutators (`make_*`, `append_*`, `set_source_span`, `set_root`), `owned_block`/`allocate_block`, `detail::value_storage`, `detail::document_storage_owner`. A consumer can mutate a parsed `parsed_document::values` (for example append to an exact-reserved document, which then fails with the internal-error code). | Large API/ABI commitment for 0.x; invariants of parsed documents are not protected by the type system. | Split a read-only `document` view from a `document_builder`, move `owned_block`/`document_storage_owner` under an explicitly unsupported `detail/` folder, and document which headers are stable. |
| ARC-04 | Low | `includes/adaptive_json5/error.hpp` | `error` carries four `std::optional` members (duplicate spans, numeric reason, source range, syntax reason) → 104 bytes on x86-64, embedded inline in every `result<T>`. LOG-03 and LOG-05 would add more reasons. | Stack and copy cost on small targets; the structure grows with every new reason. | Use one compact reason field (category-specific enum stored in a small integer) plus at most one span pair, keeping duplicate locations as the only two-span case. |
| ARC-05 | Low | `includes/adaptive_json5/memory/storage.hpp` | `storage_callbacks` has two allocation entry points (`allocate` returning `nullptr`, `allocate_result` returning `result<void*>`) with precedence rules. | Two paths to test and document for every provider. | Keep one (the `result` form) in the next breaking release, with an adapter for pointer-returning allocators. |
| ARC-06 | Low | `cmake/AdaptiveJSON5Config.cmake.in:73-74` | The package config sets `AdaptiveJSON5_parser_FOUND` and `AdaptiveJSON5_build_options_FOUND` but not `AdaptiveJSON5_hosted_FOUND`, so `find_package(AdaptiveJSON5 CONFIG REQUIRED COMPONENTS hosted)` fails even when the hosted library is installed; consumers must use `if(TARGET AdaptiveJSON5::hosted)`. | Unidiomatic component discovery. | Set `AdaptiveJSON5_hosted_FOUND` when the target exists. |
| ARC-07 | Low | `CMakeLists.txt:121` | Package compatibility is `SameMinorVersion`, but there is no ABI policy (no inline versioned namespace, no statement on which build options change the ABI beyond the recorded exceptions/RTTI values). | Mixing two 0.2.x builds with different options is not detected. | Document the ABI policy; consider an inline namespace (e.g. `adaptive_json5::v0_2`) once the API stabilizes. |
| ARC-08 | Info | `CMakeLists.txt:12-15`, `cli/README.md`, `schema/README.md`, `editor/README.md` | `ADAPTIVE_JSON5_BUILD_SCHEMA` fails configuration ("not available yet") and `ADAPTIVE_JSON5_BUILD_EXECUTABLE` builds the example programs, not a CLI. The optional components are placeholders. | Option names promise more than they deliver. | Rename `ADAPTIVE_JSON5_BUILD_EXECUTABLE` to `ADAPTIVE_JSON5_BUILD_EXAMPLES` (keep the old name as an alias) until the CLI exists. |
| ARC-09 | Info | `includes/adaptive_json5/value/value_index.hpp`; `core/src/value/value_storage.cpp:249-280` | *Added 2026-11-06 from the comparison.* `value_index` carries no document identity. An index from document A used with document B is bounds-checked (no memory error) but silently addresses an unrelated record. Document, view and accounting lifetimes are caller preconditions. | Misuse yields wrong data instead of an error. | Keep it as a documented precondition, or add an owner tag (for example a per-document serial) to indices and views if misuse is observed. |

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
| DOC-03 | Medium | `docs/embedded.md:22-23`; J5-101 | The embedded page says to "record static initialization, alignment, byte order, integer-size assumptions, stack/work storage and actual compiler/device limitations", but records none of them (no stack figure, no static-initialization statement, no data-model assumptions). | Fill in the figures (stack per entry point, no static constructors, required C++ runtime subset, assumed `CHAR_BIT == 8`, two's-complement, `std::uintptr_t` availability, alignment needs). <br>*Added from the comparison:* the facilities to list include `std::variant`, `std::optional`, `<type_traits>`, `std::memcpy`, placement new and `std::launder`, plus the octet/character-representation assumptions of the UTF-8 code. |
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
| DOC-18 | Low | `.agents/skills/guide-requirement/SKILL.md:180`; `.zed/README.md:38` | *Added 2026-11-06 from the comparison (residue of ChatGPT F04).* The requirement-guidance skill still calls the complete-document entry "future", although `parse_document()` exists, and the Zed guide says "currently 98 cases" while CTest registers 2514. The other stale status statements ChatGPT listed no longer exist at `0.2.0` (those files were rewritten or removed); DOC-12 is the remaining architecture-diagram case. | Update both statements; avoid hard-coded test totals in guides, or date them. |
| DOC-19 | Low | `docs/requirements.md:83` (J5-017) vs `docs/memory-and-limits.md:217-219` and `includes/adaptive_json5/memory/resource_limits.hpp` (`max_diagnostics`) | *Added 2026-11-06 from the comparison (ChatGPT F05).* J5-017 says warnings "shall not turn valid parsing into failure", but a valid document whose warnings exceed `max_diagnostics` fails with `resource_limit`. *Probe:* with `max_diagnostics = 0`, a single raw U+2028 inside a string fails the parse, while the same input succeeds with the default limit and one warning. The user guide documents this behavior; the requirement contradicts it. | Decide the precedence (operational limit before advisory success, or succeed with a count but no descriptor) and align J5-017, the API note and the units. |

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
| REPO-06 | Low | `tools/tests/test_package_release.py`; `tools/generate_unicode_tables.py` | The packaging script's Python tests are not run by any workflow, and no job checks that `core/includes/unicode/unicode_tables.hpp` regenerates byte-identically from `tools/unicode-data/`. | Add a lightweight `tools` job (pure Python, no new C++ gate) or document both as manual release steps. <br>**Update 2026-11-06:** confirmed by the release itself — the first 0.2.0 packaging run failed its header inventory check because `tools/package_release.py` expected the optional `hosted/` header; the fix is on `72-release-packaging` (`6caab0d`, unmerged). Running the packaging tests/script on pull requests would have caught it before the tag. <br>**Update 2026-11-06 (comparison):** reproduced locally — packaging tag `0.2.0` with the tool from the release commit fails the header inventory check; the Python units (4/4) pass and the Unicode header regenerates byte-identically, so both checks are cheap to add to CI. |
| REPO-07 | Low | `tests/value/numeric_locale_tests.cpp:91,163` | The locale-independence unit (J5-052) is skipped on every Linux runner because no comma-decimal locale is installed; it executes only on the Windows job. | Install a comma-decimal locale (e.g. `de_DE.UTF-8`) in the Linux jobs. |
| REPO-08 | Low | `.gitattributes` | Only `tests/fixtures/third_party/** -text` is declared; there is no `* text=auto eol=lf` normalization although development happens on Windows and Linux. | Add text normalization to avoid CRLF churn and formatter differences. |
| REPO-09 | Low | 171 of 172 production C++ files | No per-file copyright/SPDX header (only one file has `SPDX-License-Identifier`); tests have none. Apache-2.0 does not require it, but automotive supplier audits and SBOM tooling commonly expect it. | Add `// SPDX-License-Identifier: Apache-2.0` and a copyright line to each source file. |
| REPO-10 | Low | `.github/pull_request_template.md` | The validation checklist names unit tests (exceptions OFF/RTTI OFF) and formatting, but not the validation-test gate or the exceptions-ON leg that CI requires. | Align the checklist with the required checks. |
| REPO-11 | Info | `CMakeLists.txt:122-135` | Inconsistent indentation inside the `install(...)`/hosted `if` block. | Reformat (cmake-format or the `.neocmakelint.toml` rules). |
| REPO-12 | Info | `core/docs/requirements-issue-backlog-before-consolidation-2026-10-13.json` (158 kB), `core/docs/requirements-issue-backlog-before-p-sequence.json` (71 kB) | Historical backlog snapshots live beside the current backlog and implementation-order JSON. | Move snapshots to an `archive/` folder or rely on git history. |
| REPO-13 | Info | `.zed/user-config/AGENTS.md`, `.zed/user-config/settings.json` | Personal editor configuration (including a commit-message prompt) is committed. | Keep project-wide editor settings only, or document why user-level settings are shared. |
| REPO-14 | Info | Branch `44-schema-1-component-owned-references-exact-equality-and-bounded-evaluation-p043p046` | *Added 2026-11-06.* The coding-style decision (a docs-only change) is the only commit on the schema P043–P046 feature branch, which is based on `64510be` and one commit behind `main`; the commit subject has a typo ("conding"). Merging it with the schema work ties a project-wide convention to an unrelated feature review. | Land the coding-style change in its own small documentation PR (and fix the subject), then rebase the schema branch on `main`. |
| REPO-15 | Medium | `cmake/ImportAdaptiveJSON5.cmake:14-17, 94-101` | *Added 2026-11-06 from the comparison (ChatGPT F01), reproduced at `0.2.0`.* The import helper reduces the requested tag to its numeric version and checks only `find_package(... EXACT)` and the profile; it never compares the package's `build-info.json` `release_tag`, and the tag is not exported as a package variable. *Probe:* a locally built `0.2.0` package imported through `ROOT` was accepted when `VERSION 0.2.0-rc.9` and `VERSION v0.2.0-beta.1` were requested (exit 0, `AdaptiveJSON5_VERSION=0.2.0`). The download/`ARCHIVE` path is protected by the consumer-pinned SHA-256 and the archive folder name. | Compare the exact release tag on the `ROOT` and reused-cache paths (decide whether a `v` prefix is an alias; keep pre-release identity distinct), export it as `AdaptiveJSON5_RELEASE_TAG`, and add matching and mismatching cases to the tool tests. Relevant to FS-09 (configuration identity). |

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
| Step 2 — Logic (LOG) | 1 | 3 | 5 | 2 | 11 |
| Step 3 — Coding standards (STD) | 0 | 6 | 6 | 3 | 15 |
| Step 4 — ISO 26262 (FS) | 3 | 6 | 1 | 0 | 10 |
| Step 5 — ISO/SAE 21434 (CS) | 1 | 3 | 2 | 1 | 7 |
| Step 6 — Architecture (ARC) | 0 | 3 | 4 | 2 | 9 |
| Step 7 — Documentation (DOC) | 0 | 3 | 14 | 2 | 19 |
| Step 8 — Full repository (REPO) | 0 | 3 | 8 | 4 | 15 |
| **Total** | **5** | **27** | **40** | **14** | **86** |

**Prioritized recommendations** (for their sequence relative to the safety and security
scope documents, see [order of work](#order-of-work-scope-documents-haratara-versus-review-findings))

1. **Fix LOG-01/LOG-02 and LOG-11** (with ARC-01, CS-03, DOC-01, DOC-02): walk the member list once in `member()` instead of quadratically; make `as_double()`
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

Questions added from the comparison with the ChatGPT review (2026-11-06):

12. Which ECU/CPU, compiler/runtime and data models are the intended targets, and is
    parsing limited to start-up or allowed inside a real-time task? (This decides how much
    LOG-11 and FS-06 matter.)
13. Which exact MISRA edition and corrigenda, CERT errata and OEM/customer coding rules are
    contractual? Is a tailored BARR-C subset or a separate AUTOSAR C++14 assessment required?
14. Are safety plans or manuals, a TARA, cybersecurity cases, tool-confidence records or a
    disclosure procedure maintained outside this repository?
15. With zero diagnostic slots, should a valid document with a warning fail with
    `resource_limit` or succeed with only a warning count (DOC-19)?
16. Should `v0.2.0` and `0.2.0` be aliases for package identity, while pre-release
    identifiers stay distinct (REPO-15)?
17. Should consumers have one work allowance across parsing and later lookup, traversal and
    export, or a separate work/cancellation context per operation (LOG-11)?
18. Do the live GitHub required checks include the Linux aarch64 and Windows MinGW unit jobs?
    The versioned ruleset was removed in `64510be`, so only the repository settings can show it.
19. Should cross-document indices and document/accounting lifetimes remain caller
    preconditions, or should owner-bound handles enforce them (ARC-09)?

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
- **One component-status table** keyed by P step, with separate implementation, named-unit
  and executed-evidence columns, linked from the architecture page, the implementation order
  and the skills, so status text cannot drift again (DOC-12, DOC-18; from the comparison).
- **Sizing examples for limits:** publish small representative documents with the limit each
  one exhausts, so users can size `max_work_units` and `max_diagnostics` (from the comparison).
- **Package identity end to end:** keep tag, source revision, toolchain and packaging identity
  together and test matching and mismatching imports (REPO-15; from the comparison).
- **Owner-bound handles** for indices and views, if misuse is observed (ARC-09).
- **Authenticated release manifests or signatures** where the consumer's threat model needs
  them; a checksum fetched from the same channel proves integrity, not authenticity (CS-04).

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

---

## Comparison with the independent ChatGPT review (2026-11-06)

**Source.** `full-repository-review-2026-11-06.md`, supplied by the owner. Its baseline is
`1455359` ("Fix release tags without v and add safe existing-release recovery (#92)"), which
is older than this review's `64510be` / tag `0.2.0`: since then the source tree was
restructured (`src/` → `core/`), several documents were rewritten or removed, the ruleset was
deleted, and the suite grew from 1,710 to 2,514 unit cases. Every ChatGPT item was therefore
re-checked against `0.2.0` before anything was copied; nothing was taken over on trust.

| ChatGPT item | Re-check at `0.2.0` | Disposition here |
| --- | --- | --- |
| F01 — import helper ignores exact pre-release identity | **Valid**, reproduced with a locally built `0.2.0` package (`0.2.0-rc.9` and `v0.2.0-beta.1` accepted via `ROOT`) | Added as **REPO-15** (Medium) |
| F02 — CI actions selected through mutable tags | Valid | Already covered by REPO-05 and CS-04 |
| F03 — generic public include guards | Valid, reproduced (`-DOPTIONS_HPP_` breaks the umbrella header) | Already STD-09; reproduction evidence added |
| F04 — status documents contradict the implemented code | Mostly obsolete: `docs/roadmap.md`, `docs/current-architecture.md`, `docs/architecture.md` no longer exist and `docs/requirements.md`/`traceability.md` now describe the implemented parser. Still valid: the skill's "future complete-document entry" and the Zed guide's "98 cases" | Residue added as **DOC-18** (Low); DOC-12 already covers the architecture diagram |
| F05 — warning success versus diagnostic budget | **Valid**, reproduced (`max_diagnostics = 0` + raw U+2028 → `resource_limit`); its remark that only the first warning is retained is obsolete | Added as **DOC-19** (Low) and question 15 |
| F06 — stored ruleset omits portability checks | Obsolete: `.github/rulesets/main.json` and `docs/branch-protection.md` were removed in `64510be` | Not copied as a finding; the live-settings concern became question 18 |
| F07 — tracked `__cmake_systeminformation/` | Obsolete: no longer tracked (ignored in `.gitignore`) | Not copied |
| G01 — coding-standard evidence incomplete | Valid | Already FS-10 and the STD basis; edition/corrigenda note added |
| G02 — safety role and assumptions | Valid | Already FS-01 |
| G03 — traceability is not safety verification | Valid | Already FS-02 |
| G04 — target, timing, interference, tool confidence | Valid | Already FS-06 and FS-08 |
| G05 — no TARA or cybersecurity lifecycle | Valid; its point that syntax acceptance does not authenticate configuration was missing | Already CS-01; **authenticity row added to the TARA** |
| G06 — no vulnerability intake | Valid | Already CS-01 and CS-06 |
| G07 — post-parse operations not budgeted | **Valid**, and sharper than stated: `json5_value::member()` itself is quadratic (about 2.5 ms per lookup on a 1024-member object, `-O2`) | Added as **LOG-11** (Medium) and question 17 |
| G08 — embedded runtime/data-model contract | Valid | Already DOC-03; facility list added |
| Logic observation — 301/401-byte work-budget example | Obsolete: both documents parse with default limits at `0.2.0` (work accounting changed with ADR 0003) | Not copied; the sizing-examples idea kept as an improvement |
| Logic observation — document indices lack owner identity | Valid | Added as **ARC-09** (Info) and question 19 |
| Standards applicability (ASPICE, UN R155/R156, ISO 24089, SOTIF, IATF 16949, PSE51/52, AUTOSAR APIs) | Valid and missing | Applicability table added to Step 3 (without unverified edition claims) |
| BARR-C tailoring (80 columns, 4-space indent, C99, `auto`) | Valid against `.clang-format` (`ColumnLimit: 120`, `IndentWidth: 2`) | BARR-C tailoring paragraph added to Step 3 |
| Evidence: standalone public headers, Python units, Unicode regeneration, tests-off install | Re-executed at `0.2.0`: 38/38 headers, 4/4 Python units, byte-identical tables, clean install | Five evidence rows added to Step 1 (plus the packaging reproduction) |
| Non-blocking questions 1–10 | Q1/Q4 overlapped existing questions; the rest were valid | Added as questions 12–19 |
| Further improvements 1–10 | Most overlapped existing items; five were new and valid | Added to the improvements list |

**What this review has that the ChatGPT review does not** (for context; not changed by the
comparison): the High defect LOG-01 (`as_double()` exhausts a caller buffer) and the related
data race LOG-02; the limit-error location/reason gap LOG-03; the warning-flag finding STD-01
with an extended-warning sweep; MISRA construct mapping STD-02 … STD-05; complexity metrics
STD-07; Release-versus-Debug test configuration FS-07 with measured coverage; and executed
sanitizer and fuzzing evidence (ChatGPT ran neither). ChatGPT's conclusion that no P0/P1
defect exists did not cover the simple `json5_document` API, which did not exist at its
baseline.

---

## Proposed GitHub issues (copy-ready)

**How this section is organized.** The 86 findings are grouped into 16 proposed issues
(I-01 … I-16). Every finding maps to exactly one issue (table below). The issues are
written for #117 ("M7 — Independent three-model full repository re-review and
remediation"), which asks to "create remediation issues, owner fixes production C++, AI
tests only"; add each one as a sub-issue of #117. The repository's issue workflow allows
at most three active issues per subsystem, so the issues are split into waves: open
**Wave 1** now and the others when a slot frees up. Bodies follow the repository's
`Bug report` and `Implementation task` templates, plus a findings list and a
responsibilities section (`AGENTS.md`: Romanr92 writes production C++ and public headers;
AI may change unit tests, CMake, CI and documentation).

**Created on GitHub on 2026-11-06** at the owner's request: #124 … #139 in milestone
"M4 — JSON Schema Draft-07 and CLI", titles prefixed with their proposal ID
(`[Bug] I-01 — …`). I-01, I-02, I-06 and I-07 are assigned to Romanr92; the others are
unassigned and not yet linked as sub-issues of #117. Labels are from the repository's
existing set (`bug`, `enhancement`, `documentation`, `component:core`,
`requirements:draft`).

### Order of work: scope documents (HARA/TARA) versus review findings

**Short answer:** decide and document the safety and security scope *before* working
through most review findings, but fix the confirmed bugs now.

**Why there is no HARA or item TARA in this repository.** A HARA (and the item-level TARA)
is performed for a vehicle *item*, a function with hazards; a JSON5 parser has none on its
own. The integrator's HARA produces the safety goals and the ASIL, and the integrator's TARA
produces the cybersecurity goals. The library instead documents itself out of context:

- **ISO 26262:** Safety Element out of Context with an *assumed* ASIL (or QM only),
  assumptions of use (input trust, one thread per document, caller storage, timing),
  assumed safety requirements and a safety manual (I-15).
- **ISO/SAE 21434:** out-of-context cybersecurity assumptions, a component-level threat
  analysis (the review's TARA-lite) and an assumed assurance level (CAL) (I-16). Accepting
  the syntax does not authenticate a configuration; the integrator has to do that.

**Order:**

1. **Now — independent of any scope:** fix the confirmed defects and test gaps: I-01
   (`as_double()` buffer exhaustion, data race, quadratic `member()`), I-06 (import helper
   ignores the release tag), I-07 (no Release-configuration test leg) and I-02 (missing
   error reasons and locations). They are wrong behavior under any scope.
2. **Next — decide the scope** (one owner decision; review questions 1 and 2): QM only, or a
   Safety Element out of Context with an assumed ASIL, and the assumed trust level of the
   input.
3. **Then — write the scope documents before re-triaging the remaining findings:** I-15
   (safety assumptions) and I-16 (security assumptions). They decide whether MC/DC, the
   MISRA deviation register, tool confidence and fuzzing become mandatory, or whether those
   findings can be closed as not applicable.
4. **Finally — re-triage and re-review:** re-triage I-03, I-04, I-05 and I-08 … I-14 against
   the documented scope, freeze the commit and run the #117 re-review against it.

**If a real vehicle project already has a HARA/TARA,** use its ASIL/CAL and the requirements
it allocates to the library instead of assumptions. The library then becomes a software
component to be qualified for that project (ISO 26262-8 clause 12).

### Overview

| Issue | GitHub | Title | Highest severity | Subsystem | Wave | Related existing issues |
| --- | --- | --- | --- | --- | --- | --- |
| I-01 | #124 (assigned) | Simple API reads exhaust caller buffers, race, and lookups are quadratic | **High** | core | 1 | #117, #69 |
| I-02 | #125 (assigned) | Error model: locate and explain limit, access and internal failures | Medium | core | 1 | #117, #69 |
| I-03 | #126 | Enable extended compiler warnings and fix mechanical coding-rule findings | Medium | core | 1 | #117 |
| I-04 | #127 | Reduce complexity hot spots and generate both parser passes from one walker | Medium | core | 2 | #117 |
| I-05 | #128 | Settle the public API surface before 1.0 | Medium | core | 2 | #117 |
| I-06 | #129 (assigned) | Package import: check the exact release tag and expose the hosted component | Medium | build | 1 | #117 |
| I-07 | #130 (assigned) | CI: test the shipped Release configuration and run gates on main | **High** | CI | 1 | #117 |
| I-08 | #131 | Supply-chain hardening for workflows and release artifacts | Medium | CI | 1 | #117 |
| I-09 | #132 | Optional sanitizer and fuzz workflow (needs scope decision) | Medium | CI | 2 | #117, #116 |
| I-10 | #133 | Build and repository hygiene | Low | build | 2 | #117 |
| I-11 | #134 | Documentation corrections from the full review | Medium | docs | 1 | #117 |
| I-12 | #135 | Release notes, known anomalies and release baselines | Medium | docs | 1 | #117 |
| I-13 | #136 | Embedded integration figures: stack, runtime facilities, data model, timing | Medium | docs | 1 | #117, #115, #69 |
| I-14 | #137 | Coding-standard governance: fail-fast text, deviation register, tailoring | Medium | process | 1 | #117, #44 |
| I-15 | #138 | ISO 26262 readiness (SEooC scope, requirements baseline, analyses, tools) | **High** | process | 1 | #117 |
| I-16 | #139 | ISO/SAE 21434 readiness (security policy, TARA, guidance) | **High** | process | 1 | #117 |

### Finding-to-issue map

| Issue | Findings |
| --- | --- |
| I-01 | LOG-01, LOG-02, LOG-11, ARC-01, CS-03, DOC-01, DOC-02 |
| I-02 | LOG-03, LOG-04, LOG-05, LOG-06, LOG-07, LOG-08, ARC-04, FS-05, DOC-05, DOC-06, DOC-19 |
| I-03 | STD-01, STD-04, STD-08, STD-09, STD-10, STD-11, STD-12, STD-14, STD-15 |
| I-04 | LOG-09, STD-07, ARC-02 |
| I-05 | ARC-03, ARC-05, ARC-07, ARC-09 |
| I-06 | REPO-15, ARC-06 |
| I-07 | FS-07, REPO-01, REPO-02, REPO-06, REPO-07 |
| I-08 | CS-04, REPO-05 |
| I-09 | CS-02, REPO-03 |
| I-10 | ARC-08, REPO-04, REPO-08, REPO-09, REPO-10, REPO-11, REPO-12, REPO-13 |
| I-11 | LOG-10, DOC-04, DOC-07, DOC-08, DOC-09, DOC-10, DOC-11, DOC-12, DOC-14, DOC-15, DOC-18 |
| I-12 | FS-09, DOC-13, DOC-16 |
| I-13 | FS-06, DOC-03 |
| I-14 | STD-02, STD-03, STD-05, STD-06, STD-13, FS-04, FS-10, DOC-17, REPO-14 |
| I-15 | FS-01, FS-02, FS-03, FS-08 |
| I-16 | CS-01, CS-05, CS-06, CS-07 |

---

### I-01 — Simple API reads exhaust caller buffers, race, and lookups are quadratic

**Title:** `[Bug] I-01 — json5_value::as_double() exhausts caller buffers; member() is quadratic` · **GitHub:** #124 (assigned to Romanr92)
**Labels:** `bug`, `component:core`, `requirements:draft` · **Template:** Bug report ·
**Wave:** 1 · **Parent:** #117 · **Related:** #69 (J5-048/049/050)

````markdown
### Expected and actual behavior

Expected: reading values from a parsed `json5_document` is repeatable, does not consume
the caller's buffer, is safe to do from several threads on a `const` document, and a
member lookup costs time proportional to the object's size.

Actual:
1. `json5_value::as_double()` takes big-integer scratch from the document's own storage.
   For a caller-buffer document, `buffer_arena` only rewinds when every allocation is
   released, which never happens while the document lives. Every `as_double()` call
   consumes arena space permanently; `to_double` reserves two scratch integers for every
   finite non-zero value. `current_bytes()`/`peak_bytes()` do not show it, so the
   documented sizing method cannot detect it.
2. Because `as_double()` (a `const` member) mutates the document's accounting and arena,
   two threads reading numbers from one `json5_document` race on
   `accounted_storage::m_current_bytes/m_peak_bytes` and `buffer_arena::m_used/m_live`.
   `document.hpp:209` promises concurrent const reads; `json5_document.hpp` says nothing.
3. `json5_value::member()` loops over `member_at(position)`, and each `member_at` walks
   the sibling list from the first member, so one lookup is O(n²) for n members and
   always scans every member. No post-parse operation takes a work budget.

### Minimal reproduction

```cpp
#include "adaptive_json5/adaptive_json5.hpp"
#include <cstdio>
#include <string_view>

int main()
{
  static unsigned char memory[4096];
  constexpr std::string_view text = "{ port: 8080, ratio: 1.5 }";
  const auto parsed = adaptive_json5::json5_document::parse({text.data(), text.size()}, memory, sizeof(memory));
  const auto root = parsed.value_if()->root();
  for (int i = 1; i <= 100000; ++i)
  {
    const auto d = root.member("ratio").as_double();
    if (!d.has_value())
    {
      std::printf("failed at read %d, code %d\n", i, static_cast<int>(d.error_if()->code));
      return 1;
    }
  }
  return 0;
}
```

Result at `0.2.0`: `failed at read 163, code 7` (`allocation_failure`), while
`current_bytes()` stays 649 and `peak_bytes()` 761.

Lookup cost: an object with 1024 members (the default `max_members`) takes about 2.5 ms per
`member()` call on an x86-64 host with an `-O2` library, whichever key is requested.

### Version and build configuration

Tag `0.2.0` (`903aedc`; production code identical to the reviewed `64510be`). Linux x86-64,
GCC 13.3.0, exceptions OFF, RTTI OFF, caller-buffer (`json5_document::parse`) path.

### Review findings covered

LOG-01 (High), LOG-02, LOG-11, ARC-01, CS-03, DOC-01, DOC-02 in
`doc/review/2026-11-06-full-repository-review-0.2.0.md`.

### Proposed direction (owner decides)

- Make value reads allocation-free for the common case (fast path for spellings with
  ≤ 19 significant digits and small exponents), or make `buffer_arena` reclaim LIFO
  releases (the two scratch integers are already released in reverse order), or take
  scratch from a separate caller-supplied provider.
- Walk the sibling chain once in `member()` (carry the current `value_view`), making it O(n).
- State the thread-safety rule in `json5_document.hpp` and `json5_value.hpp`.
- Decide whether lookups/exports need their own work budget (review question 17).

### Unit-test acceptance criteria

- 10 000 `as_double()` reads on a 4 KiB caller buffer succeed, including values with
  300-digit spellings; the same for `hosted::parse`.
- `J5_037_Json5Value.ReadsDoublesUsingDocumentScratch` gains an oracle that detects arena
  consumption (not only `current_bytes()`).
- Either two threads reading numbers from one document are race-free (document why), or
  the header states that concurrent `as_double()` on one document is not supported.
- `member()` on 0, 1, 1024 members: first, last, missing and duplicated names give the same
  results as today; if a lookup work budget is added, exhaustion at the boundary returns
  `resource_limit`.
- All existing units pass with exceptions OFF and ON, RTTI OFF.

### Responsibilities

- Romanr92: production C++ and public headers.
- AI: the unit tests above and the documentation updates (`docs/memory-and-limits.md`).
````

---

### I-02 — Error model: locate and explain limit, access and internal failures

**Title:** `[Implementation] I-02 — Error reasons and locations for limits, access and internal faults` · **GitHub:** #125 (assigned to Romanr92)
**Labels:** `enhancement`, `component:core`, `requirements:draft` · **Template:**
Implementation task · **Wave:** 1 · **Parent:** #117 · **Related:** #69

````markdown
### Requirement IDs and prerequisites

J5-017, J5-039, J5-043, J5-044, J5-045. No prerequisite issue.

### Behavior and scope

Make every failure identifiable without reading prose:

1. Exceeding `max_depth`, `max_nodes`, `max_members` (and `max_input_bytes`) returns
   `resource_limit` without a byte offset, and no `resource_limit` says which limit was hit
   (`core/src/parser/parser_engine.cpp:126-148, 684-698`). Add the offset of the token that
   would exceed the limit and an optional limit reason (input, depth, nodes, members,
   string, total string, number bytes, exponent digits, owned bytes, diagnostics, work).
2. `json5_value::member()` returns the same `wrong_type_or_access` for a missing member, a
   duplicated member and a non-object. Add an access reason (wrong type, absent member,
   duplicate member, index out of range).
3. Internal invariant violations (pass disagreement) are reported as
   `wrong_type_or_access`; add an `internal_error` category or reason.
   `limited_scan_error` (`parser_work.cpp:85-100`) turns any unlocated error (e.g.
   `allocation_failure`) into `resource_limit` when the scan was cut; restrict that to
   `syntax`/`encoding` errors.
4. Unknown words starting with `N` or `I` (`None`, `Index`, `Nan`) are reported mid-word
   without `source_range`, unlike `nul`/`trueish`.
5. `allocate_block` reports an invalid alignment as `resource_limit`; it is an invalid
   argument.
6. `accounted_storage::deallocate_callback` subtracts without an underflow check; a
   mismatched release wraps the counter. Saturate and latch a fault state.
7. Decide the J5-017 precedence: with `max_diagnostics = 0` a valid document with one raw
   U+2028 fails with `resource_limit`, while J5-017 says warnings never turn valid parsing
   into failure. Align J5-017, the API note and `docs/memory-and-limits.md`.
8. Consider a compact `error` layout while adding reasons (today 104 bytes on x86-64,
   inline in every `result<T>`).

### Unit-test acceptance criteria

- Each limit at limit + 1: `resource_limit`, the documented reason, and an offset at the
  offending token (or an explicit "no location" rule for `max_input_bytes`).
- `member()` cases: absent, duplicated, non-object, invalid UTF-8 name → distinct reasons.
- A cut scan with an allocation failure keeps `allocation_failure`.
- `[None]`, `[Index]`, `[Nan]` → `unknown_literal` with a whole-word `source_range`.
- Invalid alignment → `wrong_type_or_access`.
- Mismatched release does not wrap `current_bytes()`.
- `max_diagnostics` 0 and 1 with a value warning and a member-name warning follow the
  decided precedence.
- All in both exception modes, RTTI OFF.

### Review findings covered

LOG-03, LOG-04, LOG-05, LOG-06, LOG-07, LOG-08, ARC-04, FS-05, DOC-05, DOC-06, DOC-19.

### Responsibilities

- Romanr92: `error.hpp`, parser and memory production code, J5-017 decision.
- AI: unit tests, `docs/memory-and-limits.md` and requirement text after the decision.
````

---

### I-03 — Enable extended compiler warnings and fix mechanical coding-rule findings

**Title:** `[Implementation] I-03 — Extended warnings, prefixed include guards and mechanical MISRA fixes` · **GitHub:** #126
**Labels:** `enhancement`, `component:core` · **Template:** Implementation task ·
**Wave:** 1 · **Parent:** #117

````markdown
### Requirement IDs and prerequisites

Coding baseline in `core/docs/design-coding-style.md` (MISRA C++:2023, CERT C++). Land
I-14's guide changes first or together.

### Behavior and scope

1. GCC/Clang targets use only `-Wparentheses -Werror` (`CMakeLists.txt:62-63`). Add at least
   `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wold-style-cast`.
   Review sweep at `0.2.0`: 65 of 71 production TUs already clean; remaining diagnostics are
   an unused typedef (`json5_value.cpp:226`), two useless casts
   (`pointer_alignment.cpp:18,21`) and 32 `-Wswitch-enum` sites. Test targets also need a pass.
2. Prefix every installed include guard with `ADAPTIVE_JSON5_` (22 of 38 public headers use
   e.g. `INPUT_HPP_`, `OPTIONS_HPP_`; `-DOPTIONS_HPP_` breaks the umbrella header) and update
   the coding-style guide rule.
3. Default member initializers for public aggregates (`error`, `string_warning`, span
   structs); initialize the three uninitialized locals (`punctuation.cpp:32`,
   `string_boundary.cpp:34`, `string_warning.cpp:133`).
4. `parser_stack::pop()` returns `bool` but all three callers ignore it; return `void` or
   mark `[[nodiscard]]` and check it.
5. List every enumerator in switches over `parser_state`/`punctuation_kind` (or enable
   `-Wswitch-enum` for the parser directory).
6. Replace pointer↔`uintptr_t` alignment arithmetic by `std::align` where possible.
7. Use fixed-width types (or documented minimum-range reasoning) instead of `int`/`unsigned
   int` in numeric code; spell out deduced arithmetic types where `auto` hides them.
8. Fix the comment typos listed in STD-14.

### Unit-test acceptance criteria

- All project targets (library, hosted, units, validation) build warning-free with the new
  flags in both exception modes.
- A compile-only unit: a translation unit that defines `INPUT_HPP_`, `OPTIONS_HPP_`,
  `PARSER_HPP_`, `DOCUMENT_HPP_` and `STORAGE_HPP_` before including the umbrella header
  compiles.
- `error{}` value-initializes `code`, `byte_offset` and `has_byte_offset` (static check).
- All existing units pass unchanged.

### Review findings covered

STD-01, STD-04, STD-08, STD-09, STD-10, STD-11, STD-12, STD-14, STD-15.

### Responsibilities

- Romanr92: production and public-header edits.
- AI: CMake flag change, test-code warning fixes, compile-only units, coding-guide text.
````

---

### I-04 — Reduce complexity hot spots and generate both parser passes from one walker

**Title:** `[Implementation] I-04 — Split high-complexity functions and unify count/build passes` · **GitHub:** #127
**Labels:** `enhancement`, `component:core` · **Template:** Implementation task ·
**Wave:** 2 · **Parent:** #117

````markdown
### Requirement IDs and prerequisites

No behavior change intended. Prerequisite: I-03 (warnings) to keep the refactor safe.

### Behavior and scope

1. 46 functions exceed cyclomatic complexity 10 and 10 exceed 100 lines. Largest:
   `convert_to_floating` (CCN 54, 239 lines), `read_integer_magnitude` (36),
   `analyse_number` (36), `consume_decimal` (32), `parser_engine::read_value` (31). Split
   along natural phases and record the accepted thresholds in the coding-style guide.
2. The count pass (`count_value/count_name/count_separator/count_close`) duplicates the
   build pass state machine; agreement is only checked at run time. Generate both from one
   templated walker with a counting and a building policy.
3. Optional performance follow-ups: single-decode whitespace loop; reuse the count-pass
   measurement in the build pass.

### Unit-test acceptance criteria

- All 2514 unit and 921 validation cases pass unchanged in both exception modes.
- No function above the recorded thresholds, or each exception justified in the guide.
- Work charged per document is unchanged or documented where intentionally reduced.

### Review findings covered

LOG-09, STD-07, ARC-02.

### Responsibilities

- Romanr92: production refactor.
- AI: metric report, any unit additions for newly separated helpers (internal units in
  `core/tests/`).
````

---

### I-05 — Settle the public API surface before 1.0

**Title:** `[Implementation] I-05 — Decide public API surface: builder, storage callbacks, ABI, index identity` · **GitHub:** #128
**Labels:** `enhancement`, `component:core`, `requirements:draft` · **Template:**
Implementation task · **Wave:** 2 · **Parent:** #117

````markdown
### Requirement IDs and prerequisites

J5-008, J5-092, J5-093. Record decisions in an ADR before code changes.

### Behavior and scope

1. Public headers expose construction internals: `document::make_*`, `append_*`,
   `set_source_span`, `set_root`, `owned_block`/`allocate_block`, `detail::value_storage`,
   `detail::document_storage_owner`. A consumer can mutate a parsed document (an append to
   an exact-reserved document then fails with the internal-error code). Decide on a
   read-only view versus a separate builder and an explicitly unsupported `detail/` folder.
2. `storage_callbacks` has two allocation entry points (`allocate`, `allocate_result`);
   decide whether to keep one in the next breaking release.
3. Write an ABI policy (which options change the ABI; consider an inline versioned
   namespace once the API stabilizes).
4. `value_index` has no document identity; an index from another document silently
   addresses an unrelated record. Keep it as a documented precondition or add an owner tag.

### Unit-test acceptance criteria

- Per decision: compile-only units for the supported surface; a misuse case for the index
  identity rule (documented error or documented precondition).
- Umbrella coverage unit updated if headers move.

### Review findings covered

ARC-03, ARC-05, ARC-07, ARC-09.

### Responsibilities

- Romanr92: ADR decisions, public headers, production code.
- AI: ADR drafting on request, units, documentation.
````

---

### I-06 — Package import: check the exact release tag and expose the hosted component

**Title:** `[Bug] I-06 — Import helper accepts a different release tag; hosted component not discoverable` · **GitHub:** #129 (assigned to Romanr92)
**Labels:** `bug` · **Template:** Bug report · **Wave:** 1 · **Parent:** #117

````markdown
### Expected and actual behavior

Expected: `adaptive_json5_import_release(VERSION 0.2.0-rc.9 ROOT <dir>)` fails when `<dir>`
contains the final `0.2.0` package, and `find_package(AdaptiveJSON5 CONFIG COMPONENTS hosted)`
works when the hosted library is installed.

Actual:
1. `cmake/ImportAdaptiveJSON5.cmake:14-17` reduces the tag to its numeric version and lines
   94-101 check only `find_package(... EXACT)` and the profile. `build-info.json`
   `release_tag` is never compared and not exported.
2. `cmake/AdaptiveJSON5Config.cmake.in` sets `AdaptiveJSON5_parser_FOUND` and
   `AdaptiveJSON5_build_options_FOUND` but never `AdaptiveJSON5_hosted_FOUND`.

### Minimal reproduction

Build a `linux-x86_64-gcc` package of tag `0.2.0` (with the packaging fix from
`72-release-packaging`), extract it, and configure a consumer with
`adaptive_json5_import_release(VERSION 0.2.0-rc.9 ROOT <extracted>)` or
`VERSION v0.2.0-beta.1`: both exit 0 and report `AdaptiveJSON5_VERSION=0.2.0`.

### Version and build configuration

Tag `0.2.0`, CMake 3.28, GCC 13.3.0, Linux x86-64.

### Review findings covered

REPO-15, ARC-06.

### Unit-test acceptance criteria

- Tool tests (Python or CMake script mode): matching tag succeeds; `rc.1` vs `rc.2`,
  pre-release vs final and (per decision) `v`-prefix mismatches fail on the `ROOT` and
  reused-cache paths; wrong profile/numeric version still fail.
- `AdaptiveJSON5_RELEASE_TAG` exported.
- `find_package(AdaptiveJSON5 CONFIG REQUIRED COMPONENTS hosted)` succeeds with a hosted
  install and fails clearly without it.

### Responsibilities

CMake and tests only: AI may implement; owner reviews.
````

---

### I-07 — CI: test the shipped Release configuration and run gates on main

**Title:** `[Implementation] I-07 — CI: Release unit leg, push/schedule triggers, tool checks, locale` · **GitHub:** #130 (assigned to Romanr92)
**Labels:** `enhancement` · **Template:** Implementation task · **Wave:** 1 ·
**Parent:** #117

````markdown
### Requirement IDs and prerequisites

J5-052, J5-091, J5-099, J5-100. Uses existing unit/validation tests only (no new gate type).

### Behavior and scope

1. All test jobs build `Debug`; `tools/package_release.py` ships `Release` with tests OFF.
   Add a `Release` (or `RelWithDebInfo`) unit leg matching the packaging flags.
2. Gates run on `pull_request` only. Add `push: branches: [main]` and a weekly `schedule`
   (MSYS2 GCC and `ubuntu-latest` drift).
3. Run `python3 -m unittest discover -s tools/tests` and an offline Unicode-table
   regeneration diff on pull requests. The 0.2.0 packaging failure (header inventory check)
   would have been caught by exercising the packaging script before the tag.
4. Install a comma-decimal locale (e.g. `de_DE.UTF-8`) on Linux jobs so
   `J5_052_NumericLocale.ResultsIgnoreProcessLocale` executes instead of being skipped.
5. Record, per release, which configurations the evidence covers (Debug/Release, host
   only, no target execution).

### Unit-test acceptance criteria

- New Release leg: 2514/2514 units pass in both exception modes.
- Locale case reported as passed (not skipped) on Linux.
- Tool job: 4/4 Python units and byte-identical Unicode regeneration.
- A deliberately broken `tools/package_release.py` fails a PR job (one-time check).

### Review findings covered

FS-07, REPO-01, REPO-02, REPO-06, REPO-07.

### Responsibilities

CI/CMake/tests only: AI may implement; owner approves the scope of item 3 if it counts as a
new job.
````

---

### I-08 — Supply-chain hardening for workflows and release artifacts

**Title:** `[Implementation] I-08 — Pin actions and runners, attest releases, publish SBOM` · **GitHub:** #131
**Labels:** `enhancement` · **Template:** Implementation task · **Wave:** 1 ·
**Parent:** #117

````markdown
### Requirement IDs and prerequisites

J5-007, J5-093. No prerequisite.

### Behavior and scope

1. Pin by full SHA: `actions/checkout@v7` and `actions/setup-python@v7` in
   `unit-tests.yml` (first job), `formatting.yml`, `examples.yml` (other jobs already pin).
2. Replace `ubuntu-latest` by a fixed image; record exact `g++-13` and MSYS2 package versions
   in release jobs.
3. `pip install clang-format==18.1.8` → hashed requirements file (`--require-hashes`).
4. Release integrity: SHA-256 files uploaded to the same release detect corruption, not
   tampering. Add GitHub artifact attestations (SLSA provenance) or Sigstore signatures and
   an SPDX or CycloneDX SBOM per archive.
5. Enable Dependabot for GitHub Actions.

### Unit-test acceptance criteria

- Every `uses:` is a 40-character SHA with a version comment.
- A release dry run produces attestation/signature and SBOM next to each archive.
- Existing unit, validation and formatting jobs stay green.

### Review findings covered

CS-04, REPO-05.

### Responsibilities

CI/packaging only: AI may implement; owner approves release changes.
````

---

### I-09 — Optional sanitizer and fuzz workflow (needs scope decision)

**Title:** `[Implementation] I-09 — Non-blocking sanitizer and fuzz workflow; sanitizer-clean tests` · **GitHub:** #132
**Labels:** `enhancement` · **Template:** Implementation task · **Wave:** 2 ·
**Parent:** #117 · **Related:** #116 (fuzzing)

````markdown
### Requirement IDs and prerequisites

J5-097, J5-098. **Needs an owner decision:** `AGENTS.md` excludes sanitizer and fuzz gates;
this proposes a scheduled, non-blocking job like the existing `examples` workflow. #117's
exit criteria already list a sanitizer/fuzz rerun.

### Behavior and scope

1. Review evidence at `0.2.0`: all 3435 unit and validation cases under ASan+UBSan show no
   production defect; 59 failures are `alloc-dealloc-mismatch` from incomplete global
   `operator new/delete` replacements in `tests/api/pure_memory_tests.cpp:24-66` and
   `tests/api/diagnostic_format_tests.cpp:45-63`. Replace all overloads (plain, nothrow,
   aligned, sized, array) consistently.
2. Commit a test-only libFuzzer harness under `tests/` (parse both modes and policies, walk
   the value tree, numeric conversions, UTF-8 export, `format_error`, simple API) with a
   seed corpus from the committed fixtures. A 10-minute review run (21.5 M executions) found
   no crash.
3. Scheduled, non-blocking workflow: ASan+UBSan unit run plus a short fuzz run.

### Unit-test acceptance criteria

- Sanitizer job: all units and validation cases pass (no `alloc-dealloc-mismatch`).
- Fuzz smoke run (e.g. 60 s) completes without a crash, timeout or OOM.
- Normal jobs unaffected.

### Review findings covered

CS-02, REPO-03.

### Responsibilities

Test code and CI only: AI may implement after the owner's scope decision.
````

---

### I-10 — Build and repository hygiene

**Title:** `[Implementation] I-10 — Build and repository hygiene from the full review` · **GitHub:** #133
**Labels:** `enhancement` · **Template:** Implementation task · **Wave:** 2 ·
**Parent:** #117

````markdown
### Requirement IDs and prerequisites

J5-007, J5-092. No prerequisite.

### Behavior and scope

1. `cmake/Dependencies.cmake`: scope `FETCHCONTENT_UPDATES_DISCONNECTED`, avoid `FORCE` on
   generic GoogleTest cache options, and download GoogleTest into the binary directory
   instead of `tools/gtest` in the source tree.
2. Add `* text=auto eol=lf` to `.gitattributes`.
3. Add `SPDX-License-Identifier: Apache-2.0` and a copyright line to every source file
   (171 of 172 production files and all tests lack it).
4. Align `.github/pull_request_template.md` with the required checks (validation tests,
   exceptions-ON leg).
5. Reformat the `install(...)`/hosted block in `CMakeLists.txt:122-135`.
6. Move historical backlog snapshots in `core/docs/*.json` to an archive folder.
7. Decide whether `.zed/user-config/` belongs in the repository.
8. Rename `ADAPTIVE_JSON5_BUILD_EXECUTABLE` to `ADAPTIVE_JSON5_BUILD_EXAMPLES` with a
   compatibility alias until a CLI exists.

### Unit-test acceptance criteria

- Parent-project probe: enabling this subproject's tests does not change the parent's
  GoogleTest cache options.
- All existing jobs green; formatting unchanged except the reformatted CMake block.

### Review findings covered

ARC-08, REPO-04, REPO-08, REPO-09, REPO-10, REPO-11, REPO-12, REPO-13.

### Responsibilities

CMake, configuration and headers' comment lines: AI may implement CMake/config; SPDX lines in
production files need owner approval (production files).
````

---

### I-11 — Documentation corrections from the full review

**Title:** `[Documentation] I-11 — Correct stale and inaccurate statements found in the full review` · **GitHub:** #134
**Labels:** `documentation` · **Template:** Implementation task · **Wave:** 1 ·
**Parent:** #117

````markdown
### Requirement IDs and prerequisites

J5-008, J5-100. No prerequisite.

### Behavior and scope

1. `docs/memory-and-limits.md:237-238`: "every limit … found in the first pass" is not true
   for `max_owned_bytes` and partly `max_work_units`.
2. `docs/conformance.md:158`: broken anchor `requirements.md#minimum-unit-regression-checklist`
   → `../core/docs/requirements-quality.md#minimum-unit-regression-checklist`.
3. `docs/traceability.md:26-70`: duplicated "Layout and mapping rules" block and empty
   "Core requirements" heading.
4. `AGENTS.md:8`, `docs/dependencies.md:14`: proposal path `docs/requirements/proposals/`
   does not exist.
5. `docs/requirements.md:12`, `core/docs/requirements-quality.md` (J5-099): "unit tests and
   formatting only" — validation tests and the examples job exist.
6. `docs/adr/0001-project-scope.md:21`: macOS target conflicts with requirements and README.
7. `core/docs/design-architecture.md:137`: "Remaining: P034/P035 …" node is stale.
8. `includes/adaptive_json5/result.hpp:230,245`: `error_if()` Doxygen brief says "Construct
   failure with a payload" (public header — owner edit).
9. Repeated "official-specification implementation sequence" paragraph in README, AGENTS.md
   and requirements.md; keep one copy.
10. `.agents/skills/guide-requirement/SKILL.md:180` calls the complete-document entry
    "future"; `.zed/README.md:38` says "currently 98 cases".
11. Move the U+200B whitespace choice into the conformance deviations table.

### Unit-test acceptance criteria

- Relative-link and anchor check over all Markdown files reports no errors.
- No hard-coded current test totals in guides (or dated ones).

### Review findings covered

LOG-10, DOC-04, DOC-07, DOC-08, DOC-09, DOC-10, DOC-11, DOC-12, DOC-14, DOC-15, DOC-18.

### Responsibilities

AI may implement all documentation items; item 8 is a public-header comment (owner).
````

---

### I-12 — Release notes, known anomalies and release baselines

**Title:** `[Documentation] I-12 — 0.2.0 release notes with known anomalies; release baseline process` · **GitHub:** #135
**Labels:** `documentation` · **Template:** Implementation task · **Wave:** 1 ·
**Parent:** #117

````markdown
### Requirement IDs and prerequisites

J5-093, J5-100. Prerequisite: merge `72-release-packaging` (README pin to `0.2.0`, packaging
inventory fix).

### Behavior and scope

1. Publish 0.2.0 release notes with a **known anomalies** section: LOG-01 (`as_double()`
   exhausts caller buffers), LOG-02 (data race on shared reads), LOG-11 (quadratic
   `member()`), with the workaround "use `values()` with `to_double()` and your own scratch
   storage, or the hosted library, for repeated floating-point reads".
2. Add a CHANGELOG or release-notes template (supported versions, evidence links, known
   anomalies).
3. Keep documentation pinned to release tags (README quick start).

### Unit-test acceptance criteria

Documentation only: release notes linked from `docs/conformance.md`; README quick start
compiles and prints `port: 8080` with `GIT_TAG 0.2.0`.

### Review findings covered

FS-09, DOC-13, DOC-16.

### Responsibilities

AI may draft; owner publishes the GitHub release text.
````

---

### I-13 — Embedded integration figures: stack, runtime facilities, data model, timing

**Title:** `[Documentation] I-13 — Record embedded runtime facilities, stack usage and timing method` · **GitHub:** #136
**Labels:** `documentation`, `component:core`, `requirements:draft` · **Template:**
Implementation task · **Wave:** 1 · **Parent:** #117 · **Related:** #115, #69 (J5-101)

````markdown
### Requirement IDs and prerequisites

J5-101, J5-048. Related to #115 (embedded examples and release evidence) and #69.

### Behavior and scope

`docs/embedded.md:22-23` lists what must be recorded but records none of it. Add:

1. Worst-case stack usage per public entry point (GCC `-fstack-usage` plus call-graph
   analysis). Review probe on x86-64: `sizeof(parser_engine)` = 968 B, `sizeof(error)` =
   104 B, `sizeof(json5_value)` = 144 B.
2. Required C++ runtime facilities: `std::variant`, `std::optional`, `<type_traits>`,
   `std::memcpy`, placement new, `std::launder`; no static constructors (confirm).
3. Data-model assumptions: `CHAR_BIT == 8`, two's complement, `std::uintptr_t`
   availability, alignment requirements, octet/character representation for UTF-8.
4. A method to convert work units into time on a target, and the statement that work
   counters are not WCET.
5. Plan for executing the unit suite on a representative target (e.g. the AdaptivePi
   AArch64 profile).

### Unit-test acceptance criteria

- A host unit (or static assertion) for each documented data-model assumption.
- Stack figures reproducible from a documented command.

### Review findings covered

FS-06, DOC-03.

### Responsibilities

AI may measure on the host and write the documentation; owner confirms target plans.
````

---

### I-14 — Coding-standard governance: fail-fast text, deviation register, tailoring

**Title:** `[Documentation] I-14 — Coding-standard deviation register and accurate fail-fast rules` · **GitHub:** #137
**Labels:** `documentation` · **Template:** Implementation task · **Wave:** 1 ·
**Parent:** #117 · **Related:** #44 (the branch currently carrying the coding-style commit)

````markdown
### Requirement IDs and prerequisites

`core/docs/design-coding-style.md`. Land separately from the schema work in #44.

### Behavior and scope

1. Move commit `f103fc2` ("Control flow and error handling") from branch
   `44-schema-1-…-p043p046` into its own documentation PR (fix the subject typo "conding").
2. Make the rules match the code, or state the exceptions: near-cut error reclassification
   (`parser_work.cpp:85-100`), the single-use parser state that is discarded instead of
   rolled back, limited-scan charging after the scan (`parser_engine.cpp:247`), and the
   ignored `pop()` result (I-03). Add one sentence: RAII owners release resources on every
   early return — the ISO 26262-6 Table 6 item 1a rationale.
3. Fix the two stale "unit tests and formatting" sentences (lines 56 and 80) and the bare
   header-guard rule (see I-03).
4. Start a deviation register (rule, edition, scope, rationale, compensating measures):
   placement new/explicit destructor calls (STD-02), heap use in the hosted library
   (STD-03), `void*` callback contexts (STD-05), pointer↔integer casts (STD-04 residue).
5. Record the exact MISRA C++:2023 edition and technical corrigenda, CERT errata, the AUTOSAR
   C++14 stance (C++14 only; supplementary) and a BARR-C:2018 tailoring (80 vs 120 columns,
   4 vs 2 spaces, C99 and `auto`).

### Unit-test acceptance criteria

Documentation only; every register entry references code locations that exist at the
release commit.

### Review findings covered

STD-02, STD-03, STD-05, STD-06, STD-13, FS-04, FS-10, DOC-17, REPO-14.

### Responsibilities

AI may draft the guide and register; owner approves each deviation.
````

---

### I-15 — ISO 26262 readiness (SEooC scope, requirements baseline, analyses, tools)

**Title:** `[Implementation] I-15 — ISO 26262 readiness: safety scope, requirements baseline, analyses, tool confidence` · **GitHub:** #138
**Labels:** `enhancement`, `requirements:draft` · **Template:** Implementation task ·
**Wave:** 1 · **Parent:** #117

````markdown
### Requirement IDs and prerequisites

All J5 rows (status remains Draft until reviewed). **Owner decision first:** QM-only use,
or Safety Element out of Context up to a target ASIL (review question 1).

### Behavior and scope

1. Safety scope: no HARA at library level — the HARA, safety goals and ASIL belong to the
   integrator's item. The library records an *assumed* ASIL (or QM), assumptions of use (input trust, single-threaded use
   per document, caller storage guarantees, timing budget), software safety requirements,
   safety manual with integration obligations and known anomalies.
2. Requirements baseline: split rows into atomic, verifiable statements; add attributes
   (ASIL/QM, status, verification method, result reference); hold and record a requirements
   review; baseline per release. 45 rows have no unit test today.
3. Architecture and analyses: current static and dynamic architecture view, interface table
   (resources, errors per component), software FMEA (wrong value accepted, valid input
   rejected, hang, memory corruption, wrong error category) and dependent-failure analysis
   for shared storage.
4. Tool confidence: TI/TD/TCL evaluation for compilers (GCC, MinGW, Yocto SDK), CMake,
   `tools/generate_unicode_tables.py` (reference `core/tests/unicode/unicode_tables_tests.cpp`
   as detection), `tools/package_release.py`, clang-format, GoogleTest and gcovr.
5. Structural coverage per release (review run: 95.7 % lines, 98.5 % functions, 90.0 %
   branches; MC/DC not measured) with justification of uncovered branches; MC/DC tooling if
   ASIL C/D is targeted.

### Unit-test acceptance criteria

Process issue: each item produces a reviewed document; new units only where the analyses
identify untested failure modes.

### Review findings covered

FS-01, FS-02, FS-03, FS-08 (FS-04 → I-14, FS-05 → I-02, FS-06 → I-13, FS-07 → I-07,
FS-09 → I-12, FS-10 → I-14).

### Responsibilities

Owner: scope decision and approvals. AI may draft documents and add units.
````

---

### I-16 — ISO/SAE 21434 readiness (security policy, TARA, guidance)

**Title:** `[Documentation] I-16 — SECURITY.md, TARA and cybersecurity guidance for integrators` · **GitHub:** #139
**Labels:** `documentation` · **Template:** Implementation task · **Wave:** 1 ·
**Parent:** #117

````markdown
### Requirement IDs and prerequisites

J5-032, J5-047. Prerequisite for supply-chain items: I-08.

### Behavior and scope

1. Add `SECURITY.md`: private reporting channel, supported versions, response targets,
   advisory process; enable GitHub security advisories.
2. The item TARA (damage scenarios, impact ratings, cybersecurity goals) belongs to the
   integrator. At library level, turn the review's TARA-lite into a maintained
   out-of-context threat analysis with an assumed assurance level (CAL): assets, threats (memory
   corruption, resource exhaustion, semantic confusion, disclosure, supply chain),
   controls, residual risks, and out-of-context assumptions — including that syntax
   acceptance does **not** authenticate a configuration (integrator must authenticate the
   source).
3. Integrator guidance: use `reject_duplicates` for security-relevant configuration; note
   that `as_string()` may return embedded NUL bytes (C-string consumers truncate); prefer
   the caller-buffer or callback storage path over the hosted (global heap) library.
4. Known-anomaly and fix process for released versions (links to I-12).

### Unit-test acceptance criteria

Documentation only; optionally a unit demonstrating the NUL behavior and the
duplicate-policy guidance.

### Review findings covered

CS-01, CS-05, CS-06, CS-07 (CS-02 → I-09, CS-03 → I-01, CS-04 → I-08).

### Responsibilities

AI may draft; owner approves policy and contact details.
````
