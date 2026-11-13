#include "../shared_infrastructure/document_provider.hpp"
#include "adaptive_json5/access_error_reason.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/json5_document.hpp"
#include "adaptive_json5/json5_value.hpp"
#include "adaptive_json5/memory/accounted_storage.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/parser.hpp"
#include "adaptive_json5/resource_limit_reason.hpp"
#include "adaptive_json5/syntax_error_reason.hpp"
#include "adaptive_json5/value/string_warning.hpp"

#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <vector>

namespace
{
  using namespace adaptive_json5;
  using adaptive_json5_test::document_provider;

  /* ==================================== Fixtures ===================================== */
  /**
   * @brief Parse text into a caller buffer large enough for every case in this file.
   * @par Requirements
   * J5-043; issue #125.
   */
  class parsed_text
  {
    private:
      std::vector<unsigned char> m_buffer = std::vector<unsigned char>(std::size_t{1} << 16U);
      std::string m_text;

    public:
      result<json5_document> m_result;

      /**
       * @brief Parse text with the given options.
       * @par Requirements
       * J5-043; issue #125.
       */
      parsed_text(std::string text, const parse_options& options)
          : m_text(std::move(text)),
            m_result(json5_document::parse({m_text.data(), m_text.size()}, m_buffer.data(), m_buffer.size(), options))
      {
      }
  };
  /* ================================== End Fixtures =================================== */

  /* =================================== Test_J5_043 =================================== */
  // The compact layout keeps every result<T> small (issue #125, decision D3).
  static_assert((sizeof(void*) != 8U) || (sizeof(error) <= 48U));
  // A reason is only readable under its own code.
  static_assert(error::limit_at(3U, resource_limit_reason::depth).limit_reason() == resource_limit_reason::depth);
  static_assert(!error::limit_at(3U, resource_limit_reason::depth).access_reason().has_value());
  static_assert(!error::limit_at(3U, resource_limit_reason::depth).syntax_reason().has_value());
  static_assert(error::access_failure(access_error_reason::absent_member).access_reason() ==
                access_error_reason::absent_member);
  static_assert(!error::access_failure(access_error_reason::absent_member).limit_reason().has_value());
  static_assert(!error::internal_fault().limit_reason().has_value());
  static_assert(!error::internal_fault().access_reason().has_value());

