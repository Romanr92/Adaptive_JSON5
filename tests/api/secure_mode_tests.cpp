#include "../shared_infrastructure/document_provider.hpp"
#include "adaptive_json5/diagnostic_format.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/input.hpp"
#include "adaptive_json5/memory/accounted_storage.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/parser.hpp"
#include "adaptive_json5/security_reason.hpp"
#include "adaptive_json5/syntax_error_reason.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <optional>
#include <string_view>

namespace
{
  using namespace adaptive_json5;
  using adaptive_json5_test::document_provider;

  /* ==================================== Fixtures ===================================== */

  /**
   * @brief Result of one complete parse, reduced to what the secure-mode units compare.
   * @par Requirements
   * CSR-07; J5-032; issue #145.
   */
  struct parse_outcome
  {
      std::optional<error> failure{}; // Set when the parse failed.
      std::size_t warning_count{};    // Warnings of a successful parse.
  };

  /**
   * @brief Parse a complete document with caller-owned test storage and release it again.
   * @par Requirements
   * CSR-07; J5-032; issue #145.
   * @param text Complete input bytes.
   * @param options Syntax, policies and limits for the parse.
   * @return The failure, or the warning count of the successful parse.
   * @note The provider checks on destruction that every allocation was released.
   */
  parse_outcome parse_text(std::string_view text, const parse_options& options)
  {
    document_provider provider;
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    const auto parsed = parse_document({text.data(), text.size()}, options, accounting);

    parse_outcome outcome{};
    if (parsed.has_value())
    {
      outcome.warning_count = parsed.value_if()->warnings.size();
    }
    else
    {
      outcome.failure = *parsed.error_if();
    }
    return outcome;
  }

  /**
   * @brief Return secure-mode options for one syntax mode.
   * @par Requirements
   * CSR-07; issue #145.
   */
  parse_options secure_options(syntax_mode mode)
  {
    parse_options options = secure_parse_options();
    options.syntax = mode;
    return options;
  }

  /**
   * @brief Name a syntax mode for SCOPED_TRACE.
   * @par Requirements
   * J5-005.
   */
  const char* mode_name(syntax_mode mode)
  {
    return (mode == syntax_mode::json5) ? "json5" : "strict_json";
  }

  /* ================================== End Fixtures =================================== */
  /* =================================== Test_J5_032 =================================== */

  // Secure mode is off by default and the factory turns every rejection on (CSR-07).
  static_assert(noexcept(secure_parse_options()));
  static_assert(parse_options{}.duplicate_names == duplicate_name_policy::preserve_all);
  static_assert(parse_options{}.zero_bytes == zero_byte_policy::preserve);
  static_assert(parse_options{}.name_controls == name_control_policy::preserve);
  static_assert(parse_options{}.unpaired_surrogates == unpaired_surrogate_policy::warn);
  static_assert(secure_parse_options().duplicate_names == duplicate_name_policy::reject_duplicates);
  static_assert(secure_parse_options().zero_bytes == zero_byte_policy::reject);
  static_assert(secure_parse_options().name_controls == name_control_policy::reject);
  static_assert(secure_parse_options().unpaired_surrogates == unpaired_surrogate_policy::reject);
  static_assert(error_code::security_policy == static_cast<error_code>(9));

  /**
   * @brief One input that secure mode rejects with a security_policy error.
   * @par Requirements
   * CSR-07; J5-032; issue #145.
   */
  struct rejected_case
  {
      const char* name;          // Case identifier.
      std::string_view input;    // Input complete document bytes.
      bool json5_only;           // Input needs JSON5 syntax.
      security_reason reason;    // Expected reason.
      std::size_t byte_offset;   // Expected start of the offending character or escape, in bytes.
      std::size_t byte_length;   // Expected length of the offending character or escape, in bytes.
      std::size_t default_warns; // Expected warning count under default options.
  };

  class J5_032_SecureModeRejects : public testing::TestWithParam<rejected_case>
  {
  };

