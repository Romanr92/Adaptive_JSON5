# ADR 0003: Storage and budget decisions from the allocation audit

Status: All decisions accepted by the user, 2026-10-25. Decisions were recorded one at a time as the user made them;
implementation starts only after the whole list is decided. Production C++ is
written by the human developer; this record does not claim implementation or
verification.

Context: the 2026-10-25 audit of allocation, growth and recursion paths in the
parser, numeric code and diagnostics. Related contracts: J5-046 (fallible
allocation), J5-047 (configurable limits), J5-048 (bounded traversal) and J5-049
(work budgets). The pending units are in
`core/tests/memory/allocation_audit_tests.cpp`.

| # | Topic | Status |
|---|---|---|
| 1 | `big_unsigned` growth | Decided: A2 pre-size; B1 10^4 chunks in this PR; 1c charging |
| 2 | `decoded_string` capacity after `finish` | Decided: S4, count units then decode once |
| 3 | Allocation-free literal recognition | Decided: L1, compare source bytes |
| 4 | `warning_list` own capacity limit | Decided: W2 default; adaptive policy opt-in (T = 64, doubling); full table fails |
| 5 | Scan charging before or after lexing | Decided: C4, budget-limited view |
| 6 | `attach_child` ancestor walk | Decided: AW1, parser path without walk (rationale corrected: analyzability, not cost); public builder keeps it |
| 7 | `value_storage` relocation charging | Decided: V3, counting pre-pass and exact reservation; default work 131072 |
| 8 | Single or duplicated string-unit counter | Decided: K1, single counter in `resource_budget` |
| 9 | Reservation rollback on failure | Decided: R1, single-use parser state, no rollback |

## Decision 1: pre-size `big_unsigned` from the numeric spelling (A2)

Decided by the user, 2026-10-25.

### Problem

`big_unsigned::append_digit` (`core/src/value/big_unsigned.cpp`) allocates a
replacement of exactly `m_size + 1` limbs on every carry, copies the old limbs
and releases the old block. Loading 1000 decimal digits makes about 200 scratch
allocations, so the number of provider calls grows with the length of the number.

### Decision

Each conversion or comparison computes an upper bound for each exact integer it
will build. It reserves that capacity once, before loading any digits. After
that, arithmetic only writes into the reserved storage.

- **Bound from the spelling.** The bound comes from values already known after
  `analyse_number` and the decimal-part scan: significand digit count, radix,
  scale magnitude and the fixed shift headroom (`c_quotient_bits` for floating
  conversion). Decimal digits contribute at most `ceil(digits * 10 / 3)` bits
  (10/3 > log2 10), hexadecimal digits contribute 4 bits each, and a power of ten
  10^s contributes at most `ceil(s * 10 / 3)` bits. Add the shift headroom and one
  limb of margin, then convert the total to limbs.
- **Checked before work or allocation.** The bound uses `checked_add` and
  `checked_multiply`. An overflowing bound, or a reservation larger than
  `max_scratch_bytes`, returns `resource_limit` before any digit work is charged.
- **One reservation per integer.** `floating_conversion` reserves its numerator
  and denominator, and `number_comparison` reserves its two operands. Each
  reservation is one allocation through the conversion's `accounted_storage`
  arena, so it counts against `max_scratch_bytes`.
- **No hidden growth after reserving.** `append_digit`, `shift_left`, `subtract`
  and `assign` never allocate once capacity is reserved. An operation that would
  exceed the reserved capacity means the bound was wrong. It returns
  `wrong_type_or_access` (internal contract violation) and does not grow silently.

### Rationale

- **Known size up front.** The scratch requirement of a conversion is known, and
  checked against the caller's ceiling, before any work is charged. This suits
  embedded callers that size a fixed scratch arena.
- **Few provider calls.** A conversion makes a constant number of scratch
  allocations, independent of digit count. That is two for floating conversion
  and two for comparison, plus any separately decided temporaries.
- **No transient blow-up.** The peak stays at one copy of each integer. Geometric
  growth (A1) can briefly hold the old block plus a block about twice as large.

### Consequences

- `big_unsigned` gains a capacity member and a reserve operation, and every call
  site must reserve before loading. Suggested shape, not an existing API: a
  declaration in `core/includes/value/big_unsigned.hpp` and its definition in
  `core/src/value/big_unsigned.cpp`.
