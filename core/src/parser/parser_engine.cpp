#include "parser/parser_engine.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/input.hpp"
#include "adaptive_json5/memory/accounted_storage.hpp"
#include "adaptive_json5/memory/owned_block.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/syntax_error_reason.hpp"
#include "adaptive_json5/value/document.hpp"
#include "adaptive_json5/value/string_warning.hpp"
#include "adaptive_json5/value/value_index.hpp"
#include "adaptive_json5/value/warning_list.hpp"
#include "common/checked_size.hpp"
#include "input/cursor.hpp"
#include "lexer/member_name_lexer.hpp"
#include "lexer/punctuation.hpp"
#include "lexer/scalar_lexer.hpp"
#include "lexer/scalar_token.hpp"
#include "lexer/string/string_boundary.hpp"
#include "lexer/string/string_lexer.hpp"
#include "lexer/string/string_token.hpp"
#include "memory/resource_budget.hpp"
#include "memory/work_budget.hpp"
#include "parser/parser_attachment.hpp"
#include "parser/parser_container_close.hpp"
#include "parser/parser_container_open.hpp"
#include "parser/parser_dispatch.hpp"
#include "parser/parser_duplicate.hpp"
#include "parser/parser_frame.hpp"
#include "parser/parser_stack.hpp"
#include "parser/parser_work.hpp"
#include "parser/primitive_parser.hpp"
#include "value/document_access.hpp"

#include <cstddef>
#include <optional>
#include <utility>
#include <variant>

namespace adaptive_json5
{
  namespace detail
  {
    namespace
    {
      constexpr std::size_t c_byte_order_mark_bytes = 3;

      /**
       * @brief Carry the destination list and any append failure through the warning callback.
       * @par Requirements
       * P039; J5-017.
       */
      struct warning_sink
      {
          warning_list* list;
          bool failed{};
      };

      /**
       * @brief Report whether input begins with the UTF-8 encoding of U+FEFF.
       * @par Requirements
       * P040; J5-025.
       * @param input Borrowed original input.
       * @return True when the first three bytes are EF BB BF.
       * @note Compares unsigned byte values, because char may be signed.
       */
      [[nodiscard]] bool starts_with_byte_order_mark(input_view input) noexcept
      {
        if (input.size < c_byte_order_mark_bytes)
        {
          return false;
        }

        const auto* bytes = reinterpret_cast<const unsigned char*>(input.data);

        return (bytes[0] == 0xEFU) && (bytes[1] == 0xBBU) && (bytes[2] == 0xBFU);
      }

      /**
       * @brief Append one re-reported warning into previously reserved capacity.
       * @par Requirements
       * P039; J5-017.
       * @param context Borrowed warning_sink.
       * @param warning Warning reported by the string walk.
       * @note The callback type cannot return a failure, so one is recorded in the sink instead.
       */
      void collect_warning(void* context, const string_warning& warning) noexcept
      {
        auto& sink = *static_cast<warning_sink*>(context);

        if (!sink.list->append_reserved(warning).has_value())
        {
          sink.failed = true;
        }
      }
    } // namespace

