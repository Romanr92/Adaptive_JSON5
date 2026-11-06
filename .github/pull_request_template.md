## Behavior

Describe the concrete behavior introduced or fixed, with a before/after example
where useful.

## Requirements and issue

Closes #<!-- issue number -->

Requirements: <!-- J5-IDs -->

## Safety impact

Change impact analysis for the ISO 26262 safety work (ISO 26262-8, clause 8; see
the safety plan in `docs/ISO26262-safety/safety-plan.md`). Answer this for every
pull request, including documentation-only changes. AI agents must fill it in too.

- [ ] This change does **not** affect any software safety requirement (SSR) in
  `docs/requirements.md` section K or any SEooC assumption in
  `docs/ISO26262-safety/seooc-assumptions.md`.
- [ ] This change **does** affect them. Affected SSR or assumption IDs and how:
  <!-- for example: SSR-02, a new limit is enforced; A-05, unchanged -->

## Validation

- [ ] Relevant required unit cases pass with exceptions OFF and RTTI OFF.
- [ ] clang-format 18 check passes.
- [ ] Named unit references and affected essential public API/limit contracts are accurate; no separate per-issue report is required.
- [ ] Incorporated source/data/fixtures have recorded versions and notices.

## Remaining limitations

Describe unsupported behavior or unresolved Draft contracts relevant to this change.
