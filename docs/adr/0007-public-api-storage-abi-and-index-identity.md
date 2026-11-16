# ADR 0007: Public API, storage callbacks, ABI and index identity

Status: Policy decisions and index-identity design accepted by the user, 2026-11-15, for
[#128](https://github.com/Romanr92/Adaptive_JSON5/issues/128).
The mutation boundary, single allocation callback, ABI policy, tagged-index/token
design and compact private links are implemented in the current checkout. Compact
links were added in `4e78361`, with the ancestor-walk correction in `77c544c`. Named unit inventory is in `docs/traceability.md`;
executed results are recorded in the implementation sequence. This ADR does not
change requirement status or establish complete verification. ADR 0006 is being
prepared on another branch; its number is reserved.

## Context

Issue #128 covers review findings ARC-03, ARC-05, ARC-07 and ARC-09: exposed
document construction, two allocation callbacks, binary compatibility policy and
indices that contain a position without document identity. Its primary contract
references are J5-008, J5-092 and J5-093 in
[requirements](../requirements.md). Ownership, access, fallible storage and
embedded constraints also apply (J5-039, J5-040, J5-046, J5-047 and J5-101).

At issue creation, public `document` exposed `make_*`, `append_*`, `set_source_span`
and `set_root`. Construction helpers and storage representations are also
reachable through public headers. At issue creation, `storage_callbacks` supported both `allocate`
and `allocate_result`, preferring the latter. The original `value_index` contained only a
position; an in-range index from document A can silently address a different
value in document B.

The existing owner decision on #128 requires both internal-only mutation and
an obligation in the Safety Manual forbidding modification of parsed documents.
The user selected options **1A, 2A, 3A and 4B** in this conversation.

## Decisions

### 1. Construction is internal; no supported public builder (1A)

The supported document API exposes parsing, ownership and read access.
Construction and mutation belong to internal `detail` facilities, rather than
public document mutators or a separate supported builder.

Use the existing private `detail::document_access` mechanism where appropriate.
Merely adding a `detail` name while retaining callable public document mutators
does not establish this boundary. Supporting declarations needed for the
by-value layout of public objects may remain visible and explicitly unsupported,
as allowed by [ADR 0002](../../core/docs/adr-0002-public-private-source-layout.md).
Public headers must not depend on the private include tree.

Review `owned_block`, `allocate_block`, `detail::value_storage` and
`detail::document_storage_owner` as part of the supported-surface inventory;
their exposure must not provide a supported path for mutating parsed documents.
Do not introduce a parallel public `detail/` implementation tree.

This keeps the supported API smaller and addresses the existing safety decision.
A supported programmatic builder is outside this issue's chosen API.

### 2. One result-returning allocation callback (2A)

In the next breaking release, `storage_callbacks` has one allocation entry point:
the result-returning callback currently named `allocate_result`, paired with
`deallocate` and the provider context. Remove the pointer-or-null `allocate`
entry point and its dispatch fallback.

Keep explicit, non-throwing failures in all compiler exception modes. Preserve
the distinction between configured budget exhaustion (`resource_limit`) and
provider exhaustion (`allocation_failure`), together with alignment, ownership,
provider lifetime and matching deallocation contracts. Error reporting must not
require a successful allocation.

Existing providers need migration. Documentation should show how a provider that
previously returned a pointer or null returns a result instead. This simplifies
dispatch and testing without losing failure information. Retaining both callbacks
was considered and rejected because it preserves two paths and precedence rules.

### 3. No ABI guarantee across releases (3A)

The project does not promise binary compatibility across releases, including
patch releases. Consumers rebuild the library and dependent C++ code when
upgrading, and use compatible target, compiler, runtime and build settings.
This is an ABI policy, not a promise that source compatibility is unchanged.

Do not introduce a versioned inline namespace as part of this decision. Revisit
ABI stability when the API stabilizes. A versioned namespace alone would not
establish binary compatibility.

Build on the existing package metadata for immutable source identity and build
configuration. Document relevant ABI compatibility conditions, including compiler
and standard-library ABI, architecture, packing, exception/RTTI settings and
public allocator/callback or object-layout changes. Distinguish settings that
change representation or calling conventions from settings that only select
implementation algorithms; do not claim that every option changes the ABI.

This permits the current pre-1.0 API cleanup without freezing layouts. The tradeoff
is that prebuilt consumers cannot assume a release is a drop-in binary replacement.

### 4. Detect foreign indices at runtime (4B)

An index identifies both a document's value storage and a position. Operations
accepting an index must validate document identity as well as bounds before
accessing records. A foreign index returns an explicit error, including when its
position would be in range in the receiving document. It must not silently select
a value from that document.

For example, an index obtained for `20` in document A containing `[10, 20]` must
be rejected when supplied to document B containing `[90, 80]`; it must not return
`80`. Ordinary array element positions remain positions, not owner-tagged handles.

The user accepts the added index storage and identity-management cost in exchange
for detecting this misuse. Keeping compact indices with only a documented
same-document precondition was considered and rejected.

The user subsequently accepted the following implementation design.

#### Stable identity token

Each document owns a small, separately allocated identity token. Allocate it
through the document's caller-supplied storage when its first records are
allocated; empty construction still allocates nothing. The token remains at a
stable address through record-storage growth and document moves. Its address
provides identity; it is not the address of the movable document object.

Token allocation is fallible, uses the existing result/error contract and counts
against the existing owned-memory budget. If initialization fails, release any
allocations acquired by that attempt and publish no index or partial document.
Release records before releasing the token. Document the additional per-document
allocation and storage cost, including its effect on caller-buffer sizing.

Do not use the accounting provider itself as document identity: multiple live
documents can share one provider. This design needs neither a global ID counter
nor an OS service or implicit heap allocation.

#### Tagged public handles and compact private links

A public `value_index` holds an owner-token identity and a record position.
Private parent, sibling and container links store positions only, using an
internal type distinct from the tagged public handle. Validate private positions
before record access; keeping them compact does not remove bounds checks or
the existing invariant-error handling for damaged links.

This avoids duplicating the owner tag in every record relationship. Public
handles become larger, while private links remain compact. Exact sizes depend
on the target ABI and must be measured rather than assumed.

#### Moves, reuse and lifetime

| Operation | Index behavior |
|---|---|
| Move document A into B | A's existing indices identify the values now owned by B |
| Use those indices with moved-from A | Rejected |
| Reuse moved-from A internally | New records receive a new identity |
| Grow record storage | Existing indices retain their identity and positions |
| Destroy the owning document | Its indices cease to be valid |

Existing views and iterators remain invalidated by moves as documented; unlike
indices, they borrow the address of the movable storage object. Identity tokens
may be reused after destruction. Foreign-index detection applies during valid
handle lifetimes and does not promise detection of stale handles after their
document has died. Indices do not extend document lifetime or dereference their
token to establish ownership.

#### Library-issued indices

Remove supported construction of a valid index from an arbitrary numeric
position. Retain default construction of an unset index and copying of issued
indices. Restrict creation of tagged indices to internal library facilities.

Provide a checked, read-only `value_view` accessor returning its `value_index`
through the existing result contract. This lets consumers obtain indices from
root access, lookup and iteration after public builders are removed. Issuing an
index performs no allocation. Ordinary array element positions remain numeric
positions and do not require owner tags.

#### Central validation and explicit errors

Use the storage access layer as the main validation boundary and audit every
index-consuming path for bypasses. Validate before record access, in this order:

1. An unset index reports `access_error_reason::index_out_of_range`.
2. An owner mismatch reports a new `access_error_reason::foreign_index`, even
   when the supplied position is in range.
3. A matching owner with an invalid position reports `index_out_of_range`.
4. Otherwise, resolve the record.

`foreign_index` refines `error_code::wrong_type_or_access`; caller misuse is not
an `internal_error`. Include the new reason in diagnostic formatting and public
error documentation. Private damaged-link failures retain their existing
invariant-error behavior.

#### Implementation locations

Update `value_index.hpp` for tagged handles and restricted issuance, and
`document.hpp` / `core/src/value/value_storage.cpp` for token ownership, checked
access and move behavior. Update private record/container representations for
compact links. Add the checked index accessor in the `value_view.hpp` /
`core/src/value/value_view.cpp` declaration/source pair, and update access-error
definitions and diagnostic formatting. These paths refer to the existing public
headers under `includes/adaptive_json5/` and private code under `core/`.

Representation and declarations required by public by-value layouts must follow
ADR 0002: public headers cannot include a private header. Substantial ordinary
implementations remain compiled source definitions.

## Implementation and verification follow-up

Romanr92 owns production C++ and public-header changes. AI may prepare build
setup, documentation and GoogleTest units. Substantial implementations remain
in compiled `.cpp` files under `core/src/`, with their source lists maintained.

1. Inventory the supported API, implement the internal construction boundary and
   migrate parser callers. Keep public-header/library units in `tests/`; units
   that require construction internals belong in `core/tests/`.
2. Migrate storage providers and allocation dispatch together. Verify distinct
   failures, alignment and exactly matched allocation/deallocation ownership.
3. Document ABI and provider migration rules using existing package metadata.
4. Implement the accepted identity-token design, compact private links, checked
   index issuance and validation consistently for every index-taking access path.
5. Add public-surface compile checks, umbrella coverage updates and runtime units
   for foreign indices with equal in-range positions, same-document access,
   unset/out-of-range indices and the chosen move/reuse behavior. Public units
   must not receive private include access.
6. Run applicable GoogleTest units with exceptions OFF and RTTI OFF and complete
   the repository's required quality checks. Record actual execution separately
   from test existence and these accepted policy decisions.

Update [traceability](../traceability.md) with the resulting implementation and
named test evidence. Current builder-based public units will need adaptation;
their existence does not verify the new API boundary or owner validation.

The mutation decision addresses SSR-03 through FMEA VM-06/AP-05 and Safety Manual
C-3.3. The supported mutation surface has been removed and local host evidence is
recorded in P030. Required CI and independent safety/security review remain
separate and outstanding. This ADR is not a safety/security approval and does not close #128.

Local verification on 2026-11-16 covers `77c544c` plus test-only migration of
relationship fault injection to `record_link`; see [P030](../../core/docs/requirements-json5-implementation-order.md#p030--owned-valuecontainer-model).