    /**
     * @brief Start a container or construct and accept a scalar value.
     * @par Requirements
     * P033; P034; P035; J5-013; J5-027; J5-028; J5-048.
     * @note Reserve node/member resources before construction and depth
     * before growing container frames.
     * Scalar decoding uses the remaining aggregate string allowance.
     * Failure preserves the source cursor but may change private state;
     * the failed engine must be abandoned.
     * Failed container opening releases its matching reservations.
     * Successful work charges are not refunded.
     */
    status_result parser_engine::read_value() noexcept
    {
      if (m_root.is_set())
      {
        return status_result::failure(error::without_location(error_code::wrong_type_or_access));
      }

      bool member = false;

      // Inspect the parent only before any operation that can grow frames.
      if (const auto* parent = m_frames.top(); parent != nullptr)
      {
        switch (parent->state)
        {
        case parser_state::array_first_or_end:
        case parser_state::array_value_after_comma:
          if (parent->pending_name.has_value())
          {
            return status_result::failure(error::without_location(error_code::wrong_type_or_access));
          }
          break;

        case parser_state::object_value:
          if (!parent->pending_name.has_value())
          {
            return status_result::failure(error::without_location(error_code::wrong_type_or_access));
          }

          member = true;
          break;

        default:
          return status_result::failure(error::without_location(error_code::wrong_type_or_access));
        }
      }

      const auto current = m_source.peek();

      if (!current.has_value())
      {
        // Without an open container no root value was started, so the document itself is empty.
        return status_result::failure(error::syntax_at(m_source.offset(), (m_frames.top() == nullptr)
                                                                            ? syntax_error_reason::empty_input
                                                                            : syntax_error_reason::unexpected_end));
      }

      const auto reserved_value = m_resources.reserve_value(member);

      if (!reserved_value.has_value())
      {
        return status_result::failure(*reserved_value.error_if());
      }

      if ((*current == '[') || (*current == '{'))
      {
        const auto entered = m_resources.enter_container();

        if (!entered.has_value())
        {
          // No record was created, so undo the value/member reservation.
          const auto released = m_resources.release_value(member);

          if (!released.has_value())
          {
            return status_result::failure(*released.error_if());
          }

          return status_result::failure(*entered.error_if());
        }

        cursor candidate = m_source;

        // The byte check above guarantees a recognized opening token.
        const auto opening = consume_punctuation(candidate);

        const auto opened = open_container(m_document, m_frames, *opening, m_work);

        if (!opened.has_value())
        {
          // open container already removed any provisional frame.
          const auto depth_released = m_resources.leave_container();
          const auto value_released = m_resources.release_value(member);

          if (!depth_released.has_value())
          {
            return status_result::failure(*depth_released.error_if());
          }

          if (!value_released.has_value())
          {
            return status_result::failure(*value_released.error_if());
          }

          return status_result::failure(*opened.error_if());
        }

        // The new container remains detached until finish_container().
        m_source = candidate;
        return status_result::success();
      }

      const auto& limits = m_options.limits;

      // The budget owns the aggregate counter, so it alone derives the next string's limit.
      const std::size_t string_limit = m_resources.string_unit_allowance();

      const auto started = begin_limited_scan(m_input, m_source.offset(), m_work);

      if (!started.has_value())
      {
        return status_result::failure(*started.error_if());
      }

      limited_scan scan = *started.value_if();

      // Temporary identifier decoding is separate from retained string content.
      auto recognized = consume_scalar(scan.source, m_options.syntax, m_storage, string_limit, m_input.size,
                                       limits.max_exponent_digits);

      if (!recognized.has_value())
      {
        // A failure at the cut may only mean the budget hid the rest of the token.
        return status_result::failure(limited_scan_error(scan, *recognized.error_if()));
      }

      // Charge before the no-match check, so an exhausted budget reports resource_limit.
      const auto recognition = finish_limited_scan(scan, scan.source.offset(), m_work);

      if (!recognition.has_value())
      {
        return status_result::failure(*recognition.error_if());
      }

      if (!recognized.value_if()->has_value())
      {
        return status_result::failure(error::syntax_at(m_source.offset(), syntax_error_reason::unexpected_character));
      }

      auto& token = **recognized.value_if();

      std::size_t string_units = 0;
      std::size_t string_offset = 0;
      std::size_t string_length = 0;
      std::size_t warning_count = 0;

      // Capture metadata before construction transfers the token's string.
      if (const auto* string = std::get_if<string_token>(&token); string != nullptr)
      {
        string_units = string->value.size();
        string_offset = string->byte_offset;
        string_length = string->byte_length;
        warning_count = string->warning_count;
      }

      // The lexer walked the string twice, counting then decoding, so its bytes are charged again.
      const auto decoded_charged = m_work.charge(string_length);

      if (!decoded_charged.has_value())
      {
        return status_result::failure(*decoded_charged.error_if());
      }

      const auto reserved_string = m_resources.reserve_string_units(0, string_units);

      if (!reserved_string.has_value())
      {
        return status_result::failure(*reserved_string.error_if());
      }

      const auto retained = retain_warnings(string_offset, string_length, warning_count);

      if (!retained.has_value())
      {
        return status_result::failure(*retained.error_if());
      }

      const source_span span{m_source.offset(), scan.source.offset() - m_source.offset()};

      const auto construction_units = checked_add(span.byte_length, c_record_construction_work);

      if (!construction_units.has_value())
      {
        return status_result::failure(*construction_units.error_if());
      }

      // Admit numeric copying and record construction before building the value.
      const auto construction = m_work.charge(*construction_units.value_if());

      if (!construction.has_value())
      {
        return status_result::failure(*construction.error_if());
      }

      const auto created = append_primitive_token(m_document, std::move(token), m_input, m_options, m_storage);

      if (!created.has_value())
      {
        return status_result::failure(*created.error_if());
      }

      const auto recorded = m_document.set_source_span(*created.value_if(), span, m_input.size);

      if (!recorded.has_value())
      {
        return status_result::failure(*recorded.error_if());
      }

      const auto accepted = accept_completed_value(*created.value_if());

      if (!accepted.has_value())
      {
        return status_result::failure(*accepted.error_if());
      }

      // Move the long-lived cursor; copying scan.source would keep its shortened view.
      m_source.advance(span.byte_length);

      return status_result::success();
    }

