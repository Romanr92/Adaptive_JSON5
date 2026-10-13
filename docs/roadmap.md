# Implementation roadmap

Issues #2 onward follow the concrete P001–P066 requirement sequence. Issue #1
remains the public-contract umbrella; its original full scope is not a prerequisite
for each early component. P001 configuration units passed; production target and
packaging completion is #68. P002 and P003 are implemented; issues #2 and #3 are complete.
Issue #1's public declarations/contracts scope is complete. P004 remains open:
input/options declarations exist, but its bounded reading cursor is pending.
The parser is not implemented yet.

Each implementation issue describes the behavior to implement and specific
GoogleTest cases. Its acceptance checklist is documentation, implementation and
unit tests. Required builds use exceptions OFF and RTTI OFF. Pull requests require
one passing unit-tests job and one passing formatting job. Production C++ remains
the developer's responsibility.

The original issue text is retained in [the backlog snapshot](issue-backlog-before-p-sequence.json).
Original J5 IDs remain stable; current ownership is in [traceability](traceability.md).
The P dependencies govern component implementation; older J5 dependency groups
do not force a complete parser before foundational storage/cursor work.

| Step | GitHub issue | J5 contracts | Prerequisites |
| --- | --- | --- | --- |
| P001 | [#1 — Independent C++17 build](https://github.com/Romanr92/Adaptive_JSON5/issues/1) | J5-001, J5-002, J5-091, J5-101 | None |
| P002 | [#2 — Non-throwing result foundation](https://github.com/Romanr92/Adaptive_JSON5/issues/2) | J5-003, J5-043, J5-045 | P001 |
| P003 | [#3 — Fallible storage and accounting](https://github.com/Romanr92/Adaptive_JSON5/issues/3) | J5-040, J5-046, J5-051, J5-101 | P002 |
| P004 | [#4 — Explicit input cursor](https://github.com/Romanr92/Adaptive_JSON5/issues/4) | J5-004, J5-041, J5-044 | P002 |
| P005 | [#5 — Unicode source decoding](https://github.com/Romanr92/Adaptive_JSON5/issues/5) | J5-009 | P004 |
| P006 | [#6 — Character classification](https://github.com/Romanr92/Adaptive_JSON5/issues/6) | J5-006, J5-019, J5-052 | P005 |
| P007 | [#7 — Line terminators](https://github.com/Romanr92/Adaptive_JSON5/issues/7) | J5-011, J5-017, J5-044 | P005 |
| P008 | [#8 — Whitespace](https://github.com/Romanr92/Adaptive_JSON5/issues/8) | J5-011, J5-025 | P006, P007 |
| P009 | [#9 — Line comments](https://github.com/Romanr92/Adaptive_JSON5/issues/9) | J5-012 | P007 |
| P010 | [#10 — Block comments](https://github.com/Romanr92/Adaptive_JSON5/issues/10) | J5-012 | P005 |
| P011 | [#11 — Trivia composition](https://github.com/Romanr92/Adaptive_JSON5/issues/11) | J5-012, J5-024 | P008, P009, P010 |
| P012 | [#12 — Punctuation tokens](https://github.com/Romanr92/Adaptive_JSON5/issues/12) | J5-013 | P004, P011 |
| P013 | [#13 — String storage](https://github.com/Romanr92/Adaptive_JSON5/issues/13) | J5-018, J5-029, J5-040 | P003, P005 |
| P014 | [#14 — Quoted string boundaries](https://github.com/Romanr92/Adaptive_JSON5/issues/14) | J5-014 | P007, P013 |
| P015 | [#15 — Simple character escapes](https://github.com/Romanr92/Adaptive_JSON5/issues/15) | J5-015, J5-016 | P014 |
| P016 | [#16 — Hexadecimal string escapes](https://github.com/Romanr92/Adaptive_JSON5/issues/16) | J5-016 | P014 |
| P017 | [#17 — Unicode string escapes](https://github.com/Romanr92/Adaptive_JSON5/issues/17) | J5-015, J5-018 | P013, P014 |
| P018 | [#18 — Non-escape characters](https://github.com/Romanr92/Adaptive_JSON5/issues/18) | J5-016 | P015, P016, P017 |
| P019 | [#19 — String line continuations](https://github.com/Romanr92/Adaptive_JSON5/issues/19) | J5-017 | P007, P014 |
| P020 | [#20 — Raw separator warnings](https://github.com/Romanr92/Adaptive_JSON5/issues/20) | J5-017 | P002, P014 |
| P021 | [#21 — Complete string lexer](https://github.com/Romanr92/Adaptive_JSON5/issues/21) | J5-014, J5-015, J5-016, J5-017, J5-018 | P015, P016, P017, P018, P019, P020 |
| P022 | [#22 — IdentifierName tokens](https://github.com/Romanr92/Adaptive_JSON5/issues/22) | J5-019 | P006 |
| P023 | [#23 — Identifier Unicode escapes](https://github.com/Romanr92/Adaptive_JSON5/issues/23) | J5-020, J5-033 | P017, P022 |
| P024 | [#24 — Null and boolean classification](https://github.com/Romanr92/Adaptive_JSON5/issues/24) | J5-023 | P022, P023 |
| P025 | [#25 — Decimal numeric tokens](https://github.com/Romanr92/Adaptive_JSON5/issues/25) | J5-021, J5-022 | P004 |
| P026 | [#26 — Hexadecimal numeric tokens](https://github.com/Romanr92/Adaptive_JSON5/issues/26) | J5-022 | P025 |
| P027 | [#27 — Non-finite numeric tokens](https://github.com/Romanr92/Adaptive_JSON5/issues/27) | J5-022 | P025 |
| P028 | [#28 — Token completion](https://github.com/Romanr92/Adaptive_JSON5/issues/28) | J5-024, J5-027 | P012, P021, P023, P024, P025, P026, P027 |
| P029 | [#29 — Owned numeric representation](https://github.com/Romanr92/Adaptive_JSON5/issues/29) | J5-034, J5-038, J5-040 | P003, P025, P026, P027 |
| P030 | [#30 — Owned value/container model](https://github.com/Romanr92/Adaptive_JSON5/issues/30) | J5-028, J5-030, J5-031, J5-039, J5-040 | P003, P013, P029 |
| P031 | [#31 — Resource guards](https://github.com/Romanr92/Adaptive_JSON5/issues/31) | J5-047, J5-048, J5-049 | P002, P003, P030 |
| P032 | [#32 — Primitive value dispatcher](https://github.com/Romanr92/Adaptive_JSON5/issues/32) | J5-023, J5-028 | P021, P024, P028, P029, P030, P031 |
| P033 | [#33 — Recursive container engine](https://github.com/Romanr92/Adaptive_JSON5/issues/33) | J5-013, J5-027, J5-048 | P012, P030, P031, P032 |
| P034 | [#34 — Array element grammar](https://github.com/Romanr92/Adaptive_JSON5/issues/34) | J5-013, J5-030 | P033 |
| P035 | [#35 — Object member grammar](https://github.com/Romanr92/Adaptive_JSON5/issues/35) | J5-013, J5-019, J5-020, J5-031 | P021, P023, P024, P033 |
| P036 | [#37 — Duplicate-name policy](https://github.com/Romanr92/Adaptive_JSON5/issues/37) | J5-032, J5-033 | P035 |
| P037 | [#38 — Complete document entry point](https://github.com/Romanr92/Adaptive_JSON5/issues/38) | J5-026, J5-051 | P011, P032, P034, P035, P036 |
| P038 | [#39 — Checked numeric operations](https://github.com/Romanr92/Adaptive_JSON5/issues/39) | J5-035, J5-036, J5-037, J5-038, J5-052 | P029, P030, P037 |
| P039 | [#40 — Owned document and diagnostics](https://github.com/Romanr92/Adaptive_JSON5/issues/40) | J5-040, J5-041, J5-043, J5-044, J5-045 | P020, P030, P037 |
| P040 | [#41 — Whole-grammar unit evidence](https://github.com/Romanr92/Adaptive_JSON5/issues/41) | J5-027, J5-094, J5-095, J5-097, J5-098, J5-099 | P001, P002, P003, P004, P005, P006, P007, P008, P009, P010, P011, P012, P013, P014, P015, P016, P017, P018, P019, P020, P021, P022, P023, P024, P025, P026, P027, P028, P029, P030, P031, P032, P033, P034, P035, P036, P037, P038, P039 |
| P041 | [#42 — Full-support claim](https://github.com/Romanr92/Adaptive_JSON5/issues/42) | J5-008, J5-027, J5-100 | P040 |
| P042 | [#43 — Strict JSON syntax mode](https://github.com/Romanr92/Adaptive_JSON5/issues/43) | J5-005, J5-010, J5-015, J5-021, J5-025 | P041 |
| P043 | [#44 — Independent schema component and outcomes](https://github.com/Romanr92/Adaptive_JSON5/issues/44) | J5-053, J5-063, J5-065, J5-066, J5-068 | P042 |
| P044 | [#45 — Schema locations and offline reference registry](https://github.com/Romanr92/Adaptive_JSON5/issues/45) | J5-055, J5-064, J5-066, J5-067 | P043 |
| P045 | [#46 — Exact schema equality](https://github.com/Romanr92/Adaptive_JSON5/issues/46) | J5-059, J5-061 | P043 |
| P046 | [#47 — Bounded schema evaluation engine](https://github.com/Romanr92/Adaptive_JSON5/issues/47) | J5-053, J5-056, J5-063, J5-065 | P044, P045 |
| P047 | [#48 — Type and equality assertions](https://github.com/Romanr92/Adaptive_JSON5/issues/48) | J5-057, J5-059, J5-061 | P045, P046 |
| P048 | [#49 — Numeric assertions](https://github.com/Romanr92/Adaptive_JSON5/issues/49) | J5-057, J5-059 | P047 |
| P049 | [#50 — String assertions and regex budget](https://github.com/Romanr92/Adaptive_JSON5/issues/50) | J5-057, J5-060, J5-065 | P046 |
| P050 | [#51 — Array assertions](https://github.com/Romanr92/Adaptive_JSON5/issues/51) | J5-057, J5-061 | P045, P046 |
| P051 | [#52 — Object assertions](https://github.com/Romanr92/Adaptive_JSON5/issues/52) | J5-057 | P046, P049 |
| P052 | [#53 — Logical and conditional assertions](https://github.com/Romanr92/Adaptive_JSON5/issues/53) | J5-057, J5-065 | P047, P048, P049, P050, P051 |
| P053 | [#54 — Annotations and extension handling](https://github.com/Romanr92/Adaptive_JSON5/issues/54) | J5-058, J5-062, J5-069 | P052 |
| P054 | [#55 — Public schema compilation and diagnostics](https://github.com/Romanr92/Adaptive_JSON5/issues/55) | J5-054, J5-063, J5-064 | P044, P052, P053 |
| P055 | [#56 — Schema conformance and packaging](https://github.com/Romanr92/Adaptive_JSON5/issues/56) | J5-053, J5-065, J5-066, J5-067, J5-068, J5-069, J5-096 | P054 |
| P056 | [#57 — CLI dispatch and bounded readers](https://github.com/Romanr92/Adaptive_JSON5/issues/57) | J5-070 | P042 |
| P057 | [#58 — CLI validation and diagnostic protocol](https://github.com/Romanr92/Adaptive_JSON5/issues/58) | J5-070, J5-071 | P055, P056 |
| P058 | [#59 — File-driven example executable](https://github.com/Romanr92/Adaptive_JSON5/issues/59) | J5-070, J5-071, J5-094 | P057 |
| P059 | [#60 — Host-tool packaging](https://github.com/Romanr92/Adaptive_JSON5/issues/60) | J5-002, J5-007, J5-070, J5-092, J5-093 | P058 |
| P060 | [#61 — VS Code extension package](https://github.com/Romanr92/Adaptive_JSON5/issues/61) | J5-072 | P057 |
| P061 | [#62 — Editor grammar and position mapping](https://github.com/Romanr92/Adaptive_JSON5/issues/62) | J5-074, J5-075 | P060 |
| P062 | [#63 — Versioned live documents and provisional recovery](https://github.com/Romanr92/Adaptive_JSON5/issues/63) | J5-076, J5-077 | P061 |
| P063 | [#64 — Offline workspace schema associations](https://github.com/Romanr92/Adaptive_JSON5/issues/64) | J5-073, J5-081 | P055, P062 |
| P064 | [#65 — Schema completions](https://github.com/Romanr92/Adaptive_JSON5/issues/65) | J5-078 | P053, P063 |
| P065 | [#66 — Hover and runtime-parity diagnostics](https://github.com/Romanr92/Adaptive_JSON5/issues/66) | J5-079, J5-080 | P057, P064 |
| P066 | [#67 — Editor release evidence](https://github.com/Romanr92/Adaptive_JSON5/issues/67) | J5-082, J5-100 | P065 |

## Final completion issues

- [#68 — Finish the C++17 library target and install the public headers](https://github.com/Romanr92/Adaptive_JSON5/issues/68): J5-001, J5-002, J5-007, J5-091, J5-092, J5-093.
- [#69 — Finish pure-memory parsing, concurrent contexts and embedded runtime contracts](https://github.com/Romanr92/Adaptive_JSON5/issues/69): J5-042, J5-050, J5-052, J5-101.
- [#70 — Finish the optional versioned consumer schema example](https://github.com/Romanr92/Adaptive_JSON5/issues/70): J5-083, J5-084, J5-085, J5-086.
- [#71 — Finish optional consumer numeric/string policies and result mapping](https://github.com/Romanr92/Adaptive_JSON5/issues/71): J5-087, J5-088, J5-089, J5-090.
- [#72 — Finish release records and reconcile documentation with implemented components](https://github.com/Romanr92/Adaptive_JSON5/issues/72): J5-008, J5-099, J5-100.

Core JSON5 work is P001–P041. Strict JSON is P042 and remains part of the core
project scope. Schema P043–P055, host tools P056–P059, editor P060–P066 and
consumer examples #70–#71 are separate optional components. A parser-only release
must complete its core/strict-JSON contracts and applicable #68/#69/#72 work;
optional component dependencies apply only when shipped.