- Both converters compute their bounds in their own translation units
  (`core/src/value/floating_conversion.cpp`, `core/src/value/number_comparison.cpp`).
  A shared bound helper may live beside `big_unsigned` if both need the same formula.
- Over-reservation is bounded by the 10/3 rounding and one limb per integer.
- Unit `J5_046_BigUnsigned.DigitGrowthIsGeometric` must be updated to the reserved
  protocol: reserve, load 1000 digits, and expect exactly one allocation. A
  second unit should check that loading past the reserved capacity fails with
  `wrong_type_or_access` and does not allocate.

### Rejected alternatives

- **A1, geometric capacity:** O(log n) allocations with no call-site changes,
  but the scratch need is only known after the fact and the peak is up to about
  3x the live limbs.
- **A3, unchanged:** one allocation per carry.

### Decision 1b: chunked decimal steps in the same pull request (B1)

Decided by the user, 2026-10-25.

Load decimal digits, and scale by powers of ten, in chunks of up to 4 digits per
step (multiply by 10^k and add the chunk value, with k ≤ 4) instead of one digit
per step. Ship this in the same pull request as A2.

- **Chunk size follows the limb width.** Limbs are 16 bits and multiplies are
  done in 32 bits (`core/src/value/big_unsigned.cpp`). Each step computes
  limb × 10^k + carry, which must stay below 2^32: 65535 × 10^4 + carry fits,
  65535 × 10^5 does not. The maximum chunk is therefore 10^4. No 64-bit
  multiplication is introduced, which keeps the code cheap on 16- and 32-bit
  microcontrollers.
- **Partial chunks.** A digit run that isn't a multiple of 4 ends with a shorter
  chunk (10^1 to 10^3). Scaling by 10^s uses ⌊s/4⌋ steps of 10^4, then one step
  of 10^(s mod 4).
- **Hexadecimal.** One limb holds exactly 4 hexadecimal digits. Hex spellings may
  be packed straight into limbs, which is consistent with the A2 bound of
  4 bits per digit.
- **Scope.** Applies to `big_unsigned` loading and to the scale loops in
  `core/src/value/floating_conversion.cpp` and `core/src/value/number_comparison.cpp`.

Rationale: about 4× fewer limb passes per number, which gives a smaller and more
predictable worst-case CPU time on slow cores. Work accounting for `big_unsigned`
changes once, together with A2, instead of in two separate pull requests. The A2
size bound depends only on the value, not on the step size, so it is unchanged.

Consequences: work-unit expectations in existing numeric units may change and
must be re-baselined in this pull request. Add units for chunk boundaries:
digit counts of 1–5 and 8–9, and scale exponents ≡ 0–3 (mod 4).

Rejected: a follow-up pull request. It would be a smaller review, but would
redefine numeric work accounting twice.

### Decision 1c: work charging for exact integers

Decided by the user, 2026-10-25.

- **The reservation is not charged as work.** Its cost is memory, and
  `max_scratch_bytes` already checks it before any work is done.
- **Zero-filling is charged.** If the reservation zero-fills its limbs, charge one
  unit per limb, because that is a real pass over memory.
- **Arithmetic is charged per step, before the step runs.** Charge
  `c_limb_passes × limbs + c_tail_work_units` for each chunk step (decision 1b),
  shift, subtract and compare. A failed charge leaves the integer untouched.
- **The size bound is charged as a constant:** one unit, independent of input size.

Rationale: each limit measures one resource. `max_scratch_bytes` covers RAM and
`max_work_units` covers passes over memory, so integrators can size them
independently. Every loop over limbs is charged and nothing is charged twice.

Rejected: charging reserved bytes as work, which mixes memory and CPU in one
budget; and leaving zero-filling uncharged, which leaves a pass uncharged.

## Decision 2: size decoded strings exactly with a counting pass (S4)

Decided by the user, 2026-10-25, chosen for embedded and resource-constrained targets.

### Problem

`decoded_string_builder` doubles its capacity while decoding, and `finish()` hands
the whole block to the `decoded_string`. Retained strings can therefore hold up to
twice their content for the life of the document, all of it counted against
`max_owned_bytes`. A 33-unit string retains 64 units.

