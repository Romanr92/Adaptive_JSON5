# Implementation roadmap

The complete P001–P066 technical contracts and prerequisite sequence remain in
[the numbered baseline](requirements/json5-implementation-order.md). Tracking now
uses [the retained-scope consolidation plan](issue-consolidation-plan.md), with
at most three active issues per subsystem. Schema P043–P055, minimal testing CLI
P056–P059 and full advanced memory accounting are required in this delivery.
They remain independently optional build components for library consumers.

Romanr92 writes production C++ and public headers. AI creates independent unit
tests. No separate per-issue documentation report is required; essential API,
ownership, limits/defaults/accounting, errors, versions/provenance and brief usage
contracts remain. Required PR checks stay one exceptions-OFF/RTTI-OFF GoogleTest
job and one formatting job.

Merged PRs #73/#75/#76 provide input/options, result/error, storage primitives and
cursor component evidence. The parser/schema/CLI remain unimplemented. P003/#3
does not complete integrated advanced memory accounting. Administrative issue
closures do not establish implementation or Verified status. Preserve the P005
branch and anchor #5.

| Step | Current GitHub issue | J5 contracts | Prerequisites |
| --- | --- | --- | --- |
| P001 | [#1](https://github.com/Romanr92/Adaptive_JSON5/issues/1) | J5-001, J5-002, J5-091, J5-101 | None |
| P002 | [#2](https://github.com/Romanr92/Adaptive_JSON5/issues/2) | J5-003, J5-043, J5-045 | P001 |
| P003 | [#3](https://github.com/Romanr92/Adaptive_JSON5/issues/3) | J5-040, J5-046, J5-051, J5-101 | P002 |
| P004 | [#4](https://github.com/Romanr92/Adaptive_JSON5/issues/4) | J5-004, J5-041, J5-044 | P002 |
| P005 | [#5](https://github.com/Romanr92/Adaptive_JSON5/issues/5) | J5-009 | P004 |
| P006 | [#5](https://github.com/Romanr92/Adaptive_JSON5/issues/5) | J5-006, J5-019, J5-052 | P005 |
| P007 | [#5](https://github.com/Romanr92/Adaptive_JSON5/issues/5) | J5-011, J5-017, J5-044 | P005 |
| P008 | [#5](https://github.com/Romanr92/Adaptive_JSON5/issues/5) | J5-011, J5-025 | P006, P007 |
| P009 | [#9](https://github.com/Romanr92/Adaptive_JSON5/issues/9) | J5-012 | P007 |
| P010 | [#9](https://github.com/Romanr92/Adaptive_JSON5/issues/9) | J5-012 | P005 |
| P011 | [#9](https://github.com/Romanr92/Adaptive_JSON5/issues/9) | J5-012, J5-024 | P008, P009, P010 |
| P012 | [#9](https://github.com/Romanr92/Adaptive_JSON5/issues/9) | J5-013 | P004, P011 |
| P013 | [#13](https://github.com/Romanr92/Adaptive_JSON5/issues/13) | J5-018, J5-029, J5-040 | P003, P005 |
| P014 | [#13](https://github.com/Romanr92/Adaptive_JSON5/issues/13) | J5-014 | P007, P013 |
| P015 | [#13](https://github.com/Romanr92/Adaptive_JSON5/issues/13) | J5-015, J5-016 | P014 |
| P016 | [#13](https://github.com/Romanr92/Adaptive_JSON5/issues/13) | J5-016 | P014 |
| P017 | [#13](https://github.com/Romanr92/Adaptive_JSON5/issues/13) | J5-015, J5-018 | P013, P014 |
| P018 | [#18](https://github.com/Romanr92/Adaptive_JSON5/issues/18) | J5-016 | P015, P016, P017 |
| P019 | [#18](https://github.com/Romanr92/Adaptive_JSON5/issues/18) | J5-017 | P007, P014 |
| P020 | [#18](https://github.com/Romanr92/Adaptive_JSON5/issues/18) | J5-017 | P002, P014 |
| P021 | [#18](https://github.com/Romanr92/Adaptive_JSON5/issues/18) | J5-014, J5-015, J5-016, J5-017, J5-018 | P015, P016, P017, P018, P019, P020 |
| P022 | [#22](https://github.com/Romanr92/Adaptive_JSON5/issues/22) | J5-019 | P006 |
| P023 | [#22](https://github.com/Romanr92/Adaptive_JSON5/issues/22) | J5-020, J5-033 | P017, P022 |
| P024 | [#22](https://github.com/Romanr92/Adaptive_JSON5/issues/22) | J5-023 | P022, P023 |
| P025 | [#25](https://github.com/Romanr92/Adaptive_JSON5/issues/25) | J5-021, J5-022 | P004 |
| P026 | [#25](https://github.com/Romanr92/Adaptive_JSON5/issues/25) | J5-022 | P025 |
| P027 | [#25](https://github.com/Romanr92/Adaptive_JSON5/issues/25) | J5-022 | P025 |
| P028 | [#25](https://github.com/Romanr92/Adaptive_JSON5/issues/25) | J5-024, J5-027 | P012, P021, P023, P024, P025, P026, P027 |
| P029 | [#29](https://github.com/Romanr92/Adaptive_JSON5/issues/29) | J5-034, J5-038, J5-040 | P003, P025, P026, P027 |
| P030 | [#29](https://github.com/Romanr92/Adaptive_JSON5/issues/29) | J5-028, J5-030, J5-031, J5-039, J5-040 | P003, P013, P029 |
| P031 | [#29](https://github.com/Romanr92/Adaptive_JSON5/issues/29) | J5-047, J5-048, J5-049 | P002, P003, P030 |
| P032 | [#29](https://github.com/Romanr92/Adaptive_JSON5/issues/29) | J5-023, J5-028 | P021, P024, P028, P029, P030, P031 |
| P033 | [#33](https://github.com/Romanr92/Adaptive_JSON5/issues/33) | J5-013, J5-027, J5-048 | P012, P030, P031, P032 |
| P034 | [#33](https://github.com/Romanr92/Adaptive_JSON5/issues/33) | J5-013, J5-030 | P033 |
| P035 | [#33](https://github.com/Romanr92/Adaptive_JSON5/issues/33) | J5-013, J5-019, J5-020, J5-031 | P021, P023, P024, P033 |
| P036 | [#33](https://github.com/Romanr92/Adaptive_JSON5/issues/33) | J5-032, J5-033 | P035 |
| P037 | [#38](https://github.com/Romanr92/Adaptive_JSON5/issues/38) | J5-026, J5-051 | P011, P032, P034, P035, P036 |
| P038 | [#38](https://github.com/Romanr92/Adaptive_JSON5/issues/38) | J5-035, J5-036, J5-037, J5-038, J5-052 | P029, P030, P037 |
| P039 | [#38](https://github.com/Romanr92/Adaptive_JSON5/issues/38) | J5-040, J5-041, J5-043, J5-044, J5-045 | P020, P030, P037 |
| P040 | [#38](https://github.com/Romanr92/Adaptive_JSON5/issues/38) | J5-027, J5-094, J5-095, J5-097, J5-098, J5-099 | P001, P002, P003, P004, P005, P006, P007, P008, P009, P010, P011, P012, P013, P014, P015, P016, P017, P018, P019, P020, P021, P022, P023, P024, P025, P026, P027, P028, P029, P030, P031, P032, P033, P034, P035, P036, P037, P038, P039 |
| P041 | [#38](https://github.com/Romanr92/Adaptive_JSON5/issues/38) | J5-008, J5-027, J5-100 | P040 |
| P042 | [#38](https://github.com/Romanr92/Adaptive_JSON5/issues/38) | J5-005, J5-010, J5-015, J5-021, J5-025 | P041 |
| P043 | [#44](https://github.com/Romanr92/Adaptive_JSON5/issues/44) | J5-053, J5-063, J5-065, J5-066, J5-068 | P042 |
| P044 | [#44](https://github.com/Romanr92/Adaptive_JSON5/issues/44) | J5-055, J5-064, J5-066, J5-067 | P043 |
| P045 | [#44](https://github.com/Romanr92/Adaptive_JSON5/issues/44) | J5-059, J5-061 | P043 |
| P046 | [#44](https://github.com/Romanr92/Adaptive_JSON5/issues/44) | J5-053, J5-056, J5-063, J5-065 | P044, P045 |
| P047 | [#48](https://github.com/Romanr92/Adaptive_JSON5/issues/48) | J5-057, J5-059, J5-061 | P045, P046 |
| P048 | [#48](https://github.com/Romanr92/Adaptive_JSON5/issues/48) | J5-057, J5-059 | P047 |
| P049 | [#48](https://github.com/Romanr92/Adaptive_JSON5/issues/48) | J5-057, J5-060, J5-065 | P046 |
| P050 | [#48](https://github.com/Romanr92/Adaptive_JSON5/issues/48) | J5-057, J5-061 | P045, P046 |
| P051 | [#48](https://github.com/Romanr92/Adaptive_JSON5/issues/48) | J5-057 | P046, P049 |
| P052 | [#53](https://github.com/Romanr92/Adaptive_JSON5/issues/53) | J5-057, J5-065 | P047, P048, P049, P050, P051 |
| P053 | [#53](https://github.com/Romanr92/Adaptive_JSON5/issues/53) | J5-058, J5-062, J5-069 | P052 |
| P054 | [#53](https://github.com/Romanr92/Adaptive_JSON5/issues/53) | J5-054, J5-063, J5-064 | P044, P052, P053 |
| P055 | [#53](https://github.com/Romanr92/Adaptive_JSON5/issues/53) | J5-053, J5-065, J5-066, J5-067, J5-068, J5-069, J5-096 | P054 |
| P056 | [#57](https://github.com/Romanr92/Adaptive_JSON5/issues/57) | J5-070 | P042 |
| P057 | [#57](https://github.com/Romanr92/Adaptive_JSON5/issues/57) | J5-070, J5-071 | P055, P056 |
| P058 | [#57](https://github.com/Romanr92/Adaptive_JSON5/issues/57) | J5-070, J5-071, J5-094 | P057 |
| P059 | [#57](https://github.com/Romanr92/Adaptive_JSON5/issues/57) | J5-002, J5-007, J5-070, J5-092, J5-093 | P058 |
| P060 | [#61](https://github.com/Romanr92/Adaptive_JSON5/issues/61) | J5-072 | P057 |
| P061 | [#61](https://github.com/Romanr92/Adaptive_JSON5/issues/61) | J5-074, J5-075 | P060 |
| P062 | [#61](https://github.com/Romanr92/Adaptive_JSON5/issues/61) | J5-076, J5-077 | P061 |
| P063 | [#64](https://github.com/Romanr92/Adaptive_JSON5/issues/64) | J5-073, J5-081 | P055, P062 |
| P064 | [#64](https://github.com/Romanr92/Adaptive_JSON5/issues/64) | J5-078 | P053, P063 |
| P065 | [#66](https://github.com/Romanr92/Adaptive_JSON5/issues/66) | J5-079, J5-080 | P057, P064 |
| P066 | [#66](https://github.com/Romanr92/Adaptive_JSON5/issues/66) | J5-082, J5-100 | P065 |

## Completion anchors

- [#68 — Finish the C++17 library target and install the public headers](https://github.com/Romanr92/Adaptive_JSON5/issues/68): J5-001, J5-002, J5-007, J5-091, J5-092, J5-093.
- [#69 — Complete advanced memory accounting and pure-memory/concurrent/embedded runtime](https://github.com/Romanr92/Adaptive_JSON5/issues/69): J5-042, J5-050, J5-052, J5-101, J5-040, J5-041, J5-045, J5-046, J5-047, J5-048, J5-049, J5-051, J5-063, J5-065, J5-066, J5-068, J5-098.
- [#70 — Finish the optional versioned consumer schema example](https://github.com/Romanr92/Adaptive_JSON5/issues/70): J5-083, J5-084, J5-085, J5-086.
- [#71 — Finish optional consumer numeric/string policies and result mapping](https://github.com/Romanr92/Adaptive_JSON5/issues/71): J5-087, J5-088, J5-089, J5-090.
- [#72 — Keep essential API, limits, provenance and truthful release evidence](https://github.com/Romanr92/Adaptive_JSON5/issues/72): J5-008, J5-099, J5-100.

#69 requires complete accounting/failure recovery across parser, exact operations,
owned results/diagnostics, full schema compilation/evaluation and bounded CLI
readers; a parser-only interim release cannot complete it. Stage-specific guards
land in #29 and operation-specific groups; #69 owns final cross-component evidence.

Editor and consumer features remain fully tracked and independently optional.
The selected parser/schema/CLI/accounting scope is approximately 25–45 working
days under the assumptions in the consolidation plan. Ten working days is a
checkpoint, not a supported full-completion commitment.