    /**
     * @brief Decode and retain the innermost object's next member name.
     * @par Requirements
     * P033; P035; P036; J5-032; J5-033; J5-049; J5-017; J5-019; J5-020; J5-048.
     * @note Limits lexical growth to the remaining decoded-unit allowance.
     * Success retains the name and selects object_colon.
     * Retains only the first warning descriptor and counts all warnings.
     * Failure preserves the source cursor and pending name.
     * Earlier budget reservations are not rolled back after later failure;
     * the failed engine must be abandoned.
     * Under reject_duplicates, checks completed members of the same object
     * before committing the name or cursor.
     * Duplicate failures retain both original name-token spans.
     * Preserve-all bypasses duplicate scanning.
     */
    status_result parser_engine::read_name() noexcept
    {
      auto* frame = m_frames.top();

      if ((frame == nullptr) || ((frame->state != parser_state::object_first_name_or_end) &&
                                 (frame->state != parser_state::object_name_after_comma)))
      {
        return status_result::failure(error::without_location(error_code::wrong_type_or_access));
      }

      if (frame->pending_name.has_value())
      {
        return status_result::failure(error::without_location(error_code::wrong_type_or_access));
      }

      // The lexer must stop growth before either string limit is exceeded.
      const std::size_t max_units = m_resources.string_unit_allowance();

      const auto started = begin_limited_scan(m_input, m_source.offset(), m_work);

      if (!started.has_value())
      {
        return status_result::failure(*started.error_if());
      }

      limited_scan scan = *started.value_if();
      auto recognized = consume_member_name(scan.source, m_options.syntax, m_storage, max_units);

      if (!recognized.has_value())
      {
        // A failure at the cut may only mean the budget hid the rest of the name.
        return status_result::failure(limited_scan_error(scan, *recognized.error_if()));
      }

      // Charge before the no-match check, so an exhausted budget reports resource_limit.
      const auto charged = finish_limited_scan(scan, scan.source.offset(), m_work);

      if (!charged.has_value())
      {
        return status_result::failure(*charged.error_if());
      }

      if (!recognized.value_if()->has_value())
      {
        return status_result::failure(error::syntax_at(m_source.offset(), syntax_error_reason::expected_member_name));
      }

      auto& name = **recognized.value_if();

      // Quoted and identifier names are both walked twice, counting then decoding.
      const auto decode_charge = m_work.charge(name.byte_length);

      if (!decode_charge.has_value())
      {
        return status_result::failure(*decode_charge.error_if());
      }

      // Reject decoded collisions before committing the name, cursor or retained metadata.
      if (m_options.duplicate_names == duplicate_name_policy::reject_duplicates)
      {
        const source_span repeated_span{name.byte_offset, name.byte_length};

        const auto unique = reject_duplicate_name(m_document, frame->container, name.value, repeated_span, m_work);

        if (!unique.has_value())
        {
          return status_result::failure(*unique.error_if());
        }
      }

      const auto reserved = m_resources.reserve_string_units(0, name.value.size());

      if (!reserved.has_value())
      {
        return status_result::failure(*reserved.error_if());
      }

      // Identifier names report no warnings, so only quoted names reach the string walk.
      const auto retained = retain_warnings(name.byte_offset, name.byte_length, name.warning_count);

      if (!retained.has_value())
      {
        return status_result::failure(*retained.error_if());
      }

      // P035: own the decoded name until its scalar or nested container value is attached.
      // Construct the optional's value directly: decoded_string cannot be assigned
      frame->pending_name.emplace(std::move(name.value));
      frame->name_offset = name.byte_offset;
      frame->name_length = name.byte_length;
      frame->state = parser_state::object_colon;

      // Move the long-lived cursor; copying scan.source would keep its shortened view.
      m_source.advance(scan.source.offset() - m_source.offset());

      return status_result::success();
    }

    /**
     * @brief Consume a dispatched colon or comma and update its frame.
     * @par Requirements
     * P033; P034; P035; J5-013; J5-027.
     * @note Validates action, state and punctuation before mutation.
     * Success consumes exactly one byte.
     * Failure preserves the cursor and frame.
     * Dispatch work is charged by run().
     */
    status_result parser_engine::consume_separator(parser_action action) noexcept
    {
      auto* frame = m_frames.top();

      if (frame == nullptr)
      {
        return status_result::failure(error::without_location(error_code::wrong_type_or_access));
      }

      parser_state next_state = frame->state;
      punctuation_kind expected = punctuation_kind::comma;

      if (action == parser_action::consume_colon)
      {
        if ((frame->state != parser_state::object_colon) || (!frame->pending_name.has_value()))
        {
          return status_result::failure(error::without_location(error_code::wrong_type_or_access));
        }

        expected = punctuation_kind::colon;
        next_state = parser_state::object_value;
      }
      else if (action == parser_action::consume_comma)
      {
        // P034/P035: a completed element or member must precede every comma.
        if (frame->pending_name.has_value())
        {
          return status_result::failure(error::without_location(error_code::wrong_type_or_access));
        }

        switch (frame->state)
        {
        case parser_state::array_comma_or_end:
          next_state = parser_state::array_value_after_comma;
          break;

        case parser_state::object_comma_or_end:
          next_state = parser_state::object_name_after_comma;
          break;

        default:
          return status_result::failure(error::without_location(error_code::wrong_type_or_access));
        }
      }
      else
      {
        return status_result::failure(error::without_location(error_code::wrong_type_or_access));
      }

      // Validate the actual byte before publishing the grammar transition.
      cursor candidate = m_source;
      const auto punctuation = consume_punctuation(candidate);

      if ((!punctuation.has_value()) || (punctuation->kind != expected))
      {
        return status_result::failure(error::syntax_at(m_source.offset(), syntax_error_reason::unexpected_character));
      }

      frame->state = next_state;
      m_source = candidate;

      return status_result::success();
    }