### Decision

Decode each string value and quoted member name in two passes.

1. **Count.** Walk the string body from the opening quote to the closing quote,
   validating escapes, encoding and terminators, and count the UTF-16 units the
   decoded value will hold. Allocate nothing.
2. **Check, then reserve.** Check the count against `max_string_units` and the
   remaining `max_total_string_units` with checked arithmetic. On failure, return
   `resource_limit` with nothing allocated. On success, make one allocation of
   exactly that many units.
3. **Decode.** Walk the body again, writing units into the reserved storage. The
   decode pass never grows the storage. Writing past the counted size means the
   two passes disagree, and returns `wrong_type_or_access` (internal contract
   violation).

The work charge for the second pass is settled by decision 5.

### Rationale

- **No growth.** Strings are never reallocated, so parsing leaves no freed blocks
  of varying sizes in the caller's heap or pool. That avoids fragmentation, which
  causes late out-of-memory failures on small heaps.
- **Exact memory.** Peak and retained bytes equal the decoded size, so callers can
  set `max_owned_bytes` close to the real requirement.
- **Fails before allocating.** Limit failures happen before any allocation, so
  there is nothing to roll back.
- **One rule.** It follows the same principle as decision 1: compute the size,
  reserve once, never grow.

### Consequences

- Each string is scanned twice. That is linear and bounded by `max_input_bytes`,
  but string-heavy input uses up to about two work units per string byte.
- The builder's growth path is replaced by a reserve-exact operation. The lexer
  calls the counting walk before constructing the builder. Suggested shape, not an
  existing API: declarations in `core/includes/memory/decoded_string_builder.hpp`
  and `core/includes/lexer/string/string_lexer.hpp`, definitions in the matching
  `core/src/` files.
- Unit `J5_046_DecodedStringBuilder.FinishRetainsExactUnits` stays valid, but should
  be restated as "reserve exactly, one allocation". A second unit should check that
  appending past the reservation fails without allocating.
- Escape-heavy and non-ASCII strings retain no extra bytes, unlike pre-sizing from
  the source length.

### Rejected alternatives

- **S1, keep the spare capacity:** up to 2x retained memory, and log2 n allocations.
- **S2, shrink on `finish`:** retains the exact size, but has the highest peak and
  the most fragmentation.
- **S3, pre-size from the source byte length:** one allocation, but retains up to
  3x for CJK text and up to 6x for `\uXXXX` escapes.

## Decision 3: recognize value literals by comparing source bytes (L1)

Decided by the user, 2026-10-25.

### Problem

In value position, `consume_scalar` calls `consume_identifier`, which decodes the
identifier into a temporary buffer from the storage provider, compares it with
the literal spellings and releases it. Each literal costs 3–4 allocation attempts
and leaves short-lived blocks in the caller's heap.

### Decision

Recognize `true`, `false`, `null`, `Infinity` and `NaN` in value position by
comparing source bytes directly, then check the identifier boundary after the
spelling. Value-literal recognition makes no allocation and does not use the
storage callbacks.

- **Mode handling is unchanged.** Strict JSON accepts only `true`, `false` and
  `null`. `Infinity` and `NaN`, with or without a sign, remain JSON5-only and keep
  the existing number-token path.
- **Errors are unchanged.** A non-literal identifier in value position (for
  example `[foo]`) and an escaped spelling (for example `true`) fail with the
  same syntax error and the same offset as today. Recognizing a literal makes no
  allocation; rejecting a non-literal may reuse the identifier path, so its errors
  stay identical. Amended by the user, 2026-10-26.
- **The boundary rule is unchanged.** A spelling followed by an identifier
  continuation character (for example `trueish`) is not a literal.
- **Member names are unaffected.** Identifier member names still decode through
  `consume_identifier`, sized per decision 2.

### Rationale

- **Nothing to fail.** There is no heap use and no storage callback, so literal
  recognition cannot fail for memory reasons.
- **Constant cost.** At most 8 byte comparisons per literal.
- **Smaller provider footprint.** A document made only of literals and numbers
  needs the provider only for value storage.

### Consequences

