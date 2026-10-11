---
name: teach-requirement
description: Explain an Adaptive_JSON5 J5 requirement and teach its design and unit verification. Use for requirement meaning, API design, implementation guidance or review; guide production C++ changes without editing production files.
---

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

## Contracts that shape guidance

- JSON5 is the planned default, with explicit strict JSON selection and complete
  input consumption. Syntax acceptance differs from schema or consumer rejection.
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
