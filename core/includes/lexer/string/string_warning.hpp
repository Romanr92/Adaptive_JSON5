#ifndef STRING_WARNING_HPP_
#define STRING_WARNING_HPP_

#include "adaptive_json5/options.hpp"
#include "adaptive_json5/value/string_warning.hpp"
#include "lexer/string/string_boundary.hpp"
#include "unicode/utf16.hpp"

#include <cstddef>
#include <optional>

namespace adaptive_json5
{

  /**
   * @brief Describe a warning for a raw JSON5 string separator, if present.
   * @par Requirements
   * P020; J5-017.
   * @param piece An original source piece returned by consume_string_piece.
   * @return Warning for a raw JSON5 U+2028/U+2029 character, otherwise nullopt.
   * @note Does not change content, cursor state or parsing success.
   * Call only for source pieces, never reconstructed decoded escape values.
   * Strict JSON receives no JSON5 separator advisory.
   * No allocation or callback is performed.
   */
  [[nodiscard]] std::optional<string_warning> raw_separator_warning_for(const string_piece& piece,
                                                                        syntax_mode mode) noexcept;

  namespace detail
  {
    /**
     * @brief Warnings produced by one step of surrogate pairing, in source order.
     * @par Requirements
     * P041; J5-017; J5-018.
     */
    struct [[nodiscard]] surrogate_warnings
    {
        static constexpr std::size_t c_capacity = 3;

        string_warning items[c_capacity]{};
        std::size_t size{};
    };

    /**
     * @brief Find escaped surrogate units that do not form a pair in the decoded string.
     * @par Requirements
     * P041; J5-017; J5-018.
     * @note Feed every piece of one string in order. A high surrogate pairs with a low
     * surrogate that is the next decoded unit, even across a line continuation, which
     * adds no units. A raw character, another unit or the closing quote leaves it
     * unpaired. Each warning covers the whole escape sequence that produced the unit.
     * Applies in both syntax modes. Performs no allocation.
     */
    class surrogate_pairing
    {
      private:
        std::optional<string_warning> m_pending_high{};

      public:
        /**
         * @brief Record the units decoded from one escape sequence.
         * @par Requirements
         * P041; J5-017; J5-018.
         * @param units Units decoded from the escape; zero for a line continuation.
         * @param byte_offset Original offset of the escape's backslash.
         * @param byte_length Original length of the whole escape sequence.
         */
        [[nodiscard]] surrogate_warnings escape(const encoded_utf16& units, std::size_t byte_offset,
                                                std::size_t byte_length) noexcept;

        /**
         * @brief Record a raw source character, which ends any pending high surrogate.
         * @par Requirements
         * P041; J5-017; J5-018.
         */
        [[nodiscard]] surrogate_warnings character() noexcept;

        /**
         * @brief Record the closing quote, which ends any pending high surrogate.
         * @par Requirements
         * P041; J5-017; J5-018.
         */
        [[nodiscard]] surrogate_warnings finish() noexcept;
    };
  } // namespace detail
} // namespace adaptive_json5

#endif /* STRING_WARNING_HPP_ */