- The `max_identifier_units` argument of `consume_scalar` no longer drives an
  allocation in value position. Whether to keep it in the signature is a
  public/internal API choice for the developer. `consume_scalar` is declared in
  `core/includes/lexer/scalar_lexer.hpp` and defined in
  `core/src/lexer/scalar_lexer.cpp`.
- Unit `J5_046_ScalarLexer.LiteralsDoNotAllocate` becomes the acceptance check. It
  should also cover `Infinity` and `NaN` in JSON5 mode. Existing identifier,
  non-finite and boundary units must still pass unchanged.

### Rejected alternatives

- **L2, decode into a fixed stack buffer:** no allocation, but it keeps decoding
  that serves no purpose and needs a fallback rule for long identifiers.
- **L3, unchanged:** 3–4 allocation attempts per literal.

## Decision 4: warning table sized once by default, hybrid by option; full means failure

Decided by the user, 2026-10-25.

### Problem

`warning_list::reserve` grows by doubling and has no maximum of its own. Its size
stays bounded only because `parser_engine::retain_warnings` reserves against
`max_diagnostics` first. Doubling also leaves spare entries in the parse result.

### Decision

- **Capacity belongs to the type.** `warning_list` receives its maximum entry
  count at construction, taken from `max_diagnostics`. No reservation can exceed it.
- **Default, W2.** On the first warning, allocate the whole table of
  `max_diagnostics` entries in one allocation. It never grows after that. A
  document without warnings allocates nothing, and `max_diagnostics = 0` never
  allocates.
- **Opt-in hybrid.** When the caller enables it, the first warning allocates
  `min(max_diagnostics, T)` entries. Only if more are needed does the table grow,
  never past `max_diagnostics`. While `max_diagnostics ≤ T`, behavior is identical
  to W2.
- **Full table, F1: the parse fails completely.** Warnings beyond `max_diagnostics`
  fail the parse with `resource_limit` and return no document. Each string's
  warnings are checked against the remaining capacity before any of them is
  written, so a string is never retained partially. This keeps the existing
  contract in `includes/adaptive_json5/parser.hpp`: "Every warning is retained;
  more warnings than max_diagnostics fail with resource_limit."

### Rationale

- **Default suits embedded targets.** W2 gives at most one allocation, no
  fragmentation, and a worst case known at configuration time:
  `max_diagnostics × sizeof(string_warning)`, about 12 bytes per entry on 32-bit
  targets and 24 bytes on 64-bit hosts.
- **Hybrid suits hosted tooling.** Tooling that sets a large `max_diagnostics` but
  usually sees few warnings can opt in for a lower typical RAM use.
- **F1 is all or nothing.** A returned document always carries every warning, and
  no requirement or public contract changes.

### Consequences

- The `warning_list` constructor in `includes/adaptive_json5/value/warning_list.hpp`
  gains a capacity parameter: a public-header change for the developer.
- Enabling the hybrid adds a caller-visible switch and a second code path.
- Units: no warnings → no allocation. With the default policy, the first warning
  makes one allocation of `max_diagnostics` entries. `max_diagnostics = 0` fails
  on the first warning without allocating. A string whose warnings exceed the
  remaining slots fails with nothing written. With the hybrid enabled, cover the
  boundaries T−1, T and T+1.

### Decision 4b: hybrid details

Decided by the user, 2026-10-25.

- **Enabling.** A `parse_options` policy enum, suggested shape
  `warning_storage_policy::{fixed, adaptive}`, defaulting to `fixed` (W2). This
  matches the existing `duplicate_name_policy` and `strict_bom_policy` style and
  adds no build configuration.
- **Threshold.** T is a fixed, documented constant: 64 entries. There is no new
  limit field.
- **Growth above T.** Double the capacity, capped at `max_diagnostics`. This keeps
  reallocations logarithmic. Spare entries occur only for callers that opted in,
  never with the default policy.

Consequences: `includes/adaptive_json5/options.hpp` gains the policy field, a
public-header change for the developer. Units for the adaptive policy cover the
first allocation of `min(max_diagnostics, 64)` entries, growth at entries 64 and
65, and the cap at `max_diagnostics`.

### Rejected alternatives

