# Full repository review — Adaptive JSON5 after 0.2.0

| Item | Value |
| --- | --- |
| Review date | 2026-11-15 |
| Baseline | `47d4f84b27b7b193ad14d351edf8152f3a1be373` (`main`, 7 commits after tag `0.2.0`; CMake version still 0.2.0) |
| Previous review | [0.2.0 review](2026-11-06-full-repository-review-0.2.0.md) of `64510be` |
| Review branch | `claude/keen-edison-0aacft` |
| Reviewer | Claude Code (AI agent) |
| Procedure | [How to run a full repository review](how-to-run-a-full-repository-review.md) |
| Status | **Complete** — all plan steps done |

This document records review findings only. Per `AGENTS.md`, the reviewer does not
change production C++ or public headers. No finding claims a compliance,
certification or "verified" status.

**Scope rule for this review (owner request).** A finding is listed only if **no open
GitHub issue already covers it**. Findings of the 0.2.0 review that are tracked by an
open issue are not repeated. Step 0 records which issue excludes what.

---

## Review plan

### Progress tracker

| Step | Area | Status | Results |
| --- | --- | --- | --- |
| 0 | Method, baseline, exclusion list from open issues | Done | [Step 0](#step-0-results--method-and-exclusions) |
| 1 | Sources of truth and baseline evidence | Done | [Step 1](#step-1-results--baseline-evidence) |
| 2 | Logic: all production code, with focus on code changed since 0.2.0 | Done | [Step 2](#step-2-results--logic-findings) |
| 3 | Coding standards | Done | [Step 3](#step-3-results--coding-standards-findings) |
| 4 | ISO 26262: review of the new safety baseline against the code | Done | [Step 4](#step-4-results--iso-26262-findings) |
| 5 | ISO/SAE 21434: review of the new cybersecurity baseline against the code | Done | [Step 5](#step-5-results--isosae-21434-findings) |
| 6 | Architecture | Done | [Step 6](#step-6-results--architecture-findings) |
| 7 | Documentation | Done | [Step 7](#step-7-results--documentation-findings) |
| 8 | Full repository (build, CI, tests, tools) | Done | [Step 8](#step-8-results--full-repository-findings) |
| 9 | Consolidation | Done | [Step 9](#step-9-results--summary) |

### What is different from the 0.2.0 review

1. **Three fixes to verify.** #124 (number reads, member lookup), #125 (error reasons and
   locations) and #129 (release-tag check, hosted component) were merged. Each is
   re-tested against its own acceptance criteria; anything left open is a new finding,
   because the issue is closed.
2. **New code to review from scratch.** The fast number path (power-of-five table, wide
   multiply), caller scratch, error-reason types and the new tests.
3. **New safety and cybersecurity baselines.** `docs/ISO26262-safety/` and
   `docs/ISO21434-cybersecurity/` now exist. Steps 4 and 5 therefore check the documents
   against the code and CI, instead of listing what is missing.
4. **Changed quality scope.** `AGENTS.md` now allows sanitizers, fuzzing, coverage, Clang
   and static analysis.

### Method

Severity scale, finding format and ID prefixes are those of the
[how-to guide](how-to-run-a-full-repository-review.md). Finding IDs carry the prefix
`R2-` (for example `R2-LOG-01`) so they cannot be confused with the 0.2.0 review's IDs.
Every `path:line` refers to the baseline `47d4f84`. "Probe" means a throw-away program
compiled against the baseline library, kept outside the repository.

---

## Step 0 results — Method and exclusions

**What was excluded.** These open issues already cover the listed topics, so nothing
about them appears below, even where the review saw the problem again.

| Open issue | Topics left out of this review |
| --- | --- |
| #126 | Compiler warning set, include-guard names, uninitialized aggregates, `pop()` result, switch completeness, fixed-width types, comment typos |
| #127 | Function size and complexity, duplicated count/build passes |
| #128 | Public API surface: document mutators, storage callbacks, ABI policy, index identity |
| #130 | Release test job, `push`/schedule triggers, tool tests in CI, locale test skipped on Linux |
| #131 | Action and runner pinning, hashed tool installs, release signing, SBOM |
| #132 | Sanitizer and fuzz jobs; the 81 `alloc-dealloc-mismatch` reports from test-only allocator replacements |
| #133 | CMake/GoogleTest scoping, `.gitattributes`, SPDX headers, PR template wording, option naming |
| #134 | Known stale statements, including the broken anchor in `docs/conformance.md` |
| #135 | Release notes with known anomalies |
| #136 | Stack figures, runtime facilities, data-model assumptions, timing method |
| #137 | Deviation register, coding-style guide alignment |
| #141, #142, #143, #146 | Clang job, tool self-checks, static analysis |
| #145 | Secure mode (duplicates, zero bytes, control characters, unpaired surrogates) |
| #69, #44–#71, #110–#117 | Planned features and milestones |

**Closed issues re-tested.** #124, #125 and #129 are closed, so their fixes were
re-tested against their acceptance criteria. All three hold (see Step 2). What they left
open is listed as new findings.

## Step 1 results — Baseline evidence

Linux x86-64, GCC 13.3.0, CMake 3.28.3, clang 18.1.3, clang-format 18. Nothing was
changed in the repository to produce these results; all build trees, probes and the fuzz
harness are outside it.

| Check | Result |
| --- | --- |
| Unit tests, Debug, exceptions OFF and ON, RTTI OFF | **2652 / 2652 passed** in each mode (2 cases reported as skipped) |
| Validation tests, exceptions OFF and ON | **921 / 921 passed** in each mode |
| Unit and validation tests, **Release** build | **3573 / 3573 passed** |
| Unit tests with `ADAPTIVE_JSON5_EISEL_LEMIRE=OFF` | **2650 / 2650 passed** |
| `python3 tools/format.py --check` | Passed for 268 C++ files |
| `python3 -m unittest discover -s tools/tests` | 22 / 22 passed |
| Regenerate Unicode tables and power-of-five table | Both byte-identical to the checked-in files |
| Power-of-five table spot check | Entries for 5^-342, 5^0 and 5^308 equal the published values of Lemire's algorithm |
| Extended GCC warnings (library and hosted) | 66 of 72 files clean. New since 0.2.0: 12 × `-Wfloat-conversion` and 1 × `-Wuseless-cast` in `floating_conversion.cpp` (R2-STD-01) |
| ASan + UBSan over all 3573 cases | No report in production code; 81 test-harness reports (excluded, #132) |
| clang-tidy over `core/src` | No new class of finding; 16 functions above cognitive complexity 25 (excluded, #127) |
| libFuzzer + ASan + UBSan, 4 workers × 421 s | **12,315,098 executions, no crash, timeout or out-of-memory**; 2515 covered edges |
| Markdown links and anchors, 58 files | One broken anchor, already in #134 |
| README quick start | Compiles and prints `port: 8080` against `main` and against the `0.2.0` tag headers |
| Software-unit IDs | 416 `@par ID` tags in code match the registry; no duplicate, missing or reused ID |

## Step 2 results — Logic findings

**Closed fixes verified (probes).**

- **#124:** 100,000 `as_double()` reads on a 4 KiB buffer succeed for `1.5`, a
  300-digit number, `1e-310`, `0xFFFFFFFFFFFFFFFF` and a 19-digit number;
  `current_bytes()` is unchanged. `member()` on 1023 members takes about 21 µs
  (Release library, x86-64), down from about 2.5 ms.
- **#125:** every limit reports its reason, and token-bound limits report an offset;
  `member()` distinguishes absent, duplicated and wrong-type; `None`, `Index`, `Nan` get
  a whole-word range; `sizeof(error)` dropped from 104 to 48 bytes.
- **#129:** covered by the new tool tests (22 pass).

**New code reviewed without a defect.** The Eisel–Lemire step
(`core/src/value/floating_conversion.cpp:213-307`) matches the published algorithm
line by line: normalization, the 128-bit product with the conditional second multiply,
the safe-exponent check, the round-to-even case, the carry after rounding, and the
fallback to the exact path for subnormal, overflow and undecidable cases. The portable
64×64→128 multiply (`core/includes/value/wide_multiply.hpp`) cannot overflow. The
LIFO release in `buffer_arena` and the refused-release logic in `accounted_storage`
behave as documented.

| ID | Severity | Location | Observation | Evidence / impact | Recommendation |
| --- | --- | --- | --- | --- | --- |
| R2-LOG-01 | **Medium** | `core/src/value/floating_conversion.cpp:376-397`; `includes/adaptive_json5/value/floating_conversion.hpp:19,48,73` | The new Clinger fast path does one hardware floating-point multiply or divide, so its result follows the caller's current rounding mode. The exact path and the Eisel–Lemire step use integers only and always round to nearest. Short numbers are therefore rounded differently from long ones when the caller has changed the rounding mode. Before #124 every result was independent of it. The public header still promises round-to-nearest without a condition; only ADR 0005 mentions the assumption. | *Probe:* with `fesetround(FE_DOWNWARD)`, `as_double()` of `0.1` returns `0x1.9999999999999p-4` instead of `0x1.999999999999ap-4`; with `FE_UPWARD`, `0.3` returns `0x1.3333333333334p-2`. The 35-digit spelling of the same 0.1 stays correct in every mode. A wrong value is returned as success, which SSR-04 (determinism) and SSR-06 (never a wrongly rounded value) exclude. No unit test changes the rounding mode. | Skip the Clinger step when Eisel–Lemire is compiled in (it covers the same inputs with integers), or guard it with a rounding-mode check; otherwise state the precondition in the public header, the safety manual and SSR-04. Add a unit that reads values under each rounding mode. |
| R2-LOG-02 | **Medium** | `core/src/value/json5_value.cpp:164`; `core/src/value/value_view.cpp:78` | #124 made `member()` linear but left `at(index)` as it was: each call walks the element list from the start. `json5_value` has no other way to visit array elements, so the natural loop `for (i = 0; i < size; ++i) at(i)` is quadratic. ADR 0005 (decision 6) only discusses `member()`. | *Probe:* one full `at(i)` loop over 1023 elements takes about **2.4 ms** (Release library, x86-64); a linear walk would take a few microseconds. On a small controller this is tens of milliseconds for a document within default limits (TARA threat T-03). | Give `json5_value` a linear way to iterate (a `next()`/cursor or a `for_each`), or cache the last visited position; document the cost of `at()` until then. |
| R2-LOG-03 | Low | `core/src/lexer/number/non_finite_lexer.cpp:76,107-132` | #125 gives unsigned unknown words a whole-word range, but signed ones keep the old behaviour: they are reported at the first mismatching byte with no range. | *Probe:* `[-Infinit]` reports `unknown literal` at offset 9, which is the `]`, with no `source_range`; `[Infinityx]` reports offset 1 with range 1+9. | Report signed words like unsigned ones: the range from the sign to the end of the word. |
| R2-LOG-04 | Low | `includes/adaptive_json5/memory/resource_limits.hpp:27,30` | The defaults `max_nodes = 1024` and `max_members = 1024` cannot both be reached: an object with 1024 members needs 1025 values (the object plus its members). The `max_members` default is unreachable. | *Probe:* an object with 1024 members fails with `limit_reason() == nodes`. Integrators copying the defaults (A-04) get a members limit that never applies. | Make the defaults consistent (for example `max_nodes = 1025` or `max_members = 1023`), or document the relation in `docs/memory-and-limits.md`. |

## Step 3 results — Coding standards findings

Everything the sweeps reported is covered by #126, #127 or #137, except code added
after those issues were written:

| ID | Severity | Location | Observation | Recommendation |
| --- | --- | --- | --- | --- |
| R2-STD-01 | Low | `core/src/value/floating_conversion.cpp:383-384`, `:304` | The Clinger table `constexpr T c_powers[]` is initialized from `double` literals up to `1e22` for both `float` and `double`. For `float`, twelve entries are converted inexactly (for example `1e11` becomes `9.9999998e10f`). They are never read, because `float` stops at `1e10`, but the implicit narrowing is exactly what `-Wconversion` reports (12 × `-Wfloat-conversion`), and one `static_cast<int>` is a useless cast on this target. #126 lists the old diagnostics only. | Use one table per type (11 entries for `float`), and add both diagnostics to the #126 list. |

## Step 4 results — ISO 26262 findings

The safety baseline added in #138 is honest about its status (Draft, not independently
confirmed, most checks planned) and its structure is sound: assumptions A-01 to A-09,
SSR-01 to SSR-08, an FMEA with a "gap and action" column, a dependent failure analysis,
a tool evaluation and a checklist-style safety manual. The decomposition, coverage and
test-derivation tables agree with the reviewer's knowledge of ISO 26262. The findings
below are places where the documents and the code have drifted apart.

| ID | Severity | Location | Observation | Recommendation |
| --- | --- | --- | --- | --- |
| R2-FS-01 | **Medium** | `docs/ISO26262-safety/safety-manual.md:132-134`; `software-safety-analysis.md:115-118`; `docs/ISO21434-cybersecurity/tara.md:62,94`; `cybersecurity-manual.md:108,160`; `cybersecurity-case.md:35` | #124 was merged after the two baselines, but they were not updated. The safety manual and the cybersecurity manual still list the `as_double()` buffer problem, the shared-state problem and the quadratic `member()` as open and "tracked in #124"; FMEA rows VM-03, VM-04 and VM-05 still say "Gap"; the TARA and the cybersecurity case still count them as residual risk. #125 did update its rows, so the documents are now inconsistent with each other. An integrator reading the known-limits table gets a wrong picture in both directions: fixed problems are listed, and the new ones (R2-LOG-01, R2-LOG-02) are not. | Update all five documents: close or re-word the rows, and add the remaining limits (plain `as_double()` still uses document storage for numbers that need scratch; `at()` loops are quadratic; rounding-mode precondition). Add "safety and security documents updated" to the PR checklist for changes that close a listed gap. |
| R2-FS-02 | **Medium** | `docs/ISO26262-safety/safety-manual.md:100` (C-3.8); `seooc-assumptions.md:373` (A-09); `docs/memory-and-limits.md:206`; `includes/adaptive_json5/json5_document.hpp` | Integrators must "check `faulted()` after releasing a document". With the simple API (`json5_document::parse` into a buffer, and `hosted::parse`) this cannot be done: the `accounted_storage` lives inside the document's storage, is not reachable through `json5_document`, and is destroyed together with the document. The obligation can only be met with the advanced `parse_document` API. | Expose the state on `json5_document` (for example `storage_faulted()`, to be read before destruction), or limit C-3.8 and A-09 to the advanced API and say what simple-API users must do instead. |
| R2-FS-03 | **Medium** | `docs/ISO26262-safety/software-safety-analysis.md:115` (VM-02); `docs/requirements.md` (SSR-04, SSR-06); `seooc-assumptions.md` | The analysis does not cover the new number path. VM-02 still names "big-integer arithmetic" as the only safety measure, but most numbers now go through the Clinger or Eisel–Lemire step. Missing failure causes: a wrong table entry (generator fault), dependence on the floating-point environment (R2-LOG-01), and excess-precision targets. SSR-04 lists what must be equal for equal results but does not mention the floating-point rounding mode, and no assumption A-xx covers it. | Add FMEA rows for the fast path with their measures and tests (the table check in `core/tests/value/fast_conversion_support_tests.cpp` is one), and either remove the rounding-mode dependence or add it as an assumption and a safety-manual item. |
| R2-FS-04 | Medium | `docs/ISO26262-safety/tool-confidence.md` (tool table) | `tools/generate_power_of_five_table.py` generates production code (`core/src/value/power_of_five_table.cpp`, 651 entries) but is not in the tool confidence evaluation, which lists only the Unicode generator. The detection measure exists (a unit recomputes every entry; regeneration is byte-identical), it is just not recorded, and #130's regeneration check names the Unicode tables only. | Add the generator to the evaluation with its TI/TD argument, and extend the regeneration check planned in #130 to this table. |
| R2-FS-05 | Low | CI workflows; `docs/adr/0005-allocation-free-number-reads.md` (decision 3) | `ADAPTIVE_JSON5_EISEL_LEMIRE=OFF` is a supported configuration that changes which code converts numbers, and the ADR says "units cover both settings". No workflow or preset builds it, so it is only tested when someone does so by hand. The review's local run passes (2650 / 2650). | Add an option-OFF leg to the unit-test workflow, or to the Release job planned in #130. |
| R2-FS-06 | Low | `docs/ISO26262-safety/verification-strategy.md:72`; `tools/coverage.py:68-69` | The strategy says gcovr 8.6 is "pinned in `tools/coverage.py`". The script only prints that version in an install hint and runs with whatever gcovr is installed (the 0.2.0 review ran it with 8.8). The tool confidence table relies on the same statement. | Check the gcovr version in the script and fail on a mismatch, or change the wording to "recommended". |
| R2-FS-07 | Low | `docs/ISO26262-safety/software-safety-analysis.md:107` (PA-05) | PA-05 names "frames are moved on growth" as the safety measure against stale frame pointers. Since ADR 0003 the frame stack is reserved once at its exact size and never grows, so the row describes a mechanism that no longer exists, and its gap column still waits for a test of it. | Rewrite the row for the exact reservation (`parser_stack::reserve_exact`, `push` refusing beyond capacity) and name the existing units. |
| R2-FS-08 | Info | `docs/ISO26262-safety/seooc-assumptions.md` ("What each ASIL requires") | The tables summarize licensed ISO 26262 tables from memory and say so. The reviewer has no licensed text either. Two rows look simplified: the independence row gives one level per ASIL although the standard sets it per confirmation measure, and "static code analysis" is shown as only advised at ASIL A. | Have the tables checked once against the licensed standard, or mark them more clearly as illustrative. |

## Step 5 results — ISO/SAE 21434 findings

The cybersecurity baseline from #139 (assumptions, TARA, plan, verification, case,
manual, interface agreement, `SECURITY.md`) is consistent in itself and clearly marks
what is planned. The review's sanitizer and fuzz runs again found nothing in production
code. Stale references to #124 are in R2-FS-01.

| ID | Severity | Location | Observation | Recommendation |
| --- | --- | --- | --- | --- |
| R2-CS-01 | Low | `docs/ISO21434-cybersecurity/tara.md:62` (T-03 controls) | The TARA names "slow lookups" as a denial-of-service route and counts on #124 for it. #124 fixed `member()` only; an attacker-supplied array still makes every `at(i)` loop quadratic (R2-LOG-02). | Keep the residual risk in the TARA and the cybersecurity manual until R2-LOG-02 is fixed. |
| R2-CS-02 | Info | `SECURITY.md` | Until private reporting is switched on, a reporter must open a public issue saying "possible security issue, please contact me". That announces that something exists before a fix does. GitHub private vulnerability reporting is a repository setting that needs no code or release. | Switch private vulnerability reporting on now and shorten the policy to that route. |

## Step 6 results — Architecture findings

| ID | Severity | Location | Observation | Recommendation |
| --- | --- | --- | --- | --- |
| R2-ARC-01 | **Medium** | `CMakeLists.txt:2`, `:121` (`SameMinorVersion`); `includes/adaptive_json5/error.hpp`; `README.md:61,157` | `main` still declares version **0.2.0**, the same as the released tag, although the public API has changed incompatibly since: `error` went from public fields to accessors, `export_utf8` now takes `std::byte*`, and `as_float`, `find_member`, `limit_reason()`, `access_reason()` and `internal_error` are new. `find_package(AdaptiveJSON5 0.2 CONFIG)` accepts a build of `main` as "0.2", and code written for one fails to compile against the other. The user guide on `main` (`docs/memory-and-limits.md`) describes the new API, while the README quick start pins the `0.2.0` tag that does not have it. | Bump the development version now (for example `0.3.0`), so packages built from `main` cannot be mistaken for the release, and say in the README which documentation matches which tag. #135 covers release notes, not the version number. |

## Step 7 results — Documentation findings

| ID | Severity | Location | Observation | Recommendation |
| --- | --- | --- | --- | --- |
| R2-DOC-01 | Low | `AGENTS.md:55-58`; `CONTRIBUTING.md`; `docs/ISO26262-safety/safety-plan.md:112` | `AGENTS.md` and `CONTRIBUTING.md` say in the present tense that pull requests "must pass" the sanitizer, fuzz, Clang and static-analysis jobs. Those jobs do not exist yet (#132, #141, #146 are open; the workflows are unchanged since 0.2.0). The safety plan states the real situation: unit tests, validation tests and formatting. An agent or contributor following `AGENTS.md` cannot meet the rule and may assume evidence that was never produced. | Until the jobs exist, word the rule as "once implemented (#132, #141, #146)", as the verification strategy already does. |

## Step 8 results — Full repository findings

No finding outside the open issues. The CI workflows, CMake files, PR template and
tools were re-read; everything observed is covered by #130, #131, #132 and #133. New
since 0.2.0 and in good order: the import-helper tests (`tools/tests/test_import_release.py`),
the release-tag check, `AdaptiveJSON5_hosted_FOUND`, and the software-unit ID registry.

## Step 9 results — Summary

**Overall.** The three fixes merged since 0.2.0 work, and the new number-conversion
code is correct by review, by 2652 unit tests and by 12.3 million fuzz inputs. No High
finding. The Medium findings fall into two groups: two behaviours the fixes introduced
or left behind (rounding-mode dependence, quadratic `at()` loops), and safety and
security documents that were not updated when the code changed.

| Area | High | Medium | Low | Info | Total |
| --- | ---: | ---: | ---: | ---: | ---: |
| Logic (R2-LOG) | 0 | 2 | 2 | 0 | 4 |
| Coding standards (R2-STD) | 0 | 0 | 1 | 0 | 1 |
| ISO 26262 (R2-FS) | 0 | 4 | 3 | 1 | 8 |
| ISO/SAE 21434 (R2-CS) | 0 | 0 | 1 | 1 | 2 |
| Architecture (R2-ARC) | 0 | 1 | 0 | 0 | 1 |
| Documentation (R2-DOC) | 0 | 0 | 1 | 0 | 1 |
| Full repository (R2-REPO) | 0 | 0 | 0 | 0 | 0 |
| **Total** | **0** | **7** | **8** | **2** | **17** |

**Suggested order**

1. R2-LOG-01 with R2-FS-03: decide whether the Clinger step stays; this is the only
   finding where a wrong value can be returned as success.
2. R2-FS-01: bring the safety and cybersecurity documents back in line with the code.
3. R2-ARC-01: bump the development version.
4. R2-LOG-02 with R2-CS-01: linear array iteration for `json5_value`.
5. R2-FS-02 and R2-FS-04, then the Low items.

**GitHub issues**

Created on 2026-11-16 in milestone M4, as part of #117. Each finding is covered by
exactly one issue.

| Issue | Number | Subject | Findings covered |
| --- | --- | --- | --- |
| R2-01 | [#153](https://github.com/Romanr92/Adaptive_JSON5/issues/153) | `as_double()`/`as_float()` follow the caller's rounding mode | R2-LOG-01, R2-FS-03, R2-STD-01 |
| R2-02 | [#154](https://github.com/Romanr92/Adaptive_JSON5/issues/154) | Linear array iteration for `json5_value` | R2-LOG-02, R2-CS-01 |
| R2-03 | [#155](https://github.com/Romanr92/Adaptive_JSON5/issues/155) | Signed unknown words lack a range; default `max_members` unreachable | R2-LOG-03, R2-LOG-04 |
| R2-04 | [#156](https://github.com/Romanr92/Adaptive_JSON5/issues/156) | Safety and cybersecurity documents back in line with the code | R2-FS-01, R2-FS-07, R2-FS-08, R2-DOC-01, R2-CS-02 |
| R2-05 | [#157](https://github.com/Romanr92/Adaptive_JSON5/issues/157) | Storage fault state reachable from `json5_document` | R2-FS-02 |
| R2-06 | [#158](https://github.com/Romanr92/Adaptive_JSON5/issues/158) | Tool evidence: generator, option-OFF test leg, gcovr check | R2-FS-04, R2-FS-05, R2-FS-06 |
| R2-07 | [#159](https://github.com/Romanr92/Adaptive_JSON5/issues/159) | Bump the development version | R2-ARC-01 |

**Questions (no answer needed for this review)**

1. Should the library be independent of the caller's rounding mode (as before #124), or
   is "default rounding mode" an acceptable assumption of use?
2. Should `json5_value` get an iteration interface, or is `values()` with the advanced
   iterator the intended way to walk arrays?
3. Is `main` meant to become 0.3.0, and when does the version change: at the first
   incompatible commit or at release time?
4. `coding-guidelines.md` says no amendment to MISRA C++:2023 had been published on
   2026-11-07; the earlier ChatGPT review mentioned a technical corrigendum. Which is
   right could not be checked here.

**Further improvements**

- A unit that runs a sample of conversions under all four rounding modes and with
  `ADAPTIVE_JSON5_EISEL_LEMIRE` OFF and ON, comparing against the exact path.
- A small script that fails when a safety or security document still references a
  closed issue as open; it would have caught R2-FS-01.
- A differential fuzz target that compares the fast paths with the exact path for the
  same spelling.
