#include "value/power_of_five_table.hpp"
#include "value/wide_multiply.hpp"

#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <vector>

namespace
{
  using adaptive_json5::detail::multiply_64x64_to_128;
  using adaptive_json5::detail::product_128;
  /* ==================================== Fixtures ===================================== */
  constexpr std::uint64_t c_max_64 = 0xFFFFFFFFFFFFFFFFU;

  /**
   * @brief Minimal unsigned big integer used only to recompute expected values in units.
   * @par Requirements
   * J5-037.
   * @note Little-endian 32-bit limbs, no leading zero limbs. Supports only what the table
   * check needs: multiplication, shifts, adding or subtracting one, comparison.
   */
  class test_big
  {
    private:
      std::vector<std::uint32_t> m_limbs;

      /**
       * @brief Drop leading zero limbs so equal values compare equal.
       * @par Requirements
       * J5-037.
       */
      void trim()
      {
        while (!m_limbs.empty() && (m_limbs.back() == 0U))
        {
          m_limbs.pop_back();
        }
      }

    public:
      /**
       * @brief Build a value from up to 128 bits.
       * @par Requirements
       * J5-037.
       */
      static test_big from_128(std::uint64_t high, std::uint64_t low)
      {
        test_big value;
        value.m_limbs = {static_cast<std::uint32_t>(low), static_cast<std::uint32_t>(low >> 32U),
                         static_cast<std::uint32_t>(high), static_cast<std::uint32_t>(high >> 32U)};
        value.trim();
        return value;
      }

      /**
       * @brief Build 2^exponent.
       * @par Requirements
       * J5-037.
       */
      static test_big power_of_two(std::size_t exponent)
      {
        return from_128(0U, 1U).shifted_left(exponent);
      }

      /**
       * @brief Build 5^exponent.
       * @par Requirements
       * J5-037.
       */
      static test_big power_of_five(std::size_t exponent)
      {
        test_big value = from_128(0U, 1U);
        for (std::size_t step{}; step < exponent; ++step)
        {
          value = value * from_128(0U, 5U);
        }
        return value;
      }

      /**
       * @brief Multiply two values exactly with schoolbook multiplication.
       * @par Requirements
       * J5-037.
       */
      friend test_big operator*(const test_big& left, const test_big& right)
      {
        test_big product;
        product.m_limbs.assign(left.m_limbs.size() + right.m_limbs.size(), 0U);
        for (std::size_t i{}; i < left.m_limbs.size(); ++i)
        {
          std::uint64_t carry{};
          for (std::size_t j{}; j < right.m_limbs.size(); ++j)
          {
            const std::uint64_t sum =
              (static_cast<std::uint64_t>(left.m_limbs[i]) * right.m_limbs[j]) + product.m_limbs[i + j] + carry;
            product.m_limbs[i + j] = static_cast<std::uint32_t>(sum);
            carry = sum >> 32U;
          }
          product.m_limbs[i + right.m_limbs.size()] = static_cast<std::uint32_t>(carry);
        }
        product.trim();
        return product;
      }

      /**
       * @brief Return this value times 2^bits.
       * @par Requirements
       * J5-037.
       */
      test_big shifted_left(std::size_t bits) const
      {
        test_big shifted;
        shifted.m_limbs.assign(bits / 32U, 0U);
        const std::size_t partial = bits % 32U;
        std::uint32_t carry{};
        for (const std::uint32_t limb : m_limbs)
        {
          shifted.m_limbs.push_back((partial == 0U) ? limb : ((limb << partial) | carry));
          carry = (partial == 0U) ? 0U : (limb >> (32U - partial));
        }
        shifted.m_limbs.push_back(carry);
        shifted.trim();
        return shifted;
      }

