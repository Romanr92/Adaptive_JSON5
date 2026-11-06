# Safety plan

| Item | Value |
| --- | --- |
| Work product | Safety plan (ISO 26262-2, clause 6) |
| Issue | [#138](https://github.com/Romanr92/Adaptive_JSON5/issues/138) |
| Status | Draft, not reviewed |
| Standard edition | ISO 26262:2018 is assumed |

This plan describes how the library's safety work is organized: who does what,
which parts of ISO 26262 apply, which documents are produced, and how changes and
reviews are handled.

The library is developed as a Safety Element out of Context (SEooC) targeting
**ASIL B(D)**, as one part of an ASIL D decomposition. The assumptions behind this
are in [SEooC assumptions](seooc-assumptions.md).

No compliance, certification or "verified" status is claimed in this document.

## 1. Roles

| Role | Who | Responsibilities |
| --- | --- | --- |
| Developer | Romanr92 | Writes all production C++ and public headers. |
| Safety manager | Romanr92 | Owns this plan, the safety documents and their approval. |
| Review support | AI tools: Claude, ChatGPT, GitHub Copilot | Draft documents and unit tests, and run extra review passes, within the limits in [AGENTS.md](../../AGENTS.md). |
| Independent confirmation reviewer | **Open, see below** | Confirmation reviews and the functional safety assessment. |

> **Open item: no independent confirmation yet.**
>
> ISO 26262-2 requires confirmation reviews by a qualified person who is independent
> of the developer. AI tools do not count as independent reviewers. Today there is
> only one developer, so the library's safety documents are **not independently
> confirmed**.
>
> Until an independent reviewer is found, for example a colleague or an external
> assessor, **every integrator must provide their own independent reviewer** and
> perform the confirmation reviews in their own project before relying on these
> documents. The Safety Manual will state this as an obligation of use. This is meant
> as a temporary situation.

## 2. Tailoring

For a SEooC software library, only some parts of ISO 26262 apply. The rest belong
to the integrator, who knows the vehicle and the item.

| Part | Topic | Applies here? | Reason |
| --- | --- | --- | --- |
| 1 | Vocabulary | Yes | Terms are used as defined there. |
| 2 | Management of functional safety | Yes | This plan, roles and confirmation measures. |
| 3 | Concept phase (item definition, HARA, safety goals) | No, integrator | A library is not an item. The HARA is done at vehicle level, and the ASIL is assumed here instead (see SEooC assumptions). |
| 4 | Product development at the system level | No, integrator | The library has no system architecture of its own. The integrator allocates its technical safety requirements to it. |
| 5 | Product development at the hardware level | No, integrator | The library is software only and makes no hardware assumptions beyond those in A-01 and A-07. |
| 6 | Product development at the software level | **Yes, main part** | Software safety requirements, architecture, unit design, coding, unit verification and integration of the library. |
| 7 | Production, operation, service and decommissioning | No, integrator | The library has no production or service process. Releases are handled under Part 8 configuration management. |
| 8 | Supporting processes | Yes | Configuration and change management, verification, documentation, requirements management and tool confidence. |
| 9 | ASIL-oriented and safety-oriented analyses | Yes | ASIL B(D) decomposition, dependent failure analysis and the software safety analysis. |
| 10 | Guidelines | Used as guidance | Clause 9 describes how to develop a SEooC. |
| 11 | Semiconductors | No | Not a semiconductor. |
| 12 | Motorcycles | No | Not motorcycle-specific. |

## 3. Work products

All work products are stored in `docs/ISO26262-safety/` unless stated otherwise.
Owner of all work products: Romanr92.

| # | Work product | Location | Status |
| --- | --- | --- | --- |
| 1 | SEooC assumptions and decomposition rationale | [`seooc-assumptions.md`](seooc-assumptions.md) | Draft, A-01 to A-08 |
| 2 | Safety plan | This document | Draft |
| 3 | Software safety requirements | [Section K of `docs/requirements.md`](../requirements.md#k-software-safety-requirements-iso-26262) | Draft, SSR-01 to SSR-08 |
| 4 | Software architecture and safety analysis (FMEA, dependent failure analysis) | [`software-safety-analysis.md`](software-safety-analysis.md); architecture in [`core/docs/design-architecture.md`](../../core/docs/design-architecture.md) | Draft: FMEA rows and dependent failure analysis reviewed with the owner. A separate interface view of the architecture is still to do. |
| 5 | Coding guidelines, building on the MISRA fail-fast rules (see #137) | [`coding-guidelines.md`](coding-guidelines.md) | Draft |
| 6 | Verification and structural coverage strategy | [`verification-strategy.md`](verification-strategy.md) | Draft; most new checks still to be implemented (#132, #141) |
| 7 | Tool confidence evaluation (ISO 26262-8, clause 11) | [`tool-confidence.md`](tool-confidence.md) | Draft |
| 8 | Traceability | Not started | Not started |
| 9 | Safety Manual for integrators | Not started | Not started |

**How the architecture and the FMEA rows are produced.** An AI agent reads the code
and the existing design documents and drafts each part: what it does, its interfaces,
which errors it can return, and its FMEA rows. Nothing is committed at that stage.
The owner then goes through the draft part by part and confirms or corrects each one
before it is written into the work product. The agent does the reading; the owner
makes the decisions.

## 4. Supporting processes

### Configuration and change management

- Every change to `main` goes through a pull request that is linked to a GitHub issue.
- A pull request is merged only when all required CI checks pass: host unit tests,
  validation tests and formatting.
- Every pull request answers the **safety impact check** in the
  [pull request template](../../.github/pull_request_template.md): does the change
  affect a software safety requirement (SSR) or a SEooC assumption, and if so, which
  one and how. This is the change impact analysis required by ISO 26262-8,
  clause 8. AI agents must fill it in as well (see [AGENTS.md](../../AGENTS.md)).
- Each release is identified by a Git tag. Because the safety documents live in the
  repository, the tag also freezes the matching set of safety documents for that
  release.

### Verification

How the library is verified, including structural coverage, is defined in the
[verification strategy](verification-strategy.md) (work product 6). Besides unit
tests, validation tests and formatting, it adds blocking sanitizer runs, fuzzing,
a second compiler and, once full branch coverage is reached, branch coverage.

### Tool confidence

Tool confidence for the compilers, CMake, GoogleTest, clang-format, the coverage
tools and the repository's own scripts is evaluated in the
[tool confidence evaluation](tool-confidence.md) (work product 7).

## 5. Confirmation measures and reviews

- **Safety documents:** each safety document is reviewed in its pull request before
  it is merged.
- **Before each release:** a full repository review is done, like the
  [0.2.0 review](../reviews/2026-11-06-full-repository-review-0.2.0.md), and the safety
  documents are checked against the code to confirm they still match it.
- **Independent confirmation reviews** of this plan and of the later safety case,
  and the **functional safety assessment**, require an independent reviewer. The
  level of independence required by ISO 26262-2 for an ASIL B(D) element still has
  to be determined. Until a reviewer is found, this is an open item that integrators
  cover (see section 1).

## 6. Safety is never finished

Issue #138 sets up the **first safety baseline**. Closing it does not close the safety
work: safety evolves with the library for as long as it is developed.

- **Every milestone gets its own safety review issue.** In it, the SEooC assumptions,
  the software safety requirements, the safety analysis and the other work products
  are reviewed against everything that changed in that milestone, and updated where
  needed.
- **Between milestones**, the safety impact check in every pull request (section 4)
  catches changes that affect a safety requirement or assumption.
- **Before each release**, the reviews in section 5 confirm that the safety documents
  still match the code, and the release tag freezes them.
- New features, such as the planned serializer, start with an update of the SEooC
  assumptions and the safety requirements before they are implemented.
