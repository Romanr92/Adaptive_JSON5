#include "parser/primitive_parser.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/input.hpp"
#include "adaptive_json5/memory/accounted_storage.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/document.hpp"
#include "adaptive_json5/value/owned_number.hpp"
#include "adaptive_json5/value/value_index.hpp"
#include "common/checked_size.hpp"
#include "input/cursor.hpp"
#include "lexer/number/number_token.hpp"
#include "lexer/scalar_lexer.hpp"
#include "lexer/scalar_token.hpp"
#include "lexer/string/string_token.hpp"
#include "lexer/trivia.hpp"
#include "memory/resource_budget.hpp"
#include "memory/work_budget.hpp"
#include "parser/parser_work.hpp"

#include <cstddef>
#include <optional>
#include <utility>
#include <variant>

namespace adaptive_json5
{
  namespace detail
  {
    /**
     * @brief Transfer one completed primitive token into a detached value.
     * @par Requirements
     * P032; J5-023; J5-028; J5-034; J5-040.
     * @param destination Document receiving the detached value.
     * @param token Completed token, consumed on entry.
     * @param input Original input used to recognize the token.
     * @param options Syntax and numeric spelling limit.
     * @param storage Provider for the owned numeric spelling.
     * @return New value index or an explicit failure.
     * @note Tokens must originate from the scalar lexer using this input
     * and syntax mode. Input must remain alive and unchanged during the call.
     * String content is transferred; numeric spelling is independently copied.
     * Provider state must outlive any transferred or copied allocation.
     * Failure leaves existing document values and its root unchanged.
     * Consumed string content may be released on failure.
     * Does not establish a root, advance a cursor or parse containers.
     * Source spans and string-warning publication are handled by composition.
     * Does not itself integrate owned-byte or work accounting.
     */
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

  /**
   * @brief Recognize and construct one detached primitive using shared work accounting.
   * @par Requirements
   * P032; J5-017; J5-023; J5-028; J5-040; J5-041; J5-047; J5-049.
   * @param destination Document receiving the constructed value.
   * @param source Cursor positioned after caller-managed leading trivia.
   * @param input Exact original input underlying source.
   * @param options Syntax, string-content and numeric component limits.
   * @param storage Provider for lexical storage and owned numeric spelling.
   * @param work Shared work budget for the enclosing operation.
   * @return Constructed primitive, successful nullopt at EOF or structural
   * punctuation, or an explicit failure.
   * @note Input must remain alive and unchanged throughout this call.
   * No-match and failure leave source and destination values unchanged.
   * Successful work charges remain consumed on no-match or failure.
   * Success appends one detached value and advances source past its token.
   * Does not set the root, skip trivia or require complete input consumption.
   * Containers are not parsed; structural punctuation remains unconsumed.
   * Warning metadata is returned without invoking warning callbacks.
   * Record source spans are not assigned by this component.
   * Each string is bounded by both per-string and total string limits.
   * Repeated calls do not share aggregate string or diagnostic accounting.
   * Temporary identifier units are bounded separately by input size.
   * A retained first-warning descriptor consumes one diagnostic entry.
   * warning_count summarizes occurrences without retaining additional entries.
   * Diagnostic checks bound returned metadata, not temporary lexical metadata.
   * Recognition charges one unit per remaining input byte before scanning.
   * Construction charges token bytes plus c_record_construction_work.
   * Construction charges conservatively include possible record relocation.
   * These are phase-accounting units, not measured execution time.
   */
  result<std::optional<primitive_value>> consume_primitive(document& destination, cursor& source, input_view input,
                                                           const parse_options& options,
                                                           const storage_callbacks& storage, work_budget& work) noexcept
  {
    using primitive_result = result<std::optional<primitive_value>>;

    if (((input.data == nullptr) && (input.size != 0)) || (source.offset() > input.size))
    {
      return primitive_result::failure(error::without_location(error_code::wrong_type_or_access));
    }

    // A primitive root can contain only one owned string value.
    const std::size_t string_limit = (options.limits.max_string_units < options.limits.max_total_string_units)
                                       ? options.limits.max_total_string_units
                                       : options.limits.max_total_string_units;

    // Recognition must not publish cursor advancement before ownership succeeds.
    const auto started = detail::begin_limited_scan(input, source.offset(), work);

    if (!started.has_value())
    {
      return primitive_result::failure(*started.error_if());
    }

    detail::limited_scan scan = *started.value_if();

    // temporary identifier decoding cannot produce more units than input bytes.
    auto token = consume_scalar(scan.source, options.syntax, storage, string_limit, input.size,
                                options.limits.max_exponent_digits);

    if (!token.has_value())
    {
      return primitive_result::failure(detail::limited_scan_error(scan, *token.error_if()));
    }

    // Charge before the no-match check, so an exhausted budget reports resource_limit.
    const auto recognition = detail::finish_limited_scan(scan, scan.source.offset(), work);

    if (!recognition.has_value())
    {
      return primitive_result::failure(*recognition.error_if());
    }

    if (!token.value_if()->has_value())
    {
      return primitive_result::success(std::nullopt);
    }

    primitive_value value{};
    value.byte_offset = source.offset();
    value.byte_length = scan.source.offset() - source.offset();

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

    const auto construction_units = checked_add(value.byte_length, detail::c_record_construction_work);

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
    source.advance(value.byte_length);

    return completed;
  }

  /**
   * @brief Parse exactly one primitive root with surrounding mode-valid trivia.
   * @par Requirements
   * P032; J5-009; J5-023; J5-027; J5-028; J5-040; J5-041; J5-046; J5-047; J5-049.
   * @param input Borrowed complete input, unchanged throughout this call.
   * @param options Syntax and configured component limits.
   * @param accounting Caller-owned allocation accounting with a stable address.
   * @return Complete owned primitive document or an explicit failure.
   * @note accounting and its provider state must outlive the returned document.
   * Its byte ceiling must equal options.limits.max_owned_bytes.
   * Existing live allocations through accounting share that ceiling.
   * Rejects empty input, additional tokens and container roots.
   * Failure releases this call's temporary allocations and publishes no document.
   * Does not invoke warning callbacks; root metadata retains warning information.
   * Checks input bytes and uses existing node/string/numeric component limits.
   * Lexical, numeric-spelling and document-record allocations use accounting.
   * The single root string obeys per-string and total decoded-unit limits.
   * Decimal exponent digits are checked during recognition.
   * The optional first-warning descriptor obeys the retained diagnostic limit.
   * Each trivia scan charges one unit per remaining input byte before scanning.
   * Primitive recognition and construction charge the same shared work budget.
   * Charges are conservative phase allowances, not measured execution time.
   * Successful charges remain consumed when a later phase fails.
   * Cleanup releases owned storage without requiring additional work charges.
   * The root span is stored in its value record and returned root metadata.
   */
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

    const auto leading = detail::skip_trivia_within_budget(source, input, options.syntax, work);

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
      return document_result::failure(error::syntax_at(source.offset(), source.is_at_end()
                                                                          ? syntax_error_reason::empty_input
                                                                          : syntax_error_reason::unexpected_character));
    }

    const auto trailing = detail::skip_trivia_within_budget(source, input, options.syntax, work);

    if (!trailing.has_value())
    {
      return document_result::failure(*trailing.error_if());
    }

    if (!source.is_at_end())
    {
      return document_result::failure(error::syntax_at(source.offset(), syntax_error_reason::trailing_content));
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