  /* Verify secure mode rejects each case with its reason and exact source range, while the
   * default options accept the same document.
   * Arrange: Zero bytes, member-name control characters and unpaired surrogates, raw and in
   *          every escape form, in string values, a document root and member names.
   * Act: Parse each case in every applicable syntax mode, once with default options and once
   *      with secure_parse_options().
   * Expect: The default parse succeeds (an unpaired surrogate only warns); the secure parse fails
   *         with security_policy, the expected security_reason, byte_offset() at the range start
   *         and source_range() covering exactly the offending character or escape.
   */
  TEST_P(J5_032_SecureModeRejects, RejectsWithReasonAndRange)
  {
    /* Arrange */
    const rejected_case& fixture = GetParam();
    for (const auto mode : {syntax_mode::strict_json, syntax_mode::json5})
    {
      if (fixture.json5_only && (mode == syntax_mode::strict_json))
      {
        continue;
      }
      SCOPED_TRACE(mode_name(mode));
      parse_options standard{};
      standard.syntax = mode;
      /* Act */
      const parse_outcome accepted = parse_text(fixture.input, standard);
      const parse_outcome rejected = parse_text(fixture.input, secure_options(mode));
      /* Expect */
      EXPECT_FALSE(accepted.failure.has_value());
      EXPECT_EQ(accepted.warning_count, fixture.default_warns);
      ASSERT_TRUE(rejected.failure.has_value());
      const error& failure = *rejected.failure;
      EXPECT_EQ(failure.code(), error_code::security_policy);
      ASSERT_TRUE(failure.security_reason().has_value());
      EXPECT_EQ(*failure.security_reason(), fixture.reason);
      EXPECT_FALSE(failure.syntax_reason().has_value());
      ASSERT_TRUE(failure.has_byte_offset());
      EXPECT_EQ(failure.byte_offset(), fixture.byte_offset);
      ASSERT_TRUE(failure.source_range().has_value());
      const source_span range = *failure.source_range();
      EXPECT_EQ(range.byte_offset, fixture.byte_offset);
      EXPECT_EQ(range.byte_length, fixture.byte_length);
      EXPECT_FALSE(failure.duplicate_names().has_value());
    }
  }

  INSTANTIATE_TEST_SUITE_P(
    CSR_07, J5_032_SecureModeRejects,
    testing::Values(
      rejected_case{"ZeroUnicodeEscape", R"("\u0000")", false, security_reason::zero_byte, 1, 6, 0},
      rejected_case{"ZeroNulEscape", R"("\0")", true, security_reason::zero_byte, 1, 2, 0},
      rejected_case{"ZeroHexEscape", R"("\x00")", true, security_reason::zero_byte, 1, 4, 0},
      rejected_case{"ZeroRaw", std::string_view{"\"\0\"", 3}, true, security_reason::zero_byte, 1, 1, 0},
      rejected_case{"ZeroInArrayValue", R"([1, "a\u0000b"])", false, security_reason::zero_byte, 6, 6, 0},
      rejected_case{"ZeroInName", R"({"a\u0000": 1})", false, security_reason::zero_byte, 3, 6, 0},
      rejected_case{"ControlNewlineEscapeInName", R"({"\n": 1})", false, security_reason::name_control_character, 2, 2,
                    0},
      rejected_case{"ControlUnicodeEscapeInName", R"({"\u001F": 1})", false, security_reason::name_control_character, 2,
                    6, 0},
      rejected_case{"ControlHexEscapeInName", R"({'\x01': 1})", true, security_reason::name_control_character, 2, 4, 0},
      rejected_case{"ControlRawTabInName", "{'a\tb': 1}", true, security_reason::name_control_character, 3, 1, 0},
      rejected_case{"ControlInNestedName", R"({"a": {"b\r": 1}})", false, security_reason::name_control_character, 9, 2,
                    0},
      rejected_case{"HighSurrogate", R"("\uD800")", false, security_reason::unpaired_surrogate, 1, 6, 1},
      rejected_case{"LowSurrogate", R"("x\uDC00")", false, security_reason::unpaired_surrogate, 2, 6, 1},
      rejected_case{"ReversedPair", R"("\uDC00\uD800")", false, security_reason::unpaired_surrogate, 1, 6, 2},
      rejected_case{"HighThenRaw", R"("\uD800a")", false, security_reason::unpaired_surrogate, 1, 6, 1},
      rejected_case{"SurrogateInName", R"({"\uDBFF": 1})", false, security_reason::unpaired_surrogate, 2, 6, 1},
      rejected_case{"SurrogateBeforeZero", R"("\uD800\u0000")", false, security_reason::unpaired_surrogate, 1, 6, 1}),
    [](const testing::TestParamInfo<rejected_case>& param_info)
    {
      return param_info.param.name;
    });

