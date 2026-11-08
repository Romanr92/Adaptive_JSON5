# Cybersecurity verification

| Item | Value |
| --- | --- |
| Work product | Cybersecurity verification of the component (ISO/SAE 21434, clause 10) |
| Issue | [#139](https://github.com/Romanr92/Adaptive_JSON5/issues/139) |
| Status | Draft, not reviewed. Most checks are planned, not implemented yet. |
| Scope | Parser library `AdaptiveJSON5::parser` |

This document says how the [cybersecurity requirements](../requirements.md#l-cybersecurity-requirements-isosae-21434)
and the controls from the [TARA](tara.md) are verified. Wherever the safety work already
verifies the same thing, it is **reused**, not repeated: the safety
[verification strategy](../ISO26262-safety/verification-strategy.md) is the base, and
this document only adds what is specific to security.

No claim is made that any requirement is verified yet.

## 1. Reused from the safety verification strategy

| ID | Method | Why it matters for security | Status |
| --- | --- | --- | --- |
| V1, V2 | Unit tests and validation tests over third-party fixtures | Show the shared CSRs (CSR-01 to CSR-05) through their SSRs | In place |
| V4 | Sanitizer runs: the tests rebuilt with AddressSanitizer and UndefinedBehaviorSanitizer, blocking every pull request | Detect memory corruption and undefined behaviour, the basis of T-01 | Planned (#132) |
| V5 | Fuzzing: a libFuzzer harness for a fixed time on every pull request, blocking on a crash, plus a longer weekly run | Finds input nobody thought of (T-01, T-03) | Planned (#132) |
| V8 | A blocking second compiler (Clang) | Guards against a single faulty or compromised compiler (T-05) | Planned (#141) |

## 2. Added for security

| ID | Method | What it shows | Threats | Blocks a PR? | Status |
| --- | --- | --- | --- | --- | --- |
| SV-1 | **Vulnerability scanning** of the pinned dependencies and CI actions, for example with Dependabot alerts or OSV-Scanner | No known vulnerability in what the library is built and tested with | T-05 | Alerts are handled as issues under `SECURITY.md` | Planned (#131) |
| SV-2 | **Security-focused fuzz targets**, in addition to the general harness: secure mode on and off, very long strings and numbers, deep nesting, many and duplicate members, zero bytes, control characters and unpaired surrogates | The specific TARA threats are exercised directly | T-01, T-02, T-03, T-06 | Yes, as part of V5 | Planned (#132; secure mode targets with #145) |
| SV-3 | **Static analysis**: clang-tidy (including the CERT C++ checks) and CodeQL with its security queries and the MISRA C++:2023 query pack from the CodeQL Coding Standards project | Code patterns that lead to vulnerabilities, and coding-standard violations | T-01, T-04, T-06 | Yes, for **new** findings; existing findings are tracked as a backlog | Planned (#146) |
| SV-4 | **Security review of the public API**, in every milestone security review | No API that interprets content, leaks memory contents, or invites misuse | T-02, T-04, T-06 | No | Planned |

## 3. Penetration testing

Penetration testing attacks a running system through its real interfaces. A library on
its own has no such system, so:

- **For the library**, the closest equivalent is fuzzing (V5, SV-2) together with the
  security review of the API (SV-4).
- **At system level**, real penetration testing is done by the **integrator**, on their
  ECU and vehicle, with this library inside. This is listed in the
  [cybersecurity manual](cybersecurity-manual.md).

## 4. AI-assisted review

An AI review with knowledge of MISRA or CERT can be used as an extra reviewer. It is
**not static analysis** and not a qualified tool: it is neither complete nor repeatable,
so no tool confidence can be argued for it. Like every AI review in this project, it does
not count as independent review (see the
[cybersecurity plan](cybersecurity-plan.md#1-development-setup-and-its-limits)).

## 5. Threats and how they are verified

| Threat | Verified by |
| --- | --- |
| T-01 Memory corruption | V1, V4, V5, SV-2, SV-3 |
| T-02 Ambiguous data | V1, V2, SV-2 (secure mode), SV-4 |
| T-03 Denial of service | V1 (limit boundary tests), V5, SV-2 |
| T-04 Information leak | V1 (diagnostic tests), SV-3, SV-4 |
| T-05 Supply chain | V8, SV-1, tool confidence self-checks (#142, #143) |
| T-06 Injection through strings | V1 (exact string tests), SV-2, SV-3, SV-4 |
