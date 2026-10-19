# C++ coding style

This guide records the user's coding conventions for Adaptive_JSON5. The
`m_` member naming rule is an explicit user decision dated 2026-10-13. The
standards and conditional real-time/isolation guidance below
record the user's additional decision dated 2026-10-16. Mechanical formatting
is defined by the tracked [.clang-format](../.clang-format).

Apply this style to production guidance, illustrative snippets and test C++.
Agents still follow [AGENTS.md](../AGENTS.md): production implementation and
public headers belong to the human developer.

## Coding standards and applicability

For file placement and header/source separation, follow
[ADR 0002](adr/0002-public-private-source-layout.md). New guidance uses the agreed
public `includes/`, private `src/includes/` and topic-based source/test layout;
internal units use `src/tests/`, while public-interface/library units use
top-level `tests/`. The structural migration was explicitly user-authorized;
subsequent production features retain the normal human-owned workflow.

Follow **MISRA C++:2023** and the **SEI CERT C++ Coding Standard** when designing,
writing, teaching and reviewing C++ in this repository. MISRA C++:2023 is the
selected C++17 coding baseline; use the published CERT C++ 2016 edition with
its official errata as the reproducible CERT reference. Consult the official
online CERT material for clarifications, identifying the revision/access date
when relying on evolving guidance.

