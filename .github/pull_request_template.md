## Behavior

Describe the concrete behavior introduced or fixed, with a before/after example
where useful.

## Requirements and issue

Closes #<!-- issue number -->

Requirements: <!-- J5-IDs -->

## Safety and security impact

Change impact analysis for the ISO 26262 safety work (ISO 26262-8, clause 8) and the
ISO/SAE 21434 cybersecurity work; see `docs/ISO26262-safety/safety-plan.md` and
`docs/ISO21434-cybersecurity/cybersecurity-plan.md`. Answer this for every pull
request, including documentation-only changes. AI agents must fill it in too.

- [ ] This change does **not** affect any software safety requirement (SSR,
  `docs/requirements.md` section K), cybersecurity requirement (CSR, section L), SEooC
  assumption (`docs/ISO26262-safety/seooc-assumptions.md`) or cybersecurity assumption
  (`docs/ISO21434-cybersecurity/cybersecurity-assumptions.md`).
- [ ] This change **does** affect them. Affected SSR, CSR or assumption IDs and how:
  <!-- for example: SSR-02 and CSR-02, a new limit is enforced; A-05, unchanged -->

## Validation

- [ ] Relevant required unit cases pass with exceptions OFF and RTTI OFF.
- [ ] clang-format 18 check passes.
- [ ] Named unit references and affected essential public API/limit contracts are accurate; no separate per-issue report is required.
- [ ] Incorporated source/data/fixtures have recorded versions and notices.

## Remaining limitations

Describe unsupported behavior or unresolved Draft contracts relevant to this change.
