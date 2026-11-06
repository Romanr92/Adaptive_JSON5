# Coding guidelines

| Item | Value |
| --- | --- |
| Work product | Coding guidelines (ISO 26262-6, clause 5, Table 1) |
| Issue | [#138](https://github.com/Romanr92/Adaptive_JSON5/issues/138); deviation register in [#137](https://github.com/Romanr92/Adaptive_JSON5/issues/137) |
| Status | Draft, not reviewed |

The way the library is coded is already written down in the
[C++ coding style](../../core/docs/design-coding-style.md) and enforced for layout by
the tracked [`.clang-format`](../../.clang-format). That guide remains the
authority for day-to-day coding. This document names the coding standards the code
follows for the safety work, in which edition, and how they relate to each other.

Adopting these standards is a coding obligation. It is not a claim that the existing
code has been assessed as compliant with any of them.

## 1. Selected coding standards

The editions below were checked on 2026-11-07.

| Standard | Edition used | Role | Notes |
| --- | --- | --- | --- |
| MISRA C++ | **MISRA C++:2023**, *Guidelines for the use of C++17 in critical systems* (October 2023) | Main rule set, the language subset | The current edition, written for C++17 (ISO/IEC 14882:2017). It replaces MISRA C++:2008 and absorbs the AUTOSAR C++14 guidelines, so AUTOSAR C++14 is not applied separately. No amendment had been published when the editions were checked. |
| SEI CERT C++ | **SEI CERT C++ Coding Standard, 2016 edition**, with its official errata | Security-focused rules, especially undefined behaviour, memory and lifetimes | The latest published edition. The online wiki is a development version; when relying on it, record the page and the date it was read. CERT C++ relies on the **SEI CERT C Coding Standard, 2016 edition** for rules shared with C, so those rules apply as well. |
| BARR-C | **BARR-C:2018**, *Embedded C Coding Standard* | Style rules that help prevent bugs | Written for C99, so it is applied **by analogy** with the tailoring in section 2. |

**Order of precedence.** When the rules conflict, MISRA C++:2023 comes first, then
CERT C++, then BARR-C:2018. The project's own contracts are never silently weakened:
C++17 portability and the non-throwing result and error API apply in every compiler
exception mode. Every conflict is resolved explicitly and recorded.

## 2. BARR-C:2018 tailoring

BARR-C:2018 is a C standard, so some rules cannot be applied literally to C++17.
They are tailored like this:

| BARR-C:2018 rule | Project choice | Reason |
| --- | --- | --- |
| §1.1 C99 as the language | C++17 | The library is a C++17 library. |
| §1.2 80-column line limit | 120 columns (`ColumnLimit: 120`) | Long C++ type and namespace names; enforced by clang-format. |
| §1.7 restrictions on the `auto` keyword | Not applied | In C++17, `auto` means type deduction, not the C storage class the rule is about. |
| §3.4 four-space indentation | Two spaces (`IndentWidth: 2`) | Existing project style; enforced by clang-format. |
| §6.2 single exit point (preferred practice) | Replaced by the fail-fast principle in section 3 | Deliberate project choice. |

All other BARR-C:2018 rules apply by analogy, for example braces on every block, one
statement per line, named constants instead of magic numbers, include guards on
every header, `default` in every switch, no `goto`, and fixed-width integer types.

## 3. Fail-fast principle

The code follows MISRA with a **fail-fast principle**: a function returns early on
the first failure and passes the error through unchanged. Resources are released by
their RAII owners on every early return, so an early exit never leaks or leaves
storage half-released.

MISRA C++:2023 has no single-exit rule, so fail-fast is not a MISRA deviation. It
does depart from BARR-C:2018 §6.2 and from the "one entry and one exit point"
principle in ISO 26262-6, Table 6, item 1a. The RAII rule above is the rationale for
that departure.

## 4. Topics ISO 26262-6 expects coding guidelines to cover

| Topic (ISO 26262-6, Table 1) | Where it is covered |
| --- | --- |
| Low complexity | BARR-C:2018 function length; complexity limits are still to be agreed (review finding STD-07). |
| Language subset | MISRA C++:2023. |
| Strong typing | MISRA C++:2023 type rules and BARR-C fixed-width types. |
| Defensive implementation | Checked results everywhere, explicit limits and the non-throwing error API. |
| Well-trusted design principles | ADRs and the [parser architecture](../../core/docs/design-architecture.md). |
| Unambiguous graphical representation | Not applicable: the code is not generated from models. Diagrams in the design documents use Mermaid. |
| Style guide | The [C++ coding style](../../core/docs/design-coding-style.md), `.clang-format` and BARR-C:2018. |
| Naming conventions | The naming section of the C++ coding style. |
| Concurrency | No locking and no shared mutable state in the library (SSR-05); the integrator provides locking (A-02). |

## 5. Deviations

Every deviation from these standards is recorded in the deviation register planned
in #137, with the rule, edition, affected code, technical reason and compensating
measures. The owner approves each deviation.

## 6. Open items

- Bring the [C++ coding style](../../core/docs/design-coding-style.md) in line with
  this document, adding BARR-C:2018 and the fail-fast principle. This belongs to #137,
  which already holds the fail-fast text on another branch.
- Agree complexity limits (STD-07).
- Start the deviation register (#137).
