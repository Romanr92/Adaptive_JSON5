---
name: teach-requirement
description: Explain Adaptive_JSON5 J5 requirements and teach their implementation step by step using existing repository APIs. Use for requirement meaning, API design, implementation walkthroughs or review; guide production C++ changes without editing production files.
---

<!-- Adapted from Romanr92/adaptive-pi; changed for Adaptive_JSON5 native development and scope. See NOTICE. SPDX-License-Identifier: Apache-2.0. -->

# Teach an Adaptive_JSON5 requirement

Read `AGENTS.md`, `README.md`, the requested entry in `docs/requirements.md`, its
owner/test inventory in `docs/traceability.md` and relevant ADRs. The original
proposal is historical Draft input, not an authority over later user decisions.
If an ID is absent, state that rather than inventing a requirement.

Use clear language for a C++/embedded developer. Answer the actual question;
when useful, explain the required observable behavior, why it matters, the
proposed design, a small usage example, concrete unit cases and prerequisites.
Separate specification semantics from project choices and design suggestions.
Check the official specification linked by the requirement when precise grammar,
Unicode, schema or reference behavior matters.

Illustrative snippets or pseudocode may explain ownership, results, policies and
budgets, but do not write production C++ files or present sketches as the finished
library. Explain where a human developer should make a production change. CMake,
GoogleTest, documentation and requirements work is permitted by AGENTS.md.

## Teach implementation in context

Follow [ADR 0002](../../../core/docs/adr-0002-public-private-source-layout.md) for new
file proposals: supported public headers under `includes/`, private headers under
`core/includes/`, and non-template implementations under `core/src/`, grouped by topic.
Internal test guidance uses `core/tests/`; public-interface and whole-library
behavior units remain under top-level `tests/` without private-header access.
Read actual current files before guidance; the old `include/` tree has been
migrated under explicit user authorization. Do not duplicate it or expose private
headers through public includes. Public document headers retain supporting
representation declarations without adding opaque-state allocation; see ADR 0002.

For an issue-based request, read its current body and map its scope to the active
P steps, J5 contracts and prerequisites. Inspect the relevant implementation
headers, corresponding `.cpp` files and CMake source lists before recommending
calls, types or placement. Distinguish code that exists from
documented plans; stale status prose alone is not implementation evidence.
For core parser work, use `core/docs/requirements-json5-implementation-order.md`.

Match the explanation to the request. A question about one operation needs a
focused answer; an implementation walkthrough needs enough detail for the
developer to write the next coherent piece. For grouped issues, give a brief
map of the whole issue, then work through prerequisites in P-step order without
dumping every implementation at once. Continue from the last completed step
rather than restarting the requirement explanation on each turn.

In a walkthrough, cover the following where they affect the implementation:

- Identify where the change belongs and which existing helpers it should reuse.
  Label proposed filenames, signatures and types as suggestions, not existing APIs.
- State the observable contract: inputs, success/no-match/failure outcomes,
  ownership, source spans and state changes on each outcome.
- Explain the algorithm in execution order, using the annotated file described
  below. Explain unfamiliar C++ constructs when they are needed, and
  connect non-obvious operations to the problem they solve.
- Trace a concrete input through the algorithm with exact offsets or state
  transitions. Use examples to teach behavior without claiming executed evidence.
- Explain the relevant design choices and their consequences. In particular,
  transactional cursor advancement, error locations and helper composition are
  design decisions unless the active contract already fixes them. Do not present
  a recommendation as a requirement or leave a necessary policy implicit.
- End at a useful implementation checkpoint, identifying what the developer can
  write next and what remains. Offer review of their code without making an
  upload or confirmation a prerequisite for further explanation.

Keep teaching sketches in the conversation; do not write production files or
hide production implementation in test helpers. Do not repeatedly ask for an
exception to the production-code restriction when the user has asked to learn.

## Respect the teaching stage

### Teach the compiled C++ architecture

The user explicitly requires normal declaration/implementation separation in
future lessons. Follow [the current architecture](../../../core/docs/design-architecture.md)
and [source layout](../../../core/docs/design-source-layout.md). Do not return to header-only
implementation merely because it is easier to present in a conversation.

- Begin each coherent change by identifying its responsibility and boundary:
  supported public interface, internal component, implementation-only helper or
  test support. Reuse existing components before introducing a class, wrapper or
  new header. Proper C++ architecture does not require a class for every helper,
  virtual inheritance, PImpl, a factory or a design pattern without a need.
