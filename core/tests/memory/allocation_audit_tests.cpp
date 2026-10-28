#include "../../../tests/shared_infrastructure/document_provider.hpp"
#include "../../../tests/shared_infrastructure/guard_provider.hpp"
#include "../../../tests/shared_infrastructure/string_provider.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/memory/accounted_storage.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/syntax_error_reason.hpp"
#include "adaptive_json5/value/string_warning.hpp"
#include "input/cursor.hpp"
#include "lexer/scalar_lexer.hpp"
#include "lexer/scalar_token.hpp"
#include "lexer/string/string_token.hpp"
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
  /* Verify an exact integer is loaded into one reservation sized from its digit count.
   * Arrange: An empty big_unsigned over a counting provider and an ample work budget.
   * Act: Reserve limbs_for_bits(decimal_digit_bits(1000)), then append 1000 nonzero decimal digits,
   *      the default max_number_bytes order of magnitude.
   * Expect: Every append succeeds with exactly one allocation, released afterwards (ADR 0003, decision 1).
   */
  TEST(J5_046_BigUnsigned, DigitsLoadIntoOneReservation)
  {
    /* Arrange */
    constexpr std::size_t c_digits = 1000;
    guard_provider provider;
    work_budget work{c_ample_work};
    std::size_t appended{};
    const auto bits = detail::decimal_digit_bits(c_digits);
    ASSERT_TRUE(bits.has_value());
    const auto limbs = detail::limbs_for_bits(*bits.value_if());
    ASSERT_TRUE(limbs.has_value());
    /* Act */
    {
      big_unsigned value{provider.callbacks()};
      ASSERT_TRUE(value.reserve(*limbs.value_if()).has_value());
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
    EXPECT_EQ(provider.m_allocations, 1U);
    EXPECT_EQ(provider.m_live_bytes, 0U);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify a decoded string is reserved exactly once and retains no spare capacity.
   * Arrange: A 33-unit string, which used to force capacity past the next power of two.
   * Act: Recognize it as a scalar and keep the token alive.
   * Expect: One allocation, and the retained string owns exactly 33 UTF-16 units.
   */
  TEST(J5_046_DecodedStringBuilder, FinishRetainsExactUnits)
  {
    /* Arrange */
    constexpr std::size_t c_units = 33;
    const std::string input = "\"" + std::string(c_units, 'a') + "\"";
    string_provider provider;
    cursor source{input_view{input.data(), input.size()}};
    /* Act */
    const auto token =
      consume_scalar(source, syntax_mode::json5, provider.callbacks(), 64, 64, std::numeric_limits<std::size_t>::max());
    /* Expect */
    ASSERT_TRUE(token.has_value());
    ASSERT_TRUE(token.value_if()->has_value());
    const auto* string = std::get_if<string_token>(&**token.value_if());
    ASSERT_NE(string, nullptr);
    EXPECT_EQ(string->value.size(), c_units);
    // Counting first allows one exact reservation (ADR 0003, decision 2).
    EXPECT_EQ(provider.m_allocations, 1U);
    // Retained strings are charged against max_owned_bytes, so spare capacity is wasted budget.
    EXPECT_EQ(provider.m_live_bytes, c_units * sizeof(char16_t));
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify value literals are recognized without temporary allocation.
   * Arrange: Literal spellings alone and before a boundary, in both syntax modes, and a
   *          provider that counts attempts.
   * Act: Recognize each spelling as a scalar.
   * Expect: A literal token with the exact kind and span, the cursor after the spelling
   *         and zero allocation attempts.
   */
  TEST(J5_046_ScalarLexer, LiteralsDoNotAllocate)
  {
    struct literal_case
    {
        const char* spelling; // Input literal spelling.
        literal_kind kind;    // Expected token kind.
    };
    constexpr literal_case c_cases[] = {{"true", literal_kind::true_literal},
                                        {"false", literal_kind::false_literal},
                                        {"null", literal_kind::null_literal}};

    for (const auto mode : {syntax_mode::json5, syntax_mode::strict_json})
    {
      for (const literal_case& item : c_cases)
      {
        for (const std::string suffix : {std::string{}, std::string{"]"}, std::string{" "}})
        {
          /* Arrange */
          const std::string input = std::string{item.spelling} + suffix;
          const std::size_t length = std::string{item.spelling}.size();
          string_provider provider;
          cursor source{input_view{input.data(), input.size()}};
          /* Act */
          const auto token =
            consume_scalar(source, mode, provider.callbacks(), 64, 64, std::numeric_limits<std::size_t>::max());
          /* Expect */
          ASSERT_TRUE(token.has_value()) << input;
          ASSERT_TRUE(token.value_if()->has_value()) << input;
          const auto* literal = std::get_if<literal_token>(&**token.value_if());
          ASSERT_NE(literal, nullptr) << input;
          EXPECT_EQ(literal->kind, item.kind) << input;
          EXPECT_EQ(literal->byte_offset, 0U) << input;
          EXPECT_EQ(literal->byte_length, length) << input;
          EXPECT_EQ(source.offset(), length) << input;
          EXPECT_EQ(provider.m_attempts, 0U) << input;
        }
      }
    }
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify JSON5 non-finite spellings are recognized without temporary allocation.
   * Arrange: Signed and unsigned Infinity and NaN and a provider that counts attempts.
   * Act: Recognize each spelling as a JSON5 scalar.
   * Expect: A number token covering the whole spelling and zero allocation attempts.
   */
  TEST(J5_046_ScalarLexer, NonFiniteSpellingsDoNotAllocate)
  {
    for (const std::string spelling : {"Infinity", "-Infinity", "+Infinity", "NaN", "-NaN", "+NaN"})
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
      EXPECT_TRUE(std::holds_alternative<number_token>(**token.value_if())) << spelling;
      EXPECT_EQ(source.offset(), spelling.size()) << spelling;
      EXPECT_EQ(provider.m_attempts, 0U) << spelling;
    }
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify unknown words are rejected without decoding or allocating them.
   * Arrange: Plain, escaped and multibyte unknown words and a provider that counts attempts.
   * Act: Recognize each as a JSON5 scalar, and once more with a provider that fails every request.
   * Expect: unknown_literal with the word's range both times, the cursor unchanged and zero
   *         allocation attempts (ADR 0003, decision 10, F).
   */
  TEST(J5_046_ScalarLexer, UnknownWordsDoNotAllocate)
  {
    struct word_case
    {
        const char* input;  // Input starting with an unknown word.
        std::size_t length; // Expected byte length of the word's range.
    };
    constexpr word_case c_cases[] = {{"foo]", 3}, {"tru\\u0065", 9}, {"caf\xC3\xA9 ", 5}};

    for (const word_case& item : c_cases)
    {
      for (const bool failing : {false, true})
      {
        /* Arrange */
        const std::string input = item.input;
        string_provider provider;
        provider.m_fail_on = failing ? 1U : 0U;
        cursor source{input_view{input.data(), input.size()}};
        /* Act */
        const auto token = consume_scalar(source, syntax_mode::json5, provider.callbacks(), 64, 64,
                                          std::numeric_limits<std::size_t>::max());
        /* Expect */
        ASSERT_FALSE(token.has_value()) << input;
        const error& failure = *token.error_if();
        EXPECT_EQ(failure.code, error_code::syntax) << input;
        EXPECT_EQ(failure.syntax_reason, syntax_error_reason::unknown_literal) << input;
        ASSERT_TRUE(failure.source_range.has_value()) << input;
        EXPECT_EQ(failure.source_range->byte_offset, 0U) << input;
        EXPECT_EQ(failure.source_range->byte_length, item.length) << input;
        EXPECT_EQ(source.offset(), 0U) << input;
        EXPECT_EQ(provider.m_attempts, 0U) << input;
      }
    }
  }

  /* ----------------------------------------------------------------------------------- */

  /**
   * @brief Bytes retained by the result of one parse.
   * @par Requirements
   * J5-046.
   * @param input Complete document bytes that must parse.
   * @param options Parse options, including the diagnostics limit and warning policy.
   * @return accounting.current_bytes() while the parsed tree is still alive.
   */
  std::size_t retained_bytes(const std::string& input, const parse_options& options)
  {
    document_provider provider;
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    parser_engine engine{{input.data(), input.size()}, options, accounting};
    const auto parsed = engine.run();
    EXPECT_TRUE(parsed.has_value());
    return accounting.current_bytes();
  }

  /* Verify the parse result's warning table follows the selected policy.
   * Arrange: A document with one raw line separator, and one without warnings.
   * Act: Parse each under different max_diagnostics values and both policies.
   * Expect: Without warnings, max_diagnostics changes nothing. With a warning, the fixed
   *         table holds max_diagnostics entries; the adaptive table holds 64 entries when
   *         max_diagnostics is larger.
   */
  TEST(J5_046_ParserWarnings, WarningTableFollowsPolicy)
  {
    /* Arrange */
    const std::string warned = "[\"a\xE2\x80\xA8\"]";
    const std::string plain = "[\"a\"]";
    constexpr std::size_t c_entry = sizeof(string_warning);
    parse_options small;
    small.limits.max_diagnostics = 16;
    parse_options large;
    large.limits.max_diagnostics = 200;
    parse_options adaptive = large;
    adaptive.warning_storage = warning_storage_policy::adaptive;
    /* Act */
    const std::size_t plain_small = retained_bytes(plain, small);
    const std::size_t plain_large = retained_bytes(plain, large);
    const std::size_t warned_small = retained_bytes(warned, small);
    const std::size_t warned_large = retained_bytes(warned, large);
    const std::size_t warned_adaptive = retained_bytes(warned, adaptive);
    /* Expect */
    // A document without warnings allocates no warning table at all.
    EXPECT_EQ(plain_small, plain_large);
    // The only difference between these parses is the size of the warning table.
    EXPECT_EQ(warned_large - warned_small, (200U - 16U) * c_entry);
    EXPECT_EQ(warned_large - warned_adaptive, (200U - 64U) * c_entry);
  }

  /* ================================= End Test_J5_046 ================================= */

  /* =================================== Test_J5_049 =================================== */
  /* Verify a string's warnings are recorded without a second pass over the string.
   * Arrange: Two arrays holding a 203-byte string body; one ends in U+4E00, the other in
   *          U+2028, which has the same UTF-8 and UTF-16 length but emits a warning.
   * Act: Find the minimal successful work ceiling for each document.
   * Expect: Both need the same work: the warning is recorded during the decode walk, so
   *         nothing is re-scanned (ADR 0003, decision 10, D; replaces WarningRescanIsCharged).
   */
  TEST(J5_049_ParserWork, WarningsNeedNoRescan)
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
    EXPECT_EQ(warned_work, plain_work);
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

  /* ----------------------------------------------------------------------------------- */

  /* Verify a string value pays for both lexer passes, counting and decoding.
   * Arrange: Arrays holding a 200-byte number and a 200-byte string; both tokens are the
   *          same length, so scanning and construction cost the same.
   * Act: Find the minimal successful work ceiling for each document.
   * Expect: The string needs at least one extra unit per token byte for its decode pass.
   */
  TEST(J5_049_ParserWork, StringDecodePassIsCharged)
  {
    /* Arrange */
    const std::string number = "[" + std::string(200, '1') + "]";
    const std::string string = "[\"" + std::string(198, 'a') + "\"]";
    ASSERT_EQ(number.size(), string.size());
    /* Act */
    const std::size_t number_work = minimum_work(number);
    const std::size_t string_work = minimum_work(string);
    /* Expect */
    EXPECT_GE(string_work, number_work + 200);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify member names pay for both lexer passes, quoted and identifier alike.
   * Arrange: Objects whose single member name is 10 or 110 bytes longer.
   * Act: Find the minimal successful work ceiling for each document.
   * Expect: 100 more name bytes cost at least 200 more units: 100 for the scan and 100
   *         for the decode pass.
   */
  TEST(J5_049_ParserWork, MemberNameDecodePassIsCharged)
  {
    for (const bool quoted : {false, true})
    {
      /* Arrange */
      const auto object = [quoted](std::size_t length)
      {
        const std::string name(length, 'a');
        return "{" + (quoted ? "\"" + name + "\"" : name) + ":1}";
      };
      /* Act */
      const std::size_t short_work = minimum_work(object(10));
      const std::size_t long_work = minimum_work(object(110));
      /* Expect */
      EXPECT_GE(long_work, short_work + 200) << (quoted ? "quoted" : "identifier");
    }
  }

  /* ----------------------------------------------------------------------------------- */

  /**
   * @brief One document whose every insufficient work budget must fail with resource_limit.
   * @par Requirements
   * J5-049.
   */
  struct cut_case
  {
      const char* name;  // Input case label.
      std::string input; // Input document; parses completely under ample work.
  };

  class J5_049_ScanCut : public testing::TestWithParam<cut_case>
  {
  };

  /* Verify a cut scan never produces a shortened token or a false syntax error.
   * Arrange: A document and its minimal successful work ceiling.
   * Act: Parse under every smaller ceiling, then under the minimal one.
   * Expect: resource_limit for every smaller ceiling; the minimal ceiling consumes the
   *         whole document, so the last scan ends exactly at the real end of input.
   */
  TEST_P(J5_049_ScanCut, BudgetBelowMinimumFailsWithResourceLimit)
  {
    /* Arrange */
    const std::string& input = GetParam().input;
    const std::size_t minimum = minimum_work(input);
    ASSERT_LT(minimum, c_ample_work);
    for (std::size_t budget{}; budget <= minimum; ++budget)
    {
      document_provider provider;
      parse_options options;
      options.limits.max_work_units = budget;
      accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
      parser_engine engine{{input.data(), input.size()}, options, accounting};
      /* Act */
      const auto parsed = engine.run();
      /* Expect */
      if (budget < minimum)
      {
        ASSERT_FALSE(parsed.has_value()) << "budget " << budget;
        EXPECT_EQ(parsed.error_if()->code, error_code::resource_limit)
          << "budget " << budget << ", error offset " << parsed.error_if()->byte_offset;
      }
      else
      {
        ASSERT_TRUE(parsed.has_value()) << "budget " << budget;
        EXPECT_EQ(parsed.value_if()->consumed_bytes, input.size());
      }
    }
  }

  INSTANTIATE_TEST_SUITE_P(
    J5_049, J5_049_ScanCut,
    testing::Values(cut_case{"NumberRoot", "12345"},                  // A cut number must not become a shorter number.
                    cut_case{"NumberElement", "[12345,6789]"},        // Cut inside a number followed by more input.
                    cut_case{"CommentOpener", "[ //note\n1]"},        // A cut after the first '/' hides the comment.
                    cut_case{"BlockComment", "[/* note */1]"},        // A cut inside a block comment hides its end.
                    cut_case{"MemberName", "{name:\"value\"}"},       // Identifier name, then a quoted value.
                    cut_case{"WarningString", "[\"a\xE2\x80\xA8\"]"}, // Multi-byte character and warning re-scan.
                    cut_case{"Literal", "[true,false,null]"}),        // Literals followed by separators.
    [](const testing::TestParamInfo<cut_case>& info)
    {
      return std::string(info.param.name);
    });

  /* ----------------------------------------------------------------------------------- */

  /**
   * @brief One lexer error located well before the end of a cut view.
   * @par Requirements
   * J5-049.
   */
  struct early_error_case
  {
      const char* name;         // Input case label.
      std::string prefix;       // Input bytes containing the error.
      std::size_t error_offset; // Expected syntax error byte offset.
  };

  class J5_049_ScanCutError : public testing::TestWithParam<early_error_case>
  {
  };

  /* Verify a lexer error before the cut keeps its own code and offset.
   * Arrange: An erroneous prefix followed by 1000 bytes, under a 100-unit ceiling, so
   *          the scan view is cut far after the error.
   * Act: Parse the document.
   * Expect: The original syntax error at its original offset, not resource_limit.
   */
  TEST_P(J5_049_ScanCutError, KeepsLexerErrorBeforeCut)
  {
    /* Arrange */
    const auto& item = GetParam();
    const std::string input = item.prefix + std::string(1000, ' ');
    document_provider provider;
    parse_options options;
    options.limits.max_work_units = 100;
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    parser_engine engine{{input.data(), input.size()}, options, accounting};
    /* Act */
    const auto parsed = engine.run();
    /* Expect */
    ASSERT_FALSE(parsed.has_value());
    EXPECT_EQ(parsed.error_if()->code, error_code::syntax);
    EXPECT_TRUE(parsed.error_if()->has_byte_offset);
    EXPECT_EQ(parsed.error_if()->byte_offset, item.error_offset);
  }

  INSTANTIATE_TEST_SUITE_P(J5_049, J5_049_ScanCutError,
                           testing::Values(early_error_case{"UnexpectedCharacter", "[@", 1}, // Not a value start.
                                           early_error_case{"NumberSuffix", "[1x", 2},       // Invalid number boundary.
                                           early_error_case{"UnknownLiteral", "[nul,", 1}, // Unknown identifier value.
                                           early_error_case{"MissingSeparator", "[1 2", 3}), // Element without a comma.
                           [](const testing::TestParamInfo<early_error_case>& info)
                           {
                             return std::string(info.param.name);
                           });
  /* ================================= End Test_J5_049 ================================= */
} // namespace