    /**
     * @brief Close the innermost container and accept its completed value.
     * @par Requirements
     * P033; P034; P035; J5-013; J5-027; J5-048.
     * @note Successful closure records the span and removes the frame.
     * Releases depth before accepting the completed value.
     * Failure requires abandoning this engine; earlier internal mutations
     * are not rolled back after a later attachment failure.
     * The source cursor is committed only after all steps succeed.
     */
    status_result parser_engine::finish_container() noexcept
    {
      // Keep the closing byte unconsumed until completion succeeds.
      cursor candidate = m_source;
      const auto closing = consume_punctuation(candidate);

      if (!closing.has_value() ||
          ((closing->kind != punctuation_kind::right_bracket) && (closing->kind != punctuation_kind::right_brace)))
      {
        return status_result::failure(error::syntax_at(m_source.offset(), syntax_error_reason::unexpected_character));
      }

      const auto completed = close_container(m_document, m_frames, *closing, m_options.syntax, m_input.size, m_work);

      if (!completed.has_value())
      {
        return status_result::failure(*completed.error_if());
      }

      // The closed container no longer consumes an open nesting level.
      const auto release = m_resources.leave_container();

      if (!release.has_value())
      {
        return status_result::failure(*release.error_if());
      }

      // close_container removed the child frame; completion reacquires its parent.
      const auto accepted = accept_completed_value(*completed.value_if());

      if (!accepted.has_value())
      {
        return status_result::failure(*accepted.error_if());
      }

      m_source = candidate;
      return status_result::success();
    }

    /**
     * @brief Attach a completed value or record the completed root.
     * @par Requirements
     * P033; P034; P035; J5-013; J5-027.
     * @note Reuse attach_parsed_value when a parent frame exists.
     * Otherwise record m_root without publishing the document yet.
     */
    status_result parser_engine::accept_completed_value(value_index completed) noexcept
    {
      if (!completed.is_set() || m_root.is_set())
      {
        return status_result::failure(error::without_location(error_code::wrong_type_or_access));
      }

      // Reacquire the parent after any stack growth or completed-container pop.
      auto* parent = m_frames.top();

      if (parent != nullptr)
      {
        return attach_parsed_value(m_document, *parent, completed, m_work, m_input.size);
      }

      // Root publication waits until run() finishes successfully.
      m_root = completed;

      return status_result::success();
    }

    /**
     * @brief Reserve diagnostics and list capacity, then re-walk the string to record its warnings.
     * @par Requirements
     * P039; J5-017; J5-041; J5-046; J5-047; J5-049.
     */
    status_result parser_engine::retain_warnings(std::size_t token_offset, std::size_t token_length,
                                                 std::size_t count) noexcept
    {
      if (count == 0)
      {
        return status_result::success();
      }

      // The string was already validated and its length is known, so admit the whole re-scan up front.
      const auto rescan_charged = m_work.charge(token_length);

      if (!rescan_charged.has_value())
      {
        return status_result::failure(*rescan_charged.error_if());
      }

      // Every retained warning is a diagnostic entry, not only the first one.
      for (std::size_t index{}; index < count; ++index)
      {
        const auto diagnostic = m_resources.reserve_diagnostic();

        if (!diagnostic.has_value())
        {
          return status_result::failure(*diagnostic.error_if());
        }
      }

      const auto reserved = m_warnings.reserve(count);

      if (!reserved.has_value())
      {
        return status_result::failure(*reserved.error_if());
      }

      // The lexer already validated this string, so walking it again only re-reports its warnings.
      cursor body{m_input};
      body.advance(token_offset + 1);

      const string_quote quote =
        (m_input.data[token_offset] == '\'') ? string_quote::single : string_quote::double_quote;

      warning_sink sink{&m_warnings};
      const string_warning_observer observer{&sink, &collect_warning};

      const auto notified = notify_string_warnings(body, quote, m_options.syntax, observer);

      if (!notified.has_value())
      {
        return status_result::failure(*notified.error_if());
      }

      // Exactly count slots were reserved, so a failed append means the two passes disagree.
      if (sink.failed)
      {
        return status_result::failure(error::without_location(error_code::wrong_type_or_access));
      }

      return status_result::success();
    }

    /**
     * @brief Address the nesting-kind bits for the current pre-pass.
     * @par Requirements
     * P033; J5-046; J5-048.
     */
    unsigned char* parser_engine::nesting_kinds() noexcept
    {
      return (m_extra_kinds.data() != nullptr) ? static_cast<unsigned char*>(m_extra_kinds.data()) : m_inline_kinds;
    }

