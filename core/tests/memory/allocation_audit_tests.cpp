#include "../../../tests/shared_infrastructure/document_provider.hpp"
#include "../../../tests/shared_infrastructure/guard_provider.hpp"
#include "../../../tests/shared_infrastructure/string_provider.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/memory/accounted_storage.hpp"
#include "adaptive_json5/options.hpp"
#include "input/cursor.hpp"
#include "lexer/scalar_lexer.hpp"
#include "memory/decoded_string_builder.hpp"
#include "memory/work_budget.hpp"
#include "parser/parser_engine.hpp"
#include "value/big_unsigned.hpp"

#include <cstddef>
#include <gtest/gtest.h>
#include <limits>
#include <string>
#include <variant>

namespace
{
  using namespace adaptive_json5;
  using namespace adaptive_json5::detail;
  /* ==================================== Fixtures ===================================== */
  using adaptive_json5_test::document_provider;
  using adaptive_json5_test::guard_provider;
  using adaptive_json5_test::string_provider;

  constexpr std::size_t c_ample_work = 10000000;

  /**
   * @brief Parse a document with a fresh provider under one work ceiling.
   * @par Requirements
   * J5-049.
   * @param input Complete document bytes.
   * @param max_work_units Work ceiling for this parse.
   * @return True when the container engine parsed the document.
   */
  bool parses_within(const std::string& input, std::size_t max_work_units)
  {
    document_provider provider;
    parse_options options;
    options.limits.max_work_units = max_work_units;
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    parser_engine engine{{input.data(), input.size()}, options, accounting};

    return engine.run().has_value();
  }

  /**
   * @brief Find the smallest work ceiling under which a document parses.
   * @par Requirements
   * J5-049.
   * @param input Complete document bytes that parse under c_ample_work.
   * @return Minimal successful max_work_units.
   * @note Work charges are deterministic, so success is monotonic in the ceiling.
   */
  std::size_t minimum_work(const std::string& input)
  {
    std::size_t low = 0;
    std::size_t high = c_ample_work;

    while (low < high)
    {
      const std::size_t middle = low + ((high - low) / 2);

      if (parses_within(input, middle))
      {
        high = middle;
      }
      else
      {
        low = middle + 1;
      }
    }

    return low;
  }
  /* ================================== End Fixtures =================================== */