      /**
       * @brief Return floor(this / 2^bits).
       * @par Requirements
       * J5-037.
       */
      test_big shifted_right(std::size_t bits) const
      {
        test_big shifted;
        const std::size_t whole = bits / 32U;
        const std::size_t partial = bits % 32U;
        for (std::size_t index = whole; index < m_limbs.size(); ++index)
        {
          const std::uint64_t next = ((index + 1U) < m_limbs.size()) ? m_limbs[index + 1U] : 0U;
          const std::uint64_t pair = (next << 32U) | m_limbs[index];
          shifted.m_limbs.push_back(static_cast<std::uint32_t>(pair >> partial));
        }
        shifted.trim();
        return shifted;
      }

      /**
       * @brief Return this value plus one.
       * @par Requirements
       * J5-037.
       */
      test_big plus_one() const
      {
        test_big sum = *this;
        for (std::uint32_t& limb : sum.m_limbs)
        {
          if (++limb != 0U)
          {
            return sum;
          }
        }
        sum.m_limbs.push_back(1U);
        return sum;
      }

      /**
       * @brief Return this value minus one; this value must be nonzero.
       * @par Requirements
       * J5-037.
       */
      test_big minus_one() const
      {
        test_big difference = *this;
        for (std::uint32_t& limb : difference.m_limbs)
        {
          if (limb-- != 0U)
          {
            break;
          }
        }
        difference.trim();
        return difference;
      }

      /**
       * @brief Return the number of significant bits; zero for zero.
       * @par Requirements
       * J5-037.
       */
      std::size_t bit_length() const
      {
        if (m_limbs.empty())
        {
          return 0U;
        }
        std::size_t bits = (m_limbs.size() - 1U) * 32U;
        for (std::uint32_t top = m_limbs.back(); top != 0U; top >>= 1U)
        {
          ++bits;
        }
        return bits;
      }

      /**
       * @brief Return bits 127..0 of this value as two 64-bit halves.
       * @par Requirements
       * J5-037.
       */
      product_128 low_128() const
      {
        std::uint32_t limbs[4]{};
        for (std::size_t index{}; (index < 4U) && (index < m_limbs.size()); ++index)
        {
          limbs[index] = m_limbs[index];
        }
        return product_128{(static_cast<std::uint64_t>(limbs[3]) << 32U) | limbs[2],
                           (static_cast<std::uint64_t>(limbs[1]) << 32U) | limbs[0]};
      }

      friend bool operator<(const test_big& left, const test_big& right)
      {
        if (left.m_limbs.size() != right.m_limbs.size())
        {
          return left.m_limbs.size() < right.m_limbs.size();
        }
        for (std::size_t index = left.m_limbs.size(); index > 0U; --index)
        {
          if (left.m_limbs[index - 1U] != right.m_limbs[index - 1U])
          {
            return left.m_limbs[index - 1U] < right.m_limbs[index - 1U];
          }
        }
        return false;
      }

      friend bool operator<=(const test_big& left, const test_big& right)
      {
        return !(right < left);
      }
  };

  /**
   * @brief Small deterministic generator so failures reproduce on every host.
   * @par Requirements
   * J5-037.
   * @note SplitMix64; any fixed sequence would do.
   */
  std::uint64_t next_random(std::uint64_t& state)
  {
    state += 0x9E3779B97F4A7C15U;
    std::uint64_t mixed = state;
    mixed = (mixed ^ (mixed >> 30U)) * 0xBF58476D1CE4E5B9U;
    mixed = (mixed ^ (mixed >> 27U)) * 0x94D049BB133111EBU;
    return mixed ^ (mixed >> 31U);
  }
  /* ================================== End Fixtures =================================== */

  /* =================================== Test_J5_037 =================================== */
  // The multiply is constexpr, so its corner cases are also checked at compile time.
  static_assert(multiply_64x64_to_128(0U, c_max_64).high == 0U);
  static_assert(multiply_64x64_to_128(0U, c_max_64).low == 0U);
  static_assert(multiply_64x64_to_128(c_max_64, c_max_64).high == 0xFFFFFFFFFFFFFFFEU);
  static_assert(multiply_64x64_to_128(c_max_64, c_max_64).low == 1U);
  static_assert(noexcept(multiply_64x64_to_128(1U, 1U)));

