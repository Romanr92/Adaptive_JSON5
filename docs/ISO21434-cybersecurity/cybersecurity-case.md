# Cybersecurity case

| Item | Value |
| --- | --- |
| Work product | Cybersecurity case (ISO/SAE 21434, clause 6) |
| Issue | [#139](https://github.com/Romanr92/Adaptive_JSON5/issues/139) |
| Status | **Draft structure only.** The argument is laid out; most of the evidence does not exist yet. |
| Scope | Parser library `AdaptiveJSON5::parser`, developed towards CAL 4 |

The cybersecurity case is the argument that ties every claim to its evidence. Today it
shows **where the evidence is still missing** more than it proves anything. It is kept
up to date in every milestone security review, until each line has real evidence.

> **⚠️ No claim in this document is achieved.** The independent review and assessment
> that a cybersecurity case needs have not been done, and an AI review does not count
> (see the [development setup and its limits](cybersecurity-plan.md#1-development-setup-and-its-limits)).

## 1. Top-level claim

**Used as described in the [cybersecurity manual](cybersecurity-manual.md), the parser
library does not add unreasonable cybersecurity risk to an item that needs up to CAL 4.**

This claim rests on the [cybersecurity assumptions](cybersecurity-assumptions.md) being
true for the integrator's item. It is supported by the claims below.

## 2. Supporting claims

Status values: **Evidence exists** (evidence is there, but not independently reviewed),
**Partial** (some evidence, gaps listed) and **Missing** (no evidence yet).

| ID | Claim | Argument | Evidence | Status |
| --- | --- | --- | --- | --- |
| CC-1 | Crafted input cannot corrupt memory or take control (T-01) | Every access is bounds-checked, there is no heap and no recursion (CSR-01, CSR-05), and independent dynamic checks look for memory errors | Unit and validation tests (V1, V2); sanitizer runs and fuzzing (V4, V5, SV-2); static analysis (SV-3) | **Partial**: tests exist; sanitizers, fuzzing and static analysis are missing (#132, #146) |
| CC-2 | Ambiguous or tampered data is not accepted as valid (T-02) | Every failure is an explicit error (CSR-03), values are exact (CSR-04), duplicates are never resolved silently, and secure mode is offered (CSR-07) | Unit tests, including `J5_032_Json5Document.RejectsDuplicateNamesWhenConfigured` | **Partial**: secure mode is not implemented yet (#145) |
| CC-3 | Crafted input cannot exhaust time or memory (T-03) | Every resource limit is enforced and inclusive (CSR-02), and a work budget bounds the run time | Limit boundary unit tests; fuzzing (V5) | **Partial**: fuzzing is missing (#132); two known denial-of-service gaps (#124) |
| CC-4 | No data leaks through the library (T-04) | Diagnostics carry only codes, positions and ranges (CSR-08), and a copy of the input is kept only on request | Diagnostic unit tests; API security review (SV-4) | **Partial**: the API security review has not been done yet |
| CC-5 | The library is built from its reviewed source (T-05) | Pinned dependencies and actions, a second compiler, signed or attested releases, and vulnerability scanning | Tool confidence evaluation; Clang job (V8); release attestation and SBOM; SV-1 | **Missing**: #131, #141 |
| CC-6 | String content can never execute inside the library (T-06) | The library never interprets values or member names (CSR-06), and strings keep their exact length | Unit tests, including `J5_029_Json5Value.KeepsEmbeddedZeroBytesAtFullLength`; static analysis (SV-3) | **Partial**: static analysis is missing (#146) |
| CC-7 | The integrator knows their obligations | Every obligation is a checklist item in the cybersecurity manual, which is the basis of the interface agreement | [Cybersecurity manual](cybersecurity-manual.md), [interface agreement template](cybersecurity-interface-agreement.md) | **Evidence exists** (Draft) |
| CC-8 | Vulnerabilities found after release are handled | A reporting route, supported versions, and advisories | [`SECURITY.md`](../../SECURITY.md) | **Partial**: private reporting is not switched on; response targets come with 1.0.0 |
| CC-9 | The work was independently reviewed and assessed | An independent reviewer and assessor confirm the plan, the analysis and this case | Review and assessment reports | **Missing**: no independent reviewer yet; integrators provide their own |

## 3. How this case is maintained

- Each milestone security review updates the status column and links new evidence.
- Each pull request that touches a CSR or a cybersecurity assumption checks whether a
  claim here is affected (the safety and security impact check).
- A release may only describe a claim as supported when its status is **Evidence exists**
  and an independent reviewer has confirmed it.