    /**
     * @brief Count one value without building it: reserve its resources and validate it.
     * @par Requirements
     * P033; P034; P035; J5-013; J5-027; J5-046; J5-047; J5-048.
     * @note Mirrors read_value step by step so both report the same errors.
     */
    status_result parser_engine::count_value() noexcept
    {
      if (m_count_root_done)
      {
        return status_result::failure(error::without_location(error_code::wrong_type_or_access));
      }

      bool member = false;

      if (m_count_depth != 0)
      {
        switch (m_count_top.state)
        {
        case parser_state::array_first_or_end:
        case parser_state::array_value_after_comma:
          break;

        case parser_state::object_value:
          member = true;
          break;

        default:
          return status_result::failure(error::without_location(error_code::wrong_type_or_access));
        }
      }

      const auto current = m_source.peek();

      if (!current.has_value())
      {
        // Without an open container no root value was started, so the document itself is empty.
        return status_result::failure(error::syntax_at(m_source.offset(), (m_count_depth == 0)
                                                                            ? syntax_error_reason::empty_input
                                                                            : syntax_error_reason::unexpected_end));
      }

      const auto reserved_value = m_resources.reserve_value(member);

      if (!reserved_value.has_value())
      {
        return status_result::failure(*reserved_value.error_if());
      }

      if ((*current == '[') || (*current == '{'))
      {
        const auto entered = m_resources.enter_container();

        if (!entered.has_value())
        {
          return status_result::failure(*entered.error_if());
        }

        // enter_container() succeeded, so m_count_depth is below max_depth and the bit exists.
        const bool object = (*current == '{');
        const std::size_t byte = m_count_depth / c_bits_per_byte;
        const auto mask = static_cast<unsigned char>(1U << (m_count_depth % c_bits_per_byte));
        unsigned char* kinds = nesting_kinds();

        kinds[byte] = object ? static_cast<unsigned char>(kinds[byte] | mask)
                             : static_cast<unsigned char>(kinds[byte] & static_cast<unsigned char>(~mask));

        ++m_count_depth;
        m_count_top.state = object ? parser_state::object_first_name_or_end : parser_state::array_first_or_end;
        m_source.advance(1);

        return status_result::success();
      }

      const auto& limits = m_options.limits;
      const std::size_t string_limit = m_resources.string_unit_allowance();

      const auto started = begin_limited_scan(m_input, m_source.offset(), m_work);

      if (!started.has_value())
      {
        return status_result::failure(*started.error_if());
      }

      limited_scan scan = *started.value_if();

      const auto measured =
        measure_scalar(scan.source, m_options.syntax, string_limit, m_input.size, limits.max_exponent_digits);

      if (!measured.has_value())
      {
        // A failure at the cut may only mean the budget hid the rest of the token.
        return status_result::failure(limited_scan_error(scan, *measured.error_if()));
      }

      // Charge before the no-match check, so an exhausted budget reports resource_limit.
      const auto recognition = finish_limited_scan(scan, scan.source.offset(), m_work);

      if (!recognition.has_value())
      {
        return status_result::failure(*recognition.error_if());
      }

      if (!measured.value_if()->has_value())
      {
        return status_result::failure(error::syntax_at(m_source.offset(), syntax_error_reason::unexpected_character));
      }

      const scalar_measure& value = **measured.value_if();

      if (const auto* string = std::get_if<string_measure>(&value); string != nullptr)
      {
        const auto reserved_string = m_resources.reserve_string_units(0, string->units);

        if (!reserved_string.has_value())
        {
          return status_result::failure(*reserved_string.error_if());
        }

        // The build pass retains every warning as one diagnostic entry, so count them now.
        for (std::size_t index{}; index < string->warning_count; ++index)
        {
          const auto diagnostic = m_resources.reserve_diagnostic();

          if (!diagnostic.has_value())
          {
            return status_result::failure(*diagnostic.error_if());
          }
        }
      }
      else if (const auto* number = std::get_if<number_token>(&value); number != nullptr)
      {
        // Same check and location as owned_number::copy_spelling in the build pass.
        if (number->byte_length > limits.max_number_bytes)
        {
          return status_result::failure(error::at_offset(error_code::resource_limit, number->byte_offset));
        }
      }

      const auto completed = count_completed_value();

      if (!completed.has_value())
      {
        return status_result::failure(*completed.error_if());
      }

      // Move the long-lived cursor; copying scan.source would keep its shortened view.
      m_source.advance(scan.source.offset() - m_source.offset());

      return status_result::success();
    }

