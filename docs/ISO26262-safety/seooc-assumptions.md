# SEooC assumptions

| Item | Value |
| --- | --- |
| Work product | Assumptions for developing the library as a Safety Element out of Context (ISO 26262-10, clause 9) |
| Issue | [#138](https://github.com/Romanr92/Adaptive_JSON5/issues/138) |
| Status | Draft, not reviewed. The owner discussion is still in progress, so more assumptions will follow. |

The library is developed without knowing the vehicle or system it will be used in.
Each assumption below therefore has to be confirmed by the integrator against their
own item, their HARA and their technical safety concept. If an assumption does not
hold for a project, that project cannot rely on the library's safety argument
without its own extra analysis.

No compliance, certification or "verified" status is claimed in this document.

## Scope of these assumptions

These assumptions currently cover the **parser** only. A serializer that writes JSON
for vehicle networks such as Ethernet is planned but does not exist yet. These
assumptions will be extended when it is added, and until then nothing in this
document applies to serialization.

The assumed integrity level is **ASIL B(D)**: the library is one part of an ASIL D
decomposition, and the integrator provides the other, independent part (see A-08).

## How an ASIL is determined (general method)

This section explains how an ASIL is normally worked out, so that the reasoning in the
next section can be followed. It is a summary in our own words. The standard's text is
licensed: for a real rating, use ISO 26262-3 itself.

An ASIL (Automotive Safety Integrity Level) is determined in the **hazard analysis and
risk assessment (HARA)** at vehicle level. For each hazardous event (a malfunction in a
specific driving situation), three factors are rated, and a table combines them.

### Legend: the three factors

**Severity (S): how badly people could be hurt.**

| Class | Meaning | Example |
| --- | --- | --- |
| S0 | No injuries | Damage to the vehicle only |
| S1 | Light and moderate injuries | Low-speed collision |
| S2 | Severe and life-threatening injuries, survival probable | Medium-speed collision |
| S3 | Life-threatening injuries with survival uncertain, or fatal injuries | High-speed collision, pedestrian hit |

**Exposure (E): how often the vehicle is in the situation where the malfunction is
dangerous.** It can be rated by time (share of operating time) or by frequency.

| Class | Meaning | By time (rough guide) | By frequency (rough guide) |
| --- | --- | --- | --- |
| E0 | Incredible | — | Practically never |
| E1 | Very low probability | Below 0.1 % of operating time | Less than once in the vehicle's life |
| E2 | Low probability | 0.1 % to 1 % | A few times a year |
| E3 | Medium probability | 1 % to 10 % | Once a month or more |
| E4 | High probability | Above 10 % | Almost every drive |

**Controllability (C): how many drivers or other road users could still avoid the harm.**

| Class | Meaning | Rough guide |
| --- | --- | --- |
| C0 | Controllable in general | Almost everyone avoids harm |
| C1 | Simply controllable | 99 % or more avoid harm |
| C2 | Normally controllable | 90 % or more avoid harm |
| C3 | Difficult to control or uncontrollable | Fewer than 90 % avoid harm |

### The ASIL table

If any factor is S0, E0 or C0, the result is **QM** (quality management: normal
development, no ASIL). Otherwise:

| Severity | Exposure | C1 | C2 | C3 |
| --- | --- | --- | --- | --- |
| S1 | E1 | QM | QM | QM |
| S1 | E2 | QM | QM | QM |
| S1 | E3 | QM | QM | A |
| S1 | E4 | QM | A | B |
| S2 | E1 | QM | QM | QM |
| S2 | E2 | QM | QM | A |
| S2 | E3 | QM | A | B |
| S2 | E4 | A | B | C |
| S3 | E1 | QM | QM | A |
| S3 | E2 | QM | A | B |
| S3 | E3 | A | B | C |
| S3 | E4 | B | C | **D** |

**Memory aid:** add up the three numbers. A sum of 10 gives ASIL D, 9 gives C, 8 gives
B, 7 gives A, and 6 or less gives QM. For example, S3 + E4 + C3 = 10, so ASIL D.

ASIL D is the highest level and needs the most rigorous development; QM needs none of
the ISO 26262-specific measures.

### ASIL decomposition

A safety requirement with a high ASIL can be split between two **sufficiently
independent** parts, which together achieve the original ASIL (ISO 26262-9). Each part
gets a lower ASIL, written with the original in brackets:

| Original | Allowed splits |
| --- | --- |
| ASIL D | C(D) + A(D), **B(D) + B(D)**, or D(D) + QM(D) |
| ASIL C | B(C) + A(C), or C(C) + QM(C) |
| ASIL B | A(B) + A(B), or B(B) + QM(B) |
| ASIL A | A(A) + QM(A) |

**Notation:** "B(D)" means "developed to ASIL B, as one part of a decomposed ASIL D". The
bracket matters: some activities, such as the confirmation measures and the analysis
of dependent failures, still have to respect the original ASIL D.

Decomposition only works if the two parts cannot fail for the same reason. That is why
the independence between them has to be shown with a dependent failure analysis.

### What each ASIL requires

The higher the ASIL, the more methods ISO 26262 expects. The table below summarizes
the main software-level methods of ISO 26262-6 and the confirmation measures of
ISO 26262-2, in our own words. It is a guide, not a copy of the standard: check the
licensed standard for the exact, complete tables.

**Legend**

| Value | Meaning | Mark in the standard |
| --- | --- | --- |
| **Required** | Expected at this level. Leaving it out needs a written reason that shows an equivalent alternative. | `++` highly recommended |
| **Advised** | Recommended. Do it unless there is a good reason not to. | `+` recommended |
| **Optional** | Free to do, but not expected at this level; it adds little on its own. | `o` no recommendation |

QM means normal quality management: none of the ISO 26262-specific methods are
expected, so every row is optional, although good engineering practice still applies.

**Coding guidelines** (ISO 26262-6)

| Method | QM | A | B | C | D |
| --- | --- | --- | --- | --- | --- |
| Enforce low complexity | Optional | Required | Required | Required | Required |
| Use a language subset (for example MISRA) | Optional | Required | Required | Required | Required |
| Enforce strong typing | Optional | Required | Required | Required | Required |
| Defensive implementation techniques | Optional | Advised | Advised | Required | Required |
| Well-trusted design principles | Optional | Advised | Advised | Required | Required |
| Style guides | Optional | Advised | Required | Required | Required |
| Naming conventions | Optional | Required | Required | Required | Required |

**Unit verification** (ISO 26262-6)

| Method | QM | A | B | C | D |
| --- | --- | --- | --- | --- | --- |
| Walk-through (informal review) | Optional | Required | Advised | Optional | Optional |
| Inspection (formal review with roles and checklists) | Optional | Advised | Required | Required | Required |
| Static code analysis | Optional | Advised | Required | Required | Required |
| Control flow and data flow analysis | Optional | Advised | Advised | Required | Required |
| Formal verification | Optional | Optional | Optional | Advised | Advised |
| Requirements-based tests | Optional | Required | Required | Required | Required |
| Interface tests | Optional | Required | Required | Required | Required |
| Fault injection tests | Optional | Advised | Advised | Advised | Required |
| Resource usage evaluation (memory, stack, timing) | Optional | Advised | Advised | Advised | Required |

**How test cases are derived** (ISO 26262-6)

| Method | QM | A | B | C | D |
| --- | --- | --- | --- | --- | --- |
| Analysis of requirements | Optional | Required | Required | Required | Required |
| Equivalence classes | Optional | Advised | Required | Required | Required |
| Boundary values | Optional | Advised | Required | Required | Required |
| Error guessing from experience | Optional | Advised | Advised | Advised | Advised |

**Structural coverage at unit level** (ISO 26262-6)

| Metric | QM | A | B | C | D |
| --- | --- | --- | --- | --- | --- |
| Statement coverage | Optional | Required | Required | Advised | Advised |
| Branch coverage | Optional | Advised | Required | Required | Required |
| MC/DC | Optional | Advised | Advised | Advised | Required |

**Confirmation measures and independence** (ISO 26262-2)

| Measure | QM | A | B | C | D |
| --- | --- | --- | --- | --- | --- |
| Confirmation reviews of work products by someone other than the author | Optional | Required | Required | Required | Required |
| Functional safety audit (is the process being followed?) | Optional | Optional | Advised | Required | Required |
| Functional safety assessment (is the result safe?) | Optional | Optional | Advised | Required | Required |
| Independence of the reviewer or assessor | — | Another person | Another person | Another team | Another department or organization |

The required independence also grows with the ASIL. The levels are usually written I0
to I3: I0 means the measure should be done, I1 by a different person, I2 by a person
from a different team, and I3 by a person from a different department or organization.

**For this library:** as ASIL B(D), it follows the ASIL B column for development
methods, while measures tied to the original ASIL D, such as the confirmation measures
and the dependent failure analysis, respect ASIL D. The project already plans full
branch coverage (required at B) and measures MC/DC as extra evidence (required at D);
see the [verification strategy](verification-strategy.md).

## How we arrived at ASIL B(D)

This is the reasoning behind the assumed ASIL, step by step.

1. **There is no HARA for a library.** A JSON parser is not an item: it has no vehicle
   function of its own, so severity, exposure and controllability cannot be rated for
   it. As a Safety Element out of Context, it **assumes** an ASIL instead, and each
   integrator's HARA must confirm it.
2. **We do not know the projects it will be used in.** The library is meant for anything
   from a microcontroller to an HPC (A-01), so the assumption has to cover the worst case.
3. **It runs while the vehicle is driving.** It parses data at run time, and the planned
   serializer will send data to other ECUs over networks such as Ethernet. The library is
   in the data path in both directions, so a fault could feed wrong data into a function
   while driving.
4. **So the worst case is ASIL D.** If that data drives a function where a wrong value can
   kill (severity S3), in a situation that occurs on almost every drive (E4), where the
   driver cannot react in time (C3), the table gives **S3 + E4 + C3 = ASIL D**.
5. **Developing the library itself to ASIL D would be very expensive**, and it is not
   necessary, because automotive systems already protect such data paths with an
   independent mechanism: end-to-end protection (E2E), such as AUTOSAR E2E with a CRC, a
   sequence counter and a timeout check, plus plausibility checks on the values.
6. **So ASIL D is decomposed into B(D) + B(D).** One part is the library. The other,
   independent part is the integrator's E2E protection and plausibility checks.
   **This is a conditional assumption, not a given.** E2E protection only shows that the
   bytes arrived intact; it cannot tell whether the parser then read them correctly.
   A wrong value that is still plausible can also pass a range check. So the decomposition
   holds only if the integrator **shows** that their independent part detects the
   relevant parser failures for the safety requirement it protects (A-08).
7. **Result: the library is developed to ASIL B(D).** The second part is an assumption on
   the integrator (A-08), and its independence is covered by the dependent failure
   analysis (DFA-02 in the [software safety analysis](software-safety-analysis.md#2-dependent-failure-analysis)).
   Without that second part, the library alone does not support ASIL D.

## A-01 Target platforms

The library is assumed to run on any platform from a bare-metal microcontroller
up to a high-performance computer (HPC).

- No operating system, file system, threads, clock or other OS service is assumed.
- The integrator compiles the library with a C++17 compiler for their target. The
  integrator is responsible for checking that their compiler, standard library and
  build settings match the configuration that was verified.
- Because small microcontrollers have little memory and computing headroom, stack
  usage and execution time must be bounded and documented. The integrator must check
  them against their own stack size and timing budget.

## A-02 Concurrency: one thread per document

A document is used by only one thread at a time.

- A document includes its storage buffer and every value, view, string or iterator
  obtained from it.
- Different threads may each parse and use their own documents at the same time.
- On bare metal, parsing and serializing are assumed to run in one task at a time.
- The library does not lock anything. A code search of version 0.2.0 found no mutable
  global state, `thread_local` or `static` mutable data in the parser, so independent
  documents do not interfere with each other.

> **⚠️ !!! INTEGRATOR: YOU MUST BRING YOUR OWN MUTEX !!!**
>
> **The library provides NO locking. If a document, its buffer, or any value, view or
> iterator from it can be reached from more than one thread or task, YOU must protect
> ALL access with your own mutex (lock before the first access, unlock after the last
> one). A lock around a single library call is NOT enough, because values and views
> keep pointing into the document after the call returns. Without your mutex the
> behaviour is undefined!**

## A-03 All input is untrusted

Every input to the library is assumed to be untrusted, always.

- Input may be malformed, truncated, corrupted in transport or deliberately crafted
  to cause harm, for example data received over a vehicle network.
- The library must accept any byte sequence of any length without crashing, hanging
  or causing undefined behaviour. Every failure is reported as an explicit error
  value; nothing is thrown.
- Successfully parsing a document only means it is syntactically valid JSON5 or
  JSON. It does not mean the values in it are correct or plausible for the
  application.

## A-04 The integrator sets the resource limits

The integrator chooses every resource limit for their use case, and the library
enforces them.

- Limits are set in `parse_options::limits` (`resource_limits`): input size,
  nesting depth, number of values and members, string lengths, number and exponent
  length, owned bytes, diagnostics and work units.
- The built-in default values are only a starting point. They are not a safety
  argument. The integrator must choose each limit deliberately, so that worst-case
  memory use and parsing time fit their memory, stack and timing budget.
- The library guarantees that it never goes past a configured limit. Limits are
  **inclusive**: using exactly the limit is allowed, for example an input of exactly
  `max_input_bytes`. When an operation would **exceed** a limit, it stops before going
  past it and returns an explicit resource-limit error.

## A-05 Memory: no heap, and which library is which

The heap is never used in a safety-related build. The repository builds two
libraries, and only one of them is in safety scope.

| Library | CMake target | Where memory comes from | Safety scope |
| --- | --- | --- | --- |
| Parser library | `AdaptiveJSON5::parser` | Only a buffer or memory functions that the integrator passes in. It never calls `new`, `malloc` or any other heap function. | **In scope, ASIL B(D)** |
| Hosted add-on | `AdaptiveJSON5::hosted` | The global heap, through non-throwing `operator new`. It is still capped by `max_owned_bytes`. | **Out of scope, QM only** |

- **Parser library.** This is the actual JSON5 and JSON parser. The integrator calls
  `json5_document::parse(input, buffer, buffer_size, options)` with their own buffer,
  or uses the advanced API with their own memory functions (`storage_callbacks`).
  Everything the parser creates lives in that memory.
- **Hosted add-on.** This is a small convenience wrapper for desktop and server use.
  `hosted::parse(input)` takes only the input and gets the memory from the heap. It
  runs the same parser underneath; the only difference is where the memory comes
  from. It is behind the build switch `ADAPTIVE_JSON5_BUILD_HOSTED`, which is off by
  default, and it is not included in the release packages. The quick start in the
  README uses it only because the first example is shorter that way.
- A safety-related build must have `ADAPTIVE_JSON5_BUILD_HOSTED=OFF` and must link
  only `AdaptiveJSON5::parser`. The hosted add-on may be used for tools, tests and
  desktop use, never in a safety-related function.
- If the integrator supplies their own memory functions (`storage_callbacks`), those
  functions must not use the heap either. They must hand out memory from storage that
  is reserved in advance, for example a static buffer or a fixed pool.

## A-06 Every result is checked, and any error rejects the whole document

The integrator checks every result the library returns, and treats any error as
"this data is invalid".

- This applies to every call that returns a result: parsing, and every later access
  and conversion, such as looking up a member or converting a number with
  `as_int64()`, `as_uint64()` or `as_double()`.
- If any error occurs, the integrator rejects the **whole** document. A document that
  produced an error is never partly used.
- Only the integrator knows what "safe" means in their system, so the integrator
  defines the reaction: for example, keeping the last valid data, using a safe
  default, or switching to a degraded mode.
- Results are marked `[[nodiscard]]`, so the compiler warns when a result is ignored.
  This helps, but it is not proof that every result is handled correctly, so the
  integrator remains responsible for it.

## A-07 Hardware faults and freedom from interference

The library does not detect or handle random hardware faults, and it does not
protect itself from other software.

- Faults such as bit flips in RAM, faulty registers or a misbehaving CPU are handled
  by the integrator's hardware and safety mechanisms, for example ECC memory, lockstep
  cores and the end-to-end protection in A-08.
- If the library runs on the same processor as software with a different ASIL or QM
  software, the integrator must provide freedom from interference, for example with
  memory protection (MPU or MMU), separate partitions, and timing supervision such as
  a watchdog. Other software must not be able to corrupt the library's input,
  buffers, documents or stack.

## A-08 Decomposition: the integrator provides an independent E2E check

The integrator provides the second, independent ASIL B(D) part of the decomposition.

- That part consists of end-to-end protection on every message the library parses,
  for example AUTOSAR E2E with a CRC, a sequence counter and a timeout check, plus
  plausibility checks on the values the application takes from parsed documents.
- **E2E protection alone does not detect parser errors.** A CRC only shows that the
  input bytes are intact; a correctly transmitted message can still be parsed into a
  wrong value, and a wrong but plausible value can pass a range check. The integrator
  must therefore **show** that the independent part detects the parser failures that
  matter for the safety requirement it protects (for example the effects in the
  [software FMEA](software-safety-analysis.md#15-fmea-table), such as a wrong value
  reported as success), for instance with redundant information, cross-checks against
  independent sources, or a second, diverse decoding of the critical values.
- That part must be truly independent of this library. It must not use this library,
  or code shared with it, to do its job, so that a single fault cannot break both
  parts at the same time. The integrator documents this independence in their own
  dependent failure analysis.
- Without this second part, the library alone does not support ASIL D. In that case
  the integrator has to do their own analysis.