  /* ----------------------------------------------------------------------------------- */

  /**
   * @brief One input that secure mode still accepts.
   * @par Requirements
   * CSR-07; J5-032; issue #145.
   */
  struct accepted_case
  {
      const char* name;       // Case identifier.
      std::string_view input; // Input complete document bytes.
      bool json5_only;        // Input needs JSON5 syntax.
  };

  class J5_032_SecureModeAccepts : public testing::TestWithParam<accepted_case>
  {
  };

  /* Verify secure mode rejects only its four cases and leaves similar valid input alone.
   * Arrange: Control characters in string values, paired surrogates, a line continuation,
   *          ordinary escapes in names and the same name in different objects.
   * Act: Parse each case with secure_parse_options() in every applicable syntax mode.
   * Expect: Every parse succeeds without warnings.
   */
  TEST_P(J5_032_SecureModeAccepts, AcceptsOtherValidInput)
  {
    /* Arrange */
    const accepted_case& fixture = GetParam();
    for (const auto mode : {syntax_mode::strict_json, syntax_mode::json5})
    {
      if (fixture.json5_only && (mode == syntax_mode::strict_json))
      {
        continue;
      }
      SCOPED_TRACE(mode_name(mode));
      /* Act */
      const parse_outcome outcome = parse_text(fixture.input, secure_options(mode));
      /* Expect */
      EXPECT_FALSE(outcome.failure.has_value());
      EXPECT_EQ(outcome.warning_count, 0U);
    }
  }

