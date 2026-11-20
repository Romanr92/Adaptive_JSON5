# ADR 0008: Fundamental types and the text/byte boundary

Status: Accepted by the user, 2026-11-20, during
[#154](https://github.com/Romanr92/Adaptive_JSON5/issues/154). Replaces the type list in
the coding style's "Fundamental types" section, which now links here.

## Context

The coding style asked for `std::` type names everywhere, but kept six fundamental types
under their own names: `bool`, `char`, `char16_t`, `char32_t`, `float` and `double`.

Two things made this need an explicit decision:

1. **MISRA findings on #154.** `json5_value::as_string()` and `name()` write UTF-8 into a
   caller's `char` buffer through `export_utf8()`, which took `std::byte*`. Every pointer
   conversion between the two is a MISRA C++:2023 finding: `reinterpret_cast` violates
   Rule 8.2.5, and a `static_cast` through `void*` violates Rule 8.2.6.
2. **The owner's goal to use `std::` types only.** `char16_t` and `char32_t` have exact
   `std::` equivalents in width, while `char`, `bool`, `float` and `double` have none in
   C++17.

## Decision

### 1. Allowed fundamental types: `bool`, `char`, `float`, `double`

Production code, tests and examples use `std::` type names. Only these fundamental types
keep their own names:

| Type | Why it stays |
| --- | --- |
| `bool` | No `std::` alternative. |
| `char` | Text: string literals and caller buffers for UTF-8 text. No `std::` alternative. |
| `float`, `double` | Results of `as_float()` / `as_double()` and the floating conversion (J5-037, ADR 0005). `std::float32_t` / `std::float64_t` only exist from C++23. |

`int` stays only where the C++ standard requires exactly that type, with a comment at
that place, as the coding style lists (`int main()`, the `std::ldexp` exponent, and so on).

### 2. `char16_t` → `std::uint16_t`, `char32_t` → `std::uint32_t`

UTF-16 code units are `std::uint16_t` and Unicode code points are `std::uint32_t`
everywhere in the library, including the public `decoded_string::copy_units()` and
`decoded_string::unit_at()`. Character constants become hex values with the character in
a comment, for example `0x0AU // '\n'`.

`std::u16string_view` has no `std::uint16_t` equivalent (the standard defines no
`char_traits` for it), so the one internal use,
`detail::identifier_spelling_matches()`, takes a pointer and a length.

**Test exception:** tests may keep writing expected UTF-16 text as `u"..."` literals and
`std::u16string`, which is far more readable than lists of hex units. They convert it to
`std::uint16_t` with `adaptive_json5_test::utf16()` in
`tests/shared_infrastructure/utf16_units.hpp` before calling the library.

### 3. Text is `char`, raw memory is `std::byte`, and no pointer casts between them

Caller buffers that receive UTF-8 text are `char*`, so `as_string()` and `name()` can
return `std::string_view` and callers never need a cast. Raw memory (storage blocks,
allocation callbacks) stays `std::byte`. No code converts a pointer between the two.
Code that produces UTF-8 writes each byte value into the `char` buffer with
`static_cast<char>(value)`.

## Consequences

- **Public API change:** `decoded_string::copy_units()` takes `const std::uint16_t*`,
  `unit_at()` returns `std::uint16_t`, and `export_utf8()` takes `char*`. Callers that
  passed `char16_t` data must convert it: a `char16_t` value converts implicitly to
  `std::uint16_t`, a `char16_t*` pointer does not. Callers that exported into a
  `std::byte` buffer pass a `char` buffer instead.
- Tests check UTF-8 output in `char` buffers against `make_chars()` / `to_chars()` from
  `tests/shared_infrastructure/byte_array.hpp`.
- `export_utf8()` writes into `char*` (it took `std::byte*` before), so `as_string()` and
  `name()` pass the caller's buffer straight through. The library has no
  `reinterpret_cast` or `void*` cast for text buffers.
- Comparisons, shifts and widening of `std::uint16_t` values are written with an explicit
  `static_cast<std::uint32_t>`, because `std::uint16_t` promotes to signed `int`
  (MISRA C++:2023 integer promotion and numeric assignment rules). A local CodeQL MISRA
  run against the merge base showed no new findings, and about 220 fewer in total.
- MISRA C++:2023 Rule 8.2.5 and Rule 8.2.6 need no deviation for text output.
- Hex character constants are less readable than literals; the comment beside each one
  keeps the character visible.
- Moving to C++23 later would allow `std::float32_t` / `std::float64_t`; that would be a
  new decision.
