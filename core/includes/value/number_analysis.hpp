#ifndef ADAPTIVE_JSON5_DETAIL_NUMBER_ANALYSIS_HPP_
#define ADAPTIVE_JSON5_DETAIL_NUMBER_ANALYSIS_HPP_

#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/number_kind.hpp"
#include "adaptive_json5/value/owned_number.hpp"
#include "memory/work_budget.hpp"

#include <cstddef>

namespace adaptive_json5
{
  namespace detail
  {
    /**
     * @brief Describe a number's classification and exact value properties.
     * @par Requirements
     * P038; J5-036; J5-038.
     * @note Mathematical integrality is independent of spelling:
     * 1, 1.0, 1e0 and 0x1 are all integral.
     * Infinity and NaN have is_zero and is_integral set to false.
     * Negative zero retains negative and is_zero set to true.
     * Contains no borrowed pointers and owns no allocated storage.
     * digit_count and the scale bound exact-integer reservations (ADR 0003, decision 1); hexadecimal and non-finite
     * numbers have zero scale.
     */
    struct [[nodiscard]] number_analysis
    {
        number_kind kind = number_kind::decimal;
        bool negative{};
        bool is_zero{};
        bool is_integral{};
        std::size_t digit_count{};     // Significand digits, leading zeros and hexadecimal digits included.
        bool scale_negative{};         // Sign of the combined power-of-ten exponent.
        std::size_t scale_magnitude{}; // Absolute combined scale; the written exponent magnitude saturates at input
                                       // size + 2048 before combining it with the fractional digit count.
    };

    /**
     * @brief Inspect a validated number without converting through floating point.
     * @par Requirements
     * P038; J5-003; J5-036; J5-038; J5-048.
     * @param number Borrowed number whose spelling remains unchanged.
     * @param work Work budget charged before scanning the spelling.
     * @return Exact number properties or an explicit error.
     * @note Empty spelling returns unlocated wrong_type_or_access.
     * Charge one work unit per spelling byte before scanning.
     * Failed charging returns resource_limit without scanning.
     * Successful charges are not refunded.
     * Performs one forward scan without allocation or locale-dependent operations.
     */
    [[nodiscard]] result<number_analysis> analyse_number(const owned_number& number, work_budget& work) noexcept;
  } // namespace detail

} // namespace adaptive_json5

#endif /* ADAPTIVE_JSON5_DETAIL_NUMBER_ANALYSIS_HPP_ */
