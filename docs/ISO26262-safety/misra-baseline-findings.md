# MISRA C++:2023 baseline findings: fix order and log

| Item | Value |
| --- | --- |
| Issue | [#166](https://github.com/Romanr92/Adaptive_JSON5/issues/166) Fix baseline MISRA C++:2023 findings |
| Related | [#146](https://github.com/Romanr92/Adaptive_JSON5/issues/146) static analysis gate; [#137](https://github.com/Romanr92/Adaptive_JSON5/issues/137) deviation register; [coding guidelines](coding-guidelines.md) |
| Baseline report | `misra-cpp` workflow artifact `misra-report-html`, CodeQL 2.27.5, `codeql/misra-cpp-coding-standards`, Clang 18 |
| Baseline totals | 869 results: 823 in project code, 46 in CMake-generated files; badge: critical 0, high 7, medium 0, low 0, unrated 862 |

This page orders the baseline findings **by rule**, from most to least severe, and records
each rule as it is fixed. Production C++ and public headers are changed by Romanr92; this
page, the workflow and the deviation register are documentation and CI work. A rule is
**Fixed** only when a later `misra-cpp` run reports zero findings for it, and **Deviated**
only when the owner approves an entry in the deviation register (#137).

## How the order was chosen

1. The only result with a security severity (CodeQL `cpp/suspicious-add-sizeof`, 8.8).
2. MISRA **required** rules whose violation is undefined behaviour or a use-after-move.
3. Required rules about memory, pointer casts and the non-throwing error contract.
4. Required rules about conversions and types.
5. Required rules about structure and API shape.
6. MISRA **advisory** rules; undecidable ones first, because they can reveal real defects.

Within a tier, rules that can break a safety requirement (SSR-02, SSR-08) come first, then
by number of findings. Counts below are project code only.

## Step 0: tooling findings (not project code)

46 results came from `build/misra/CMakeFiles/*/CompilerIdCXX/CMakeCXXCompilerId.cpp`,
CMake's compiler-detection source, which was compiled while CodeQL was tracing. The
`misra-cpp` workflow now configures CMake **before** `codeql-action/init`, so that file is
no longer traced.

| MISRA rule | CodeQL query | Generated-file results | Status |
| --- | --- | ---: | --- |
| 19.1.3 | `cpp/misra/identifiers-used-in-the-controlling-expression-of` | 11 | Open: confirm on next run |
| 6.0.3 | `cpp/misra/global-namespace-declarations` | 6 | Open: confirm on next run |
| 6.5.1 | `cpp/misra/external-linkage-not-declared-in-header-file-misra` | 5 | Open: confirm on next run |
| 19.0.2 | `cpp/misra/function-like-macros-defined` | 3 | Open: confirm on next run |
| 5.10.1 | `cpp/misra/poorly-formed-identifier` | 2 | Open: confirm on next run |
| 6.9.2 | `cpp/misra/avoid-standard-integer-type-names` | 1 | Open: confirm on next run |
| 19.3.1 | `cpp/misra/and-preprocessor-operators-should-not-be-used` | 1 | Open: confirm on next run |
| 6.7.2 | `cpp/misra/global-variable-used` | 6 (also has project findings below) | Open: confirm on next run |
| 7.0.3 | `cpp/misra/no-character-numerical-value` | 9 (also has project findings below) | Open: confirm on next run |
| 11.3.1 | `cpp/misra/variable-declared-array-type` | 2 (also has project findings below) | Open: confirm on next run |

## Fix order

### 1. Security, high severity

| # | MISRA rule | Rule | CodeQL query | Obligation | Findings | Files | Why here | Status |
| ---: | --- | --- | --- | --- | ---: | ---: | --- | --- |
| 1 | CWE-468 | Suspicious add with sizeof | `cpp/suspicious-add-sizeof` | security high (8.8) | 7 | 5 | Only high-severity result. `std::byte*` plus `sizeof(T)` is a correct byte offset, but each site must be confirmed and made explicit so the query stays clean. | Open |

### 2. Required: undefined behaviour and moved-from objects

| # | MISRA rule | Rule | CodeQL query | Obligation | Findings | Files | Why here | Status |
| ---: | --- | --- | --- | --- | ---: | ---: | --- | --- |
| 2 | 4.1.3 | No signed integer overflow | `cpp/misra/signed-integer-overflow` | required, undecidable | 35 | 2 | Undefined behaviour; can break SSR-08 memory bounds. | Open |
| 3 | 8.7.1 | Valid pointer/index arguments to `<cstring>` functions | `cpp/misra/pointer-argument-to-cstring-function-is-invalid` | required, undecidable | 1 | 1 | Out-of-bounds read or write if wrong (SSR-08). | Open |
| 4 | 28.6.3 | No use of a potentially moved-from object | `cpp/misra/object-used-while-in-potentially-moved-from-state` | required | 55 | 13 | Use-after-move; touches ownership (J5-040, J5-051). | Open |

### 3. Required: memory, pointers and the non-throwing contract

| # | MISRA rule | Rule | CodeQL query | Obligation | Findings | Files | Why here | Status |
| ---: | --- | --- | --- | --- | ---: | ---: | --- | --- |
| 5 | 8.2.6 | No integral or `void*` to object pointer cast | `cpp/misra/int-to-pointer-cast-prohibited` | required | 26 | 13 | Pointer provenance and alignment in arenas and storage. | Open |
| 6 | 8.2.5 | No `reinterpret_cast` | `cpp/misra/reinterpret-cast-shall-not-be-used` | required | 5 | 3 | Same storage code as 8.2.6; fix together or record a deviation. | Open |
| 7 | 21.6.3 | No advanced memory management (placement new etc.) | `cpp/misra/advanced-memory-management-used` | required | 7 | 3 | Placement construction in caller buffers is by design; likely a deviation (#137). | Open |
| 8 | 24.5.2 | No `memcpy`/`memmove`/`memset` | `cpp/misra/no-memory-functions-from-c-string` | required | 7 | 5 | Raw memory copies; replace or deviate. | Open |
| 9 | 18.4.1 | Exception-unfriendly functions are `noexcept` | `cpp/misra/exception-unfriendly-function-must-be-noexcept` | required | 13 | 11 | Non-throwing error contract. | Open |

### 4. Required: conversions and types

| # | MISRA rule | Rule | CodeQL query | Obligation | Findings | Files | Why here | Status |
| ---: | --- | --- | --- | --- | ---: | ---: | --- | --- |
| 10 | 7.0.5 | Promotions must not change signedness | `cpp/misra/no-signedness-change-from-promotion` | required | 9 | 1 | Silent sign changes in big-number arithmetic. | Open |
| 11 | 7.0.4 | Appropriate operands for bitwise and shift operators | `cpp/misra/inappropriate-bitwise-or-shift-operands` | required | 4 | 1 | Shift and mask correctness in float conversion. | Open |
| 12 | 7.0.6 | Appropriate numeric assignments | `cpp/misra/numeric-assignment-type-mismatch` | required | 13 | 5 | Narrowing or sign-changing assignments. | Open |
| 13 | 7.0.1 | No conversion from `bool` | `cpp/misra/no-conversion-from-bool` | required | 1 | 1 | Single site. | Open |
| 14 | 7.0.3 | Do not use the numerical value of a character | `cpp/misra/no-character-numerical-value` | required | 135 | 25 | Largest required rule; lexer/number code relies on character arithmetic. Needs one consistent pattern or a deviation. | Open |
| 15 | 5.13.4 | Unsigned literals have a `U` suffix | `cpp/misra/unsigned-integer-literals-not-appropriately-suffixed` | required | 24 | 15 | Mechanical. | Open |
| 16 | 8.2.2 | No C-style or functional casts | `cpp/misra/no-c-style-or-functional-casts` | required | 18 | 4 | Mechanical: use `static_cast`. | Open |
| 17 | 10.2.1 | Enumerations have an explicit underlying type | `cpp/misra/enumeration-not-defined-with-an-explicit-underlying-type` | required | 15 | 13 | Mechanical; public headers affected (ABI check). | Open |
| 18 | 10.2.3 | No numeric use of unscoped enum without fixed type | `cpp/misra/unscoped-enum-without-fixed-underlying-type-used` | required | 2 | 1 | Fixed together with 10.2.1. | Open |
| 19 | 6.9.1 | Same type aliases in all declarations | `cpp/misra/type-aliases-declaration` | required | 41 | 9 | Header and source declarations must match. | Open |

### 5. Required: structure and API

| # | MISRA rule | Rule | CodeQL query | Obligation | Findings | Files | Why here | Status |
| ---: | --- | --- | --- | --- | ---: | ---: | --- | --- |
| 20 | 0.1.2 | Use the value returned by a function | `cpp/misra/unused-return-value-misra-cpp` | required | 15 | 12 | Ignored results can hide failures (non-throwing error contract). | Open |
| 21 | 9.4.1 | `if ... else if` ends with `else` | `cpp/misra/if-else-if-end-condition` | required | 8 | 8 | Mechanical. | Open |
| 22 | 9.4.2 | Appropriate `switch` structure | `cpp/misra/appropriate-structure-of-switch-statement` | required | 7 | 4 | Parser dispatch. | Open |
| 23 | 15.0.1 | Special member functions provided appropriately | `cpp/misra/improperly-provided-special-member-functions` | required | 3 | 3 | Public value types. | Open |
| 24 | 15.0.1 (audit) | Special member functions (audit query) | `cpp/misra/improperly-provided-special-member-functions-audit` | required, audit | 55 | 42 | Review each; many may be accepted as-is. | Open |
| 25 | 0.2.2 | No unused named parameters | `cpp/misra/unused-parameter-misra-cpp` | required | 2 | 2 | Mechanical. | Open |
| 26 | 6.7.2 | No global variables | `cpp/misra/global-variable-used` | required | 1 | 1 | One project site (`power_of_five_table.cpp`); the rest were CMake files. | Open |

### 6. Advisory

| # | MISRA rule | Rule | CodeQL query | Obligation | Findings | Files | Why here | Status |
| ---: | --- | --- | --- | --- | ---: | ---: | --- | --- |
| 27 | 0.0.2 | Controlling expressions are not invariant | `cpp/misra/invariant-condition` | advisory, undecidable | 15 | 3 | May reveal dead code or wrong limits. | Open |
| 28 | 0.1.1 | No unnecessary writes to locals | `cpp/misra/unnecessary-write-to-local-object` | advisory, undecidable | 24 | 8 | May reveal logic errors. | Open |
| 29 | 11.6.1 | All variables are initialized | `cpp/misra/uninitialized-variable` | advisory | 3 | 3 | Cheap and safety-relevant. | Open |
| 30 | 8.0.1 | Parentheses clarify precedence | `cpp/misra/missing-precedence-clarifying-parenthesis` | advisory | 11 | 7 | Mechanical. | Open |
| 31 | 8.2.7 | No pointer to integral cast | `cpp/misra/no-pointer-to-integral-cast` | advisory | 3 | 2 | Alignment checks; likely a deviation. | Open |
| 32 | 8.20.1 | Unsigned constant operations do not wrap | `cpp/misra/unsigned-operation-with-constant-operands-wraps` | advisory | 187 | 39 | Largest count; mostly intended modular arithmetic. Fix or deviate per file. | Open |
| 33 | 9.5.1 | Legacy `for` statements are simple | `cpp/misra/legacy-for-statements-should-be-simple` | advisory | 32 | 16 | Loop shape. | Open |
| 34 | 11.3.1 | No variables of array type | `cpp/misra/variable-declared-array-type` | advisory | 11 | 6 | Unicode tables; `std::array` or deviation. | Open |
| 35 | 0.2.1 | Limited-visibility variables are used | `cpp/misra/unused-limited-visibility-variable` | advisory | 13 | 7 | Header constants in the lexer. | Open |
| 36 | 15.0.2 | Copy/move members have the proper signature | `cpp/misra/invalid-signature-for-special-member-function` | advisory | 7 | 6 | Owner types. | Open |
| 37 | 6.8.4 | Members returning references to `*this` are ref-qualified | `cpp/misra/member-functions-refqualified` | advisory | 2 | 1 | `json5_document` accessors. | Open |
| 38 | 10.1.1 | Pointer/reference parameters are `const` where possible | `cpp/misra/pointer-or-ref-param-not-const` | advisory | 2 | 2 | Mechanical. | Open |
| 39 | 4.1.2 | No deprecated implicit copy constructor | `cpp/misra/implicit-declaration-of-copy-constructor` | advisory | 2 | 2 | Declare explicitly. | Open |
| 40 | 4.1.2 | No redeclaration of `static constexpr` members | `cpp/misra/redeclaration-of-static-constexpr-data-member` | advisory | 1 | 1 | Single site. | Open |
| 41 | 6.5.2 | Internal linkage specified appropriately | `cpp/misra/internal-linkage-specified-appropriately` | advisory | 1 | 1 | Single site. | Open |

Status values: **Open**, **In progress**, **Fixed** (zero findings in a named run),
**Deviated** (approved entry in #137), **False positive** (reason recorded below).

## Fixed findings log

Add one row per rule when its status changes. Name the commit or pull request and the
`misra-cpp` run that shows zero findings. Do not mark a rule fixed without that run.

| # | MISRA rule | Findings before | Findings after | Change (commit / PR) | Verified by run | Notes |
| ---: | --- | ---: | ---: | --- | --- | --- |
| 0 | Generated files (7 rules) | 46 | — | `misra-cpp.yml`: configure before CodeQL init | — | Pending next run |
