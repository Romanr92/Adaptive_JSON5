// SPDX-License-Identifier: Apache-2.0
#include "adaptive_json5/detail/parser/primitive_parser.hpp"

#include "adaptive_json5/detail/common/checked_size.hpp"
#include "adaptive_json5/detail/input/cursor.hpp"
#include "adaptive_json5/detail/lexer/number/number_token.hpp"
#include "adaptive_json5/detail/lexer/scalar_lexer.hpp"
#include "adaptive_json5/detail/lexer/scalar_token.hpp"
#include "adaptive_json5/detail/lexer/string/string_token.hpp"
#include "adaptive_json5/detail/lexer/trivia.hpp"
#include "adaptive_json5/detail/memory/resource_budget.hpp"
#include "adaptive_json5/detail/memory/work_budget.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/input.hpp"
#include "adaptive_json5/memory/accounted_storage.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/document.hpp"
#include "adaptive_json5/value/owned_number.hpp"
#include "adaptive_json5/value/value_index.hpp"

#include <cstddef>
#include <optional>
#include <utility>
#include <variant>

namespace adaptive_json5
{
  namespace detail
  {
    result<value_index> append_primitive_token(document& destination, scalar_token token, input_view input,
                                               const parse_options& options, const storage_callbacks& storage) noexcept
    {
      if (auto* string = std::get_if<string_token>(&token); string != nullptr)
      {
        return destination.make_string(std::move(string->value));
      }

      if (const auto* number = std::get_if<number_token>(&token); number != nullptr)
      {
        // Validate the borrowed range before pointer arithmetic or copying.
        if ((input.data == nullptr) || (number->byte_offset > input.size) ||
            (number->byte_length > (input.size - number->byte_offset)))
        {
          return result<value_index>::failure(error::without_location(error_code::wrong_type_or_access));
        }

        const input_view spelling{input.data + number->byte_offset, number->byte_length};

        auto owned = owned_number::copy_spelling(spelling, options.syntax, storage, options.limits.max_number_bytes);

        if (!owned.has_value())
        {
          auto reason = *owned.error_if();

          if (reason.has_byte_offset)
          {
            // Reject locations outside the substring before rebasing them.
            if (reason.byte_offset > spelling.size)
            {
              return result<value_index>::failure(error::without_location(error_code::wrong_type_or_access));
            }

            // Translate the substring-relative offset to the original input.
            reason.byte_offset += number->byte_offset;
          }

          return result<value_index>::failure(reason);
        }

        // Reject metadata inconsistent with the independently checked spelling.
        if (owned.value_if()->kind() != number->kind)
        {
          return result<value_index>::failure(error::without_location(error_code::wrong_type_or_access));
        }

        return destination.make_number(std::move(*owned.value_if()));
      }

      if (const auto* literal = std::get_if<literal_token>(&token); literal != nullptr)
      {
        switch (literal->kind)
        {
        case literal_kind::null_literal:
          return destination.make_null();

        case literal_kind::true_literal:
          return destination.make_boolean(true);

        case literal_kind::false_literal:
          return destination.make_boolean(false);
        }
      }

      return result<value_index>::failure(error::without_location(error_code::wrong_type_or_access));
    }
  } // namespace detail

