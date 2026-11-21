# Coding-standard deviation register

| Item | Value |
| --- | --- |
| Issues | [#137](https://github.com/Romanr92/Adaptive_JSON5/issues/137) deviation register; [#166](https://github.com/Romanr92/Adaptive_JSON5/issues/166) baseline MISRA findings |
| Standard | MISRA C++:2023, applied as described in [coding-guidelines.md](coding-guidelines.md) |
| Findings source | [misra-baseline-findings.md](misra-baseline-findings.md) (CodeQL `codeql/misra-cpp-coding-standards`) |
| Purpose | Optional audit index. Status records historical review decisions. |

## How deviations are handled

- **A rule is never switched off.** Required MISRA rules stay active in the `misra-cpp`
  workflow, so a new use anywhere in the code is still reported and blocks the pull request
  (#146).
- **Each line is justified in the source.** Start with one
  `// MISRA rule: RULE-21-6-3 cpp/misra/advanced-memory-management-used` line
  per rule and query. Write the rationale in a `// MISRA justification: ...`
  comment with consecutive `//` continuation lines as needed. Put
  `// codeql::misra_deviation_next_line(DEV-001.01)` immediately above the affected
  code, using a unique deviation ID. Review the rationale and scope in the PR
  that adds the comment.
- **The source comments are enough for the scan.** Local and GitHub Actions
  scans use `tools/misra_check.py` to index CodeQL records from the comments. The
  findings report links to a separate page with the finding severity, MISRA rule,
  CodeQL query, source location and rationale. GitHub code scanning receives the
  open findings.
- **Line numbers are informative.** They refer to the tree that was checked when the entry
  was written. The function and the code are what identify a line; update the number when
  code moves.
- **A record is removed when its last line is fixed.** Its ID is not reused.

Status values: **Proposed**, **Approved** (date, by Romanr92), **Rejected** (fix instead),
**Withdrawn** (code removed or fixed).

---

## DEV-001: Rule 21.6.3, construction and destruction in caller-provided storage

| Field | Value |
| --- | --- |
| Rule | MISRA C++:2023 Rule 21.6.3 "Advanced memory management shall not be used" (required) |
| CodeQL query | `cpp/misra/advanced-memory-management-used` |
| Requirements protected | J5-101 (caller-buffer parsing), J5-046 (fallible allocation), J5-040 / J5-051 (lifetime), [embedded.md](../embedded.md); DFA-01 / safety manual C-3.6 |
| Status | Approved (2026-11-20, Romanr92) |

### Why this rule is deviated instead of fixed

The library must work without a heap, on bare metal and inside a classic AUTOSAR
stack. Every object a document needs lives in memory the integrator provides. In
C++17, a non-trivial object can only be created in such memory with placement `new`,
and must be ended with an explicit destructor call. Neither uses the heap: they only
start and end an object's lifetime at an address the library already owns.

The alternatives each break a requirement:

| Alternative | Requirement broken |
| --- | --- |
| Allocate with `new` / `std::allocator` | J5-101 and the no-heap design in embedded.md |
| Static object pools | One buffer per document and re-entrancy (DFA-01, C-3.6); a fixed upper limit not set by the integrator |
| Restrict records to trivial types and copy raw bytes | Loses `owned_block` ownership: blocks would no longer be released exactly once (J5-040, FMEA ME-03) |

### Risk and compensating measures

The risks the rule targets are use of an object outside its lifetime, a missing or double
destructor call, and misaligned or out-of-range placement.

- Every placement uses an address that was aligned and range-checked first
  (`align_within`, `slot_address`), and a slot past the current size is never constructed.
- Each container's destructor ends every live object in reverse order, and owner types
  cannot be copied (J5-040).
- Objects are read through `std::launder` after placement (see DEV-002).
- Verification: unit tests for document storage, parser stack and value storage
  lifetimes; AddressSanitizer and UndefinedBehaviorSanitizer runs; fuzzing (#138).

### Lines

| ID | Location | Code | Justification | Status |
| --- | --- | --- | --- | --- |
| DEV-001.01 | `core/src/memory/buffer_document_storage.cpp:100`, `make_buffer_document_storage()` | `::new (aligned) buffer_document_storage(...)` | Creates the document's control block (arena and accounting) at the start of the caller's buffer, at an address `align_within()` has aligned and checked for size. This is how J5-101 avoids the heap. | Approved (2026-11-20, Romanr92) |
| DEV-001.02 | `core/src/memory/buffer_document_storage.cpp:56`, `release_buffer_document_storage()` | `static_cast<buffer_document_storage*>(block)->~buffer_document_storage();` | Ends the control block created in DEV-001.01. The caller owns the buffer, so it is not freed. `document_storage_owner` calls this exactly once, after every document allocation is released (J5-051). | Approved (2026-11-20, Romanr92) |
| DEV-001.03 | `core/src/parser/parser_stack.cpp:166`, `parser_stack::push()` | `::new (slot_address(m_storage, m_size)) parser_frame(std::move(frame));` | Adds a frame to the parser's nesting stack, in a block obtained from the accounted storage (J5-046). The capacity is checked before the slot is used. | Approved (2026-11-20, Romanr92) |
| DEV-001.04 | `core/src/parser/parser_stack.cpp:217`, `parser_stack::pop()` | `live_frame(m_size)->~parser_frame();` | Ends the frame removed by `pop()`, after `m_size` is reduced; that slot is not used again until it is reconstructed. | Approved (2026-11-20, Romanr92) |
| DEV-001.05 | `core/src/value/value_storage.cpp:300`, `value_storage::append()` | `::new (slot_address(m_storage, m_size)) value_record(std::move(record));` | Adds a value record to the document's record block in accounted storage. Capacity and `max_records` are checked first (J5-047). | Approved (2026-11-20, Romanr92) |
| DEV-001.06 | `core/src/value/value_storage.cpp:163`, growth path | `::new (slot_address(*allocation.value_if(), index)) value_record(std::move(*live_record(index)));` | Moves each record into the new, larger block during growth. The old records are ended (DEV-001.07) before the old block is released. | Approved (2026-11-20, Romanr92) |
| DEV-001.07 | `core/src/value/value_storage.cpp:89`, record teardown | `live_record(m_size)->~value_record();` | Ends records in reverse order before their block is released, so each record's owned blocks are released exactly once (J5-040). | Approved (2026-11-20, Romanr92) |

Since #166, DEV-001.02 also covers Rule 8.2.6 (the `static_cast` from `void*` to the control
block, as in DEV-002), and DEV-001.06 also covers Rule 28.6.3 (each record is moved once and then
only destroyed, see DEV-001.07). The source comments hold both justifications.

---

## DEV-002: Rule 8.2.6, object pointers from `void*` in the storage layer

| Field | Value |
| --- | --- |
| Rule | MISRA C++:2023 Rule 8.2.6 "An object with integral, enumerated, or pointer to void type shall not be cast to a pointer type" (required) |
| CodeQL query | `cpp/misra/int-to-pointer-cast-prohibited` |
| Requirements protected | ADR 0007 (storage callback ABI), J5-046, J5-101, ADR 0008 (text/byte boundary) |
| Status | Approved (2026-11-20, Romanr92) |

### Why this rule is deviated instead of fixed

Two designs need a `static_cast` from `void*`:

1. **The storage callbacks.** `storage_callbacks` holds plain function pointers with a
   `void* context` (ADR 0007), so an integrator can connect a C allocator, an AUTOSAR
   memory service or a static pool without templates, virtual functions or RTTI. The
   library gets the context back as `void*` and must convert it to its own type.
2. **Raw storage.** Allocators (`owned_block::data()`, the provider callbacks) return
   `void*`, because the memory has no type yet. Code that places objects or bytes there
   must convert it to `std::byte*`, to the record type, or to `const char*` for text.

To avoid the cast, the public storage interface would need templates or virtual
interfaces. That reverses ADR 0007, would no longer be a plain function-pointer ABI, and
does not suit integrators with RTTI or virtual dispatch turned off.

### Risk and compensating measures

The rule targets converting to a type the memory does not hold.

- Each context is passed only by the same object that registered it (`callbacks()`
  stores `this`), so the type is fixed by construction.
- Record and frame pointers are read only from slots that were constructed (DEV-001) and
  go through `std::launder`.
- Conversions to `std::byte*` and `const char*` are allowed to access any object's bytes
  (C++17 [basic.lval]/8).
- Verification: unit tests, sanitizers and fuzzing, as in DEV-001.

### Lines

| ID | Location | Code | Justification | Status |
| --- | --- | --- | --- | --- |
| DEV-002.01 | `core/src/memory/accounted_storage.cpp:34`, `allocate_callback()` | `*static_cast<accounted_storage*>(context)` | Gets back the `accounted_storage` that `callbacks()` registered as context (ADR 0007). | Approved (2026-11-20, Romanr92) |
| DEV-002.02 | `core/src/memory/accounted_storage.cpp:101`, `deallocate_callback()` | `*static_cast<accounted_storage*>(context)` | Same as DEV-002.01, for releases. | Approved (2026-11-20, Romanr92) |
| DEV-002.03 | `core/src/memory/buffer_arena.cpp:25`, allocate callback | `*static_cast<buffer_arena*>(context)` | Gets back the `buffer_arena` that registered itself as context. | Approved (2026-11-20, Romanr92) |
| DEV-002.04 | `core/src/memory/buffer_arena.cpp:59`, deallocate callback | `*static_cast<buffer_arena*>(context)` | Same as DEV-002.03, for releases. | Approved (2026-11-20, Romanr92) |
| DEV-002.05 | `core/src/memory/buffer_arena.cpp:81` | `static_cast<std::byte*>(address)` | Views a released block as bytes to compare it with the arena's top. No object is accessed. | Approved (2026-11-20, Romanr92) |
| DEV-002.06 | `core/src/memory/buffer_arena.cpp:100`, constructor | `static_cast<std::byte*>(buffer)` | Views the caller's untyped buffer as bytes for offset arithmetic (J5-101). | Approved (2026-11-20, Romanr92) |
| DEV-002.07 | `core/src/memory/buffer_document_storage.cpp:93` | `static_cast<std::byte*>(aligned) + sizeof(buffer_document_storage)` | Computes where the arena's space starts, directly after the control block (DEV-001.01). | Approved (2026-11-20, Romanr92) |
| DEV-002.08 | `core/src/memory/decoded_string_builder.cpp:45` | `static_cast<std::byte*>(m_storage.data())` | Views an owned block as bytes to write decoded string units. | Approved (2026-11-20, Romanr92) |
| DEV-002.09 | `core/src/parser/parser_stack.cpp:33`, `slot_address()` | `static_cast<std::byte*>(storage.data())` | Views the frame block as bytes to compute a slot's address. | Approved (2026-11-20, Romanr92) |
| DEV-002.10 | `core/src/parser/parser_stack.cpp:50`, `live_frame()` | `std::launder(static_cast<parser_frame*>(slot_address(...)))` | Accesses a frame constructed by DEV-001.03; the index is below `m_size`. | Approved (2026-11-20, Romanr92) |
| DEV-002.11 | `core/src/parser/parser_stack.cpp:66`, `live_frame() const` | `std::launder(static_cast<const parser_frame*>(slot_address(...)))` | Read-only version of DEV-002.10. | Approved (2026-11-20, Romanr92) |
| DEV-002.12 | `core/src/parser/parser_engine.cpp:602`, `reserve_string()` | `*static_cast<parser_engine*>(context)` | Restores the parser instance registered by the string warning sink to reserve units and warnings for that parse. | Approved (2026-11-20, Romanr92) |
| DEV-002.13 | `core/src/parser/parser_engine.cpp:651`, `record_warning()` | `*static_cast<parser_engine*>(context)` | Restores the parser instance registered by the string warning sink to record a warning in its reserved storage. | Approved (2026-11-20, Romanr92) |
| DEV-002.14 | `core/src/parser/parser_engine.cpp:688` | `static_cast<std::byte*>(m_extra_kinds.data())` | Views the overflow block for container kinds as bytes. | Approved (2026-11-20, Romanr92) |
| DEV-002.15 | `core/src/value/big_unsigned.cpp:52` | `static_cast<const std::byte*>(storage.data())` | Reads big-number limbs stored byte-wise in an owned block. | Approved (2026-11-20, Romanr92) |
| DEV-002.16 | `core/src/value/big_unsigned.cpp:75` | `static_cast<std::byte*>(storage.data())` | Writes big-number limbs byte-wise into an owned block. | Approved (2026-11-20, Romanr92) |
| DEV-002.17 | `core/src/value/decoded_string.cpp:122` | `static_cast<const std::byte*>(m_storage.data())` | Reads stored decoded string units. | Approved (2026-11-20, Romanr92) |
| DEV-002.18 | `core/src/value/owned_number.cpp:112`, `spelling()` | `static_cast<const char*>(m_storage.data())` | Returns the copied number spelling as text. The block holds bytes copied from `char` input (ADR 0008). | Approved (2026-11-20, Romanr92) |
| DEV-002.19 | `core/src/value/retained_source.cpp:74`, `text()` | `static_cast<const char*>(m_bytes.data())` | Returns the retained input copy as text (J5-041, ADR 0008). | Approved (2026-11-20, Romanr92) |
| DEV-002.20 | `core/src/value/value_storage.cpp:37`, `slot_address()` | `static_cast<std::byte*>(storage.data())` | Views the record block as bytes to compute a slot's address. | Approved (2026-11-20, Romanr92) |
| DEV-002.21 | `core/src/value/value_storage.cpp:54`, `live_record()` | `std::launder(static_cast<value_record*>(slot_address(...)))` | Accesses a record constructed by DEV-001.05 / DEV-001.06; the index is below `m_size`. | Approved (2026-11-20, Romanr92) |
| DEV-002.22 | `core/src/value/value_storage.cpp:70`, `live_record() const` | `std::launder(static_cast<const value_record*>(slot_address(...)))` | Read-only version of DEV-002.21. | Approved (2026-11-20, Romanr92) |
| DEV-002.23 | `core/src/value/warning_list.cpp:44` | `static_cast<const std::byte*>(storage.data())` | Reads stored warnings byte-wise. | Approved (2026-11-20, Romanr92) |
| DEV-002.24 | `core/src/value/warning_list.cpp:66` | `static_cast<std::byte*>(storage.data())` | Writes warnings byte-wise into an owned block. | Approved (2026-11-20, Romanr92) |
| DEV-002.25 | `core/src/memory/pointer_alignment.cpp:48` | `position = static_cast<std::byte*>(position) + padding;` | Converts the untyped storage pointer to the byte or object type it was created as. This is a static_cast from void*, not an integer-to-pointer conversion. | Proposed |

`core/src/memory/pointer_alignment.cpp:30`, the 25th Rule 8.2.6 finding, is **not** in this
record: it can be removed by computing the padding with `std::align` (see DEV-003.02).

---

## DEV-003: Rules 8.2.5 and 8.2.7, address arithmetic for alignment and overlap checks

| Field | Value |
| --- | --- |
| Rules | MISRA C++:2023 Rule 8.2.5 "`reinterpret_cast` shall not be used" (required); Rule 8.2.7 "A cast should not convert a pointer type to an integral type" (advisory) |
| CodeQL queries | `cpp/misra/reinterpret-cast-shall-not-be-used`, `cpp/misra/no-pointer-to-integral-cast` |
| Requirements protected | `json5_document::parse` precondition "input and buffer must not overlap", safety manual C-3.6, DFA-01, SSR-08; J5-046 (alignment); ADR 0008 |
| Status | Approved (2026-11-20, Romanr92) |

### Why these rules are deviated instead of fixed

- **The overlap check.** `json5_document::parse` rejects an input that overlaps the
  buffer, because the parser would otherwise overwrite input it has not read yet
  (C-3.6, DFA-01, SSR-08). Comparing pointers into different objects with `<` is
  unspecified in C++17; `std::less` is the only portable alternative, and it gives order,
  not distance. The check needs the addresses as integers.
- **Alignment.** Placement (DEV-001) needs aligned addresses. Reading an address as an
  integer is the direct way to compute padding.
- **The char/byte view.** The parser engine reads the `const char*` input as
  `const std::byte*`. Viewing any object's bytes is allowed (C++17 [basic.lval]/8), and
  ADR 0008 puts this conversion at the text/byte boundary.

Removing these casts would remove the overlap check, which is a documented safety
precondition.

### Risk and compensating measures

- The integers are only compared or used for padding; they are never turned back into
  pointers (after DEV-003.02 is fixed).
- `std::uintptr_t` is required on the supported targets; this is an assumption for
  integrators (see A-03 / A-05 in seooc-assumptions.md).
- Verification: `J5_101_Json5Document.RejectsOverlappingInputAndBuffer`,
  `J5_046_BufferDocumentAlignment`, sanitizers.

### Lines

| ID | Location | Code | Rules | Justification | Status |
| --- | --- | --- | --- | --- | --- |
| DEV-003.01 | `core/src/memory/pointer_alignment.cpp:23`, `align_within()` | `reinterpret_cast<std::uintptr_t>(position)` | 8.2.5, 8.2.7 | Reads the address to compute the padding needed for the requested alignment. | Approved (2026-11-20, Romanr92) |
| DEV-003.02 | `core/src/memory/pointer_alignment.cpp:35`, `align_within()` | `reinterpret_cast<void*>(address + padding)` | 8.2.5, 8.2.6 | **Fix proposed instead:** step `position` forward by `padding` bytes through `std::byte*`, or use `std::align`. That also removes the integer-to-pointer conversion. Keep as a deviation only if the owner rejects the fix. | Withdrawn (fixed in #166: the pointer is advanced through `std::byte*`) |
| DEV-003.03 | `core/src/memory/json5_document.cpp:54`, overlap check | `reinterpret_cast<std::uintptr_t>(input.data)` | 8.2.5, 8.2.7 | Start address of the input, for the input/buffer overlap check (C-3.6). | Approved (2026-11-20, Romanr92) |
| DEV-003.04 | `core/src/memory/json5_document.cpp:60`, overlap check | `reinterpret_cast<std::uintptr_t>(buffer)` | 8.2.5, 8.2.7 | Start address of the buffer, for the same check. | Approved (2026-11-20, Romanr92) |
| DEV-003.05 | `core/src/parser/parser_engine.cpp:71` | `reinterpret_cast<const std::byte*>(input.data)` | 8.2.5 | Views the `char` input as bytes at the text/byte boundary (ADR 0008). Byte access to any object is allowed. | Approved (2026-11-20, Romanr92) |

---

## DEV-004: Rule 8.20.1, unsigned arithmetic that cannot wrap

| Field | Value |
| --- | --- |
| Rule | MISRA C++:2023 Rule 8.20.1 |
| CodeQL query | `cpp/misra/unsigned-operation-with-constant-operands-wraps` |
| Lines | 148 |
| Status | Proposed (#166) |

### Why this rule is deviated instead of fixed

Counters, offsets, digit accumulation and limb arithmetic on `std::size_t` and the limb types. Each operation is bounded by a check, a loop condition or a type width that CodeQL's range analysis does not follow (run-time bounds, template constants).

Rewriting each operation with checked helpers would add branches that can never be taken and work against full branch coverage. One intended modular addition (the 128-bit carry in Eisel-Lemire) is documented as such.

### Risk and compensating measures

Each line states the bound it relies on. Unit tests, validation tests, UBSan, fuzzing and the resource-limit tests exercise the bounds.

### Lines

| ID | Location | Code | Rules | Justification | Status |
| --- | --- | --- | --- | --- | --- |
| DEV-004.71 | `core/src/diagnostics/diagnostic_format.cpp:101` | `++m_length;` | 8.20.1 | m_length counts the bytes of one diagnostic message, built from fixed texts and a few numbers, so it stays far below SIZE_MAX. | Proposed |
| DEV-004.70 | `core/src/diagnostics/diagnostic_format.cpp:126` | `--start;` | 8.20.1 | c_max_digits has room for every digit of a std::size_t, so start stays above zero until the last digit is written. | Proposed |
| DEV-004.69 | `core/src/diagnostics/diagnostic_format.cpp:141` | `append(std::string_view{&digits[start], c_max_digits - start});` | 8.20.1 | start is at most c_max_digits, so the difference is the number of digits written. | Proposed |
| DEV-004.68 | `core/src/diagnostics/diagnostic_format.cpp:161` | `m_buffer[(m_length < m_capacity) ? m_length : (m_capacity - 1U)] = '\0';` | 8.20.1 | The enclosing check guarantees m_capacity != 0, so subtracting one cannot wrap. | Proposed |
| DEV-004.72 | `core/src/lexer/identifier/identifier_lexer.cpp:243` | `const identifier_measure measured{totals.units, start, counting.offset() - start, total...` | 8.20.1 | A cursor only moves forward, so the later offset is never below the earlier offset it started from, and the difference is the number of bytes consumed. | Proposed |
| DEV-004.114 | `core/src/lexer/number/decimal_lexer.cpp:240` | `const number_token token{number_kind::decimal, start, candidate.offset() - start};` | 8.20.1 | A cursor only moves forward, so the later offset is never below the earlier offset it started from, and the difference is the number of bytes consumed. | Proposed |
| DEV-004.113 | `core/src/lexer/string/simple_escape.cpp:201` | `error::syntax_at(source.offset() + c_escape_byte_count, syntax_error_reason::invalid_es...` | 8.20.1 | The two escape bytes were read from the input, so offset + c_escape_byte_count is at most the input size. | Proposed |
| DEV-004.76 | `core/src/lexer/string/string_lexer.cpp:112` | `++walk.warning_count;` | 8.20.1 | Counts warnings of one string, at most one per character of the input, so it cannot wrap std::size_t. | Proposed |
| DEV-004.75 | `core/src/lexer/string/string_lexer.cpp:220` | `const source_span open_string{start, source.offset() - start};` | 8.20.1 | A cursor only moves forward, so the later offset is never below the earlier offset it started from, and the difference is the number of bytes consumed. | Proposed |
| DEV-004.74 | `core/src/lexer/string/string_lexer.cpp:356` | `deliver(pairing.escape(*escaped.value_if(), escape_offset, source.offset() - escape_off...` | 8.20.1 | A cursor only moves forward, so the later offset is never below the earlier offset it started from, and the difference is the number of bytes consumed. | Proposed |
| DEV-004.73 | `core/src/lexer/string/string_lexer.cpp:423` | `totals.units,         start, candidate.offset() - start, totals.warning_count, totals.f...` | 8.20.1 | A cursor only moves forward, so the later offset is never below the earlier offset it started from, and the difference is the number of bytes consumed. | Proposed |
| DEV-004.20 | `core/src/lexer/string/string_warning.cpp:37` | `++found.size;` | 8.20.1 | The check above keeps found.size below c_capacity, so the increment reaches at most c_capacity. | Proposed |
| DEV-004.115 | `core/src/lexer/trivia.cpp:175` | `const std::size_t length = candidate.offset() - start;` | 8.20.1 | A cursor only moves forward, so the later offset is never below the earlier offset it started from, and the difference is the number of bytes consumed. | Proposed |
| DEV-004.120 | `core/src/lexer/whitespace.cpp:53` | `++line_break_count;` | 8.20.1 | Counts characters or sequences of the spelling or input being scanned, so the count is at most its length and cannot wrap std::size_t. | Proposed |
| DEV-004.119 | `core/src/lexer/whitespace.cpp:89` | `const std::size_t length = candidate.offset() - start;` | 8.20.1 | A cursor only moves forward, so the later offset is never below the earlier offset it started from, and the difference is the number of bytes consumed. | Proposed |
| DEV-004.128 | `core/src/memory/accounted_storage.cpp:73` | `self.m_current_bytes += bytes;` | 8.20.1 | The byte-ceiling check above proved that this addition stays within the configured maximum. | Proposed |
| DEV-004.127 | `core/src/memory/accounted_storage.cpp:78` | `++self.m_outstanding_blocks;` | 8.20.1 | Counts live blocks; each one occupies at least one byte of the checked byte ceiling, so the count cannot wrap. | Proposed |
| DEV-004.126 | `core/src/memory/accounted_storage.cpp:129` | `self.m_current_bytes -= bytes;` | 8.20.1 | Releases a block that this storage allocated earlier, so its size was added to m_current_bytes before. | Proposed |
| DEV-004.125 | `core/src/memory/accounted_storage.cpp:134` | `--self.m_outstanding_blocks;` | 8.20.1 | Releases a block that this storage allocated earlier, so the count is at least one here. | Proposed |
| DEV-004.124 | `core/src/memory/buffer_arena.cpp:31` | `std::size_t space = self.m_capacity - self.m_used;` | 8.20.1 | The arena never hands out more than its capacity, so m_used is at most m_capacity. | Proposed |
| DEV-004.123 | `core/src/memory/buffer_arena.cpp:47` | `self.m_used = (self.m_capacity - space) + bytes;` | 8.20.1 | align_within() succeeded, so space is at most the capacity and bytes is at most space; the new m_used stays within the capacity. | Proposed |
| DEV-004.122 | `core/src/memory/buffer_arena.cpp:52` | `++self.m_live;` | 8.20.1 | Counts live blocks, each taking at least one byte of the finite arena, so the count cannot wrap. | Proposed |
| DEV-004.121 | `core/src/memory/buffer_arena.cpp:81` | `--self.m_live;` | 8.20.1 | Releases a block that this arena handed out, so the live count is at least one here. | Proposed |
| DEV-004.77 | `core/src/memory/buffer_document_storage.cpp:108` | `const std::size_t allocation_size = space - c_control_bytes;` | 8.20.1 | align_within() checked that space holds the control block, so space is at least c_control_bytes. | Proposed |
| DEV-004.117 | `core/src/memory/pointer_alignment.cpp:28` | `const auto mask = static_cast<std::uintptr_t>(alignment - 1);` | 8.20.1 | Callers pass a validated nonzero power-of-two alignment, so alignment - 1 is the alignment mask. | Proposed |
| DEV-004.116 | `core/src/memory/pointer_alignment.cpp:35` | `const auto padding = static_cast<std::size_t>((static_cast<std::uintptr_t>(alignment) -...` | 8.20.1 | The subtraction is intentionally modular; the result is masked to below alignment, which is the padding to the next aligned address. | Proposed |
| DEV-004.139 | `core/src/memory/resource_budget.cpp:114` | `++m_nodes;` | 8.20.1 | The node limit was checked above, so m_nodes stays at most the configured maximum. | Proposed |
| DEV-004.138 | `core/src/memory/resource_budget.cpp:122` | `++m_members;` | 8.20.1 | The member limit was checked above, so m_members stays at most the configured maximum. | Proposed |
| DEV-004.137 | `core/src/memory/resource_budget.cpp:149` | `--m_nodes;` | 8.20.1 | The check above returns an internal fault when no node is accounted, so m_nodes is at least one. | Proposed |
| DEV-004.136 | `core/src/memory/resource_budget.cpp:156` | `--m_members;` | 8.20.1 | Only a node accounted as a member is released here, so m_members is at least one. | Proposed |
| DEV-004.135 | `core/src/memory/resource_budget.cpp:181` | `++m_depth;` | 8.20.1 | The depth limit was checked above, so m_depth stays at most the configured maximum. | Proposed |
| DEV-004.134 | `core/src/memory/resource_budget.cpp:210` | `--m_depth;` | 8.20.1 | The check above returns an internal fault at depth zero, so m_depth is at least one. | Proposed |
| DEV-004.133 | `core/src/memory/resource_budget.cpp:246` | `m_string_units += additional_units;` | 8.20.1 | checked_add() above computed the new total within the limit, so this addition cannot wrap. | Proposed |
| DEV-004.132 | `core/src/memory/resource_budget.cpp:336` | `++m_diagnostics;` | 8.20.1 | The diagnostic limit was checked above, so the count stays at most the configured maximum. | Proposed |
| DEV-004.131 | `core/src/memory/resource_budget.cpp:358` | `--m_diagnostics;` | 8.20.1 | The check above returns an internal fault when no diagnostic is accounted, so the count is at least one. | Proposed |
| DEV-004.130 | `core/src/memory/work_budget.cpp:43` | `m_consumed += units;` | 8.20.1 | The check above returns when units exceeds the remaining budget, so m_consumed stays at most m_limit. | Proposed |
| DEV-004.129 | `core/src/memory/work_budget.cpp:70` | `return m_limit - m_consumed;` | 8.20.1 | charge() keeps m_consumed at most m_limit, so the difference cannot wrap. | Proposed |
| DEV-004.112 | `core/src/parser/parser_container_close.cpp:156` | `const std::size_t end_offset = closing.byte_offset + 1;` | 8.20.1 | closing.byte_offset is the offset of a byte inside the input, so adding one is at most the input size. | Proposed |
| DEV-004.111 | `core/src/parser/parser_container_close.cpp:161` | `const source_span span{frame->opening_offset, end_offset - frame->opening_offset};` | 8.20.1 | The container opened before its closing byte, so end_offset is above opening_offset. | Proposed |
| DEV-004.46 | `core/src/parser/parser_engine.cpp:286` | `const source_span span{m_source.offset(), scan.source.offset() - m_source.offset()};` | 8.20.1 | A cursor only moves forward, so the later offset is never below the earlier offset it started from, and the difference is the number of bytes consumed. | Proposed |
| DEV-004.45 | `core/src/parser/parser_engine.cpp:459` | `m_source.advance(scan.source.offset() - m_source.offset());` | 8.20.1 | A cursor only moves forward, so the later offset is never below the earlier offset it started from, and the difference is the number of bytes consumed. | Proposed |
| DEV-004.44 | `core/src/parser/parser_engine.cpp:853` | `++m_count_depth;` | 8.20.1 | The nesting depth was checked against max_depth before this frame was counted, so m_count_depth stays at most max_depth. | Proposed |
| DEV-004.43 | `core/src/parser/parser_engine.cpp:954` | `m_source.advance(scan.source.offset() - m_source.offset());` | 8.20.1 | A cursor only moves forward, so the later offset is never below the earlier offset it started from, and the difference is the number of bytes consumed. | Proposed |
| DEV-004.42 | `core/src/parser/parser_engine.cpp:1050` | `m_source.advance(scan.source.offset() - m_source.offset());` | 8.20.1 | A cursor only moves forward, so the later offset is never below the earlier offset it started from, and the difference is the number of bytes consumed. | Proposed |
| DEV-004.41 | `core/src/parser/parser_engine.cpp:1157` | `--m_count_depth;` | 8.20.1 | Closing a container requires an open one, so m_count_depth is at least one here. | Proposed |
| DEV-004.40 | `core/src/parser/parser_engine.cpp:1165` | `const std::size_t parent = m_count_depth - 1;` | 8.20.1 | This path runs only inside an open container, so m_count_depth is at least one. | Proposed |
| DEV-004.39 | `core/src/parser/parser_engine.cpp:1246` | `const std::size_t bytes = (m_options.limits.max_depth / c_bits_per_byte) + 1;` | 8.20.1 | Dividing by c_bits_per_byte leaves room below SIZE_MAX, so adding one cannot wrap. | Proposed |
| DEV-004.98 | `core/src/parser/parser_stack.cpp:38` | `const std::size_t offset = index * sizeof(parser_frame);` | 8.20.1 | index is below the slot count of the block, and the block size (count * sizeof) was computed with checked_multiply when it was allocated, so this product fits std::size_t. | Proposed |
| DEV-004.97 | `core/src/parser/parser_stack.cpp:191` | `++m_size;` | 8.20.1 | push() checked m_size against the reserved capacity, so the increment stays within it. | Proposed |
| DEV-004.96 | `core/src/parser/parser_stack.cpp:209` | `return (m_size == 0) ? nullptr : live_frame(m_size - 1);` | 8.20.1 | The condition handles m_size == 0 first, so m_size - 1 is the last frame's index. | Proposed |
| DEV-004.95 | `core/src/parser/parser_stack.cpp:224` | `return (m_size == 0) ? nullptr : live_frame(m_size - 1);` | 8.20.1 | The condition handles m_size == 0 first, so m_size - 1 is the last frame's index. | Proposed |
| DEV-004.94 | `core/src/parser/parser_stack.cpp:245` | `--m_size;` | 8.20.1 | The check above returns when the stack is empty, so m_size is at least one. | Proposed |
| DEV-004.142 | `core/src/parser/parser_work.cpp:45` | `cursor source{input_view{input.data, start_offset + visible}};` | 8.20.1 | visible is limited to the input bytes left after start_offset, so the sum is at most the input size. | Proposed |
| DEV-004.141 | `core/src/parser/parser_work.cpp:53` | `limited_scan{source, start_offset, start_offset + visible, visible < input_left});` | 8.20.1 | visible is limited to the input bytes left after start_offset, so the sum is at most the input size. | Proposed |
| DEV-004.140 | `core/src/parser/parser_work.cpp:162` | `source.advance(scan.source.offset() - source.offset());` | 8.20.1 | A cursor only moves forward, so the later offset is never below the earlier offset it started from, and the difference is the number of bytes consumed. | Proposed |
| DEV-004.100 | `core/src/parser/primitive_parser.cpp:97` | `reason = reason.relocated(reason.byte_offset() + number->byte_offset);` | 8.20.1 | The error offset lies inside the number's spelling, which lies inside the input, so the sum is an offset inside the input. | Proposed |
| DEV-004.99 | `core/src/parser/primitive_parser.cpp:220` | `value.byte_length = scan.source.offset() - source.offset();` | 8.20.1 | A cursor only moves forward, so the later offset is never below the earlier offset it started from, and the difference is the number of bytes consumed. | Proposed |
| DEV-004.143 | `core/src/unicode/utf16_decode.cpp:63` | `const auto second = source.unit_at(index + c_single_unit_count);` | 8.20.1 | index is a valid unit index below the unit count, so adding one is at most the unit count. | Proposed |
| DEV-004.38 | `core/src/value/big_unsigned.cpp:59` | `const std::size_t offset = index * sizeof(limb_type);` | 8.20.1 | index is below the slot count of the block, and the block size (count * sizeof) was computed with checked_multiply when it was allocated, so this product fits std::size_t. | Proposed |
| DEV-004.37 | `core/src/value/big_unsigned.cpp:96` | `const std::size_t offset = index * sizeof(limb_type);` | 8.20.1 | index is below the slot count of the block, and the block size (count * sizeof) was computed with checked_multiply when it was allocated, so this product fits std::size_t. | Proposed |
| DEV-004.36 | `core/src/value/big_unsigned.cpp:241` | `auto top = static_cast<wide_type>(read_limb(m_storage, m_size - c_additional_limb));` | 8.20.1 | Callers reach this only for a nonzero value, which has at least one limb, so m_size - 1 is the top limb index. | Proposed |
| DEV-004.35 | `core/src/value/big_unsigned.cpp:250` | `++top_bits;` | 8.20.1 | Counts the bits of one 16-bit limb, so top_bits is at most 16. | Proposed |
| DEV-004.34 | `core/src/value/big_unsigned.cpp:258` | `const auto lower_bits = checked_multiply(m_size - c_additional_limb, static_cast<std::s...` | 8.20.1 | Callers reach this only for a nonzero value, which has at least one limb, so m_size - 1 is the top limb index. | Proposed |
| DEV-004.33 | `core/src/value/big_unsigned.cpp:333` | `const auto expanded = (static_cast<wide_type>(read_limb(m_storage, index)) * radix) + c...` | 8.20.1 | The limb is below 2^16, radix and carry are at most 10000, so the result is below 2^32 and fits wide_type. | Proposed |
| DEV-004.32 | `core/src/value/big_unsigned.cpp:360` | `const auto expanded = (static_cast<wide_type>(read_limb(m_storage, index)) * radix) + c...` | 8.20.1 | The limb is below 2^16, radix and carry are at most 10000, so the result is below 2^32 and fits wide_type. | Proposed |
| DEV-004.31 | `core/src/value/big_unsigned.cpp:374` | `++m_size;` | 8.20.1 | The value's capacity was reserved for the extra limb before this call, so m_size stays within it. | Proposed |
| DEV-004.30 | `core/src/value/big_unsigned.cpp:529` | `write_limb(m_storage, index + whole_limbs, read_limb(m_storage, index));` | 8.20.1 | The capacity for m_size + whole_limbs limbs was reserved before shifting, so the destination index fits. | Proposed |
| DEV-004.29 | `core/src/value/big_unsigned.cpp:537` | `const std::uint32_t carry_bits = c_limb_bits - partial_bits;` | 8.20.1 | partial_bits is bit_count % c_limb_bits, so it is below c_limb_bits. | Proposed |
| DEV-004.28 | `core/src/value/big_unsigned.cpp:544` | `const auto top_limb = static_cast<wide_type>(read_limb(m_storage, m_size - c_additional...` | 8.20.1 | Callers reach this only for a nonzero value, which has at least one limb, so m_size - 1 is the top limb index. | Proposed |
| DEV-004.27 | `core/src/value/big_unsigned.cpp:550` | `write_limb(m_storage, m_size + whole_limbs, static_cast<limb_type>(top));` | 8.20.1 | The capacity for m_size + whole_limbs limbs was reserved before shifting, so the destination index fits. | Proposed |
| DEV-004.26 | `core/src/value/big_unsigned.cpp:558` | `++significant_count;` | 8.20.1 | Counts limbs within the capacity reserved for the shifted value, so it cannot wrap. | Proposed |
| DEV-004.25 | `core/src/value/big_unsigned.cpp:566` | `std::size_t index = m_size - c_additional_limb;` | 8.20.1 | Callers reach this only for a nonzero value, which has at least one limb, so m_size - 1 is the top limb index. | Proposed |
| DEV-004.24 | `core/src/value/big_unsigned.cpp:577` | `write_limb(m_storage, index + whole_limbs, static_cast<limb_type>((high \| low) % c_lim...` | 8.20.1 | The capacity for m_size + whole_limbs limbs was reserved before shifting, so the destination index fits. | Proposed |
| DEV-004.23 | `core/src/value/big_unsigned.cpp:684` | `difference = (left + c_limb_radix) - subtrahend;` | 8.20.1 | left is below the limb radix and subtrahend is at most the radix, so the borrowed difference lies in 0 to 2^17 in the 32-bit wide type. | Proposed |
| DEV-004.22 | `core/src/value/big_unsigned.cpp:703` | `const std::size_t highest_index = m_size - c_additional_limb;` | 8.20.1 | Callers reach this only for a nonzero value, which has at least one limb, so m_size - 1 is the top limb index. | Proposed |
| DEV-004.21 | `core/src/value/big_unsigned.cpp:715` | `--m_size;` | 8.20.1 | The loop runs only while m_size is nonzero, so m_size is at least one. | Proposed |
| DEV-004.17 | `core/src/value/floating_conversion.cpp:168` | `++bits;` | 8.20.1 | Each iteration shifts a 64-bit value right by one bit, so the loop runs at most 64 times and bits never exceeds 64. | Proposed |
| DEV-004.16 | `core/src/value/floating_conversion.cpp:218` | `++position;` | 8.20.1 | The loop condition position < spelling.size holds here, so position + 1 is at most spelling.size and cannot wrap std::size_t. | Proposed |
| DEV-004.15 | `core/src/value/floating_conversion.cpp:234` | `++position;` | 8.20.1 | The loop condition position < spelling.size holds here, so position + 1 is at most spelling.size and cannot wrap std::size_t. | Proposed |
| DEV-004.14 | `core/src/value/floating_conversion.cpp:261` | `chunk = (chunk * c_decimal_radix) + digit;` | 8.20.1 | At most three digits are held before this step, so chunk is at most 999, and number analysis guarantees digit is 0 to 9. The result is at most 9999 and cannot wrap std::uint32_t. | Proposed |
| DEV-004.13 | `core/src/value/floating_conversion.cpp:266` | `chunk_radix *= c_decimal_radix;` | 8.20.1 | chunk_radix is reset to 1 when it reaches c_decimal_chunk_radix (10000), so it is at most 1000 before this multiply. | Proposed |
| DEV-004.12 | `core/src/value/floating_conversion.cpp:358` | `constexpr std::uint32_t c_shift = 64U - static_cast<std::uint32_t>(c_explicit_bits) - 3U;` | 8.20.1 | Compile-time constant: c_explicit_bits is 52 for double and 23 for float, giving 9 or 38. The static_assert below rejects any type for which the subtraction would wrap. | Proposed |
| DEV-004.18 | `core/src/value/floating_conversion.cpp:364` | `constexpr std::uint32_t c_shift_upper = c_shift + 1U;` | 8.20.1 | Compile-time constant: c_shift is 9 for double and 38 for float, so adding one gives 10 or 39. The static_assert below rejects any type for which the addition would wrap or the shift would leave 64 bits. | Proposed |
| DEV-004.19 | `core/src/value/floating_conversion.cpp:370` | `constexpr std::uint64_t c_precision_mask = c_all_ones >> (c_explicit_shift + 3U);` | 8.20.1 | Compile-time constant: c_explicit_shift is 52 or 23, so the shift is 55 or 26. | Proposed |
| DEV-004.11 | `core/src/value/floating_conversion.cpp:386` | `const auto leading_zeros = static_cast<std::int32_t>(64U - bit_length_of(significand));` | 8.20.1 | significand is nonzero (checked above), so bit_length_of() returns 1 to 64 and the subtraction yields 0 to 63 without wrapping. | Proposed |
| DEV-004.10 | `core/src/value/floating_conversion.cpp:407` | `product.low += second.high;` | 8.20.1 | This is the low word of a 128-bit addition and is meant to wrap modulo 2^64. The next statement detects the wrap (second.high > product.low) and carries it into product.high, so no value is lost. | Proposed |
| DEV-004.09 | `core/src/value/floating_conversion.cpp:414` | `++product.high;` | 8.20.1 | product.high is the high word of a 64x64-bit product, so it is at most 2^64 - 2. Adding the carry cannot wrap std::uint64_t. | Proposed |
| DEV-004.08 | `core/src/value/floating_conversion.cpp:462` | `mantissa += mantissa & 1U;` | 8.20.1 | mantissa is product.high shifted right by at least c_shift (9 for double, 38 for float) bits, so it is below 2^55. Adding its lowest bit cannot wrap std::uint64_t. | Proposed |
| DEV-004.07 | `core/src/value/floating_conversion.cpp:543` | `for (position += c_hexadecimal_prefix_bytes; position < spelling.size; ++position)` | 8.20.1 | number analysis has already accepted the optional sign and the 0x prefix, so position + c_hexadecimal_prefix_bytes is at most spelling.size. The increment runs only while position < spelling.size. Neither can wrap std::size_t. | Proposed |
| DEV-004.06 | `core/src/value/floating_conversion.cpp:551` | `significand = (significand * c_hexadecimal_radix) + hexadecimal_digit_value(spelling.da...` | 8.20.1 | try_fast_path() returns before this loop when parts.digit_count exceeds 16, and number analysis only accepts hexadecimal digits here. The accumulated value is at most 16^16 - 1 = 2^64 - 1, so neither the multiply nor the add can wrap. | Proposed |
| DEV-004.04 | `core/src/value/floating_conversion.cpp:579` | `significand = (significand * c_decimal_radix) + static_cast<std::uint32_t>(current - '0');` | 7.0.3, 8.20.1 | try_fast_path() returns before this loop when parts.digit_count exceeds 19, and number analysis only accepts '0'-'9' here besides '.' and the exponent marker. The accumulated value is at most 10^19 - 1, which is below 2^64 - 1, so neither the multiply nor the add can wrap. Converts an already validated digit or hexadecimal letter to its value, or a value 0-9 back to its digit. The C++ standard guarantees '0'-'9' are contiguous. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-004.05 | `core/src/value/floating_conversion.cpp:586` | `++position;` | 8.20.1 | The loop condition position < spelling.size holds here, so position + 1 is at most spelling.size and cannot wrap std::size_t. | Proposed |
| DEV-004.03 | `core/src/value/floating_conversion.cpp:1098` | `significand += 1U;` | 8.20.1 | significand is quotient >> drop, and the quotient holds at most c_quotient_bits + 1 (56) bits, so significand is below 2^56. Adding one cannot wrap a 64-bit unsigned value. A carry into a new bit is handled below as an exponent adjustment. | Proposed |
| DEV-004.60 | `core/src/value/integer_magnitude.cpp:57` | `return magnitude_result::success((current * radix) + digit);` | 8.20.1 | The check above returns when current * radix + digit would exceed maximum, so the expression stays within maximum. | Proposed |
| DEV-004.59 | `core/src/value/integer_magnitude.cpp:141` | `++position;` | 8.20.1 | position indexes a byte of the spelling or input that was just read, so it is below the size and the increment reaches at most the size; it cannot wrap std::size_t. | Proposed |
| DEV-004.58 | `core/src/value/integer_magnitude.cpp:230` | `++position;` | 8.20.1 | position indexes a byte of the spelling or input that was just read, so it is below the size and the increment reaches at most the size; it cannot wrap std::size_t. | Proposed |
| DEV-004.57 | `core/src/value/integer_magnitude.cpp:244` | `++significand_digits;` | 8.20.1 | Counts characters or sequences of the spelling or input being scanned, so the count is at most its length and cannot wrap std::size_t. | Proposed |
| DEV-004.56 | `core/src/value/integer_magnitude.cpp:252` | `++fractional_digits;` | 8.20.1 | Counts characters or sequences of the spelling or input being scanned, so the count is at most its length and cannot wrap std::size_t. | Proposed |
| DEV-004.55 | `core/src/value/integer_magnitude.cpp:259` | `++position;` | 8.20.1 | position indexes a byte of the spelling or input that was just read, so it is below the size and the increment reaches at most the size; it cannot wrap std::size_t. | Proposed |
| DEV-004.54 | `core/src/value/integer_magnitude.cpp:277` | `++position;` | 8.20.1 | position indexes a byte of the spelling or input that was just read, so it is below the size and the increment reaches at most the size; it cannot wrap std::size_t. | Proposed |
| DEV-004.53 | `core/src/value/integer_magnitude.cpp:295` | `++position;` | 8.20.1 | position indexes a byte of the spelling or input that was just read, so it is below the size and the increment reaches at most the size; it cannot wrap std::size_t. | Proposed |
| DEV-004.52 | `core/src/value/integer_magnitude.cpp:320` | `const std::size_t integer_digits = significand_digits - fractional_digits;` | 8.20.1 | Fractional digits are a subset of the significand digits, so the difference cannot wrap. | Proposed |
| DEV-004.51 | `core/src/value/integer_magnitude.cpp:331` | `const std::size_t digits_to_remove = fractional_digits + static_cast<std::size_t>(expon...` | 8.20.1 | The check above ensures exponent_magnitude is at most the integer digits, so the sum is at most significand_digits. | Proposed |
| DEV-004.50 | `core/src/value/integer_magnitude.cpp:337` | `digits_to_keep -= digits_to_remove;` | 8.20.1 | digits_to_remove is at most significand_digits, which is the initial value of digits_to_keep. | Proposed |
| DEV-004.49 | `core/src/value/integer_magnitude.cpp:352` | `digits_to_keep -= digits_to_remove;` | 8.20.1 | digits_to_remove is at most significand_digits, which is the initial value of digits_to_keep. | Proposed |
| DEV-004.48 | `core/src/value/integer_magnitude.cpp:365` | `++position;` | 8.20.1 | position indexes a byte of the spelling or input that was just read, so it is below the size and the increment reaches at most the size; it cannot wrap std::size_t. | Proposed |
| DEV-004.47 | `core/src/value/integer_magnitude.cpp:392` | `++retained_digits;` | 8.20.1 | Counts characters or sequences of the spelling or input being scanned, so the count is at most its length and cannot wrap std::size_t. | Proposed |
| DEV-004.110 | `core/src/value/number_analysis.cpp:90` | `++analysis.digit_count;` | 8.20.1 | Counts characters or sequences of the spelling or input being scanned, so the count is at most its length and cannot wrap std::size_t. | Proposed |
| DEV-004.109 | `core/src/value/number_analysis.cpp:95` | `++position;` | 8.20.1 | position indexes a byte of the spelling or input that was just read, so it is below the size and the increment reaches at most the size; it cannot wrap std::size_t. | Proposed |
| DEV-004.108 | `core/src/value/number_analysis.cpp:114` | `++position;` | 8.20.1 | position indexes a byte of the spelling or input that was just read, so it is below the size and the increment reaches at most the size; it cannot wrap std::size_t. | Proposed |
| DEV-004.107 | `core/src/value/number_analysis.cpp:132` | `++analysis.digit_count;` | 8.20.1 | Counts characters or sequences of the spelling or input being scanned, so the count is at most its length and cannot wrap std::size_t. | Proposed |
| DEV-004.106 | `core/src/value/number_analysis.cpp:140` | `++fractional_digits;` | 8.20.1 | Counts characters or sequences of the spelling or input being scanned, so the count is at most its length and cannot wrap std::size_t. | Proposed |
| DEV-004.105 | `core/src/value/number_analysis.cpp:149` | `++trailing_zeros;` | 8.20.1 | Counts characters or sequences of the spelling or input being scanned, so the count is at most its length and cannot wrap std::size_t. | Proposed |
| DEV-004.104 | `core/src/value/number_analysis.cpp:191` | `++position;` | 8.20.1 | position indexes a byte of the spelling or input that was just read, so it is below the size and the increment reaches at most the size; it cannot wrap std::size_t. | Proposed |
| DEV-004.103 | `core/src/value/number_analysis.cpp:207` | `++position;` | 8.20.1 | position indexes a byte of the spelling or input that was just read, so it is below the size and the increment reaches at most the size; it cannot wrap std::size_t. | Proposed |
| DEV-004.102 | `core/src/value/number_analysis.cpp:218` | `: ((scale_exponent * 10) + digit);` | 8.20.1 | The condition selects the saturated limit when the next value would exceed it, so this branch stays within the limit. | Proposed |
| DEV-004.101 | `core/src/value/number_analysis.cpp:235` | `exponent_magnitude = (exponent_magnitude * 10) + digit;` | 8.20.1 | The check above stops accumulating once the value would exceed threshold, so this stays within threshold. | Proposed |
| DEV-004.90 | `core/src/value/number_comparison.cpp:179` | `power *= c_decimal_radix;` | 8.20.1 | digits is at most c_chunk_digits (4), so power is at most 10^4. | Proposed |
| DEV-004.89 | `core/src/value/number_comparison.cpp:212` | `count -= step;` | 8.20.1 | step is the smaller of count and c_chunk_digits, so it is at most count. | Proposed |
| DEV-004.88 | `core/src/value/number_comparison.cpp:301` | `count += c_chunk_digits;` | 8.20.1 | count is the number of decimal digits found so far, which is bounded by the bit length of the value, so it cannot wrap. | Proposed |
| DEV-004.87 | `core/src/value/number_comparison.cpp:330` | `++count;` | 8.20.1 | count is the number of decimal digits found so far, which is bounded by the bit length of the value, so it cannot wrap. | Proposed |
| DEV-004.86 | `core/src/value/number_comparison.cpp:386` | `++position;` | 8.20.1 | position indexes a byte of the spelling or input that was just read, so it is below the size and the increment reaches at most the size; it cannot wrap std::size_t. | Proposed |
| DEV-004.85 | `core/src/value/number_comparison.cpp:421` | `++position;` | 8.20.1 | position indexes a byte of the spelling or input that was just read, so it is below the size and the increment reaches at most the size; it cannot wrap std::size_t. | Proposed |
| DEV-004.84 | `core/src/value/number_comparison.cpp:445` | `chunk = (chunk * c_decimal_radix) + digit;` | 8.20.1 | chunk holds at most three digits (999) before this step, so the result is at most 9999. | Proposed |
| DEV-004.83 | `core/src/value/number_comparison.cpp:449` | `++chunk_digits;` | 8.20.1 | chunk_digits is reset after c_chunk_digits digits, so it is at most 4. | Proposed |
| DEV-004.82 | `core/src/value/number_comparison.cpp:470` | `++fractional_digits;` | 8.20.1 | Counts characters or sequences of the spelling or input being scanned, so the count is at most its length and cannot wrap std::size_t. | Proposed |
| DEV-004.81 | `core/src/value/number_comparison.cpp:485` | `++significant_digits;` | 8.20.1 | Counts characters or sequences of the spelling or input being scanned, so the count is at most its length and cannot wrap std::size_t. | Proposed |
| DEV-004.80 | `core/src/value/number_comparison.cpp:512` | `++position;` | 8.20.1 | position indexes a byte of the spelling or input that was just read, so it is below the size and the increment reaches at most the size; it cannot wrap std::size_t. | Proposed |
| DEV-004.79 | `core/src/value/number_comparison.cpp:535` | `exponent = (exponent * c_decimal_radix) + digit;` | 8.20.1 | The check above returns a limit failure before exponent could exceed c_max_exponent. | Proposed |
| DEV-004.78 | `core/src/value/number_comparison.cpp:540` | `++position;` | 8.20.1 | position indexes a byte of the spelling or input that was just read, so it is below the size and the increment reaches at most the size; it cannot wrap std::size_t. | Proposed |
| DEV-004.147 | `core/src/value/utf8_export.cpp:120` | `index += decoded.value_if()->unit_count;` | 8.20.1 | unit_count comes from decoding a sequence that lies inside the remaining units, so index stays at most the unit count. | Proposed |
| DEV-004.146 | `core/src/value/utf8_export.cpp:222` | `index += decoded.value_if()->unit_count;` | 8.20.1 | unit_count comes from decoding a sequence that lies inside the remaining units, so index stays at most the unit count. | Proposed |
| DEV-004.148 | `core/src/value/utf8_name_equal.cpp:83` | `++position;` | 8.20.1 | position indexes a byte of the spelling or input that was just read, so it is below the size and the increment reaches at most the size; it cannot wrap std::size_t. | Proposed |
| DEV-004.61 | `core/src/value/value_iterator.cpp:146` | `--m_remaining;` | 8.20.1 | A child was just returned, which requires m_remaining to be at least one. | Proposed |
| DEV-004.93 | `core/src/value/value_storage.cpp:50` | `const std::size_t offset = index * sizeof(value_record);` | 8.20.1 | index is below the slot count of the block, and the block size (count * sizeof) was computed with checked_multiply when it was allocated, so this product fits std::size_t. | Proposed |
| DEV-004.92 | `core/src/value/value_storage.cpp:100` | `--m_size;` | 8.20.1 | The loop runs only while m_size is nonzero, so m_size is at least one. | Proposed |
| DEV-004.91 | `core/src/value/value_storage.cpp:340` | `const std::size_t required = m_size + 1;` | 8.20.1 | m_size is at most max_records, which the limit check keeps below SIZE_MAX, so adding one cannot wrap. | Proposed |
| DEV-004.64 | `core/src/value/warning_list.cpp:50` | `const std::size_t offset = index * sizeof(string_warning);` | 8.20.1 | index is below the slot count of the block, and the block size (count * sizeof) was computed with checked_multiply when it was allocated, so this product fits std::size_t. | Proposed |
| DEV-004.63 | `core/src/value/warning_list.cpp:86` | `const std::size_t offset = index * sizeof(string_warning);` | 8.20.1 | index is below the slot count of the block, and the block size (count * sizeof) was computed with checked_multiply when it was allocated, so this product fits std::size_t. | Proposed |
| DEV-004.62 | `core/src/value/warning_list.cpp:240` | `++m_size;` | 8.20.1 | append_reserved() checked m_size against the reserved capacity, so the increment stays within it. | Proposed |
| DEV-004.118 | `core/includes/common/checked_size.hpp:68` | `return result<std::size_t>::success(left * right);` | 8.20.1 | The overflow check above returns before this line when left * right would exceed SIZE_MAX. | Proposed |
| DEV-004.67 | `core/includes/input/cursor.hpp:112` | `return m_view.data[m_offset + delta];` | 8.20.1 | The check above returns when delta is not below remaining(), so m_offset + delta stays below the view size. | Proposed |
| DEV-004.66 | `core/includes/input/cursor.hpp:132` | `return m_view.data[m_offset++];` | 8.20.1 | The check above returns at the end of the view, so m_offset is below the view size before the increment. | Proposed |
| DEV-004.65 | `core/includes/input/cursor.hpp:147` | `m_offset += (count < rem) ? count : rem;` | 8.20.1 | The step is limited to remaining(), so m_offset reaches at most the view size. | Proposed |
| DEV-004.01 | `core/includes/unicode/unicode.hpp:63` | `const std::size_t middle = begin + ((end - begin) / 2);` | 8.20.1 | The loop requires begin < end, so end - begin fits. Adding half that distance to begin gives a midpoint strictly below end. | Proposed |
| DEV-004.02 | `core/includes/unicode/unicode.hpp:79` | `begin = middle + 1;` | 8.20.1 | middle < end, so advancing by one reaches at most end. | Proposed |
| DEV-004.145 | `core/includes/value/wide_multiply.hpp:49` | `const std::uint64_t middle = (low_low >> c_half_bits) + (high_low & c_low_32_mask) + lo...` | 8.20.1 | Each term is below 2^32 or a 32x32-bit product, so the sum is at most (2^32 - 1)^2 + 2 * (2^32 - 1) = 2^64 - 1 and fits std::uint64_t. | Proposed |
| DEV-004.144 | `core/includes/value/wide_multiply.hpp:55` | `return product_128{high_high + (high_low >> c_half_bits) + (middle >> c_half_bits),` | 8.20.1 | This is the exact high word of a 64x64-bit product, which is at most 2^64 - 2, so the column sum fits std::uint64_t. | Proposed |
| DEV-004.149 | `core/src/value/integer_magnitude.cpp:273` | `position += c_hexadecimal_prefix_bytes;` | The spelling was validated as hexadecimal, so it starts with the two-byte prefix and position + 2 is at most its length; the sum cannot wrap std::size_t. | Proposed |
| DEV-004.150 | `core/src/value/number_analysis.cpp:239` | `position += 2;` | The spelling was validated as hexadecimal, so it starts with the two-byte prefix and position + 2 is at most its length; the sum cannot wrap std::size_t. | Proposed |

---

## DEV-005: Rule 4.1.3, signed exponent arithmetic

| Field | Value |
| --- | --- |
| Rule | MISRA C++:2023 Rule 4.1.3 |
| CodeQL query | `cpp/misra/signed-integer-overflow` |
| Lines | 19 |
| Status | Proposed (#166) |

### Why this rule is deviated instead of fixed

Exponent, shift and scale arithmetic in the floating conversion and number comparison uses `std::ptrdiff_t`/`std::int32_t` values that are bounded by earlier limit checks (`c_max_bits`, `c_max_exponent`, the power-of-five table range).

The bounds are established once at the entry checks; repeating checks at every step would add unreachable branches.

### Risk and compensating measures

The bound for `number_comparison.cpp` scale and leading exponent holds for spellings shorter than 3/4 of `PTRDIFF_MAX`, which `max_number_bytes` normally ensures; the owner accepted this residual risk (#166). UBSan and fuzzing run on every pull request.

### Lines

| ID | Location | Code | Rules | Justification | Status |
| --- | --- | --- | --- | --- | --- |
| DEV-005.15 | `core/src/value/floating_conversion.cpp:340` | `constexpr std::int32_t c_infinite_power = (2 * limits::max_exponent) - 1; // 2047 or 255` | 4.1.3 | Compile-time constant: limits::max_exponent is 1024 or 128, giving 2047 or 255. An overflowing constant expression would not compile. | Proposed |
| DEV-005.14 | `core/src/value/floating_conversion.cpp:393` | `detail::c_power_of_five_table[static_cast<std::size_t>(power - detail::c_smallest_power...` | 4.1.3 | power is between c_smallest_power_of_five and c_largest_power_of_five (checked above), so the difference is a table index from 0 to the table size minus one. | Proposed |
| DEV-005.13 | `core/src/value/floating_conversion.cpp:436` | `(scaled >= 0) ? (scaled / c_log2_ten_scale) : -((-scaled + c_log2_ten_scale - 1) / c_lo...` | 4.1.3 | power lies in the power-of-five table range (checked above), so \|scaled\| = \|217706 * power\| is below 10^8; negating it, adding the scale and dividing by 65536 cannot overflow std::int64_t. | Proposed |
| DEV-005.12 | `core/src/value/floating_conversion.cpp:442` | `static_cast<std::int32_t>(floor_log2) + 63 + (upper_bit ? 1 : 0) - leading_zeros - c_mi...` | 4.1.3 | power lies in the table range, so \|floor_log2\| is below 1200; leading_zeros is 0 to 63 and c_minimum_exponent is -1023 or -127, so the sum stays within a few thousand. | Proposed |
| DEV-005.11 | `core/src/value/floating_conversion.cpp:473` | `++power2;` | 4.1.3 | power2 is below c_infinite_power plus a few units here (it comes from the bounded table exponent), so the increment cannot overflow std::int32_t. | Proposed |
| DEV-005.10 | `core/src/value/floating_conversion.cpp:488` | `std::ldexp(static_cast<T>(mantissa), static_cast<int>(power2 + c_minimum_exponent - c_e...` | 4.1.3 | power2 is between 1 and c_infinite_power - 1 here (checked above), so adding c_minimum_exponent and subtracting c_explicit_bits stays within a few thousand. | Proposed |
| DEV-005.09 | `core/src/value/floating_conversion.cpp:630` | `const auto converted = try_eisel_lemire<T>(significand, parts.scale_negative ? -scale :...` | 4.1.3 | The branch condition limits scale_magnitude to c_largest_table_scale, so scale is small and positive and negating it cannot overflow std::int32_t. | Proposed |
| DEV-005.08 | `core/src/value/floating_conversion.cpp:673` | `constexpr std::ptrdiff_t c_quotient_bits = c_precision + 2;` | 0.1.1, 4.1.3 | Compile-time constant: c_precision is 53 for double and 24 for float, so the result is 55 or 26. The static_assert below rejects any other value that would leave the 64-bit quotient. Initialized at the declaration so that no path can read an indeterminate value; every path assigns the real value before use. For constants and results, the value is read later in the same function or template instantiation, which the tool does not track. | Proposed |
| DEV-005.07 | `core/src/value/floating_conversion.cpp:952` | `c_quotient_bits - (static_cast<std::ptrdiff_t>(numerator_bits) - static_cast<std::ptrdi...` | 4.1.3 | Both bit counts are at most c_max_bits (PTRDIFF_MAX / 4), checked above, so their difference is within +/- PTRDIFF_MAX / 4 and subtracting it from c_quotient_bits cannot overflow std::ptrdiff_t. | Proposed |
| DEV-005.06 | `core/src/value/floating_conversion.cpp:1048` | `const std::ptrdiff_t leading_exponent = (quotient_bits - 1) - shift;` | 4.1.3 | quotient_bits is 0 to 64 and \|shift\| is at most PTRDIFF_MAX / 4 + 55 because the bit counts are limited by c_max_bits, so neither subtraction can overflow std::ptrdiff_t. | Proposed |
| DEV-005.05 | `core/src/value/floating_conversion.cpp:1060` | `kept_bits -= (c_min_normal_exponent - leading_exponent);` | 4.1.3 | In this branch leading_exponent < c_min_normal_exponent, so the difference is positive and at most PTRDIFF_MAX / 4 + 1200 (\|leading_exponent\| is bounded through shift). Subtracting it from c_precision cannot overflow. | Proposed |
| DEV-005.04 | `core/src/value/floating_conversion.cpp:1068` | `std::ptrdiff_t dropped_bits = quotient_bits - kept_bits;` | 4.1.3 | quotient_bits is 0 to 64 and kept_bits is at most c_precision, and at least c_precision - (PTRDIFF_MAX / 4 + 1200) after the adjustment above. The difference cannot overflow std::ptrdiff_t. | Proposed |
| DEV-005.03 | `core/src/value/floating_conversion.cpp:1075` | `const std::ptrdiff_t max_dropped_bits = quotient_bits + 1;` | 4.1.3 | quotient_bits is the bit length of a 64-bit value, 0 to 64, so adding one cannot overflow std::ptrdiff_t. | Proposed |
| DEV-005.02 | `core/src/value/floating_conversion.cpp:1111` | `const std::ptrdiff_t scale_exponent = dropped_bits - shift;` | 4.1.3 | dropped_bits is between -53 and quotient_bits + 1 (at most 65), and \|shift\| is at most PTRDIFF_MAX / 4 + 55 because the bit counts are limited by c_max_bits. The difference cannot overflow std::ptrdiff_t. | Proposed |
| DEV-005.01 | `core/src/value/floating_conversion.cpp:1120` | `(static_cast<std::ptrdiff_t>(bit_length_of(significand)) - 1) + scale_exponent;` | 4.1.3 | significand is nonzero (checked above), so the bit length minus one is 0 to 63. scale_exponent is at most PTRDIFF_MAX / 4 + 120 in magnitude (see the bounds at shift), so the sum cannot overflow std::ptrdiff_t. | Proposed |
| DEV-005.19 | `core/src/value/number_comparison.cpp:401` | `form.leading_exponent = static_cast<std::ptrdiff_t>(*count.value_if()) - 1;` | 4.1.3 | count is the number of decimal digits of a nonzero value, at least one and far below PTRDIFF_MAX, so subtracting one cannot overflow. | Proposed |
| DEV-005.18 | `core/src/value/number_comparison.cpp:552` | `(exponent_negative ? -signed_exponent : signed_exponent) - static_cast<std::ptrdiff_t>(...` | 4.1.3 | \|exponent\| is at most c_max_exponent (PTRDIFF_MAX / 4, checked while parsing) and fractional_digits is at most the spelling length, limited by max_number_bytes. The result stays within std::ptrdiff_t while the spelling is shorter than 3/4 of PTRDIFF_MAX. | Proposed |
| DEV-005.17 | `core/src/value/number_comparison.cpp:557` | `form.leading_exponent = form.scale + static_cast<std::ptrdiff_t>(significant_digits) - 1;` | 4.1.3 | form.scale is bounded as above and significant_digits is at most the spelling length, so the sum stays within std::ptrdiff_t for spellings shorter than 3/4 of PTRDIFF_MAX. | Proposed |
| DEV-005.16 | `core/src/value/number_comparison.cpp:682` | `left_larger_scale ? (left_parts.scale - right_parts.scale) : (right_parts.scale - left_...` | 4.1.3 | Equal leading exponents bound the scale difference by the two digit counts, which are at most the spelling lengths, so the difference is far below PTRDIFF_MAX. | Proposed |

---

## DEV-006: Rule 0.0.2, conditions that are constant per instantiation

| Field | Value |
| --- | --- |
| Rule | MISRA C++:2023 Rule 0.0.2 |
| CodeQL query | `cpp/misra/invariant-condition` |
| Lines | 12 |
| Status | Proposed (#166) |

### Why this rule is deviated instead of fixed

Template conditions on `std::numeric_limits<T>` properties and generic integer range checks are constant for one instantiation but differ between the supported types.

Removing them would break the other instantiations.

### Risk and compensating measures

Compile-time `static_assert`s and per-type unit tests.

### Lines

| ID | Location | Code | Justification | Status |
| --- | --- | --- | --- | --- |
| DEV-006.12 | `core/src/value/floating_conversion.cpp:345` | `constexpr std::int32_t c_round_even_min = (limits::digits == 53) ? -4 : -17;` | Compile-time property of the template parameter or target type. The condition is constant for one instantiation but differs between the supported types, so it is needed. | Proposed |
| DEV-006.11 | `core/src/value/floating_conversion.cpp:350` | `constexpr std::int32_t c_round_even_max = (limits::digits == 53) ? 23 : 10;` | Compile-time property of the template parameter or target type. The condition is constant for one instantiation but differs between the supported types, so it is needed. | Proposed |
| DEV-006.10 | `core/includes/value/floating_format.hpp:75` | `return std::numeric_limits<T>::is_iec559 && (std::numeric_limits<T>::radix == c_binary_...` | Compile-time property of the template parameter or target type. The condition is constant for one instantiation but differs between the supported types, so it is needed. | Proposed |
| DEV-006.09 | `core/includes/value/floating_format.hpp:81` | `(std::numeric_limits<T>::digits == c_float_precision) &&` | Compile-time property of the template parameter or target type. The condition is constant for one instantiation but differs between the supported types, so it is needed. | Proposed |
| DEV-006.08 | `core/includes/value/floating_format.hpp:87` | `(std::numeric_limits<T>::min_exponent == c_float_min_exponent) &&` | Compile-time property of the template parameter or target type. The condition is constant for one instantiation but differs between the supported types, so it is needed. | Proposed |
| DEV-006.07 | `core/includes/value/floating_format.hpp:93` | `(std::numeric_limits<T>::max_exponent == c_float_max_exponent) &&` | Compile-time property of the template parameter or target type. The condition is constant for one instantiation but differs between the supported types, so it is needed. | Proposed |
| DEV-006.06 | `core/includes/value/floating_format.hpp:107` | `return std::numeric_limits<T>::is_iec559 && (std::numeric_limits<T>::radix == c_binary_...` | Compile-time property of the template parameter or target type. The condition is constant for one instantiation but differs between the supported types, so it is needed. | Proposed |
| DEV-006.05 | `core/includes/value/floating_format.hpp:113` | `(std::numeric_limits<T>::digits == c_double_precision) &&` | Compile-time property of the template parameter or target type. The condition is constant for one instantiation but differs between the supported types, so it is needed. | Proposed |
| DEV-006.04 | `core/includes/value/floating_format.hpp:119` | `(std::numeric_limits<T>::min_exponent == c_double_min_exponent) &&` | Compile-time property of the template parameter or target type. The condition is constant for one instantiation but differs between the supported types, so it is needed. | Proposed |
| DEV-006.03 | `core/includes/value/floating_format.hpp:125` | `(std::numeric_limits<T>::max_exponent == c_double_max_exponent) &&` | Compile-time property of the template parameter or target type. The condition is constant for one instantiation but differs between the supported types, so it is needed. | Proposed |
| DEV-006.02 | `includes/adaptive_json5/value/integer_conversion.hpp:60` | `if ((value < c_minimum) \|\| (value > c_maximum))` | Compile-time property of the template parameter or target type. The condition is constant for one instantiation but differs between the supported types, so it is needed. | Proposed |
| DEV-006.01 | `includes/adaptive_json5/value/integer_conversion.hpp:84` | `if (value > c_maximum)` | Compile-time property of the template parameter or target type. The condition is constant for one instantiation but differs between the supported types, so it is needed. | Proposed |

---

## DEV-007: Rule 0.1.1, defensive initialization and template uses

| Field | Value |
| --- | --- |
| Rule | MISRA C++:2023 Rule 0.1.1 |
| CodeQL query | `cpp/misra/unnecessary-write-to-local-object` |
| Lines | 26 |
| Status | Proposed (#166) |

### Why this rule is deviated instead of fixed

Variables are initialized at their declaration so that no path reads an indeterminate value; constants and results are read later in templates, which the query does not track.

Removing the initialization would conflict with the project's rule against uninitialized variables (#166 commit a89fe96).

### Risk and compensating measures

Compiler warnings and unit tests.

### Lines

| ID | Location | Code | Rules | Justification | Status |
| --- | --- | --- | --- | --- | --- |
| DEV-007.23 | `core/src/lexer/punctuation.cpp:38` | `punctuation_kind kind{};` | 0.1.1 | Initialized at the declaration so that no path can read an indeterminate value; every path assigns the real value before use. For constants and results, the value is read later in the same function or template instantiation, which the tool does not track. | Proposed |
| DEV-007.17 | `core/src/lexer/string/string_boundary.cpp:41` | `string_quote quote{};` | 0.1.1 | Initialized at the declaration so that no path can read an indeterminate value; every path assigns the real value before use. For constants and results, the value is read later in the same function or template instantiation, which the tool does not track. | Proposed |
| DEV-007.18 | `core/src/lexer/string/string_warning.cpp:165` | `string_warning_code code{};` | 0.1.1 | Initialized at the declaration so that no path can read an indeterminate value; every path assigns the real value before use. For constants and results, the value is read later in the same function or template instantiation, which the tool does not track. | Proposed |
| DEV-007.21 | `core/src/parser/parser_engine.cpp:493` | `parser_state next_state = frame->state;` | 0.1.1 | Initialized at the declaration so that no path can read an indeterminate value; every path assigns the real value before use. For constants and results, the value is read later in the same function or template instantiation, which the tool does not track. | Proposed |
| DEV-007.20 | `core/src/parser/parser_engine.cpp:1078` | `parser_state next_state = m_count_top.state;` | 0.1.1 | Initialized at the declaration so that no path can read an indeterminate value; every path assigns the real value before use. For constants and results, the value is read later in the same function or template instantiation, which the tool does not track. | Proposed |
| DEV-007.19 | `core/src/value/big_unsigned.cpp:675` | `wide_type difference{};` | 0.1.1 | Initialized at the declaration so that no path can read an indeterminate value; every path assigns the real value before use. For constants and results, the value is read later in the same function or template instantiation, which the tool does not track. | Proposed |
| DEV-007.24 | `core/src/value/document_links.cpp:128` | `container_links* links = nullptr;` | 0.1.1 | Initialized at the declaration so that no path can read an indeterminate value; every path assigns the real value before use. For constants and results, the value is read later in the same function or template instantiation, which the tool does not track. | Proposed |
| DEV-007.16 | `core/src/value/floating_conversion.cpp:679` | `constexpr std::ptrdiff_t c_min_normal_exponent = limits::min_exponent - 1;` | 0.1.1 | Initialized at the declaration so that no path can read an indeterminate value; every path assigns the real value before use. For constants and results, the value is read later in the same function or template instantiation, which the tool does not track. | Proposed |
| DEV-007.15 | `core/src/value/floating_conversion.cpp:690` | `constexpr auto c_max_bits = static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t...` | 0.1.1, 4.1.3 | Initialized at the declaration so that no path can read an indeterminate value; every path assigns the real value before use. For constants and results, the value is read later in the same function or template instantiation, which the tool does not track. Compile-time constant: the maximum of std::ptrdiff_t is converted to std::size_t and divided by four, which cannot overflow. | Proposed |
| DEV-007.14 | `core/src/value/floating_conversion.cpp:705` | `const auto special = detail::convert_special_floating<T>(*analysis.value_if(), options);` | 0.1.1 | Initialized at the declaration so that no path can read an indeterminate value; every path assigns the real value before use. For constants and results, the value is read later in the same function or template instantiation, which the tool does not track. | Proposed |
| DEV-007.13 | `core/src/value/floating_conversion.cpp:748` | `const auto fast = try_fast_path<T>(number.spelling(), parts, work);` | 0.1.1 | Initialized at the declaration so that no path can read an indeterminate value; every path assigns the real value before use. For constants and results, the value is read later in the same function or template instantiation, which the tool does not track. | Proposed |
| DEV-007.12 | `core/src/value/floating_conversion.cpp:825` | `checked_add(larger_bound, static_cast<std::size_t>(c_quotient_bits), resource_limit_rea...` | 0.1.1 | Initialized at the declaration so that no path can read an indeterminate value; every path assigns the real value before use. For constants and results, the value is read later in the same function or template instantiation, which the tool does not track. | Proposed |
| DEV-007.11 | `core/src/value/floating_conversion.cpp:839` | `checked_add(*aligned_bound.value_if(), static_cast<std::size_t>(1U), resource_limit_rea...` | 0.1.1 | Initialized at the declaration so that no path can read an indeterminate value; every path assigns the real value before use. For constants and results, the value is read later in the same function or template instantiation, which the tool does not track. | Proposed |
| DEV-007.10 | `core/src/value/floating_conversion.cpp:961` | `const auto widened = numerator.shift_left(static_cast<std::size_t>(shift), work);` | 0.1.1 | Initialized at the declaration so that no path can read an indeterminate value; every path assigns the real value before use. For constants and results, the value is read later in the same function or template instantiation, which the tool does not track. | Proposed |
| DEV-007.09 | `core/src/value/floating_conversion.cpp:975` | `const auto widened = denominator.shift_left(static_cast<std::size_t>(-shift), work);` | 0.1.1 | Initialized at the declaration so that no path can read an indeterminate value; every path assigns the real value before use. For constants and results, the value is read later in the same function or template instantiation, which the tool does not track. | Proposed |
| DEV-007.08 | `core/src/value/floating_conversion.cpp:993` | `const auto aligned = denominator.shift_left(static_cast<std::size_t>(c_quotient_bits), ...` | 0.1.1 | Initialized at the declaration so that no path can read an indeterminate value; every path assigns the real value before use. For constants and results, the value is read later in the same function or template instantiation, which the tool does not track. | Proposed |
| DEV-007.22 | `core/src/value/integer_magnitude.cpp:148` | `std::uint32_t digit{};` | 0.1.1 | Initialized at the declaration so that no path can read an indeterminate value; every path assigns the real value before use. For constants and results, the value is read later in the same function or template instantiation, which the tool does not track. | Proposed |
| DEV-007.25 | `core/src/value/value_iterator.cpp:53` | `const container_links* links = nullptr;` | 0.1.1 | Initialized at the declaration so that no path can read an indeterminate value; every path assigns the real value before use. For constants and results, the value is read later in the same function or template instantiation, which the tool does not track. | Proposed |
| DEV-007.26 | `core/src/value/value_view.cpp:93` | `const container_links* links = nullptr;` | 0.1.1 | Initialized at the declaration so that no path can read an indeterminate value; every path assigns the real value before use. For constants and results, the value is read later in the same function or template instantiation, which the tool does not track. | Proposed |
| DEV-007.07 | `core/includes/value/floating_format.hpp:29` | `constexpr std::int32_t c_binary_radix = 2;` | 0.1.1 | Initialized at the declaration so that no path can read an indeterminate value; every path assigns the real value before use. For constants and results, the value is read later in the same function or template instantiation, which the tool does not track. | Proposed |
| DEV-007.06 | `core/includes/value/floating_format.hpp:36` | `constexpr std::int32_t c_float_precision = 24;` | 0.1.1 | Initialized at the declaration so that no path can read an indeterminate value; every path assigns the real value before use. For constants and results, the value is read later in the same function or template instantiation, which the tool does not track. | Proposed |
| DEV-007.05 | `core/includes/value/floating_format.hpp:42` | `constexpr std::int32_t c_float_min_exponent = -125;` | 0.1.1 | Initialized at the declaration so that no path can read an indeterminate value; every path assigns the real value before use. For constants and results, the value is read later in the same function or template instantiation, which the tool does not track. | Proposed |
| DEV-007.04 | `core/includes/value/floating_format.hpp:48` | `constexpr std::int32_t c_float_max_exponent = 128;` | 0.1.1 | Initialized at the declaration so that no path can read an indeterminate value; every path assigns the real value before use. For constants and results, the value is read later in the same function or template instantiation, which the tool does not track. | Proposed |
| DEV-007.03 | `core/includes/value/floating_format.hpp:55` | `constexpr std::int32_t c_double_precision = 53;` | 0.1.1 | Initialized at the declaration so that no path can read an indeterminate value; every path assigns the real value before use. For constants and results, the value is read later in the same function or template instantiation, which the tool does not track. | Proposed |
| DEV-007.02 | `core/includes/value/floating_format.hpp:61` | `constexpr std::int32_t c_double_min_exponent = -1021;` | 0.1.1 | Initialized at the declaration so that no path can read an indeterminate value; every path assigns the real value before use. For constants and results, the value is read later in the same function or template instantiation, which the tool does not track. | Proposed |
| DEV-007.01 | `core/includes/value/floating_format.hpp:67` | `constexpr std::int32_t c_double_max_exponent = 1024;` | 0.1.1 | Initialized at the declaration so that no path can read an indeterminate value; every path assigns the real value before use. For constants and results, the value is read later in the same function or template instantiation, which the tool does not track. | Proposed |
| DEV-007.27 | `core/src/value/floating_conversion.cpp:732` | `const auto zero = finish_underflow<T>(parts.negative, options);` | Initialized at the declaration so that no path can read an indeterminate value; every path assigns the real value before use. For constants and results, the value is read later in the same function or template instantiation, which the tool does not track. | Proposed |
| DEV-007.28 | `core/src/value/floating_conversion.cpp:1280` | `reserve_operands(parts, static_cast<std::size_t>(c_quotient_bits), numerator, denominator, work);` | Initialized at the declaration so that no path can read an indeterminate value; every path assigns the real value before use. For constants and results, the value is read later in the same function or template instantiation, which the tool does not track. | Proposed |
| DEV-007.29 | `core/src/value/floating_conversion.cpp:1366` | `const auto by_scale = reject_by_scale<T>(parts, options);` | Initialized at the declaration so that no path can read an indeterminate value; every path assigns the real value before use. For constants and results, the value is read later in the same function or template instantiation, which the tool does not track. | Proposed |

---

## DEV-008: Rule 0.2.1, span members kept for callers and tests

| Field | Value |
| --- | --- |
| Rule | MISRA C++:2023 Rule 0.2.1 |
| CodeQL query | `cpp/misra/unused-limited-visibility-variable` |
| Lines | 13 |
| Status | Proposed (#166) |

### Why this rule is deviated instead of fixed

Span result members (`byte_offset`, `byte_length`, `line_break_count`) describe the complete recognized range. Unit tests read them; production callers that only need success ignore them.

Removing them would make the internal results incomplete and the tests unable to check ranges.

### Risk and compensating measures

Unit tests on every lexer span.

### Lines

| ID | Location | Code | Justification | Status |
| --- | --- | --- | --- | --- |
| DEV-008.02 | `core/includes/lexer/comment.hpp:26` | `std::size_t byte_offset;` | Part of the span result returned to callers, so the result describes the complete recognized range. Unit tests read the member to verify the range; production callers that only need success ignore it. | Proposed |
| DEV-008.01 | `core/includes/lexer/comment.hpp:32` | `std::size_t byte_length;` | Part of the span result returned to callers, so the result describes the complete recognized range. Unit tests read the member to verify the range; production callers that only need success ignore it. | Proposed |
| DEV-008.04 | `core/includes/lexer/line_terminator.hpp:25` | `std::size_t byte_offset;` | Part of the span result returned to callers, so the result describes the complete recognized range. Unit tests read the member to verify the range; production callers that only need success ignore it. | Proposed |
| DEV-008.03 | `core/includes/lexer/line_terminator.hpp:31` | `std::size_t byte_length;` | Part of the span result returned to callers, so the result describes the complete recognized range. Unit tests read the member to verify the range; production callers that only need success ignore it. | Proposed |
| DEV-008.07 | `core/includes/lexer/member_name_lexer.hpp:65` | `std::size_t byte_offset{};` | Part of the span result returned to callers, so the result describes the complete recognized range. Unit tests read the member to verify the range; production callers that only need success ignore it. | Proposed |
| DEV-008.06 | `core/includes/lexer/member_name_lexer.hpp:71` | `std::size_t byte_length{};` | Part of the span result returned to callers, so the result describes the complete recognized range. Unit tests read the member to verify the range; production callers that only need success ignore it. | Proposed |
| DEV-008.05 | `core/includes/lexer/scalar_token.hpp:45` | `std::size_t byte_offset{};` | Part of the span result returned to callers, so the result describes the complete recognized range. Unit tests read the member to verify the range; production callers that only need success ignore it. | Proposed |
| DEV-008.12 | `core/includes/lexer/trivia.hpp:26` | `std::size_t byte_offset;` | Part of the span result returned to callers, so the result describes the complete recognized range. Unit tests read the member to verify the range; production callers that only need success ignore it. | Proposed |
| DEV-008.11 | `core/includes/lexer/trivia.hpp:32` | `std::size_t byte_length;` | Part of the span result returned to callers, so the result describes the complete recognized range. Unit tests read the member to verify the range; production callers that only need success ignore it. | Proposed |
| DEV-008.10 | `core/includes/lexer/whitespace.hpp:28` | `std::size_t byte_offset;` | Part of the span result returned to callers, so the result describes the complete recognized range. Unit tests read the member to verify the range; production callers that only need success ignore it. | Proposed |
| DEV-008.09 | `core/includes/lexer/whitespace.hpp:34` | `std::size_t byte_length;` | Part of the span result returned to callers, so the result describes the complete recognized range. Unit tests read the member to verify the range; production callers that only need success ignore it. | Proposed |
| DEV-008.08 | `core/includes/lexer/whitespace.hpp:40` | `std::size_t line_break_count;` | Part of the span result returned to callers, so the result describes the complete recognized range. Unit tests read the member to verify the range; production callers that only need success ignore it. | Proposed |
| DEV-008.13 | `includes/adaptive_json5/value/member_lookup.hpp:42` | `member_lookup_state state = member_lookup_state::absent;` | Part of the span result returned to callers, so the result describes the complete recognized range. Unit tests read the member to verify the range; production callers that only need success ignore it. | Proposed |

---

## DEV-009: Rule 4.1.2, explicitly declared copy members and constexpr variables

| Field | Value |
| --- | --- |
| Rule | MISRA C++:2023 Rule 4.1.2 |
| CodeQL query | `cpp/misra/implicit-declaration-of-copy-constructor, cpp/misra/redeclaration-of-static-constexpr-data-member` |
| Lines | 2 |
| Status | Proposed (#166) |

### Why this rule is deviated instead of fixed

The classes declare their copy members explicitly, and `c_literal_spelling` is a namespace-scope constexpr variable declared once. The query reports these as deprecated implicit declarations or redeclarations.

Nothing to change in the code.

### Risk and compensating measures

Code review.

### Lines

| ID | Location | Code | Justification | Status |
| --- | --- | --- | --- | --- |
| DEV-009.01 | `core/src/lexer/scalar_lexer.cpp:53` | `constexpr std::array<literal_spelling, 3> c_literal_spelling{{{"null", literal_kind::nu...` | c_literal_spelling is a namespace-scope constexpr variable, not a static data member, and it is declared only once. | Proposed |
| DEV-009.02 | `core/includes/value/record_link.hpp:27` | `class [[nodiscard]] record_link` | The copy constructor and copy assignment are explicitly declared in the class (= default or = delete), so nothing relies on a deprecated implicit declaration. | Proposed |
| DEV-009.03 | `core/src/diagnostics/diagnostic_format.cpp:229` | `static constexpr std::array<description, 20U> messages{` | messages is a function-local static constexpr variable, not a static data member, and it is declared only once. | Proposed |

---

## DEV-010: Rule 5.13.4, empty-brace value-initialization

| Field | Value |
| --- | --- |
| Rule | MISRA C++:2023 Rule 5.13.4 |
| CodeQL query | `cpp/misra/unsigned-integer-literals-not-appropriately-suffixed` |
| Lines | 24 |
| Status | Proposed (#166) |

### Why this rule is deviated instead of fixed

`T x{};` contains no integer literal; the query reports the implicit zero as an unsuffixed octal literal.

Nothing to change in the code.

### Risk and compensating measures

Code review.

### Lines

| ID | Location | Code | Justification | Status |
| --- | --- | --- | --- | --- |
| DEV-010.13 | `core/src/lexer/identifier/identifier_lexer.cpp:119` | `identifier_walk walk{};` | Empty-brace value-initialization contains no integer literal. The tool reports the implicit zero of the initialization as an unsuffixed octal literal. | Proposed |
| DEV-010.21 | `core/src/lexer/string/string_escape.cpp:89` | `encoded_utf16 output{};` | Empty-brace value-initialization contains no integer literal. The tool reports the implicit zero of the initialization as an unsuffixed octal literal. | Proposed |
| DEV-010.20 | `core/src/lexer/string/string_escape.cpp:111` | `return result<encoded_utf16>::success(encoded_utf16{});` | Empty-brace value-initialization contains no integer literal. The tool reports the implicit zero of the initialization as an unsuffixed octal literal. | Proposed |
| DEV-010.14 | `core/src/lexer/string/string_lexer.cpp:90` | `string_walk walk{};` | Empty-brace value-initialization contains no integer literal. The tool reports the implicit zero of the initialization as an unsuffixed octal literal. | Proposed |
| DEV-010.02 | `core/src/lexer/string/string_warning.cpp:59` | `surrogate_warnings found{};` | Empty-brace value-initialization contains no integer literal. The tool reports the implicit zero of the initialization as an unsuffixed octal literal. | Proposed |
| DEV-010.01 | `core/src/lexer/string/string_warning.cpp:126` | `surrogate_warnings found{};` | Empty-brace value-initialization contains no integer literal. The tool reports the implicit zero of the initialization as an unsuffixed octal literal. | Proposed |
| DEV-010.22 | `core/src/parser/parser_container_open.cpp:49` | `parser_frame frame{};` | Empty-brace value-initialization contains no integer literal. The tool reports the implicit zero of the initialization as an unsuffixed octal literal. | Proposed |
| DEV-010.19 | `core/src/parser/primitive_parser.cpp:214` | `primitive_value value{};` | Empty-brace value-initialization contains no integer literal. The tool reports the implicit zero of the initialization as an unsuffixed octal literal. | Proposed |
| DEV-010.23 | `core/src/unicode/utf16.cpp:40` | `encoded_utf16 output{};` | Empty-brace value-initialization contains no integer literal. The tool reports the implicit zero of the initialization as an unsuffixed octal literal. | Proposed |
| DEV-010.03 | `core/src/value/integer_magnitude.cpp:103` | `integer_magnitude converted{};` | Empty-brace value-initialization contains no integer literal. The tool reports the implicit zero of the initialization as an unsuffixed octal literal. | Proposed |
| DEV-010.24 | `core/src/value/number_analysis.cpp:55` | `number_analysis analysis{};` | Empty-brace value-initialization contains no integer literal. The tool reports the implicit zero of the initialization as an unsuffixed octal literal. | Proposed |
| DEV-010.04 | `core/includes/input/cursor.hpp:30` | `input_view m_view{};` | Empty-brace value-initialization contains no integer literal. The tool reports the implicit zero of the initialization as an unsuffixed octal literal. | Proposed |
| DEV-010.15 | `core/includes/parser/parser_engine.hpp:94` | `parser_frame m_count_top{};` | Empty-brace value-initialization contains no integer literal. The tool reports the implicit zero of the initialization as an unsuffixed octal literal. | Proposed |
| DEV-010.16 | `core/includes/parser/primitive_parser.hpp:65` | `primitive_value root{};` | Empty-brace value-initialization contains no integer literal. The tool reports the implicit zero of the initialization as an unsuffixed octal literal. | Proposed |
| DEV-010.18 | `core/includes/value/container_links.hpp:53` | `container_links elements{};` | Empty-brace value-initialization contains no integer literal. The tool reports the implicit zero of the initialization as an unsuffixed octal literal. | Proposed |
| DEV-010.17 | `core/includes/value/container_links.hpp:76` | `container_links members{};` | Empty-brace value-initialization contains no integer literal. The tool reports the implicit zero of the initialization as an unsuffixed octal literal. | Proposed |
| DEV-010.11 | `includes/adaptive_json5/error.hpp:55` | `source_span first{};` | Empty-brace value-initialization contains no integer literal. The tool reports the implicit zero of the initialization as an unsuffixed octal literal. | Proposed |
| DEV-010.10 | `includes/adaptive_json5/error.hpp:60` | `source_span repeated{};` | Empty-brace value-initialization contains no integer literal. The tool reports the implicit zero of the initialization as an unsuffixed octal literal. | Proposed |
| DEV-010.09 | `includes/adaptive_json5/error.hpp:426` | `return error{error_code::numeric_conversion, c_no_offset, c_has_reason, source_span{},` | Empty-brace value-initialization contains no integer literal. The tool reports the implicit zero of the initialization as an unsuffixed octal literal. | Proposed |
| DEV-010.08 | `includes/adaptive_json5/error.hpp:447` | `return error{error_code::syntax, byte_offset, c_has_offset \| c_has_reason, source_span{},` | Empty-brace value-initialization contains no integer literal. The tool reports the implicit zero of the initialization as an unsuffixed octal literal. | Proposed |
| DEV-010.07 | `includes/adaptive_json5/error.hpp:502` | `return error{error_code::resource_limit, byte_offset, c_has_offset \| c_has_reason, sou...` | Empty-brace value-initialization contains no integer literal. The tool reports the implicit zero of the initialization as an unsuffixed octal literal. | Proposed |
| DEV-010.06 | `includes/adaptive_json5/error.hpp:522` | `return error{error_code::resource_limit, c_no_offset, c_has_reason, source_span{},` | Empty-brace value-initialization contains no integer literal. The tool reports the implicit zero of the initialization as an unsuffixed octal literal. | Proposed |
| DEV-010.05 | `includes/adaptive_json5/error.hpp:541` | `return error{error_code::wrong_type_or_access, c_no_offset, c_has_reason, source_span{},` | Empty-brace value-initialization contains no integer literal. The tool reports the implicit zero of the initialization as an unsuffixed octal literal. | Proposed |
| DEV-010.12 | `includes/adaptive_json5/result.hpp:62` | `static constexpr std::size_t c_value_index = 0;` | Empty-brace value-initialization contains no integer literal. The tool reports the implicit zero of the initialization as an unsuffixed octal literal. | Proposed |
| DEV-010.26 | `core/src/value/number_analysis.cpp:344` | `decimal_exponent exponent{};` | Empty-brace value-initialization contains no integer literal. The tool reports the implicit zero of the initialization as an unsuffixed octal literal. | Proposed |
| DEV-010.25 | `core/src/lexer/string/string_lexer.cpp:315` | `string_body_state state{max_units, warning_allowance, output, sink, checks};` | Aggregate initialization value-initializes the members it does not list and contains no integer literal. The tool reports that implicit zero as an unsuffixed octal literal. | Proposed |

---

## DEV-011: Rule 6.5.2, static member function in an anonymous namespace

| Field | Value |
| --- | --- |
| Rule | MISRA C++:2023 Rule 6.5.2 |
| CodeQL query | `cpp/misra/internal-linkage-specified-appropriately` |
| Lines | 1 |
| Status | Proposed (#166) |

### Why this rule is deviated instead of fixed

`static` declares a static member function of a class inside the anonymous namespace, not a namespace-scope static.

Nothing to change in the code.

### Risk and compensating measures

Code review.

### Lines

| ID | Location | Code | Justification | Status |
| --- | --- | --- | --- | --- |
| DEV-011.01 | `core/src/diagnostics/diagnostic_format.cpp:70` | `[[nodiscard]] static result<message_writer> create(char* buffer, std::size_t capacity) ...` | Here static declares a static member function of a class in the anonymous namespace; it is not a namespace-scope static, so it does not duplicate the anonymous namespace's internal linkage. | Proposed |

---

## DEV-012: Rule 6.8.4, reference accessors of `json5_document`

| Field | Value |
| --- | --- |
| Rule | MISRA C++:2023 Rule 6.8.4 |
| CodeQL query | `cpp/misra/member-functions-refqualified` |
| Lines | 2 |
| Status | Proposed (#166) |

### Why this rule is deviated instead of fixed

`values()` and `source()` return references into the document, valid while the `json5_document` lives as the public contract states.

Adding `const&` alone does not satisfy the query; only deleting the rvalue overloads would, which changes the public API. The owner chose to keep the API (#166).

### Risk and compensating measures

Documented lifetime contract; unit tests.

### Lines

| ID | Location | Code | Justification | Status |
| --- | --- | --- | --- | --- |
| DEV-012.02 | `core/src/memory/json5_document.cpp:207` | `const document& json5_document::values() const noexcept` | The reference stays valid while the json5_document lives, as the public contract states. Ref-qualifying or deleting the rvalue overload would change the public API. | Proposed |
| DEV-012.01 | `core/src/memory/json5_document.cpp:223` | `const retained_source& json5_document::source() const noexcept` | The reference stays valid while the json5_document lives, as the public contract states. Ref-qualifying or deleting the rvalue overload would change the public API. | Proposed |

---

## DEV-013: Rule 6.9.1, the same alias in declaration and definition

| Field | Value |
| --- | --- |
| Rule | MISRA C++:2023 Rule 6.9.1 |
| CodeQL query | `cpp/misra/type-aliases-declaration` |
| Lines | 41 |
| Status | Proposed (#166) |

### Why this rule is deviated instead of fixed

Declarations and definitions both use `status_result` (or the same callback alias). There is one alias declaration; the query reports it as different across translation units.

Nothing to change in the code.

### Risk and compensating measures

Code review; the compiler rejects mismatched types.

### Lines

| ID | Location | Code | Justification | Status |
| --- | --- | --- | --- | --- |
| DEV-013.12 | `core/src/parser/parser_engine.cpp:96` | `status_result parser_engine::read_value() noexcept` | The declaration and the definition use the same alias (status_result for result<void>, or the same callback alias). There is only one alias declaration; the tool compares the alias across translation units and reports it as different. | Proposed |
| DEV-013.11 | `core/src/parser/parser_engine.cpp:353` | `status_result parser_engine::read_name() noexcept` | The declaration and the definition use the same alias (status_result for result<void>, or the same callback alias). There is only one alias declaration; the tool compares the alias across translation units and reports it as different. | Proposed |
| DEV-013.10 | `core/src/parser/parser_engine.cpp:479` | `status_result parser_engine::consume_separator(parser_action action) noexcept` | The declaration and the definition use the same alias (status_result for result<void>, or the same callback alias). There is only one alias declaration; the tool compares the alias across translation units and reports it as different. | Proposed |
| DEV-013.09 | `core/src/parser/parser_engine.cpp:570` | `status_result parser_engine::finish_container() noexcept` | The declaration and the definition use the same alias (status_result for result<void>, or the same callback alias). There is only one alias declaration; the tool compares the alias across translation units and reports it as different. | Proposed |
| DEV-013.08 | `core/src/parser/parser_engine.cpp:622` | `status_result parser_engine::accept_completed_value(value_index completed) noexcept` | The declaration and the definition use the same alias (status_result for result<void>, or the same callback alias). There is only one alias declaration; the tool compares the alias across translation units and reports it as different. | Proposed |
| DEV-013.07 | `core/src/parser/parser_engine.cpp:658` | `status_result parser_engine::reserve_string(void* context, std::size_t units, std::size...` | The declaration and the definition use the same alias (status_result for result<void>, or the same callback alias). There is only one alias declaration; the tool compares the alias across translation units and reports it as different. | Proposed |
| DEV-013.06 | `core/src/parser/parser_engine.cpp:771` | `status_result parser_engine::count_value() noexcept` | The declaration and the definition use the same alias (status_result for result<void>, or the same callback alias). There is only one alias declaration; the tool compares the alias across translation units and reports it as different. | Proposed |
| DEV-013.05 | `core/src/parser/parser_engine.cpp:972` | `status_result parser_engine::count_name() noexcept` | The declaration and the definition use the same alias (status_result for result<void>, or the same callback alias). There is only one alias declaration; the tool compares the alias across translation units and reports it as different. | Proposed |
| DEV-013.04 | `core/src/parser/parser_engine.cpp:1066` | `status_result parser_engine::count_separator(parser_action action) noexcept` | The declaration and the definition use the same alias (status_result for result<void>, or the same callback alias). There is only one alias declaration; the tool compares the alias across translation units and reports it as different. | Proposed |
| DEV-013.03 | `core/src/parser/parser_engine.cpp:1124` | `status_result parser_engine::count_close() noexcept` | The declaration and the definition use the same alias (status_result for result<void>, or the same callback alias). There is only one alias declaration; the tool compares the alias across translation units and reports it as different. | Proposed |
| DEV-013.02 | `core/src/parser/parser_engine.cpp:1194` | `status_result parser_engine::count_completed_value() noexcept` | The declaration and the definition use the same alias (status_result for result<void>, or the same callback alias). There is only one alias declaration; the tool compares the alias across translation units and reports it as different. | Proposed |
| DEV-013.01 | `core/src/parser/parser_engine.cpp:1237` | `status_result parser_engine::count_document(bool complete) noexcept` | The declaration and the definition use the same alias (status_result for result<void>, or the same callback alias). There is only one alias declaration; the tool compares the alias across translation units and reports it as different. | Proposed |
| DEV-013.38 | `core/src/parser/parser_stack.cpp:88` | `status_result parser_stack::reserve_exact(std::size_t frames) noexcept` | The declaration and the definition use the same alias (status_result for result<void>, or the same callback alias). There is only one alias declaration; the tool compares the alias across translation units and reports it as different. | Proposed |
| DEV-013.37 | `core/src/parser/parser_stack.cpp:163` | `status_result parser_stack::push(parser_frame&& frame, work_budget& work) noexcept` | The declaration and the definition use the same alias (status_result for result<void>, or the same callback alias). There is only one alias declaration; the tool compares the alias across translation units and reports it as different. | Proposed |
| DEV-013.39 | `core/includes/lexer/string/string_lexer.hpp:35` | `[[nodiscard]] status_result notify_string_warnings(cursor source, string_quote quote, s...` | The declaration and the definition use the same alias (status_result for result<void>, or the same callback alias). There is only one alias declaration; the tool compares the alias across translation units and reports it as different. | Proposed |
| DEV-013.24 | `core/includes/lexer/string/string_token.hpp:82` | `string_reserve_function reserve = nullptr;` | The declaration and the definition use the same alias (status_result for result<void>, or the same callback alias). There is only one alias declaration; the tool compares the alias across translation units and reports it as different. | Proposed |
| DEV-013.36 | `core/includes/parser/parser_engine.hpp:123` | `[[nodiscard]] status_result count_value() noexcept;` | The declaration and the definition use the same alias (status_result for result<void>, or the same callback alias). There is only one alias declaration; the tool compares the alias across translation units and reports it as different. | Proposed |
| DEV-013.35 | `core/includes/parser/parser_engine.hpp:139` | `[[nodiscard]] status_result count_name() noexcept;` | The declaration and the definition use the same alias (status_result for result<void>, or the same callback alias). There is only one alias declaration; the tool compares the alias across translation units and reports it as different. | Proposed |
| DEV-013.34 | `core/includes/parser/parser_engine.hpp:153` | `[[nodiscard]] status_result count_separator(parser_action action) noexcept;` | The declaration and the definition use the same alias (status_result for result<void>, or the same callback alias). There is only one alias declaration; the tool compares the alias across translation units and reports it as different. | Proposed |
| DEV-013.33 | `core/includes/parser/parser_engine.hpp:167` | `[[nodiscard]] status_result count_close() noexcept;` | The declaration and the definition use the same alias (status_result for result<void>, or the same callback alias). There is only one alias declaration; the tool compares the alias across translation units and reports it as different. | Proposed |
| DEV-013.32 | `core/includes/parser/parser_engine.hpp:180` | `[[nodiscard]] status_result count_completed_value() noexcept;` | The declaration and the definition use the same alias (status_result for result<void>, or the same callback alias). There is only one alias declaration; the tool compares the alias across translation units and reports it as different. | Proposed |
| DEV-013.31 | `core/includes/parser/parser_engine.hpp:197` | `[[nodiscard]] status_result count_document(bool complete) noexcept;` | The declaration and the definition use the same alias (status_result for result<void>, or the same callback alias). There is only one alias declaration; the tool compares the alias across translation units and reports it as different. | Proposed |
| DEV-013.30 | `core/includes/parser/parser_engine.hpp:227` | `[[nodiscard]] status_result read_value() noexcept;` | The declaration and the definition use the same alias (status_result for result<void>, or the same callback alias). There is only one alias declaration; the tool compares the alias across translation units and reports it as different. | Proposed |
| DEV-013.29 | `core/includes/parser/parser_engine.hpp:250` | `[[nodiscard]] status_result read_name() noexcept;` | The declaration and the definition use the same alias (status_result for result<void>, or the same callback alias). There is only one alias declaration; the tool compares the alias across translation units and reports it as different. | Proposed |
| DEV-013.28 | `core/includes/parser/parser_engine.hpp:267` | `[[nodiscard]] status_result consume_separator(parser_action action) noexcept;` | The declaration and the definition use the same alias (status_result for result<void>, or the same callback alias). There is only one alias declaration; the tool compares the alias across translation units and reports it as different. | Proposed |
| DEV-013.27 | `core/includes/parser/parser_engine.hpp:285` | `[[nodiscard]] status_result finish_container() noexcept;` | The declaration and the definition use the same alias (status_result for result<void>, or the same callback alias). There is only one alias declaration; the tool compares the alias across translation units and reports it as different. | Proposed |
| DEV-013.26 | `core/includes/parser/parser_engine.hpp:300` | `[[nodiscard]] status_result accept_completed_value(value_index completed) noexcept;` | The declaration and the definition use the same alias (status_result for result<void>, or the same callback alias). There is only one alias declaration; the tool compares the alias across translation units and reports it as different. | Proposed |
| DEV-013.25 | `core/includes/parser/parser_engine.hpp:324` | `[[nodiscard]] static status_result reserve_string(void* context, std::size_t units,` | The declaration and the definition use the same alias (status_result for result<void>, or the same callback alias). There is only one alias declaration; the tool compares the alias across translation units and reports it as different. | Proposed |
| DEV-013.41 | `core/includes/parser/parser_stack.hpp:135` | `[[nodiscard]] status_result reserve_exact(std::size_t frames) noexcept;` | The declaration and the definition use the same alias (status_result for result<void>, or the same callback alias). There is only one alias declaration; the tool compares the alias across translation units and reports it as different. | Proposed |
| DEV-013.40 | `core/includes/parser/parser_stack.hpp:153` | `[[nodiscard]] status_result push(parser_frame&& frame, work_budget& work) noexcept;` | The declaration and the definition use the same alias (status_result for result<void>, or the same callback alias). There is only one alias declaration; the tool compares the alias across translation units and reports it as different. | Proposed |
| DEV-013.23 | `includes/adaptive_json5/memory/storage.hpp:56` | `allocate_function allocate = nullptr;` | The declaration and the definition use the same alias (status_result for result<void>, or the same callback alias). There is only one alias declaration; the tool compares the alias across translation units and reports it as different. | Proposed |
| DEV-013.20 | `includes/adaptive_json5/value/document.hpp:102` | `[[nodiscard]] status_result reserve(std::size_t required) noexcept;` | The declaration and the definition use the same alias (status_result for result<void>, or the same callback alias). There is only one alias declaration; the tool compares the alias across translation units and reports it as different. | Proposed |
| DEV-013.19 | `includes/adaptive_json5/value/document.hpp:177` | `[[nodiscard]] status_result reserve_exact(std::size_t records) noexcept;` | The declaration and the definition use the same alias (status_result for result<void>, or the same callback alias). There is only one alias declaration; the tool compares the alias across translation units and reports it as different. | Proposed |
| DEV-013.18 | `includes/adaptive_json5/value/document.hpp:356` | `[[nodiscard]] status_result append_element(value_index array, value_index child) noexcept;` | The declaration and the definition use the same alias (status_result for result<void>, or the same callback alias). There is only one alias declaration; the tool compares the alias across translation units and reports it as different. | Proposed |
| DEV-013.17 | `includes/adaptive_json5/value/document.hpp:378` | `[[nodiscard]] status_result append_member(value_index object, decoded_string&& name, va...` | The declaration and the definition use the same alias (status_result for result<void>, or the same callback alias). There is only one alias declaration; the tool compares the alias across translation units and reports it as different. | Proposed |
| DEV-013.16 | `includes/adaptive_json5/value/document.hpp:403` | `[[nodiscard]] status_result append_member(value_index object, decoded_string&& name, va...` | The declaration and the definition use the same alias (status_result for result<void>, or the same callback alias). There is only one alias declaration; the tool compares the alias across translation units and reports it as different. | Proposed |
| DEV-013.15 | `includes/adaptive_json5/value/document.hpp:426` | `[[nodiscard]] status_result set_source_span(value_index index, adaptive_json5::source_s...` | The declaration and the definition use the same alias (status_result for result<void>, or the same callback alias). There is only one alias declaration; the tool compares the alias across translation units and reports it as different. | Proposed |
| DEV-013.14 | `includes/adaptive_json5/value/document.hpp:449` | `[[nodiscard]] status_result set_member_name_source_span(value_index index, adaptive_jso...` | The declaration and the definition use the same alias (status_result for result<void>, or the same callback alias). There is only one alias declaration; the tool compares the alias across translation units and reports it as different. | Proposed |
| DEV-013.13 | `includes/adaptive_json5/value/document.hpp:465` | `[[nodiscard]] status_result set_root(value_index index) noexcept;` | The declaration and the definition use the same alias (status_result for result<void>, or the same callback alias). There is only one alias declaration; the tool compares the alias across translation units and reports it as different. | Proposed |
| DEV-013.22 | `includes/adaptive_json5/value/warning_list.hpp:126` | `[[nodiscard]] status_result reserve(std::size_t additional) noexcept;` | The declaration and the definition use the same alias (status_result for result<void>, or the same callback alias). There is only one alias declaration; the tool compares the alias across translation units and reports it as different. | Proposed |
| DEV-013.21 | `includes/adaptive_json5/value/warning_list.hpp:142` | `[[nodiscard]] status_result append_reserved(const string_warning& warning) noexcept;` | The declaration and the definition use the same alias (status_result for result<void>, or the same callback alias). There is only one alias declaration; the tool compares the alias across translation units and reports it as different. | Proposed |
| DEV-013.42 | `core/includes/parser/parser_engine.hpp:84` | `[[nodiscard]] status_result open_value_container(bool member) noexcept;` | The declaration and the definition use the same alias (status_result for result<void>, or the same callback alias). There is only one alias declaration; the tool compares the alias across translation units and reports it as different. | Proposed |
| DEV-013.43 | `core/includes/parser/parser_engine.hpp:97` | `[[nodiscard]] status_result append_scanned_scalar(scalar_token& token, source_span span) noexcept;` | The declaration and the definition use the same alias (status_result for result<void>, or the same callback alias). There is only one alias declaration; the tool compares the alias across translation units and reports it as different. | Proposed |
| DEV-013.44 | `core/includes/parser/parser_engine.hpp:110` | `[[nodiscard]] status_result read_scalar_value() noexcept;` | The declaration and the definition use the same alias (status_result for result<void>, or the same callback alias). There is only one alias declaration; the tool compares the alias across translation units and reports it as different. | Proposed |
| DEV-013.45 | `core/includes/parser/parser_engine.hpp:123` | `[[nodiscard]] status_result accept_scanned_name(parser_frame& frame, member_name_token& name,` | The declaration and the definition use the same alias (status_result for result<void>, or the same callback alias). There is only one alias declaration; the tool compares the alias across translation units and reports it as different. | Proposed |
| DEV-013.46 | `core/includes/parser/parser_engine.hpp:137` | `[[nodiscard]] status_result count_open_container(char opening) noexcept;` | The declaration and the definition use the same alias (status_result for result<void>, or the same callback alias). There is only one alias declaration; the tool compares the alias across translation units and reports it as different. | Proposed |
| DEV-013.47 | `core/includes/parser/parser_engine.hpp:150` | `[[nodiscard]] status_result reserve_measured_scalar(const scalar_measure& value) noexcept;` | The declaration and the definition use the same alias (status_result for result<void>, or the same callback alias). There is only one alias declaration; the tool compares the alias across translation units and reports it as different. | Proposed |
| DEV-013.48 | `core/includes/parser/parser_engine.hpp:163` | `[[nodiscard]] status_result count_scalar_value() noexcept;` | The declaration and the definition use the same alias (status_result for result<void>, or the same callback alias). There is only one alias declaration; the tool compares the alias across translation units and reports it as different. | Proposed |
| DEV-013.49 | `core/includes/parser/parser_engine.hpp:179` | `[[nodiscard]] status_result walk_document() noexcept;` | The declaration and the definition use the same alias (status_result for result<void>, or the same callback alias). There is only one alias declaration; the tool compares the alias across translation units and reports it as different. | Proposed |
| DEV-013.50 | `core/includes/parser/parser_engine.hpp:193` | `[[nodiscard]] status_result apply_action(parser_action action) noexcept;` | The declaration and the definition use the same alias (status_result for result<void>, or the same callback alias). There is only one alias declaration; the tool compares the alias across translation units and reports it as different. | Proposed |
| DEV-013.51 | `core/includes/parser/parser_engine.hpp:206` | `[[nodiscard]] status_result finish_input(bool complete) noexcept;` | The declaration and the definition use the same alias (status_result for result<void>, or the same callback alias). There is only one alias declaration; the tool compares the alias across translation units and reports it as different. | Proposed |
| DEV-013.52 | `core/includes/parser/parser_engine.hpp:219` | `[[nodiscard]] status_result walk_value() noexcept;` | The declaration and the definition use the same alias (status_result for result<void>, or the same callback alias). There is only one alias declaration; the tool compares the alias across translation units and reports it as different. | Proposed |
| DEV-013.53 | `core/includes/parser/parser_engine.hpp:232` | `[[nodiscard]] status_result advance_separator(parser_state& state, parser_action action) noexcept;` | The declaration and the definition use the same alias (status_result for result<void>, or the same callback alias). There is only one alias declaration; the tool compares the alias across translation units and reports it as different. | Proposed |

---

## DEV-014: Rule 7.0.1, explicit discard of `pop()`

| Field | Value |
| --- | --- |
| Rule | MISRA C++:2023 Rule 7.0.1 |
| CodeQL query | `cpp/misra/no-conversion-from-bool` |
| Lines | 2 |
| Status | Proposed (#166) |

### Why this rule is deviated instead of fixed

`static_cast<void>(frames.pop())` discards the result where a frame is known to exist, as Rule 0.1.2 permits.

Checking the result would add a failure branch that can never be taken and work against full branch coverage; the owner chose the explicit discard (#166).

### Risk and compensating measures

Unit tests on container open and close.

### Lines

| ID | Location | Code | Justification | Status |
| --- | --- | --- | --- | --- |
| DEV-014.02 | `core/src/parser/parser_container_close.cpp:175` | `static_cast<void>(frames.pop());` | The cast to void explicitly discards pop()'s result, as Rule 0.1.2 permits; it is not a numeric conversion. A frame is known to exist here, so the result carries no information. | Proposed |
| DEV-014.01 | `core/src/parser/parser_container_open.cpp:104` | `static_cast<void>(frames.pop());` | The cast to void explicitly discards pop()'s result, as Rule 0.1.2 permits; it is not a numeric conversion. A frame is known to exist here, so the result carries no information. | Proposed |

---

## DEV-015: Rule 7.0.3, character comparisons and digit values

| Field | Value |
| --- | --- |
| Rule | MISRA C++:2023 Rule 7.0.3 |
| CodeQL query | `cpp/misra/no-character-numerical-value` |
| Lines | 81 |
| Status | Proposed (#166) |

### Why this rule is deviated instead of fixed

The parser compares UTF-8 input bytes with character literals and converts validated digits and hexadecimal letters to their values.

Lookup tables for every comparison would make the lexer much harder to read without removing the dependency on the character set.

### Risk and compensating measures

Relies on SEooC assumption [A-10](seooc-assumptions.md#a-10-the-compiler-uses-an-ascii-compatible-execution-character-set) (safety manual C-2.1a). Unit and validation tests cover every character class.

### Lines

| ID | Location | Code | Justification | Status |
| --- | --- | --- | --- | --- |
| DEV-015.26 | `core/src/diagnostics/diagnostic_format.cpp:133` | `digits[start] = static_cast<char>('0' + (value % 10U));` | Converts an already validated digit or hexadecimal letter to its value, or a value 0-9 back to its digit. The C++ standard guarantees '0'-'9' are contiguous. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.44 | `core/src/lexer/comment.cpp:42` | `if ((!first.has_value()) \|\| (!second.has_value()) \|\| (*first != c_slash) \|\| (*sec...` | Tests whether the byte equals one specific character. An equality comparison between two character operands does not use the numeric value; the tool reports the implicit promotion of both operands to int. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII- compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.43 | `core/src/lexer/comment.cpp:123` | `if ((!first.has_value()) \|\| (!second.has_value()) \|\| (*first != c_slash) \|\| (*sec...` | Tests whether the byte equals one specific character. An equality comparison between two character operands does not use the numeric value; the tool reports the implicit promotion of both operands to int. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII- compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.42 | `core/src/lexer/comment.cpp:146` | `if (current.has_value() && following.has_value() && (*current == c_asterisk) && (*follo...` | Tests whether the byte equals one specific character. An equality comparison between two character operands does not use the numeric value; the tool reports the implicit promotion of both operands to int. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.29 | `core/src/lexer/identifier/identifier_lexer.cpp:136` | `const bool escaped = next.has_value() && (*next == '\\');` | Tests whether the byte equals one specific character. An equality comparison between two character operands does not use the numeric value; the tool reports the implicit promotion of both operands to int. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.45 | `core/src/lexer/line_terminator.cpp:78` | `if ((following.has_value()) && (*following == c_line_feed_byte))` | Tests whether the byte equals one specific character. An equality comparison between two character operands does not use the numeric value; the tool reports the implicit promotion of both operands to int. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.66 | `core/src/lexer/number/decimal_lexer.cpp:97` | `const bool signed_start = (*initial == '+') \|\| (*initial == '-');` | Tests whether the byte equals one specific character. An equality comparison between two character operands does not use the numeric value; the tool reports the implicit promotion of both operands to int. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII- compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.65 | `core/src/lexer/number/decimal_lexer.cpp:104` | `if (!signed_start && (*initial != '.') && !detail::is_decimal_digit(initial))` | Tests whether the byte equals one specific character. An equality comparison between two character operands does not use the numeric value; the tool reports the implicit promotion of both operands to int. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII- compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.64 | `core/src/lexer/number/decimal_lexer.cpp:120` | `if ((*initial == '+') && (mode == syntax_mode::strict_json))` | Tests whether the byte equals one specific character. An equality comparison between two character operands does not use the numeric value; the tool reports the implicit promotion of both operands to int. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.63 | `core/src/lexer/number/decimal_lexer.cpp:137` | `if (first.has_value() && ((*first == 'I') \|\| (*first == 'N')))` | Tests whether the byte equals one specific character. An equality comparison between two character operands does not use the numeric value; the tool reports the implicit promotion of both operands to int. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII- compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.62 | `core/src/lexer/number/decimal_lexer.cpp:164` | `if (*first == '0')` | Tests whether the byte equals one specific character. An equality comparison between two character operands does not use the numeric value; the tool reports the implicit promotion of both operands to int. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.74 | `core/src/lexer/number/hexadecimal_lexer.cpp:55` | `if (initial.has_value() && ((*initial == '+') \|\| (*initial == '-')))` | Tests whether the byte equals one specific character. An equality comparison between two character operands does not use the numeric value; the tool reports the implicit promotion of both operands to int. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII- compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.73 | `core/src/lexer/number/hexadecimal_lexer.cpp:69` | `if (!zero.has_value() \|\| (*zero != '0'))` | Tests whether the byte equals one specific character. An equality comparison between two character operands does not use the numeric value; the tool reports the implicit promotion of both operands to int. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII- compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.72 | `core/src/lexer/number/hexadecimal_lexer.cpp:80` | `if (!marker.has_value() \|\| ((*marker != 'x') && (*marker != 'X')))` | Tests whether the byte equals one specific character. An equality comparison between two character operands does not use the numeric value; the tool reports the implicit promotion of both operands to int. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII- compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.46 | `core/src/lexer/number/non_finite_lexer.cpp:68` | `if (initial.has_value() && ((*initial == '-') \|\| (*initial == '+')))` | Tests whether the byte equals one specific character. An equality comparison between two character operands does not use the numeric value; the tool reports the implicit promotion of both operands to int. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII- compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.25 | `core/src/lexer/punctuation.cpp:47` | `switch (*current)` | Selects on the exact character with character case labels; no arithmetic uses the character value. The tool reports the integral promotion of the condition and labels. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.24 | `core/src/lexer/punctuation.cpp:55` | `case '{':` | Selects on the exact character with character case labels; no arithmetic uses the character value. The tool reports the integral promotion of the condition and labels. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.23 | `core/src/lexer/punctuation.cpp:65` | `case '}':` | Selects on the exact character with character case labels; no arithmetic uses the character value. The tool reports the integral promotion of the condition and labels. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.22 | `core/src/lexer/punctuation.cpp:75` | `case '[':` | Selects on the exact character with character case labels; no arithmetic uses the character value. The tool reports the integral promotion of the condition and labels. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.21 | `core/src/lexer/punctuation.cpp:85` | `case ']':` | Selects on the exact character with character case labels; no arithmetic uses the character value. The tool reports the integral promotion of the condition and labels. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.20 | `core/src/lexer/punctuation.cpp:95` | `case ':':` | Selects on the exact character with character case labels; no arithmetic uses the character value. The tool reports the integral promotion of the condition and labels. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.19 | `core/src/lexer/punctuation.cpp:105` | `case ',':` | Selects on the exact character with character case labels; no arithmetic uses the character value. The tool reports the integral promotion of the condition and labels. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.28 | `core/src/lexer/scalar_lexer.cpp:82` | `if (*next == '\\')` | Tests whether the byte equals one specific character. An equality comparison between two character operands does not use the numeric value; the tool reports the implicit promotion of both operands to int. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII- compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.27 | `core/src/lexer/scalar_lexer.cpp:127` | `if ((!byte.has_value()) \|\| (*byte != literal.text[index]))` | Tests whether the byte equals one specific character. An equality comparison between two character operands does not use the numeric value; the tool reports the implicit promotion of both operands to int. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.41 | `core/src/lexer/string/hex_escape.cpp:88` | `if (*current != c_hex_introducer)` | Tests whether the byte equals one specific character. An equality comparison between two character operands does not use the numeric value; the tool reports the implicit promotion of both operands to int. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII- compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.60 | `core/src/lexer/string/simple_escape.cpp:53` | `switch (*current)` | Selects on the exact character with character case labels; no arithmetic uses the character value. The tool reports the integral promotion of the condition and labels. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.59 | `core/src/lexer/string/simple_escape.cpp:61` | `case '"':` | Selects on the exact character with character case labels; no arithmetic uses the character value. The tool reports the integral promotion of the condition and labels. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.58 | `core/src/lexer/string/simple_escape.cpp:71` | `case '\'':` | Selects on the exact character with character case labels; no arithmetic uses the character value. The tool reports the integral promotion of the condition and labels. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.57 | `core/src/lexer/string/simple_escape.cpp:82` | `case '\\':` | Selects on the exact character with character case labels; no arithmetic uses the character value. The tool reports the integral promotion of the condition and labels. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.56 | `core/src/lexer/string/simple_escape.cpp:92` | `case '/':` | Selects on the exact character with character case labels; no arithmetic uses the character value. The tool reports the integral promotion of the condition and labels. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.55 | `core/src/lexer/string/simple_escape.cpp:102` | `case 'b':` | Selects on the exact character with character case labels; no arithmetic uses the character value. The tool reports the integral promotion of the condition and labels. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.54 | `core/src/lexer/string/simple_escape.cpp:112` | `case 'f':` | Selects on the exact character with character case labels; no arithmetic uses the character value. The tool reports the integral promotion of the condition and labels. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.53 | `core/src/lexer/string/simple_escape.cpp:122` | `case 'n':` | Selects on the exact character with character case labels; no arithmetic uses the character value. The tool reports the integral promotion of the condition and labels. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.52 | `core/src/lexer/string/simple_escape.cpp:132` | `case 'r':` | Selects on the exact character with character case labels; no arithmetic uses the character value. The tool reports the integral promotion of the condition and labels. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.51 | `core/src/lexer/string/simple_escape.cpp:142` | `case 't':` | Selects on the exact character with character case labels; no arithmetic uses the character value. The tool reports the integral promotion of the condition and labels. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.50 | `core/src/lexer/string/simple_escape.cpp:152` | `case 'v':` | Selects on the exact character with character case labels; no arithmetic uses the character value. The tool reports the integral promotion of the condition and labels. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.49 | `core/src/lexer/string/simple_escape.cpp:163` | `case '0':` | Selects on the exact character with character case labels; no arithmetic uses the character value. The tool reports the integral promotion of the condition and labels. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.48 | `core/src/lexer/string/simple_escape.cpp:183` | `if (*current == c_decimal_digits_min)` | Tests whether the byte equals one specific character. An equality comparison between two character operands does not use the numeric value; the tool reports the implicit promotion of both operands to int. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII- compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.47 | `core/src/lexer/string/simple_escape.cpp:194` | `if (following.has_value() && (*following >= c_decimal_digits_min) && (*following <= c_d...` | Tests whether the byte equals one specific character. An equality comparison between two character operands does not use the numeric value; the tool reports the implicit promotion of both operands to int. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.08 | `core/src/lexer/string/string_boundary.cpp:48` | `if (*current == '"')` | Tests whether the byte equals one specific character. An equality comparison between two character operands does not use the numeric value; the tool reports the implicit promotion of both operands to int. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII- compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.07 | `core/src/lexer/string/string_boundary.cpp:58` | `else if (*current == '\'')` | Tests whether the byte equals one specific character. An equality comparison between two character operands does not use the numeric value; the tool reports the implicit promotion of both operands to int. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII- compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.39 | `core/src/lexer/string/string_escape.cpp:39` | `if (current.has_value() && (*current == 'x'))` | Tests whether the byte equals one specific character. An equality comparison between two character operands does not use the numeric value; the tool reports the implicit promotion of both operands to int. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.38 | `core/src/lexer/string/string_escape.cpp:50` | `if (current.has_value() && (*current == 'u'))` | Tests whether the byte equals one specific character. An equality comparison between two character operands does not use the numeric value; the tool reports the implicit promotion of both operands to int. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.30 | `core/src/lexer/string/string_lexer.cpp:485` | `const string_quote quote = (*source.peek() == '\'') ? string_quote::single : string_quo...` | Tests whether the byte equals one specific character. An equality comparison between two character operands does not use the numeric value; the tool reports the implicit promotion of both operands to int. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII- compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.67 | `core/src/lexer/string/unicode_escape.cpp:50` | `if (*current != c_unicode_introducer)` | Tests whether the byte equals one specific character. An equality comparison between two character operands does not use the numeric value; the tool reports the implicit promotion of both operands to int. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII- compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.71 | `core/src/lexer/token_boundary.cpp:84` | `const bool comment_opener = next.has_value() && ((*next == '/') \|\| (*next == '*'));` | Tests whether the byte equals one specific character. An equality comparison between two character operands does not use the numeric value; the tool reports the implicit promotion of both operands to int. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.70 | `core/src/lexer/trivia.cpp:128` | `if ((!first.has_value()) \|\| (!second.has_value()) \|\| (*first != c_slash))` | Tests whether the byte equals one specific character. An equality comparison between two character operands does not use the numeric value; the tool reports the implicit promotion of both operands to int. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.69 | `core/src/lexer/trivia.cpp:139` | `const bool is_line_comment = (*second == c_slash);` | Tests whether the byte equals one specific character. An equality comparison between two character operands does not use the numeric value; the tool reports the implicit promotion of both operands to int. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.68 | `core/src/lexer/trivia.cpp:146` | `const bool is_block_comment = (*second == c_asterisk);` | Tests whether the byte equals one specific character. An equality comparison between two character operands does not use the numeric value; the tool reports the implicit promotion of both operands to int. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.79 | `core/src/parser/parser_dispatch.cpp:33` | `switch (byte)` | Selects on the exact character with character case labels; no arithmetic uses the character value. The tool reports the integral promotion of the condition and labels. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.78 | `core/src/parser/parser_dispatch.cpp:41` | `case ',':` | Selects on the exact character with character case labels; no arithmetic uses the character value. The tool reports the integral promotion of the condition and labels. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.77 | `core/src/parser/parser_dispatch.cpp:49` | `case ':':` | Selects on the exact character with character case labels; no arithmetic uses the character value. The tool reports the integral promotion of the condition and labels. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.76 | `core/src/parser/parser_dispatch.cpp:57` | `case ']':` | Selects on the exact character with character case labels; no arithmetic uses the character value. The tool reports the integral promotion of the condition and labels. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.75 | `core/src/parser/parser_dispatch.cpp:64` | `case '}':` | Selects on the exact character with character case labels; no arithmetic uses the character value. The tool reports the integral promotion of the condition and labels. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.11 | `core/src/parser/parser_engine.cpp:160` | `if ((*current == '[') \|\| (*current == '{'))` | Tests whether the byte equals one specific character. An equality comparison between two character operands does not use the numeric value; the tool reports the implicit promotion of both operands to int. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.10 | `core/src/parser/parser_engine.cpp:825` | `if ((*current == '[') \|\| (*current == '{'))` | Tests whether the byte equals one specific character. An equality comparison between two character operands does not use the numeric value; the tool reports the implicit promotion of both operands to int. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.09 | `core/src/parser/parser_engine.cpp:842` | `const bool object = (*current == '{');` | Tests whether the byte equals one specific character. An equality comparison between two character operands does not use the numeric value; the tool reports the implicit promotion of both operands to int. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.81 | `core/src/unicode/utf8.cpp:83` | `const auto lead = static_cast<std::byte>(static_cast<std::uint8_t>(*first));` | Tests whether the byte equals one specific character. An equality comparison between two character operands does not use the numeric value; the tool reports the implicit promotion of both operands to int. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII- compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.80 | `core/src/unicode/utf8.cpp:132` | `const auto byte = static_cast<std::byte>(static_cast<std::uint8_t>(*next));` | Tests whether the byte equals one specific character. An equality comparison between two character operands does not use the numeric value; the tool reports the implicit promotion of both operands to int. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.06 | `core/src/value/floating_conversion.cpp:114` | `if ((digit >= '0') && (digit <= '9'))` | Classifies the byte with a range check on ASCII digits or letters; the result is only used to decide what the byte is. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.05 | `core/src/value/floating_conversion.cpp:122` | `return static_cast<std::uint32_t>(digit - '0');` | Converts an already validated digit or hexadecimal letter to its value, or a value 0-9 back to its digit. The C++ standard guarantees '0'-'9' are contiguous. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.04 | `core/src/value/floating_conversion.cpp:130` | `if ((digit >= 'a') && (digit <= 'f'))` | Classifies the byte with a range check on ASCII digits or letters; the result is only used to decide what the byte is. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.03 | `core/src/value/floating_conversion.cpp:138` | `return c_hexadecimal_letter_values[static_cast<std::size_t>(digit - 'a')];` | Converts an already validated digit or hexadecimal letter to its value, or a value 0-9 back to its digit. The C++ standard guarantees '0'-'9' are contiguous. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.02 | `core/src/value/floating_conversion.cpp:147` | `return c_hexadecimal_letter_values[static_cast<std::size_t>(digit - 'A')];` | Converts an already validated digit or hexadecimal letter to its value, or a value 0-9 back to its digit. The C++ standard guarantees '0'-'9' are contiguous. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.01 | `core/src/value/floating_conversion.cpp:253` | `const auto digit = static_cast<std::uint32_t>(current - '0');` | Converts an already validated digit or hexadecimal letter to its value, or a value 0-9 back to its digit. The C++ standard guarantees '0'-'9' are contiguous. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.18 | `core/src/value/integer_magnitude.cpp:156` | `if ((current >= '0') && (current <= '9'))` | Classifies the byte with a range check on ASCII digits or letters; the result is only used to decide what the byte is. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.17 | `core/src/value/integer_magnitude.cpp:164` | `digit = static_cast<std::uint32_t>(current - '0');` | Converts an already validated digit or hexadecimal letter to its value, or a value 0-9 back to its digit. The C++ standard guarantees '0'-'9' are contiguous. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.16 | `core/src/value/integer_magnitude.cpp:172` | `else if ((current >= 'a') && (current <= 'f'))` | Classifies the byte with a range check on ASCII digits or letters; the result is only used to decide what the byte is. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.15 | `core/src/value/integer_magnitude.cpp:180` | `digit = c_hex_letter_values[static_cast<std::size_t>(current - 'a')];` | Converts an already validated digit or hexadecimal letter to its value, or a value 0-9 back to its digit. The C++ standard guarantees '0'-'9' are contiguous. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.14 | `core/src/value/integer_magnitude.cpp:191` | `digit = c_hex_letter_values[static_cast<std::size_t>(current - 'A')];` | Converts an already validated digit or hexadecimal letter to its value, or a value 0-9 back to its digit. The C++ standard guarantees '0'-'9' are contiguous. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.13 | `core/src/value/integer_magnitude.cpp:290` | `const auto digit = static_cast<std::uint32_t>(spelling.data[position] - '0');` | Converts an already validated digit or hexadecimal letter to its value, or a value 0-9 back to its digit. The C++ standard guarantees '0'-'9' are contiguous. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.12 | `core/src/value/integer_magnitude.cpp:378` | `const auto digit = static_cast<std::uint32_t>(current - '0');` | Converts an already validated digit or hexadecimal letter to its value, or a value 0-9 back to its digit. The C++ standard guarantees '0'-'9' are contiguous. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.40 | `core/src/value/number_analysis.cpp:202` | `const auto digit = static_cast<std::size_t>(spelling.data[position] - '0');` | Converts an already validated digit or hexadecimal letter to its value, or a value 0-9 back to its digit. The C++ standard guarantees '0'-'9' are contiguous. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.37 | `core/src/value/number_comparison.cpp:77` | `if ((digit >= '0') && (digit <= '9'))` | Classifies the byte with a range check on ASCII digits or letters; the result is only used to decide what the byte is. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.36 | `core/src/value/number_comparison.cpp:85` | `return static_cast<std::uint32_t>(digit - '0');` | Converts an already validated digit or hexadecimal letter to its value, or a value 0-9 back to its digit. The C++ standard guarantees '0'-'9' are contiguous. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.35 | `core/src/value/number_comparison.cpp:93` | `if ((digit >= 'a') && (digit <= 'f'))` | Classifies the byte with a range check on ASCII digits or letters; the result is only used to decide what the byte is. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.34 | `core/src/value/number_comparison.cpp:101` | `return c_hexadecimal_letter_values[static_cast<std::size_t>(digit - 'a')];` | Converts an already validated digit or hexadecimal letter to its value, or a value 0-9 back to its digit. The C++ standard guarantees '0'-'9' are contiguous. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.33 | `core/src/value/number_comparison.cpp:110` | `return c_hexadecimal_letter_values[static_cast<std::size_t>(digit - 'A')];` | Converts an already validated digit or hexadecimal letter to its value, or a value 0-9 back to its digit. The C++ standard guarantees '0'-'9' are contiguous. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.32 | `core/src/value/number_comparison.cpp:440` | `const auto digit = static_cast<std::uint32_t>(current - '0');` | Converts an already validated digit or hexadecimal letter to its value, or a value 0-9 back to its digit. The C++ standard guarantees '0'-'9' are contiguous. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.31 | `core/src/value/number_comparison.cpp:523` | `const auto digit = static_cast<std::size_t>(spelling.data[position] - '0');` | Converts an already validated digit or hexadecimal letter to its value, or a value 0-9 back to its digit. The C++ standard guarantees '0'-'9' are contiguous. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |
| DEV-015.61 | `core/includes/lexer/number/decimal_lexer.hpp:32` | `return byte.has_value() && (*byte >= '0') && (*byte <= '9');` | Classifies the byte with a range check on ASCII digits or letters; the result is only used to decide what the byte is. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges. | Proposed |

---

## DEV-017: Rule 9.5.1, simple counting loops

| Field | Value |
| --- | --- |
| Rule | MISRA C++:2023 Rule 9.5.1 |
| CodeQL query | `cpp/misra/legacy-for-statements-should-be-simple` |
| Lines | 26 |
| Status | Proposed (#166) |

### Why this rule is deviated instead of fixed

The loops have one `std::size_t`/`std::ptrdiff_t` counter, a bound the body does not modify and one increment. The query does not resolve the typedefs as integer types.

Nothing to change in the code.

### Risk and compensating measures

Code review.

### Lines

| ID | Location | Code | Justification | Status |
| --- | --- | --- | --- | --- |
| DEV-017.16 | `core/src/lexer/identifier/identifier_lexer.cpp:185` | `for (std::size_t index{}; index < encoded.value_if()->size; ++index)` | The loop is simple: one counter declared in the init-statement, compared with a bound that the body does not modify, and changed only in the increment expression. The counter is std::size_t or std::ptrdiff_t, which are integer types; the tool does not resolve the typedef. | Proposed |
| DEV-017.15 | `core/src/lexer/scalar_lexer.cpp:117` | `for (std::size_t index{}; index < literal.text.size(); ++index)` | The loop is simple: one counter declared in the init-statement, compared with a bound that the body does not modify, and changed only in the increment expression. The counter is std::size_t or std::ptrdiff_t, which are integer types; the tool does not resolve the typedef. | Proposed |
| DEV-017.22 | `core/src/lexer/string/hex_escape.cpp:108` | `for (std::size_t index{}; index < c_hex_digit_count; ++index)` | The loop is simple: one counter declared in the init-statement, compared with a bound that the body does not modify, and changed only in the increment expression. The counter is std::size_t or std::ptrdiff_t, which are integer types; the tool does not resolve the typedef. | Proposed |
| DEV-017.19 | `core/src/lexer/string/string_lexer.cpp:138` | `for (std::size_t index{}; index < found.size; ++index)` | The loop is simple: one counter declared in the init-statement, compared with a bound that the body does not modify, and changed only in the increment expression. The counter is std::size_t or std::ptrdiff_t, which are integer types; the tool does not resolve the typedef. | Proposed |
| DEV-017.18 | `core/src/lexer/string/string_lexer.cpp:193` | `for (std::size_t index{}; index < units.size; ++index)` | The loop is simple: one counter declared in the init-statement, compared with a bound that the body does not modify, and changed only in the increment expression. The counter is std::size_t or std::ptrdiff_t, which are integer types; the tool does not resolve the typedef. | Proposed |
| DEV-017.17 | `core/src/lexer/string/string_lexer.cpp:319` | `for (std::size_t index{}; index < found.size; ++index)` | The loop is simple: one counter declared in the init-statement, compared with a bound that the body does not modify, and changed only in the increment expression. The counter is std::size_t or std::ptrdiff_t, which are integer types; the tool does not resolve the typedef. | Proposed |
| DEV-017.03 | `core/src/lexer/string/string_warning.cpp:67` | `for (std::size_t index{}; index < units.size; ++index)` | The loop is simple: one counter declared in the init-statement, compared with a bound that the body does not modify, and changed only in the increment expression. The counter is std::size_t or std::ptrdiff_t, which are integer types; the tool does not resolve the typedef. | Proposed |
| DEV-017.23 | `core/src/lexer/string/unicode_escape.cpp:65` | `for (std::size_t index{}; index < c_unicode_digit_count; ++index)` | The loop is simple: one counter declared in the init-statement, compared with a bound that the body does not modify, and changed only in the increment expression. The counter is std::size_t or std::ptrdiff_t, which are integer types; the tool does not resolve the typedef. | Proposed |
| DEV-017.11 | `core/src/parser/parser_engine.cpp:679` | `for (std::size_t index{}; index < warning_count; ++index)` | The loop is simple: one counter declared in the init-statement, compared with a bound that the body does not modify, and changed only in the increment expression. The counter is std::size_t or std::ptrdiff_t, which are integer types; the tool does not resolve the typedef. | Proposed |
| DEV-017.10 | `core/src/parser/parser_engine.cpp:919` | `for (std::size_t index{}; index < string->warning_count; ++index)` | The loop is simple: one counter declared in the init-statement, compared with a bound that the body does not modify, and changed only in the increment expression. The counter is std::size_t or std::ptrdiff_t, which are integer types; the tool does not resolve the typedef. | Proposed |
| DEV-017.09 | `core/src/parser/parser_engine.cpp:1033` | `for (std::size_t index{}; index < name.warning_count; ++index)` | The loop is simple: one counter declared in the init-statement, compared with a bound that the body does not modify, and changed only in the increment expression. The counter is std::size_t or std::ptrdiff_t, which are integer types; the tool does not resolve the typedef. | Proposed |
| DEV-017.24 | `core/src/unicode/utf8.cpp:115` | `for (std::size_t index = c_first_continuation_index; index < byte_count; ++index)` | The loop is simple: one counter declared in the init-statement, compared with a bound that the body does not modify, and changed only in the increment expression. The counter is std::size_t or std::ptrdiff_t, which are integer types; the tool does not resolve the typedef. | Proposed |
| DEV-017.08 | `core/src/value/big_unsigned.cpp:327` | `for (std::size_t index{}; index < m_size; ++index)` | The loop is simple: one counter declared in the init-statement, compared with a bound that the body does not modify, and changed only in the increment expression. The counter is std::size_t or std::ptrdiff_t, which are integer types; the tool does not resolve the typedef. | Proposed |
| DEV-017.07 | `core/src/value/big_unsigned.cpp:354` | `for (std::size_t index{}; index < m_size; ++index)` | The loop is simple: one counter declared in the init-statement, compared with a bound that the body does not modify, and changed only in the increment expression. The counter is std::size_t or std::ptrdiff_t, which are integer types; the tool does not resolve the typedef. | Proposed |
| DEV-017.06 | `core/src/value/big_unsigned.cpp:591` | `for (std::size_t index{}; index < whole_limbs; ++index)` | The loop is simple: one counter declared in the init-statement, compared with a bound that the body does not modify, and changed only in the increment expression. The counter is std::size_t or std::ptrdiff_t, which are integer types; the tool does not resolve the typedef. | Proposed |
| DEV-017.05 | `core/src/value/big_unsigned.cpp:662` | `for (std::size_t index{}; index < m_size; ++index)` | The loop is simple: one counter declared in the init-statement, compared with a bound that the body does not modify, and changed only in the increment expression. The counter is std::size_t or std::ptrdiff_t, which are integer types; the tool does not resolve the typedef. | Proposed |
| DEV-017.04 | `core/src/value/big_unsigned.cpp:771` | `for (std::size_t index{}; index < other.m_size; ++index)` | The loop is simple: one counter declared in the init-statement, compared with a bound that the body does not modify, and changed only in the increment expression. The counter is std::size_t or std::ptrdiff_t, which are integer types; the tool does not resolve the typedef. | Proposed |
| DEV-017.26 | `core/src/value/decoded_name_equal.cpp:35` | `for (std::size_t index{}; index < left.size(); ++index)` | The loop is simple: one counter declared in the init-statement, compared with a bound that the body does not modify, and changed only in the increment expression. The counter is std::size_t or std::ptrdiff_t, which are integer types; the tool does not resolve the typedef. | Proposed |
| DEV-017.02 | `core/src/value/floating_conversion.cpp:900` | `for (std::size_t chunk{}; chunk < full_chunks; ++chunk)` | The loop is simple: one counter declared in the init-statement, compared with a bound that the body does not modify, and changed only in the increment expression. The counter is std::size_t or std::ptrdiff_t, which are integer types; the tool does not resolve the typedef. | Proposed |
| DEV-017.01 | `core/src/value/floating_conversion.cpp:1007` | `for (std::ptrdiff_t bit = c_quotient_bits; bit >= 0; --bit)` | The loop is simple: one counter declared in the init-statement, compared with a bound that the body does not modify, and changed only in the increment expression. The counter is std::size_t or std::ptrdiff_t, which are integer types; the tool does not resolve the typedef. | Proposed |
| DEV-017.20 | `core/src/value/number_comparison.cpp:174` | `for (std::size_t index{}; index < digits; ++index)` | The loop is simple: one counter declared in the init-statement, compared with a bound that the body does not modify, and changed only in the increment expression. The counter is std::size_t or std::ptrdiff_t, which are integer types; the tool does not resolve the typedef. | Proposed |
| DEV-017.25 | `core/src/value/utf8_name_equal.cpp:59` | `for (std::size_t index{}; index < encoded.value_if()->size; ++index)` | The loop is simple: one counter declared in the init-statement, compared with a bound that the body does not modify, and changed only in the increment expression. The counter is std::size_t or std::ptrdiff_t, which are integer types; the tool does not resolve the typedef. | Proposed |
| DEV-017.21 | `core/src/value/value_storage.cpp:183` | `for (std::size_t index{}; index < m_size; ++index)` | The loop is simple: one counter declared in the init-statement, compared with a bound that the body does not modify, and changed only in the increment expression. The counter is std::size_t or std::ptrdiff_t, which are integer types; the tool does not resolve the typedef. | Proposed |
| DEV-017.13 | `core/src/value/value_view.cpp:134` | `for (std::size_t ordinal{}; ordinal <= position; ++ordinal)` | The loop is simple: one counter declared in the init-statement, compared with a bound that the body does not modify, and changed only in the increment expression. The counter is std::size_t or std::ptrdiff_t, which are integer types; the tool does not resolve the typedef. | Proposed |
| DEV-017.12 | `core/src/value/value_view.cpp:462` | `for (std::size_t ordinal{}; ordinal < data->members.count; ++ordinal)` | The loop is simple: one counter declared in the init-statement, compared with a bound that the body does not modify, and changed only in the increment expression. The counter is std::size_t or std::ptrdiff_t, which are integer types; the tool does not resolve the typedef. | Proposed |
| DEV-017.14 | `core/src/value/warning_list.cpp:210` | `for (std::size_t index{}; index < m_size; ++index)` | The loop is simple: one counter declared in the init-statement, compared with a bound that the body does not modify, and changed only in the increment expression. The counter is std::size_t or std::ptrdiff_t, which are integer types; the tool does not resolve the typedef. | Proposed |

---

## DEV-018: Rule 10.1.1, writes through a block

| Field | Value |
| --- | --- |
| Rule | MISRA C++:2023 Rule 10.1.1 |
| CodeQL query | `cpp/misra/pointer-or-ref-param-not-const` |
| Lines | 2 |
| Status | Proposed (#166) |

### Why this rule is deviated instead of fixed

The functions write into the bytes owned by the block; a non-const reference documents that.

A const reference would hide the write.

### Risk and compensating measures

Unit tests.

### Lines

| ID | Location | Code | Justification | Status |
| --- | --- | --- | --- | --- |
| DEV-018.01 | `core/src/value/big_unsigned.cpp:84` | `void write_limb(owned_block& storage, std::size_t index, limb_type value) noexcept` | The function writes into the bytes owned by the block. owned_block::data() is const- qualified, but a non-const reference documents that the call modifies the block's contents. | Proposed |
| DEV-018.02 | `core/src/value/warning_list.cpp:74` | `void write_entry(owned_block& storage, std::size_t index, const string_warning& entry) ...` | The function writes into the bytes owned by the block. owned_block::data() is const- qualified, but a non-const reference documents that the call modifies the block's contents. | Proposed |

---

## DEV-019: Rule 10.2.3, `std::float_denorm_style` comparisons

| Field | Value |
| --- | --- |
| Rule | MISRA C++:2023 Rule 10.2.3 |
| CodeQL query | `cpp/misra/unscoped-enum-without-fixed-underlying-type-used` |
| Lines | 2 |
| Status | Proposed (#166) |

### Why this rule is deviated instead of fixed

Both operands have the standard library's enum type `std::float_denorm_style`.

The type is defined by the standard library.

### Risk and compensating measures

Code review.

### Lines

| ID | Location | Code | Justification | Status |
| --- | --- | --- | --- | --- |
| DEV-019.02 | `core/includes/value/floating_format.hpp:99` | `(std::numeric_limits<T>::has_denorm == std::denorm_present);` | Both operands have the standard library's own enum type std::float_denorm_style (has_denorm and denorm_present), so no conversion to a different type takes place. | Proposed |
| DEV-019.01 | `core/includes/value/floating_format.hpp:131` | `(std::numeric_limits<T>::has_denorm == std::denorm_present);` | Both operands have the standard library's own enum type std::float_denorm_style (has_denorm and denorm_present), so no conversion to a different type takes place. | Proposed |

---

## DEV-020: Rule 15.0.1, special member functions

| Field | Value |
| --- | --- |
| Rule | MISRA C++:2023 Rule 15.0.1 |
| CodeQL query | `cpp/misra/improperly-provided-special-member-functions(-audit)` |
| Lines | 58 |
| Status | Proposed (#166) |

### Why this rule is deviated instead of fixed

Classes follow the rule of zero, or declare all special members explicitly. The audit query cannot analyse implicit members that are never used.

Declaring every member of 55 aggregates would add noise and change aggregate behaviour.

### Risk and compensating measures

Code review.

### Lines

| ID | Location | Code | Rules | Justification | Status |
| --- | --- | --- | --- | --- | --- |
| DEV-020.26 | `core/src/diagnostics/diagnostic_format.cpp:37` | `class message_writer` | 15.0.1 | Audit result only: the class declares no special members of the listed kind, so the compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because implicit members that are never odr-used are not in its database. | Proposed |
| DEV-020.30 | `core/src/lexer/identifier/identifier_lexer.cpp:36` | `struct identifier_walk` | 15.0.1 | Audit result only: the class declares no special members of the listed kind, so the compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because implicit members that are never odr-used are not in its database. | Proposed |
| DEV-020.29 | `core/src/lexer/scalar_lexer.cpp:43` | `struct literal_spelling` | 15.0.1 | Audit result only: the class declares no special members of the listed kind, so the compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because implicit members that are never odr-used are not in its database. | Proposed |
| DEV-020.33 | `core/src/lexer/string/string_lexer.cpp:49` | `struct string_walk` | 15.0.1 | Audit result only: the class declares no special members of the listed kind, so the compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because implicit members that are never odr-used are not in its database. | Proposed |
| DEV-020.34 | `core/src/memory/buffer_document_storage.cpp:30` | `struct buffer_document_storage` | 15.0.1 | Audit result only: the class declares no special members of the listed kind, so the compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because implicit members that are never odr-used are not in its database. | Proposed |
| DEV-020.58 | `core/src/value/number_comparison.cpp:55` | `struct [[nodiscard]] decimal_form` | 15.0.1 | Audit result only: the class declares no special members of the listed kind, so the compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because implicit members that are never odr-used are not in its database. | Proposed |
| DEV-020.08 | `core/includes/input/cursor.hpp:23` | `class [[nodiscard]] cursor` | 15.0.1 | Audit result only: the class declares no special members of the listed kind, so the compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because implicit members that are never odr-used are not in its database. | Proposed |
| DEV-020.21 | `core/includes/lexer/identifier/identifier_token.hpp:26` | `struct [[nodiscard]] identifier_token` | 15.0.1 | Audit result only: the class declares no special members of the listed kind, so the compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because implicit members that are never odr-used are not in its database. | Proposed |
| DEV-020.20 | `core/includes/lexer/identifier/identifier_token.hpp:48` | `struct [[nodiscard]] identifier_measure` | 15.0.1 | Audit result only: the class declares no special members of the listed kind, so the compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because implicit members that are never odr-used are not in its database. | Proposed |
| DEV-020.03 | `core/includes/lexer/member_name_lexer.hpp:34` | `struct [[nodiscard]] member_name_token` | 15.0.1 | Audit result only: the class declares no special members of the listed kind, so the compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because implicit members that are never odr-used are not in its database. | Proposed |
| DEV-020.02 | `core/includes/lexer/member_name_lexer.hpp:57` | `struct [[nodiscard]] member_name_measure` | 15.0.1 | Audit result only: the class declares no special members of the listed kind, so the compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because implicit members that are never odr-used are not in its database. | Proposed |
| DEV-020.17 | `core/includes/lexer/number/number_token.hpp:26` | `struct [[nodiscard]] number_token` | 15.0.1 | Audit result only: the class declares no special members of the listed kind, so the compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because implicit members that are never odr-used are not in its database. | Proposed |
| DEV-020.01 | `core/includes/lexer/scalar_token.hpp:37` | `struct [[nodiscard]] literal_token` | 15.0.1 | Audit result only: the class declares no special members of the listed kind, so the compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because implicit members that are never odr-used are not in its database. | Proposed |
| DEV-020.27 | `core/includes/lexer/string/string_checks.hpp:21` | `struct string_checks` | 15.0.1 | Audit result only: the class declares no special members of the listed kind, so the compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because implicit members that are never odr-used are not in its database. | Proposed |
| DEV-020.25 | `core/includes/lexer/string/string_token.hpp:40` | `struct [[nodiscard]] string_warning_observer` | 15.0.1 | Audit result only: the class declares no special members of the listed kind, so the compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because implicit members that are never odr-used are not in its database. | Proposed |
| DEV-020.24 | `core/includes/lexer/string/string_token.hpp:74` | `struct [[nodiscard]] string_warning_sink` | 15.0.1 | Audit result only: the class declares no special members of the listed kind, so the compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because implicit members that are never odr-used are not in its database. | Proposed |
| DEV-020.23 | `core/includes/lexer/string/string_token.hpp:103` | `struct [[nodiscard]] string_token` | 15.0.1 | Audit result only: the class declares no special members of the listed kind, so the compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because implicit members that are never odr-used are not in its database. | Proposed |
| DEV-020.22 | `core/includes/lexer/string/string_token.hpp:128` | `struct [[nodiscard]] string_measure` | 15.0.1 | Audit result only: the class declares no special members of the listed kind, so the compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because implicit members that are never odr-used are not in its database. | Proposed |
| DEV-020.32 | `core/includes/lexer/string/string_warning.hpp:43` | `struct [[nodiscard]] surrogate_warnings` | 15.0.1 | Audit result only: the class declares no special members of the listed kind, so the compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because implicit members that are never odr-used are not in its database. | Proposed |
| DEV-020.31 | `core/includes/lexer/string/string_warning.hpp:66` | `class surrogate_pairing` | 15.0.1 | Audit result only: the class declares no special members of the listed kind, so the compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because implicit members that are never odr-used are not in its database. | Proposed |
| DEV-020.38 | `core/includes/memory/resource_budget.hpp:28` | `class [[nodiscard]] resource_budget` | 15.0.1 | Audit result only: the class declares no special members of the listed kind, so the compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because implicit members that are never odr-used are not in its database. | Proposed |
| DEV-020.37 | `core/includes/memory/work_budget.hpp:27` | `class [[nodiscard]] work_budget` | 15.0.1 | Audit result only: the class declares no special members of the listed kind, so the compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because implicit members that are never odr-used are not in its database. | Proposed |
| DEV-020.44 | `core/includes/parser/parser_engine.hpp:45` | `struct [[nodiscard]] parsed_value_tree` | 15.0.1 | Audit result only: the class declares no special members of the listed kind, so the compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because implicit members that are never odr-used are not in its database. | Proposed |
| DEV-020.43 | `core/includes/parser/parser_engine.hpp:68` | `class [[nodiscard]] parser_engine` | 15.0.1 | Audit result only: the class declares no special members of the listed kind, so the compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because implicit members that are never odr-used are not in its database. | Proposed |
| DEV-020.41 | `core/includes/parser/parser_frame.hpp:48` | `struct [[nodiscard]] parser_frame` | 15.0.1 | Audit result only: the class declares no special members of the listed kind, so the compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because implicit members that are never odr-used are not in its database. | Proposed |
| DEV-020.42 | `core/includes/parser/parser_work.hpp:59` | `struct [[nodiscard]] limited_scan` | 15.0.1 | Audit result only: the class declares no special members of the listed kind, so the compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because implicit members that are never odr-used are not in its database. | Proposed |
| DEV-020.48 | `core/includes/parser/primitive_parser.hpp:35` | `struct [[nodiscard]] primitive_value` | 15.0.1 | Audit result only: the class declares no special members of the listed kind, so the compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because implicit members that are never odr-used are not in its database. | Proposed |
| DEV-020.47 | `core/includes/parser/primitive_parser.hpp:58` | `struct [[nodiscard]] primitive_document` | 15.0.1 | Audit result only: the class declares no special members of the listed kind, so the compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because implicit members that are never odr-used are not in its database. | Proposed |
| DEV-020.28 | `core/includes/unicode/utf16.hpp:24` | `struct [[nodiscard]] encoded_utf16` | 15.0.1 | Audit result only: the class declares no special members of the listed kind, so the compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because implicit members that are never odr-used are not in its database. | Proposed |
| DEV-020.46 | `core/includes/unicode/utf16_decode.hpp:23` | `struct [[nodiscard]] decoded_utf16` | 15.0.1 | Audit result only: the class declares no special members of the listed kind, so the compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because implicit members that are never odr-used are not in its database. | Proposed |
| DEV-020.55 | `core/includes/value/container_links.hpp:28` | `struct [[nodiscard]] container_links` | 15.0.1 | Audit result only: the class declares no special members of the listed kind, so the compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because implicit members that are never odr-used are not in its database. | Proposed |
| DEV-020.54 | `core/includes/value/container_links.hpp:47` | `struct [[nodiscard]] array_data` | 15.0.1 | Audit result only: the class declares no special members of the listed kind, so the compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because implicit members that are never odr-used are not in its database. | Proposed |
| DEV-020.53 | `core/includes/value/container_links.hpp:70` | `struct [[nodiscard]] object_data` | 15.0.1 | Audit result only: the class declares no special members of the listed kind, so the compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because implicit members that are never odr-used are not in its database. | Proposed |
| DEV-020.51 | `core/includes/value/integer_magnitude.hpp:24` | `struct [[nodiscard]] integer_magnitude` | 15.0.1 | Audit result only: the class declares no special members of the listed kind, so the compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because implicit members that are never odr-used are not in its database. | Proposed |
| DEV-020.50 | `core/includes/value/number_analysis.hpp:32` | `struct [[nodiscard]] number_analysis` | 15.0.1 | Audit result only: the class declares no special members of the listed kind, so the compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because implicit members that are never odr-used are not in its database. | Proposed |
| DEV-020.52 | `core/includes/value/value_record.hpp:49` | `struct [[nodiscard]] value_record` | 15.0.1 | Audit result only: the class declares no special members of the listed kind, so the compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because implicit members that are never odr-used are not in its database. | Proposed |
| DEV-020.10 | `includes/adaptive_json5/error.hpp:49` | `struct [[nodiscard]] duplicate_names_locations` | 15.0.1 | Audit result only: the class declares no special members of the listed kind, so the compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because implicit members that are never odr-used are not in its database. | Proposed |
| DEV-020.09 | `includes/adaptive_json5/error.hpp:88` | `struct [[nodiscard]] error` | 15.0.1 | Audit result only: the class declares no special members of the listed kind, so the compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because implicit members that are never odr-used are not in its database. | Proposed |
| DEV-020.14 | `includes/adaptive_json5/input.hpp:23` | `struct [[nodiscard]] input_view` | 15.0.1 | Audit result only: the class declares no special members of the listed kind, so the compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because implicit members that are never odr-used are not in its database. | Proposed |
| DEV-020.13 | `includes/adaptive_json5/input.hpp:44` | `struct [[nodiscard]] source_span` | 15.0.1 | Audit result only: the class declares no special members of the listed kind, so the compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because implicit members that are never odr-used are not in its database. | Proposed |
| DEV-020.39 | `includes/adaptive_json5/json5_value.hpp:47` | `class [[nodiscard]] json5_value` | 15.0.1 | Audit result only: the class declares no special members of the listed kind, so the compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because implicit members that are never odr-used are not in its database. | Proposed |
| DEV-020.15 | `includes/adaptive_json5/memory/resource_limits.hpp:23` | `struct [[nodiscard]] resource_limits` | 15.0.1 | Audit result only: the class declares no special members of the listed kind, so the compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because implicit members that are never odr-used are not in its database. | Proposed |
| DEV-020.18 | `includes/adaptive_json5/memory/storage.hpp:48` | `struct [[nodiscard]] storage_callbacks` | 15.0.1 | Audit result only: the class declares no special members of the listed kind, so the compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because implicit members that are never odr-used are not in its database. | Proposed |
| DEV-020.16 | `includes/adaptive_json5/options.hpp:194` | `struct [[nodiscard]] parse_options` | 15.0.1 | Audit result only: the class declares no special members of the listed kind, so the compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because implicit members that are never odr-used are not in its database. | Proposed |
| DEV-020.35 | `includes/adaptive_json5/parser.hpp:29` | `struct [[nodiscard]] parsed_document` | 15.0.1 | Audit result only: the class declares no special members of the listed kind, so the compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because implicit members that are never odr-used are not in its database. | Proposed |
| DEV-020.12 | `includes/adaptive_json5/result.hpp:30` | `class [[nodiscard]] result` | 15.0.1, 4.1.2 | Audit result only: the class declares no special members of the listed kind, so the compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because implicit members that are never odr-used are not in its database. The copy constructor and copy assignment are explicitly declared in the class (= default or = delete), so nothing relies on a deprecated implicit declaration. | Proposed |
| DEV-020.11 | `includes/adaptive_json5/result.hpp:224` | `class [[nodiscard]] result<void, E>` | 15.0.1 | Audit result only: the class declares no special members of the listed kind, so the compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because implicit members that are never odr-used are not in its database. | Proposed |
| DEV-020.19 | `includes/adaptive_json5/text_position.hpp:23` | `struct [[nodiscard]] text_position` | 15.0.1 | Audit result only: the class declares no special members of the listed kind, so the compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because implicit members that are never odr-used are not in its database. | Proposed |
| DEV-020.05 | `includes/adaptive_json5/value/decoded_string.hpp:32` | `class [[nodiscard]] decoded_string` | 15.0.1 | The destructor is explicitly declared (~T() noexcept = default) next to the custom move constructor, and copy and move assignment are explicitly deleted, so all special members are declared. | Proposed |
| DEV-020.06 | `includes/adaptive_json5/value/document.hpp:267` | `class [[nodiscard]] document` | 15.0.1 | The destructor is explicitly declared (~T() noexcept = default) next to the custom move constructor, and copy and move assignment are explicitly deleted, so all special members are declared. | Proposed |
| DEV-020.49 | `includes/adaptive_json5/value/floating_conversion.hpp:34` | `struct [[nodiscard]] floating_conversion_options` | 15.0.1 | Audit result only: the class declares no special members of the listed kind, so the compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because implicit members that are never odr-used are not in its database. | Proposed |
| DEV-020.04 | `includes/adaptive_json5/value/member_lookup.hpp:35` | `struct [[nodiscard]] member_lookup` | 15.0.1 | Audit result only: the class declares no special members of the listed kind, so the compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because implicit members that are never odr-used are not in its database. | Proposed |
| DEV-020.57 | `includes/adaptive_json5/value/number_comparison.hpp:47` | `struct [[nodiscard]] number_comparison_options` | 15.0.1 | Audit result only: the class declares no special members of the listed kind, so the compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because implicit members that are never odr-used are not in its database. | Proposed |
| DEV-020.56 | `includes/adaptive_json5/value/numeric_conversion.hpp:27` | `struct [[nodiscard]] numeric_conversion_options` | 15.0.1 | Audit result only: the class declares no special members of the listed kind, so the compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because implicit members that are never odr-used are not in its database. | Proposed |
| DEV-020.40 | `includes/adaptive_json5/value/value_index.hpp:28` | `class [[nodiscard]] value_index` | 15.0.1 | Audit result only: the class declares no special members of the listed kind, so the compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because implicit members that are never odr-used are not in its database. | Proposed |
| DEV-020.45 | `includes/adaptive_json5/value/value_iterator.hpp:34` | `class [[nodiscard]] value_iterator` | 15.0.1 | Audit result only: the class declares no special members of the listed kind, so the compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because implicit members that are never odr-used are not in its database. | Proposed |
| DEV-020.36 | `includes/adaptive_json5/value/value_view.hpp:39` | `class [[nodiscard]] value_view` | 15.0.1 | Audit result only: the class declares no special members of the listed kind, so the compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because implicit members that are never odr-used are not in its database. | Proposed |
| DEV-020.07 | `includes/adaptive_json5/value/warning_list.hpp:28` | `class [[nodiscard]] warning_list` | 15.0.1 | The destructor is explicitly declared (~T() noexcept = default) next to the custom move constructor, and copy and move assignment are explicitly deleted, so all special members are declared. | Proposed |
| DEV-020.59 | `core/src/diagnostics/diagnostic_format.cpp:220` | `struct description` | Audit result only: the class declares no special members of the listed kind, so the compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because implicit members that are never odr-used are not in its database. | Proposed |
| DEV-020.61 | `core/src/value/number_analysis.cpp:26` | `struct decimal_exponent` | Audit result only: the class declares no special members of the listed kind, so the compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because implicit members that are never odr-used are not in its database. | Proposed |
| DEV-020.60 | `core/src/lexer/string/string_lexer.cpp:62` | `struct string_body_state` | Audit result only: the class declares no special members of the listed kind, so the compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because implicit members that are never odr-used are not in its database. | Proposed |

---

## DEV-021: Rule 15.0.2, noexcept move operations

| Field | Value |
| --- | --- |
| Rule | MISRA C++:2023 Rule 15.0.2 |
| CodeQL query | `cpp/misra/invalid-signature-for-special-member-function` |
| Lines | 7 |
| Status | Proposed (#166) |

### Why this rule is deviated instead of fixed

The move operations are declared `noexcept`; the query does not see the specifier in the `-fno-exceptions` analysis build.

Nothing to change in the code.

### Risk and compensating measures

`static_assert(std::is_nothrow_move_constructible_v<...>)` and unit tests.

### Lines

| ID | Location | Code | Rules | Justification | Status |
| --- | --- | --- | --- | --- | --- |
| DEV-021.05 | `core/src/memory/document_storage_owner.cpp:46` | `document_storage_owner::document_storage_owner(document_storage_owner&& other) noexcept` | 15.0.2, 18.4.1 | The move operation is declared noexcept in both its declaration and its definition. The tool does not see the specifier in the -fno-exceptions analysis build. | Proposed |
| DEV-021.03 | `core/src/memory/owned_block.cpp:38` | `owned_block::owned_block(owned_block&& other) noexcept` | 15.0.2, 18.4.1 | The move operation is declared noexcept in both its declaration and its definition. The tool does not see the specifier in the -fno-exceptions analysis build. | Proposed |
| DEV-021.02 | `core/src/memory/owned_block.cpp:63` | `owned_block& owned_block::operator=(owned_block&& other) & noexcept` | 15.0.2, 18.4.1 | The move operation is declared noexcept in both its declaration and its definition. The tool does not see the specifier in the -fno-exceptions analysis build. | Proposed |
| DEV-021.04 | `core/src/value/decoded_string.cpp:41` | `decoded_string::decoded_string(decoded_string&& other) noexcept` | 15.0.2, 18.4.1 | The move operation is declared noexcept in both its declaration and its definition. The tool does not see the specifier in the -fno-exceptions analysis build. | Proposed |
| DEV-021.06 | `core/src/value/document.cpp:66` | `document::document(document&& other) noexcept : m_values(std::move(other.m_values)), m_...` | 15.0.2, 18.4.1 | The move operation is declared noexcept in both its declaration and its definition. The tool does not see the specifier in the -fno-exceptions analysis build. | Proposed |
| DEV-021.07 | `core/src/value/value_storage.cpp:236` | `value_storage::value_storage(value_storage&& other) noexcept` | 15.0.2, 18.4.1 | The move operation is declared noexcept in both its declaration and its definition. The tool does not see the specifier in the -fno-exceptions analysis build. | Proposed |
| DEV-021.01 | `core/src/value/warning_list.cpp:159` | `warning_list::warning_list(warning_list&& other) noexcept` | 15.0.2, 18.4.1 | The move operation is declared noexcept in both its declaration and its definition. The tool does not see the specifier in the -fno-exceptions analysis build. | Proposed |

---

## DEV-022: Rule 18.4.1, noexcept move operations

| Field | Value |
| --- | --- |
| Rule | MISRA C++:2023 Rule 18.4.1 |
| CodeQL query | `cpp/misra/exception-unfriendly-function-must-be-noexcept` |
| Lines | 6 |
| Status | Proposed (#166) |

### Why this rule is deviated instead of fixed

Same as DEV-021.

Nothing to change in the code.

### Risk and compensating measures

As DEV-021.

### Lines

| ID | Location | Code | Justification | Status |
| --- | --- | --- | --- | --- |
| DEV-022.04 | `core/src/memory/json5_document.cpp:185` | `json5_document::json5_document(json5_document&&) noexcept = default;` | The move operation is declared noexcept in both its declaration and its definition. The tool does not see the specifier in the -fno-exceptions analysis build. | Proposed |
| DEV-022.06 | `core/includes/value/record_link.hpp:57` | `record_link(record_link&&) noexcept = default;` | The move operation is declared noexcept in both its declaration and its definition. The tool does not see the specifier in the -fno-exceptions analysis build. | Proposed |
| DEV-022.05 | `core/includes/value/record_link.hpp:62` | `record_link& operator=(record_link&&) noexcept = default;` | The move operation is declared noexcept in both its declaration and its definition. The tool does not see the specifier in the -fno-exceptions analysis build. | Proposed |
| DEV-022.01 | `includes/adaptive_json5/result.hpp:138` | `result(result&&) noexcept = default;` | The move operation is declared noexcept in both its declaration and its definition. The tool does not see the specifier in the -fno-exceptions analysis build. | Proposed |
| DEV-022.02 | `includes/adaptive_json5/value/owned_number.hpp:61` | `owned_number(owned_number&& other) noexcept = default;` | The move operation is declared noexcept in both its declaration and its definition. The tool does not see the specifier in the -fno-exceptions analysis build. | Proposed |
| DEV-022.03 | `includes/adaptive_json5/value/retained_source.hpp:44` | `retained_source(retained_source&&) noexcept = default;` | The move operation is declared noexcept in both its declaration and its definition. The tool does not see the specifier in the -fno-exceptions analysis build. | Proposed |

---

## DEV-023: Rule 24.5.2, `std::memcpy` for object representations

| Field | Value |
| --- | --- |
| Rule | MISRA C++:2023 Rule 24.5.2 |
| CodeQL query | `cpp/misra/no-memory-functions-from-c-string` |
| Lines | 7 |
| Status | Proposed (#166) |

### Why this rule is deviated instead of fixed

Copies bytes between non-overlapping, size-checked buffers. In C++17 this is the defined way to copy trivially copyable object representations to and from untyped storage (`std::bit_cast` is C++20).

No C++17 alternative without aliasing or alignment assumptions.

### Risk and compensating measures

Unit tests, ASan and fuzzing.

### Lines

| ID | Location | Code | Rules | Justification | Status |
| --- | --- | --- | --- | --- | --- |
| DEV-023.02 | `core/src/value/big_unsigned.cpp:65` | `static_cast<void>(std::memcpy(&value, bytes + offset, sizeof(value)));` | 24.5.2 | std::memcpy copies bytes between non-overlapping buffers whose sizes were checked when the storage was allocated. In C++17 this is the defined way to copy a trivially copyable object representation to or from untyped storage without alignment or aliasing assumptions. | Proposed |
| DEV-023.01 | `core/src/value/big_unsigned.cpp:102` | `static_cast<void>(std::memcpy(bytes + offset, &value, sizeof(value)));` | 24.5.2 | std::memcpy copies bytes between non-overlapping buffers whose sizes were checked when the storage was allocated. In C++17 this is the defined way to copy a trivially copyable object representation to or from untyped storage without alignment or aliasing assumptions. | Proposed |
| DEV-023.05 | `core/src/value/number_access.cpp:114` | `static_cast<void>(std::memcpy(allocation.value_if()->data(), text.data, text.size));` | 24.5.2 | std::memcpy copies bytes between non-overlapping buffers whose sizes were checked when the storage was allocated. In C++17 this is the defined way to copy a trivially copyable object representation to or from untyped storage without alignment or aliasing assumptions. | Proposed |
| DEV-023.07 | `core/src/value/owned_number.cpp:96` | `static_cast<void>(std::memcpy(allocation.value_if()->data(), text.data, text.size));` | 24.5.2 | std::memcpy copies bytes between non-overlapping buffers whose sizes were checked when the storage was allocated. In C++17 this is the defined way to copy a trivially copyable object representation to or from untyped storage without alignment or aliasing assumptions. | Proposed |
| DEV-023.06 | `core/src/value/retained_source.cpp:50` | `static_cast<void>(std::memcpy(allocation.value_if()->data(), input.data, input.size));` | 24.5.2 | std::memcpy copies bytes between non-overlapping buffers whose sizes were checked when the storage was allocated. In C++17 this is the defined way to copy a trivially copyable object representation to or from untyped storage without alignment or aliasing assumptions. | Proposed |
| DEV-023.04 | `core/src/value/warning_list.cpp:56` | `static_cast<void>(std::memcpy(&entry, bytes + offset, sizeof(entry)));` | 24.5.2 | std::memcpy copies bytes between non-overlapping buffers whose sizes were checked when the storage was allocated. In C++17 this is the defined way to copy a trivially copyable object representation to or from untyped storage without alignment or aliasing assumptions. | Proposed |
| DEV-023.03 | `core/src/value/warning_list.cpp:95` | `static_cast<void>(std::memcpy(bytes + offset, &entry, sizeof(entry)));` | 24.5.2, 8.7.1 | std::memcpy copies bytes between non-overlapping buffers whose sizes were checked when the storage was allocated. In C++17 this is the defined way to copy a trivially copyable object representation to or from untyped storage without alignment or aliasing assumptions. entry is a reference to a string_warning, so &entry points to a whole string_warning and sizeof(entry) is the size of that object. The tool takes the size of a pointer instead. | Proposed |

---

## DEV-024: Rule 28.6.3, single moves out of results

| Field | Value |
| --- | --- |
| Rule | MISRA C++:2023 Rule 28.6.3 |
| CodeQL query | `cpp/misra/object-used-while-in-potentially-moved-from-state` |
| Lines | 18 |
| Status | Proposed (#166) |

### Why this rule is deviated instead of fixed

The value inside a result or frame is moved exactly once and not read afterwards; the query reports the operand expressions of the same move.

Nothing to change in the code.

### Risk and compensating measures

Unit tests and ASan.

### Lines

| ID | Location | Code | Justification | Status |
| --- | --- | --- | --- | --- |
| DEV-024.03 | `core/src/lexer/scalar_lexer.cpp:296` | `scalar_token token{std::in_place_type<string_token>, std::move(**string.value_if())};` | The object is moved exactly once in this expression and is not read again afterwards; the reported accesses are the operand expressions that form this same move (for example value_if() on a result that still holds its value). | Proposed |
| DEV-024.13 | `core/src/memory/decoded_string_builder.cpp:117` | `m_storage = std::move(*allocation.value_if());` | The object is moved exactly once in this expression and is not read again afterwards; the reported accesses are the operand expressions that form this same move (for example value_if() on a result that still holds its value). | Proposed |
| DEV-024.09 | `core/src/memory/json5_document.cpp:134` | `return detail::json5_document_access::parse_with_storage(input, options, std::move(*sto...` | The object is moved exactly once in this expression and is not read again afterwards; the reported accesses are the operand expressions that form this same move (for example value_if() on a result that still holds its value). | Proposed |
| DEV-024.08 | `core/src/memory/json5_document.cpp:341` | `return document_result::success(json5_document{std::move(storage), std::move(*parsed.va...` | The object is moved exactly once in this expression and is not read again afterwards; the reported accesses are the operand expressions that form this same move (for example value_if() on a result that still holds its value). | Proposed |
| DEV-024.15 | `core/src/parser/parser.cpp:65` | `parsed_document{std::move(tree.values), std::move(tree.warnings), std::move(*retained.v...` | The object is moved exactly once in this expression and is not read again afterwards; the reported accesses are the operand expressions that form this same move (for example value_if() on a result that still holds its value). | Proposed |
| DEV-024.14 | `core/src/parser/parser_attachment.cpp:50` | `return document_access::attach_parsed_member(destination, frame.container, std::move(*frame...` | The pending name is moved exactly once into the new member and is reset after a successful attach; on failure the parse is abandoned, so the moved-from name is never read. | Proposed |
| DEV-024.02 | `core/src/parser/parser_engine.cpp:1259` | `m_extra_kinds = std::move(*allocation.value_if());` | The object is moved exactly once in this expression and is not read again afterwards; the reported accesses are the operand expressions that form this same move (for example value_if() on a result that still holds its value). | Proposed |
| DEV-024.16 | `core/src/parser/parser_stack.cpp:118` | `m_storage = std::move(*allocation.value_if());` | The object is moved exactly once in this expression and is not read again afterwards; the reported accesses are the operand expressions that form this same move (for example value_if() on a result that still holds its value). | Proposed |
| DEV-024.18 | `core/src/parser/primitive_parser.cpp:108` | `return detail::document_access::make_number(destination, std::move(*owned.value_if()));` | The object is moved exactly once in this expression and is not read again afterwards; the reported accesses are the operand expressions that form this same move (for example value_if() on a result that still holds its value). | Proposed |
| DEV-024.17 | `core/src/parser/primitive_parser.cpp:268` | `detail::append_primitive_token(destination, std::move(**token.value_if()), input, optio...` | The object is moved exactly once in this expression and is not read again afterwards; the reported accesses are the operand expressions that form this same move (for example value_if() on a result that still holds its value). | Proposed |
| DEV-024.01 | `core/src/value/big_unsigned.cpp:202` | `m_storage = std::move(*allocation.value_if());` | The object is moved exactly once in this expression and is not read again afterwards; the reported accesses are the operand expressions that form this same move (for example value_if() on a result that still holds its value). | Proposed |
| DEV-024.10 | `core/src/value/number_access.cpp:121` | `return number_result::success(owned_number{std::move(*allocation.value_if()), kind});` | The object is moved exactly once in this expression and is not read again afterwards; the reported accesses are the operand expressions that form this same move (for example value_if() on a result that still holds its value). | Proposed |
| DEV-024.12 | `core/src/value/owned_number.cpp:104` | `return number_result::success(owned_number{std::move(*allocation.value_if()), kind});` | The object is moved exactly once in this expression and is not read again afterwards; the reported accesses are the operand expressions that form this same move (for example value_if() on a result that still holds its value). | Proposed |
| DEV-024.11 | `core/src/value/retained_source.cpp:56` | `retained.m_bytes = std::move(*allocation.value_if());` | The object is moved exactly once in this expression and is not read again afterwards; the reported accesses are the operand expressions that form this same move (for example value_if() on a result that still holds its value). | Proposed |
| DEV-024.07 | `core/src/value/value_storage.cpp:164` | `pending_identity = std::move(*token.value_if());` | The object is moved exactly once in this expression and is not read again afterwards; the reported accesses are the operand expressions that form this same move (for example value_if() on a result that still holds its value). | Proposed |
| DEV-024.06 | `core/src/value/value_storage.cpp:205` | `m_storage = std::move(*allocation.value_if());` | The object is moved exactly once in this expression and is not read again afterwards; the reported accesses are the operand expressions that form this same move (for example value_if() on a result that still holds its value). | Proposed |
| DEV-024.05 | `core/src/value/value_storage.cpp:291` | `m_identity = std::move(*token.value_if());` | The object is moved exactly once in this expression and is not read again afterwards; the reported accesses are the operand expressions that form this same move (for example value_if() on a result that still holds its value). | Proposed |
| DEV-024.04 | `core/src/value/value_storage.cpp:297` | `m_storage = std::move(*allocation.value_if());` | The object is moved exactly once in this expression and is not read again afterwards; the reported accesses are the operand expressions that form this same move (for example value_if() on a result that still holds its value). | Proposed |
