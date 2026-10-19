#ifndef PARSER_DISPATCH_HPP_
#define PARSER_DISPATCH_HPP_

#include "adaptive_json5/detail/parser/parser_frame.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"

#include <cstddef>
#include <optional>

namespace adaptive_json5
{
  namespace detail
  {
    /**
     * @brief Identify the next operation of the iterative container engine.
     * @par Requirements
     * P033; J5-013; J5-027.
     * @note Actions describe work without consuming input or changing frames.
     */
    enum class parser_action
    {
      read_value,
      read_name,
      consume_colon,
      consume_comma,
      close_container
    };

    /**
     * @brief Select the next action for the innermost open container.
     * @par Requirements
     * P033; J5-013; J5-027.
     * @param frame Valid innermost open-container frame.
     * @param current Current byte after caller-managed trivia, or nullopt at EOF.
     * @param byte_offset Original offset of current, or the EOF offset.
     * @param mode Selected syntax mode.
     * @return Next action or a located syntax error.
     * @note Performs no allocation, input consumption or state mutation.
     * Lexers validate value and name spellings after dispatch.
     * EOF inside an open container is always a syntax error.
     * Post-comma states are entered only after a completed element or member.
     */
    result<parser_action> next_container_action(const parser_frame& frame, std::optional<char> current,
                                                std::size_t byte_offset, syntax_mode mode) noexcept;
  } // namespace detail
} // namespace adaptive_json5

#endif /* PARSER_DISPATCH_HPP_ */
