# Threat analysis at the library boundary (TARA)

| Item | Value |
| --- | --- |
| Work product | Threat analysis and risk assessment for a component out of context (ISO/SAE 21434, clause 15 methods) |
| Issue | [#139](https://github.com/Romanr92/Adaptive_JSON5/issues/139) |
| Status | Draft, not reviewed |
| Scope | Parser library `AdaptiveJSON5::parser`. Assumptions in [cybersecurity assumptions](cybersecurity-assumptions.md). |

## 1. Method

- **Attack feasibility** is rated with the **attack vector** method of ISO/SAE 21434.
  It fits a library well and only uses what is known at the library boundary: how the
  attacker reaches the library.

  | Attack vector | Attack feasibility |
  | --- | --- |
  | Network (for example crafted messages over Ethernet) | High |
  | Adjacent (for example a local bus or a nearby wireless link) | Medium |
  | Local (code or access on the same ECU) | Low |
  | Physical (hands-on access to the hardware) | Very low |

- **Impact** (safety, financial, operational and privacy damage) depends on the vehicle
  and the function. Like the [software FMEA](../ISO26262-safety/software-safety-analysis.md#12-where-the-effect-stops),
  this analysis **stops at the library boundary**: it says what the integrator's code
  would see. The integrator rates the impact and the risk value in their own TARA,
  using the threats below as input.
- Every input is assumed to come from the network, so every runtime threat is rated
  **High** feasibility. This is deliberate: the library cannot know where its input
  comes from.

## 2. Assets

What an attacker could harm at the library boundary, and which cybersecurity property
matters for each:

| ID | Asset | Property | Why it matters |
| --- | --- | --- | --- |
| AS-1 | The parsed data: values, member names and the structure of the document | Integrity | The integrator acts on it; a wrong or ambiguous value can drive a wrong decision |
| AS-2 | The parsing service: the ability to parse valid input within the planned time and memory | Availability | A parse that fails, hangs or runs too long can stop the function that needs the data |
| AS-3 | The memory around the library: the integrator's buffer, the stack and whatever lies next to them | Integrity | Corrupting it can break other software or let an attacker take control |
| AS-4 | The input and anything derived from it, such as error messages and a retained copy | Confidentiality | The input may contain data that must not leave the ECU |
| AS-5 | The library itself: its source, its build tools and its release packages | Integrity and authenticity | A tampered build reaches every integrator |

## 3. Threats

| ID | Threat | Attacker goal | Attack vector → feasibility | Effect at the library boundary |
| --- | --- | --- | --- | --- |
| T-01 | **Memory corruption from crafted input**, such as a buffer overflow (AS-3) | G-1 take control | Network → **High** | The library reads or writes outside the input or its buffer. This is the top threat. |
| T-02 | **Ambiguous data accepted as valid**, for example duplicate member names: one parser keeps the first value, another keeps the last, so a gateway and an ECU read the same message differently | G-2 integrity | Network → **High** | A document is accepted whose meaning depends on which parser reads it. |
| T-03 | **Denial of service from crafted input**: deep nesting, huge numbers or strings, many members, slow lookups | G-3 availability | Network → **High** | Parsing or access takes too long or too much memory, or valid data is rejected. |
| T-04 | **Information leak**, through error messages or retained copies of the input | G-4 confidentiality | Network → **High** | Data leaves the library in a place the integrator did not expect. |
| T-05 | **Supply-chain tampering** with the Unicode data, CI actions, compiler or release packages | G-1, G-2 | Network (to the build system) → **High** | The library itself is built differently from its reviewed source. |
| T-06 | **Injection through strings or member names**: content that is harmless to the parser but is later interpreted by something else, such as a shell command, a file path, a log line or a script, or an embedded zero byte that cuts a key short for C string functions (`"admin\u0000x"` becomes `admin`) | G-1, G-2 | Network → **High** | The library returns the exact string, including control characters or zero bytes. The danger arises in the integrator's code. |

## 4. Controls

| ID | In the library | Planned in the library | By the integrator |
| --- | --- | --- | --- |
| T-01 | No heap; every access bounds-checked; checked size arithmetic; no recursion; input/buffer overlap detected ([SSR-01, SSR-08](../requirements.md#k-software-safety-requirements-iso-26262)) | Blocking sanitizer and fuzz runs (#132), second compiler (#141) | Memory protection (MPU, PMP, MMU), stack guard regions, ASLR where the OS offers it ([CA-03](cybersecurity-assumptions.md#ca-03-how-memory-is-laid-out-and-why-it-matters)) |
| T-02 | The `reject_duplicates` duplicate-name policy; `member()` reports an ambiguous name as an error under the default policy | **Secure mode** (section 5) | Use secure mode, or at least `reject_duplicates`, for security-relevant data. **Authenticate the source**: a successful parse only means the syntax is valid, not that the sender is genuine |
| T-03 | Inclusive resource limits and the work budget ([SSR-02](../requirements.md#k-software-safety-requirements-iso-26262)) | Fix the `as_double()` buffer use and the quadratic `member()` lookup (#124) | Choose every limit for the use case ([A-04](../ISO26262-safety/seooc-assumptions.md#a-04-the-integrator-sets-the-resource-limits)); watchdog or timing supervision |
| T-04 | Errors carry a code, a byte offset and an optional range, not memory contents; a copy of the input is kept only if the integrator enables `retain_copy` (off by default) | — | Decide where error messages and retained input go; do not send them to untrusted receivers |
| T-05 | Pinned GoogleTest commit; Unicode tables checked by unit tests | Pinned CI actions, hashed tool installs, SBOM and signed or attested releases (#131); second compiler (#141) | Verify release checksums and signatures; build from a tagged release |
| T-06 | The library never runs or interprets values or member names: no evaluation, no format strings. To the library, a string is just text with an exact length | **Secure mode** rejects zero bytes, control characters and unpaired surrogates (section 5) | Treat every string and member name as untrusted. Escape it for its destination (shell, path, log, script) and do not use C string functions on strings that may contain zero bytes |

Full escaping cannot live in the library, because the right escaping depends on where
the string goes next, and only the integrator knows that. A one-size-fits-all escape
would give false confidence.

## 5. Secure mode (planned)

Secure mode is a planned set of parse options (#145) for input
that comes from a network or another untrusted source.

- It is **off by default**, so the default behaviour stays fully standard.
- Turning it on enables **every** security option at once:
  - reject duplicate member names (`reject_duplicates`);
  - reject zero bytes (`U+0000`) in strings and member names;
  - reject control characters in member names;
  - reject unpaired surrogates, which today only produce a warning.
- After turning it on, each option can still be switched off individually.

> **⚠️ Secure mode is not fully standard JSON or JSON5.** It deliberately rejects some
> documents that the standards allow: duplicate member names, zero bytes and control
> characters in strings or member names, and unpaired surrogates. A valid document from
> another system may therefore fail to parse. Use secure mode only when you control both
> ends, or when rejecting these cases is what you want.

## 6. Residual risks

- Until the sanitizer and fuzz runs exist (#132), the evidence for T-01 is limited to
  unit and validation tests.
- Until #124 is fixed, an attacker can make repeated `as_double()` reads fail and slow
  down `member()` on large objects (T-03).
- Until secure mode exists, T-02 and T-06 rely on the integrator's own checks, apart
  from `reject_duplicates`.
- Impact ratings and risk values remain the integrator's job.
