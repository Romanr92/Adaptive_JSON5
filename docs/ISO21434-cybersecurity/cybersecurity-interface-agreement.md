# Cybersecurity interface agreement (template)

| Item | Value |
| --- | --- |
| Work product | Template for a cybersecurity interface agreement (ISO/SAE 21434, clause 7) |
| Issue | [#139](https://github.com/Romanr92/Adaptive_JSON5/issues/139) |
| Status | Draft template |

A cybersecurity interface agreement records who does what between a supplier and a
customer. Here, the **library** is the supplier and your project, the **integrator**, is
the customer. Copy this template into your project, fill in the blanks, and keep it with
your cybersecurity case.

> **⚠️ The library is maintained by one developer as a personal, free-time project.** It
> has no support contract, no guaranteed response times before 1.0.0, and no independent
> assessment. The "library" column describes what the project does on its side, as is,
> without any warranty (see the [README disclaimers](../../README.md#disclaimers)).

## 1. Parties and scope

| Field | Value |
| --- | --- |
| Library | Adaptive_JSON5, parser library `AdaptiveJSON5::parser` |
| Library version (release tag) | _fill in_ |
| Integrator project and item | _fill in_ |
| CAL needed by the integrator's item | _fill in_ (the library is developed towards CAL 4) |
| Integrator contact for cybersecurity | _fill in_ |
| Library contact | Through [`SECURITY.md`](../../SECURITY.md) |

## 2. Responsibilities

**R** = responsible for doing it. **I** = informed, or provides input.

| Activity | Library | Integrator | Where it is described |
| --- | --- | --- | --- |
| TARA of the item: damage scenarios, impact, risk values, CAL | I | **R** | [TARA, method](tara.md#1-method) |
| Threat analysis at the library boundary | **R** | I | [TARA](tara.md) |
| Cybersecurity requirements of the library (CSR-01 to CSR-08) | **R** | I | [Section L](../requirements.md#l-cybersecurity-requirements-isosae-21434) |
| Verification of the library: tests, fuzzing, sanitizers, static analysis | **R** | I | [Cybersecurity verification](cybersecurity-verification.md) |
| Authenticating the input before parsing | — | **R** | [Manual C-2.1](cybersecurity-manual.md#2-before-parsing) |
| Choosing the limits and the options, including secure mode | I | **R** | [Manual, sections 2 and 3](cybersecurity-manual.md#3-while-parsing) |
| Escaping strings for their destination | — | **R** | [Manual C-4.1](cybersecurity-manual.md#4-after-parsing) |
| Platform protection: memory protection, stack guards, secure boot | — | **R** | [Manual, section 5](cybersecurity-manual.md#5-your-platform) |
| Penetration testing at system level | — | **R** | [Manual C-7.2](cybersecurity-manual.md#7-your-own-work) |
| Independent review and assessment of the library's documents | — | **R** (for now) | [Manual C-7.3](cybersecurity-manual.md#7-your-own-work) |
| Monitoring and fixing vulnerabilities in the library | **R** | I | [`SECURITY.md`](../../SECURITY.md) |
| Updating vehicles in the field | — | **R** | Integrator's own process |
| Telling the library about a vulnerability found by the integrator | I | **R** | [`SECURITY.md`](../../SECURITY.md#reporting-a-security-problem) |

## 3. Information shared

| From | To | What | When |
| --- | --- | --- | --- |
| Library | Integrator | Security advisories and fixed releases | When a vulnerability is fixed |
| Library | Integrator | Supported versions and end of support | In [`SECURITY.md`](../../SECURITY.md#supported-versions) |
| Integrator | Library | Vulnerabilities found in the library | As soon as found, without public details |

## 4. Agreement

| | Name | Date |
| --- | --- | --- |
| Integrator | _fill in_ | _fill in_ |

The library side is described by this repository at the agreed release tag. There is no
signature from the library side: the project accepts no contractual obligations.
