#ifndef PARSER_FRAME_HPP_
#define PARSER_FRAME_HPP_

#include "adaptive_json5/value/decoded_string.hpp"
#include "adaptive_json5/value/value_index.hpp"

#include <cstddef>
#include <optional>

namespace adaptive_json5
{
  namespace detail
  {
    /**
     * @brief Identify the next grammar operation for one open container.
     * @par Requirements
     * P033; J5-013; J5-027.
     * @note Separate initial and post-comma states allow the parser
     * to distinguish empty containers from trailing commas.
     */
    enum class parser_state
    {
      array_first_or_end,
      array_value_after_comma,
      array_comma_or_end,
      object_first_name_or_end,
      object_name_after_comma,
      object_colon,
      object_value,
      object_comma_or_end
    };

    /**
     * @brief Retain the parsing state of one open array or object.
     * @par Requirements
     * P033; J5-013; J5-048.
     * @note The parser must initialize container and state before use.
     * Container indices survive relocation of document records.
     * pending_name owns a decoded object name until member attachment.
     * This type neither allocates frame storage nor enforces budgets.
     */
    struct parser_frame
    {
        value_index container{};
        parser_state state = parser_state::array_first_or_end;
        std::size_t opening_offset{};
        std::optional<decoded_string> pending_name{};
        std::size_t name_offset{};
        std::size_t name_length{};
    };
  } // namespace detail
} // namespace adaptive_json5

#endif /* PARSER_FRAME_HPP_ */