    /**
     * @brief Count one member name without decoding it.
     * @par Requirements
     * P033; P035; J5-017; J5-019; J5-020; J5-046; J5-047.
     * @note Mirrors read_name except for the duplicate check (ADR 0003, decision 7, C1).
     */
    status_result parser_engine::count_name() noexcept
    {
      if ((m_count_depth == 0) || ((m_count_top.state != parser_state::object_first_name_or_end) &&
                                   (m_count_top.state != parser_state::object_name_after_comma)))
      {
        return status_result::failure(error::without_location(error_code::wrong_type_or_access));
      }

      const std::size_t max_units = m_resources.string_unit_allowance();

      const auto started = begin_limited_scan(m_input, m_source.offset(), m_work);

      if (!started.has_value())
      {
        return status_result::failure(*started.error_if());
      }

      limited_scan scan = *started.value_if();
      const auto measured = measure_member_name(scan.source, m_options.syntax, max_units);

      if (!measured.has_value())
      {
        // A failure at the cut may only mean the budget hid the rest of the name.
        return status_result::failure(limited_scan_error(scan, *measured.error_if()));
      }

      // Charge before the no-match check, so an exhausted budget reports resource_limit.
      const auto charged = finish_limited_scan(scan, scan.source.offset(), m_work);

      if (!charged.has_value())
      {
        return status_result::failure(*charged.error_if());
      }

      if (!measured.value_if()->has_value())
      {
        return status_result::failure(error::syntax_at(m_source.offset(), syntax_error_reason::expected_member_name));
      }

      const member_name_measure& name = **measured.value_if();

      const auto reserved = m_resources.reserve_string_units(0, name.units);

      if (!reserved.has_value())
      {
        return status_result::failure(*reserved.error_if());
      }

      for (std::size_t index{}; index < name.warning_count; ++index)
      {
        const auto diagnostic = m_resources.reserve_diagnostic();

        if (!diagnostic.has_value())
        {
          return status_result::failure(*diagnostic.error_if());
        }
      }

      m_count_top.state = parser_state::object_colon;

      // Move the long-lived cursor; copying scan.source would keep its shortened view.
      m_source.advance(scan.source.offset() - m_source.offset());

      return status_result::success();
    }

    /**
     * @brief Consume a dispatched colon or comma in the pre-pass.
     * @par Requirements
     * P033; P034; P035; J5-013; J5-027.
     */
    status_result parser_engine::count_separator(parser_action action) noexcept
    {
      if (m_count_depth == 0)
      {
        return status_result::failure(error::without_location(error_code::wrong_type_or_access));
      }

      parser_state next_state = m_count_top.state;
      punctuation_kind expected = punctuation_kind::comma;

      if ((action == parser_action::consume_colon) && (m_count_top.state == parser_state::object_colon))
      {
        expected = punctuation_kind::colon;
        next_state = parser_state::object_value;
      }
      else if ((action == parser_action::consume_comma) && (m_count_top.state == parser_state::array_comma_or_end))
      {
        next_state = parser_state::array_value_after_comma;
      }
      else if ((action == parser_action::consume_comma) && (m_count_top.state == parser_state::object_comma_or_end))
      {
        next_state = parser_state::object_name_after_comma;
      }
      else
      {
        return status_result::failure(error::without_location(error_code::wrong_type_or_access));
      }

      cursor candidate = m_source;
      const auto punctuation = consume_punctuation(candidate);

      if ((!punctuation.has_value()) || (punctuation->kind != expected))
      {
        return status_result::failure(error::syntax_at(m_source.offset(), syntax_error_reason::unexpected_character));
      }

      m_count_top.state = next_state;
      m_source = candidate;

      return status_result::success();
    }

    /**
     * @brief Close the innermost container in the pre-pass.
     * @par Requirements
     * P033; P034; P035; J5-013; J5-027; J5-048.
     */
    status_result parser_engine::count_close() noexcept
    {
      cursor candidate = m_source;
      const auto closing = consume_punctuation(candidate);

      if (!closing.has_value() ||
          ((closing->kind != punctuation_kind::right_bracket) && (closing->kind != punctuation_kind::right_brace)))
      {
        return status_result::failure(error::syntax_at(m_source.offset(), syntax_error_reason::unexpected_character));
      }

      if (m_count_depth == 0)
      {
        return status_result::failure(error::without_location(error_code::wrong_type_or_access));
      }

      const auto closable = check_container_close(m_count_top.state, *closing, m_options.syntax);

      if (!closable.has_value())
      {
        return status_result::failure(*closable.error_if());
      }

      const auto release = m_resources.leave_container();

      if (!release.has_value())
      {
        return status_result::failure(*release.error_if());
      }

      --m_count_depth;

      if (m_count_depth != 0)
      {
        // The parent was expecting a value when this container opened, so only its kind is needed.
        const std::size_t parent = m_count_depth - 1;
        const bool object = ((nesting_kinds()[parent / c_bits_per_byte] >> (parent % c_bits_per_byte)) & 1U) != 0U;

        m_count_top.state = object ? parser_state::object_value : parser_state::array_first_or_end;
      }

      const auto completed = count_completed_value();

      if (!completed.has_value())
      {
        return status_result::failure(*completed.error_if());
      }

      m_source = candidate;
      return status_result::success();
    }