  /*
   * Build every #125 failure kind with its factory and read it back through the accessors.
   * Arrange: One failure per factory: located and unlocated limits, an access failure and an
   *          internal fault.
   * Act: Read code, location and every reason accessor.
   * Expect: Each factory sets its code and reason and only the location it was given; the
   *         reasons of other codes stay empty.
   */
  TEST(J5_043_ErrorReasons, FactoriesSetCodeReasonAndLocation)
  {
    /* Arrange */
    const error located = error::limit_at(7U, resource_limit_reason::nodes);
    const error unlocated = error::limit_without_location(resource_limit_reason::owned_bytes);
    const error access = error::access_failure(access_error_reason::index_out_of_range);
    const error internal = error::internal_fault();
    /* Act */
    const auto located_reason = located.limit_reason();
    const auto unlocated_reason = unlocated.limit_reason();
    const auto access_reason = access.access_reason();
    /* Expect */
    EXPECT_EQ(located.code(), error_code::resource_limit);
    EXPECT_TRUE(located.has_byte_offset());
    EXPECT_EQ(located.byte_offset(), 7U);
    EXPECT_EQ(located_reason, resource_limit_reason::nodes);
    EXPECT_FALSE(located.source_range().has_value());
    EXPECT_EQ(unlocated.code(), error_code::resource_limit);
    EXPECT_FALSE(unlocated.has_byte_offset());
    EXPECT_EQ(unlocated_reason, resource_limit_reason::owned_bytes);
    EXPECT_EQ(access.code(), error_code::wrong_type_or_access);
    EXPECT_FALSE(access.has_byte_offset());
    EXPECT_EQ(access_reason, access_error_reason::index_out_of_range);
    EXPECT_FALSE(access.limit_reason().has_value());
    EXPECT_EQ(internal.code(), error_code::internal_error);
    EXPECT_FALSE(internal.has_byte_offset());
    EXPECT_FALSE(internal.syntax_reason().has_value());
    EXPECT_FALSE(internal.numeric_reason().has_value());
    EXPECT_FALSE(internal.limit_reason().has_value());
    EXPECT_FALSE(internal.access_reason().has_value());
    EXPECT_FALSE(internal.duplicate_names().has_value());
    EXPECT_EQ(static_cast<int>(error_code::internal_error), 8);
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * Move a failure to another location or drop its location without losing the reason.
   * Arrange: A syntax failure with a reason and a source range, and an unlocated limit failure.
   * Act: relocated() both, then unlocated() the syntax failure.
   * Expect: relocated() sets only the primary offset and keeps code, reason and range;
   *         unlocated() clears offset and range and keeps code and reason.
   */
  TEST(J5_043_ErrorReasons, RelocatedAndUnlocatedKeepTheReason)
  {
    /* Arrange */
    const error ranged = error::syntax_at_range(4U, source_span{2U, 5U}, syntax_error_reason::unknown_literal);
    const error limit = error::limit_without_location(resource_limit_reason::string);
    /* Act */
    const error moved = ranged.relocated(9U);
    const error located_limit = limit.relocated(3U);
    const error dropped = ranged.unlocated();
    /* Expect */
    EXPECT_EQ(moved.code(), error_code::syntax);
    EXPECT_EQ(moved.byte_offset(), 9U);
    EXPECT_EQ(moved.syntax_reason(), syntax_error_reason::unknown_literal);
    ASSERT_TRUE(moved.source_range().has_value());
    EXPECT_EQ(moved.source_range()->byte_offset, 2U);
    EXPECT_EQ(moved.source_range()->byte_length, 5U);
    EXPECT_TRUE(located_limit.has_byte_offset());
    EXPECT_EQ(located_limit.byte_offset(), 3U);
    EXPECT_EQ(located_limit.limit_reason(), resource_limit_reason::string);
    EXPECT_FALSE(dropped.has_byte_offset());
    EXPECT_EQ(dropped.byte_offset(), 0U);
    EXPECT_FALSE(dropped.source_range().has_value());
    EXPECT_EQ(dropped.syntax_reason(), syntax_error_reason::unknown_literal);
  }
  /* ----------------------------------------------------------------------------------- */
  /**
   * @brief One unknown word and its expected whole-word range.
   * @par Requirements
   * J5-024; J5-043; J5-044; issue #125.
   */
  struct word_case
  {
      const char* name;           // Case identifier.
      const char* text;           // Input document.
      syntax_mode mode;           // Input syntax.
      syntax_error_reason reason; // Expected syntax reason.
      std::size_t offset;         // Expected primary byte offset.
      std::size_t range_length;   // Expected source_range length at offset; 0 means no range.
  };

  /** @brief Name a word case for parameterized test output. */
  std::ostream& operator<<(std::ostream& stream, const word_case& value)
  {
    return stream << value.name;
  }

  class J5_043_UnknownWord : public testing::TestWithParam<word_case>
  {
  };

  /*
   * Report an unknown word that starts like Infinity or NaN as a whole word, like nul.
   * Arrange: The case document in its syntax mode.
   * Act: Parse it.
   * Expect: The case reason at the case offset, with a source_range over the whole word when the
   *         case names one, and no range otherwise.
   */
  TEST_P(J5_043_UnknownWord, ReportsWholeWordRange)
  {
    /* Arrange */
    const word_case& param = GetParam();
    parse_options options{};
    options.syntax = param.mode;
    /* Act */
    const parsed_text parsed{param.text, options};
    /* Expect */
    ASSERT_FALSE(parsed.m_result.has_value());
    const error& failure = *parsed.m_result.error_if();
    EXPECT_EQ(failure.code(), error_code::syntax);
    EXPECT_EQ(failure.syntax_reason(), param.reason);
    EXPECT_EQ(failure.byte_offset(), param.offset);
    if (param.range_length == 0U)
    {
      EXPECT_FALSE(failure.source_range().has_value());
    }
    else
    {
      ASSERT_TRUE(failure.source_range().has_value());
      EXPECT_EQ(failure.source_range()->byte_offset, param.offset);
      EXPECT_EQ(failure.source_range()->byte_length, param.range_length);
    }
  }
  constexpr syntax_mode c_json5 = syntax_mode::json5;
  constexpr syntax_mode c_strict = syntax_mode::strict_json;
  constexpr syntax_error_reason c_unknown = syntax_error_reason::unknown_literal;
  INSTANTIATE_TEST_SUITE_P(Issue125, J5_043_UnknownWord,
                           testing::Values(word_case{"NoneJson5", "[None]", c_json5, c_unknown, 1U, 4U},
                                           word_case{"IndexJson5", "[Index]", c_json5, c_unknown, 1U, 5U},
                                           word_case{"NanJson5", "[Nan]", c_json5, c_unknown, 1U, 3U},
                                           word_case{"InfinityxJson5", "[Infinityx]", c_json5, c_unknown, 1U, 9U},
                                           word_case{"NaPrefixJson5", "[Na]", c_json5, c_unknown, 1U, 2U},
                                           word_case{"RootNoneJson5", "None", c_json5, c_unknown, 0U, 4U},
                                           word_case{"NulForComparison", "[nul]", c_json5, c_unknown, 1U, 3U},
                                           word_case{"NoneStrict", "[None]", c_strict, c_unknown, 1U, 4U},
                                           word_case{"IndexStrict", "[Index]", c_strict, c_unknown, 1U, 5U},
                                           word_case{"NanStrict", "[Nan]", c_strict, c_unknown, 1U, 3U},
                                           word_case{"RootNoneStrict", "None", c_strict, c_unknown, 0U, 4U},
                                           // A sign before the word keeps the located failure at the mismatch.
                                           word_case{"SignedWord", "[-Nope]", c_json5, c_unknown, 3U, 0U},
                                           // A decimal point is not part of a word, so it is reported at the point.
                                           word_case{"NanPoint", "[NaN.5]", c_json5, c_unknown, 4U, 0U},
                                           // Only a complete Infinity or NaN is a strict-JSON violation.
                                           word_case{"StrictNan", "[NaN]", c_strict,
                                                     syntax_error_reason::not_allowed_in_strict_json, 1U, 0U}),
                           testing::PrintToStringParamName());
  /* ================================= End Test_J5_043 ================================= */

  /* =================================== Test_J5_047 =================================== */
  /**
   * @brief One configured limit exceeded by one and its expected reason and location.
   * @par Requirements
   * J5-044; J5-047; J5-048; J5-049; issue #125.
   */
  struct limit_case
  {
      const char* name;              // Case identifier.
      const char* text;              // Input document.
      resource_limits limits;        // Input limits; the case limit is one below what the text needs.
      resource_limit_reason reason;  // Expected limit reason.
      std::optional<std::size_t> at; // Expected byte offset of the offending token; nullopt means unlocated.
  };

  /** @brief Name a limit case for parameterized test output. */
  std::ostream& operator<<(std::ostream& stream, const limit_case& value)
  {
    return stream << value.name;
  }

  /**
   * @brief Default limits with one field changed.
   * @par Requirements
   * J5-047; issue #125.
   */
  resource_limits with(std::size_t resource_limits::*field, std::size_t value)
  {
    resource_limits limits{};
    limits.*field = value;
    return limits;
  }

  class J5_047_LimitReason : public testing::TestWithParam<limit_case>
  {
  };

  /*
   * Name the exceeded limit and locate the token that exceeded it.
   * Arrange: The case document and limits, each limit one below what the document needs.
   * Act: Parse it.
   * Expect: resource_limit with the case reason, located at the offending token, or without a
   *         location for limits that concern the whole input or memory.
   */
  TEST_P(J5_047_LimitReason, ReportsReasonAtOffendingToken)
  {
    /* Arrange */
    const limit_case& param = GetParam();
    parse_options options{};
    options.limits = param.limits;
    /* Act */
    const parsed_text parsed{param.text, options};
    /* Expect */
    ASSERT_FALSE(parsed.m_result.has_value());
    const error& failure = *parsed.m_result.error_if();
    EXPECT_EQ(failure.code(), error_code::resource_limit);
    EXPECT_EQ(failure.limit_reason(), param.reason);
    EXPECT_EQ(failure.has_byte_offset(), param.at.has_value());
    if (param.at.has_value())
    {
      EXPECT_EQ(failure.byte_offset(), *param.at);
    }
  }
  INSTANTIATE_TEST_SUITE_P(
    Issue125, J5_047_LimitReason,
    testing::Values(
      limit_case{"Input", "[1,2]", with(&resource_limits::max_input_bytes, 4U), resource_limit_reason::input,
                 std::nullopt},
      limit_case{"Depth", "[[[1]]]", with(&resource_limits::max_depth, 2U), resource_limit_reason::depth, 2U},
      limit_case{"Nodes", "[1, 2, 3]", with(&resource_limits::max_nodes, 3U), resource_limit_reason::nodes, 7U},
      limit_case{"Members", "{a: 1, b: 2}", with(&resource_limits::max_members, 1U), resource_limit_reason::members,
                 10U},
      limit_case{"String", "[\"abcd\"]", with(&resource_limits::max_string_units, 3U), resource_limit_reason::string,
                 5U},
      limit_case{"TotalString", "[\"abc\", \"def\"]",
                 []
                 {
                   resource_limits limits = with(&resource_limits::max_total_string_units, 5U);
                   limits.max_string_units = 10U;
                   return limits;
                 }(),
                 resource_limit_reason::total_string, 11U},
      limit_case{"NumberBytes", "[1, 12345]", with(&resource_limits::max_number_bytes, 3U),
                 resource_limit_reason::number_bytes, 4U},
      limit_case{"ExponentDigits", "[1e123]", with(&resource_limits::max_exponent_digits, 2U),
                 resource_limit_reason::exponent_digits, 5U},
      limit_case{"OwnedBytes", "[1, 2, 3]", with(&resource_limits::max_owned_bytes, 16U),
                 resource_limit_reason::owned_bytes, std::nullopt},
      limit_case{"Diagnostics", "[\"a\xE2\x80\xA8\"]", with(&resource_limits::max_diagnostics, 0U),
                 resource_limit_reason::diagnostics, 3U}),
    testing::PrintToStringParamName());
  /* ----------------------------------------------------------------------------------- */
  /*
   * Report work exhaustion as a located work limit, distinct from syntax rejection.
   * Arrange: A valid array and a work budget too small for it.
   * Act: Parse it.
   * Expect: resource_limit with the work reason and a location inside the input.
   */
  TEST(J5_047_LimitReason, WorkExhaustionIsLocatedWork)
  {
    /* Arrange */
    parse_options options{};
    options.limits.max_work_units = 6U;
    const std::string text = "[1, 2, 3, 4, 5]";
    /* Act */
    const parsed_text parsed{text, options};
    /* Expect */
    ASSERT_FALSE(parsed.m_result.has_value());
    const error& failure = *parsed.m_result.error_if();
    EXPECT_EQ(failure.code(), error_code::resource_limit);
    EXPECT_EQ(failure.limit_reason(), resource_limit_reason::work);
    ASSERT_TRUE(failure.has_byte_offset());
    EXPECT_LE(failure.byte_offset(), text.size());
  }
  /* ================================= End Test_J5_047 ================================= */

  /* =================================== Test_J5_017 =================================== */
  /**
   * @brief One document with more warnings than max_diagnostics allows.
   * @par Requirements
   * J5-017; J5-044; J5-047; issue #125.
   */
  struct diagnostics_case
  {
      const char* name;             // Case identifier.
      std::string text;             // Input document; LS is a raw U+2028, one warning each.
      std::size_t max_diagnostics;  // Input warning limit.
      std::size_t max_string_units; // Input string limit.
      resource_limit_reason reason; // Expected limit reason.
      std::size_t at;               // Expected byte offset: the first warning that does not fit.
  };

  /** @brief Name a diagnostics case for parameterized test output. */
  std::ostream& operator<<(std::ostream& stream, const diagnostics_case& value)
  {
    return stream << value.name;
  }

  class J5_017_ExcessWarning : public testing::TestWithParam<diagnostics_case>
  {
  };

  const std::string c_ls = "\xE2\x80\xA8";
  constexpr std::size_t c_many = 1000U;

  /*
   * Fail at exactly the first warning that exceeds max_diagnostics.
   * Arrange: The case document, warning limit and string limit.
   * Act: Parse it.
   * Expect: resource_limit with the case reason, located at the case offset; a string that is too
   *         long reports its string limit before any warning.
   */
  TEST_P(J5_017_ExcessWarning, LocatesFirstExcessWarning)
  {
    /* Arrange */
    const diagnostics_case& param = GetParam();
    parse_options options{};
    options.limits.max_diagnostics = param.max_diagnostics;
    options.limits.max_string_units = param.max_string_units;
    /* Act */
    const parsed_text parsed{param.text, options};
    /* Expect */
    ASSERT_FALSE(parsed.m_result.has_value());
    const error& failure = *parsed.m_result.error_if();
    EXPECT_EQ(failure.code(), error_code::resource_limit);
    EXPECT_EQ(failure.limit_reason(), param.reason);
    ASSERT_TRUE(failure.has_byte_offset());
    EXPECT_EQ(failure.byte_offset(), param.at);
  }
  const auto c_diagnostics = resource_limit_reason::diagnostics;
  INSTANTIATE_TEST_SUITE_P(
    Issue125, J5_017_ExcessWarning,
    testing::Values(
      diagnostics_case{"ZeroAllowedValue", "[\"a" + c_ls + "\"]", 0U, c_many, c_diagnostics, 3U},
      diagnostics_case{"SecondInSameString", "[\"a" + c_ls + "b" + c_ls + "\"]", 1U, c_many, c_diagnostics, 7U},
      diagnostics_case{"SecondInNextString", "[\"" + c_ls + "\", \"" + c_ls + "\"]", 1U, c_many, c_diagnostics, 9U},
      diagnostics_case{"ZeroAllowedMemberName", "{\"" + c_ls + "\": 1}", 0U, c_many, c_diagnostics, 2U},
      diagnostics_case{"SecondInNextMemberName", "{\"" + c_ls + "\": 1, \"" + c_ls + "\": 2}", 1U, c_many,
                       c_diagnostics, 12U},
      diagnostics_case{"ValueAfterMemberName", "{\"" + c_ls + "\": \"" + c_ls + "\"}", 1U, c_many, c_diagnostics, 9U},
      diagnostics_case{"PrimitiveRoot", "\"a" + c_ls + "\"", 0U, c_many, c_diagnostics, 2U},
      diagnostics_case{"TooLongWinsOverWarnings", "[\"ab" + c_ls + "\"]", 0U, 1U, resource_limit_reason::string, 3U}),
    testing::PrintToStringParamName());
  /* ----------------------------------------------------------------------------------- */
  /*
   * Accept exactly max_diagnostics warnings and retain them all.
   * Arrange: Two raw U+2028 warnings, one in a member name and one in a value, with a limit of two.
   * Act: Parse the document.
   * Expect: Success with both warnings retained in source order.
   */
  TEST(J5_017_ExcessWarning, AcceptsWarningsUpToTheLimit)
  {
    /* Arrange */
    parse_options options{};
    options.limits.max_diagnostics = 2U;
    /* Act */
    const parsed_text parsed{"{\"" + c_ls + "\": \"" + c_ls + "\"}", options};
    /* Expect */
    ASSERT_TRUE(parsed.m_result.has_value());
    const json5_document& document = *parsed.m_result.value_if();
    ASSERT_EQ(document.warning_count(), 2U);
    const auto first = document.warning(0U);
    const auto second = document.warning(1U);
    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(first.value_if()->byte_offset, 2U);
    EXPECT_EQ(second.value_if()->byte_offset, 9U);
  }
  /* ================================= End Test_J5_017 ================================= */

  /* =================================== Test_J5_039 =================================== */
  /*
   * Tell every json5_value access failure apart by its reason.
   * Arrange: An object with a duplicated member, an array and a string.
   * Act: Make every kind of invalid access.
   * Expect: wrong_type_or_access with absent_member, duplicate_member, wrong_type or
   *         index_out_of_range as the access needs; an invalid UTF-8 name stays an encoding error.
   */
  TEST(J5_039_AccessReason, DistinguishesEveryValueAccessFailure)
  {
    /* Arrange */
    const parsed_text parsed{"{a: 1, a: 2, b: [true], s: 'x'}", parse_options{}};
    ASSERT_TRUE(parsed.m_result.has_value());
    const json5_value root = parsed.m_result.value_if()->root();
    const auto reason_of = [](const error* failure) -> std::optional<access_error_reason>
    {
      if ((failure == nullptr) || (failure->code() != error_code::wrong_type_or_access))
      {
        return std::nullopt;
      }
      return failure->access_reason();
    };
    /* Act */
    const json5_value absent = root.member("zz");
    const json5_value duplicate = root.member("a");
    const json5_value member_of_array = root.member("b").member("x");
    const json5_value index_of_object = root.at(0U);
    const json5_value past_end = root.member("b").at(1U);
    const json5_value bad_name = root.member("\xC3");
    const auto scalar_size = root.member("s").size();
    const auto string_as_bool = root.member("s").as_bool();
    const auto bool_as_int = root.member("b").at(0U).as_int64();
    /* Expect */
    EXPECT_EQ(reason_of(absent.error_if()), access_error_reason::absent_member);
    EXPECT_EQ(reason_of(duplicate.error_if()), access_error_reason::duplicate_member);
    EXPECT_EQ(reason_of(member_of_array.error_if()), access_error_reason::wrong_type);
    EXPECT_EQ(reason_of(index_of_object.error_if()), access_error_reason::wrong_type);
    EXPECT_EQ(reason_of(past_end.error_if()), access_error_reason::index_out_of_range);
    ASSERT_NE(bad_name.error_if(), nullptr);
    EXPECT_EQ(bad_name.error_if()->code(), error_code::encoding);
    EXPECT_EQ(reason_of(scalar_size.error_if()), access_error_reason::wrong_type);
    EXPECT_EQ(reason_of(string_as_bool.error_if()), access_error_reason::wrong_type);
    EXPECT_EQ(reason_of(bool_as_int.error_if()), access_error_reason::wrong_type);
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * Report a broken caller precondition as invalid_argument.
   * Arrange: A parsed document without warnings, an accounting object whose ceiling differs from
   *          the options, and a buffer that overlaps the input.
   * Act: Read a warning past the end, string-export into a null buffer, call parse_document with
   *      the mismatched accounting, append to the parsed document, append a warning without a
   *      reservation, and parse input that overlaps its own buffer.
   * Expect: index_out_of_range for the warning index and invalid_argument for the others; the
   *         failed calls change nothing.
   */
  TEST(J5_039_AccessReason, ReportsBrokenPreconditionsAsInvalidArgument)
  {
    /* Arrange */
    const parsed_text parsed{"{s: 'x'}", parse_options{}};
    ASSERT_TRUE(parsed.m_result.has_value());
    const json5_document& document = *parsed.m_result.value_if();
    document_provider provider;
    parse_options options{};
    accounted_storage mismatched{provider.callbacks(), options.limits.max_owned_bytes - 1U};
    const std::string text = "[1]";
    std::vector<unsigned char> buffer(std::size_t{1} << 16U);
    /* Act */
    const auto missing_warning = document.warning(0U);
    const auto null_export = document.root().member("s").as_string(nullptr, 4U);
    auto mismatched_parse = parse_document({text.data(), text.size()}, options, mismatched);
    const auto overlapping =
      json5_document::parse({reinterpret_cast<const char*>(buffer.data()), 4U}, buffer.data(), buffer.size(), options);
    /* Expect */
    ASSERT_FALSE(missing_warning.has_value());
    EXPECT_EQ(missing_warning.error_if()->access_reason(), access_error_reason::index_out_of_range);
    ASSERT_FALSE(null_export.has_value());
    EXPECT_EQ(null_export.error_if()->access_reason(), access_error_reason::invalid_argument);
    ASSERT_FALSE(mismatched_parse.has_value());
    EXPECT_EQ(mismatched_parse.error_if()->code(), error_code::wrong_type_or_access);
    EXPECT_EQ(mismatched_parse.error_if()->access_reason(), access_error_reason::invalid_argument);
    EXPECT_EQ(mismatched.current_bytes(), 0U);
    ASSERT_FALSE(overlapping.has_value());
    EXPECT_EQ(overlapping.error_if()->access_reason(), access_error_reason::invalid_argument);
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * Refuse to grow a parsed document or its warning table.
   * Arrange: A document from parse_document, whose value storage and warning table were reserved
   *          exactly while parsing.
   * Act: Append a value to the document and a warning without a reservation.
   * Expect: Both fail with invalid_argument and leave the document unchanged.
   */
  TEST(J5_039_AccessReason, RejectsGrowingAParsedDocument)
  {
    /* Arrange */
    document_provider provider;
    const parse_options options{};
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    const std::string text = "[1]";
    auto parsed = parse_document({text.data(), text.size()}, options, accounting);
    ASSERT_TRUE(parsed.has_value());
    parsed_document& document = *parsed.value_if();
    const std::size_t bytes = accounting.current_bytes();
    /* Act */
    const auto appended = document.values.make_null();
    const auto warned =
      document.warnings.append_reserved(string_warning{string_warning_code::raw_line_separator, 0U, 3U});
    /* Expect */
    ASSERT_FALSE(appended.has_value());
    EXPECT_EQ(appended.error_if()->code(), error_code::wrong_type_or_access);
    EXPECT_EQ(appended.error_if()->access_reason(), access_error_reason::invalid_argument);
    ASSERT_FALSE(warned.has_value());
    EXPECT_EQ(warned.error_if()->access_reason(), access_error_reason::invalid_argument);
    EXPECT_EQ(document.warnings.size(), 0U);
    EXPECT_EQ(accounting.current_bytes(), bytes);
  }
  /* ================================= End Test_J5_039 ================================= */
} // namespace