- [MISRA C++:2023, official publication](https://misra.org.uk/product/misra-cpp2023/).
- [SEI CERT C++ Coding Standard and published edition/errata](https://cmu-sei.github.io/secure-coding-standards/sei-cert-cpp-coding-standard/).

Apply the relevant rules to production C++, public headers, teaching examples
and test code. Identify host-only GoogleTest facilities and incorporated code
separately when assessing applicability; their use is not evidence that they
conform to the production coding baseline. Preserve upstream notices and versions.

Review the actual standard text before citing rule numbers or asserting that
a construct satisfies a particular rule. MISRA's licensed text is not reproduced
here. If a rule cannot be checked from an available authoritative source, record
that review as pending rather than inventing a rule interpretation or compliance
evidence. For necessary deviations, record the applicable rule/edition, affected
scope, technical reason and compensating measures in the relevant review or ADR.
Resolve conflicts explicitly against the repository's active contracts, including
C++17 portability and explicit non-throwing result/error APIs in every compiler
exception mode; do not silently weaken either the standard or a project contract.

Adoption is a coding obligation, not a claim that existing code has completed a
MISRA/CERT compliance assessment. Existing implementation requires review before
any such claim. Automated quality checks remain GoogleTest units and formatting;
this decision adds no static-analysis, certification or other automated gate.

## Conditions and compiler warnings

Use natural operand order, such as `count == 0`, `pointer != nullptr` and
`mode == syntax_mode::strict_json`. Yoda notation is no longer required, following
the user's revised decision dated 2026-10-16. Keep boolean conditions direct:
use `has_value()` or `!has_value()` rather than comparisons with boolean literals.
Preserve short-circuit safety by checking an optional or pointer before
dereferencing it. Avoid assignments inside conditions; compute the value first
and then test it.

Treat compiler warnings as errors for all configured project C++ targets,
including unit tests. GCC, Clang and AppleClang use `-Wparentheses -Werror`.
MSVC-compatible builds use `/W4 /WX /w14706`, explicitly enabling assignment-in-
condition warning C4706 at level 1. These diagnostics supplement source review;
they do not detect every accidental assignment or establish MISRA/CERT compliance.

The shared `adaptive_json5_configure_target()` CMake function applies these
options privately to project targets. New project C++ targets must use it.
The policy does not modify parent-project flags, exported consumer options or
GoogleTest's own build settings. It also applies when compiler exceptions are
enabled. Automated quality checks remain unit tests and formatting.

## Real-time profiles and freedom from interference

Where a consuming real-time environment requires it, follow **IEEE Std
1003.13-2003**, selecting and documenting the applicable **PSE51 (Minimal
Realtime System)** or **PSE52 (Realtime Controller System)** profile. Treat these
as platform integration constraints, not C++ syntax rules or a requirement to
add POSIX calls to the parser. Record the consumer's exact profile, platform
implementation/version and any restrictions or deviations before claiming
profile compatibility. See [The Open Group's POSIX profile information](https://posix.opengroup.org/index.html)
and [PSE52 conformance statement](https://posix.opengroup.org/docs/PX52.html).

The core remains OS-agnostic under [J5-101](embedded.md): it must also remain
usable without POSIX or an operating system. Any required POSIX adaptation lives
at the consuming project's boundary. Host unit results do not establish PSE51/
PSE52 conformance or a worst-case execution-time guarantee.

Apply **freedom from interference** when the consuming system's safety or
mixed-criticality requirements demand isolation. Identify the protected components
and possible interference through memory, execution time, shared resources and
communication. Define ownership and buffer boundaries, separate caller storage
and budgets where required, avoid uncontrolled shared mutable state, and specify
callback/concurrency contracts and failure containment. Keep resource exhaustion
in one context from corrupting another context or its results.

Document which guarantees the library supplies and which require the consuming
system's scheduler, memory protection, resource partitioning or other platform
mechanisms. Record applicable assumptions and actual evidence in the relevant
integration contract; this library alone cannot establish system-level freedom
from interference or safety certification. This guidance adds no OS dependency
or device/integration-test gate to the repository.

## Naming

| Element | Convention | Example |
| --- | --- | --- |
| Class data members | Prefix with `m_`; no trailing underscore | `m_error`, `m_byte_offset` |
| Namespaces | Lowercase snake_case | `adaptive_json5` |
| Types | Lowercase snake_case | `error_code`, `status_result` |
| Functions and parameters | Lowercase snake_case | `has_value`, `byte_offset` |
| Public aggregate struct fields | Current style uses snake_case without a member prefix | `code`, `has_byte_offset` |
| Enum values | Lowercase snake_case | `allocation_failure` |
| Header guards | Uppercase filename, punctuation replaced by underscores, trailing underscore | `ERROR_HPP_` |

The new member rule replaces names such as `error_` with `m_error`. Do not use
`m_error_`. Existing project-prefixed guards such as `ADAPTIVE_JSON5_ERROR_HPP_`
retain the trailing underscore. The prohibition on trailing underscores applies
to class data members, not header guards.

The `m_` rule does not prescribe camelCase for the remainder of the name;
preserve the current snake_case convention. It does not rename function
parameters or the public fields of the existing `error` aggregate. Revisit
those conventions only when the user requests a change. GoogleTest suite/test
names retain the requirement conventions in the unit-test skill.

## Layout and formatting

Use clang-format **18.1.8** with the tracked `.clang-format` as the authority for
mechanical formatting. Its current rules include:

- LLVM-based formatting with two-space indentation and continuation indentation.
- Allman braces: opening braces appear on their own lines.
- Indented namespace contents, access labels and class contents.
- A 120-column limit.
- Pointer/reference markers attached to the type: `const error*`, `error&`.
- Template declarations broken onto their own lines.
- No single-line `if` statements; only empty functions may remain on one line.
- Regrouped, case-sensitive sorted includes and indented preprocessor directives.

Keep standard includes such as `<cstddef>` and project includes such as
`"error.hpp"`; use only the headers required by the declaration. Close namespaces
with a descriptive comment. Terminate class and struct definitions with `};`.

Illustrative class layout:

```cpp
class status_result
{
  public:
    constexpr bool has_value() const noexcept
    {
      return !m_error.has_value();
    }

  private:
    std::optional<error> m_error;
};
```

This example shows formatting and naming only; it is not the completed P002 API.

## Formatting workflow

Zed's project settings format C/C++ buffers on save using clang-format and the
repository style. See [.zed/README.md](../.zed/README.md) for setup.

Check repository formatting with:

```sh
python tools/format.py --check
```

clang-format changes layout; it does not enforce or rename `m_` members. Apply
the naming rule while writing and reviewing code. No additional automated
quality gate is introduced.

For test comments, requirement delimiters, Arrange/Act/Expect sections and
traceability, follow the
[write-unit-tests skill](../.agents/skills/write-unit-tests/SKILL.md).

## Code documentation

The user requires the following comment convention from 2026-10-13 onward.
Document public types and functions, and meaningful internal declarations,
with Doxygen-style blocks. Use `@brief` for the purpose, `@par Requirements`
for the applicable P-step and J5 IDs, and `@note` for relevant behavior such as
ownership, lifetime, allocation, state or non-throwing guarantees. Add `@param`
and `@return` when they clarify the contract. Link only requirements the
declaration actually supports; comments are not evidence of full implementation
or verification.

```cpp
/**
 * @brief Non-throwing success/error result without a success payload.
 * @par Requirements
 * P002; J5-003; J5-045.
 * @note Failure information is stored inline without allocation.
 * error_if() returns nullptr on success. Otherwise, its pointer refers
 * to error storage owned by this result.
 */
```

For obscure or non-obvious code, place a `//` comment immediately above the
relevant line or block explaining why that approach is necessary. Explain the
constraint or reasoning, rather than paraphrasing the operation. Keep ordinary,
self-explanatory statements free of redundant comments.

```cpp
// Check the active alternative before dereferencing; success has no error.
return m_error ? &*m_error : nullptr;
```

Apply this convention to future teaching examples and authorized C++ work.
GoogleTest test declarations retain the unit-test skill's prescribed purpose
and Arrange/Act/Expect block comments rather than receiving duplicate blocks.