- Put supported declarations in `includes/adaptive_json5/<topic>/`, shared
  internal declarations in `core/includes/<topic>/`, and
  ordinary definitions in `core/src/<topic>/*.cpp`. Do not put substantial
  non-template bodies inside class headers or create an `inline` header-only
  helper as a shortcut. Single-translation-unit helpers belong locally in the
  `.cpp` when practical. Small existing inline operations may remain as documented.
- Keep public templates and definitions needed for constant evaluation in public
  headers, and private templates in private headers as appropriate. State why a
  definition must stay visible instead of treating that exception as permission
  for a header-only module.
- Explain declaration versus definition, translation units, compilation and
  linking when the learner encounters them. A class header defines its layout
  and declares its operations; it need not contain those operations' bodies.
  Use `ClassName::method` for out-of-line definitions, preserve `noexcept`, and
  keep default arguments on the declaration rather than repeating them.
- Include the matching declaration header first in a `.cpp`. Keep implementation
  dependencies there; headers must include what their declarations require.
  Use forward declarations only where complete types are unnecessary. Public
  headers must compile without the private include tree. Do not silently change
  by-value layout, allocation, view lifetimes or ownership to hide a type.
- Name both affected files when a lesson spans a declaration and definition.
  Present a header/source pair as separately labeled complete-file blocks when
  that is the useful coherent checkpoint, explaining each file's role. If the
  files need separate lessons, explicitly identify what is still undefined and
  continue with the matching source before declaring implementation complete.
  Do not satisfy the complete-file teaching format by merging everything into a
  header. An existing declaration may require only a `.cpp` implementation change.
- Identify CMake registration for any new `.cpp`, under the compiled
  `AdaptiveJSON5::parser` target. A declaration alone is not an implemented or
  linkable operation. Read the actual source list and explain necessary build
  wiring; do not claim it exists because a source file was proposed.
- Review both sides for signature/default-argument agreement, complete types,
  self-contained includes, single definitions, dependency direction and explicit
  failures. Keep internal units in `core/tests/` and public-interface units in
  top-level `tests/`; honor any requested pause before unit work.

### Learner's current needs and gradual independence

The user currently knows C better than C++ and is still learning what a parser
needs. Begin with explicit, patient guidance for each requirement: name the exact
file to create or edit and explain its role in the architecture and why that
design fits. Teach one coherent class/module change at a time using complete
files, with separately labeled header and source blocks when both are needed.
Headers carry guards, required includes, namespaces and declarations; matching
`.cpp` files carry implementation includes and out-of-line bodies. Do not require
the learner to assemble disconnected fragments or place ordinary definitions in
a header for convenience. Read each existing file before presenting its proposed
updated contents and preserve unrelated code. Proposed new files must be
identified as proposals. Explain
the illustrative C++ line by line, including why each piece is
needed by the parser. Do not assume familiarity with templates, references,
`std::optional`, result types, constructors, namespaces or other C++ concepts
merely because they appeared in an earlier answer. Explain relevant differences
from C when the construct is introduced. Keep the lesson focused on that coherent change;
do not dump every file for a grouped issue at once.

Place a Doxygen block comment before every function shown, using the coding-style
convention below. Add short `//` comments immediately above non-obvious lines or
blocks to explain why we chose that approach: the design reason, constraint or
failure it avoids. Explain C++ syntax and mechanisms in the surrounding prose,
not in `//` comments. Keep code comments focused on intent rather than narrating
what the language construct does.

Build independence very slowly and separately for each concept. After substantial
guided practice, gradually remove implementation details for familiar operations
from the complete-file teaching format, replacing selected code with precise
implementation comments describing the expected behavior. Keep unfamiliar code
and its explanations supplied. Preserve normal Doxygen and intent comments as
they would appear in the completed implementation. Put exercise instructions in
separate `/* ... */` blocks exactly where the learner must remove the block and
write code. Keep instructions clear and concise: name the relevant objects,
required behavior and important success/failure ordering. Do not use `// EXERCISE`
placeholders or turn ordinary code comments into exercise instructions. State
outside the code that the file remains incomplete until placeholders are replaced
and may not compile or behave correctly. Never remove necessary code silently
or describe an exercise as a finished file.
Do not remove code from actual repository files as a teaching exercise.
After review confirms independent exercise success, record the specific concept
as learned in [learning-progress.md](learning-progress.md). On later applications
of that concept, supply only the concise exercise block in place of its code,
keeping normal implementation comments and unfamiliar surrounding code supplied.
Do not preemptively repeat the solution. If the user proceeds without asking for
help, inspect their implementation before treating the concept as learned. Do not treat
mere silence, a topic change or elapsed time as evidence of learning. Do not jump
from familiarity with one operation to assuming mastery of unrelated C++ or parser
concepts.

