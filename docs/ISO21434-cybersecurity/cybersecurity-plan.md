# Cybersecurity plan

| Item | Value |
| --- | --- |
| Work product | Cybersecurity plan (ISO/SAE 21434, clause 6) |
| Issue | [#139](https://github.com/Romanr92/Adaptive_JSON5/issues/139) |
| Status | Draft, not reviewed |
| Scope | Parser library `AdaptiveJSON5::parser`, developed as a component out of context towards **CAL 4** (see [how we arrived at CAL 4](cybersecurity-assumptions.md#how-we-arrived-at-cal-4)) |

This plan describes how the library's cybersecurity work is organized: who does what,
which parts of ISO/SAE 21434 apply, which documents are produced, and how changes,
reviews and vulnerabilities are handled. It mirrors the
[safety plan](../ISO26262-safety/safety-plan.md) and reuses its processes where they
fit.

No compliance, certification or "secure" status is claimed in this document.

## 1. Development setup and its limits

> **⚠️ One developer, supported by AI. The independent steps are simulated, not fulfilled.**

This project is developed by one person, Romanr92, with AI tools (Claude, ChatGPT and
GitHub Copilot) that help draft documents and tests and run extra review passes.
ISO/SAE 21434 and ISO 26262 both require reviews, audits and assessments by qualified
humans who are independent of the developer. There is nobody else on this project to do
that work, so the steps that need independence were simulated with an AI to follow the
process as closely as possible.

That has clear consequences:

- **An AI review is not an independent review**, and it is not accepted as one.
- The technical work, such as tests, analyses, fuzzing and coverage, is real, and its
  evidence can be checked by anyone.
- No review, audit or assessment that requires independence has been fulfilled.
- **CAL 4 is a design target, not an achieved level.** No ISO/SAE 21434 claim is valid
  without an independent assessment by a qualified human.

## 2. Roles

| Role | Who | Responsibilities |
| --- | --- | --- |
| Developer | Romanr92 | Writes all production C++ and public headers. |
| Cybersecurity manager | Romanr92 | Owns this plan, the cybersecurity documents and their approval. |
| Vulnerability manager | Romanr92 | Receives and handles reports under [`SECURITY.md`](../../SECURITY.md), prepares fixes and advisories. |
| Review support | AI tools: Claude, ChatGPT, GitHub Copilot | Draft documents and tests, run extra review passes, within the limits in [AGENTS.md](../../AGENTS.md). Not independent. |
| Independent security reviewer and assessor | **Open** | Independent reviews and the cybersecurity assessment. Until one is found, **every integrator must provide their own**. |

## 3. Tailoring

For a component developed out of context, only some clauses of ISO/SAE 21434 apply.
The rest belong to the integrator, who knows the vehicle and the item.

| Clause | Topic | Applies here? | Reason |
| --- | --- | --- | --- |
| 4 | General considerations | Yes | The overall approach. |
| 5 | Organizational cybersecurity management | Partly | Only **tool management**: the [tool confidence evaluation](../ISO26262-safety/tool-confidence.md) and the pinned supply chain (#131). Organization-wide rules, culture and audits do not apply to a one-person project. |
| 6 | Project dependent cybersecurity management | Yes | This plan, the development as a component out of context, the **cybersecurity case** and the **cybersecurity assessment**. The assessment needs an independent assessor, so it is an open gap (section 1). |
| 7 | Distributed cybersecurity activities | Yes | The library acts as a supplier to each integrator. The [cybersecurity manual](cybersecurity-manual.md) is the basis of a **cybersecurity interface agreement**: who does what between the library and the integrator. |
| 8 | Continual cybersecurity activities | Yes | Monitoring for vulnerabilities, analysing them and managing them, through [`SECURITY.md`](../../SECURITY.md) and dependency scanning. |
| 9 | Concept | No, integrator | The item definition, cybersecurity goals and concept belong to the vehicle. The library assumes them instead ([cybersecurity assumptions](cybersecurity-assumptions.md)). |
| 10 | Product development | **Yes, main part** | Cybersecurity requirements, design, implementation and the library's own verification ([cybersecurity verification](cybersecurity-verification.md)). |
| 11 | Cybersecurity validation | No, integrator | Validation happens at vehicle level, for the item as a whole. |
| 12 | Production | No, integrator | The library has no production process; releases are handled under configuration management. |
| 13 | Operations and maintenance | Partly | Responding to vulnerabilities found after release, with fixes and advisories. Updating vehicles in the field belongs to the integrator. |
| 14 | End of cybersecurity support and decommissioning | Yes | Which versions receive security fixes and for how long: the support policy in [`SECURITY.md`](../../SECURITY.md#supported-versions). Decommissioning belongs to the integrator. |
| 15 | TARA methods | Yes | Used for the [TARA at the library boundary](tara.md). |

## 4. Work products

All work products are stored in `docs/ISO21434-cybersecurity/` unless stated
otherwise. Owner of all work products: Romanr92.

| # | Work product | Location | Status |
| --- | --- | --- | --- |
| 1 | Cybersecurity assumptions, including the CAL | [`cybersecurity-assumptions.md`](cybersecurity-assumptions.md) | Draft |
| 2 | Cybersecurity plan | This document | Draft |
| 3 | TARA at the library boundary | [`tara.md`](tara.md) | Draft |
| 4 | Cybersecurity requirements | [Section L of `docs/requirements.md`](../requirements.md#l-cybersecurity-requirements-isosae-21434) | Draft, CSR-01 to CSR-08 |
| 5 | Cybersecurity verification | [`cybersecurity-verification.md`](cybersecurity-verification.md) | Draft; most checks still to be implemented |
| 6 | Vulnerability handling | [`SECURITY.md`](../../SECURITY.md) | Draft; private reporting not yet switched on |
| 7 | Cybersecurity manual for integrators | [`cybersecurity-manual.md`](cybersecurity-manual.md) | Draft |
| 8 | Cybersecurity case | Not started | Not started |
| 9 | Cybersecurity interface agreement (template) | Based on the cybersecurity manual | Not started |
| 10 | End-of-support policy | [`SECURITY.md`, supported versions](../../SECURITY.md#supported-versions) | Draft |
| 11 | Response targets and the process for known problems in released versions | [`SECURITY.md`](../../SECURITY.md#response-times-and-known-problems-in-releases) | To be defined with the 1.0.0 release (#135) |

## 5. What CAL 4 means for this project

The general rigour levels are described in the
[cybersecurity assumptions](cybersecurity-assumptions.md#the-levels-and-their-rigour).
For this library, "most thorough" means:

| Area | In practice | Status |
| --- | --- | --- |
| Every change | A pull request linked to an issue, all CI checks passing, and the **safety and security impact check** answered | In place |
| Robust input | Blocking sanitizer runs and fuzzing on every pull request, plus a longer weekly fuzz run | Planned (#132) |
| Compiler diversity | A blocking second compiler (Clang) | Planned (#141) |
| Static analysis | clang-tidy and CodeQL, including the MISRA C++:2023 queries, blocking new findings | Planned (#146) |
| Supply chain | Pinned CI actions, hashed tool installs, SBOM and signed or attested releases; dependency vulnerability scanning | Planned (#131) |
| Secure input handling | Opt-in secure mode | Planned (#145) |
| Reviews | A security review in every milestone; an **independent** reviewer and assessor | Milestone review planned; independent reviewer **open** |
| Evidence | A complete cybersecurity case | Not started |

## 6. Supporting processes

The same processes as for safety apply (see the
[safety plan](../ISO26262-safety/safety-plan.md#4-supporting-processes)):

- Every change to `main` goes through a pull request linked to an issue, and is merged
  only when all required CI checks pass.
- Every pull request answers the **safety and security impact check** in the
  [pull request template](../../.github/pull_request_template.md): does the change affect
  a software safety requirement (SSR), a cybersecurity requirement (CSR), a SEooC
  assumption or a cybersecurity assumption, and if so, which one and how. AI agents fill
  it in as well.
- Each release is identified by a Git tag, which also freezes the matching set of
  cybersecurity documents.

## 7. Security is never finished

Issue #139 sets up the **first cybersecurity baseline**. Closing it does not close the
cybersecurity work:

- **Every milestone gets a security review**, together with the safety review: the
  assumptions, the TARA, the requirements and the manual are checked against everything
  that changed, and updated where needed.
- **Between milestones**, the impact check in every pull request catches changes that
  affect a cybersecurity requirement or assumption.
- **Vulnerability monitoring is continuous**: reports under `SECURITY.md`, dependency
  alerts and public advisories are analysed as they arrive.
- New features, such as the planned serializer, start with an update of the assumptions,
  the TARA and the requirements before they are implemented.
