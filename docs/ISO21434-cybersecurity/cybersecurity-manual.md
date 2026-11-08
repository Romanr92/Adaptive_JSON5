# Cybersecurity manual for integrators

| Item | Value |
| --- | --- |
| Work product | Cybersecurity manual for a component out of context, and basis of the cybersecurity interface agreement (ISO/SAE 21434, clauses 6 and 7) |
| Library | Adaptive_JSON5, parser library `AdaptiveJSON5::parser` |
| Issue | [#139](https://github.com/Romanr92/Adaptive_JSON5/issues/139) |
| Status | Draft, not reviewed and **not independently assessed** |

> **⚠️ Developed by one developer with AI support. No independent review or assessment has
> been done.** CAL 4 is a design target, not an achieved level. An AI review is not an
> independent review. Before relying on this library, have it reviewed by a qualified
> human in your own project (see C-7.3 and the
> [development setup and its limits](cybersecurity-plan.md#1-development-setup-and-its-limits)).

This manual is for teams that want to use Adaptive_JSON5 where cybersecurity matters. It
can be read on its own: every item is one short line that you tick off for your project,
with a link to where it is explained in detail. It also serves as the starting point for
a **cybersecurity interface agreement** between your project and this library: the items
below say what the library does and what you do.

**No compliance, certification or "secure" status is claimed.**

## How to use this checklist

1. Go through every item and decide for your project: **met**, **not met**, or
   **not applicable** (with a reason).
2. An item that is **not met** means you cannot rely on the library's cybersecurity
   argument for that point. You need your own analysis or control instead.
3. Keep the filled-in checklist with your own cybersecurity case.

The [Safety Manual](../ISO26262-safety/safety-manual.md) has a separate checklist for
functional safety. If you use the library in a safety-related function, fill in both.

---

## 1. Scope

- [ ] **C-1.1** Only the parser library `AdaptiveJSON5::parser` is in scope, developed
  towards **CAL 4**.
  → [How we arrived at CAL 4](cybersecurity-assumptions.md#how-we-arrived-at-cal-4)
- [ ] **C-1.2** The hosted add-on and the planned serializer are not covered.
  → [Cybersecurity assumptions](cybersecurity-assumptions.md)
- [ ] **C-1.3** Your own TARA confirms which CAL you need. It may be **lower** than CAL 4,
  for example when the input only arrives through local access or the impact is limited.
  Using a library developed towards a higher CAL is fine.
  → [When an integrator may use a lower CAL](cybersecurity-assumptions.md#when-an-integrator-may-use-a-lower-cal)
- [ ] **C-1.4** You use the caller-buffer path (`json5_document::parse`) or your own
  storage callbacks (`parse_document`), not the hosted add-on, which takes its memory
  from the heap and is not covered.
  → [A-05](../ISO26262-safety/seooc-assumptions.md#a-05-memory-no-heap-and-which-library-is-which)
- [ ] **C-1.5** You have read the assumptions about the attacker: the attacker controls
  the input, but cannot change code or memory directly.
  → [CA-01](cybersecurity-assumptions.md#ca-01-attacker-model)

## 2. Before parsing

- [ ] **C-2.1 Verify before you parse.** If the input can come from an untrusted source,
  verify the authenticity of the **raw bytes** first, for example with a signature
  checked by your platform's crypto library or hardware security module (HSM). Only call
  the parser once the check has passed. **Never parse first and verify afterwards.**
  The library does not do this for you: a signature check needs keys and platform crypto,
  and it must stay independent of the parser.
  → [TARA, T-02](tara.md#4-controls)
- [ ] **C-2.2** For data protection against random corruption (a CRC, a sequence counter
  and a timeout), use your end-to-end protection, as required for safety.
  → [A-08](../ISO26262-safety/seooc-assumptions.md#a-08-decomposition-the-integrator-provides-an-independent-e2e-check)
- [ ] **C-2.3** You set every resource limit as tight as your use case allows.
  → [A-04](../ISO26262-safety/seooc-assumptions.md#a-04-the-integrator-sets-the-resource-limits),
  [TARA, T-03](tara.md#3-threats)

## 3. While parsing

- [ ] **C-3.1** Using **secure mode** is **advised** for data from a network or another
  untrusted source, once it is available. It is not required.
  → [Secure mode](tara.md#5-secure-mode-planned), #145
- [ ] **C-3.2** If you do not use secure mode, you handle duplicate member names, zero bytes,
  control characters and unpaired surrogates yourself, for example with
  `reject_duplicates` and your own checks after parsing.
  → [TARA, T-02 and T-06](tara.md#4-controls)

> **⚠️ Secure mode is not fully standard JSON or JSON5.** It deliberately rejects some
> documents that the standards allow. See the [TARA](tara.md#5-secure-mode-planned).

## 4. After parsing

- [ ] **C-4.1** You treat **every string and member name as untrusted**, and escape it for
  where it goes next: a shell command, a file path, a log line, a script or a database
  query. The library returns strings exactly and never escapes them.
  → [CSR-06](../requirements.md#l-cybersecurity-requirements-isosae-21434), [TARA, T-06](tara.md#3-threats)
- [ ] **C-4.2** You do **not** use C string functions (`strlen`, `strcpy`, `printf("%s")`
  and similar) on strings that may contain zero bytes; use the returned length.
  → [TARA, T-06](tara.md#3-threats)
- [ ] **C-4.3** You decide where error messages and any retained copy of the input
  (`retain_copy`) go, and do not send them to untrusted receivers.
  → [CSR-08](../requirements.md#l-cybersecurity-requirements-isosae-21434), [TARA, T-04](tara.md#3-threats)
- [ ] **C-4.4** You check every result and reject the whole document on any error.
  → [A-06](../ISO26262-safety/seooc-assumptions.md#a-06-every-result-is-checked-and-any-error-rejects-the-whole-document)

## 5. Your platform

- [ ] **C-5.1** Memory protection (MPU, PMP or MMU) separates the library's memory from
  other software, and a guard region sits below every stack.
  → [CA-03](cybersecurity-assumptions.md#ca-03-how-memory-is-laid-out-and-why-it-matters)
- [ ] **C-5.2** Where your operating system offers it, address space layout randomization
  (ASLR) and non-executable data memory are enabled.
  → [CA-03](cybersecurity-assumptions.md#ca-03-how-memory-is-laid-out-and-why-it-matters)
- [ ] **C-5.3** Secure boot and signed software updates prevent an attacker from replacing
  the code or changing memory directly.
  → [CA-01](cybersecurity-assumptions.md#ca-01-attacker-model)

## 6. Supply chain and updates

- [ ] **C-6.1** You build from a tagged release and verify its checksums and, once
  available, its signature or attestation.
  → [CA-04](cybersecurity-assumptions.md#ca-04-supply-chain-the-second-way-in), #131
- [ ] **C-6.2** You follow the project's security advisories and update when a fix is
  released.
  → [`SECURITY.md`](../../SECURITY.md)
- [ ] **C-6.3** You know which versions receive security fixes, and for how long.
  → [Supported versions](../../SECURITY.md#supported-versions)
- [ ] **C-6.4** You evaluate your own compiler and toolchain.
  → [Tool confidence](../ISO26262-safety/tool-confidence.md#3-compilers)

## 7. Your own work

- [ ] **C-7.1** Your own TARA covers the item, with damage scenarios, impact ratings and
  risk values. The library's TARA stops at its boundary.
  → [TARA](tara.md#1-method)
- [ ] **C-7.2** You do **penetration testing at system level**, on your ECU and vehicle
  with this library inside.
  → [Cybersecurity verification, penetration testing](cybersecurity-verification.md#3-penetration-testing)
- [ ] **C-7.3 Independent reviewer. For now, you provide one.** The library's cybersecurity
  documents have not been independently reviewed or assessed. Until that changes, you do
  that in your own project before relying on them.
  → [Cybersecurity plan, roles](cybersecurity-plan.md#2-roles)
- [ ] **C-7.4** You agree with the library on who does what, using this checklist as the
  basis of your cybersecurity interface agreement.

## 8. Known limits and open issues

At the time of writing, these are known and tracked. Check their state for the release
you use.

| Topic | Effect for you | Tracked in |
| --- | --- | --- |
| Secure mode not implemented yet | Duplicate names, zero bytes, control characters and unpaired surrogates are your job (C-3.2) | #145 |
| `as_double()` buffer use and quadratic `member()` lookup | An attacker can make repeated reads fail or slow down lookups on large objects | #124 |
| Sanitizer and fuzz runs not implemented yet | Less evidence against memory corruption than planned | #132 |
| Static analysis not implemented yet | Less evidence than planned | #146 |
| Releases not yet signed or attested; no SBOM | You can check integrity with checksums only | #131 |
| Private vulnerability reporting not yet switched on | Reports go through a short public issue first | [`SECURITY.md`](../../SECURITY.md) |
