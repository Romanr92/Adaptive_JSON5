# ADR 0006: Use of AI assistance in this repository

Status: Accepted by the user, 2026-11-15. Documentation only; no issue. Related:
[CONTRIBUTING](../../CONTRIBUTING.md), [AGENTS](../../AGENTS.md), `.agents/skills/`,
[traceability](../traceability.md) and [reviews](../reviews/).

## Context

Adaptive_JSON5 targets automotive and embedded software and documents its work against
ISO 26262 and ISO/SAE 21434 expectations. Those standards care about who made each
decision, what evidence backs it and whether a competent person reviewed it. AI coding
assistants are now a normal part of a developer's toolbox, and this repository already
uses them: `AGENTS.md` and `.agents/skills/` constrain how agents may act, and the
0.2.0 full repository review was produced on an agent branch. What was missing was one
place that states where AI is and is not used, and who is accountable for the result.

This record exists so that a reader, a reviewer or a future maintainer does not have to
guess. It describes the current practice honestly; it is not a marketing statement.

## Decision

### 1. Production code is written by the maintainer

All production logic under `core/src/`, `core/includes/` and `includes/` (the parser,
lexer, memory handling, value model, public headers and the hosted add-on) is written
by hand by the repository owner. AI tools are not used to generate, rewrite or refactor
production code.

The one exception to hand-writing is mechanically generated data, which is not AI
output. `core/includes/unicode/unicode_tables.hpp` is produced by the project-owned
script `tools/generate_unicode_tables.py` from pinned Unicode data, and its table
entries are not edited by hand. The script is rated as a TI2 tool in
[tool confidence](../ISO26262-safety/tool-confidence.md), and
`core/tests/unicode/unicode_tables_tests.cpp` checks the generated tables against the
Unicode data. The Eisel–Lemire table of [ADR 0005](0005-allocation-free-number-reads.md)
follows the same rule.

Evidence: this is the maintainer's own statement of practice. The git history and the
pull-request record show who authored and merged each change to production code. They
do not by themselves prove how the contents were written, so this decision rests on
the maintainer's word and is open to challenge in review. Where an AI review suggests a
change to production code, the maintainer decides whether it is right and implements
it himself.

### 2. Unit tests are AI-assisted and maintainer-reviewed

GoogleTest units under `tests/` and `core/tests/` are drafted with AI assistance. Two
things are kept apart:

- **What a test expects** comes from the requirements (J5-xxx), the JSON5
  specification and the design documents, never from reading the implementation.
- **What a test calls** follows [ADR 0002](../../core/docs/adr-0002-public-private-source-layout.md):
  units in `tests/` use only the public API; units in `core/tests/` may include private
  headers and exercise internal interfaces, such as the input cursor, against their
  documented contracts.

Taking the expected results from the requirements rather than the code is what makes
the tests traceable: each unit is recorded against its requirement in
[traceability](../traceability.md), and a test that only restates what the code does
would prove nothing.

Every generated test is read by the maintainer before it is committed. The maintainer
checks that the test exercises the intended requirement, that its expected values are
derived from the JSON5 specification or the design document rather than from the
implementation, and that it fails when the behaviour it guards is broken. Tests that do
not meet this bar are rewritten or dropped.

### 3. Build configuration is AI-assisted and maintainer-reviewed

CMake files, CMake presets, the Ninja-based build setup and the CI workflows are
written with AI help. The maintainer reviews each change, runs the configured presets
locally and relies on CI on Linux x86-64, Linux ARM64 and Windows MinGW as the
acceptance check.

### 4. Documentation is AI-assisted and maintainer-owned

Design documents, requirements, traceability, conformance notes, safety and
cybersecurity guidance, reviews and ADRs (including this one) may be drafted, edited or
restructured with AI assistance. The maintainer sets the content, verifies every factual
claim against the code and test results, and is the author of record.

### 5. The maintainer is accountable for everything that is merged

AI assistance does not change who signs off. Pull requests are reviewed and merged by
the maintainer. An AI-generated suggestion carries no more weight than any other
suggestion: it is accepted only when the maintainer understands it and agrees with it.

### 6. AI involvement is visible, not hidden

Agent constraints live in `AGENTS.md` and `.agents/skills/`. Branches created by agents
keep their agent-prefixed names. Reviews produced with AI assistance name the AI
reviewer in their header. This ADR is linked from `CONTRIBUTING.md`.

## Consequences

Positive:

- Accountability is unambiguous: one person owns the design and the production code,
  and that person has read every test and build change.
- Test coverage grows faster than a single developer could manage alone (over 3,400
  GoogleTest cases at 0.2.0) while every case still has a human reviewer.
- Build and CI work, which is repetitive and easy to get subtly wrong across three
  platforms, benefits from assistance without AI editing production C++. Build files
  can still change what is shipped (tool confidence rates CMake and Ninja TI2), so the
  safeguard is review and validation, see the mitigation below.
- Documentation stays current with the code because the cost of maintaining it is
  lower.
- The practice can be explained plainly to an assessor: AI is a drafting and review aid
  at the edges; the safety-relevant core is hand-written and reviewed by the
  maintainer. This is not safety verification: the software safety requirements stay
  Draft, and Verified status needs the independent confirmation measures described in
  [traceability](../traceability.md).

Negative, with mitigations:

- AI-drafted tests can be plausible but wrong, or can mirror the implementation instead
  of the specification. Mitigation: decision 2 requires expected values to be traced to
  the specification or design, and the traceability document records which units cover
  which requirement.
- AI-drafted build files can introduce options, dependencies or source-list changes
  that nobody asked for, and so build something different from the reviewed code.
  Mitigation: decision 3 requires review of every build change, a local run of the
  presets and green CI on all three targets before merge; the tests run on what CMake
  built, `build_configuration_tests` checks the build settings, and
  [dependencies](../dependencies.md) records provenance.
- Review discipline is a human habit and can slip. Mitigation: this ADR makes the rules
  explicit so that a lapse is a recognisable deviation rather than an unstated norm.

## Alternatives considered

- **No AI use at all.** Rejected: it would slow the test and documentation work
  considerably without improving the safety argument, since the safety-relevant code is
  already hand-written.
- **AI-generated production code with human review.** Rejected for this project: for a
  library that is meant to sit inside an embedded, safety-adjacent system, the
  maintainer wants to be able to explain every line of the parser from first principles,
  and generated code works against that.
- **Undocumented use.** Rejected: it would be inconsistent with the traceability and
  honesty the rest of the repository aims for.

## Review

This ADR is revisited when the practice changes, for example if a tool begins to be
used for production code, or when the project approaches a 1.0 release.