- **W1, exact growth only:** up to one reallocation per warning-bearing string.
- **W3, doubling with a cap:** keeps up to 2x spare entries in every result.
- **W4, no cap and a documented caller duty:** the bound would depend on every
  caller remembering the check.
- **F2, truncate and continue:** a returned document could silently miss warnings,
  and the public contract and requirements would change.

## Decision 5: charge scanning through a budget-limited view (C4)

Decided by the user, 2026-10-25.

### Problem

The container engine charges scanning after the lexer has run (`charge_scanned`
in `read_value`, `read_name` and trivia skipping), so the work budget cannot stop
a long scan. `primitive_parser` charges `source.remaining()` before its single
token. Applying that rule before every token in the engine would grow the total
charge quadratically. Decision 2 adds a second pass over each string, and
`retain_warnings` already re-scans strings without charging them.

### Decision

Every scan runs over a cursor limited to `min(input remaining, work.remaining())`
bytes.

- **Reaching the limit fails.** If the scan reaches the end of the limited view
  while more real input exists, the result is `resource_limit`, **even if the
  lexer reported success**. This prevents a cut token, for example `12345` cut to
  `12`, from being accepted as a shorter token.
- **Otherwise the exact bytes are charged.** If the scan ends before the limit, or
  at the real end of input, charge exactly the bytes consumed and keep the lexer's
  result: success, no-match or its own error, with its original offset.
- **One rule for every scan:** trivia, scalars, member names, both string passes
  of decision 2, and the `retain_warnings` re-scan. Each pass is charged for the
  bytes it reads.
- **`primitive_parser` follows the same rule.** It no longer charges `remaining()`
  up front, so both parsers charge the same way.
- **Scans near the cut (clarified by the user, 2026-10-26).** A lexer cannot tell
  the cut from the real end of input. It may peek up to 2 bytes past what it
  consumes (a comment opener), and `decode_utf8` reports a truncated character at
  its first byte. So when the view is cut, any scan outcome within 3 bytes of the
  end of the view becomes `resource_limit` at the end of the view. That covers
  success, no-match, an error located there, an error whose `source_range` ends
  there, and an error with no location. Any other outcome keeps its own result,
  code and offset.
  - *Why the range counts:* an unknown value identifier is reported at its start,
    even when the identifier lexer read up to the cut (for example `false` cut to
    `fals`). Its error therefore carries the identifier's `source_range`, so the
    parser can see how far the scan reached. Consumers can see this too:
    `unknown_literal` errors now include the range of the whole identifier. Their
    code, offset and reason are unchanged. The margin is
  `c_cut_margin_bytes` in `core/includes/parser/parser_work.hpp`.
  - *Why success needs it:* with input `[ //note⏎1]` and the view ending after
    the first `/`, trivia skipping cannot see the second `/`. It stops before the
    `/` with success, and the dispatcher would then report a false syntax error
    on a valid document.
  - *Cost:* a scan that really ends in the last 3 bytes of the budget, including
    a real syntax error there, is reported as `resource_limit`. This is
    deterministic, and costs at most 3 extra work units per scan at the boundary.
  - *Embedded:* the check needs no allocation, clock or OS service, and nothing
    is added inside the lexer loops. The cursor cannot read past the limited view,
    so a parse never reads more input than its work budget allows, which helps
    bound the time of one call. Integrators who size `max_work_units` tightly
    from measurements should keep a few units of headroom.
- **The warning re-scan is charged up front (clarified by the user, 2026-10-26).**
  The re-scan in `retain_warnings` walks a string the lexer already validated, so
  its byte length is known. Charge that length before the re-scan instead of
  giving it a limited view. This is exact and stops before any work.

### Rationale

- **The work limit bounds CPU time per call.** A scan stops as soon as the budget
  is used up.
- **Linear and exact.** The total charge equals the bytes actually scanned, so a
  work unit stands for roughly the same time on a given target.
- **Nothing on the hot path.** There is no per-byte charge call inside lexer loops,
  and lexer interfaces stay as they are. The check happens once at each call site.

### Consequences

- A helper that builds the limited view and maps an exhausted view to
  `resource_limit` belongs in the parser work helpers. Suggested location, not an
  existing API: declaration in `core/includes/parser/parser_work.hpp`, definition
  in `core/src/parser/parser_work.cpp`.
