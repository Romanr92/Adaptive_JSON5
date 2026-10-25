# P002 result design

Design recorded on 2026-10-13. Implementation and executed unit
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

This describes the selected API shape; implementation and verification are
tracked separately. No AUTOSAR SDK, namespace, runtime or implementation is incorporated.
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
  only where the payload contract supports them. Value-bearing result assignment
  is disabled; emplacement is not exposed. The void specialization currently
  retains generated assignment and requires non-throwing error assignment.
- A value-bearing result must not acquire a third, valueless state on an exposed
  operation. Value-bearing storage is a private `std::variant<T, E>` with value
  at index 0 and error at index 1. Construction is selected explicitly by index,
  including when T and E are identical. Non-throwing alternative construction
  and destruction are constrained at compile time, assignment is disabled and
  checked inspection uses `std::get_if`. The void specialization uses
  `std::optional<E>` to represent success by the absence of an error.

## Verification scope

GoogleTest coverage includes both result forms, failure codes, location
boundaries, checked inspection, compile-time guarantees, supported copy/move
behavior, move-only payloads, same-type value/error alternatives and balanced
destruction. The same behavior is checked with exceptions OFF and ON and RTTI
OFF. Implementation existence does not itself establish verification.
