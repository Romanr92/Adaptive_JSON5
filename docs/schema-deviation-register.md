# Schema component MISRA C++:2023 deviation register

Deviations of the optional schema component (`AdaptiveJSON5::schema`, ADR 0006). The
component is QM and outside the ASIL B(D) scope of the
[parser deviation register](ISO26262-safety/deviation-register.md), so its deviations
are recorded here, with their own IDs (DEV-101 and up). Markers in the source use the same
format and are read by `tools/misra_check.py`.

Status: all entries were **accepted** by the owner on 2026-11-25; the rows added for issue #187 were accepted on 2026-11-27. Line numbers are those at
the time of writing; the marker ID in the source is authoritative.

| ID | Category | Lines |
| --- | --- | --- |
| DEV-101 | Rule 8.20.1, unsigned arithmetic that cannot wrap | 260 |
| DEV-102 | Rule 7.0.3, character comparisons and code-unit values | 37 |
| DEV-103 | Rule 5.13.4, empty-brace value-initialization and literals | 55 |
| DEV-104 | Rule 15.0.1, special member functions | 46 |
| DEV-105 | Rule 9.5.1, counting loops | 21 |
| DEV-106 | Rules 7.0.5 and 7.0.6, integral promotion and assignment | 22 |
| DEV-107 | Rule 8.2.6 and related, object pointers in caller-provided storage | 14 |
| DEV-108 | Rule 28.6.3, objects after a move | 11 |
| DEV-109 | Rules 15.0.2 and 18.4.1, noexcept special members | 7 |
| DEV-110 | Rules 21.2.2 and 24.5.2, std::memcpy of trivially copyable elements | 1 |
| DEV-111 | Rule 0.1.1, defensive initialization | 3 |
| DEV-112 | Rule 21.6.3, construction in caller-provided storage | 3 |
| DEV-113 | Rule 4.1.3, signed arithmetic bounded by saturation | 6 |
| DEV-114 | Rule 6.0.3/redeclaration query, namespace-scope constexpr constant | 13 |

## DEV-101: Rule 8.20.1, unsigned arithmetic that cannot wrap

