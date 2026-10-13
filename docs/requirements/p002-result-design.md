# P002 result design

Decision selected by the user on 2026-10-13. Implementation and executed unit
evidence are tracked separately. This decision does not declare the broader
J5 contracts implemented or verified.

## Contract and API shape

P002 introduces an explicit success/error foundation. Its linked contracts are
J5-003 (non-throwing errors), J5-043 (structured categories) and J5-045
(allocation-free minimal reporting).

Use a project-owned C++17 value-or-error template inspired by the model of
AUTOSAR Adaptive's `ara::core::Result`, with a specialization for operations
without a success payload:

```cpp
template<typename T, typename E = error>
class result;

template<typename E>
class result<void, E>;

using status_result = result<void>;
```

This is a design sketch, not an implemented public API. Existing code currently
provides a concrete `status_result`; adapting that code is the developer's next
step. No AUTOSAR SDK, namespace, runtime or implementation is incorporated.
The [AUTOSAR interface guidelines](https://autosar.org/fileadmin/standards/R23-11/AP/AUTOSAR_AP_EXP_InterfacesGuidelines.pdf)
provide conceptual background; this project's requirements govern behavior.

## Required behavior and project design choices

- Each constructed result represents exactly one alternative: success or error.
  The `void` specialization represents success by the absence of an error.
- Public named factories are `success(...)` and `failure(...)`; their supporting
  constructors remain private as a readability choice, not a P002 mandate.
- `has_value()` queries the state. `value_if()` on a value-bearing result and
  `error_if()` return pointers to the active alternative, or null when absent.
  The `void` specialization has no success-value accessor.
- Inspection is non-throwing. Pointers refer to result-owned storage; they must
  not outlive that storage or be assumed stable across state-changing operations.
- The wrapper stores alternatives inline. This does not imply that a success
  payload's own storage is allocation-free. Payload storage follows its separate
  fallible-allocation contract.
- Failure construction and minimal error inspection require no allocation and
  have identical behavior with compiler exceptions OFF and ON.
- No throwing accessors, wrong-alternative assertions/termination or hidden
  exception-dependent recovery are provided.
- Custom `E` types must support the non-throwing construction and destruction
  operations the specialization uses. Reject incompatible types at compile time;
  unconditional `noexcept` alone must not hide a potentially throwing operation.
- Value-bearing results must support move-only payloads and constrain the
  construction/move/destruction paths they expose. Copy operations are provided
  only where the payload contract supports them. Assignment and emplacement are
  deferred until active-alternative lifetime handling has been designed and tested.
- A value-bearing result must not acquire a third, valueless state on an exposed
  operation. Its storage implementation is still to be selected; an arbitrary
  `std::variant` or tagged union is not assumed correct without lifetime analysis.

## Incremental implementation order

1. Adapt the existing class to `result<void, E>` and retain `status_result` as an
   alias. Keep its error in `std::optional<E>` and enforce the supported error
   type's non-throwing properties.
2. Verify success, failure, location boundaries, checked inspection, compile-time
   guarantees and supported copy/move behavior using GoogleTest in both exception
   configurations, with RTTI OFF.
3. Design and implement `result<T, E>` storage and lifetime handling, initially
   exposing only the operations needed by following foundation steps. Verify
   both alternatives, move-only values, same-type value/error alternatives and
   balanced destruction before using it in P003.

Use the `m_` member naming convention in [coding style](../coding-style.md).
Production headers remain the human developer's responsibility. The agent may
connect CMake and write units against the actual declarations, but must not
provide a substitute implementation in test helpers.