    /**
     * @brief Record a completed value in the pre-pass: finish the root or advance the parent.
     * @par Requirements
     * P033; P034; P035; J5-013.
     */
    status_result parser_engine::count_completed_value() noexcept
    {
      if (m_count_depth == 0)
      {
        m_count_root_done = true;
        return status_result::success();
      }

      // Same transitions as attach_parsed_value after a successful attachment.
      switch (m_count_top.state)
      {
      case parser_state::array_first_or_end:
      case parser_state::array_value_after_comma:
        m_count_top.state = parser_state::array_comma_or_end;
        return status_result::success();

      case parser_state::object_value:
        m_count_top.state = parser_state::object_comma_or_end;
        return status_result::success();

      default:
        return status_result::failure(error::without_location(error_code::wrong_type_or_access));
      }
    }

    /**
     * @brief Run the counting pre-pass over the whole document.
     * @par Requirements
     * P033; P037; J5-026; J5-046; J5-047; J5-048; J5-049.
     * @param complete True to also require complete input consumption.
     * @return Success with the counts held in m_resources, or the first error.
     */
    status_result parser_engine::count_document(bool complete) noexcept
    {
      // Deeper configurations need more bits than the inline array holds (ADR 0003, decision 7, D1).
      if (m_options.limits.max_depth > c_inline_nesting_bits)
      {
        const std::size_t bytes = (m_options.limits.max_depth / c_bits_per_byte) + 1;
        auto allocation = allocate_block(m_storage, bytes, alignof(unsigned char));

        if (!allocation.has_value())
        {
          return status_result::failure(*allocation.error_if());
        }

        m_extra_kinds = std::move(*allocation.value_if());
      }

      status_result counted = status_result::success();

      while (counted.has_value() && !m_count_root_done)
      {
        // Charge dispatch even when no lexical scanner is needed, as the build pass does.
        counted = m_work.charge(1);

        if (counted.has_value())
        {
          counted = skip_trivia_within_budget(m_source, m_input, m_options.syntax, m_work);
        }

        if (!counted.has_value())
        {
          break;
        }

        parser_action action = parser_action::read_value;

        if (m_count_depth != 0)
        {
          const auto selected =
            next_container_action(m_count_top, m_source.peek(), m_source.offset(), m_options.syntax);

          if (!selected.has_value())
          {
            counted = status_result::failure(*selected.error_if());
            break;
          }

          action = *selected.value_if();
        }

        switch (action)
        {
        case parser_action::read_value:
          counted = count_value();
          break;

        case parser_action::read_name:
          counted = count_name();
          break;

        case parser_action::consume_colon:
        case parser_action::consume_comma:
          counted = count_separator(action);
          break;

        case parser_action::close_container:
          counted = count_close();
          break;

        default:
          counted = status_result::failure(error::without_location(error_code::wrong_type_or_access));
          break;
        }
      }

      if (counted.has_value() && complete)
      {
        counted = skip_trivia_within_budget(m_source, m_input, m_options.syntax, m_work);

        if (counted.has_value() && !m_source.is_at_end())
        {
          counted = status_result::failure(error::syntax_at(m_source.offset(), syntax_error_reason::trailing_content));
        }
      }

      // The bits are only needed while counting, so the build pass never holds them.
      m_extra_kinds = owned_block{};

      return counted;
    }

    /**
     * @brief Prepare private state without allocating parser storage.
     * @par Requirements
     * P033; J5-040; J5-047; J5-048.
     * @note accounting and its upstream provider must outlive returned data.
     * Input must remain alive and unchanged throughout run().
     */
    parser_engine::parser_engine(input_view input, const parse_options& options, accounted_storage& accounting) noexcept
        : m_input(input), m_options(options), m_storage(accounting.callbacks()),
          m_owned_byte_limit(accounting.byte_limit()), m_source(input), m_document(m_storage, options.limits.max_nodes),
          m_resources(options.limits), m_work(options.limits.max_work_units),
          m_frames(m_storage, options.limits.max_depth),
          m_warnings(m_storage, options.limits.max_diagnostics, options.warning_storage)
    {
    }