  /* Verify the 64x64->128 multiply on fixed cases that exercise every carry.
   * Arrange: Pairs with known products: zero, one, 2^32 squared, carries out of the middle
   *          column and the largest operands.
   * Act: Multiply each pair.
   * Expect: The exact high and low halves.
   */
  TEST(J5_037_WideMultiply, ComputesKnownProducts)
  {
    /* Arrange */
    struct multiply_case
    {
        std::uint64_t left;  // Input left operand.
        std::uint64_t right; // Input right operand.
        std::uint64_t high;  // Expected bits 127..64.
        std::uint64_t low;   // Expected bits 63..0.
    };
    const multiply_case cases[] = {
      {0U, 0U, 0U, 0U},
      {1U, c_max_64, 0U, c_max_64},
      {0x100000000U, 0x100000000U, 1U, 0U},
      {0xFFFFFFFFU, 0xFFFFFFFFU, 0U, 0xFFFFFFFE00000001U},
      {0xFFFFFFFF00000000U, 0xFFFFFFFFU, 0xFFFFFFFEU, 0x0000000100000000U},
      {c_max_64, 2U, 1U, 0xFFFFFFFFFFFFFFFEU},
      {c_max_64, c_max_64, 0xFFFFFFFFFFFFFFFEU, 1U},
      {0x8000000000000000U, 0x8000000000000000U, 0x4000000000000000U, 0U},
    };
    for (const multiply_case& item : cases)
    {
      /* Act */
      const product_128 product = multiply_64x64_to_128(item.left, item.right);
      const product_128 swapped = multiply_64x64_to_128(item.right, item.left);
      /* Expect */
      EXPECT_EQ(product.high, item.high) << item.left << " * " << item.right;
      EXPECT_EQ(product.low, item.low) << item.left << " * " << item.right;
      EXPECT_EQ(swapped.high, item.high);
      EXPECT_EQ(swapped.low, item.low);
    }
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify the multiply against independent long multiplication on many operands.
   * Arrange: 100000 deterministic pseudo-random operand pairs.
   * Act: Multiply each pair with the production function and with test_big.
   * Expect: Identical 128-bit products.
   */
  TEST(J5_037_WideMultiply, MatchesLongMultiplication)
  {
    /* Arrange */
    std::uint64_t state = 124U;
    for (std::size_t round{}; round < 100000U; ++round)
    {
      const std::uint64_t left = next_random(state);
      const std::uint64_t right = next_random(state);
      /* Act */
      const product_128 actual = multiply_64x64_to_128(left, right);
      const product_128 expected = (test_big::from_128(0U, left) * test_big::from_128(0U, right)).low_128();
      /* Expect */
      ASSERT_EQ(actual.high, expected.high) << left << " * " << right;
      ASSERT_EQ(actual.low, expected.low) << left << " * " << right;
    }
  }

#if ADAPTIVE_JSON5_EISEL_LEMIRE
  using adaptive_json5::detail::c_largest_power_of_five;
  using adaptive_json5::detail::c_power_of_five_count;
  using adaptive_json5::detail::c_power_of_five_table;
  using adaptive_json5::detail::c_smallest_power_of_five;

  static_assert(c_smallest_power_of_five == -342);
  static_assert(c_largest_power_of_five == 308);
  static_assert(c_power_of_five_count == 651U);

  /* ----------------------------------------------------------------------------------- */

  /* Verify every table entry matches its definition (Lemire 2021), recomputed exactly.
   * Arrange: For each q = -342..308, the table entry T as a 128-bit value.
   * Act: Recompute or check T with exact big-integer arithmetic.
   * Expect: Bit 127 set; q >= 0: T is 5^q truncated to its top 128 bits; -27 <= q < 0:
   *         T = floor(2^(z+127) / 5^-q) + 1; q < -27: T = floor(C / 2^s) with
   *         C = floor(2^(2z+128) / 5^-q) + 1, where z is the smallest integer with 2^z >= 5^-q.
   */
  TEST(J5_037_PowerOfFiveTable, MatchesDefinitionForEveryEntry)
  {
    for (std::int32_t q = c_smallest_power_of_five; q <= c_largest_power_of_five; ++q)
    {
      /* Arrange */
      const auto& entry = c_power_of_five_table[static_cast<std::size_t>(q - c_smallest_power_of_five)];
      const test_big actual = test_big::from_128(entry.high, entry.low);
      ASSERT_NE(entry.high >> 63U, 0U) << "5^" << q << " is not normalized";

      if (q >= 0)
      {
        /* Act */
        const test_big power = test_big::power_of_five(static_cast<std::size_t>(q));
        const std::size_t bits = power.bit_length();
        const product_128 expected =
          ((bits > 128U) ? power.shifted_right(bits - 128U) : power.shifted_left(128U - bits)).low_128();
        /* Expect */
        EXPECT_EQ(entry.high, expected.high) << "5^" << q;
        EXPECT_EQ(entry.low, expected.low) << "5^" << q;
        continue;
      }

      /* Act */
      const test_big divisor = test_big::power_of_five(static_cast<std::size_t>(-q));
      // 5^k is never a power of two for k > 0, so z = bit_length(5^k).
      const std::size_t z = divisor.bit_length();

      if (q >= -27)
      {
        // T - 1 = floor(X / d)  <=>  (T - 1) * d <= X < T * d.
        const test_big numerator = test_big::power_of_two(z + 127U);
        /* Expect */
        EXPECT_TRUE(actual.minus_one() * divisor <= numerator) << "5^" << q;
        EXPECT_TRUE(numerator < actual * divisor) << "5^" << q;
        continue;
      }

      // C has 128 + s bits; T = floor(C / 2^s) <=> (T * 2^s - 1) * d <= X < ((T + 1) * 2^s - 1) * d,
      // where C = floor(X / d) + 1. Normalized T fixes s, so one s must satisfy both bounds.
      // 2^(z-1) < d < 2^z gives 2^(z+128) < X / d < 2^(z+129), so C has z + 129 bits (z + 130
      // only if the + 1 carries): s is z + 1 or z + 2.
      const test_big numerator = test_big::power_of_two((2U * z) + 128U);
      bool matched = false;
      for (std::size_t s = z + 1U; (s <= z + 2U) && !matched; ++s)
      {
        const test_big lower = actual.shifted_left(s).minus_one() * divisor;
        const test_big upper = actual.plus_one().shifted_left(s).minus_one() * divisor;
        matched = (lower <= numerator) && (numerator < upper);
      }
      /* Expect */
      EXPECT_TRUE(matched) << "5^" << q;
    }
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify anchor entries whose values are known by hand.
   * Arrange: The entries for 5^0, 5^1, 5^-1 and 5^308.
   * Act: Read them from the table.
   * Expect: 2^127, 5 * 2^125, 0xCCCC...CD (1/5 rounded up) and a normalized 5^308.
   */
  TEST(J5_037_PowerOfFiveTable, HoldsHandCheckedAnchors)
  {
    /* Arrange */
    const auto entry = [](std::int32_t q)
    {
      return c_power_of_five_table[static_cast<std::size_t>(q - c_smallest_power_of_five)];
    };
    /* Act */
    const auto one = entry(0);
    const auto five = entry(1);
    const auto fifth = entry(-1);
    const auto largest = entry(308);
    /* Expect */
    EXPECT_EQ(one.high, 0x8000000000000000U);
    EXPECT_EQ(one.low, 0U);
    EXPECT_EQ(five.high, 0xA000000000000000U);
    EXPECT_EQ(five.low, 0U);
    EXPECT_EQ(fifth.high, 0xCCCCCCCCCCCCCCCCU);
    EXPECT_EQ(fifth.low, 0xCCCCCCCCCCCCCCCDU);
    EXPECT_NE(largest.high >> 63U, 0U);
  }
#endif
  /* ================================= End Test_J5_037 ================================= */
} // namespace
