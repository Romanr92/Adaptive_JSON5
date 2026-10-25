# Memory, limits and failure behavior

This page is for people who import the library and call `parse_document`. It
explains what you provide, what the limits mean, and what happens when a parse
fails. Internal design reasons are in
[ADR 0003](../core/docs/adr-0003-storage-and-budget-decisions.md).

Each section says whether it describes **current behavior** (implemented, and
documented in the public headers) or a **planned change** (decided, not yet
implemented). Don't rely on a planned change until a release says it is
implemented.

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
| `max_work_units` | 32768 | Total work charged during one call (planned to change, see below) |

**How string units are counted.** Each string value and each member name must fit
`max_string_units` by itself. All of them together must fit
`max_total_string_units` for the whole document. Both are measured after decoding
escapes, in UTF-16 units: a character outside the Basic Multilingual Plane counts
as 2. Internally there is exactly one counter for the total, so both checks always
agree.

**Warnings.** Every warning is kept. More warnings than `max_diagnostics` fail
the whole parse with `resource_limit`. No document is returned with some of its
warnings missing.

## Planned changes (decided in ADR 0003, not yet implemented)

These change what you observe once implemented:

| Change | What you will notice |
|---|---|
| Default `max_work_units` becomes **131072** (8 × `max_input_bytes`) | Documents near the size limits parse with default options. If you raise `max_input_bytes`, raise the work limit in the same proportion. The default work budgets of number conversion and comparison rise with it. |
| Counting pre-pass, then exact allocation | Memory used for values, frames and strings equals what the document needs, with no spare capacity. Invalid or over-limit input fails **before any allocation**. A successful parse makes few, fixed-size allocations, so pools fragment less. |
| Literals (`true`, `false`, `null`) allocate nothing | Fewer calls to your allocator. |
| Warning table allocated once | The first warning allocates `max_diagnostics` entries, which is the worst case known in advance. Input without warnings allocates nothing for warnings. An optional adaptive policy in `parse_options` starts smaller, for tooling that sets a large `max_diagnostics`. |
| Work charged before scanning goes too far | A long or unterminated token under a small work budget fails with `resource_limit` instead of being scanned to the end. |

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
limits to match, with some margin. After the planned work-budget change, keep
`max_work_units` at about 8 × `max_input_bytes`.

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
   the longest parse time you can allow.

See also [embedded integration](embedded.md).