    /**
     * @brief Count, reserve exactly, then build one root value.
     * @par Requirements
     * P033; P037; J5-013; J5-026; J5-040; J5-046; J5-048; J5-049.
     * @param complete True to require complete input consumption.
     * @note Syntax and limit failures occur in the pre-pass, before any record or frame
     * allocation (ADR 0003, decision 7).
     */
    result<parsed_value_tree> parser_engine::parse(bool complete) noexcept
    {
      using tree_result = result<parsed_value_tree>;

      if ((m_input.data == nullptr) && (m_input.size != 0))
      {
        return tree_result::failure(error::without_location(error_code::wrong_type_or_access));
      }

      // All parser allocations must use the configured byte ceiling.
      if (m_owned_byte_limit != m_options.limits.max_owned_bytes)
      {
        return tree_result::failure(error::without_location(error_code::wrong_type_or_access));
      }

      const auto input_checked = m_resources.check_input(m_input.size);

      if (!input_checked.has_value())
      {
        return tree_result::failure(*input_checked.error_if());
      }

      // Skip at most one leading mark, and only in strict mode by request; JSON5 treats it as whitespace.
      if ((m_options.syntax == syntax_mode::strict_json) && (m_options.strict_bom == strict_bom_policy::skip_one) &&
          starts_with_byte_order_mark(m_input))
      {
        m_source.advance(c_byte_order_mark_bytes);
      }

      // Both passes start after any skipped mark.
      const cursor start = m_source;

      const auto counted = count_document(complete);

      if (!counted.has_value())
      {
        return tree_result::failure(*counted.error_if());
      }

      const std::size_t records = m_resources.reserved_nodes();
      const std::size_t frames = m_resources.peak_depth();

      // The build pass reserves every resource again, so it starts from an unused budget.
      m_resources = resource_budget{m_options.limits};
      m_source = start;

      // One allocation each, at exactly the counted sizes; neither container grows afterwards.
      const auto records_reserved = document_access::reserve_records_exact(m_document, records);

      if (!records_reserved.has_value())
      {
        return tree_result::failure(*records_reserved.error_if());
      }

      const auto frames_reserved = m_frames.reserve_exact(frames);

      if (!frames_reserved.has_value())
      {
        return tree_result::failure(*frames_reserved.error_if());
      }

      while (!m_root.is_set())
      {
        // Charge dispatch even when no lexical scanner is needed.
        const auto step_charged = m_work.charge(1);

        if (!step_charged.has_value())
        {
          return tree_result::failure(*step_charged.error_if());
        }

        const auto trivia = skip_trivia_within_budget(m_source, m_input, m_options.syntax, m_work);

        if (!trivia.has_value())
        {
          return tree_result::failure(*trivia.error_if());
        }

        parser_action action = parser_action::read_value;

        // An empty stack means the root value has not started yet.
        if (m_frames.top() != nullptr)
        {
          const auto selected =
            next_container_action(*m_frames.top(), m_source.peek(), m_source.offset(), m_options.syntax);

          if (!selected.has_value())
          {
            return tree_result::failure(*selected.error_if());
          }

          action = *selected.value_if();
        }

        // Keep action-specific ownership and reservations in their methods.
        switch (action)
        {
        case parser_action::read_value:
        {
          const auto read = read_value();

          if (!read.has_value())
          {
            return tree_result::failure(*read.error_if());
          }
          break;
        }

        case parser_action::read_name:
        {
          const auto read = read_name();

          if (!read.has_value())
          {
            return tree_result::failure(*read.error_if());
          }
          break;
        }

        case parser_action::consume_colon:
        case parser_action::consume_comma:
        {
          const auto consumed = consume_separator(action);

          if (!consumed.has_value())
          {
            return tree_result::failure(*consumed.error_if());
          }
          break;
        }

        case parser_action::close_container:
        {
          const auto finished = finish_container();

          if (!finished.has_value())
          {
            return tree_result::failure(*finished.error_if());
          }
          break;
        }

        default:
          return tree_result::failure(error::without_location(error_code::wrong_type_or_access));
        }
      }

      // Establish the root only after the entire value was completed.
      const auto rooted = m_document.set_root(m_root);

      if (!rooted.has_value())
      {
        return tree_result::failure(*rooted.error_if());
      }

      if (complete)
      {
        const auto trivia = skip_trivia_within_budget(m_source, m_input, m_options.syntax, m_work);

        if (!trivia.has_value())
        {
          return tree_result::failure(*trivia.error_if());
        }

        // The pre-pass already required complete consumption, so this only guards a disagreement.
        if (!m_source.is_at_end())
        {
          return tree_result::failure(error::syntax_at(m_source.offset(), syntax_error_reason::trailing_content));
        }
      }

      return tree_result::success(parsed_value_tree{std::move(m_document), m_source.offset(), std::move(m_warnings)});
    }

    /**
     * @brief Construct one root value and return its consumed prefix.
     * @par Requirements
     * P033; P034; P035; J5-013; J5-027; J5-040; J5-048.
     * @note Invoke once on a fresh engine.
     * This internal operation does not validate trailing input.
     * Every failure requires abandoning this engine instance.
     */
    result<parsed_value_tree> parser_engine::run() noexcept
    {
      return parse(false);
    }

    /**
     * @brief Construct one root and require complete input consumption.
     * @par Requirements
     * P037; J5-026; J5-049; J5-051.
     * @note Invoke once on a fresh engine instead of run().
     * Admits trailing trivia according to the selected syntax mode.
     * Success sets consumed_bytes to the complete input size.
     * Failure returns no tree; discard this engine instance.
     */
    result<parsed_value_tree> parser_engine::run_complete() noexcept
    {
      return parse(true);
    }
  } // namespace detail
} // namespace adaptive_json5