  /* =================================== Test_J5_046 =================================== */
  /* Verify exact-integer growth reserves spare limbs instead of one limb per carry.
   * Arrange: An empty big_unsigned over a counting provider and an ample work budget.
   * Act: Append 1000 decimal digits, the default max_number_bytes order of magnitude.
   * Expect: Allocation count grows logarithmically (at most 16) rather than once per limb.
   */
  TEST(J5_046_BigUnsigned, DigitGrowthIsGeometric)
  {
    /* Arrange */
    constexpr std::size_t c_digits = 1000;
    constexpr std::size_t c_max_allocations = 16;
    guard_provider provider;
    work_budget work{c_ample_work};
    std::size_t appended{};
    /* Act */
    {
      big_unsigned value{provider.callbacks()};
      for (; appended < c_digits; ++appended)
      {
        // A nonzero leading digit forces every later digit to extend the magnitude.
        if (!value.append_digit((appended % 9U) + 1U, 10U, work).has_value())
        {
          break;
        }
      }
    }
    /* Expect */
    EXPECT_EQ(appended, c_digits);
    EXPECT_LE(provider.m_allocations, c_max_allocations);
    EXPECT_EQ(provider.m_live_bytes, 0U);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify a finished decoded string does not retain geometric spare capacity.
   * Arrange: A builder over a slot provider that tracks live bytes.
   * Act: Append 33 units, which forces capacity past the next power of two, then finish.
   * Expect: The retained string owns exactly 33 UTF-16 units of storage.
   */
  TEST(J5_046_DecodedStringBuilder, FinishRetainsExactUnits)
  {
    /* Arrange */
    constexpr std::size_t c_units = 33;
    string_provider provider;
    decoded_string_builder builder(provider.callbacks(), 64);
    /* Act */
    for (std::size_t index{}; index < c_units; ++index)
    {
      ASSERT_TRUE(builder.append_unit(u'a').has_value());
    }
    const decoded_string text = builder.finish();
    /* Expect */
    EXPECT_EQ(text.size(), c_units);
    // Retained strings are charged against max_owned_bytes, so spare capacity is wasted budget.
    EXPECT_EQ(provider.m_live_bytes, c_units * sizeof(char16_t));
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify value literals are recognized without temporary allocation.
   * Arrange: JSON5 literal spellings and a provider that counts attempts.
   * Act: Recognize each spelling as a scalar.
   * Expect: A literal token and zero allocation attempts for every spelling.
   */
  TEST(J5_046_ScalarLexer, LiteralsDoNotAllocate)
  {
    for (const std::string spelling : {"true", "false", "null"})
    {
      /* Arrange */
      string_provider provider;
      cursor source{input_view{spelling.data(), spelling.size()}};
      /* Act */
      const auto token = consume_scalar(source, syntax_mode::json5, provider.callbacks(), 64, 64,
                                        std::numeric_limits<std::size_t>::max());
      /* Expect */
      ASSERT_TRUE(token.has_value()) << spelling;
      ASSERT_TRUE(token.value_if()->has_value()) << spelling;
      EXPECT_TRUE(std::holds_alternative<literal_token>(**token.value_if())) << spelling;
      EXPECT_EQ(provider.m_attempts, 0U) << spelling;
    }
  }
  /* ================================= End Test_J5_046 ================================= */

  /* =================================== Test_J5_049 =================================== */
  /* Verify the second pass that collects a string's warnings is charged as work.
   * Arrange: Two arrays holding a 203-byte string body; one ends in U+4E00, the other in
   *          U+2028, which has the same UTF-8 and UTF-16 length but emits a warning.
   * Act: Find the minimal successful work ceiling for each document.
   * Expect: The warning document needs at least one extra unit per rescanned body byte.
   */
  TEST(J5_049_ParserWork, WarningRescanIsCharged)
  {
    /* Arrange */
    const std::string body(200, 'a');
    const std::string plain = "[\"" + body + "\xE4\xB8\x80\"]";
    const std::string warned = "[\"" + body + "\xE2\x80\xA8\"]";
    ASSERT_EQ(plain.size(), warned.size());
    /* Act */
    const std::size_t plain_work = minimum_work(plain);
    const std::size_t warned_work = minimum_work(warned);
    /* Expect */
    EXPECT_GE(warned_work, plain_work + body.size());
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify the container engine charges scanning before lexing, like primitive_parser.
   * Arrange: Unterminated 1000-byte strings as root, array element and member name.
   * Act: Parse each with a 100-unit ceiling, far below the bytes the lexer would read.
   * Expect: resource_limit, because the budget must stop the scan before the lexer
   *         reaches the end of input and reports an unterminated string.
   */
  TEST(J5_049_ParserWork, ChargesScanBeforeLexing)
  {
    const std::string body(1000, 'a');
    for (const std::string prefix : {std::string{"\""}, std::string{"[\""}, std::string{"{\""}})
    {
      /* Arrange */
      const std::string input = prefix + body;
      document_provider provider;
      parse_options options;
      options.limits.max_work_units = 100;
      accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
      parser_engine engine{{input.data(), input.size()}, options, accounting};
      /* Act */
      const auto parsed = engine.run();
      /* Expect */
      ASSERT_FALSE(parsed.has_value()) << prefix;
      EXPECT_EQ(parsed.error_if()->code, error_code::resource_limit) << prefix;
    }
  }
  /* ================================= End Test_J5_049 ================================= */
} // namespace
