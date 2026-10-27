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

- Scoped-enum `switch` mapping (2026-10-25): #96 step 3b `describe()` for
  `numeric_conversion_reason` and `string_warning_code` written independently from
  exercise blocks, with every enumerator, no `default:` and the fallback after the
  switch. One typo in a message literal (`escapeg`), a slip rather than a concept
  issue. Use exercise blocks for later enum-to-value mappings.

## Exercise block wording (learner feedback, 2026-10-27)

The step 7 lesson 2 block "Build an identifier_measure from totals, start and the
distance counting moved, then move source to counting and return the measurement
as success" was unclear to the learner. Write every multi-step exercise block as:

- numbered steps, one action per line;
- exact names from the code (`counting.offset() - start`), never a paraphrase
  such as "the distance counting moved";
- the field-to-value mapping when a struct is built, and any formula spelled out;
- a pointer to an earlier identical pattern when one exists ("like in
  `measure_string`").

One-line guard blocks for learned concepts stay as they are.

## Retry after further guided practice

- Committing a successful private cursor to the caller: the scalar finish helper
  exercise needed help. Supplied `source = candidate;` and explained references,
  cursor copies and ordering after validation/result construction. Keep guided
  support for now and retry this operation later rather than immediately.
  2026-10-27, ADR 0003 step 7 lesson 2 (`measure_identifier`): the combined
  block "build the measurement by aggregate initialisation, commit the cursor,
  return success" needed help. Supplied all three lines and explained field
  order, `counting.offset() - start` and the commit. Lesson 1's same ending had
  been supplied, so this was its first exercise. Keep it supplied in lesson 3,
  then retry it as a block with a pointer to the struct's field order.

- Forwarding a same-typed result (2026-10-25): #96 step 3c `format_warning`
  wrapped `format_warning_at(...)` in `result<std::size_t>::success(...)`, passed
  `position.value_if()` without `*` and applied `std::move` to a raw pointer.
  Learner asked for help; explained pass-through returns, dereferencing
  `value_if()` and why moving a pointer is pointless. The surrounding
  error-propagation guards were again correct. Learner explicitly asked to
  retry this very soon: offer a concise exercise block at the next function
  that returns another call's same-typed result, without supplying the line.
  Retried 2026-10-26 (ADR 0003 step 3, `consume_scalar` literal branch): again
  wrapped `finish_scalar_token(...)` in `scalar_result::success(...)`. Explained
  that the call already returns `result<std::optional<scalar_token>>`. Keep
  guided; retry later with a short pointer to the callee's return type.
  Retried 2026-10-26 (ADR 0003 step 4, `finish_limited_scan`) with that short
  pointer: returned `work.charge(...)` directly, correctly. Passed a wrong
  argument (`view_end` instead of `end_offset`), which is a separate slip. Next
  time offer the exercise without the return-type hint; if it succeeds, move
  this concept to Independently completed.

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
  2026-10-26 (ADR 0003 step 4, `read_name`): while moving the supplied no-match
  check, changed `!recognized.value_if()->has_value()` to
  `!recognized.has_value()`, so an empty optional was dereferenced (segfault).
  The same lesson's error guard used `!started.error_if()` instead of
  `!started.has_value()`. Explained outer result versus inner optional and what
  `error_if()` returns. Keep supplying nested inspections with an explanation.
  2026-10-27 (ADR 0003 step 7 lesson 2, `consume_identifier`): the no-match
  check `!measured.value_if()->has_value()` was missing after the rewrite. The
  lesson supplied it without an intent comment and its notes did not say that
  the old check (`totals.units == 0`, after `totals`) had to move before
  `totals` and change form, so editing the old function in place hid it. This
  was a lesson-presentation gap, not a misunderstanding: once its role was
  explained, the learner placed it correctly. Keep supplying the check with an
  intent comment, and in review check that it sits between the failure guard
  and the double dereference.
- Error-propagation exercise blocks (2026-10-25): across the P038 floating,
  comparison and P039 warning-list lessons the learner filled the guards
  correctly, except one `warning_list::reserve()` guard that returned
  `*required.has_value()` instead of `*required.error_if()`. Treat it as a
  slip, like the earlier P030 typing mistake; keep the concept learned and keep
  using exercise blocks.
- Failure-guard condition (2026-10-26, ADR 0003 step 4): two inverted guards in a
  row, `!started.error_if()` in `read_name` and `if (rescan_charged.has_value())`
  (missing `!`) in `retain_warnings`; both crashed on the success path. The
  return lines were right each time. The guard condition itself needs attention:
  in the next exercises, ask for the whole guard and point out in review that the
  condition must be true only on failure.
  Same day, lesson 4 (`consume_primitive`): wrote the whole guard for
  `started`, condition included, correctly. One success; give one or two more
  whole-guard exercises before recording it as learned.
  2026-10-26, step 5 lesson 1 (`reserve_exact`): whole allocation guard correct
  again. Two successes in a row; one more, then record as learned.
  2026-10-27, step 5 lesson 2 (`consume_string`): the guard exercise was left
  as the placeholder, and the learner asked the agent to write it at the end of
  the day. That's not evidence either way; offer the next whole-guard exercise.
  2026-10-27, step 5 lesson 3 (`consume_identifier`): whole guard correct, the
  third independent success. Treat the whole failure guard as learned: from now
  on use a one-line exercise block for it.
  2026-10-27, step 9 lesson 1 (`big_unsigned::reserve`): the first one-line
  block came back as `if (!allocation.error_if())`, inverted. The learner
  confirmed this was inattention, not a misunderstanding: they understand the
  guard. Treat it as a slip; the concept stays learned and needs no extra hints.
  Just point the slip out in review.
