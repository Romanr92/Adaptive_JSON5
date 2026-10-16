# Teaching progress

Updated 2026-10-16. Track individual demonstrated concepts, not whole-language
mastery. Review implementations before recording independent success. Preserve
normal code comments; exercise placeholders use concise `/* ... */` blocks.

## Independently completed

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

- Nested result/optional inspection: the numeric wrapper initially used
  `value()` where `has_value()` was needed. Corrected after explanation and
  reviewed. This correction is not independent mastery of the concept.