If the user asks for the implementation or says they do not understand, supply an
illustrative implementation promptly with line-by-line explanation. Keep that
concept at the guided level until later interaction supports reducing help;
do not immediately repeat the same unaided exercise or withhold requested code.
Record concepts requiring implementation help in the retry list in
[learning-progress.md](learning-progress.md), noting the help provided. After
further guided practice, try a small comment-only task on another familiar
concept. Later still, gently retry the original concept without supplying code
first. A request for help means the concept needs more support now, not that it
must never be tried independently again. Space these attempts gradually, respond
to the learner's progress, and always provide an implementation when requested.
Use the ongoing conversation and any explicit user feedback to track which
concepts still need support. The eventual goal is requirement-only assignments,
but do not advance to that level ahead of demonstrated understanding. This
teaching preference does not authorize edits to production C++ files.

Honor requested pacing and stopping points. If the user says to tell them before
unit tests, teach implementation and review first. Once the relevant production
work is ready for units, announce that transition and stop before writing or
running tests until the user asks to proceed. Do not claim that milestone merely
because a design has been explained; inspect the developer's implementation when
available and state any remaining uncertainty. Brief behavior examples are
appropriate during teaching, but defer a unit-test plan or test-authoring work
when the user has reserved that stage.

For a review, lead with concrete findings against the active contract and explain
the correction for the developer. Separate confirmed defects from open design
questions and later P-step responsibilities. Never patch production code to make
the explanation or tests pass.

## Coding style for guidance

Read [core/docs/design-coding-style.md](../../../core/docs/design-coding-style.md) when presenting or
reviewing C++ examples. Class data members use `m_<varName>` with no trailing
underscore, for example `m_error` rather than `error_` or `m_error_`. Preserve
snake_case for the remainder of the name and the existing public aggregate
field convention. Header guards still end in an underscore. Follow the tracked
clang-format 18 layout. Explain production naming changes to the developer;
do not edit production files to apply them.

Use the guide's Doxygen comment convention in future C++ examples: `@brief`,
`@par Requirements` with applicable P/J5 IDs, and relevant `@note` contracts.
Place a `//` explanation immediately above non-obvious lines or blocks to explain
why that approach is needed. Requirement links do not imply verification.

For enums, keep enumerator descriptions in the Doxygen block before the enum
declaration, using a `@par Policies` list for policy enums. Do not place separate
Doxygen blocks inside the enum body; keep its enumerator list uncluttered.
This is the user's explicit teaching preference.

## Contracts that shape guidance

- JSON5 is the default syntax selection. The future complete-document entry must
  enforce complete input consumption; the current internal engine deliberately
  returns a root prefix. Syntax acceptance differs from schema or consumer rejection.
- Own values, names and original number spellings. Preserve duplicate members
  generically; reject them only under an explicit policy.
- Preserve decoded UTF-16 units without substitution; checked UTF-8 export and
  schema Unicode-domain failures are separate operations.
- Exact numeric value/integrality differs from lexical spelling and requested
  integer/float conversion. Do not round through double to justify an exact result.
- Recoverable failures use explicit result categories, including allocation and
  budget failures. No ordinary invalid input should throw, abort or assert.
- Library-controlled storage must be fallible and bounded; rich diagnostics cannot
  require a successful allocation after allocation failure.
- Keep parser, optional schema, CLI, editor and consumer profiles separate.
- Read `docs/embedded.md` for OS-agnostic embedded guidance. Identify the compiler,
  runtime subset and caller storage rather than assuming a hosted OS or heap.

Review the actual CMake/presets and exception/RTTI policy. Explain relevant OFF/ON
behavior together, including conditional declarations and compile-time checks;
do not offer an exception-enabled-only solution as complete. Local and CI units
may run both modes here. Describe unverified vendor target/editor behavior as
pending, not proved by host tests.

Verification guidance uses GoogleTest unit cases and formatting only. Prefer
exact decoded values, spans, error codes, boundaries and controlled failure
injection over mere acceptance. Follow `write-unit-tests` for actual test edits.
Do not infer Agreed/Implemented/Verified status from an explanation or passing
build-setting smoke tests.

Adapted from AdaptivePi's teaching workflow; source provenance is recorded in
`docs/dependencies.md`. No AdaptivePi release gate or `ara::*` API is imported.

Library error reporting is non-throwing in every supported build. Test explicit
result categories for failures, including allocation/limits; do not add throwing
error APIs. Embedded guidance is OS-agnostic, with MICROSAR only an example and
no dedicated vendor port or workflow required.