- A `resource_limit` caused by the cut carries the offset where the view ends.
- Existing work-count expectations in parser and primitive-parser units must be
  re-baselined.
- Pending units stay valid. `J5_049_ParserWork.ChargesScanBeforeLexing` expects
  `resource_limit` when the cut is reached. `J5_049_ParserWork.WarningRescanIsCharged`
  expects the re-scan to be charged its bytes. Add units for a number cut exactly at
  the limit, a token ending exactly at the real end of input with a budget that
  exactly matches, and a lexer error that occurs before the limit (its original
  error must be kept).

### Rejected alternatives

- **C1, charge `remaining()` before every token:** the total charge grows
  quadratically with document size.
- **C2, charge the exact bytes after scanning:** exact, but cannot stop a scan.
- **C3, charge inside the lexers:** exact and prompt, but adds a work parameter to
  every lexer and a charge call to the hot loops.

## Decision 6: the parser attaches without the ancestor walk (AW1)

Decided by the user, 2026-10-25. Rationale corrected on 2026-10-25, see below.

### Correction

The audit originally claimed that every parser attach walks the whole parent
chain, about 131 000 uncharged steps for a document nested 512 deep. **That claim
was wrong.** Open containers stay detached until they close
(`core/src/parser/parser_engine.cpp`: "The new container remains detached until
finish_container()"), and the parser attaches only in `accept_completed_value`,
always to the open container on top of its frame stack. The walk in
`attach_child` therefore inspects one record and stops, and that constant cost is
already charged as `c_attachment_work` (`core/src/parser/parser_attachment.cpp`).
Parser work is already linear in the node count. AW1 is kept for the reasons below,
not for runtime cost.

### Problem

The walk on the parser path is cheap only because of the attach order, an
invariant that lives in comments. The code's own loop bound is the record count
(`values.size()`), so:

- **Timing analysis.** Static worst-case execution time (WCET) tools derive loop
  bounds from code. They must assume O(nodes) per attach and O(nodes²) per parse,
  unless someone writes and justifies a manual annotation.
- **Fragile guarantee.** A future change that attaches containers while they are
  still open would silently restore long walks.
- **Structural coverage.** Called from the parser, the walk's loop iterations
  beyond the first and its cycle-found branch cannot be reached, so statement,
  branch or MC/DC coverage of the parser path needs a justification.

### Decision

- **The parser path has no walk.** The parser attaches through an internal entry
  point without the ancestor walk. It keeps the O(1) checks: valid indices, the
  parent has the right container type, the child is not attached yet and is not
  the root or the parent itself, the member name is present exactly for objects,
  and the source span fits the input.
- **Why this is safe.** The parser attaches only a value it just completed, which
  is not attached anywhere, to the open container on top of its frame stack, which
  is itself detached. A cycle would require the child to be an ancestor of that
  container, which is impossible when the container has no parent.
- **The public builder keeps the walk.** `document::append_element` and
  `document::append_member` keep rejecting cycles through the shared validation
  plus the walk. Their cost is documented as O(depth), bounded by `max_nodes`.

### Rationale

Chosen for embedded and automotive (safety-related) development, where properties
must be shown by analysis, not only observed:

- **Bounded by structure.** The parser's attach path contains no loop, so its
  constant time follows from the code and needs no WCET annotation.
- **Robust to change.** The guarantee no longer depends on the order in which the
  engine attaches containers.
- **Fully coverable.** The parser path is covered by parsing; the cycle check is
  covered by the public builder units.
- **Same runtime cost** as today. This decision changes analyzability, not speed.

### Consequences

- **Shared validation.** In `core/src/value/document_links.cpp`, split
  `attach_child` into the shared O(1) validation and linking plus a separate cycle
  check. `attach_child` keeps its current contract (validation, cycle check,
  linking); the new internal entry point uses validation and linking only.
  Suggested shape, not an existing API: declared in
  `core/includes/value/document_links.hpp`.
- **Access from the parser.** The parser holds a `document&`, whose storage is
  private. Grant access through an internal accessor type that the public header
  only names: a forward declaration of `detail::document_access` and one `friend`
  line in `includes/adaptive_json5/value/document.hpp`, with the accessor defined
  in a private header under `core/includes/value/`. This adds no public operation,
  and the public header does not include the private tree (ADR 0002). The `friend`
  line is a public-header change for the developer.
- **Parser call site.** `core/src/parser/parser_attachment.cpp` calls the accessor
  instead of `append_member` / `append_element`.
- **Documentation.** The `append_element` and `append_member` comments state the
  O(depth) cycle-check cost.
- **Units:** the loop-free entry point rejects every link fault that `attach_child`
  rejects, without changing anything; it rejects an attached parent that
  `attach_child` would accept; it links an element and a named member with its span;
  the public `append_element` and `append_member` still reject cycles (already
  covered). A deep-nesting work unit was considered and rejected: the walk was never
  charged, so measured work cannot detect it.

### Rejected alternatives

- **Keep the walk and rely on the invariant (option 1).** Equal runtime cost, but
  the bound depends on a documented invariant instead of the code's structure.
- **A public `append_parsed_member` operation.** It would expose an unchecked
  attach to every library user.
- **AW2, charge each walk step.** It charges for analysis-visible worst cases that
  never occur.

## Decision 7: count first, then allocate records and frames exactly once (V3)

Decided by the user, 2026-10-25.

### Problem

`value_storage` and `parser_stack` double when full and move every element into
the new block. On the 64-bit host build, `value_record` is 208 bytes and
`parser_frame` is 112 bytes. With the default limits, the final record doubling
briefly holds 512 old and 1024 new records: a 312 KB peak, about 60 % of the
default `max_owned_bytes` (512 KB). Each doubling also leaves a freed block of a
different size behind. Value-storage relocation is not charged as work, while
frame-stack relocation is.

### Decision

Parse in two passes.

1. **Counting pre-pass.** Lex and validate the whole document without building
   anything, counting values and the maximum nesting depth. This pass allocates
   nothing: strings use the counting walk of decision 2, literals use decision 3,
   and numbers already lex without allocating. Limit checks (`max_nodes`,
   `max_members`, `max_depth`, string units, diagnostics) apply during this pass.
   Invalid or over-limit documents fail here, before any allocation.
2. **Exact reservation.** Allocate the value records and the parser frames once
   each, at exactly the counted sizes.
3. **Build pass.** Parse again and construct into the reserved storage. Neither
   container grows. Exceeding a reservation means the two passes disagree and
   returns `wrong_type_or_access` (internal contract violation).

Both passes are charged under decision 5.

### Rationale

Peak and retained memory equal the document's actual needs. Each parse makes two
container allocations, with no relocation and no fragmentation. Invalid input
costs no memory. This follows the rule shared by decisions 1, 2 and 4: count,
allocate once, never grow.

### Consequences

- The input is lexed twice, so work for the same document roughly doubles. The
  default `max_work_units` must be re-baselined; the value is still open (see the
  open point below).
- Error offsets and reasons must be identical whichever pass detects the error.
- Suggested shape, not an existing API: a counting run of the engine in
  `core/includes/parser/parser_engine.hpp` and `core/src/parser/parser_engine.cpp`,
  plus reserve-exact operations on `value_storage`
  (`includes/adaptive_json5/value/document.hpp`, `core/src/value/value_storage.cpp`)
  and `parser_stack` (`core/includes/parser/parser_stack.hpp`,
  `core/src/parser/parser_stack.cpp`).
- Units: one allocation each for records and frames, at exactly the counted sizes;
  syntax and limit failures make no allocation; error offsets are identical to the
  current single-pass parser; work is charged for both passes.

### Decision 7b: default work budget

Decided by the user, 2026-10-25.

The default `max_work_units` becomes **131072**, documented as 8 x
`max_input_bytes`, so integrators who change the input limit can scale the work
limit with it.

Estimated worst cases for a 16384-byte input under decisions 2, 5 and 7. These
are derived from the charging rules, not measured:

| Document shape | Estimate |
|---|---|
| Few nodes, mostly plain strings | about 51 000 |
| Long strings that all carry warnings | about 74 000 |
| 1024 numbers of about 16 bytes | about 57 000 |
| 1024 small nodes, many tokens | about 50 000 |

131072 covers the largest estimate with about 1.75x margin. Doubling the current
default to 65536 would not cover warning-heavy documents.

- **Known exception.** With `duplicate_name_policy::reject_duplicates`, one object
  with 1024 members costs about 524 000 units of name checks. Document beside the
  policy that large objects under this policy need a raised work budget. Faster
  duplicate detection is outside this record.
- **Evidence required.** Add maximum-size worst-case documents, one for each shape
  above. Each must parse under the default limits and fail just below its measured
  requirement, which replaces these estimates with measured results.
- **Public change.** The default lives in
  `includes/adaptive_json5/memory/resource_limits.hpp`. The defaults of the numeric
  conversion and comparison options derive from it and change with it.

### Rejected alternatives

- **V1, doubling with relocation charged:** consistent charging, but keeps the
  1.5x peak and the fragmentation.
- **V2, doubling covered by the construction charge:** the same memory behavior,
  with charging that only holds on average.
- **V4, fixed reservation of `max_nodes` and `max_depth`:** predictable, but every
  parse pays the configured worst case (264 KB with the defaults, even for `{}`).

## Decision 8: one string-unit counter (K1)

Decided by the user, 2026-10-25.

The total of decoded string units is tracked only in `resource_budget`. The
duplicate `parser_engine::m_string_units` is removed. The engine reads the
remaining total from `resource_budget` through a read accessor (suggested, not an
existing API: declared in `core/includes/memory/resource_budget.hpp`, defined in
`core/src/memory/resource_budget.cpp`) to compute the lexer limit,
`min(max_string_units, remaining total)`. With decision 7, the pre-pass and the
build pass use the same counting.

Rationale: every other limit counter already lives only in `resource_budget`. One
source of truth means a future change cannot update one counter and leave the
other stale.

Consequences: no public behavior changes. The consumer guide
([memory and limits](../../docs/memory-and-limits.md#limits-current-behavior))
states how per-string and total limits are counted and that both checks use one
counter. Units: a document whose names and values together exceed
`max_total_string_units` by one unit fails, and one at exactly the limit succeeds.

Rejected: **K2, keep both counters with documentation.** It works today, but the
risk of the counters drifting apart stays.

## Decision 9: single-use parser state, no counter rollback (R1)

Decided by the user, 2026-10-25.

- **Single use.** A `parser_engine` is used for one parse only. After `run()` or
  `run_complete()` returns any result, construct a new engine to parse again.
- **No counter rollback.** After a failure, reservations in `resource_budget`
  (nodes, members, depth, string units, diagnostics) are not undone; the failed
  state is discarded as a whole. Owned memory is still released by destructors
  (RAII), so memory safety does not depend on rollback.
- **The existing rollback stays.** The tested rollback in `open_container` is
  kept. It is harmless, and removing it would be churn.
- **The public contract is unchanged.** `parse_document` builds fresh state for
  every call, publishes no document on failure, releases the call's allocations
  and leaves the `accounted_storage` reusable, as already documented in
  `includes/adaptive_json5/parser.hpp`.

Rationale: with decision 7, limit failures occur in the pre-pass, before any
allocation. In the build pass only allocation failure, the work budget or the
internal disagreement error can fail, so rollback never affects any observable
result. Less code and fewer branches suit embedded targets.

Consequences:

- Document single use on `parser_engine`, `run()` and `run_complete()` in
  `core/includes/parser/parser_engine.hpp`: "Single use: after any result,
  construct a new engine to parse again." This is a developer change.
- **Visible to consumers.** The consumer guide
  ([memory and limits](../../docs/memory-and-limits.md#failure-behavior-current-behavior))
  explains failure behavior in user terms: each call stands alone, there is
  nothing to reset, how to retry, and what happens to `current_bytes()` and
  `peak_bytes()`. It is linked from the README and the embedded guide. The public
  `parse_document` comment may link the guide; editing the public header is a
  developer change.
- Units: existing fresh-engine-after-failure units (for example
  `FailsEachAllocationAndAllowsFreshEngine`) already follow the rule. Add a public
  unit: after a failed `parse_document`, `current_bytes()` returns to its earlier
  value and a second call with the same accounting succeeds.

Rejected: **R2, roll back every reservation.** It is consistent, but adds code
paths and tests whose effect no caller can observe.
