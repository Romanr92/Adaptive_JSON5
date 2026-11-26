# CLI MISRA C++:2023 deviation register

The optional QM host tool uses the DEV-201 series. Each source marker identifies one statement and lists every applicable query. Locations below refer to the scan before marker insertion.

Status: all entries were **accepted** by the owner on 2026-11-27 (#57). Line numbers are those at the time of writing; the marker ID in the source is authoritative.

| ID | Subject | Lines |
| --- | --- | --- |
| DEV-201 | Bounded unsigned arithmetic | 45 |
| DEV-202 | Character values | 5 |
| DEV-203 | Aggregate initialization | 26 |
| DEV-204 | Special members | 16 |
| DEV-205 | Numeric conversions | 13 |
| DEV-206 | Namespace constants | 8 |
| DEV-207 | Host streams and formatting | 5 |
| DEV-208 | Host allocation | 2 |
| DEV-211 | Untyped host allocation | 1 |
| DEV-212 | Compile-time array bounds | 1 |
| DEV-213 | Transfer of a parse result | 1 |
| DEV-215 | Exception-disabled entry point | 1 |

## DEV-201: Bounded unsigned arithmetic

| Field | Value |
| --- | --- |
| CodeQL queries | `cpp/misra/unsigned-operation-with-constant-operands-wraps` |
| Lines | 45 |
| Status | Accepted by the owner 2026-11-27 (#57) |

### Why this rule is deviated instead of fixed

Indices, offsets and counters follow the enclosing sequence or storage bounds. Decimal accumulation uses a digit bound or an explicit range check.

### Risk and compensating measures

A wrapped index or allocation size could address the wrong byte. Depth allocation checks the size before multiplying by the frame size. Array indices, string offsets and counts follow their enclosing buffers. Signed decimal output deliberately computes the unsigned magnitude modulo 2^64.

### Lines

| ID | Location | Code | Rules | Status |
| --- | --- | --- | --- | --- |
| DEV-201.1 | `cli/src/cli_app.cpp:57` | `put(std::string_view{digits.data() + first, digits.size() - first});` | 8.20.1 | Accepted |
| DEV-201.2 | `cli/src/cli_app.cpp:53` | `--first;` | 8.20.1 | Accepted |
| DEV-201.3 | `cli/src/cli_options.cpp:369` | `index += 2U;` | 8.20.1 | Accepted |
| DEV-201.4 | `cli/src/cli_options.cpp:361` | `++index;` | 8.20.1 | Accepted |
| DEV-201.5 | `cli/src/cli_options.cpp:287` | `const std::string_view value{argv[index + 1U]};` | 8.20.1 | Accepted |
| DEV-201.6 | `cli/src/cli_options.cpp:151` | `value = (value * 10U) + digit;` | 8.20.1 | Accepted |
| DEV-201.7 | `cli/src/host_io.cpp:67` | `++index;` | 8.20.1 | Accepted |
| DEV-201.8 | `cli/src/inspect_command.cpp:552` | `host_buffer utf8{scratch, *size.value_if() + 1U};` | 8.20.1 | Accepted |
| DEV-201.9 | `cli/src/inspect_command.cpp:547` | `++index;` | 8.20.1 | Accepted |
| DEV-201.10 | `cli/src/inspect_command.cpp:485` | `++index;` | 8.20.1 | Accepted |
| DEV-201.11 | `cli/src/inspect_command.cpp:466` | `++depth;` | 8.20.1 | Accepted |
| DEV-201.12 | `cli/src/inspect_command.cpp:457` | `++top.count;` | 8.20.1 | Accepted |
| DEV-201.13 | `cli/src/inspect_command.cpp:413` | `static_cast<void>(std::memcpy(&frame, stack + (index * sizeof(walk_frame)), sizeof(walk_...` | 8.20.1, 24.5.2 | Accepted |
| DEV-201.14 | `cli/src/inspect_command.cpp:401` | `static_cast<void>(std::memcpy(stack + (index * sizeof(walk_frame)), &frame, sizeof(walk_...` | 8.20.1, 24.5.2, 8.7.1 | Accepted |
| DEV-201.15 | `cli/src/inspect_command.cpp:338` | `++level;` | 8.20.1 | Accepted |
| DEV-201.16 | `cli/src/inspect_command.cpp:142` | `out.put_count(static_cast<std::size_t>(0U - static_cast<std::uint64_t>(value)));` | 8.20.1 | Accepted |
| DEV-201.17 | `cli/src/report.cpp:334` | `++m_count;` | 8.20.1 | Accepted |
| DEV-201.18 | `cli/src/report.cpp:195` | `index += length;` | 8.20.1 | Accepted |
| DEV-201.19 | `cli/src/report.cpp:190` | `++index;` | 8.20.1 | Accepted |
| DEV-201.20 | `cli/src/report.cpp:177` | `++index;` | 8.20.1 | Accepted |
| DEV-201.21 | `cli/src/report.cpp:115` | `index += length;` | 8.20.1 | Accepted |
| DEV-201.22 | `cli/src/report.cpp:94` | `++offset;` | 8.20.1 | Accepted |
| DEV-201.23 | `cli/src/run_command.cpp:1207` | `: selected.manifest.substr(0U, last + 1U),` | 8.20.1 | Accepted |
| DEV-201.24 | `cli/src/run_command.cpp:1194` | `host_buffer results{host.storage, *count.value_if() + 1U};` | 8.20.1 | Accepted |
| DEV-201.25 | `cli/src/run_command.cpp:1155` | `rest = rest.substr(slash + 1U);` | 8.20.1 | Accepted |
| DEV-201.26 | `cli/src/run_command.cpp:1061` | `++index;` | 8.20.1 | Accepted |
| DEV-201.27 | `cli/src/run_command.cpp:1060` | `totals.errors += (result == case_result::error) ? 1U : 0U;` | 8.20.1 | Accepted |
| DEV-201.28 | `cli/src/run_command.cpp:1059` | `totals.unsupported += (result == case_result::unsupported) ? 1U : 0U;` | 8.20.1 | Accepted |
| DEV-201.29 | `cli/src/run_command.cpp:1058` | `totals.failed += (result == case_result::fail) ? 1U : 0U;` | 8.20.1 | Accepted |
| DEV-201.30 | `cli/src/run_command.cpp:1057` | `totals.passed += (result == case_result::pass) ? 1U : 0U;` | 8.20.1 | Accepted |
| DEV-201.31 | `cli/src/run_command.cpp:988` | `++written;` | 8.20.1 | Accepted |
| DEV-201.32 | `cli/src/run_command.cpp:938` | `++index;` | 8.20.1 | Accepted |
| DEV-201.33 | `cli/src/run_command.cpp:895` | `++index;` | 8.20.1 | Accepted |
| DEV-201.34 | `cli/src/run_command.cpp:727` | `++size;` | 8.20.1 | Accepted |
| DEV-201.35 | `cli/src/run_command.cpp:426` | `++index;` | 8.20.1 | Accepted |
| DEV-201.36 | `cli/src/run_command.cpp:317` | `decimal_text((v < 0) ? (0U - static_cast<std::uint64_t>(v)) : static_cast<std::uint64_t>...` | 8.20.1 | Accepted |
| DEV-201.37 | `cli/src/run_command.cpp:296` | `++out.size;` | 8.20.1 | Accepted |
| DEV-201.38 | `cli/src/run_command.cpp:282` | `--first;` | 8.20.1 | Accepted |
| DEV-201.39 | `cli/src/run_command.cpp:243` | `++context.mismatches;` | 8.20.1 | Accepted |
| DEV-201.40 | `cli/src/run_command.cpp:178` | `index = (index * 10U) + c_digits.find(c);` | 8.20.1 | Accepted |
| DEV-201.41 | `cli/src/run_command.cpp:155` | `++index;` | 8.20.1 | Accepted |
| DEV-201.42 | `cli/src/run_command.cpp:154` | `++out.size;` | 8.20.1 | Accepted |
| DEV-201.43 | `cli/src/run_command.cpp:147` | `++index;` | 8.20.1 | Accepted |
| DEV-201.44 | `cli/src/validate_command.cpp:289` | `++index;` | 8.20.1 | Accepted |
| DEV-201.45 | `cli/src/validate_command.cpp:99` | `++index;` | 8.20.1 | Accepted |

## DEV-202: Character values

| Field | Value |
| --- | --- |
| CodeQL queries | `cpp/misra/no-character-numerical-value` |
| Lines | 5 |
| Status | Accepted by the owner 2026-11-27 (#57) |

### Why this rule is deviated instead of fixed

Encoding and escaping use the numeric values of ASCII characters and Unicode code units.

### Risk and compensating measures

Numeric character handling could corrupt output. Diagnostic tests reparse strict JSON, including escaped text and non-ASCII input.

### Lines

| ID | Location | Code | Rules | Status |
| --- | --- | --- | --- | --- |
| DEV-202.1 | `cli/src/report.cpp:28` | `return static_cast<std::uint32_t>(static_cast<unsigned char>(text[index]));` | 7.0.3 | Accepted |
| DEV-202.2 | `cli/src/run_command.cpp:1136` | `if (pointer[0U] != '/')` | 7.0.3 | Accepted |
| DEV-202.3 | `cli/src/run_command.cpp:1056` | `results[index] = static_cast<char>(result);` | 7.0.3 | Accepted |
| DEV-202.4 | `cli/src/run_command.cpp:901` | `const auto result = static_cast<case_result>(stored);` | 7.0.3, 4.1.3 | Accepted |
| DEV-202.5 | `cli/src/run_command.cpp:146` | `c = (token[index + 1U] == '1') ? '/' : '~';` | 7.0.3 | Accepted |

## DEV-203: Aggregate initialization

| Field | Value |
| --- | --- |
| CodeQL queries | `cpp/misra/unsigned-integer-literals-not-appropriately-suffixed` |
| Lines | 26 |
| Status | Accepted by the owner 2026-11-27 (#57) |

### Why this rule is deviated instead of fixed

Aggregate initialization supplies the declared member defaults or value-initializes an unsigned member to zero.

### Risk and compensating measures

An incorrect implicit initial value could alter dispatch or diagnostics. The member declarations fix the types and defaults. CLI tests exercise default options and failure reports.

### Lines

| ID | Location | Code | Rules | Status |
| --- | --- | --- | --- | --- |
| DEV-203.1 | `cli/includes/cli/cli_options.hpp:94` | `argument_error failure{};` | 5.13.4 | Accepted |
| DEV-203.2 | `cli/src/cli_options.cpp:340` | `options parsed{};` | 5.13.4 | Accepted |
| DEV-203.3 | `cli/src/cli_options.cpp:316` | `parsed_arguments result{};` | 5.13.4 | Accepted |
| DEV-203.4 | `cli/src/cli_options.cpp:256` | `parsed_arguments result{};` | 5.13.4 | Accepted |
| DEV-203.5 | `cli/src/inspect_command.cpp:524` | `failures.add(diagnostic{severity::error, kind, message});` | 5.13.4 | Accepted |
| DEV-203.6 | `cli/src/inspect_command.cpp:412` | `walk_frame frame{};` | 5.13.4 | Accepted |
| DEV-203.7 | `cli/src/run_command.cpp:1202` | `case_context context{selected,` | 5.13.4 | Accepted |
| DEV-203.8 | `cli/src/run_command.cpp:1145` | `short_text token{};` | 5.13.4 | Accepted |
| DEV-203.9 | `cli/src/run_command.cpp:1050` | `run_totals totals{};` | 5.13.4 | Accepted |
| DEV-203.10 | `cli/src/run_command.cpp:1002` | `failures.add(diagnostic{severity::error, kind, message});` | 5.13.4 | Accepted |
| DEV-203.11 | `cli/src/run_command.cpp:928` | `short_text input{};` | 5.13.4 | Accepted |
| DEV-203.12 | `cli/src/run_command.cpp:896` | `const short_text path = item.has_value() ? text_of(*item.value_if(), "input") : short_te...` | 5.13.4 | Accepted |
| DEV-203.13 | `cli/src/run_command.cpp:595` | `short_text pointer{};` | 5.13.4 | Accepted |
| DEV-203.14 | `cli/src/run_command.cpp:439` | `short_text other{};` | 5.13.4 | Accepted |
| DEV-203.15 | `cli/src/run_command.cpp:383` | `short_text wanted{};` | 5.13.4 | Accepted |
| DEV-203.16 | `cli/src/run_command.cpp:349` | `short_text value{};` | 5.13.4 | Accepted |
| DEV-203.17 | `cli/src/run_command.cpp:102` | `short_text out{};` | 5.13.4 | Accepted |
| DEV-203.18 | `cli/src/validate_command.cpp:416` | `report.add(diagnostic{severity::error, category::io, describe_io(read)});` | 5.13.4 | Accepted |
| DEV-203.19 | `cli/src/validate_command.cpp:409` | `report.add(diagnostic{severity::error, category::allocation, "out of memory"});` | 5.13.4 | Accepted |
| DEV-203.20 | `cli/src/validate_command.cpp:286` | `report.add(diagnostic{severity::error, category::allocation, "out of memory"});` | 5.13.4 | Accepted |
| DEV-203.21 | `cli/src/validate_command.cpp:247` | `report.add(diagnostic{severity::error, category::io, describe_io(read)});` | 5.13.4 | Accepted |
| DEV-203.22 | `cli/src/validate_command.cpp:241` | `report.add(diagnostic{severity::error, category::allocation, "out of memory"});` | 5.13.4 | Accepted |
| DEV-203.23 | `cli/src/validate_command.cpp:210` | `diagnostic item{};` | 5.13.4 | Accepted |
| DEV-203.24 | `cli/src/validate_command.cpp:176` | `diagnostic item{};` | 5.13.4 | Accepted |
| DEV-203.25 | `cli/src/validate_command.cpp:109` | `diagnostic item{};` | 5.13.4 | Accepted |
| DEV-203.26 | `cli/src/validate_command.cpp:75` | `diagnostic item{};` | 5.13.4 | Accepted |

## DEV-204: Special members

| Field | Value |
| --- | --- |
| CodeQL queries | `cpp/misra/improperly-provided-special-member-functions-audit` |
| Lines | 16 |
| Status | Accepted by the owner 2026-11-27 (#57) |

### Why this rule is deviated instead of fixed

The type relies on its member types for copy, move and destruction, or explicitly restricts ownership transfer.

### Risk and compensating measures

Implicit copying could duplicate ownership. Review member ownership when changing these types. The allocation-failure sweeps exercise cleanup paths.

### Lines

| ID | Location | Code | Rules | Status |
| --- | --- | --- | --- | --- |
| DEV-204.1 | `cli/includes/cli/cli_app.hpp:24` | `class text_writer` | 15.0.1 | Accepted |
| DEV-204.2 | `cli/includes/cli/cli_io.hpp:67` | `struct environment` | 15.0.1 | Accepted |
| DEV-204.3 | `cli/includes/cli/cli_io.hpp:56` | `struct output_sink` | 15.0.1 | Accepted |
| DEV-204.4 | `cli/includes/cli/cli_io.hpp:32` | `struct input_source` | 15.0.1 | Accepted |
| DEV-204.5 | `cli/includes/cli/cli_options.hpp:91` | `struct parsed_arguments` | 15.0.1 | Accepted |
| DEV-204.6 | `cli/includes/cli/cli_options.hpp:80` | `struct argument_error` | 15.0.1 | Accepted |
| DEV-204.7 | `cli/includes/cli/cli_options.hpp:44` | `struct options` | 15.0.1 | Accepted |
| DEV-204.8 | `cli/includes/cli/report.hpp:101` | `class reporter` | 15.0.1 | Accepted |
| DEV-204.9 | `cli/includes/cli/report.hpp:70` | `struct diagnostic` | 15.0.1 | Accepted |
| DEV-204.10 | `cli/src/cli_options.cpp:44` | `struct option_spec` | 15.0.1 | Accepted |
| DEV-204.11 | `cli/src/inspect_command.cpp:167` | `class inspection` | 15.0.1 | Accepted |
| DEV-204.12 | `cli/src/inspect_command.cpp:46` | `struct walk_frame` | 15.0.1 | Accepted |
| DEV-204.13 | `cli/src/run_command.cpp:872` | `struct run_totals` | 15.0.1 | Accepted |
| DEV-204.14 | `cli/src/run_command.cpp:644` | `struct observation` | 15.0.1 | Accepted |
| DEV-204.15 | `cli/src/run_command.cpp:205` | `struct case_context` | 15.0.1 | Accepted |
| DEV-204.16 | `cli/src/run_command.cpp:48` | `struct short_text` | 15.0.1 | Accepted |

## DEV-205: Numeric conversions

| Field | Value |
| --- | --- |
| CodeQL queries | `cpp/misra/numeric-assignment-type-mismatch` |
| Lines | 13 |
| Status | Accepted by the owner 2026-11-27 (#57) |

### Why this rule is deviated instead of fixed

The constant, index or code unit is representable in the destination. Standard stream macros expand to host runtime expressions. Two sites pass a fixed-width typedef to its own type (std::int64_t, std::int32_t); CodeQL on LP64 Linux reports the underlying long or int as a different type, so the CI scan finds them and the Windows scan does not.

### Risk and compensating measures

Narrowing could change a byte count or index. These sites pass representable constants, code units, stream handles or array offsets. The byte-oriented smoke fixtures check file input and emitted output.

### Lines

| ID | Location | Code | Rules | Status |
| --- | --- | --- | --- | --- |
| DEV-205.1 | `cli/src/host_io.cpp:88` | `return std::fwrite(text.data(), 1U, text.size(), target) == text.size();` | 7.0.6, 30.0.1 | Accepted |
| DEV-205.2 | `cli/src/host_io.cpp:87` | `std::FILE* target = (stream == output_stream::out) ? stdout : stderr;` | 7.0.6, 30.0.1 | Accepted |
| DEV-205.3 | `cli/src/host_io.cpp:56` | `return read_stream(stdin, buffer, capacity, size);` | 7.0.6 | Accepted |
| DEV-205.4 | `cli/src/host_io.cpp:36` | `if (std::fread(&extra, 1U, 1U, stream) == 1U)` | 7.0.6, 30.0.1 | Accepted |
| DEV-205.5 | `cli/src/host_io.cpp:28` | `size = std::fread(buffer, 1U, capacity, stream);` | 7.0.6, 30.0.1 | Accepted |
| DEV-205.6 | `cli/src/inspect_command.cpp:546` | `out.put_count(unit.has_value() ? *unit.value_if() : 0U);` | 7.0.6 | Accepted |
| DEV-205.7 | `cli/src/inspect_command.cpp:440` | `store_frame(stack.data(), 0U,` | 7.0.6 | Accepted |
| DEV-205.8 | `cli/src/report.cpp:176` | `out.put_count(byte_at(text, index));` | 7.0.6 | Accepted |
| DEV-205.9 | `cli/src/report.cpp:143` | `out.put(std::string_view{&c, 1U});` | 7.0.6 | Accepted |
| DEV-205.10 | `cli/src/report.cpp:138` | `const std::array<char, 6> escape{{'\\', 'u', '0', '0', c_hex[value >> 4U], c_hex[value &...` | 7.0.6 | Accepted |
| DEV-205.11 | `cli/src/report.cpp:55` | `return (lead < 0xF5U) ? 4U : 0U;` | 7.0.6 | Accepted |
| DEV-205.12 | `cli/src/main.cpp:32` | `return adaptive_json5::cli::run_cli(argc, argv, adaptive_json5::cli::host_environment());` | 7.0.6 | Accepted |
| DEV-205.13 | `cli/src/inspect_command.cpp:262` | `put_signed(m_out, *as_int.value_if());` | 7.0.6 | Accepted |

## DEV-206: Namespace constants

| Field | Value |
| --- | --- |
| CodeQL queries | `cpp/misra/redeclaration-of-static-constexpr-data-member` |
| Lines | 8 |
| Status | Accepted by the owner 2026-11-27 (#57) |

### Why this rule is deviated instead of fixed

This namespace-scope constant has one definition and is not a static data member.

### Risk and compensating measures

Multiple definitions could disagree. These definitions have namespace scope and internal linkage.

### Lines

| ID | Location | Code | Rules | Status |
| --- | --- | --- | --- | --- |
| DEV-206.1 | `cli/src/cli_app.cpp:73` | `constexpr std::string_view c_usage =` | 4.1.2 | Accepted |
| DEV-206.2 | `cli/src/cli_options.cpp:53` | `constexpr std::array<option_spec, 10> c_options{{` | 4.1.2 | Accepted |
| DEV-206.3 | `cli/src/cli_options.cpp:17` | `constexpr std::string_view c_digits{"0123456789"};` | 4.1.2 | Accepted |
| DEV-206.4 | `cli/src/report.cpp:18` | `constexpr std::string_view c_hex{"0123456789abcdef"};` | 4.1.2 | Accepted |
| DEV-206.5 | `cli/src/run_command.cpp:41` | `constexpr std::string_view c_digits{"0123456789"};` | 4.1.2 | Accepted |
| DEV-206.6 | `cli/src/validate_command.cpp:45` | `constexpr std::string_view c_schema_text = "on";` | 4.1.2 | Accepted |
| DEV-206.7 | `cli/src/validate_command.cpp:42` | `constexpr std::string_view c_exceptions_text = "off";` | 4.1.2 | Accepted |
| DEV-206.8 | `cli/src/validate_command.cpp:41` | `constexpr std::string_view c_exceptions_json = "false";` | 4.1.2 | Accepted |

## DEV-207: Host streams and formatting

| Field | Value |
| --- | --- |
| CodeQL queries | `cpp/misra/cstdio-functions-shall-not-be-used`, `cpp/misra/cstdio-types-shall-not-be-used` |
| Lines | 5 |
| Status | Accepted by the owner 2026-11-27 (#57) |

### Why this rule is deviated instead of fixed

The optional host adapter uses standard C streams for byte-oriented I/O. Finite doubles use bounded snprintf formatting with the return length checked.

### Risk and compensating measures

Short reads, write failures and truncation could lose data. File operations check their outcomes. A 40-byte buffer holds a finite binary64 value formatted with 17 significant digits in the initial C locale.

### Lines

| ID | Location | Code | Rules | Status |
| --- | --- | --- | --- | --- |
| DEV-207.1 | `cli/src/host_io.cpp:75` | `static_cast<void>(std::fclose(file));` | 30.0.1 | Accepted |
| DEV-207.2 | `cli/src/host_io.cpp:69` | `std::FILE* file = std::fopen(name.data(), "rb");` | 30.0.1, 30.0.1 | Accepted |
| DEV-207.3 | `cli/src/host_io.cpp:29` | `if (std::ferror(stream) != 0)` | 30.0.1 | Accepted |
| DEV-207.4 | `cli/src/host_io.cpp:26` | `io_status read_stream(std::FILE* stream, char* buffer, std::size_t capacity, std::size_t...` | 30.0.1 | Accepted |
| DEV-207.5 | `cli/src/inspect_command.cpp:158` | `const std::int32_t written = std::snprintf(text.data(), text.size(), "%.17g", value);` | 30.0.1 | Accepted |

## DEV-208: Host allocation

| Field | Value |
| --- | --- |
| CodeQL queries | `cpp/misra/dynamic-memory-should-not-be-used`, `cpp/misra/dynamic-memory-managed-manually` |
| Lines | 2 |
| Status | Accepted by the owner 2026-11-27 (#57) |

### Why this rule is deviated instead of fixed

The host storage callbacks pair malloc and free. Allocation failure is returned to the caller.

### Risk and compensating measures

Allocation failures and mismatched deallocation could leak host memory. Storage callbacks return explicit failure and pair each successful allocation with release. The failure sweep exercises cleanup.

### Lines

| ID | Location | Code | Rules | Status |
| --- | --- | --- | --- | --- |
| DEV-208.1 | `cli/src/host_io.cpp:112` | `std::free(address);` | 21.6.2, 21.6.1, 0.1.2 | Accepted |
| DEV-208.2 | `cli/src/host_io.cpp:99` | `void* address = std::malloc(bytes);` | 21.6.2, 21.6.1 | Accepted |

## DEV-211: Untyped host allocation

| Field | Value |
| --- | --- |
| CodeQL queries | `cpp/misra/int-to-pointer-cast-prohibited` |
| Lines | 1 |
| Status | Accepted by the owner 2026-11-27 (#57) |

### Why this rule is deviated instead of fixed

The storage callback returns an allocated void pointer. The host buffer accesses its bytes through char.

### Risk and compensating measures

Using an allocation with the wrong type could violate alignment or aliasing. The buffer exposes char storage for byte access and releases the original allocation through its callback.

### Lines

| ID | Location | Code | Rules | Status |
| --- | --- | --- | --- | --- |
| DEV-211.1 | `cli/includes/cli/host_buffer.hpp:37` | `m_data = static_cast<char*>(*block.value_if());` | 8.2.6 | Accepted |

## DEV-212: Compile-time array bounds

| Field | Value |
| --- | --- |
| CodeQL queries | `cpp/misra/unused-limited-visibility-variable` |
| Lines | 1 |
| Status | Accepted by the owner 2026-11-27 (#57) |

### Why this rule is deviated instead of fixed

The constant supplies a compile-time buffer extent.

### Risk and compensating measures

Removing the named bounds would obscure buffer capacities. They are referenced as array extents at compile time.

### Lines

| ID | Location | Code | Rules | Status |
| --- | --- | --- | --- | --- |
| DEV-212.1 | `cli/src/run_command.cpp:39` | `constexpr std::size_t c_text_capacity = 256U;` | 0.2.1 | Accepted |

## DEV-213: Transfer of a parse result

| Field | Value |
| --- | --- |
| CodeQL queries | `cpp/misra/object-used-while-in-potentially-moved-from-state` |
| Lines | 1 |
| Status | Accepted by the owner 2026-11-27 (#57) |

### Why this rule is deviated instead of fixed

The success discriminator is checked before the first and only move from the contained document.

### Risk and compensating measures

A second move could read an invalid payload. The success check dominates the sole move and subsequent processing uses the destination optional.

### Lines

| ID | Location | Code | Rules | Status |
| --- | --- | --- | --- | --- |
| DEV-213.1 | `cli/src/run_command.cpp:851` | `static_cast<void>(parsed.emplace(std::move(*parsed_result.value_if())));` | 28.6.3 | Accepted |

## DEV-215: Exception-disabled entry point

| Field | Value |
| --- | --- |
| CodeQL queries | `cpp/misra/missing-catch-all-exception-handler-in-main` |
| Lines | 1 |
| Status | Accepted by the owner 2026-11-27 (#57) |

### Why this rule is deviated instead of fixed

Exception-enabled builds wrap dispatch in catch (...). The scanned exception-disabled build dispatches through noexcept interfaces.

### Risk and compensating measures

An exception escaping main would bypass exit reporting. The exception-enabled build has a catch-all. The exception-disabled build calls noexcept interfaces.

### Lines

| ID | Location | Code | Rules | Status |
| --- | --- | --- | --- | --- |
| DEV-215.1 | `cli/src/main.cpp:12` | `int main(int argc, char** argv)` | 18.3.1 | Accepted |
