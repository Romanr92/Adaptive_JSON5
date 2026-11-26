#include "cli/run_command.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/input.hpp"
#include "adaptive_json5/memory/accounted_storage.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/parser.hpp"
#include "adaptive_json5/text_position.hpp"
#include "adaptive_json5/value/decoded_name_equal.hpp"
#include "adaptive_json5/value/decoded_string.hpp"
#include "adaptive_json5/value/floating_conversion.hpp"
#include "adaptive_json5/value/integer_conversion.hpp"
#include "adaptive_json5/value/numeric_conversion_reason.hpp"
#include "adaptive_json5/value/owned_number.hpp"
#include "adaptive_json5/value/utf8_export.hpp"
#include "adaptive_json5/value/value_iterator.hpp"
#include "adaptive_json5/value/value_kind.hpp"
#include "adaptive_json5/value/value_view.hpp"
#include "cli/cli_app.hpp"
#include "cli/cli_io.hpp"
#include "cli/cli_options.hpp"
#include "cli/host_buffer.hpp"
#include "cli/report.hpp"
#include "cli/validate_command.hpp"

#if defined(ADAPTIVE_JSON5_CLI_HAS_SCHEMA)
  #include "adaptive_json5/schema.hpp"
#endif

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace adaptive_json5::cli
{
  namespace
  {
    constexpr std::size_t c_text_capacity = 256U;
    constexpr std::size_t c_path_capacity = 1024U;
    constexpr std::string_view c_digits{"0123456789"};
    constexpr error c_not_a_manifest = error::without_location(error_code::wrong_type_or_access);

    /**
     * @brief Fixed buffer holding a short UTF-8 text copied out of a document.
     * @par Requirements
     * P058.
     */
    struct short_text
    {
        std::array<char, c_text_capacity> bytes{};
        std::size_t size{};

        [[nodiscard]] std::string_view view() const noexcept
        {
          return std::string_view{bytes.data(), size};
        }
    };

    /**
     * @brief Copy a string value as UTF-8 into a short_text; false when it does not fit or is
     * not a string.
     * @par Requirements
     * P058.
     * @par ID SWU-CLI-081
     */
    bool copy_text(const value_view& value, short_text& out) noexcept
    {
      const auto text = value.as_string();
      if (!text.has_value())
      {
        return false;
      }
      const auto exported = export_utf8(**text.value_if(), out.bytes.data(), out.bytes.size());
      out.size = exported.has_value() ? *exported.value_if() : 0U;
      return exported.has_value() && (out.size <= out.bytes.size());
    }

    /**
     * @brief Copy member name of object as UTF-8; false when absent or not a short string.
     * @par Requirements
     * P058.
     * @par ID SWU-CLI-082
     */
    bool member_text(const value_view& object, std::string_view name, short_text& out) noexcept
    {
      const auto member = object.find_member(name);
      return member.has_value() && copy_text(*member.value_if(), out);
    }

    /**
     * @brief Read member name of object as an unsigned integer.
     * @par Requirements
     * P058.
     * @par ID SWU-CLI-083
     */
    std::optional<std::size_t> member_count(const value_view& object, std::string_view name) noexcept
    {
      const auto member = object.find_member(name);
      if (!member.has_value())
      {
        return std::nullopt;
      }
      const auto value = to_integer<std::uint64_t>(*member.value_if());
      return value.has_value() ? std::optional<std::size_t>{static_cast<std::size_t>(*value.value_if())} : std::nullopt;
    }

    /**
     * @brief Decode one pointer token (~1 to '/', ~0 to '~') into out.
     * @par Requirements
     * P058; RFC 6901 section 4.
     * @par ID SWU-CLI-084
     */
    bool decode_token(std::string_view token, short_text& out) noexcept
    {
      out.size = 0U;
      std::size_t index = 0U;
      while (index < token.size())
      {
        char c = token[index];
        if (c == '~')
        {
          if ((index + 1U) >= token.size())
          {
            return false;
          }
          c = (token[index + 1U] == '1') ? '/' : '~';
          ++index;
        }
        if (out.size >= out.bytes.size())
        {
          return false;
        }
        out.bytes[out.size] = c;
        ++out.size;
        ++index;
      }
      return true;
    }

    /**
     * @brief Apply one decoded token to a value (member name or array index).
     * @par Requirements
     * P058; RFC 6901 section 4.
     * @par ID SWU-CLI-085
     */
    std::optional<value_view> step(const value_view& current, std::string_view token) noexcept
    {
      const auto kind = current.kind();
      if (kind.has_value() && (*kind.value_if() == value_kind::array))
      {
        if (token.empty() || (token.find_first_not_of(c_digits) != std::string_view::npos) || (token.size() > 9U))
        {
          return std::nullopt;
        }
        std::size_t index = 0U;
        for (const char c : token)
        {
          index = (index * 10U) + c_digits.find(c);
        }
        const auto element = current.element_at(index);
        return element.has_value() ? std::optional<value_view>{*element.value_if()} : std::nullopt;
      }
      const auto member = current.find_member(token);
      return member.has_value() ? std::optional<value_view>{*member.value_if()} : std::nullopt;
    }

    /**
     * @brief Outcome of one manifest case.
     * @par Requirements
     * P058; J5-070.
     */
    enum class case_result : std::uint8_t
    {
      pass,
      fail,
      unsupported,
      error
    };

    /**
     * @brief Everything one case run needs.
     * @par Requirements
     * P058.
     */
    struct case_context
    {
        const options& selected;
        const environment& host;
        text_writer& out;
        bool json;
        std::string_view base; // Manifest directory with a trailing separator, or empty.
        std::string_view id;
        std::size_t mismatches{};
    };

    /**
     * @brief Record one mismatch of the current case.
     * @par Requirements
     * P058; J5-071.
     * @par ID SWU-CLI-086
     */
    void mismatch(case_context& context, std::string_view what, std::string_view detail) noexcept
    {
      if (context.json)
      {
        context.out.put((context.mismatches == 0U) ? "{\"what\": " : ", {\"what\": ");
        put_json_text(context.out, what);
        context.out.put(", \"detail\": ");
        put_json_text(context.out, detail);
        context.out.put("}");
      }
      else
      {
        context.out.put("  ").put(context.id).put(": ").put(what);
        context.out.put(detail.empty() ? "" : " (").put(detail).put(detail.empty() ? "" : ")").put("\n");
      }
      ++context.mismatches;
    }

    /**
     * @brief Name of a numeric conversion outcome ("ok" or the failure reason).
     * @par Requirements
     * P058; J5-034.
     * @par ID SWU-CLI-087
     */
    std::string_view conversion_name(const error* failure) noexcept
    {
      if (failure == nullptr)
      {
        return "ok";
      }
      const auto reason = failure->numeric_reason();
      if (!reason.has_value())
      {
        return "failed";
      }
      constexpr std::array<std::string_view, 5> c_reasons{
        {"non_integral", "out_of_range", "non_finite", "underflow", "unsupported_destination"}};
      const auto index = static_cast<std::size_t>(*reason);
      return (index < c_reasons.size()) ? c_reasons[index] : std::string_view{"failed"};
    }

    /**
     * @brief Write an integer as decimal text into out.
     * @par Requirements
     * P058.
     * @par ID SWU-CLI-088
     */
    void decimal_text(std::uint64_t magnitude, bool negative, short_text& out) noexcept
    {
      std::array<char, 24> digits{};
      std::size_t first = digits.size();
      std::uint64_t rest = magnitude;
      do
      {
        --first;
        digits[first] = c_digits[static_cast<std::size_t>(rest % 10U)];
        rest /= 10U;
      } while (rest != 0U);
      out.size = 0U;
      if (negative)
      {
        out.bytes[0U] = '-';
        out.size = 1U;
      }
      for (std::size_t index = first; index < digits.size(); ++index)
      {
        out.bytes[out.size] = digits[index];
        ++out.size;
      }
    }

    /**
     * @brief check "conversion": to int64/uint64/double gives the expected result (and value).
     * @par Requirements
     * P058; J5-034; J5-035.
     * @par ID SWU-CLI-089
     */
    bool check_conversion(const value_view& target, const value_view& check, const environment& host) noexcept
    {
      short_text to{};
      short_text expected{};
      const auto number = target.as_number();
      if (!number.has_value() || !member_text(check, "to", to) || !member_text(check, "result", expected))
      {
        return false;
      }
      short_text value{};
      std::string_view observed{};
      if (to.view() == "int64")
      {
        const auto converted = to_integer<std::int64_t>(**number.value_if());
        observed = conversion_name(converted.error_if());
        if (converted.has_value())
        {
          const std::int64_t v = *converted.value_if();
          decimal_text((v < 0) ? (0U - static_cast<std::uint64_t>(v)) : static_cast<std::uint64_t>(v), v < 0, value);
        }
      }
      else if (to.view() == "uint64")
      {
        const auto converted = to_integer<std::uint64_t>(**number.value_if());
        observed = conversion_name(converted.error_if());
        if (converted.has_value())
        {
          decimal_text(*converted.value_if(), false, value);
        }
      }
      else
      {
        const auto converted = to_double(**number.value_if(), host.storage);
        observed = conversion_name(converted.error_if());
      }
      short_text expected_value{};
      const bool has_value = member_text(check, "value", expected_value);
      return (observed == expected.view()) && (!has_value || (value.view() == expected_value.view()));
    }

    /**
     * @brief check "member_names": object member names in source order.
     * @par Requirements
     * P058; J5-031; J5-032.
     * @par ID SWU-CLI-090
     */
    bool check_member_names(const value_view& target, const value_view& check) noexcept
    {
      const auto names = check.find_member("names");
      auto members = target.children();
      auto expected = names.has_value() ? names.value_if()->children() : result<value_iterator>{members};
      if (!names.has_value() || !members.has_value() || !expected.has_value())
      {
        return false;
      }
      for (;;)
      {
        const auto member = members.value_if()->next();
        const auto name = expected.value_if()->next();
        if (!member.has_value() || !name.has_value())
        {
          return false;
        }
        if (!member.value_if()->has_value() || !name.value_if()->has_value())
        {
          return member.value_if()->has_value() == name.value_if()->has_value();
        }
        short_text wanted{};
        const auto actual = (*member.value_if())->member_name();
        if (!copy_text(**name.value_if(), wanted) || !actual.has_value())
        {
          return false;
        }
        std::array<char, c_text_capacity> bytes{};
        const auto exported = export_utf8(**actual.value_if(), bytes.data(), bytes.size());
        if (!exported.has_value() || (std::string_view{bytes.data(), *exported.value_if()} != wanted.view()))
        {
          return false;
        }
      }
    }

    /**
     * @brief check "string_units": the exact UTF-16 code units of a string.
     * @par Requirements
     * P058; J5-030.
     * @par ID SWU-CLI-091
     */
    bool check_units(const value_view& target, const value_view& check) noexcept
    {
      const auto text = target.as_string();
      const auto units = check.find_member("units");
      const auto count = units.has_value() ? units.value_if()->container_size() : result<std::size_t>::success(0U);
      if (!text.has_value() || !units.has_value() || !count.has_value() ||
          (*count.value_if() != (*text.value_if())->size()))
      {
        return false;
      }
      for (std::size_t index = 0U; index < *count.value_if(); ++index)
      {
        const auto expected = units.value_if()->element_at(index);
        const auto wanted = expected.has_value() ? to_integer<std::uint16_t>(*expected.value_if())
                                                 : result<std::uint16_t>::failure(*expected.error_if());
        const auto actual = (*text.value_if())->unit_at(index);
        if (!wanted.has_value() || !actual.has_value() || (*wanted.value_if() != *actual.value_if()))
        {
          return false;
        }
      }
      return true;
    }

    /**
     * @brief check "same_string": two pointers name strings with identical decoded units.
     * @par Requirements
     * P058; J5-030.
     * @par ID SWU-CLI-092
     */
    bool check_same(const value_view& root, const value_view& target, const value_view& check) noexcept
    {
      short_text other{};
      if (!member_text(check, "other", other))
      {
        return false;
      }
      const auto second = resolve_pointer(root, other.view());
      const auto a = target.as_string();
      const auto b = second.has_value() ? second->as_string() : result<const decoded_string*>{a};
      if (!second.has_value() || !a.has_value() || !b.has_value())
      {
        return false;
      }
      const auto equal = decoded_name_equal(**a.value_if(), **b.value_if());
      return equal.has_value() && *equal.value_if();
    }

    /**
     * @brief check "duplicate_rejection": reparsing with reject_duplicates fails with both
     * locations as expected.
     * @par Requirements
     * P058; J5-032; J5-044.
     * @par ID SWU-CLI-093
     */
    bool check_duplicates(const value_view& check, input_view text, const case_context& context,
                          syntax_mode syntax) noexcept
    {
      parse_options parse{};
      parse.syntax = syntax;
      parse.duplicate_names = duplicate_name_policy::reject_duplicates;
      parse.limits = context.selected.limits;
      accounted_storage accounting{context.host.storage, parse.limits.max_owned_bytes};
      const auto parsed = parse_document(text, parse, accounting);
      const auto locations = parsed.has_value() ? std::nullopt : parsed.error_if()->duplicate_names();
      if (!locations.has_value())
      {
        return false;
      }
      const auto first = locate(text, locations->first.byte_offset);
      const auto repeated = locate(text, locations->repeated.byte_offset);
      const auto want_first = check.find_member("first");
      const auto want_repeated = check.find_member("repeated");
      return first.has_value() && repeated.has_value() && want_first.has_value() && want_repeated.has_value() &&
             (member_count(*want_first.value_if(), "line") == first.value_if()->line) &&
             (member_count(*want_first.value_if(), "column") == first.value_if()->column) &&
             (member_count(*want_repeated.value_if(), "line") == repeated.value_if()->line) &&
             (member_count(*want_repeated.value_if(), "column") == repeated.value_if()->column);
    }

    /**
     * @brief check "number_spelling": the raw spelling of a number.
     * @par Requirements
     * P058; J5-021.
     * @par ID SWU-CLI-113
     */
    bool check_spelling(const value_view& target, const value_view& check) noexcept
    {
      short_text expected{};
      const auto number = target.as_number();
      const input_view spelling = number.has_value() ? (*number.value_if())->spelling() : input_view{};
      return number.has_value() && member_text(check, "spelling", expected) &&
             (std::string_view{spelling.data, spelling.size} == expected.view());
    }

    /**
     * @brief check "utf8_export": checked UTF-8 export succeeds or fails as expected.
     * @par Requirements
     * P058; J5-030.
     * @par ID SWU-CLI-114
     */
    bool check_utf8(const value_view& target, const value_view& check) noexcept
    {
      short_text expected{};
      const auto string = target.as_string();
      const bool exported = string.has_value() && utf8_size(**string.value_if()).has_value();
      return string.has_value() && member_text(check, "result", expected) &&
             (expected.view() == (exported ? "success" : "failure"));
    }

    /**
     * @brief Evaluate a check that applies to the value at its pointer.
     * @par Requirements
     * P058; J5-070.
     * @par ID SWU-CLI-106
     */
    bool evaluate_value_check(std::string_view kind, const value_view& root, const value_view& target,
                              const value_view& check, const environment& host) noexcept
    {
      short_text expected{};
      if (kind == "absent_member")
      {
        return member_text(check, "name", expected) && !target.find_member(expected.view()).has_value();
      }
      if (kind == "number_spelling")
      {
        return check_spelling(target, check);
      }
      if (kind == "utf8_export")
      {
        return check_utf8(target, check);
      }
      if (kind == "string_units")
      {
        return check_units(target, check);
      }
      if (kind == "same_string")
      {
        return check_same(root, target, check);
      }
      if (kind == "member_names")
      {
        return check_member_names(target, check);
      }
      return (kind == "conversion") && check_conversion(target, check, host);
    }

    /**
     * @brief Evaluate one structured check against a parsed document.
     * @par Requirements
     * P058; J5-070.
     * @par ID SWU-CLI-094
     */
    bool evaluate_check(const value_view& check, const parsed_document& parsed, input_view text,
                        const case_context& context, syntax_mode syntax) noexcept
    {
      short_text kind{};
      short_text pointer{};
      const auto root = parsed.values.root();
      if (!member_text(check, "check", kind) || !root.has_value())
      {
        return false;
      }
      if (kind.view() == "warnings")
      {
        return member_count(check, "count") == parsed.warnings.size();
      }
      if (kind.view() == "duplicate_rejection")
      {
        return check_duplicates(check, text, context, syntax);
      }
      const auto target = member_text(check, "pointer", pointer) ? resolve_pointer(*root.value_if(), pointer.view())
                                                                 : std::optional<value_view>{};
      return target.has_value() && evaluate_value_check(kind.view(), *root.value_if(), *target, check, context.host);
    }

    /**
     * @brief Run every structured check of a case.
     * @par Requirements
     * P058; J5-070.
     * @par ID SWU-CLI-095
     */
    void run_checks(case_context& context, const value_view& expect, const parsed_document& parsed, input_view text,
                    syntax_mode syntax) noexcept
    {
      const auto checks = expect.find_member("checks");
      auto items = checks.has_value() ? checks.value_if()->children() : result<value_iterator>::success({});
      if (!checks.has_value() || !items.has_value())
      {
        return;
      }
      for (;;)
      {
        const auto check = items.value_if()->next();
        if (!check.has_value() || !check.value_if()->has_value())
        {
          return;
        }
        if (!evaluate_check(**check.value_if(), parsed, text, context, syntax))
        {
          short_text kind{};
          short_text description{};
          static_cast<void>(member_text(**check.value_if(), "check", kind));
          static_cast<void>(member_text(**check.value_if(), "description", description));
          mismatch(context, kind.view(), description.view());
        }
      }
    }

    /**
     * @brief Observed behavior of one case input.
     * @par Requirements
     * P058; J5-070.
     */
    struct observation
    {
        bool parsed{};
        std::optional<bool> valid_schema{}; // Validation verdict when a schema was applied.
        int exit = exit_failure;
        bool unsupported{};
        bool operational{};
    };

#if defined(ADAPTIVE_JSON5_CLI_HAS_SCHEMA)
    /**
     * @brief Validate a parsed case input against its schema file.
     * @par Requirements
     * P058; J5-063; J5-070.
     * @par ID SWU-CLI-096
     */
    observation validate_case(const case_context& context, std::string_view schema_path,
                              const document& instance) noexcept
    {
      observation seen{};
      seen.parsed = true;
      host_buffer schema_text{context.host.storage, context.selected.limits.max_input_bytes};
      std::size_t size = 0U;
      if ((schema_text.data() == nullptr) ||
          (read_input(context.host, schema_path, schema_text.data(), schema_text.size(), size) != io_status::ok))
      {
        seen.operational = true;
        return seen;
      }
      accounted_storage registry_storage{context.host.storage, c_manifest_bytes * 64U};
      auto registry = schema_registry::create(registry_storage, 1U);
      const bool added = registry.has_value() && registry.value_if()->add({schema_text.data(), size}).has_value();
      compile_options compile{};
      compile.result_storage = context.host.storage;
      compile.scratch_storage = context.host.storage;
      const auto compiled =
        added ? compile_schema(*registry.value_if(), c_default_schema_base_uri, schema_dialect::draft_07, compile)
              : result<compiled_schema, schema_error>::failure(schema_error::of(schema_error_code::invalid_schema));
      if (!compiled.has_value())
      {
        seen.operational = true;
        return seen;
      }
      validation_options run{};
      run.work_storage = context.host.storage;
      run.report_storage = context.host.storage;
      const auto verdict = validate(*compiled.value_if(), instance, run);
      if (!verdict.has_value())
      {
        seen.exit = (verdict.error_if()->code() == schema_error_code::unsupported_instance) ? 1 : 2;
        seen.operational = seen.exit == 2;
        return seen;
      }
      seen.valid_schema = verdict.value_if()->valid();
      seen.exit = *seen.valid_schema ? exit_ok : exit_invalid;
      return seen;
    }
#endif

    /**
     * @brief Build base + relative into buffer; empty when it does not fit.
     * @par Requirements
     * P058.
     * @par ID SWU-CLI-097
     */
    std::string_view join_path(std::string_view base, std::string_view relative,
                               std::array<char, c_path_capacity>& buffer) noexcept
    {
      if ((base.size() + relative.size()) > buffer.size())
      {
        return {};
      }
      std::size_t size = 0U;
      for (const std::string_view part : {base, relative})
      {
        for (const char c : part)
        {
          buffer[size] = c;
          ++size;
        }
      }
      return std::string_view{buffer.data(), size};
    }

    /**
     * @brief Compare the observation with the case expectations and run its checks.
     * @par Requirements
     * P058; J5-070.
     * @par ID SWU-CLI-098
     */
    void compare(case_context& context, const value_view& expect, const observation& seen,
                 const std::optional<parsed_document>& parsed, input_view text, syntax_mode syntax) noexcept
    {
      short_text want{};
      if (member_text(expect, "parse", want) && ((want.view() == "success") != seen.parsed))
      {
        mismatch(context, "parse",
                 seen.parsed ? "expected error, observed success" : "expected success, observed error");
      }
      if (member_text(expect, "validation", want) && seen.valid_schema.has_value() &&
          ((want.view() == "success") != *seen.valid_schema))
      {
        mismatch(context, "validation",
                 *seen.valid_schema ? "expected failure, observed success" : "expected success, observed failure");
      }
      const auto exit = member_count(expect, "exit");
      if (exit.has_value() && (*exit != static_cast<std::size_t>(seen.exit)))
      {
        mismatch(context, "exit", "observed exit differs from the expected exit");
      }
      if (parsed.has_value())
      {
        run_checks(context, expect, *parsed, text, syntax);
      }
    }

    /**
     * @brief Parse options of a case (syntax, duplicate policy, the run's limits).
     * @par Requirements
     * P058; J5-070.
     * @par ID SWU-CLI-107
     */
    parse_options case_options(const case_context& context, const value_view& item, std::string_view syntax) noexcept
    {
      short_text policy{};
      parse_options parse{};
      parse.syntax = (syntax == "json") ? syntax_mode::strict_json : syntax_mode::json5;
      parse.duplicate_names = (member_text(item, "duplicates", policy) && (policy.view() == "reject"))
                                ? duplicate_name_policy::reject_duplicates
                                : duplicate_name_policy::preserve_all;
      parse.limits = context.selected.limits;
      return parse;
    }

    /**
     * @brief Observation of a parse result (exit 0, 1 or 2 like validate).
     * @par Requirements
     * P058; J5-070.
     * @par ID SWU-CLI-108
     */
    observation observe_parse(const error* failure) noexcept
    {
      observation seen{};
      if (failure == nullptr)
      {
        seen.parsed = true;
        seen.exit = exit_ok;
        return seen;
      }
      const category kind = category_of(failure->code());
      const bool invalid = (kind == category::syntax) || (kind == category::encoding) ||
                           (kind == category::duplicate) || (kind == category::security);
      seen.exit = invalid ? exit_invalid : exit_failure;
      seen.operational = !invalid;
      return seen;
    }

    /**
     * @brief Parse and (optionally) validate one case input, then compare with the expectations.
     * @par Requirements
     * P058; J5-070.
     * @par ID SWU-CLI-099
     */
    case_result evaluate_case(case_context& context, const value_view& item) noexcept
    {
      short_text input{};
      short_text syntax_name{};
      short_text schema{};
      const auto expect = item.find_member("expect");
      if (!member_text(item, "input", input) || !member_text(item, "syntax", syntax_name) || !expect.has_value())
      {
        return case_result::error;
      }
      const bool has_schema = member_text(item, "schema", schema);
#if !defined(ADAPTIVE_JSON5_CLI_HAS_SCHEMA)
      if (has_schema)
      {
        return case_result::unsupported;
      }
#endif
      std::array<char, c_path_capacity> path{};
      host_buffer bytes{context.host.storage, context.selected.limits.max_input_bytes};
      std::size_t size = 0U;
      const std::string_view input_path = join_path(context.base, input.view(), path);
      if ((bytes.data() == nullptr) || input_path.empty() ||
          (read_input(context.host, input_path, bytes.data(), bytes.size(), size) != io_status::ok))
      {
        return case_result::error;
      }
      const parse_options parse = case_options(context, item, syntax_name.view());
      accounted_storage accounting{context.host.storage, parse.limits.max_owned_bytes};
      const input_view text{bytes.data(), size};
      auto parsed_result = parse_document(text, parse, accounting);
      std::optional<parsed_document> parsed{};
      observation seen = observe_parse(parsed_result.error_if());
      if (parsed_result.has_value())
      {
        parsed.emplace(std::move(*parsed_result.value_if()));
      }
#if defined(ADAPTIVE_JSON5_CLI_HAS_SCHEMA)
      std::array<char, c_path_capacity> schema_path{};
      seen = (has_schema && parsed.has_value())
               ? validate_case(context, join_path(context.base, schema.view(), schema_path), parsed->values)
               : seen;
#endif
      if (seen.operational)
      {
        return case_result::error;
      }
      compare(context, *expect.value_if(), seen, parsed, text, parse.syntax);
      return (context.mismatches == 0U) ? case_result::pass : case_result::fail;
    }

    /**
     * @brief Counters of a run.
     * @par Requirements
     * P058.
     */
    struct run_totals
    {
        std::size_t passed{};
        std::size_t failed{};
        std::size_t unsupported{};
        std::size_t errors{};
    };

    /**
     * @brief Result of a case by its input path (for the requirement summary).
     * @par Requirements
     * P058; J5-094.
     * @par ID SWU-CLI-100
     */
    std::optional<case_result> result_for_input(const value_view& cases, std::string_view input, const char* results,
                                                std::size_t count) noexcept
    {
      std::optional<case_result> combined{};
      for (std::size_t index = 0U; index < count; ++index)
      {
        const auto item = cases.element_at(index);
        short_text path{};
        if (!item.has_value() || !member_text(*item.value_if(), "input", path) || (path.view() != input))
        {
          continue;
        }
        const auto result = static_cast<case_result>(results[index]);
        if (!combined.has_value() || (result == case_result::fail) || (result == case_result::error) ||
            ((result == case_result::pass) && (*combined == case_result::unsupported)))
        {
          combined = result;
        }
      }
      return combined;
    }

    /**
     * @brief Combined result of the cases of one requirement row (worst wins).
     * @par Requirements
     * P058; J5-094.
     * @par ID SWU-CLI-109
     */
    std::string_view requirement_status(const value_view& row, const value_view& cases, const char* results,
                                        std::size_t count) noexcept
    {
      std::optional<case_result> combined{};
      const auto inputs = row.find_member("starter_inputs");
      const auto inputs_size =
        inputs.has_value() ? inputs.value_if()->container_size() : result<std::size_t>::success(0U);
      const std::size_t inputs_count = inputs_size.has_value() ? *inputs_size.value_if() : 0U;
      for (std::size_t index = 0U; index < inputs_count; ++index)
      {
        short_text input{};
        const auto element = inputs.value_if()->element_at(index);
        const auto one = (element.has_value() && copy_text(*element.value_if(), input))
                           ? result_for_input(cases, input.view(), results, count)
                           : std::nullopt;
        const bool worse =
          one.has_value() && (!combined.has_value() || (*one == case_result::fail) || (*one == case_result::error));
        combined = worse ? one : combined;
      }
      if (!combined.has_value())
      {
        return "not demonstrated by file cases";
      }
      if (*combined == case_result::pass)
      {
        return "demonstrated";
      }
      return (*combined == case_result::unsupported) ? "unsupported" : "failed";
    }

    /**
     * @brief Write the per-requirement summary.
     * @par Requirements
     * P058; J5-094.
     * @par ID SWU-CLI-101
     */
    void summarise_requirements(text_writer& out, bool json, const value_view& manifest, const value_view& cases,
                                const char* results, std::size_t count) noexcept
    {
      const auto rows = manifest.find_member("requirements");
      auto items = rows.has_value() ? rows.value_if()->children() : result<value_iterator>::success({});
      std::size_t written = 0U;
      while (rows.has_value() && items.has_value())
      {
        const auto row = items.value_if()->next();
        if (!row.has_value() || !row.value_if()->has_value())
        {
          return;
        }
        short_text id{};
        static_cast<void>(member_text(**row.value_if(), "requirement", id));
        const std::string_view status = requirement_status(**row.value_if(), cases, results, count);
        if (json)
        {
          out.put((written == 0U) ? "{\"requirement\": " : ", {\"requirement\": ");
          put_json_text(out, id.view());
          out.put(", \"status\": \"").put(status).put("\"}");
        }
        else
        {
          out.put("  ").put(id.view()).put(": ").put(status).put("\n");
        }
        ++written;
      }
    }

    /**
     * @brief Report a run-level operational failure (manifest unreadable or malformed); exit 2.
     * @par Requirements
     * P058; J5-070.
     * @par ID SWU-CLI-102
     */
    int run_failure(const options& selected, const environment& host, category kind, std::string_view message) noexcept
    {
      reporter failures{host.output, selected.diagnostics, "run", selected.manifest};
      failures.add(diagnostic{severity::error, kind, message});
      return failures.finish(outcome::error, exit_failure);
    }

    /**
     * @brief Write the header of one case and evaluate it.
     * @par Requirements
     * P058; J5-070.
     * @par ID SWU-CLI-103
     */
    case_result run_one(case_context& context, const value_view& item, std::size_t index) noexcept
    {
      short_text id{};
      static_cast<void>(member_text(item, "id", id));
      context.id = id.view();
      context.mismatches = 0U;
      if (context.json)
      {
        context.out.put((index == 0U) ? "{\"id\": " : ", {\"id\": ");
        put_json_text(context.out, id.view());
        context.out.put(", \"mismatches\": [");
      }
      const case_result result = evaluate_case(context, item);
      constexpr std::array<std::string_view, 4> c_names{{"pass", "fail", "unsupported", "error"}};
      const std::string_view name = c_names[static_cast<std::size_t>(result)];
      if (context.json)
      {
        context.out.put("], \"result\": \"").put(name).put("\"}");
      }
      else
      {
        constexpr std::array<std::string_view, 4> c_labels{{"PASS ", "FAIL ", "UNSUPPORTED ", "ERROR "}};
        context.out.put(c_labels[static_cast<std::size_t>(result)]).put(id.view()).put("\n");
      }
      return result;
    }

    /**
     * @brief Evaluate every case, store the results and count them.
     * @par Requirements
     * P058; J5-070.
     * @par ID SWU-CLI-104
     */
    run_totals run_cases(case_context& context, const value_view& cases, char* results, std::size_t count) noexcept
    {
      run_totals totals{};
      for (std::size_t index = 0U; index < count; ++index)
      {
        const auto item = cases.element_at(index);
        const case_result result = item.has_value() ? run_one(context, *item.value_if(), index) : case_result::error;
        results[index] = static_cast<char>(result);
        totals.passed += (result == case_result::pass) ? 1U : 0U;
        totals.failed += (result == case_result::fail) ? 1U : 0U;
        totals.unsupported += (result == case_result::unsupported) ? 1U : 0U;
        totals.errors += (result == case_result::error) ? 1U : 0U;
      }
      return totals;
    }
    /**
     * @brief Write the end of a run report and return its exit code.
     * @par Requirements
     * P058; J5-070; J5-071.
     * @par ID SWU-CLI-110
     */
    int write_footer(text_writer& out, bool json, const run_totals& totals) noexcept
    {
      const int exit = (totals.errors != 0U) ? exit_failure : ((totals.failed != 0U) ? exit_invalid : exit_ok);
      if (json)
      {
        out.put("], \"summary\": {\"passed\": ")
          .put_count(totals.passed)
          .put(", \"failed\": ")
          .put_count(totals.failed);
        out.put(", \"unsupported\": ").put_count(totals.unsupported).put(", \"errors\": ").put_count(totals.errors);
        out.put("}, \"result\": \"").put((exit == exit_ok) ? "pass" : ((exit == exit_invalid) ? "fail" : "error"));
        out.put("\", \"exit\": ").put_count(static_cast<std::size_t>(exit)).put("}\n");
      }
      else
      {
        out.put_count(totals.passed).put(" passed, ").put_count(totals.failed).put(" failed, ");
        out.put_count(totals.unsupported).put(" unsupported, ").put_count(totals.errors).put(" errors\n");
      }
      return out.ok() ? exit : static_cast<int>(exit_failure);
    }

    /**
     * @brief Parse options for a manifest: strict JSON with limits sized for c_manifest_bytes.
     * @par Requirements
     * P058; J5-047.
     * @par ID SWU-CLI-111
     */
    parse_options manifest_options() noexcept
    {
      parse_options parse{};
      parse.syntax = syntax_mode::strict_json;
      parse.limits.max_input_bytes = c_manifest_bytes;
      parse.limits.max_nodes = c_manifest_bytes;
      parse.limits.max_members = c_manifest_bytes;
      parse.limits.max_total_string_units = c_manifest_bytes;
      parse.limits.max_owned_bytes = c_manifest_bytes * 64U;
      parse.limits.max_work_units = c_manifest_bytes * 64U;
      return parse;
    }
  } // namespace

  /**
   * @brief Resolve an RFC 6901 JSON Pointer in a parsed document.
   * @par Requirements
   * P058; J5-070.
   * @par ID SWU-CLI-079
   */
  std::optional<value_view> resolve_pointer(const value_view& root, std::string_view pointer) noexcept
  {
    if (pointer.empty())
    {
      return root;
    }
    if (pointer[0U] != '/')
    {
      return std::nullopt;
    }
    std::optional<value_view> current{root};
    std::string_view rest = pointer.substr(1U);
    for (;;)
    {
      const std::size_t slash = rest.find('/');
      short_text token{};
      if (!decode_token(rest.substr(0U, slash), token))
      {
        return std::nullopt;
      }
      current = step(*current, token.view());
      if (!current.has_value() || (slash == std::string_view::npos))
      {
        return current;
      }
      rest = rest.substr(slash + 1U);
    }
  }

  /**
   * @brief The run command.
   * @par Requirements
   * P058; J5-070; J5-071; J5-094.
   * @par ID SWU-CLI-080
   */
  int run_manifest(const options& selected, const environment& host) noexcept
  {
    host_buffer bytes{host.storage, c_manifest_bytes};
    std::size_t size = 0U;
    if (bytes.data() == nullptr)
    {
      return run_failure(selected, host, category::allocation, "out of memory");
    }
    const io_status read = read_input(host, selected.manifest, bytes.data(), bytes.size(), size);
    if (read != io_status::ok)
    {
      return run_failure(selected, host, category::io, describe_io(read));
    }
    const parse_options parse = manifest_options();
    accounted_storage accounting{host.storage, parse.limits.max_owned_bytes};
    const auto manifest = parse_document({bytes.data(), size}, parse, accounting);
    if (!manifest.has_value())
    {
      reporter failures{host.output, selected.diagnostics, "run", selected.manifest};
      return finish_with_configuration_failure(failures, *manifest.error_if(), {bytes.data(), size});
    }
    const auto root =
      manifest.has_value() ? manifest.value_if()->values.root() : result<value_view>::failure(c_not_a_manifest);
    const auto cases =
      root.has_value() ? root.value_if()->find_member("cases") : result<value_view>::failure(c_not_a_manifest);
    const auto count =
      cases.has_value() ? cases.value_if()->container_size() : result<std::size_t>::failure(c_not_a_manifest);
    if (!count.has_value())
    {
      return run_failure(selected, host, category::syntax, "manifest is not a valid case list");
    }
    host_buffer results{host.storage, *count.value_if() + 1U};
    if (results.data() == nullptr)
    {
      return run_failure(selected, host, category::allocation, "out of memory");
    }
    const std::size_t last = selected.manifest.find_last_of("/\\");
    text_writer out{host.output, output_stream::out};
    const bool json = selected.diagnostics == diagnostics_format::json;
    case_context context{selected,
                         host,
                         out,
                         json,
                         (last == std::string_view::npos) ? std::string_view{}
                                                          : selected.manifest.substr(0U, last + 1U),
                         {}};
    if (json)
    {
      out.put("{\"format\": \"adaptive-json5-run\", \"version\": ").put_count(c_format_version);
      out.put(", \"manifest\": ");
      put_json_text(out, selected.manifest);
      out.put(", \"cases\": [");
    }
    const run_totals totals = run_cases(context, *cases.value_if(), results.data(), *count.value_if());
    out.put(json ? "], \"requirements\": [" : "requirements:\n");
    summarise_requirements(out, json, *root.value_if(), *cases.value_if(), results.data(), *count.value_if());
    return write_footer(out, json, totals);
  }
} // namespace adaptive_json5::cli
