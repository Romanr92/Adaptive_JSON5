# Teaching progress

Updated 2026-10-17. Track individual demonstrated concepts, not whole-language
mastery. Review implementations before recording independent success. Preserve
normal code comments; exercise placeholders use concise `/* ... */` blocks.

## Independently completed

- Peak-counter update: P031 `accounted_storage::allocate_callback()` compares
  current outstanding bytes with the recorded peak and updates the peak only
  when current usage is greater. Independently filled the exercise and reviewed
  as correct. Use a concise exercise block for later applications of this concept.
- Optional-sign handling: P027 `consume_non_finite()` safely checks the optional
  byte, consumes at most one sign and advances only the private cursor. Reviewed
  as correct. Use an exercise block for later applications of this operation.
- Explicit error propagation: P028 `check_unquoted_token_boundary()` checks
  decoding success before payload access and returns the stored error through
  `status_result::failure`. Reviewed as correct. The numeric wrapper's boundary
  error guard was also independently correct. Use exercise blocks for these
  error-propagation operations in later lessons. P029 recognition and allocation
  failure guards were also independently completed and reviewed as correct.
  The P030 append failure guard's `result<value_storage>` versus
  `result<value_index>` mismatch was identified by the learner as a typing
  mistake, not a conceptual difficulty. Keep this concept learned and continue
  using exercise placeholders rather than supplying its implementation.

## Retry after further guided practice

- Committing a successful private cursor to the caller: the scalar finish helper
  exercise needed help. Supplied `source = candidate;` and explained references,
  cursor copies and ordering after validation/result construction. Keep guided
  support for now and retry this operation later rather than immediately.

## Still developing

- Error-offset rebasing: P032 numeric substring errors required explanation and
  a supplied implementation. Learner explicitly wants to keep practicing this
  concept; offer a concise exercise with a concrete offset example at the next
  suitable occurrence. Do not demote learned error propagation or count this as
  abandoning the exercise. Current implementation was reviewed as correct with
  supplied guidance, not as independent mastery.
- Nested result/optional inspection: the numeric wrapper initially used
  `value()` where `has_value()` was needed. Corrected after explanation and
  reviewed. This correction is not independent mastery of the concept.
  P039 `locate()` used supplied code for `result<std::optional<...>>`
  inspection; reviewed as correct, still supplied rather than independent.
- Error-propagation exercise blocks (2026-10-25): across the P038 floating,
  comparison and P039 warning-list lessons the learner filled the guards
  correctly, except one `warning_list::reserve()` guard that returned
  `*required.has_value()` instead of `*required.error_if()`. Treat it as a
  slip, like the earlier P030 typing mistake; keep the concept learned and keep
  using exercise blocks.
