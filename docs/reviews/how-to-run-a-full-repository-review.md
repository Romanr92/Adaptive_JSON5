# How to run a full repository review

This guide turns the
[0.2.0 full repository review](2026-11-06-full-repository-review-0.2.0.md) into a
repeatable procedure. Follow it step by step for the next review (for example the
three-reviewer re-review in #117), whether a person or an AI agent performs it.

**Outputs of one review:**

1. A review document `docs/reviews/<YYYY-MM-DD>-full-repository-review-<version>.md`.
2. Raw evidence (logs, probes, harnesses) kept **outside** the repository.
3. GitHub issues that cover every finding, in the agreed milestone.
4. One pull request that adds the review document.

---

## Ground rules

Read these before starting. Each comes from `AGENTS.md` or from a lesson of the
0.2.0 review.

- **Production code is read-only.** The reviewer does not change production C++ or
  public headers (`core/src/`, `core/includes/`, `includes/`). Findings are
  recommendations for the owner.
- **No new quality gates.** Sanitizer, fuzz, static-analysis and coverage runs are
  allowed as local review evidence only. Do not add them to CI unless the owner
  changes the scope.
- **Keep evidence out of the repository.** Probes, logs, fuzz corpora and build trees
  go to a scratch directory (`$REVIEW_OUT` below) or to an ignored `build/` folder.
- **Fix one baseline commit.** Every `path:line` in the document refers to it. If you
  later look at newer commits, say so next to the reference.
- **Take line numbers from the real file.** Use `grep -n` or an editor on the
  unmodified file. Never copy line numbers from a listing with comments stripped; in
  the 0.2.0 review this produced five wrong references that had to be corrected
  before publishing.
- **Reproduce before claiming.** A defect is "confirmed" only with a probe, test or
  tool output. Otherwise mark it "source review only".
- **No compliance claims.** Without the licensed MISRA/AUTOSAR text, rule numbers are
  "candidate mappings, to confirm". No ASIL, certification or "verified" status is
  implied by a review.
- **Respect recorded project decisions.** Read `core/docs/design-coding-style.md` and
  the ADRs first. When the owner states a convention during the review (in 0.2.0:
  "MISRA with a fail-fast principle"), re-rate the affected findings and add a
  revision note instead of silently editing them.
- **Number result sections after the plan steps.** "Step 4 results" must hold the
  Step 4 findings. In 0.2.0 an off-by-one numbering made a completed step look
  missing.

---

## Step 0 — Prepare the review

1. **Fetch and record the baseline.**

   ```bash
   git fetch --all --prune --tags
   ```

   ```bash
   git status --short && git log -1 --format='%H %ad %s' --date=short
   ```

   Record the commit, its version (`project(AdaptiveJSON5 VERSION …)` in
   `CMakeLists.txt`) and whether a release tag points to it.

2. **Create the evidence directory** outside the repository.

   ```bash
   export REVIEW_OUT="$HOME/review-$(date +%F)" && mkdir -p "$REVIEW_OUT/probe"
   ```

3. **Create the review document first**, before reading code, with:
   - a header table (date, baseline, branch, reviewer, status);
   - the plan (Steps 0–9 below) and a progress tracker table with a link column;
   - the method: severity scale, finding-ID prefixes, finding format, line-reference rule;
   - empty sections named `Step N results — …` for every step.

   Use this severity scale and these ID prefixes so reviews stay comparable:

   | Severity | Meaning |
   | --- | --- |
   | Critical | Wrong result, memory-safety defect or crash reachable from input; or a false, safety/security-relevant claim in the documents |
   | High | A defect or gap that a safety or security assessment would block on |
   | Medium | A rule deviation or design weakness with realistic impact |
   | Low | Maintainability, consistency or clarity |
   | Info | Observation or positive note |

   | Prefix | Area |
   | --- | --- |
   | `LOG` | Logic |
   | `STD` | Coding standards |
   | `FS` | ISO 26262 |
   | `CS` | ISO/SAE 21434 |
   | `ARC` | Architecture |
   | `DOC` | Documentation |
   | `REPO` | Build, CI, tests, tools, housekeeping |

   Finding format (one table row per finding): ID · severity · location (`path:line`) ·
   observation · evidence/impact · recommendation.

4. **Update the tracker after every step.** The owner may look at the document at any
   time.

## Step 1 — Read the sources of truth and record baseline evidence

1. Read `README.md`, `AGENTS.md`, `docs/requirements.md`, `docs/traceability.md`,
   `docs/conformance.md`, `docs/memory-and-limits.md`, `docs/embedded.md`, the ADRs,
   `core/docs/design-*.md`, `core/docs/requirements-quality.md` and the two skills in
   `.agents/skills/`.
2. Record the toolchain (`g++ --version`, `cmake --version`, `clang-format --version`).
3. Run the required checks exactly as CI does, in both exception modes (RTTI OFF):

   ```bash
   for p in debug-unit-tests debug-unit-tests-exceptions validation-tests validation-tests-exceptions; do cmake --preset "$p" && cmake --build --preset "$p" -j4 && ctest --preset "$p" -j4 > "$REVIEW_OUT/$p.ctest.log" 2>&1; tail -3 "$REVIEW_OUT/$p.ctest.log"; done
   ```

   ```bash
   python3 tools/format.py --check
   ```

4. Write the counts into "Step 1 results", including skipped cases. CTest counts a
   skipped case as passed; say so (in 0.2.0 the locale case was skipped on Linux).

## Step 2 — Logic review

1. **Read every production file in dependency order:**
   1. foundations: `common/`, `input/`;
   2. Unicode: UTF-8/UTF-16 and the generated tables;
   3. lexer: whitespace, line terminators, comments, trivia, punctuation, identifiers,
      strings and escapes, numbers, token boundaries, scalar and member-name lexers;
   4. memory: budgets, arena, accounted storage, owned blocks, storage owners;
   5. parser: engine (both passes), stack, dispatch, open/close, attachment, duplicates;
   6. value model: storage, document, views, iteration, lookup, numbers, strings;
   7. public API: `result`, `error`, options, `parser.hpp`, simple API, hosted parse,
      diagnostics.
2. **Check each area for:** grammar conformance (JSON5 1.0, RFC 8259, ES5.1 lexical
   rules), boundary values, integer overflow, off-by-one, malformed and truncated
   UTF-8, surrogates, error category and location, limit enforcement, lifetime and
   dangling views, hidden shared state, determinism, numeric exactness, and
   **algorithmic cost of read paths** (LOG-11 was found late in 0.2.0).
3. **Look at the API from the consumer side too.** Write the loop a real user would
   write (for example "read a value 100 000 times from a small buffer"). LOG-01 was
   found this way, not by reading the parser.
4. **Confirm every suspicion with a probe** compiled against the built library and
   kept in `$REVIEW_OUT/probe`:

   ```bash
   g++ -std=c++17 -fno-exceptions -fno-rtti -Iincludes "$REVIEW_OUT/probe/probe.cpp" build/debug-unit-tests/core/src/libadaptive_json5_parser.a -o "$REVIEW_OUT/probe/probe" && "$REVIEW_OUT/probe/probe"
   ```

   For performance claims, measure with an optimized build of the library sources, not
   the Debug archive, and state the host.
5. **Check test oracles** for each confirmed defect: why did the existing tests miss
   it? (In 0.2.0 the `as_double` test only checked `current_bytes()`.)
6. Record positive notes as well as findings.

## Step 3 — Coding standards

1. **Pattern sweep** over production code for exceptions, RTTI, `goto`, unions,
   variadics, heap calls, placement new, explicit destructor calls, casts, macros and
   `assert`/`abort`:

   ```bash
   grep -rnE '\b(throw|try|catch|dynamic_cast|typeid|goto|union|malloc|free|assert|abort)\b|reinterpret_cast|const_cast|::new \(|->~' core/src core/includes includes
   ```

2. **Extended warning build** of the library and hosted targets. It is review evidence
   only; `-k 0` keeps compiling after the first error:

   ```bash
   cmake -S . -B "$REVIEW_OUT/build-warn" -G Ninja -DADAPTIVE_JSON5_BUILD_TESTS=OFF -DADAPTIVE_JSON5_BUILD_HOSTED=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON "-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wold-style-cast -Wcast-align=strict -Wuseless-cast -Wdouble-promotion -Wnull-dereference -Wformat=2 -Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wimplicit-fallthrough -Wundef -Wswitch-enum -Wfloat-equal -Wzero-as-null-pointer-constant -Wextra-semi"
   ```

   ```bash
   ninja -C "$REVIEW_OUT/build-warn" -k 0 > "$REVIEW_OUT/warn.log" 2>&1; grep -oE '\[-W[a-z0-9=-]+\]' "$REVIEW_OUT/warn.log" | sort | uniq -c
   ```

3. **clang-tidy** with safety-relevant check groups, using the compile database from
   the warning build:

   ```bash
   for f in $(find core/src -name '*.cpp'); do clang-tidy -p "$REVIEW_OUT/build-warn" --quiet -checks='-*,bugprone-*,cert-*,cppcoreguidelines-*,hicpp-*,misc-*,performance-*,portability-*,readability-function-cognitive-complexity,readability-function-size,readability-magic-numbers,-misc-include-cleaner' "$f"; done > "$REVIEW_OUT/tidy.log" 2>&1
   ```

   Repeat with `-checks='-*,misc-no-recursion'` to confirm there is no recursion.
   Inspect samples of each warning class before counting it; `unchecked-optional-access`
   is mostly false positives behind `result::has_value()`.
4. **Complexity metrics** (HIS-style) with lizard installed into the evidence folder:

   ```bash
   python3 -m pip install --target "$REVIEW_OUT/pylib" lizard && PYTHONPATH="$REVIEW_OUT/pylib" python3 -m lizard core/src core/includes includes -l cpp --csv > "$REVIEW_OUT/lizard.csv"
   ```

   Report the function count, mean CCN, the number of functions above CCN 10/15/20
   and above 50/100 NLOC, and the top functions.
5. **Map findings to rules** as candidates only: MISRA C++:2023 (primary), SEI CERT C++
   (secondary), AUTOSAR C++14 (C++14 only, supplementary), BARR-C:2018 (C99, by analogy
   after tailoring), HIS metrics. Include the standards-applicability table from the
   0.2.0 review and update it if the project scope changed.
6. **Apply project conventions** before rating: fail-fast early returns are intended
   (not a MISRA C++:2023 deviation), naming and formatting follow the coding-style guide.

## Step 4 — ISO 26262 gap analysis

1. Search for any safety claim (`ASIL`, `26262`, `SEooC`, `safety manual`). If none
   exists, write the section as a **gap analysis** for use as a Safety Element out of
   Context. A library has **no HARA of its own**; the integrator's HARA sets goals and
   ASIL.
2. Check, and map each finding to a plan sub-item:
   - safety scope and assumptions of use;
   - requirement attributes, status, atomicity and traceability (count rows without
     unit tests);
   - architecture documentation, software FMEA and dependent-failure analysis;
   - unit design principles of ISO 26262-6 Table 6 (single exit, pointers, hidden data
     flow, initialization, recursion, implicit conversion);
   - error detection, freedom from interference (memory, timing, stack);
   - unit verification: tested versus shipped configuration, structural coverage,
     test oracles, target execution;
   - tool confidence, configuration and change management, guideline deviations.
3. **Measure structural coverage** with the repository helper (local evidence only):

   ```bash
   python3 -m pip install --target "$REVIEW_OUT/pylib" gcovr && PYTHONPATH="$REVIEW_OUT/pylib" python3 tools/coverage.py --jobs 4
   ```

   Record line, function and branch coverage and the weakest files. State that MC/DC
   was not measured unless a tool for it was used.
4. Measure object sizes that drive stack use (for example `sizeof(parser_engine)`,
   `sizeof(error)`) with a probe that includes the private headers.

## Step 5 — ISO/SAE 21434 gap analysis

1. Describe the component context and attack surface: untrusted JSON5 input; the
   source, CI and release supply chain.
2. **Sanitizer run** of all unit and validation tests (review evidence only):

   ```bash
   cmake -S . -B "$REVIEW_OUT/build-asan" -G Ninja -DADAPTIVE_JSON5_BUILD_LIBRARY=OFF -DADAPTIVE_JSON5_BUILD_TESTS=ON -DADAPTIVE_JSON5_BUILD_VALIDATION_TESTS=ON -DADAPTIVE_JSON5_BUILD_HOSTED=ON "-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -O1" "-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined"
   ```

   ```bash
   cmake --build "$REVIEW_OUT/build-asan" -j4 && ctest --test-dir "$REVIEW_OUT/build-asan" -j4 --output-on-failure > "$REVIEW_OUT/asan.log" 2>&1; grep -E "runtime error|ERROR: (Address|Leak)Sanitizer" "$REVIEW_OUT/asan.log" | sort | uniq -c
   ```

   Separate production defects from test-harness artifacts (in 0.2.0 all reports came
   from incomplete global `operator new/delete` replacements in two test files).
3. **Fuzz run.** Write a libFuzzer harness in `$REVIEW_OUT/fuzz/` that parses with
   every option combination, walks the value tree, runs every numeric conversion and
   UTF-8 export, formats errors and exercises the simple API. Seed it with the
   committed fixtures and run it for at least ten minutes. On Ubuntu, clang needs
   `libclang-rt-18-dev` for the fuzzer runtime.

   ```bash
   clang++ -std=c++17 -g -O1 -fno-exceptions -fno-rtti -fsanitize=fuzzer,address,undefined -Iincludes -Icore/includes "$REVIEW_OUT/fuzz/fuzz_parse.cpp" $(find core/src -name '*.cpp' ! -path '*/hosted/*') -o "$REVIEW_OUT/fuzz/fuzz_parse"
   ```

   ```bash
   "$REVIEW_OUT/fuzz/fuzz_parse" "$REVIEW_OUT/fuzz/corpus" -jobs=4 -workers=4 -max_total_time=600 -max_len=8192 -timeout=10 -artifact_prefix="$REVIEW_OUT/fuzz/"
   ```

   Record executions, coverage and every crash, timeout or out-of-memory artifact.
4. Write a **TARA-lite** table: asset → threat → existing control → residual risk.
   Include configuration authenticity (the parser does not authenticate input). The
   item-level TARA belongs to the integrator.
5. **Supply chain:** list every `uses:` in `.github/workflows/` (tag versus SHA),
   runner images, unpinned package installs, workflow permissions, credential
   persistence, release checksums, signatures, attestations and SBOM.

   ```bash
   grep -nE "uses:|permissions|contents:|runs-on|apt-get|update: true|pip install" .github/workflows/*.yml
   ```

6. Check vulnerability handling: `SECURITY.md`, advisories, supported versions,
   known-anomaly lists.

## Step 6 — Architecture

1. Public headers must not include the private tree:

   ```bash
   grep -rnE '#include "(common|input|lexer|memory|parser|unicode|diagnostics)/' includes
   ```

2. Every public header must compile on its own:

   ```bash
   for h in $(cd includes && find adaptive_json5 -name '*.hpp'); do echo "#include \"$h\"" > "$REVIEW_OUT/t.cpp"; g++ -std=c++17 -fno-exceptions -fno-rtti -Wall -Wextra -Iincludes -fsyntax-only "$REVIEW_OUT/t.cpp" || echo "FAIL $h"; done
   ```

3. Review the layering against ADR 0002 and the architecture page, the storage model
   against ADR 0003, the simple API against ADR 0004, the CMake export/install and
   package config, the public API surface, ABI policy and optional components.
4. Look for read paths that touch shared mutable state (`const` methods that allocate),
   which is how ARC-01 was found.

## Step 7 — Documentation

1. **Check relative links and anchors** in all tracked Markdown files with a small
   script: resolve each `](path#anchor)` against the file tree and the target's
   headings (GitHub slug rules: lowercase, punctuation removed, spaces become `-`).
2. **Compile and run the README quick start** in both memory modes (hosted and caller
   buffer) and compare the output with the documented output.
3. **Compare documented behavior with the code** established in Step 2: limits,
   error categories, thread safety, diagnostics, sizing advice.
4. Look for stale status statements, hard-coded test totals, duplicated paragraphs,
   moved paths and contradictions between requirements, ADRs and guides.

## Step 8 — Full repository

1. **CMake:** options, presets, install/export, effects on a parent project.
2. **CI:** triggers (pull request only, or also `main` and a schedule), build types of
   test jobs versus release packaging, matrices, skipped tests.
3. **Tests:** placement rules (`core/tests/` versus `tests/`), include boundaries,
   helpers, sanitizer compatibility of test-only allocator replacements.
4. **Tools:** run what CI does not run.

   ```bash
   python3 -m unittest discover -s tools/tests
   ```

   ```bash
   python3 tools/generate_unicode_tables.py --output "$REVIEW_OUT/unicode_tables.hpp" && cmp "$REVIEW_OUT/unicode_tables.hpp" core/includes/unicode/unicode_tables.hpp && echo identical
   ```

   ```bash
   cmake --preset library && cmake --build --preset library && cmake --install build/library --prefix "$REVIEW_OUT/install"
   ```

   Package the release tag with the packaging script (in a worktree of the tag) and
   import the result with `cmake/ImportAdaptiveJSON5.cmake`, including a deliberately
   wrong tag. In 0.2.0 this found both the packaging inventory failure and REPO-15.
5. **Licensing and provenance:** `LICENSE`, `NOTICE`, `LICENSES/`, fixture
   provenance, SPDX headers.
6. **Hygiene:** `.gitattributes`, `.gitignore`, editor configuration, historical
   snapshots, PR and issue templates.

## Step 9 — Consolidate

1. Fill "Step 9 results": overall assessment, a summary table (severity × area with
   totals), prioritized recommendations, questions that need no answer during the
   review, and further improvements.
2. **Verify the document mechanically** before handing it over:
   - no placeholder text left (`To be filled`, `PLACEHOLDER`);
   - the per-area counts in the summary equal the rows in the finding tables;
   - finding IDs are unique;
   - all in-document anchors resolve;
   - every `path:line` exists at the baseline and points at the described code
     (spot-check with `git show <baseline>:<path> | sed -n '<line>p'`).
3. Commit the document to the review branch.

## Step 10 — Re-check and compare (when needed)

1. **Newer commits:** if branches or tags moved during the review, compare each with
   the baseline (`git diff --stat <baseline> <ref>`), state whether production code
   changed, and add a follow-up section with a status per affected finding.
2. **Other reviewers** (the #117 process uses three independent reviews): for each of
   their items, re-check it at **your** baseline before copying anything. Record every
   item's disposition (already covered, added as a new finding, obsolete because the
   code or document changed). Copy only items that are both valid and missing.
3. Add a dated revision note at the top for every change after the first complete
   version, including owner decisions.

## Step 11 — Turn findings into issues

1. **Group findings into issues** by subsystem and remedy, so that every finding maps
   to **exactly one** issue. Add a finding-to-issue table and check it with a script.
2. **Respect the workflow rule** of at most three active issues per subsystem: plan
   waves and open the first wave first.
3. **Write bodies in the repository templates** (`Bug report`: expected/actual,
   reproduction, version; `Implementation task`: requirement IDs, behavior and scope,
   unit-test acceptance criteria). Add the covered findings and a responsibilities
   section (owner: production C++ and public headers; AI: tests, CMake, CI, docs).
4. **Agree the order of work** with the owner:
   1. fix confirmed defects that are wrong under any scope;
   2. decide the safety/security scope (QM or SEooC with an assumed ASIL; input trust);
   3. write the SEooC and out-of-context security assumptions before re-triaging;
   4. re-triage the rest and run the next re-review on a frozen commit.
5. **Create the issues only on the owner's request**, in the milestone the owner names,
   with the proposal ID in the title (`[Bug] I-01 — …`), the agreed assignees and a
   link to the review document. Record the GitHub numbers in the document.

## Step 12 — Publish

1. Move the document to `docs/reviews/<YYYY-MM-DD>-full-repository-review-<version>.md`
   and update self-references and links in the created issues.
2. Open one pull request with the review document (and any updates to this guide),
   following `.github/pull_request_template.md`. The PR changes documentation only;
   say so.
3. Hand over: the PR link, the issue list, the totals, and the open questions.

---

## Checklist

- [ ] Baseline commit recorded; evidence directory outside the repository
- [ ] Review document created first, with plan, tracker and method
- [ ] Step 1 baseline: units and validation in both exception modes, formatting
- [ ] Step 2 logic: every production file read; suspicions reproduced; consumer loops tried
- [ ] Step 3 standards: sweep, extended warnings, clang-tidy, recursion, metrics, mapping
- [ ] Step 4 ISO 26262: gap analysis, coverage measured, plan-coverage map
- [ ] Step 5 ISO/SAE 21434: sanitizers, fuzzing, TARA-lite, supply chain, policy
- [ ] Step 6 architecture: include boundary, standalone headers, ADR consistency
- [ ] Step 7 documentation: links/anchors, quick start compiled, docs versus code
- [ ] Step 8 repository: CMake, CI, tests, tools, packaging and import, licensing, hygiene
- [ ] Step 9 consolidation: counts, IDs, anchors and line references verified
- [ ] Step 10 re-check of newer refs and other reviewers' findings (if any)
- [ ] Step 11 issues: one issue per finding group, waves, order of work, owner approval
- [ ] Step 12 publication: document in `docs/reviews/`, pull request opened
