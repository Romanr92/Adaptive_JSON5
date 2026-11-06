# Software safety analysis

| Item | Value |
| --- | --- |
| Work product | Software safety analysis at the architectural level (ISO 26262-6, clause 7; method from ISO 26262-9, clause 8) and dependent failure analysis (ISO 26262-9, clause 7) |
| Issue | [#138](https://github.com/Romanr92/Adaptive_JSON5/issues/138) |
| Status | Draft, not reviewed. Method, scope and dependent failure analysis are agreed; the FMEA rows are still to be done. |
| Scope | Parser library `AdaptiveJSON5::parser` only. The hosted add-on is QM and is not analysed. The planned serializer is not covered yet. |

This document has two parts. The **software FMEA** looks at each part of the
library and asks how it could fail. The **dependent failure analysis (DFA)** looks
for a single cause that could break two things at the same time.

The architecture these analyses are based on is described in the existing
[parser architecture](../../core/docs/design-architecture.md) and the ADRs. A
safety view of the architecture (interfaces and the errors each part can return)
is still to be done; see the [safety plan](safety-plan.md).

## 1. Software FMEA

### 1.1 Method

I don't use occurrence or detection scores in this FMEA. Software faults are
systematic, not random, so any probability I gave them would be a guess. Instead,
for every failure mode I show which safety measure prevents it and which test
proves that.

A row without a safety measure or without a test is a **gap**. Every gap needs an
action: a new or changed safety requirement, a code change, or a new unit test.

### 1.2 Where the effect stops

The effect of each failure mode is described **at the library boundary**: what the
integrator's code actually sees. For example, `as_int64()` returns 300 when the
input said 3000, and still reports success.

The analysis stops there. What that effect means in the vehicle depends on how the
integrator uses the value, so it belongs in the integrator's own safety analysis,
which takes the effects described here as its starting point.

### 1.3 Parts analysed

| Part | What it does | Code |
| --- | --- | --- |
| Input | Takes the caller's input bytes and tracks positions in them. | `includes/adaptive_json5/input.hpp`, `core/src/input/` |
| Lexer and Unicode handling | Turns bytes into tokens: whitespace, comments, strings, names, numbers and literals. Decodes UTF-8 and UTF-16 escapes. | `core/src/lexer/`, `core/src/unicode/` |
| Parser | Checks the grammar, builds the document and enforces nesting and work limits. | `core/src/parser/` |
| Value model and conversions | Stores values and gives checked access, member lookup and number conversions. | `core/src/value/` |
| Memory | Hands out storage from the caller's buffer or memory functions and enforces memory limits. | `core/src/memory/` |
| Diagnostics | Builds errors, warnings and line and column positions. | `core/src/diagnostics/`, `includes/adaptive_json5/error.hpp` |
| Public API | The functions and types the integrator calls, including misuse such as using a moved-from document. | `includes/adaptive_json5/` |

### 1.4 Failure modes

Each part is checked against these five failure modes:

| ID | Failure mode | Effect at the library boundary |
| --- | --- | --- |
| FM-1 | A wrong value is accepted | The integrator gets a value that does not match the input, and the result reports success. |
| FM-2 | Valid input is rejected | The integrator gets an error for input that is valid, so correct data is lost. |
| FM-3 | The library hangs or runs too long | A call does not return within the time the integrator planned for. |
| FM-4 | Memory is corrupted | The library reads or writes memory it does not own, or its own data becomes inconsistent. |
| FM-5 | The wrong error is reported | A failure is reported, but with the wrong error category or location, so the integrator reacts in the wrong way. |

### 1.5 FMEA table

Each row has these columns: part, failure mode, effect at the library boundary,
software safety requirement, safety measure, and test that shows it.

**To do:** the rows are drafted from the code and the existing design documents,
then reviewed with the owner part by part before they are written in here.

## 2. Dependent failure analysis

Each entry names a cause that could break more than one thing at once, what the
library does to prevent it, and what the integrator must do to contain it.

| ID | Dependent failure | What could happen | Prevention in the library | Containment by the integrator | Related |
| --- | --- | --- | --- | --- | --- |
| DFA-01 | Two documents share a buffer | The integrator gives the same or overlapping memory to two documents, or lets the input overlap the buffer. One document then overwrites the other. | The buffer contract is documented: each buffer is reserved for one document only, and the input must not overlap it. | Give every document its own buffer, and keep input and buffer apart. This becomes an obligation of use in the Safety Manual. | SSR-08, A-02, A-05 |
| DFA-02 | The E2E check and the parser share code or a common bug | One fault breaks both parts of the ASIL B(D) decomposition at once, so the E2E check misses the error the parser made. | None possible inside the library. | The E2E check must not use this library or code shared with it (A-08), and the integrator documents this in their own DFA. | A-08 |
| DFA-03 | A shared toolchain | The same compiler or standard library bug affects both the library and the E2E check. | Tool confidence evaluation for the library's tools (work product 7). | Can only be noted and passed on: the integrator evaluates their own toolchain, for example with tool qualification or by building the two parts with different tools. | Work product 7 |
| DFA-04 | Buffer overrun | The library writes past its buffer and corrupts whatever is next to it in memory, such as another document or the E2E check's own data. One bug then breaks both parts of the decomposition. | SSR-08: the library never writes outside the memory it was given. Shown by unit tests; further methods are decided in the verification strategy (work product 6). | Memory protection (MPU or MMU) so that an overrun cannot reach the other part (A-07). | SSR-08, A-07 |
| DFA-05 | Stack overflow | Deeply nested or crafted input makes the stack grow into neighbouring memory. This is the same kind of cascade as DFA-04, but through the stack. | SSR-07: stack use has a known upper limit that depends only on the configured limits. The parser keeps open containers in an explicit stack in caller storage instead of using recursion (see the [parser architecture](../../core/docs/design-architecture.md), section 7); the actual stack use is still to be measured (#136). | A stack guard or MPU region below the stack, and a stack size checked against the documented limit (A-07). | SSR-07, A-01, A-07 |
| DFA-06 | Timing interference | A very large or crafted input keeps the CPU busy so long that other tasks, including the E2E check, miss their deadlines. | SSR-02: the work budget (`max_work_units`) and the other limits stop a parse that would take too long. | A watchdog or other timing supervision, and limits chosen to fit the timing budget (A-04, A-07). | SSR-02, A-04, A-07 |
