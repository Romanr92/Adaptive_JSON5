# Memory, limits and failure behavior

This page is for people who import the library, through the simple
`json5_document` API or the explicit `parse_document` API. It explains what you
provide, what the limits mean, and what happens when a parse fails. Internal design
reasons are in [ADR 0003](../core/docs/adr-0003-storage-and-budget-decisions.md)
and [ADR 0004](adr/0004-simple-api-and-umbrella-header.md).

Each section says whether it describes **current behavior** (implemented, and
documented in the public headers) or a **planned change** (decided, not yet
implemented). Don't rely on a planned change until a release says it is
implemented.

## Simple API: `json5_document` (current behavior)

Include `adaptive_json5/adaptive_json5.hpp` and link `AdaptiveJSON5::parser`.

### Parse into a buffer

```cpp
static unsigned char memory[16384];

const auto parsed = adaptive_json5::json5_document::parse({text, text_size}, memory, sizeof(memory));
```

- **The buffer** can be any writable bytes; alignment is handled inside. It holds a
  small control block at its start and every allocation of the document. It must
  stay alive, and must not be used for anything else, until the document is
  destroyed. After that, or after a failed parse, it can be reused.
- **The input** is only read during the call; the document keeps its own copies.
  Input and buffer must not overlap (`wrong_type_or_access`).
- **Options** are the same `parse_options` as for `parse_document`, and every limit
  below applies. A buffer that is too small reports `allocation_failure`; a
  configured limit reports `resource_limit`.
- **Sizing:** parse your largest real documents with a generous buffer and read
  `peak_bytes()`. Size the buffer from that value, plus a margin for the control
  block and alignment, which `peak_bytes()` does not count.
- **Ownership:** a `json5_document` can be moved but not copied or assigned. A
  moved-from document is empty and reports zero bytes.

### Read values

```cpp
const adaptive_json5::json5_value root = parsed.value_if()->root();

const auto port = root.member("server").member("port").as_int64();   // result<std::int64_t>
const auto first = root.member("hosts").at(0).as_string(name, sizeof(name));
```

- `member(name)`, `at(index)`, `size()`, `kind()` and `is_null()` … `is_object()`
  navigate. A failed step returns a `json5_value` that holds the error, so steps can be
  chained and the final read reports it. `ok()` and `error_if()` check a step directly.
- **Duplicate names:** JSON5 allows an object to repeat a member name, and the
  document keeps every member. `member(name)` then reports `wrong_type_or_access`
  instead of choosing one. Iterate through `values()` to see all of them.
- `as_bool`, `as_int64` and `as_uint64` read exactly; `1.0` is an integer, `1.5`
  is not. `as_double` rounds once. `as_string(buffer, capacity)` writes UTF-8 without
  a terminating NUL and reports `resource_limit` when the buffer is too small.
- A `json5_value` borrows its document: don't keep it after the document is
  destroyed or moved.

### Errors and warnings as text

```cpp
char message[128];
const auto text_view = adaptive_json5::describe(*parsed.error_if(), {text, text_size}, message);
// "line: 1, column: 5: unexpected end of input"
```

- `describe` writes `line: L, column: C: message` into your buffer, NUL-terminated,
  and returns a view of it. A buffer that is too short gets the shortened text. Pass
  the same input that was parsed: the document keeps no copy of it unless you ask
  for source retention.
- Valid documents can still carry warnings, for example a raw U+2028 or U+2029
  inside a string (JSON5 specification, section 5.2). Read them with
  `warning_count()` and `warning(index)`, and turn each into
  `line: L, column: C: warning: message` with `describe`.

### Advanced use

`values()` returns the full `document`: iteration over every member, exact numbers,
conversion options and source spans. `current_bytes()` and `peak_bytes()` report the
document's memory. `parse_document`, described next, parses with your own storage
callbacks instead of a buffer.

## What you provide

`parse_document` never uses a global heap. You supply memory through three objects:

| Object | Purpose |
|---|---|
| `storage_callbacks` | Your allocate and deallocate functions, plus a context pointer. Both are `noexcept`, and allocate returns `nullptr` (or a failed `result`) when memory is unavailable. |
| `accounted_storage` | Wraps your callbacks, enforces the byte ceiling and reports `current_bytes()` and `peak_bytes()`. It must stay at a stable address, and it must outlive every document parsed through it. |
| `parse_options` | Syntax mode, duplicate-name policy and `resource_limits`. `accounting.byte_limit()` must equal `options.limits.max_owned_bytes`. |

```cpp
#include "adaptive_json5/parser.hpp"

using namespace adaptive_json5;

// my_allocate / my_deallocate are your noexcept functions, e.g. backed by a static pool.
storage_callbacks callbacks{&my_pool, &my_allocate, &my_deallocate};

parse_options options{};                       // JSON5 mode, default limits
accounted_storage accounting{callbacks, options.limits.max_owned_bytes};

const auto parsed = parse_document({text, text_size}, options, accounting);
if (!parsed.has_value())
{
  const error& failure = *parsed.error_if();   // code, offset, reason; nothing to clean up
  // ... report, then return or try again
}
```

### One call on hosted systems (optional)

On desktop or server systems that have a global heap, the optional hosted component
parses in a single call. Build it with `-DADAPTIVE_JSON5_BUILD_HOSTED=ON` and link
`AdaptiveJSON5::hosted`; the README's
[memory switch tutorial](../README.md#memory-switch-adaptive_json5_build_hosted)
shows how to turn it on and off:

```cpp
#include "adaptive_json5/hosted/parse.hpp"

const auto parsed = adaptive_json5::hosted::parse({text, text_size});   // default options
if (parsed.has_value())
{
  const adaptive_json5::json5_value root = parsed.value_if()->root();
}
```

- `hosted::parse(input, options = {})` parses exactly like `parse_document`: same
  limits, errors and warnings.
- It returns the same `json5_document` as the caller-buffer path, owning its heap
  storage, so nothing else has to stay alive. It can be moved; `current_bytes()` and
  `peak_bytes()` report its memory.
- Memory comes from `::operator new(std::nothrow)`, still bounded by
  `max_owned_bytes`. A small control block per call is not counted against it.
- This component uses the global heap, so it is never part of the embedded parser
  library. On targets without a reliable heap, use `parse_document` with your own
  storage as shown above.

## Failure behavior (current behavior)

**Each call to `parse_document` stands alone.** There is no parser object to
reset, and no partial state survives a failed call.

When a call fails:

- **No document is returned,** not even part of one. You receive only the
  `error` value.
- **Everything the call allocated is released before it returns.**
  `accounting.current_bytes()` is back to what it was before the call.
- **Earlier documents are not affected.** Documents you already parsed through
  the same `accounted_storage` stay valid and keep their memory.
- **The `accounted_storage` remains usable.** To try again, for example with
  raised limits, call `parse_document` again with the same or a new accounting
  object. Nothing has to be reset or rolled back first.
- **`peak_bytes()` keeps the highest value reached,** including during the failed
  call. Use it to size your memory pool.
- **Errors are values, never exceptions,** in every compiler exception mode.

Which error tells you what:

| `error_code` | Meaning | What to do |
|---|---|---|
| `syntax`, `encoding`, `duplicate_policy` | The input is invalid for the selected mode or policy | Fix the input; retrying unchanged gives the same error |
| `resource_limit` | A configured limit or the work budget would be exceeded | Raise the specific limit, or reject the input |
| `allocation_failure` | Your allocate callback returned no memory | Free memory or grow your pool, then retry |
| `wrong_type_or_access` | Invalid argument, for example a `byte_limit()` that doesn't match `max_owned_bytes` | Fix the call |

**Internally,** the parser state for one call is single-use. A failed call
discards that state as a whole instead of undoing its counters step by step. You
never see this state; the rule is why a fresh call is always the correct retry.

## Limits (current behavior)

All limits are fields of `resource_limits` in
`includes/adaptive_json5/memory/resource_limits.hpp`. Exceeding any of them
returns `resource_limit`.

| Limit | Default | What it counts |
|---|---|---|
| `max_input_bytes` | 16384 | The whole input, including whitespace and comments |
| `max_depth` | 512 | Arrays and objects open at the same time; a scalar root has depth 0 |
| `max_nodes` | 1024 | All values, including the root and containers |
| `max_members` | 1024 | Object members across the whole document, duplicates included |
| `max_string_units` | 4096 | Decoded UTF-16 units in **one** string value or member name |
| `max_total_string_units` | 16384 | Decoded UTF-16 units of **all** string values **and** member names together |
| `max_number_bytes` | 1024 | Bytes of one number spelling |
| `max_exponent_digits` | 1024 | Digits of one exponent, leading zeros included |
| `max_owned_bytes` | 524288 | Bytes allocated through the accounting object at the same time |
| `max_diagnostics` | 16 | Retained warnings; one more fails the parse |
| `max_work_units` | 131072 | Total work charged during one call; 8 × `max_input_bytes`, so scale it with the input limit |

**How string units are counted.** Each string value and each member name must fit
`max_string_units` by itself. All of them together must fit
`max_total_string_units` for the whole document. Both are measured after decoding
escapes, in UTF-16 units: a character outside the Basic Multilingual Plane counts
as 2. Internally there is exactly one counter for the total, so both checks always
agree.

**Allocation-free literals.** `true`, `false` and `null` never call your
allocator. Strings, numbers and the document itself still allocate through it.

**How work is charged.** Each scan of the input (whitespace and comments, every
token, and the second walk over a string that has warnings) is charged exactly
the bytes it reads. A scan never reads past the remaining budget, so a long or
unterminated token under a small budget fails with `resource_limit` instead of
being read to the end. A scan that ends within 3 bytes of the limit also fails
with `resource_limit`, so a budget that is too small never produces a false
syntax error. If you size `max_work_units` from measurements, keep a few units
of headroom.

**Unknown words in value position** (for example `nul` or `trueish`) fail with a
`syntax` error whose `source_range` covers the whole word.

**Warnings.** Every warning is kept. More warnings than `max_diagnostics` fail
the whole parse with `resource_limit`. No document is returned with some of its
warnings missing.

**Warning table.** By default (`warning_storage_policy::fixed`), the first
warning allocates a table of `max_diagnostics` entries at once, and it never
grows. That is about 12 bytes per entry on 32-bit targets and 24 bytes on 64-bit
hosts, and the worst case is known when you choose `max_diagnostics`. A document
without warnings allocates nothing for warnings. Tooling that sets a large
`max_diagnostics` but usually sees few warnings can set
`parse_options::warning_storage = warning_storage_policy::adaptive`: the first
warning then allocates `min(max_diagnostics, 64)` entries, and the table doubles
as needed, never past `max_diagnostics`.

### Two passes: count first, then allocate exactly

Each parse reads the input twice. The first pass checks the syntax and every limit
and counts what the document needs, without allocating. The second pass builds the
document into storage reserved once at exactly those sizes.

- **Invalid or over-limit input fails before any allocation.** Syntax errors,
  trailing content and every limit in the table above are found in the first pass.
- **A successful parse makes few, fixed-size allocations:** one block for all
  values, one for the open-container frames, plus one per string, member name and
  number. Nothing grows or is relocated, so memory pools fragment less.
- **Work:** both passes are charged. The default `max_work_units` of 131072 covers
  every maximum-size (16384-byte) document shape measured for ADR 0003: the
  heaviest, four strings of 4090 units, needs 65 527 units.
- **Duplicate names** are checked in the second pass only. With
  `reject_duplicates`, a document that has a duplicate name **and** another error
  reports the other error (syntax, encoding or limit), not the duplicate. A
  document whose only problem is a duplicate still reports it, with both
  locations.
- **Very deep limits:** with `max_depth` up to 512 (the default), the first pass
  uses no heap. Above 512 it allocates `max_depth / 8 + 1` bytes for its nesting
  state and releases them before the second pass.

### Work budget

The default `max_work_units` is 8 × `max_input_bytes`. If you raise
`max_input_bytes`, raise `max_work_units` in the same proportion. The default work
budgets of number conversion and comparison use the same value.

**Known cost:** with `duplicate_name_policy::reject_duplicates`, checking a large
object costs work that grows with the square of its member count. One object with
1024 members needs about 524 000 work units, so raise `max_work_units` when you
use this policy with large objects.

## Usage pattern: parsing many configuration files

This pattern fits services that read many small configuration or manifest files,
for example at startup and again when a file changes. It uses only current
behavior.

**There is no parser object to keep.** Every `parse_document` call builds its own
internal parser state on the stack, without allocating, and discards it when the
call returns. Creating that state costs almost nothing compared with lexing the
input and building the document, which any parser has to do. What you keep and
reuse across calls is everything that costs something:

| Reused across files | New for each call |
|---|---|
| Your storage provider (pool or arena) | Internal parser state (stack, no allocation) |
| `accounted_storage`, one per provider or arena | Counters for that one parse |
| `parse_options` and its limits | |
| Parsed documents, for as long as you keep them | |

**Recommended flow: parse, extract, release.**

1. **Parse** one file with `parse_document`.
2. **Extract** what your service needs into its own structures: paths, arguments,
   numbers and flags. Use the value API, the checked numeric conversions and
   `utf8_export` for strings. After this step, nothing refers to the document.
3. **Release** the document by letting it go out of scope. If the provider is an
   arena used only for this file, reset the arena **after** the document is
   destroyed, never before.
4. **Repeat** for the next file.

```cpp
for (const config_file& file : files)
{
  // One arena per file: memory never grows beyond the largest file.
  arena.reset();                                   // previous document already destroyed
  accounted_storage accounting{arena.callbacks(), options.limits.max_owned_bytes};

  const auto parsed = parse_document({file.data, file.size}, options, accounting);
  if (!parsed.has_value())
  {
    report(file, *parsed.error_if());              // this file only; others are unaffected
    continue;
  }

  my_config config{};
  if (extract(parsed.value_if()->values, config))  // copy out what you need
  {
    configs.push_back(config);
  }
}                                                  // document destroyed here, before the next reset
```

In this example, `arena`, `config_file`, `my_config`, `report`, `extract` and
`configs` are your code, not library APIs. The order is what matters: the
`accounted_storage` and the arena must outlive the parsed document.

**What this gives you:**

- **Isolation between files.** A broken or oversized file fails with an error value
  and releases its memory. The next call starts from a clean state, so one bad
  file never affects how the others parse.
- **Memory bounded by the largest file,** not by the sum of all files, because each
  document is released before the next parse.
- **Simple reloading.** When a file changes, call `parse_document` again. There is
  nothing to reset or keep in sync.
- **Parallel parsing without library locks.** Give each thread its own
  `accounted_storage` and provider, or synchronize a shared provider yourself.
  Shared `parse_options` and input bytes are only read.

**Keeping documents instead.** If you keep several documents alive at the same
time, they share their accounting object's `max_owned_bytes`. Size the ceiling for
all retained documents together, or give each its own accounting object.

**Set the limits for your files.** Configuration and manifest files can be larger
or more deeply nested than the defaults allow. Measure your largest real files,
then set `max_input_bytes`, `max_nodes`, `max_members`, `max_depth` and the string
limits to match, with some margin. Keep `max_work_units` at about
8 × `max_input_bytes`.

**When a reusable parser would help.** Keeping buffers allocated between calls only
pays off at very high parse rates, such as thousands of documents per second.
Services that parse a few dozen files at startup and occasionally afterwards don't
need it. The library offers no reusable parser object today. Adding one would be
a separate, additive decision, and nothing in the current design prevents it.

## Sizing for embedded targets

1. Set the limits to the largest documents you accept, not to the defaults.
2. Parse your largest real documents on the host and read `peak_bytes()`. Size
   your pool from that value plus margin, and set `max_owned_bytes` to the pool
   size.
3. Measure the time per work unit on your target once. Set `max_work_units` from
   the longest parse time you can allow. If you size it from the work your largest
   documents need, add some headroom: a scan that ends within 3 bytes of the limit
   fails with `resource_limit`.

See also [embedded integration](embedded.md).
