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
- That part must be truly independent of this library. It must not use this library,
  or code shared with it, to do its job, so that a single fault cannot break both
  parts at the same time. The integrator documents this independence in their own
  dependent failure analysis.
- Without this second part, the library alone does not support ASIL D. In that case
  the integrator has to do their own analysis.
