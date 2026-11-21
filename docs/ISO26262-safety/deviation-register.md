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
| DEV-003.02 | `core/src/memory/pointer_alignment.cpp:35`, `align_within()` | `reinterpret_cast<void*>(address + padding)` | 8.2.5, 8.2.6 | **Fix proposed instead:** step `position` forward by `padding` bytes through `std::byte*`, or use `std::align`. That also removes the integer-to-pointer conversion. Keep as a deviation only if the owner rejects the fix. | Proposed: fix |
| DEV-003.03 | `core/src/memory/json5_document.cpp:54`, overlap check | `reinterpret_cast<std::uintptr_t>(input.data)` | 8.2.5, 8.2.7 | Start address of the input, for the input/buffer overlap check (C-3.6). | Approved (2026-11-20, Romanr92) |
| DEV-003.04 | `core/src/memory/json5_document.cpp:60`, overlap check | `reinterpret_cast<std::uintptr_t>(buffer)` | 8.2.5, 8.2.7 | Start address of the buffer, for the same check. | Approved (2026-11-20, Romanr92) |
| DEV-003.05 | `core/src/parser/parser_engine.cpp:71` | `reinterpret_cast<const std::byte*>(input.data)` | 8.2.5 | Views the `char` input as bytes at the text/byte boundary (ADR 0008). Byte access to any object is allowed. | Approved (2026-11-20, Romanr92) |