  INSTANTIATE_TEST_SUITE_P(CSR_07, J5_032_SecureModeAccepts,
                           testing::Values(accepted_case{"ControlEscapeInValue", R"({"a": "line\nnext\u001F"})", false},
                                           accepted_case{"RawTabInValue", "'a\tb'", true},
                                           accepted_case{"SurrogatePairInValue", R"("\uD83D\uDE00")", false},
                                           accepted_case{"SurrogatePairInName", R"({"\uD83D\uDE00": 1})", false},
                                           accepted_case{"PairAcrossContinuation", "'\\uD800\\\n\\uDC00'", true},
                                           accepted_case{"PrintableEscapesInName", R"({"\u0041\u0020\"\\/": 1})",
                                                         false},
                                           accepted_case{"SameNameInSiblingObjects", R"([{"a": 1}, {"a": 2}])", false},
                                           accepted_case{"IdentifierName", "{abc: 1}", true}),
                           [](const testing::TestParamInfo<accepted_case>& param_info)
                           {
                             return param_info.param.name;
                           });

  /* ----------------------------------------------------------------------------------- */

  /**
   * @brief Which secure-mode option a matrix row switches back off.
   * @par Requirements
   * CSR-07; issue #145.
   */
  enum class secure_option : std::uint8_t
  {
    duplicate_names,
    zero_bytes,
    name_controls,
    unpaired_surrogates
  };

  /**
   * @brief One row of the switch-off matrix.
   * @par Requirements
   * CSR-07; issue #145.
   */
  struct switch_off_case
  {
      const char* name;       // Case identifier.
      secure_option switched; // Input option switched back to standard behavior.
  };

  class J5_032_SecureModeSwitchOff : public testing::TestWithParam<switch_off_case>
  {
  };

  /* Verify each secure-mode option can be switched off on its own while the others stay on.
   * Arrange: secure_parse_options() with one option set back to its default, and one document
   *          per option that only that option rejects.
   * Act: Parse all four documents in both syntax modes.
   * Expect: Only the document of the switched-off option is accepted; the other three still
   *         fail with their own error code and reason.
   */
  TEST_P(J5_032_SecureModeSwitchOff, OnlySwitchedOffCaseIsAccepted)
  {
    /* Arrange */
    struct document_case
    {
        secure_option option;   // Option that rejects this document.
        std::string_view input; // Input complete document bytes.
        error_code code;        // Expected error code while the option rejects.
    };
    const std::array<document_case, 4> documents{{
      {secure_option::duplicate_names, R"({"a": 1, "a": 2})", error_code::duplicate_policy},
      {secure_option::zero_bytes, R"(["\u0000"])", error_code::security_policy},
      {secure_option::name_controls, R"({"\n": 1})", error_code::security_policy},
      {secure_option::unpaired_surrogates, R"(["\uD800"])", error_code::security_policy},
    }};
    const switch_off_case& fixture = GetParam();
    for (const auto mode : {syntax_mode::strict_json, syntax_mode::json5})
    {
      SCOPED_TRACE(mode_name(mode));
      parse_options options = secure_options(mode);
      switch (fixture.switched)
      {
      case secure_option::duplicate_names:
        options.duplicate_names = duplicate_name_policy::preserve_all;
        break;
      case secure_option::zero_bytes:
        options.zero_bytes = zero_byte_policy::preserve;
        break;
      case secure_option::name_controls:
        options.name_controls = name_control_policy::preserve;
        break;
      case secure_option::unpaired_surrogates:
        options.unpaired_surrogates = unpaired_surrogate_policy::warn;
        break;
      }
      for (const document_case& document : documents)
      {
        SCOPED_TRACE(document.input);
        /* Act */
        const parse_outcome outcome = parse_text(document.input, options);
        /* Expect */
        if (document.option == fixture.switched)
        {
          EXPECT_FALSE(outcome.failure.has_value());
        }
        else
        {
          ASSERT_TRUE(outcome.failure.has_value());
          EXPECT_EQ(outcome.failure->code(), document.code);
        }
      }
    }
  }

  INSTANTIATE_TEST_SUITE_P(CSR_07, J5_032_SecureModeSwitchOff,
                           testing::Values(switch_off_case{"DuplicateNames", secure_option::duplicate_names},
                                           switch_off_case{"ZeroBytes", secure_option::zero_bytes},
                                           switch_off_case{"NameControls", secure_option::name_controls},
                                           switch_off_case{"UnpairedSurrogates", secure_option::unpaired_surrogates}),
                           [](const testing::TestParamInfo<switch_off_case>& param_info)
                           {
                             return param_info.param.name;
                           });

  /* ----------------------------------------------------------------------------------- */

  /* Verify which reason U+0000 in a member name gets, since both name rules match it.
   * Arrange: The name "\u0000" with secure mode, with zero_bytes switched off, and with both
   *          zero_bytes and name_controls switched off.
   * Act: Parse the document with each of the three option sets.
   * Expect: zero_byte while zero_bytes rejects; name_control_character once only name_controls
   *         rejects; success once neither does.
   */
  TEST(J5_032_SecureModeNameZero, ZeroByteReasonTakesPrecedence)
  {
    /* Arrange */
    constexpr std::string_view input{R"({"\u0000": 1})"};
    const parse_options secure = secure_parse_options();
    parse_options names_only = secure;
    names_only.zero_bytes = zero_byte_policy::preserve;
    parse_options neither = names_only;
    neither.name_controls = name_control_policy::preserve;
    /* Act */
    const parse_outcome as_zero = parse_text(input, secure);
    const parse_outcome as_control = parse_text(input, names_only);
    const parse_outcome accepted = parse_text(input, neither);
    /* Expect */
    ASSERT_TRUE(as_zero.failure.has_value());
    EXPECT_EQ(as_zero.failure->security_reason(), std::optional<security_reason>{security_reason::zero_byte});
    ASSERT_TRUE(as_control.failure.has_value());
    EXPECT_EQ(as_control.failure->security_reason(),
              std::optional<security_reason>{security_reason::name_control_character});
    EXPECT_FALSE(accepted.failure.has_value());
  }

  /* ----------------------------------------------------------------------------------- */

  /**
   * @brief One input mixing a secure-mode violation with another error.
   * @par Requirements
   * CSR-07; J5-043; issue #145.
   */
  struct order_case
  {
      const char* name;        // Case identifier.
      std::string_view input;  // Input complete document bytes.
      syntax_mode mode;        // Input syntax mode.
      error_code code;         // Expected reported error code.
      std::size_t byte_offset; // Expected primary byte offset.
  };

  class J5_032_SecureModeOrder : public testing::TestWithParam<order_case>
  {
  };

  /* Verify the agreed error order: within one string a syntax error wins, otherwise the first
   * error in source order.
   * Arrange: A zero byte followed by a syntax error in the same string, a violation before and
   *          after a syntax error elsewhere, and a violation before a duplicate name.
   * Act: Parse each document with secure_parse_options().
   * Expect: The expected error code at the expected byte offset.
   */
  TEST_P(J5_032_SecureModeOrder, ReportsAgreedError)
  {
    /* Arrange */
    const order_case& fixture = GetParam();
    /* Act */
    const parse_outcome outcome = parse_text(fixture.input, secure_options(fixture.mode));
    /* Expect */
    ASSERT_TRUE(outcome.failure.has_value());
    EXPECT_EQ(outcome.failure->code(), fixture.code);
    EXPECT_EQ(outcome.failure->byte_offset(), fixture.byte_offset);
  }

  INSTANTIATE_TEST_SUITE_P(
    CSR_07, J5_032_SecureModeOrder,
    testing::Values(
      order_case{"UnterminatedAfterZero", R"("\u0000)", syntax_mode::json5, error_code::syntax, 7},
      order_case{"InvalidEscapeAfterZero", R"("\u0000\x41")", syntax_mode::strict_json, error_code::syntax, 8},
      order_case{"ViolationBeforeSyntaxError", R"(["\u0000", @])", syntax_mode::json5, error_code::security_policy, 2},
      order_case{"SyntaxErrorBeforeViolation", R"([@, "\u0000"])", syntax_mode::json5, error_code::syntax, 1},
      order_case{"ViolationBeforeDuplicate", R"({"a": "\u0000", "a": 2})", syntax_mode::json5,
                 error_code::security_policy, 7}),
    [](const testing::TestParamInfo<order_case>& param_info)
    {
      return param_info.param.name;
    });

  /* ----------------------------------------------------------------------------------- */

  /* Verify the security_policy error contract of the public error type.
   * Arrange: An error from security_at_range() and a syntax error.
   * Act: Read code, location, range and every reason accessor.
   * Expect: security_at_range() sets code, offset, range and reason, and no other reason;
   *         security_reason() is empty for another category.
   */
  TEST(J5_032_SecureModeError, CarriesOnlyItsOwnReason)
  {
    /* Arrange */
    constexpr source_span range{4U, 6U};
    /* Act */
    constexpr error failure = error::security_at_range(range, security_reason::unpaired_surrogate);
    constexpr error other = error::syntax_at(4U, syntax_error_reason::invalid_escape);
    /* Expect */
    static_assert(failure.code() == error_code::security_policy);
    EXPECT_TRUE(failure.has_byte_offset());
    EXPECT_EQ(failure.byte_offset(), 4U);
    ASSERT_TRUE(failure.source_range().has_value());
    EXPECT_EQ(failure.source_range()->byte_offset, 4U);
    EXPECT_EQ(failure.source_range()->byte_length, 6U);
    EXPECT_EQ(failure.security_reason(), std::optional<security_reason>{security_reason::unpaired_surrogate});
    EXPECT_FALSE(failure.syntax_reason().has_value());
    EXPECT_FALSE(failure.numeric_reason().has_value());
    EXPECT_FALSE(failure.limit_reason().has_value());
    EXPECT_FALSE(failure.access_reason().has_value());
    EXPECT_FALSE(failure.duplicate_names().has_value());
    EXPECT_FALSE(other.security_reason().has_value());
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify a secure-mode rejection is formatted with its position and reason text.
   * Arrange: A zero byte escape on the second line, rejected by secure mode.
   * Act: Format the error with format_error().
   * Expect: "line: 2, column: 3: zero byte in string or member name".
   */
  TEST(J5_032_SecureModeError, FormatsPositionAndReason)
  {
    /* Arrange */
    constexpr std::string_view input{"[\n \"\\u0000\"]"};
    const parse_outcome outcome = parse_text(input, secure_parse_options());
    ASSERT_TRUE(outcome.failure.has_value());
    std::array<char, 128> buffer{};
    /* Act */
    const auto length = format_error(*outcome.failure, {input.data(), input.size()}, buffer.data(), buffer.size());
    /* Expect */
    ASSERT_TRUE(length.has_value());
    EXPECT_EQ((std::string_view{buffer.data(), *length.value_if()}),
              std::string_view{"line: 2, column: 3: zero byte in string or member name"});
  }
  /* ================================= End Test_J5_032 ================================= */
} // namespace