  result<std::optional<primitive_value>> consume_primitive(document& destination, cursor& source, input_view input,
                                                           const parse_options& options,
                                                           const storage_callbacks& storage, work_budget& work) noexcept
  {
    using primitive_result = result<std::optional<primitive_value>>;

    if (((input.data == nullptr) && (input.size != 0)) || (source.offset() > input.size))
    {
      return primitive_result::failure(error::without_location(error_code::wrong_type_or_access));
    }

    // Admit recognition before its scanning or temporary allocations begin.
    const auto recognition = work.charge(source.remaining());

    if (!recognition.has_value())
    {
      return primitive_result::failure(*recognition.error_if());
    }

    // Recognition must not publish cursor advancement before ownership succeeds.
    cursor candidate = source;

    // A primitive root can contain only one owned string value.
    const std::size_t string_limit = (options.limits.max_string_units < options.limits.max_total_string_units)
                                       ? options.limits.max_string_units
                                       : options.limits.max_total_string_units;

    // Temporary identifier decoding cannot produce more units than input bytes.
    auto token =
      consume_scalar(candidate, options.syntax, storage, string_limit, input.size, options.limits.max_exponent_digits);

    if (!token.has_value())
    {
      return primitive_result::failure(*token.error_if());
    }

    if (!token.value_if()->has_value())
    {
      return primitive_result::success(std::nullopt);
    }

    primitive_value value{};
    value.byte_offset = source.offset();
    value.byte_length = candidate.offset() - source.offset();

    // Preserve metadata before transferring the token's owned string content.
    if (const auto* string = std::get_if<string_token>(&**token.value_if()); string != nullptr)
    {
      constexpr std::size_t c_retained_warning_entries = 1;

      // Reject diagnostic publication before changing the destination document.
      if ((string->first_warning.has_value()) && (c_retained_warning_entries > options.limits.max_diagnostics))
      {
        return primitive_result::failure(
          error::at_offset(error_code::resource_limit, string->first_warning->byte_offset));
      }

      value.warning_count = string->warning_count;
      value.first_warning = string->first_warning;
    }

    constexpr std::size_t c_new_record_count = 1;

    const auto records = checked_add(destination.size(), c_new_record_count);

    if (!records.has_value())
    {
      return primitive_result::failure(*records.error_if());
    }

    const auto construction_units = checked_add(value.byte_length, *records.value_if());

    if (!construction_units.has_value())
    {
      return primitive_result::failure(*construction_units.error_if());
    }

    // Admit copying and possible record growth before changing the document.
    const auto construction = work.charge(*construction_units.value_if());

    if (!construction.has_value())
    {
      return primitive_result::failure(*construction.error_if());
    }

    const auto appended =
      detail::append_primitive_token(destination, std::move(**token.value_if()), input, options, storage);

    if (!appended.has_value())
    {
      return primitive_result::failure(*appended.error_if());
    }

    value.index = *appended.value_if();

    auto completed = primitive_result::success(std::optional<primitive_value>{value});

    // Publish cursor advancement only after construction of the success result.
    source = candidate;

    return completed;
  }

  result<primitive_document> parse_primitive_document(input_view input, const parse_options& options,
                                                      accounted_storage& accounting) noexcept
  {
    using document_result = result<primitive_document>;

    if ((input.data == nullptr) && (input.size != 0))
    {
      return document_result::failure(error::without_location(error_code::wrong_type_or_access));
    }

    // Reject inconsistent configuration before processing or allocating.
    if (accounting.byte_limit() != options.limits.max_owned_bytes)
    {
      return document_result::failure(error::without_location(error_code::wrong_type_or_access));
    }

    const resource_budget budget{options.limits};

    work_budget work{options.limits.max_work_units};

    const auto input_size = budget.check_input(input.size);

    if (!input_size.has_value())
    {
      return document_result::failure(*input_size.error_if());
    }

    // Every allocation in this call must retain the same accounting context.
    const storage_callbacks storage = accounting.callbacks();

    // Keep ownership private until the complete root and suffix are validated.
    document candidate{storage, options.limits.max_nodes};
    cursor source{input};

    // Admit the complete available range before scanning leading trivia.
    const auto leading_work = work.charge(source.remaining());

    if (!leading_work.has_value())
    {
      return document_result::failure(*leading_work.error_if());
    }

    const auto leading = skip_trivia(source, options.syntax);

    if (!leading.has_value())
    {
      return document_result::failure(*leading.error_if());
    }

    const auto parsed = consume_primitive(candidate, source, input, options, storage, work);

    if (!parsed.has_value())
    {
      return document_result::failure(*parsed.error_if());
    }

    if (!parsed.value_if()->has_value())
    {
      return document_result::failure(error::at_offset(error_code::syntax, source.offset()));
    }

    // Admit the remaining suffix before scanning trailing trivia.
    const auto trailing_work = work.charge(source.remaining());

    if (!trailing_work.has_value())
    {
      return document_result::failure(*trailing_work.error_if());
    }

    const auto trailing = skip_trivia(source, options.syntax);

    if (!trailing.has_value())
    {
      return document_result::failure(*trailing.error_if());
    }

    if (!source.is_at_end())
    {
      return document_result::failure(error::at_offset(error_code::syntax, source.offset()));
    }

    const primitive_value root = **parsed.value_if();

    // Retain the root location before publishing the owned document.
    const auto recorded = candidate.set_source_span(
      root.index, adaptive_json5::source_span{root.byte_offset, root.byte_length}, input.size);

    if (!recorded.has_value())
    {
      return document_result::failure(*recorded.error_if());
    }

    const auto established = candidate.set_root(root.index);

    if (!established.has_value())
    {
      return document_result::failure(*established.error_if());
    }

    return document_result::success(primitive_document{std::move(candidate), root});
  }
} // namespace adaptive_json5