| Field | Value |
| --- | --- |
| CodeQL queries | `cpp/misra/unsigned-operation-with-constant-operands-wraps` |
| Lines | 260 |
| Status | Accepted by the owner 2026-11-25 (#44, #48, #53; #187 accepted 2026-11-27) |

### Why this rule is deviated instead of fixed

Index, offset and count arithmetic on std::size_t is bounded by a preceding check, the loop condition or a container size, so it cannot wrap; CodeQL does not follow these run-time bounds.

### Risk and compensating measures

Unit tests, the schema fuzzer, the sanitizer runs and the allocation-failure and budget tests exercise these lines; each line keeps the justification next to it in the source.

### Lines

| ID | Location | Code | Rules | Status |
| --- | --- | --- | --- | --- |
| DEV-101.1 | `core/includes/schema/pod_vector.hpp:151` | `--m_size;` | 8.20.1 | Accepted |
| DEV-101.2 | `core/includes/schema/pod_vector.hpp:140` | `std::memcpy(static_cast<unsigned char*>(m_block.data()) + (index * sizeof(T)), &value, ...` | 8.20.1, 8.2.6, 24.5.2, 8.7.1 | Accepted |
| DEV-101.3 | `core/includes/schema/pod_vector.hpp:127` | `std::memcpy(&value, static_cast<const unsigned char*>(m_block.data()) + (index * sizeof...` | 8.20.1, 8.2.6, 24.5.2 | Accepted |
| DEV-101.4 | `core/includes/schema/pod_vector.hpp:113` | `++m_size;` | 8.20.1 | Accepted |
| DEV-101.5 | `core/includes/schema/pod_vector.hpp:112` | `std::memcpy(static_cast<unsigned char*>(m_block.data()) + (m_size * sizeof(T)), &value,...` | 8.20.1, 8.2.6, 24.5.2, 8.7.1 | Accepted |
| DEV-101.6 | `core/includes/schema/pod_vector.hpp:62` | `copy_bytes(block.value_if()->data(), m_block.data(), m_size * sizeof(T));` | 8.20.1, 24.5.2 | Accepted |
| DEV-101.9 | `core/src/schema/evaluator.cpp:1174` | `path.set(path.size() - 1U, top);` | 8.20.1 | Accepted |
| DEV-101.10 | `core/src/schema/evaluator.cpp:1173` | `++top.index;` | 8.20.1 | Accepted |
| DEV-101.11 | `core/src/schema/evaluator.cpp:1161` | `path_frame top = path.get(path.size() - 1U);` | 8.20.1 | Accepted |
| DEV-101.12 | `core/src/schema/evaluator.cpp:1064` | `offset + detail::escape_pointer_token(token, buffer + ((offset < capacity) ? offset : 0...` | 8.20.1 | Accepted |
| DEV-101.13 | `core/src/schema/evaluator.cpp:1031` | `return size_result::success(put_index(step.index - 1U, buffer, capacity, offset));` | 8.20.1 | Accepted |
| DEV-101.14 | `core/src/schema/evaluator.cpp:973` | `++depth;` | 8.20.1 | Accepted |
| DEV-101.15 | `core/src/schema/evaluator.cpp:943` | `++count;` | 8.20.1 | Accepted |
| DEV-101.16 | `core/src/schema/evaluator.cpp:924` | `offset += detail::escape_pointer_token(std::string_view{bytes.data(), length},` | 8.20.1 | Accepted |
| DEV-101.17 | `core/src/schema/evaluator.cpp:893` | `++index;` | 8.20.1 | Accepted |
| DEV-101.18 | `core/src/schema/evaluator.cpp:892` | `code_point = 0x10000U + ((code_point - 0xD800U) << 10U) + (low - 0xDC00U);` | 8.20.1 | Accepted |
| DEV-101.19 | `core/src/schema/evaluator.cpp:889` | `const std::uint32_t low = compiled_schema_access::unit(schema, units.first + index + 1U);` | 8.20.1, 7.0.6 | Accepted |
| DEV-101.20 | `core/src/schema/evaluator.cpp:886` | `std::uint32_t code_point = compiled_schema_access::unit(schema, units.first + index);` | 8.20.1, 7.0.6 | Accepted |
| DEV-101.21 | `core/src/schema/evaluator.cpp:870` | `++offset;` | 8.20.1 | Accepted |
| DEV-101.22 | `core/src/schema/evaluator.cpp:801` | `run.frames.set(run.frames.size() - 1U, current);` | 8.20.1 | Accepted |
| DEV-101.23 | `core/src/schema/evaluator.cpp:788` | `frame current = run.frames.get(run.frames.size() - 1U);` | 8.20.1 | Accepted |
| DEV-101.24 | `core/src/schema/evaluator.cpp:653` | `++owner.cursor;` | 8.20.1 | Accepted |
| DEV-101.25 | `core/src/schema/evaluator.cpp:468` | `return request(edge_at(run, node.one_of, owner.cursor - 1U), owner.instance, true, true);` | 8.20.1 | Accepted |
| DEV-101.26 | `core/src/schema/evaluator.cpp:467` | `++owner.cursor;` | 8.20.1 | Accepted |
| DEV-101.27 | `core/src/schema/evaluator.cpp:462` | `owner.passes += *delivered ? 1U : 0U;` | 8.20.1 | Accepted |
| DEV-101.28 | `core/src/schema/evaluator.cpp:442` | `return request(edge_at(run, node.any_of, owner.cursor - 1U), owner.instance, true, true);` | 8.20.1 | Accepted |
| DEV-101.29 | `core/src/schema/evaluator.cpp:441` | `++owner.cursor;` | 8.20.1 | Accepted |
| DEV-101.30 | `core/src/schema/evaluator.cpp:436` | `owner.passes += *delivered ? 1U : 0U;` | 8.20.1 | Accepted |
| DEV-101.31 | `core/src/schema/evaluator.cpp:417` | `return request(edge_at(run, node.all_of, owner.cursor - 1U), owner.instance, owner.supp...` | 8.20.1 | Accepted |
| DEV-101.32 | `core/src/schema/evaluator.cpp:416` | `++owner.cursor;` | 8.20.1 | Accepted |
| DEV-101.33 | `core/src/schema/evaluator.cpp:385` | `return compiled_schema_access::edge(run.schema, range.first + index);` | 8.20.1 | Accepted |
| DEV-101.34 | `core/src/schema/evaluator.cpp:348` | `const auto& edge = compiled_schema_access::property(run.schema, node.properties.first +...` | 8.20.1 | Accepted |
| DEV-101.35 | `core/src/schema/instance_domain.cpp:380` | `--m_size;` | 8.20.1 | Accepted |
| DEV-101.36 | `core/src/schema/instance_domain.cpp:368` | `void* const slot = static_cast<unsigned char*>(m_block.data()) + ((m_size - 1U) * sizeo...` | 8.20.1, 8.2.6 | Accepted |
| DEV-101.37 | `core/src/schema/instance_domain.cpp:356` | `++m_size;` | 8.20.1 | Accepted |
| DEV-101.38 | `core/src/schema/instance_domain.cpp:354` | `void* const slot = static_cast<unsigned char*>(m_block.data()) + (m_size * sizeof(stack...` | 8.20.1, 8.2.6 | Accepted |
| DEV-101.39 | `core/src/schema/instance_domain.cpp:108` | `++position;` | 8.20.1 | Accepted |
| DEV-101.40 | `core/src/schema/instance_domain.cpp:104` | `position += 2U;` | 8.20.1 | Accepted |
| DEV-101.41 | `core/src/schema/instance_domain.cpp:94` | `const auto next = text.unit_at(position + 1U);` | 8.20.1 | Accepted |
| DEV-101.42 | `core/src/schema/json_pointer.cpp:178` | `++needed;` | 8.20.1 | Accepted |
| DEV-101.43 | `core/src/schema/json_pointer.cpp:171` | `++needed;` | 8.20.1 | Accepted |
| DEV-101.44 | `core/src/schema/json_pointer.cpp:149` | `value = (value * 10U) + digit;` | 8.20.1 | Accepted |
| DEV-101.45 | `core/src/schema/json_pointer.cpp:67` | `return static_cast<char>(static_cast<unsigned char>((*high * 16U) + *low));` | 8.20.1, 7.0.3 | Accepted |
| DEV-101.46 | `core/src/schema/json_pointer.cpp:66` | `position += 3U;` | 8.20.1 | Accepted |
| DEV-101.47 | `core/src/schema/json_pointer.cpp:61` | `const auto low = hex_value(raw[position + 2U]);` | 8.20.1 | Accepted |
| DEV-101.48 | `core/src/schema/json_pointer.cpp:60` | `const auto high = hex_value(raw[position + 1U]);` | 8.20.1 | Accepted |
| DEV-101.49 | `core/src/schema/json_pointer.cpp:53` | `++position;` | 8.20.1 | Accepted |
| DEV-101.50 | `core/src/schema/json_pointer.cpp:36` | `return static_cast<unsigned>(c - 'A') + 10U;` | 8.20.1, 7.0.3 | Accepted |
| DEV-101.51 | `core/src/schema/json_pointer.cpp:32` | `return static_cast<unsigned>(c - 'a') + 10U;` | 8.20.1, 7.0.3 | Accepted |
| DEV-101.52 | `core/src/schema/schema_compiler.cpp:1405` | `total += detail::compiled_schema_access::node(*this, index).unknown_keywords;` | 8.20.1 | Accepted |
| DEV-101.53 | `core/src/schema/schema_compiler.cpp:1273` | `const pending_item item = state.pending.get(state.pending.size() - 1U);` | 8.20.1 | Accepted |
| DEV-101.54 | `core/src/schema/schema_compiler.cpp:1214` | `for (std::size_t hops = 0U; (target != c_no_node) && (hops <= state.refs.size()); ++hops)` | 8.20.1, 9.5.1 | Accepted |
| DEV-101.55 | `core/src/schema/schema_compiler.cpp:1138` | `const std::string_view fragment = (hash == std::string_view::npos) ? std::string_view{}...` | 8.20.1 | Accepted |
| DEV-101.56 | `core/src/schema/schema_compiler.cpp:1012` | `unit += 2U;` | 8.20.1 | Accepted |
| DEV-101.57 | `core/src/schema/schema_compiler.cpp:1004` | `const std::uint32_t offset = *code_point - 0x10000U;` | 8.20.1 | Accepted |
| DEV-101.58 | `core/src/schema/schema_compiler.cpp:1000` | `++unit;` | 8.20.1 | Accepted |
| DEV-101.59 | `core/src/schema/schema_compiler.cpp:973` | `position += extra + 1U;` | 8.20.1 | Accepted |
| DEV-101.60 | `core/src/schema/schema_compiler.cpp:966` | `const auto next = static_cast<unsigned char>(text[position + index]);` | 8.20.1, 7.0.3 | Accepted |
| DEV-101.61 | `core/src/schema/schema_compiler.cpp:897` | `node.child_count = state.nodes.size() - node.first_child;` | 8.20.1 | Accepted |
| DEV-101.62 | `core/src/schema/schema_compiler.cpp:582` | `++unknown;` | 8.20.1 | Accepted |
| DEV-101.63 | `core/src/schema/schema_compiler.cpp:502` | `properties.count = state.properties.size() - properties.first;` | 8.20.1 | Accepted |
| DEV-101.64 | `core/src/schema/schema_compiler.cpp:445` | `for (std::size_t position = 0U;; ++position)` | 8.20.1, 9.5.1 | Accepted |
| DEV-101.65 | `core/src/schema/schema_compiler.cpp:138` | `state.steps += amount;` | 8.20.1 | Accepted |
| DEV-101.66 | `core/src/schema/schema_registry.cpp:203` | `++m_size;` | 8.20.1 | Accepted |
| DEV-101.67 | `core/src/schema/schema_registry.cpp:146` | `--m_size;` | 8.20.1 | Accepted |
| DEV-101.68 | `core/src/schema/schema_registry.cpp:41` | `void* const address = static_cast<unsigned char*>(entries.data()) + (index * sizeof(reg...` | 8.20.1, 8.2.6 | Accepted |
| DEV-101.69 | `core/src/schema/schema_work.cpp:63` | `++counter;` | 8.20.1 | Accepted |
| DEV-101.70 | `core/src/schema/schema_work.cpp:41` | `++m_work_steps;` | 8.20.1 | Accepted |
| DEV-101.71 | `core/src/schema/uri.cpp:314` | `rest = rest.substr(scheme_end + 1U);` | 8.20.1 | Accepted |
| DEV-101.72 | `core/src/schema/uri.cpp:306` | `++scheme_end;` | 8.20.1 | Accepted |
| DEV-101.73 | `core/src/schema/uri.cpp:299` | `parts.query = rest.substr(question + 1U);` | 8.20.1 | Accepted |
| DEV-101.74 | `core/src/schema/uri.cpp:292` | `parts.fragment = rest.substr(hash + 1U);` | 8.20.1 | Accepted |
| DEV-101.75 | `core/src/schema/uri.cpp:189` | `const auto directory = append_text(merged, base.path.substr(0U, last + 1U));` | 8.20.1 | Accepted |
| DEV-101.76 | `core/src/schema/uri.cpp:82` | `const auto host = append_lower(out, authority.substr(host_start, host_end - host_start));` | 8.20.1 | Accepted |
| DEV-101.77 | `core/src/schema/uri.cpp:68` | `const std::size_t host_start = (at == std::string_view::npos) ? 0U : (at + 1U);` | 8.20.1 | Accepted |
| DEV-101.78 | `core/src/schema/value_equality.cpp:317` | `const value_pair pair = stack.get(stack.size() - 1U);` | 8.20.1 | Accepted |
| DEV-101.79 | `core/src/schema/pod_vector.cpp:35` | `return current * 2U;` | 8.20.1 | Accepted |
| DEV-101.80 | `core/src/schema/assertions.cpp:348` | `++index;` | 8.20.1 | Accepted |
| DEV-101.81 | `core/src/schema/assertions.cpp:347` | `count += low_half ? 0U : 1U;` | 8.20.1 | Accepted |
| DEV-101.82 | `core/src/schema/evaluator.cpp:987` | `++index;` | 8.20.1 | Accepted |
| DEV-101.83 | `core/src/schema/evaluator.cpp:941` | `++owner.cursor;` | 8.20.1 | Accepted |
| DEV-101.84 | `core/src/schema/evaluator.cpp:940` | `const auto& edge = compiled_schema_access::property(run.schema, node.dependency_schemas....` | 8.20.1 | Accepted |
| DEV-101.85 | `core/src/schema/evaluator.cpp:856` | `const auto& edge = compiled_schema_access::property(run.schema, node.pattern_properties....` | 8.20.1 | Accepted |
| DEV-101.86 | `core/src/schema/evaluator.cpp:841` | `++owner.member_target;` | 8.20.1 | Accepted |
| DEV-101.87 | `core/src/schema/evaluator.cpp:760` | `owner.passes += *delivered ? 1U : 0U;` | 8.20.1 | Accepted |
| DEV-101.89 | `core/src/schema/exact_number.cpp:172` | `const std::size_t digits = (parts.radix == 16U) ? (written * 2U) : written;` | 8.20.1 | Accepted |
| DEV-101.90 | `core/src/schema/exact_number.cpp:171` | `const std::size_t written = parts.integer.size() + parts.fraction.size();` | 8.20.1 | Accepted |
| DEV-101.91 | `core/src/schema/exact_number.cpp:120` | `parts.fraction = (point == std::string_view::npos) ? std::string_view{} : text.substr(po...` | 8.20.1 | Accepted |
| DEV-101.92 | `core/src/schema/exact_number.cpp:115` | `parts.exponent = read_exponent(text.substr(exponent + 1U));` | 8.20.1 | Accepted |
| DEV-101.93 | `core/src/schema/regex.cpp:1443` | `++position;` | 8.20.1 | Accepted |
| DEV-101.94 | `core/src/schema/regex.cpp:1424` | `return range_result::success(node_range{start, code.size() - start});` | 8.20.1 | Accepted |
| DEV-101.95 | `core/src/schema/regex.cpp:1395` | `++index;` | 8.20.1 | Accepted |
| DEV-101.96 | `core/src/schema/regex.cpp:1377` | `++moved;` | 8.20.1 | Accepted |
| DEV-101.97 | `core/src/schema/regex.cpp:1371` | `stepped = stepped.has_value() ? add_thread(state, state.next, state.program.first, posit...` | 8.20.1 | Accepted |
| DEV-101.98 | `core/src/schema/regex.cpp:1369` | `++index;` | 8.20.1 | Accepted |
| DEV-101.99 | `core/src/schema/regex.cpp:1367` | `stepped = add_thread(state, state.next, pc + 1U, position + 1U);` | 8.20.1 | Accepted |
| DEV-101.100 | `core/src/schema/regex.cpp:1357` | `++state.generation;` | 8.20.1 | Accepted |
| DEV-101.101 | `core/src/schema/regex.cpp:1352` | `const auto charged = work.regex_steps(state.current.size() + 1U);` | 8.20.1 | Accepted |
| DEV-101.102 | `core/src/schema/regex.cpp:1339` | `++index;` | 8.20.1 | Accepted |
| DEV-101.103 | `core/src/schema/regex.cpp:1337` | `const regex_range& range = tables.ranges[inst.first + index];` | 8.20.1 | Accepted |
| DEV-101.104 | `core/src/schema/regex.cpp:1308` | `added = state.pending.push_back(at + 1U);` | 8.20.1 | Accepted |
| DEV-101.105 | `core/src/schema/regex.cpp:1282` | `const std::size_t slot = at - state.program.first;` | 8.20.1 | Accepted |
| DEV-101.106 | `core/src/schema/regex.cpp:1280` | `const std::size_t at = state.pending.get(state.pending.size() - 1U);` | 8.20.1 | Accepted |
| DEV-101.107 | `core/src/schema/regex.cpp:1192` | `const emit_task task = state.tasks.get(state.tasks.size() - 1U);` | 8.20.1 | Accepted |
| DEV-101.108 | `core/src/schema/regex.cpp:1155` | `emit_task{task_kind::concat_step, task.node, task.index + 1U, 0U, 0U})` | 8.20.1 | Accepted |
| DEV-101.109 | `core/src/schema/regex.cpp:1154` | `emit_task{task_kind::emit, builder.children.get(node.first + task.index), 0U, 0U, 0U},` | 8.20.1 | Accepted |
| DEV-101.110 | `core/src/schema/regex.cpp:1132` | `++index;` | 8.20.1 | Accepted |
| DEV-101.111 | `core/src/schema/regex.cpp:1123` | `? state.tasks.push_back(emit_task{task_kind::alt_step, task.node, task.index + 1U, task....` | 8.20.1 | Accepted |
| DEV-101.112 | `core/src/schema/regex.cpp:1057` | `inst.first = split + 1U;` | 8.20.1 | Accepted |
| DEV-101.113 | `core/src/schema/regex.cpp:1049` | `const emit_task body{task_kind::emit, builder.children.get(node.first + task.index), 0U,...` | 8.20.1 | Accepted |
| DEV-101.114 | `core/src/schema/regex.cpp:1037` | `return push_tasks(state, body, emit_task{after, task.node, task.index + 1U, 0U, split});` | 8.20.1 | Accepted |
| DEV-101.115 | `core/src/schema/regex.cpp:1026` | `const std::size_t body_start = split + 1U;` | 8.20.1 | Accepted |
| DEV-101.116 | `core/src/schema/regex.cpp:1019` | `return push_tasks(state, body, emit_task{task_kind::repeat_step, task.node, task.index +...` | 8.20.1 | Accepted |
| DEV-101.117 | `core/src/schema/regex.cpp:853` | `++builder.position;` | 8.20.1 | Accepted |
| DEV-101.118 | `core/src/schema/regex.cpp:799` | `node.count = builder.ranges.size() - first_range;` | 8.20.1 | Accepted |
| DEV-101.119 | `core/src/schema/regex.cpp:756` | `builder.position += 2U;` | 8.20.1 | Accepted |
| DEV-101.120 | `core/src/schema/regex.cpp:731` | `++alt;` | 8.20.1 | Accepted |
| DEV-101.121 | `core/src/schema/regex.cpp:714` | `const group_context group = builder.groups.get(builder.groups.size() - 1U);` | 8.20.1 | Accepted |
| DEV-101.122 | `core/src/schema/regex.cpp:693` | `++term;` | 8.20.1 | Accepted |
| DEV-101.123 | `core/src/schema/regex.cpp:675` | `const group_context group = builder.groups.get(builder.groups.size() - 1U);` | 8.20.1 | Accepted |
| DEV-101.124 | `core/src/schema/regex.cpp:662` | `builder.terms.set(builder.terms.size() - 1U, index);` | 8.20.1 | Accepted |
| DEV-101.125 | `core/src/schema/regex.cpp:654` | `++builder.position;` | 8.20.1 | Accepted |
| DEV-101.126 | `core/src/schema/regex.cpp:648` | `node.first = builder.terms.get(builder.terms.size() - 1U);` | 8.20.1 | Accepted |
| DEV-101.127 | `core/src/schema/regex.cpp:622` | `++builder.position;` | 8.20.1 | Accepted |
| DEV-101.128 | `core/src/schema/regex.cpp:612` | `++builder.position;` | 8.20.1 | Accepted |
| DEV-101.129 | `core/src/schema/regex.cpp:592` | `++builder.position;` | 8.20.1 | Accepted |
| DEV-101.130 | `core/src/schema/regex.cpp:589` | `const std::size_t digit = static_cast<std::size_t>(builder.text.get(builder.position) - ...` | 8.20.1 | Accepted |
| DEV-101.131 | `core/src/schema/regex.cpp:573` | `node.count = builder.ranges.size() - node.first;` | 8.20.1 | Accepted |
| DEV-101.132 | `core/src/schema/regex.cpp:572` | `++builder.position;` | 8.20.1 | Accepted |
| DEV-101.133 | `core/src/schema/regex.cpp:556` | `++builder.position;` | 8.20.1 | Accepted |
| DEV-101.134 | `core/src/schema/regex.cpp:527` | `++builder.position;` | 8.20.1 | Accepted |
| DEV-101.135 | `core/src/schema/regex.cpp:491` | `++builder.position;` | 8.20.1 | Accepted |
| DEV-101.136 | `core/src/schema/regex.cpp:459` | `++builder.position;` | 8.20.1 | Accepted |
| DEV-101.137 | `core/src/schema/regex.cpp:440` | `builder.position += hex.has_value() ? digits : 0U;` | 8.20.1 | Accepted |
| DEV-101.138 | `core/src/schema/regex.cpp:362` | `++index;` | 8.20.1 | Accepted |
| DEV-101.139 | `core/src/schema/regex.cpp:361` | `value = (value * 16U) + *digit;` | 8.20.1 | Accepted |
| DEV-101.140 | `core/src/schema/regex.cpp:355` | `const std::size_t at = builder.position + index;` | 8.20.1 | Accepted |
| DEV-101.141 | `core/src/schema/regex.cpp:338` | `return (c - c_capital_a) + 10U;` | 8.20.1 | Accepted |
| DEV-101.142 | `core/src/schema/regex.cpp:334` | `return (c - c_small_a) + 10U;` | 8.20.1 | Accepted |
| DEV-101.143 | `core/src/schema/regex.cpp:293` | `next = range.high + 1U;` | 8.20.1 | Accepted |
| DEV-101.144 | `core/src/schema/regex.cpp:286` | `const regex_range piece = complement ? regex_range{next, range.low - 1U} : range;` | 8.20.1 | Accepted |
| DEV-101.145 | `core/src/schema/regex.cpp:251` | `++index;` | 8.20.1 | Accepted |
| DEV-101.146 | `core/src/schema/regex.cpp:243` | `++index;` | 8.20.1 | Accepted |
| DEV-101.147 | `core/src/schema/regex.cpp:242` | `value = 0x10000U + ((value - 0xD800U) << 10U) + (*low.value_if() - 0xDC00U);` | 8.20.1 | Accepted |
| DEV-101.148 | `core/src/schema/regex.cpp:239` | `const auto low = source.unit_at(index + 1U);` | 8.20.1 | Accepted |
| DEV-101.149 | `core/src/schema/regex.cpp:185` | `++builder.steps;` | 8.20.1 | Accepted |
| DEV-101.150 | `core/src/schema/schema_values.cpp:474` | `stack.set(stack.size() - 1U, frame);` | 8.20.1 | Accepted |
| DEV-101.151 | `core/src/schema/schema_values.cpp:463` | `copy_frame frame = stack.get(stack.size() - 1U);` | 8.20.1 | Accepted |
| DEV-101.152 | `core/src/schema/schema_values.cpp:240` | `++steps;` | 8.20.1 | Accepted |
| DEV-101.153 | `core/src/schema/schema_values.cpp:128` | `++index;` | 8.20.1 | Accepted |
| DEV-101.154 | `core/src/schema/schema_values.cpp:82` | `++index;` | 8.20.1 | Accepted |
| DEV-101.155 | `core/src/schema/schema_work.cpp:120` | `m_regex_steps += units;` | 8.20.1 | Accepted |
| DEV-101.156 | `core/src/schema/exact_number.cpp:420` | `built = reserve_digits(remainder, *modulus_digits + 1U);` | 8.20.1 | Accepted |
| DEV-101.157 | `core/src/schema/format.cpp:760` | `++index;` | 8.20.1 | Accepted |
| DEV-101.158 | `core/src/schema/format.cpp:642` | `++index;` | 8.20.1 | Accepted |
| DEV-101.159 | `core/src/schema/format.cpp:581` | `tilde = text.find('~', tilde + 1U);` | 8.20.1 | Accepted |
| DEV-101.160 | `core/src/schema/format.cpp:549` | `std::string_view rest = has_scheme ? text.substr(colon + 1U) : text;` | 8.20.1 | Accepted |
| DEV-101.161 | `core/src/schema/format.cpp:513` | `const std::string_view host_port = (at == std::string_view::npos) ? authority : authorit...` | 8.20.1 | Accepted |
| DEV-101.162 | `core/src/schema/format.cpp:494` | `percent = text.find('%', percent + 1U);` | 8.20.1 | Accepted |
| DEV-101.163 | `core/src/schema/format.cpp:489` | `const std::string_view escape = text.substr(percent + 1U, 2U);` | 8.20.1 | Accepted |
| DEV-101.164 | `core/src/schema/format.cpp:470` | `const std::string_view after = host_port.substr(close + 1U);` | 8.20.1 | Accepted |
| DEV-101.165 | `core/src/schema/format.cpp:468` | `const std::string_view literal = host_port.substr(1U, close - 1U);` | 8.20.1 | Accepted |
| DEV-101.166 | `core/src/schema/format.cpp:461` | `return (colon == std::string_view::npos) ? std::string_view{} : host_port.substr(colon +...` | 8.20.1 | Accepted |
| DEV-101.167 | `core/src/schema/format.cpp:419` | `const std::string_view literal = domain.substr(1U, domain.size() - 2U);` | 8.20.1 | Accepted |
| DEV-101.168 | `core/src/schema/format.cpp:379` | `const std::string_view inner = local.substr(1U, local.size() - 2U);` | 8.20.1 | Accepted |
| DEV-101.169 | `core/src/schema/format.cpp:357` | `const std::string_view right = text.substr(gap + 2U);` | 8.20.1 | Accepted |
| DEV-101.170 | `core/src/schema/format.cpp:339` | `rest = rest.substr(colon + 1U);` | 8.20.1 | Accepted |
| DEV-101.171 | `core/src/schema/format.cpp:334` | `++count;` | 8.20.1 | Accepted |
| DEV-101.172 | `core/src/schema/format.cpp:328` | `return count + 2U;` | 8.20.1 | Accepted |
| DEV-101.173 | `core/src/schema/format.cpp:301` | `rest = rest.substr(dot + 1U);` | 8.20.1 | Accepted |
| DEV-101.174 | `core/src/schema/format.cpp:269` | `rest = rest.substr(dot + 1U);` | 8.20.1 | Accepted |
| DEV-101.175 | `core/src/schema/format.cpp:205` | `positive ? (((local + c_day_minutes) - shift) % c_day_minutes) : ((local + shift) % c_da...` | 8.20.1 | Accepted |
| DEV-101.176 | `core/src/schema/format.cpp:200` | `shift = (*decimal(offset.substr(1U, 2U)) * 60U) + *decimal(offset.substr(4U, 2U));` | 8.20.1 | Accepted |
| DEV-101.177 | `core/src/schema/format.cpp:196` | `const std::uint32_t local = (*decimal(text.substr(0U, 2U)) * 60U) + *decimal(text.substr...` | 8.20.1 | Accepted |
| DEV-101.178 | `core/src/schema/format.cpp:124` | `return ((month == 2U) && leap) ? c_leap_february : c_days[month - 1U];` | 8.20.1, 7.0.6 | Accepted |
| DEV-101.179 | `core/src/schema/format.cpp:96` | `value = (value * 10U) + static_cast<std::uint32_t>(c_digits.find(c));` | 8.20.1 | Accepted |
| DEV-101.180 | `core/src/schema/schema_compiler.cpp:2163` | `++resource;` | 8.20.1 | Accepted |
| DEV-101.181 | `core/src/schema/schema_values.cpp:478` | `++copied;` | 8.20.1 | Accepted |
| DEV-101.182 | `core/src/schema/regex.cpp:935` | `const std::size_t at = builder.position + 1U;` | 8.20.1 | Accepted |
| DEV-101.183 | `core/src/schema/idna.cpp:460` | `n += i / points;` | 8.20.1 | Accepted |
| DEV-101.184 | `core/src/schema/idna.cpp:455` | `bias = adapt(i - old_i, points, old_i == 0U);` | 8.20.1 | Accepted |
| DEV-101.185 | `core/src/schema/idna.cpp:423` | `++index;` | 8.20.1 | Accepted |
| DEV-101.186 | `core/src/schema/idna.cpp:352` | `return script_at(label, count, index + 1U, "Greek");` | 8.20.1 | Accepted |
| DEV-101.187 | `core/src/schema/idna.cpp:282` | `++after;` | 8.20.1 | Accepted |
| DEV-101.188 | `core/src/schema/idna.cpp:279` | `std::size_t after = index + 1U;` | 8.20.1 | Accepted |
| DEV-101.189 | `core/src/schema/idna.cpp:232` | `++index;` | 8.20.1 | Accepted |
| DEV-101.190 | `core/src/schema/idna.cpp:212` | `++index;` | 8.20.1 | Accepted |
| DEV-101.191 | `core/src/schema/idna.cpp:133` | `k += c_base;` | 8.20.1 | Accepted |
| DEV-101.192 | `core/src/schema/idna.cpp:132` | `weight *= c_base - t;` | 8.20.1 | Accepted |
| DEV-101.193 | `core/src/schema/idna.cpp:122` | `i += *digit * weight;` | 8.20.1 | Accepted |
| DEV-101.194 | `core/src/schema/idna.cpp:121` | `++position;` | 8.20.1 | Accepted |
| DEV-101.195 | `core/src/schema/idna.cpp:85` | `return k + (((c_base - c_tmin + 1U) * scaled) / (scaled + c_skew));` | 8.20.1 | Accepted |
| DEV-101.196 | `core/src/schema/idna.cpp:83` | `k += c_base;` | 8.20.1 | Accepted |
| DEV-101.197 | `core/src/schema/idna.cpp:78` | `scaled += scaled / points;` | 8.20.1 | Accepted |
| DEV-101.198 | `core/src/schema/regex.cpp:889` | `: named_property(builder, text.substr(0U, equals), text.substr(equals + 1U));` | 8.20.1 | Accepted |
| DEV-101.199 | `core/src/schema/regex.cpp:867` | `++builder.position;` | 8.20.1 | Accepted |
| DEV-101.200 | `core/src/schema/regex.cpp:861` | `++builder.position;` | 8.20.1 | Accepted |
| DEV-101.201 | `core/src/schema/regex.cpp:851` | `++builder.position;` | 8.20.1 | Accepted |
| DEV-101.202 | `core/src/schema/regex.cpp:734` | `next = entry.last + 1U;` | 8.20.1 | Accepted |
| DEV-101.203 | `core/src/schema/regex.cpp:728` | `added = add_run(builder, first_range, next, entry.first - 1U);` | 8.20.1 | Accepted |
| DEV-101.204 | `core/src/schema/regex.cpp:521` | `--builder.position;` | 8.20.1 | Accepted |
| DEV-101.205 | `core/src/schema/regex.cpp:518` | `++builder.position;` | 8.20.1 | Accepted |
| DEV-101.206 | `core/src/schema/unicode_property.cpp:69` | `low = middle + 1U;` | 8.20.1 | Accepted |
| DEV-101.207 | `core/src/schema/unicode_property.cpp:61` | `const std::size_t middle = low + ((high - low) / 2U);` | 8.20.1 | Accepted |
| DEV-101.208 | `core/src/schema/unicode_property.cpp:37` | `low = middle + 1U;` | 8.20.1 | Accepted |
| DEV-101.209 | `core/src/schema/unicode_property.cpp:29` | `const std::size_t middle = low + ((high - low) / 2U);` | 8.20.1 | Accepted |
| DEV-101.210 | `core/src/schema/format.cpp:573` | `return is_mail_domain(text.substr(at + 1U));` | 8.20.1 | Accepted |
| DEV-101.211 | `core/src/schema/idna.cpp:767` | `rest = rest.substr(dot + 1U);` | 8.20.1 | Accepted |
| DEV-101.212 | `core/src/schema/idna.cpp:651` | `rest = rest.substr(dot + 1U);` | 8.20.1 | Accepted |
| DEV-101.213 | `core/src/schema/idna.cpp:645` | `++index;` | 8.20.1 | Accepted |
| DEV-101.214 | `core/src/schema/idna.cpp:618` | `++index;` | 8.20.1 | Accepted |
| DEV-101.215 | `core/src/schema/idna.cpp:527` | `++index;` | 8.20.1 | Accepted |
| DEV-101.216 | `core/src/schema/normalization.cpp:235` | `++index;` | 8.20.1 | Accepted |
| DEV-101.217 | `core/src/schema/normalization.cpp:220` | `++index;` | 8.20.1 | Accepted |
| DEV-101.218 | `core/src/schema/normalization.cpp:197` | `++index;` | 8.20.1 | Accepted |
| DEV-101.219 | `core/src/schema/normalization.cpp:195` | `++written;` | 8.20.1 | Accepted |
| DEV-101.220 | `core/src/schema/normalization.cpp:158` | `return first + (second - c_trailing_base);` | 8.20.1 | Accepted |
| DEV-101.221 | `core/src/schema/normalization.cpp:151` | `((((first - c_leading_base) * c_vowel_count) + (second - c_vowel_base)) * c_trailing_cou...` | 8.20.1 | Accepted |
| DEV-101.222 | `core/src/schema/normalization.cpp:150` | `return c_hangul_base +` | 8.20.1 | Accepted |
| DEV-101.223 | `core/src/schema/normalization.cpp:134` | `++index;` | 8.20.1 | Accepted |
| DEV-101.224 | `core/src/schema/normalization.cpp:57` | `const std::uint32_t index = syllable - c_hangul_base;` | 8.20.1 | Accepted |
| DEV-101.225 | `core/src/schema/unicode_property.cpp:256` | `low = middle + 1U;` | 8.20.1 | Accepted |
| DEV-101.226 | `core/src/schema/unicode_property.cpp:247` | `const std::size_t middle = low + ((high - low) / 2U);` | 8.20.1 | Accepted |
| DEV-101.227 | `core/src/schema/unicode_property.cpp:225` | `low = middle + 1U;` | 8.20.1 | Accepted |
| DEV-101.228 | `core/src/schema/unicode_property.cpp:217` | `const std::size_t middle = low + ((high - low) / 2U);` | 8.20.1 | Accepted |
| DEV-101.229 | `core/src/schema/format.cpp:953` | `const std::size_t next = index + 1U;` | 8.20.1 | Accepted |
| DEV-101.230 | `core/src/schema/format.cpp:973` | `const std::uint32_t offset = ((high - 0xD800U) << 10U) | (second - 0xDC00U);` | 8.20.1 | Accepted |
| DEV-101.231 | `core/src/schema/format.cpp:979` | `const std::uint32_t value = 0x10000U + offset;` | 8.20.1 | Accepted |
| DEV-101.232 | `core/src/schema/format.cpp:1076` | `index += point.units;` | 8.20.1 | Accepted |
| DEV-101.233 | `core/src/schema/uts46.cpp:53` | `const std::size_t middle = low + ((high - low) / 2U);` | 8.20.1 | Accepted |
| DEV-101.234 | `core/src/schema/uts46.cpp:66` | `low = middle + 1U;` | 8.20.1 | Accepted |
| DEV-101.235 | `core/src/schema/uts46.cpp:94` | `++state.size;` | 8.20.1 | Accepted |
| DEV-101.236 | `core/src/schema/uts46.cpp:136` | `++index;` | 8.20.1 | Accepted |
| DEV-101.237 | `core/src/schema/uts46.cpp:166` | `++index;` | 8.20.1 | Accepted |
| DEV-101.238 | `core/src/schema/uts46.cpp:193` | `++index;` | 8.20.1 | Accepted |
| DEV-101.239 | `core/src/schema/uts46.cpp:207` | `++index;` | 8.20.1 | Accepted |
| DEV-101.240 | `core/src/schema/uts46.cpp:256` | `++state.count;` | 8.20.1 | Accepted |
| DEV-101.241 | `core/src/schema/uts46.cpp:293` | `++index;` | 8.20.1 | Accepted |
| DEV-101.242 | `core/src/schema/uts46.cpp:319` | `++index;` | 8.20.1 | Accepted |
| DEV-101.243 | `core/src/schema/idna.cpp:273` | `++encoder.size;` | 8.20.1 | Accepted |
| DEV-101.244 | `core/src/schema/idna.cpp:331` | `++basic;` | 8.20.1 | Accepted |
| DEV-101.245 | `core/src/schema/idna.cpp:338` | `++index;` | 8.20.1 | Accepted |
| DEV-101.246 | `core/src/schema/idna.cpp:368` | `++index;` | 8.20.1 | Accepted |
| DEV-101.247 | `core/src/schema/idna.cpp:399` | `q = (q - t) / (c_base - t);` | 8.20.1 | Accepted |
| DEV-101.248 | `core/src/schema/idna.cpp:405` | `k += c_base;` | 8.20.1 | Accepted |
| DEV-101.249 | `core/src/schema/idna.cpp:428` | `++encoder.delta;` | 8.20.1 | Accepted |
| DEV-101.250 | `core/src/schema/idna.cpp:441` | `const auto points = static_cast<std::uint32_t>(encoder.handled + 1U);` | 8.20.1 | Accepted |
| DEV-101.251 | `core/src/schema/idna.cpp:449` | `++encoder.handled;` | 8.20.1 | Accepted |
| DEV-101.252 | `core/src/schema/idna.cpp:460` | `++index;` | 8.20.1 | Accepted |
| DEV-101.253 | `core/src/schema/idna.cpp:495` | `++index;` | 8.20.1 | Accepted |
| DEV-101.254 | `core/src/schema/idna.cpp:1056` | `const auto points = static_cast<std::uint32_t>(encoder.handled + 1U);` | 8.20.1 | Accepted |
| DEV-101.255 | `core/src/schema/idna.cpp:1062` | `encoder.delta += (smallest - encoder.n) * points;` | 8.20.1 | Accepted |
| DEV-101.256 | `core/src/schema/idna.cpp:1073` | `++encoder.delta;` | 8.20.1 | Accepted |
| DEV-101.257 | `core/src/schema/idna.cpp:1079` | `++encoder.n;` | 8.20.1 | Accepted |
| DEV-101.258 | `core/src/schema/normalization.cpp:307` | `++index;` | 8.20.1 | Accepted |
| DEV-101.259 | `core/src/schema/normalization.cpp:320` | `++index;` | 8.20.1 | Accepted |
| DEV-101.260 | `core/src/schema/format.cpp:1109` | `index += point.units;` | 8.20.1 | Accepted |
| DEV-101.261 | `core/src/schema/format.cpp:1181` | `octets += utf8_octets(code_point);` | 8.20.1 | Accepted |
| DEV-101.262 | `core/src/schema/format.cpp:1192` | `++index;` | 8.20.1 | Accepted |
| DEV-101.263 | `core/src/schema/format.cpp:1222` | `return is_idn_domain(code_points, index + 1U, code_points.size(), true);` | 8.20.1 | Accepted |

## DEV-102: Rule 7.0.3, character comparisons and code-unit values

| Field | Value |
| --- | --- |
| CodeQL queries | `cpp/misra/no-character-numerical-value` |
| Lines | 37 |
| Status | Accepted by the owner 2026-11-25 (#44, #48; #187 accepted 2026-11-27) |

### Why this rule is deviated instead of fixed

URI, JSON Pointer and UTF-8/UTF-16 handling compares and computes ASCII characters and code units by value, as RFC 3986, RFC 6901 and Unicode define them.

### Risk and compensating measures

Unit tests, the schema fuzzer, the sanitizer runs and the allocation-failure and budget tests exercise these lines; each line keeps the justification next to it in the source.

### Lines

| ID | Location | Code | Rules | Status |
| --- | --- | --- | --- | --- |
| DEV-102.1 | `core/src/schema/evaluator.cpp:942` | `digits[count] = static_cast<char>('0' + static_cast<int>(value % 10U));` | 7.0.3 | Accepted |
| DEV-102.2 | `core/src/schema/evaluator.cpp:920` | `bytes[3] = static_cast<char>(0x80U \| (code_point & 0x3FU));` | 7.0.3 | Accepted |
| DEV-102.3 | `core/src/schema/evaluator.cpp:919` | `bytes[2] = static_cast<char>(0x80U \| ((code_point >> 6U) & 0x3FU));` | 7.0.3 | Accepted |
| DEV-102.4 | `core/src/schema/evaluator.cpp:918` | `bytes[1] = static_cast<char>(0x80U \| ((code_point >> 12U) & 0x3FU));` | 7.0.3 | Accepted |
| DEV-102.5 | `core/src/schema/evaluator.cpp:917` | `bytes[0] = static_cast<char>(0xF0U \| (code_point >> 18U));` | 7.0.3 | Accepted |
| DEV-102.6 | `core/src/schema/evaluator.cpp:912` | `bytes[2] = static_cast<char>(0x80U \| (code_point & 0x3FU));` | 7.0.3 | Accepted |
| DEV-102.7 | `core/src/schema/evaluator.cpp:911` | `bytes[1] = static_cast<char>(0x80U \| ((code_point >> 6U) & 0x3FU));` | 7.0.3 | Accepted |
| DEV-102.8 | `core/src/schema/evaluator.cpp:910` | `bytes[0] = static_cast<char>(0xE0U \| (code_point >> 12U));` | 7.0.3 | Accepted |
| DEV-102.9 | `core/src/schema/evaluator.cpp:905` | `bytes[1] = static_cast<char>(0x80U \| (code_point & 0x3FU));` | 7.0.3 | Accepted |
| DEV-102.10 | `core/src/schema/evaluator.cpp:904` | `bytes[0] = static_cast<char>(0xC0U \| (code_point >> 6U));` | 7.0.3 | Accepted |
| DEV-102.11 | `core/src/schema/evaluator.cpp:900` | `bytes[0] = static_cast<char>(code_point);` | 7.0.3 | Accepted |
| DEV-102.12 | `core/src/schema/json_pointer.cpp:144` | `const auto digit = static_cast<std::size_t>(c - '0');` | 7.0.3 | Accepted |
| DEV-102.13 | `core/src/schema/json_pointer.cpp:140` | `if ((c < '0') \|\| (c > '9'))` | 7.0.3 | Accepted |
| DEV-102.14 | `core/src/schema/json_pointer.cpp:133` | `if (token.empty() \|\| ((token.size() > 1U) && (token[0U] == '0')))` | 7.0.3 | Accepted |
| DEV-102.15 | `core/src/schema/json_pointer.cpp:114` | `const char decoded = tilde ? ((*c == '0') ? '~' : '/') : *c;` | 7.0.3 | Accepted |
| DEV-102.16 | `core/src/schema/json_pointer.cpp:109` | `if (!tilde && (*c == '~'))` | 7.0.3 | Accepted |
| DEV-102.17 | `core/src/schema/json_pointer.cpp:105` | `if (!c.has_value() \|\| (tilde && (*c != '0') && (*c != '1')))` | 7.0.3 | Accepted |
| DEV-102.18 | `core/src/schema/json_pointer.cpp:91` | `if (rest.empty() \|\| (rest[0U] != '/'))` | 7.0.3 | Accepted |
| DEV-102.19 | `core/src/schema/json_pointer.cpp:34` | `if ((c >= 'A') && (c <= 'F'))` | 7.0.3 | Accepted |
| DEV-102.20 | `core/src/schema/json_pointer.cpp:30` | `if ((c >= 'a') && (c <= 'f'))` | 7.0.3 | Accepted |
| DEV-102.21 | `core/src/schema/json_pointer.cpp:28` | `return static_cast<unsigned>(c - '0');` | 7.0.3 | Accepted |
| DEV-102.22 | `core/src/schema/json_pointer.cpp:26` | `if ((c >= '0') && (c <= '9'))` | 7.0.3 | Accepted |
| DEV-102.23 | `core/src/schema/schema_compiler.cpp:1143` | `if (fragment[0U] != '/')` | 7.0.3 | Accepted |
| DEV-102.24 | `core/src/schema/schema_compiler.cpp:934` | `const auto lead = static_cast<unsigned char>(text[position]);` | 7.0.3 | Accepted |
| DEV-102.25 | `core/src/schema/schema_compiler.cpp:415` | `if ((hash != std::string_view::npos) && ((hash + 1U) < target.size()) && (target[hash +...` | 7.0.3 | Accepted |
| DEV-102.26 | `core/src/schema/schema_compiler.cpp:388` | `parts.has_fragment && !parts.fragment.empty() && (parts.fragment[0U] != '/');` | 7.0.3 | Accepted |
| DEV-102.27 | `core/src/schema/schema_compiler.cpp:204` | `if (!unit.has_value() \|\| (*unit.value_if() != static_cast<std::uint16_t>(keyword[inde...` | 7.0.3, 7.0.5 | Accepted |
| DEV-102.28 | `core/src/schema/uri.cpp:310` | `if (letter_first && (scheme_end < rest.size()) && (rest[scheme_end] == ':'))` | 7.0.3 | Accepted |
| DEV-102.29 | `core/src/schema/uri.cpp:309` | `!rest.empty() && (((rest[0U] >= 'a') && (rest[0U] <= 'z')) \|\| ((rest[0U] >= 'A') && (...` | 7.0.3 | Accepted |
| DEV-102.30 | `core/src/schema/uri.cpp:261` | `if (r.has_authority \|\| (!r.path.empty() && (r.path[0U] == '/')))` | 7.0.3 | Accepted |
| DEV-102.31 | `core/src/schema/uri.cpp:37` | `return ((c >= 'A') && (c <= 'Z')) ? static_cast<char>(c - 'A' + 'a') : c;` | 7.0.3 | Accepted |
| DEV-102.32 | `core/src/schema/uri.cpp:25` | `return ((c >= 'a') && (c <= 'z')) \|\| ((c >= 'A') && (c <= 'Z')) \|\| ((c >= '0') && (...` | 7.0.3 | Accepted |
| DEV-102.33 | `core/src/schema/schema_values.cpp:81` | `equal = unit.has_value() && (*unit.value_if() == static_cast<std::uint16_t>(entry.first[...` | 7.0.3, 7.0.5 | Accepted |
| DEV-102.34 | `core/src/schema/idna.cpp:177` | `output[index] = static_cast<std::uint32_t>(static_cast<unsigned char>(input[index]));` | 7.0.3 | Accepted |
| DEV-102.35 | `core/src/schema/idna.cpp:53` | `const auto unit = static_cast<std::uint32_t>(static_cast<unsigned char>(c));` | 7.0.3 | Accepted |
| DEV-102.36 | `core/src/schema/regex.cpp:859` | `name[length] = static_cast<char>(c);` | 7.0.3 | Accepted |
| DEV-102.37 | `core/src/schema/idna.cpp:526` | `output[index] = static_cast<std::uint32_t>(static_cast<unsigned char>(label[index]));` | 7.0.3 | Accepted |

## DEV-103: Rule 5.13.4, empty-brace value-initialization and literals

| Field | Value |
| --- | --- |
| CodeQL queries | `cpp/misra/unsigned-integer-literals-not-appropriately-suffixed` |
| Lines | 55 |
| Status | Accepted by the owner 2026-11-25 (#44, #48; #187 accepted 2026-11-27) |

### Why this rule is deviated instead of fixed

Empty-brace value-initialization (`{}`) or a literal whose type is fixed by its context; the value is zero or the stated constant and no conversion changes it.

### Risk and compensating measures

Unit tests, the schema fuzzer, the sanitizer runs and the allocation-failure and budget tests exercise these lines; each line keeps the justification next to it in the source.

### Lines

| ID | Location | Code | Rules | Status |
| --- | --- | --- | --- | --- |
| DEV-103.1 | `core/includes/schema/schema_graph.hpp:88` | `node_range name{};` | 5.13.4 | Accepted |
| DEV-103.2 | `core/includes/schema/schema_graph.hpp:72` | `node_range properties{};` | 5.13.4 | Accepted |
| DEV-103.3 | `core/includes/schema/schema_graph.hpp:71` | `node_range items_array{};` | 5.13.4 | Accepted |
| DEV-103.4 | `core/includes/schema/schema_graph.hpp:70` | `node_range one_of{};` | 5.13.4 | Accepted |
| DEV-103.5 | `core/includes/schema/schema_graph.hpp:69` | `node_range any_of{};` | 5.13.4 | Accepted |
| DEV-103.6 | `core/includes/schema/schema_graph.hpp:68` | `node_range all_of{};` | 5.13.4 | Accepted |
| DEV-103.7 | `core/includes/schema/schema_graph.hpp:63` | `source_span span{};` | 5.13.4 | Accepted |
| DEV-103.8 | `core/includes/schema/schema_graph.hpp:60` | `node_range step_name{};` | 5.13.4 | Accepted |
| DEV-103.9 | `core/src/schema/evaluator.cpp:1123` | `path_frame frame{};` | 5.13.4 | Accepted |
| DEV-103.10 | `core/src/schema/evaluator.cpp:755` | `frame next{};` | 5.13.4 | Accepted |
| DEV-103.11 | `core/src/schema/evaluator.cpp:200` | `schema_diagnostic entry{};` | 5.13.4 | Accepted |
| DEV-103.12 | `core/src/schema/schema_compiler.cpp:1257` | `node_range base{};` | 5.13.4 | Accepted |
| DEV-103.13 | `core/src/schema/schema_compiler.cpp:1249` | `node.span = span.value_or(source_span{});` | 5.13.4 | Accepted |
| DEV-103.14 | `core/src/schema/schema_compiler.cpp:1245` | `schema_node node{};` | 5.13.4 | Accepted |
| DEV-103.15 | `core/src/schema/schema_compiler.cpp:801` | `node_range range{};` | 5.13.4 | Accepted |
| DEV-103.16 | `core/src/schema/schema_compiler.cpp:761` | `*entry.second = evaluate ? range : node_range{};` | 5.13.4 | Accepted |
| DEV-103.17 | `core/src/schema/schema_compiler.cpp:755` | `node_range range{};` | 5.13.4 | Accepted |
| DEV-103.18 | `core/src/schema/schema_compiler.cpp:730` | `const auto added = add_child(state, item, schema_keyword::items, step_kind::none, node_...` | 5.13.4, 7.0.6 | Accepted |
| DEV-103.19 | `core/src/schema/schema_compiler.cpp:726` | `node.items_array = evaluate ? range : node_range{};` | 5.13.4 | Accepted |
| DEV-103.20 | `core/src/schema/schema_compiler.cpp:724` | `node_range range{};` | 5.13.4 | Accepted |
| DEV-103.21 | `core/src/schema/schema_compiler.cpp:699` | `entry.span = span.value_or(source_span{});` | 5.13.4 | Accepted |
| DEV-103.22 | `core/src/schema/schema_compiler.cpp:694` | `ref_entry entry{};` | 5.13.4 | Accepted |
| DEV-103.23 | `core/src/schema/schema_compiler.cpp:634` | `const auto added = add_child(state, item, keyword, step_kind::none, node_range{}, 0U, *...` | 5.13.4, 7.0.6 | Accepted |
| DEV-103.24 | `core/src/schema/schema_compiler.cpp:515` | `node_range units{};` | 5.13.4 | Accepted |
| DEV-103.25 | `core/src/schema/schema_compiler.cpp:458` | `add_child(state, parent, keyword, step_kind::index, node_range{}, position, **element.v...` | 5.13.4 | Accepted |
| DEV-103.26 | `core/src/schema/schema_compiler.cpp:363` | `child.span = span.value_or(source_span{});` | 5.13.4 | Accepted |
| DEV-103.27 | `core/src/schema/schema_compiler.cpp:354` | `schema_node child{};` | 5.13.4 | Accepted |
| DEV-103.28 | `core/src/schema/schema_compiler.cpp:319` | `key_entry entry{};` | 5.13.4 | Accepted |
| DEV-103.29 | `core/src/schema/schema_compiler.cpp:78` | `node_range base{};` | 5.13.4 | Accepted |
| DEV-103.30 | `core/src/schema/schema_compiler.cpp:65` | `source_span span{};` | 5.13.4 | Accepted |
| DEV-103.31 | `core/src/schema/schema_compiler.cpp:64` | `node_range base{};` | 5.13.4 | Accepted |
| DEV-103.32 | `core/src/schema/schema_compiler.cpp:63` | `node_range text{};` | 5.13.4 | Accepted |
| DEV-103.33 | `core/src/schema/schema_compiler.cpp:51` | `node_range text{};` | 5.13.4 | Accepted |
| DEV-103.34 | `core/src/schema/uri.cpp:350` | `uri_parts t{};` | 5.13.4 | Accepted |
| DEV-103.35 | `core/src/schema/uri.cpp:285` | `uri_parts parts{};` | 5.13.4 | Accepted |
| DEV-103.36 | `includes/adaptive_json5/schema/schema_error.hpp:155` | `: m_parser_error(parser_error), m_range(range.value_or(source_span{})), m_code(code),` | 5.13.4 | Accepted |
| DEV-103.37 | `includes/adaptive_json5/schema/validation_report.hpp:35` | `source_span schema_span{};` | 5.13.4 | Accepted |
| DEV-103.38 | `includes/adaptive_json5/schema/validation_report.hpp:31` | `source_span instance_span{};` | 5.13.4 | Accepted |
| DEV-103.39 | `core/src/schema/exact_number.cpp:105` | `decimal_parts parts{};` | 5.13.4 | Accepted |
| DEV-103.40 | `core/src/schema/regex.cpp:1200` | `regex_inst done{};` | 5.13.4 | Accepted |
| DEV-103.41 | `core/src/schema/regex.cpp:1166` | `regex_inst inst{};` | 5.13.4 | Accepted |
| DEV-103.42 | `core/src/schema/regex.cpp:1117` | `regex_inst inst{};` | 5.13.4 | Accepted |
| DEV-103.43 | `core/src/schema/regex.cpp:1075` | `regex_inst inst{};` | 5.13.4 | Accepted |
| DEV-103.44 | `core/src/schema/regex.cpp:1055` | `regex_inst inst{};` | 5.13.4 | Accepted |
| DEV-103.45 | `core/src/schema/regex.cpp:1027` | `regex_inst inst{};` | 5.13.4 | Accepted |
| DEV-103.46 | `core/src/schema/regex.cpp:896` | `auto parsed = builder.groups.push_back(group_context{});` | 5.13.4 | Accepted |
| DEV-103.47 | `core/src/schema/regex.cpp:854` | `ast_node node{};` | 5.13.4 | Accepted |
| DEV-103.48 | `core/src/schema/regex.cpp:838` | `ast_node node{};` | 5.13.4 | Accepted |
| DEV-103.49 | `core/src/schema/regex.cpp:794` | `ast_node node{};` | 5.13.4 | Accepted |
| DEV-103.50 | `core/src/schema/regex.cpp:723` | `ast_node node{};` | 5.13.4 | Accepted |
| DEV-103.51 | `core/src/schema/regex.cpp:685` | `ast_node node{};` | 5.13.4 | Accepted |
| DEV-103.52 | `core/src/schema/regex.cpp:646` | `ast_node node{};` | 5.13.4 | Accepted |
| DEV-103.53 | `core/src/schema/regex.cpp:550` | `ast_node node{};` | 5.13.4 | Accepted |
| DEV-103.54 | `core/src/schema/regex.cpp:815` | `property_selector selector{};` | 5.13.4 | Accepted |
| DEV-103.55 | `core/src/schema/regex.cpp:779` | `property_selector selector{};` | 5.13.4 | Accepted |

## DEV-104: Rule 15.0.1, special member functions

| Field | Value |
| --- | --- |
| CodeQL queries | `cpp/misra/improperly-provided-special-member-functions-audit`, `cpp/misra/improperly-provided-special-member-functions`, `cpp/misra/implicit-declaration-of-copy-constructor-audit` |
| Lines | 46 |
| Status | Accepted by the owner 2026-11-25 (#44, #48; #187 accepted 2026-11-27) |

### Why this rule is deviated instead of fixed

Aggregates and owning types follow the rule of zero or declare exactly the move-only set they need; the implicitly provided members are the intended ones.

### Risk and compensating measures

Unit tests, the schema fuzzer, the sanitizer runs and the allocation-failure and budget tests exercise these lines; each line keeps the justification next to it in the source.

### Lines

| ID | Location | Code | Rules | Status |
| --- | --- | --- | --- | --- |
| DEV-104.1 | `core/includes/schema/pod_vector.hpp:25` | `class [[nodiscard]] pod_vector` | 15.0.1, 4.1.2 | Accepted |
| DEV-104.2 | `core/includes/schema/schema_graph.hpp:86` | `struct property_edge` | 15.0.1 | Accepted |
| DEV-104.3 | `core/includes/schema/schema_graph.hpp:53` | `struct schema_node` | 15.0.1 | Accepted |
| DEV-104.4 | `core/includes/schema/schema_graph.hpp:39` | `struct node_range` | 15.0.1 | Accepted |
| DEV-104.5 | `core/includes/schema/schema_registry_access.hpp:18` | `struct registry_entry` | 15.0.1 | Accepted |
| DEV-104.6 | `core/includes/schema/schema_work.hpp:20` | `class [[nodiscard]] schema_work` | 15.0.1 | Accepted |
| DEV-104.7 | `core/includes/schema/uri.hpp:19` | `struct uri_parts` | 15.0.1 | Accepted |
| DEV-104.8 | `core/src/schema/evaluator.cpp:1010` | `struct path_frame` | 15.0.1 | Accepted |
| DEV-104.9 | `core/src/schema/evaluator.cpp:280` | `struct advance_outcome` | 15.0.1 | Accepted |
| DEV-104.10 | `core/src/schema/evaluator.cpp:130` | `struct evaluator` | 15.0.1 | Accepted |
| DEV-104.11 | `core/src/schema/evaluator.cpp:116` | `struct child_request` | 15.0.1 | Accepted |
| DEV-104.12 | `core/src/schema/evaluator.cpp:95` | `struct frame` | 15.0.1 | Accepted |
| DEV-104.13 | `core/src/schema/instance_domain.cpp:37` | `struct stack_frame` | 15.0.1 | Accepted |
| DEV-104.14 | `core/src/schema/schema_compiler.cpp:89` | `struct compile_state` | 15.0.1 | Accepted |
| DEV-104.15 | `core/src/schema/schema_compiler.cpp:74` | `struct pending_item` | 15.0.1 | Accepted |
| DEV-104.16 | `core/src/schema/schema_compiler.cpp:60` | `struct ref_entry` | 15.0.1 | Accepted |
| DEV-104.17 | `core/src/schema/schema_compiler.cpp:49` | `struct key_entry` | 15.0.1 | Accepted |
| DEV-104.18 | `core/src/schema/value_equality.cpp:29` | `struct value_pair` | 15.0.1 | Accepted |
| DEV-104.19 | `includes/adaptive_json5/schema/compiled_schema.hpp:46` | `class [[nodiscard]] compiled_schema` | 15.0.1 | Accepted |
| DEV-104.20 | `includes/adaptive_json5/schema/compiled_schema.hpp:30` | `struct [[nodiscard]] compile_options` | 15.0.1 | Accepted |
| DEV-104.21 | `includes/adaptive_json5/schema/schema_budget.hpp:36` | `struct [[nodiscard]] schema_compile_limits` | 15.0.1 | Accepted |
| DEV-104.22 | `includes/adaptive_json5/schema/schema_budget.hpp:19` | `struct [[nodiscard]] validation_budget` | 15.0.1 | Accepted |
| DEV-104.23 | `includes/adaptive_json5/schema/schema_error.hpp:143` | `class [[nodiscard]] schema_error` | 15.0.1 | Accepted |
| DEV-104.24 | `includes/adaptive_json5/schema/validation_options.hpp:54` | `struct [[nodiscard]] validation_options` | 15.0.1 | Accepted |
| DEV-104.25 | `includes/adaptive_json5/schema/validation_options.hpp:40` | `struct [[nodiscard]] cancellation_callback` | 15.0.1 | Accepted |
| DEV-104.26 | `includes/adaptive_json5/schema/validation_report.hpp:49` | `class [[nodiscard]] validation_report` | 15.0.1 | Accepted |
| DEV-104.27 | `includes/adaptive_json5/schema/validation_report.hpp:27` | `struct [[nodiscard]] schema_diagnostic` | 15.0.1 | Accepted |
| DEV-104.28 | `core/includes/schema/regex_program.hpp:58` | `struct regex_tables` | 15.0.1 | Accepted |
| DEV-104.29 | `core/includes/schema/regex_program.hpp:47` | `struct regex_range` | 15.0.1 | Accepted |
| DEV-104.30 | `core/includes/schema/regex_program.hpp:33` | `struct regex_inst` | 15.0.1 | Accepted |
| DEV-104.31 | `core/src/schema/exact_number.cpp:35` | `struct decimal_parts` | 15.0.1 | Accepted |
| DEV-104.32 | `core/src/schema/regex.cpp:1210` | `struct matcher` | 15.0.1 | Accepted |
| DEV-104.33 | `core/src/schema/regex.cpp:949` | `struct emitter` | 15.0.1 | Accepted |
| DEV-104.34 | `core/src/schema/regex.cpp:935` | `struct emit_task` | 15.0.1 | Accepted |
| DEV-104.35 | `core/src/schema/regex.cpp:373` | `struct escape_value` | 15.0.1 | Accepted |
| DEV-104.36 | `core/src/schema/regex.cpp:129` | `struct regex_builder` | 15.0.1 | Accepted |
| DEV-104.37 | `core/src/schema/regex.cpp:118` | `struct group_context` | 15.0.1 | Accepted |
| DEV-104.38 | `core/src/schema/regex.cpp:100` | `struct ast_node` | 15.0.1 | Accepted |
| DEV-104.39 | `core/src/schema/schema_compiler.cpp:56` | `struct keyword_slot` | 15.0.1 | Accepted |
| DEV-104.40 | `core/src/schema/schema_values.cpp:221` | `struct copy_frame` | 15.0.1 | Accepted |
| DEV-104.41 | `core/src/schema/regex.cpp` | `struct property_selector` | 15.0.1 | Accepted |
| DEV-104.42 | `core/includes/schema/unicode_properties.hpp` (generated) | `struct unicode_value_name` | 15.0.1 | Accepted |
| DEV-104.43 | `core/includes/schema/unicode_properties.hpp` (generated) | `struct unicode_decomposition` | 15.0.1 | Accepted |
| DEV-104.44 | `core/includes/schema/unicode_properties.hpp` (generated) | `struct unicode_composition` | 15.0.1 | Accepted |
| DEV-104.45 | `core/includes/schema/unicode_properties.hpp` (generated) | `struct uts46_entry` | 15.0.1 | Accepted |
| DEV-104.46 | `core/src/schema/format.cpp:926` | `struct code_point_read` | 15.0.1 | Accepted |

## DEV-105: Rule 9.5.1, counting loops

| Field | Value |
| --- | --- |
| CodeQL queries | `cpp/misra/legacy-for-statements-should-be-simple` |
| Lines | 21 |
| Status | Accepted by the owner 2026-11-25 (#44) |

### Why this rule is deviated instead of fixed

A counting loop over a container or range whose bound is a size read through a member call; the counter is only incremented by the loop statement.

### Risk and compensating measures

Unit tests, the schema fuzzer, the sanitizer runs and the allocation-failure and budget tests exercise these lines; each line keeps the justification next to it in the source.

### Lines

| ID | Location | Code | Rules | Status |
| --- | --- | --- | --- | --- |
| DEV-105.1 | `core/src/schema/evaluator.cpp:1077` | `for (std::size_t level = 0U; level < path.size(); ++level)` | 9.5.1 | Accepted |
| DEV-105.2 | `core/src/schema/evaluator.cpp:1044` | `for (std::size_t index = 0U; index < *size.value_if(); ++index)` | 9.5.1 | Accepted |
| DEV-105.3 | `core/src/schema/evaluator.cpp:978` | `for (std::size_t up = 1U; up < depth; ++up)` | 9.5.1 | Accepted |
| DEV-105.4 | `core/src/schema/evaluator.cpp:970` | `for (std::size_t node = diagnostic.schema_node; compiled_schema_access::node(schema, no...` | 9.5.1 | Accepted |
| DEV-105.5 | `core/src/schema/evaluator.cpp:884` | `for (std::size_t index = 0U; index < units.count; ++index)` | 9.5.1 | Accepted |
| DEV-105.6 | `core/src/schema/evaluator.cpp:742` | `for (std::size_t level = 0U; level < run.frames.size(); ++level)` | 9.5.1 | Accepted |
| DEV-105.7 | `core/src/schema/evaluator.cpp:341` | `for (std::size_t index = 0U; index < node.properties.count; ++index)` | 9.5.1 | Accepted |
| DEV-105.8 | `core/src/schema/evaluator.cpp:260` | `for (std::size_t index = 0U; index < units.count; ++index)` | 9.5.1 | Accepted |
| DEV-105.9 | `core/src/schema/schema_compiler.cpp:1403` | `for (std::size_t index = 0U; index < m_node_count; ++index)` | 9.5.1 | Accepted |
| DEV-105.10 | `core/src/schema/schema_compiler.cpp:1319` | `for (std::size_t resource = 0U; resource < registry.size(); ++resource)` | 9.5.1 | Accepted |
| DEV-105.11 | `core/src/schema/schema_compiler.cpp:1210` | `for (std::size_t index = 0U; index < state.refs.size(); ++index)` | 9.5.1 | Accepted |
| DEV-105.12 | `core/src/schema/schema_compiler.cpp:1174` | `for (std::size_t index = 0U; index < state.refs.size(); ++index)` | 9.5.1 | Accepted |
| DEV-105.13 | `core/src/schema/schema_compiler.cpp:1033` | `for (std::size_t child = owner.first_child; child < (owner.first_child + owner.child_co...` | 9.5.1 | Accepted |
| DEV-105.14 | `core/src/schema/schema_compiler.cpp:964` | `for (std::size_t index = 1U; index <= extra; ++index)` | 9.5.1 | Accepted |
| DEV-105.15 | `core/src/schema/schema_compiler.cpp:910` | `for (std::size_t index = 0U; index < state.keys.size(); ++index)` | 9.5.1 | Accepted |
| DEV-105.16 | `core/src/schema/schema_compiler.cpp:277` | `for (std::size_t index = 0U; index < *size.value_if(); ++index)` | 9.5.1 | Accepted |
| DEV-105.17 | `core/src/schema/schema_compiler.cpp:242` | `for (std::size_t index = 0U; index < name.size(); ++index)` | 9.5.1 | Accepted |
| DEV-105.18 | `core/src/schema/schema_compiler.cpp:221` | `for (std::size_t index = c_first_named_keyword; index < c_keyword_count; ++index)` | 9.5.1 | Accepted |
| DEV-105.19 | `core/src/schema/schema_compiler.cpp:201` | `for (std::size_t index = 0U; index < keyword.size(); ++index)` | 9.5.1 | Accepted |
| DEV-105.20 | `core/src/schema/schema_registry.cpp:175` | `for (std::size_t index = 0U; index < m_size; ++index)` | 9.5.1 | Accepted |
| DEV-105.21 | `core/src/schema/value_equality.cpp:59` | `for (std::size_t index = 0U; index < left.size(); ++index)` | 9.5.1 | Accepted |

## DEV-106: Rules 7.0.5 and 7.0.6, integral promotion and assignment

| Field | Value |
| --- | --- |
| CodeQL queries | `cpp/misra/no-signedness-change-from-promotion`, `cpp/misra/numeric-assignment-type-mismatch` |
| Lines | 22 |
| Status | Accepted by the owner 2026-11-25 (#44, #48, #53) |

### Why this rule is deviated instead of fixed

Comparisons and arithmetic on std::uint16_t code units promote to int without changing the value; the operands are unsigned values below 0x110000 and the results are used as such.

### Risk and compensating measures

Unit tests, the schema fuzzer, the sanitizer runs and the allocation-failure and budget tests exercise these lines; each line keeps the justification next to it in the source.

### Lines

| ID | Location | Code | Rules | Status |
| --- | --- | --- | --- | --- |
| DEV-106.1 | `core/src/schema/evaluator.cpp:964` | `std::size_t offset = put("#", buffer, capacity, 0U);` | 7.0.6 | Accepted |
| DEV-106.2 | `core/src/schema/evaluator.cpp:949` | `offset = put(std::string_view{&digits[count], 1U}, buffer, capacity, offset);` | 7.0.6 | Accepted |
| DEV-106.3 | `core/src/schema/evaluator.cpp:267` | `if (*unit.value_if() != compiled_schema_access::unit(run.schema, units.first + index))` | 7.0.5 | Accepted |
| DEV-106.4 | `core/src/schema/instance_domain.cpp:100` | `if ((low < c_low_surrogate_first) \|\| (low > c_low_surrogate_last))` | 7.0.5 | Accepted |
| DEV-106.5 | `core/src/schema/instance_domain.cpp:88` | `if ((value >= c_high_surrogate_first) && (value <= c_high_surrogate_last))` | 7.0.5 | Accepted |
| DEV-106.6 | `core/src/schema/instance_domain.cpp:84` | `if ((value >= c_low_surrogate_first) && (value <= c_low_surrogate_last))` | 7.0.5 | Accepted |
| DEV-106.7 | `core/src/schema/schema_compiler.cpp:1302` | `return resolve_target(state, detail::text_of(state.resolved, 0U, state.resolved.size()));` | 7.0.6 | Accepted |
| DEV-106.8 | `core/src/schema/schema_compiler.cpp:1185` | `const auto target = resolve_target(state, detail::text_of(state.resolved, 0U, state.res...` | 7.0.6 | Accepted |
| DEV-106.9 | `core/src/schema/schema_compiler.cpp:1114` | `find_child(state, current, keyword, detail::text_of(state.token, 0U, state.token.size()...` | 7.0.6 | Accepted |
| DEV-106.10 | `core/src/schema/schema_compiler.cpp:1088` | `const std::string_view keyword = detail::text_of(state.text, 0U, state.text.size());` | 7.0.6 | Accepted |
| DEV-106.11 | `core/src/schema/schema_compiler.cpp:1083` | `const auto copied = detail::append_text(state.text, detail::text_of(state.token, 0U, st...` | 7.0.6 | Accepted |
| DEV-106.12 | `core/src/schema/schema_compiler.cpp:1008` | `(state.units.get(units.first + unit + 1U) != low))` | 7.0.5 | Accepted |
| DEV-106.13 | `core/src/schema/schema_compiler.cpp:1007` | `if (((unit + 1U) >= units.count) \|\| (state.units.get(units.first + unit) != high) \|\|` | 7.0.5 | Accepted |
| DEV-106.14 | `core/src/schema/schema_compiler.cpp:705` | `const auto stored = store_text(state.ref_text, detail::text_of(state.text, 0U, state.te...` | 7.0.6 | Accepted |
| DEV-106.15 | `core/src/schema/schema_compiler.cpp:604` | `if (!matches_dialect_uri(state.dialect, detail::text_of(state.text, 0U, state.text.size...` | 7.0.6 | Accepted |
| DEV-106.16 | `core/src/schema/schema_compiler.cpp:522` | `const auto added = add_child(state, parent, keyword, step_kind::name, units, 0U, entry,...` | 7.0.6 | Accepted |
| DEV-106.17 | `core/src/schema/schema_compiler.cpp:397` | `const std::string_view target = detail::text_of(state.resolved, 0U, state.resolved.size...` | 7.0.6 | Accepted |
| DEV-106.18 | `core/src/schema/schema_compiler.cpp:385` | `const std::string_view id_text = detail::text_of(state.text, 0U, state.text.size());` | 7.0.6 | Accepted |
| DEV-106.19 | `core/src/schema/schema_registry.cpp:174` | `const std::string_view uri = detail::text_of(normalized, 0U, normalized.size());` | 7.0.6 | Accepted |
| DEV-106.20 | `core/src/schema/uri.cpp:272` | `path = text_of(merged, 0U, merged.size());` | 7.0.6 | Accepted |
| DEV-106.21 | `core/src/schema/value_equality.cpp:71` | `if (*a.value_if() != *b.value_if())` | 7.0.5 | Accepted |
| DEV-106.22 | `core/src/schema/format.cpp:614` | `return printable ? c_printable[unit - c_first_printable] : c_non_ascii[0U];` | 7.0.6 | Accepted |

## DEV-107: Rule 8.2.6 and related, object pointers in caller-provided storage

| Field | Value |
| --- | --- |
| CodeQL queries | `cpp/misra/int-to-pointer-cast-prohibited`, `cpp/misra/function-pointer-conversion-context` |
| Lines | 14 |
| Status | Accepted by the owner 2026-11-25 (#44, #48) |

### Why this rule is deviated instead of fixed

Address computation inside an owned block from caller storage, as in the parser storage layer (DEV-002/DEV-003); the offset is within the block size that was allocated.

### Risk and compensating measures

Unit tests, the schema fuzzer, the sanitizer runs and the allocation-failure and budget tests exercise these lines; each line keeps the justification next to it in the source.

### Lines

| ID | Location | Code | Rules | Status |
| --- | --- | --- | --- | --- |
| DEV-107.1 | `core/includes/schema/pod_vector.hpp:196` | `return static_cast<T*>(m_block.data());` | 8.2.6 | Accepted |
| DEV-107.2 | `core/includes/schema/pod_vector.hpp:106` | `const auto grown = grow();` | 7.11.3 | Accepted |
| DEV-107.3 | `core/src/schema/evaluator.cpp:1253` | `return static_cast<const schema_diagnostic*>(m_diagnostics.data()) + index;` | 8.2.6 | Accepted |
| DEV-107.4 | `core/src/schema/instance_domain.cpp:369` | `return *std::launder(static_cast<stack_frame*>(slot));` | 8.2.6 | Accepted |
| DEV-107.5 | `core/src/schema/schema_compiler.cpp:1479` | `return static_cast<const std::uint16_t*>(schema.m_units.data())[index];` | 8.2.6 | Accepted |
| DEV-107.6 | `core/src/schema/schema_compiler.cpp:1468` | `return static_cast<const property_edge*>(schema.m_properties.data())[index];` | 8.2.6 | Accepted |
| DEV-107.7 | `core/src/schema/schema_compiler.cpp:1457` | `return static_cast<const std::size_t*>(schema.m_edges.data())[index];` | 8.2.6 | Accepted |
| DEV-107.8 | `core/src/schema/schema_compiler.cpp:1446` | `return static_cast<const schema_node*>(schema.m_nodes.data())[index];` | 8.2.6 | Accepted |
| DEV-107.9 | `core/src/schema/schema_registry.cpp:265` | `return std::string_view{static_cast<const char*>(entry.uri.data()), entry.uri_size};` | 8.2.6 | Accepted |
| DEV-107.10 | `core/src/schema/schema_registry.cpp:42` | `return std::launder(static_cast<registry_entry*>(address));` | 8.2.6 | Accepted |
| DEV-107.11 | `core/src/schema/uri.cpp:383` | `return std::string_view{static_cast<const char*>(text.data()) + offset, length};` | 8.2.6 | Accepted |
| DEV-107.12 | `core/src/schema/schema_compiler.cpp:2280` | `static_cast<const regex_range*>(schema.m_regex_ranges.data())};` | 8.2.6 | Accepted |
| DEV-107.13 | `core/src/schema/schema_compiler.cpp:2279` | `return regex_tables{static_cast<const regex_inst*>(schema.m_regex_code.data()),` | 8.2.6 | Accepted |
| DEV-107.14 | `core/src/schema/evaluator.cpp:1993` | `return static_cast<const schema_annotation*>(m_annotations.data()) + index;` | 8.2.6 | Accepted |

## DEV-108: Rule 28.6.3, objects after a move

| Field | Value |
| --- | --- |
| CodeQL queries | `cpp/misra/object-used-while-in-potentially-moved-from-state` |
| Lines | 11 |
| Status | Accepted by the owner 2026-11-25 (#44, #48) |

### Why this rule is deviated instead of fixed

The moved-from object is reassigned, reset or only destroyed; a result is moved out of once.

### Risk and compensating measures

Unit tests, the schema fuzzer, the sanitizer runs and the allocation-failure and budget tests exercise these lines; each line keeps the justification next to it in the source.

### Lines

| ID | Location | Code | Rules | Status |
| --- | --- | --- | --- | --- |
| DEV-108.1 | `core/includes/schema/pod_vector.hpp:64` | `m_block = std::move(*block.value_if());` | 28.6.3 | Accepted |
| DEV-108.2 | `core/src/schema/evaluator.cpp:586` | `return std::move(*outcome);` | 28.6.3 | Accepted |
| DEV-108.3 | `core/src/schema/instance_domain.cpp:337` | `m_block = std::move(*block.value_if());` | 28.6.3 | Accepted |
| DEV-108.4 | `core/src/schema/schema_registry.cpp:202` | `registry_entry{std::move(uri_block), uri_size, std::move(*parsed.value_if())});` | 28.6.3 | Accepted |
| DEV-108.5 | `core/src/schema/schema_registry.cpp:119` | `schema_registry{storage, std::move(*entries.value_if()), capacity});` | 28.6.3 | Accepted |
| DEV-108.6 | `core/src/schema/evaluator.cpp:990` | `auto made = copy.has_value() ? detail::document_access::make_string(*run.names, std::mov...` | 28.6.3 | Accepted |
| DEV-108.7 | `core/src/schema/schema_compiler.cpp:2058` | `detail::compiled_schema_access::attach_values(schema, std::move(*state.values));` | 28.6.3 | Accepted |
| DEV-108.8 | `core/src/schema/schema_values.cpp:293` | `const auto attached = document_access::append_member(target, frame.target, std::move(*co...` | 28.6.3 | Accepted |
| DEV-108.9 | `core/src/schema/schema_values.cpp:206` | `auto string = document_access::make_string(target, std::move(*copied.value_if()));` | 28.6.3 | Accepted |
| DEV-108.10 | `core/src/schema/schema_values.cpp:152` | `auto made = document_access::make_number(target, std::move(*copied.value_if()));` | 28.6.3 | Accepted |
| DEV-108.11 | `core/src/schema/schema_values.cpp:135` | `return text_result::success(std::move(*copied.value_if()));` | 28.6.3 | Accepted |

## DEV-109: Rules 15.0.2 and 18.4.1, noexcept special members

| Field | Value |
| --- | --- |
| CodeQL queries | `cpp/misra/invalid-signature-for-special-member-function`, `cpp/misra/exception-unfriendly-function-must-be-noexcept`, `cpp/misra/member-functions-refqualified`, `cpp/misra/non-static-member-not-init-before-use` |
| Lines | 7 |
| Status | Accepted by the owner 2026-11-25 (#44) |

### Why this rule is deviated instead of fixed

Move construction and accessors are noexcept and leave the source empty; members have default member initializers, so none is read before it is set.

### Risk and compensating measures

Unit tests, the schema fuzzer, the sanitizer runs and the allocation-failure and budget tests exercise these lines; each line keeps the justification next to it in the source.

### Lines

| ID | Location | Code | Rules | Status |
| --- | --- | --- | --- | --- |
| DEV-109.1 | `core/includes/schema/pod_vector.hpp:84` | `pod_vector(pod_vector&& other) noexcept` | 18.4.1, 15.0.2 | Accepted |
| DEV-109.2 | `core/includes/schema/pod_vector.hpp:76` | `explicit pod_vector(const storage_callbacks& storage) noexcept : m_storage(storage) {}` | 15.1.4 | Accepted |
| DEV-109.3 | `core/src/schema/evaluator.cpp:1199` | `validation_report::validation_report(validation_report&& other) noexcept` | 18.4.1, 15.0.2 | Accepted |
| DEV-109.4 | `core/src/schema/schema_compiler.cpp:1358` | `compiled_schema::compiled_schema(compiled_schema&& other) noexcept` | 18.4.1, 15.0.2 | Accepted |
| DEV-109.5 | `core/src/schema/schema_registry.cpp:128` | `schema_registry::schema_registry(schema_registry&& other) noexcept` | 18.4.1, 15.0.2 | Accepted |
| DEV-109.6 | `core/src/schema/schema_work.cpp:117` | `const validation_budget& schema_work::budget() const noexcept` | 6.8.4 | Accepted |
| DEV-109.7 | `includes/adaptive_json5/schema/schema_error.hpp:278` | `[[nodiscard]] constexpr const std::optional<error>& parser_error() const noexcept` | 6.8.4 | Accepted |

## DEV-110: Rules 21.2.2 and 24.5.2, std::memcpy of trivially copyable elements

| Field | Value |
| --- | --- |
| CodeQL queries | `cpp/misra/no-memory-functions-from-c-string`, `cpp/misra/pointer-argument-to-cstring-function-is-invalid` |
| Lines | 1 (further memcpy lines in pod_vector.hpp are listed under DEV-107 with both rules) |
| Status | Accepted by the owner 2026-11-25 (#44) |

### Why this rule is deviated instead of fixed

pod_vector copies trivially copyable elements (static_assert) within allocated blocks whose sizes are element counts times sizeof(T), as in the parser (DEV-023).

### Risk and compensating measures

Unit tests, the schema fuzzer, the sanitizer runs and the allocation-failure tests exercise these lines.

### Lines

| ID | Location | Code | Rules | Status |
| --- | --- | --- | --- | --- |
| DEV-110.1 | `core/src/schema/pod_vector.cpp:52` | `static_cast<void>(std::memcpy(destination, source, bytes));` | 24.5.2 | Accepted |

## DEV-111: Rule 0.1.1, defensive initialization

| Field | Value |
| --- | --- |
| CodeQL queries | `cpp/misra/unnecessary-write-to-local-object` |
| Lines | 3 |
| Status | Accepted by the owner 2026-11-25 (#44) |

### Why this rule is deviated instead of fixed

Initialized at the declaration so that no path can read an indeterminate value (as DEV-007).

### Risk and compensating measures

Unit tests, the schema fuzzer, the sanitizer runs and the allocation-failure and budget tests exercise these lines; each line keeps the justification next to it in the source.

### Lines

| ID | Location | Code | Rules | Status |
| --- | --- | --- | --- | --- |
| DEV-111.1 | `core/src/schema/schema_compiler.cpp:936` | `std::uint32_t value = 0U;` | 0.1.1 | Accepted |
| DEV-111.2 | `core/src/schema/schema_registry.cpp:199` | `const std::size_t uri_size = normalized.size();` | 0.1.1 | Accepted |
| DEV-111.3 | `includes/adaptive_json5/schema/schema_error.hpp:369` | `schema_error_code code = schema_error_code::internal_error;` | 0.1.1 | Accepted |

## DEV-112: Rule 21.6.3, construction in caller-provided storage

| Field | Value |
| --- | --- |
| CodeQL queries | `cpp/misra/advanced-memory-management-used` |
| Lines | 3 |
| Status | Accepted by the owner 2026-11-25 (#44) |

### Why this rule is deviated instead of fixed

Placement new constructs trivially destructible or explicitly destroyed objects in owned blocks from caller storage, as in the parser (DEV-001).

### Risk and compensating measures

Unit tests, the schema fuzzer, the sanitizer runs and the allocation-failure and budget tests exercise these lines; each line keeps the justification next to it in the source.

### Lines

| ID | Location | Code | Rules | Status |
| --- | --- | --- | --- | --- |
| DEV-112.1 | `core/src/schema/instance_domain.cpp:355` | `static_cast<void>(::new (slot) stack_frame(frame));` | 21.6.3 | Accepted |
| DEV-112.2 | `core/src/schema/schema_registry.cpp:201` | `static_cast<void>(::new (static_cast<void*>(slot(m_entries, m_size)))` | 21.6.3 | Accepted |
| DEV-112.3 | `core/src/schema/schema_registry.cpp:147` | `slot(m_entries, m_size)->~registry_entry();` | 21.6.3 | Accepted |

## DEV-113: Rule 4.1.3, signed arithmetic bounded by saturation

| Field | Value |
| --- | --- |
| CodeQL queries | `cpp/misra/signed-integer-overflow` |
| Lines | 6 |
| Status | Accepted by the owner 2026-11-25 (#48) |

### Why this rule is deviated instead of fixed

Decimal exponents are saturated at 2^40 (c_scale_cap) and fraction lengths are bounded by the document size, so sums and differences of two exponents stay far inside std::int64_t; CodeQL does not follow the saturation.

### Risk and compensating measures

Unit tests, the schema fuzzer, the sanitizer runs and the allocation-failure and budget tests exercise these lines; each line keeps the justification next to it in the source.

### Lines

| ID | Location | Code | Rules | Status |
| --- | --- | --- | --- | --- |
| DEV-113.3 | `core/src/schema/exact_number.cpp:374` | `const auto modulus_zeros = static_cast<std::int64_t>(divisor_parts->exponent - common);` | 4.1.3 | Accepted |
| DEV-113.4 | `core/src/schema/exact_number.cpp:368` | `const auto dividend_zeros = static_cast<std::int64_t>(value_parts->exponent - common);` | 4.1.3 | Accepted |
| DEV-113.5 | `core/src/schema/exact_number.cpp:266` | `++count;` | 4.1.3 | Accepted |
| DEV-113.6 | `core/src/schema/exact_number.cpp:153` | `parts.exponent -= static_cast<std::int64_t>(parts.fraction.size());` | 4.1.3 | Accepted |
| DEV-113.7 | `core/src/schema/exact_number.cpp:93` | `value = (value * 10) + static_cast<std::int64_t>(digit_value(c, 10U).value_or(0U));` | 4.1.3 | Accepted |
| DEV-113.8 | `core/src/schema/exact_number.cpp:349` | `++count;` | 4.1.3 | Accepted |

## DEV-114: Rule 6.0.3/redeclaration query, namespace-scope constexpr constant

| Field | Value |
| --- | --- |
| CodeQL queries | `cpp/misra/redeclaration-of-static-constexpr-data-member` |
| Lines | 13 |
| Status | Accepted by the owner 2026-11-25 (#48, #53; #187 accepted 2026-11-27) |

### Why this rule is deviated instead of fixed

A namespace-scope constexpr std::string_view in an anonymous namespace is declared once; it is not a static data member, so the redeclaration the query reports does not exist.

### Risk and compensating measures

Unit tests, the schema fuzzer, the sanitizer runs and the allocation-failure and budget tests exercise these lines; each line keeps the justification next to it in the source.

### Lines

| ID | Location | Code | Rules | Status |
| --- | --- | --- | --- | --- |
| DEV-114.1 | `core/src/schema/exact_number.cpp:27` | `constexpr std::string_view c_signs{"+-"};` | 4.1.2 | Accepted |
| DEV-114.2 | `core/src/schema/format.cpp:96` | `constexpr std::array<std::string_view, 16> c_format_names{` | 4.1.2 | Accepted |
| DEV-114.3 | `core/src/schema/format.cpp:45` | `constexpr std::string_view c_non_ascii{"\x7f"};` | 4.1.2 | Accepted |
| DEV-114.4 | `core/src/schema/format.cpp:38` | `constexpr std::string_view c_printable{` | 4.1.2 | Accepted |
| DEV-114.5 | `core/src/schema/format.cpp:35` | `constexpr std::string_view c_uri{` | 4.1.2 | Accepted |
| DEV-114.6 | `core/src/schema/format.cpp:32` | `constexpr std::string_view c_local{` | 4.1.2 | Accepted |
| DEV-114.7 | `core/src/schema/format.cpp:30` | `constexpr std::string_view c_scheme{"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxy...` | 4.1.2 | Accepted |
| DEV-114.8 | `core/src/schema/format.cpp:29` | `constexpr std::string_view c_label{"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz...` | 4.1.2 | Accepted |
| DEV-114.9 | `core/src/schema/format.cpp:28` | `constexpr std::string_view c_letters{"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwx...` | 4.1.2 | Accepted |
| DEV-114.10 | `core/src/schema/format.cpp:27` | `constexpr std::string_view c_hex_digits{"0123456789abcdefABCDEF"};` | 4.1.2 | Accepted |
| DEV-114.11 | `core/src/schema/format.cpp:26` | `constexpr std::string_view c_digits{"0123456789"};` | 4.1.2 | Accepted |
| DEV-114.12 | `core/src/schema/meta_schema.cpp:16` | `constexpr std::string_view c_draft07_meta_schema{R"meta({` | 4.1.2 | Accepted |
| DEV-114.13 | `core/src/schema/regex.cpp:561` | `constexpr std::array<std::string_view, 95> c_unsupported_binary_properties{{` | 4.1.2 | Accepted |
