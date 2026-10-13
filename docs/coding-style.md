# C++ coding style

This guide records the user's coding conventions for Adaptive_JSON5. The
`m_` member naming rule is an explicit user decision dated 2026-10-13. The
remaining conventions describe the current headers and tracked
[.clang-format](../.clang-format); they do not introduce new API requirements.

Apply this style to production guidance, illustrative snippets and test C++.
Agents still follow [AGENTS.md](../AGENTS.md): production implementation and
public headers belong to the human developer.

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
