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
- The library guarantees that it never goes past a configured limit. When a limit
  is reached, parsing stops and returns an explicit resource-limit error.
