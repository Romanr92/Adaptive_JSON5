# Safety Manual for integrators

| Item | Value |
| --- | --- |
| Work product | Safety manual and assumptions of use for a SEooC (ISO 26262-10, clause 9) |
| Library | Adaptive_JSON5, parser library `AdaptiveJSON5::parser` |
| Issue | [#138](https://github.com/Romanr92/Adaptive_JSON5/issues/138) |
| Status | Draft, not reviewed and **not independently confirmed** (see C-4.3) |

This manual is for teams that want to use Adaptive_JSON5 in a safety-related
function. It can be read on its own: every item below is one short line that you
tick off for your project. Each line links to where the item is explained in detail.

**No compliance, certification or "verified" status is claimed.** The library is
developed with ISO 26262 in mind, as a Safety Element out of Context (SEooC). Your
project decides whether it can rely on it.

## How to use this checklist

1. Go through every item and decide for your project: **met**, **not met**, or
   **not applicable** (with a reason).
2. Any item that is **not met** means you cannot rely on the library's safety argument
   for that point. You need your own analysis or measure instead.
3. Keep the filled-in checklist with your own safety case.

---

## 1. Scope

- [ ] **C-1.1** Only the parser library `AdaptiveJSON5::parser` is in safety scope, at
  **ASIL B(D)** as one part of an ASIL D decomposition.
  → [SEooC assumptions, scope](seooc-assumptions.md#scope-of-these-assumptions)
- [ ] **C-1.2** The hosted add-on `AdaptiveJSON5::hosted` is **QM only** and is not used
  in your safety-related build (`ADAPTIVE_JSON5_BUILD_HOSTED=OFF`).
  → [A-05](seooc-assumptions.md#a-05-memory-no-heap-and-which-library-is-which)
- [ ] **C-1.3** You do not rely on a serializer. Serialization is not covered yet.
  → [SEooC assumptions, scope](seooc-assumptions.md#scope-of-these-assumptions)
- [ ] **C-1.4** Your HARA and technical safety concept confirm that ASIL B(D) for this
  library fits your item.
  → [Safety plan, tailoring](safety-plan.md#2-tailoring)

## 2. Assumptions of use

- [ ] **C-2.1 Platform.** Your target, compiler and build settings match a configuration
  you have verified, and you have checked stack use and execution time against your
  budget.
  → [A-01](seooc-assumptions.md#a-01-target-platforms)
- [ ] **C-2.2 Threads. ⚠️ BRING YOUR OWN MUTEX!** Each document, its buffer and every
  value, view or iterator from it is used by only one thread or task at a time. If
  anything is shared, **you** protect all access with your own mutex, not just single calls.
  → [A-02](seooc-assumptions.md#a-02-concurrency-one-thread-per-document)
- [ ] **C-2.3 Untrusted input.** You treat a successful parse as "syntactically valid",
  not as "correct data".
  → [A-03](seooc-assumptions.md#a-03-all-input-is-untrusted)
- [ ] **C-2.4 Limits.** You chose every value in `resource_limits` for your use case and
  did not rely on the defaults.
  → [A-04](seooc-assumptions.md#a-04-the-integrator-sets-the-resource-limits)
- [ ] **C-2.5 No heap.** You link only `AdaptiveJSON5::parser`, and any memory functions
  you supply (`storage_callbacks`) use storage reserved in advance, never the heap.
  → [A-05](seooc-assumptions.md#a-05-memory-no-heap-and-which-library-is-which)
- [ ] **C-2.6 Results.** You check every result the library returns, and on any error you
  reject the whole document and use your own safe reaction.
  → [A-06](seooc-assumptions.md#a-06-every-result-is-checked-and-any-error-rejects-the-whole-document)
- [ ] **C-2.7 Hardware and interference.** Your hardware and safety mechanisms handle
  random hardware faults, and you provide freedom from interference (memory
  protection, partitions, timing supervision) where other software runs alongside.
  → [A-07](seooc-assumptions.md#a-07-hardware-faults-and-freedom-from-interference)
- [ ] **C-2.8 Decomposition.** You provide the second, independent ASIL B(D) part:
  end-to-end protection on every message plus plausibility checks on the values you
  use. That part does not use this library or code shared with it.
  → [A-08](seooc-assumptions.md#a-08-decomposition-the-integrator-provides-an-independent-e2e-check)

## 3. Obligations from the safety analysis

- [ ] **C-3.1** The input stays unchanged while it is being parsed.
  → [FMEA IN-04](software-safety-analysis.md#input)
- [ ] **C-3.2** If staying in your fallback for a long time is dangerous for your
  function, you handle it, for example with a timeout into a safe state.
  → [FMEA LX-03](software-safety-analysis.md#lexer-and-unicode-handling)
- [ ] **C-3.3** You never modify a parsed document through the document mutators
  (`make_*`, `append_*`, `set_source_span`, `set_root` and similar).
  → [FMEA VM-06](software-safety-analysis.md#value-model-and-conversions)
- [ ] **C-3.4** You never use a moved-from document.
  → [FMEA AP-02](software-safety-analysis.md#public-api)
- [ ] **C-3.5** A document's buffer stays alive and reserved until the document is
  destroyed.
  → [FMEA AP-03](software-safety-analysis.md#public-api)
- [ ] **C-3.6** Every document has its own buffer, and the input does not overlap it.
  → [DFA-01](software-safety-analysis.md#2-dependent-failure-analysis)
- [ ] **C-3.7** You size `max_diagnostics` so that expected warnings do not make valid
  input fail.
  → [FMEA DG-03](software-safety-analysis.md#diagnostics)

## 4. Your own work

- [ ] **C-4.1 Compiler and tools.** You evaluate confidence in your own compiler, build
  options and toolchain. The library's evaluation covers only the configurations
  tested in its own CI.
  → [Tool confidence, compilers](tool-confidence.md#3-compilers), [DFA-03](software-safety-analysis.md#2-dependent-failure-analysis)
- [ ] **C-4.2 Target testing.** You run the library's tests, or your own, on your target
  with your compiler and options. The library's evidence comes from host machines only.
  → [Verification strategy, out of scope](verification-strategy.md#5-out-of-scope)
- [ ] **C-4.3 Independent reviewer. For now, you provide one.** The library's safety
  documents have not been independently confirmed. Until that changes, you do the
  confirmation reviews in your own project before relying on them.
  → [Safety plan, roles](safety-plan.md#1-roles)
- [ ] **C-4.4 Release.** You use a tagged release, and the safety documents of that same
  tag.
  → [Safety plan, change management](safety-plan.md#configuration-and-change-management)

## 5. Known limits and open issues

At the time of writing, these are known and tracked. Check their state for the
release you use.

| Topic | Effect for you | Tracked in |
| --- | --- | --- |
| `as_double()` uses up the caller buffer on repeated reads (LOG-01, High) | Later reads can fail for valid data | #124 |
| `as_double()` changes shared document state although it is `const` (LOG-02) | Concurrent reads of one document are not safe; C-2.2 applies | #124 |
| `member()` lookup is quadratic in object size (LOG-11) | Long lookup times on large objects | #124 |
| Document mutators are reachable through public headers (ARC-03) | Accidental modification possible; C-3.3 applies | #128 |
| Byte counter can wrap on a mismatched release (LOG-07) | Every later allocation fails | #125 |
| Limit errors do not say which limit or where (LOG-03), and other error-detail gaps | Harder diagnosis; the document is still rejected | #125 |
| Stack use not measured yet | You cannot yet check stack use against a documented figure (C-2.1) | #136 |
| Sanitizer, fuzz, Clang and coverage checks not implemented yet | Less verification evidence than planned | #132, #141 |

## 6. Where to find everything

All safety documents are in [`docs/ISO26262-safety/`](.) in the repository, at the tag
of your release: the [safety plan](safety-plan.md), [SEooC assumptions](seooc-assumptions.md),
[software safety requirements](../requirements.md#k-software-safety-requirements-iso-26262),
[software safety analysis](software-safety-analysis.md), [coding guidelines](coding-guidelines.md),
[verification strategy](verification-strategy.md), [tool confidence](tool-confidence.md)
and [traceability](traceability.md). This manual is checked against them in every
milestone safety review.
