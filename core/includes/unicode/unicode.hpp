#ifndef UNICODE_HPP_
#define UNICODE_HPP_

#include "unicode/unicode_range.hpp"
#include "unicode/unicode_tables.hpp"

#include <cstddef>

namespace adaptive_json5
{
  namespace detail
  {
    /**
     * @brief Check whether a value is a Unicode scalar.
     * @par Requirements
     * P006; J5-009.
     * @note Surrogates are UTF-16 code units, not Unicode scalars.
     */
    [[nodiscard]] inline constexpr bool is_unicode_scalar(char32_t value) noexcept
    {
      constexpr char32_t c_scalar_max = 0x10FFFF;
      constexpr char32_t c_surrogate_min = 0xD800;
      constexpr char32_t c_surrogate_max = 0xDFFF;

      // Reject values beyond Unicode's maximum and the surrogate interval.
      return ((value <= c_scalar_max) && ((value < c_surrogate_min) || (value > c_surrogate_max)));
    }

    /**
     * @brief Find a scalar in a sorted array of inclusive ranges.
     * @par Requirements
     * P006; J5-006; J5-052.
     * @param value Scalar whose membership is being checked.
     * @param ranges Immutable, sorted, nonoverlapping property ranges.
     * @return True when one range contains the scalar.
     * @note Binary search requires no allocation or locale facilities.
     */
    template <std::size_t RangeCount>
    [[nodiscard]] inline constexpr bool contains_unicode(char32_t value,
                                                         const unicode_range (&ranges)[RangeCount]) noexcept
    {
      if (!is_unicode_scalar(value))
      {
        return false;
      }

      // search the half-open interval [begin, end).
      //  end is one past the last candidate, so it is never indexed directly.
      std::size_t begin = 0;
      std::size_t end = RangeCount;

      while (begin < end)
      {
        // This midpoint calculation avoids overflow from begin + end.
        const std::size_t middle = begin + (end - begin) / 2;
        const unicode_range& range = ranges[middle];

        if (value < range.first)
        {
          // Sorted ranges before middle also end too early.
          // Contine searching only the later half.
          end = middle;
        }
        else if (value > range.last)
        {
          // Sorted ranges before middle also start too early.
          // Continue searching only the later half.
          begin = middle + 1;
        }
        else
        {
          // The scalar lies between this range's inclusive endpoints.
          return true;
        }
      }
      // No candidate range contains the scalar.
      return false;
    }
  } // namespace detail

  /**
   * @brief Check whether a scalar belongs to Unicode category Zs.
   * @par Requirements
   * P006; J5-006; J5-052.
   * @note This property query does not implement complete JSON5 whitespace.
   */
  [[nodiscard]] inline constexpr bool is_unicode_space_separator(char32_t value) noexcept
  {
    return detail::contains_unicode(value, detail::c_space_separator_ranges);
  }

  /**
   * @brief Check whether a scalar can begin an unquoted member name.
   * @par Requirements
   * P006; J5-006; J5-019; J5-052.
   * @note Uses versioned Unicode letter categories plus explicit ES5 additions.
   */
  [[nodiscard]] inline constexpr bool is_identifier_start(char32_t value) noexcept
  {
    // ES5 explicitly admits dollar sign and underscore independently
    // of their Unicode categories
    if ((value == U'$') || (value == U'_'))
    {
      return true;
    }

    // The table contains Lu, Ll, Lt, Lm, Lo and Nl memberships,
    // including the required historical Unicode 3.0 memberships.
    return detail::contains_unicode(value, detail::c_identifier_start_ranges);
  }

  /**
   * @brief Check whether a scalar can continue an unquoted member name.
   * @par Requirements
   * P006; J5-006; J5-019; J5-052.
   * @note Identifier continuation includes every valid starting character.
   */
  [[nodiscard]] inline constexpr bool is_identifier_part(char32_t value) noexcept
  {
    constexpr char32_t c_zero_width_non_joiner = 0x200C;
    constexpr char32_t c_zero_width_joiner = 0x200D;

    // Any character valid at the start is also valid afterword.
    if (is_identifier_start(value))
    {
      return true;
    }

    // ES5 explicitely permits these format characters after the first character.
    if ((value == c_zero_width_non_joiner) || (value == c_zero_width_joiner))
    {
      return true;
    }

    // The additional table contains combining marks, decimal digits
    // and connector punctuation: Mn, Mc, Nd and Pc.
    return detail::contains_unicode(value, detail::c_identifier_extra_ranges);
  }
} // namespace adaptive_json5

#endif /* UNICODE_HPP_ */
