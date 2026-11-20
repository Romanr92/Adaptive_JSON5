#include "../shared_infrastructure/document_provider.hpp"
#include "adaptive_json5/access_error_reason.hpp"
#include "adaptive_json5/parser.hpp"
#include "adaptive_json5/value/document.hpp"
#include "adaptive_json5/value/utf8_export.hpp"
#include "adaptive_json5/value/value_iterator.hpp"

#include <cstdint>
#include <gtest/gtest.h>
#include <string_view>
#include <type_traits>
#include <utility>

namespace
{
  using namespace adaptive_json5;
  using adaptive_json5_test::document_provider;

  /* =================================== Test_J5_039 ==================================== */
  /*
   * Arrange: A default view with no owning document.
   * Act: Request each typed value and inspect its explicit failure information.
   * Expect: Accessors consistently report access errors without allocating.
   */
  TEST(J5_039_ValueAccess, InvalidViewExposesTypedErrors)
  {
    /* Arrange */
    const value_view view;
    /* Act */
    const auto index = view.index();
    const auto boolean = view.as_boolean();
    const auto string = view.as_string();
    const auto number = view.as_number();
    const auto member = view.member_name();
    /* Expect */
    // Callers need readable error results from every typed accessor, including pointer-valued results.
    ASSERT_NE(index.error_if(), nullptr);
    EXPECT_EQ(index.error_if()->code(), error_code::wrong_type_or_access);
    EXPECT_EQ(index.error_if()->access_reason(), access_error_reason::invalid_argument);
    ASSERT_NE(boolean.error_if(), nullptr);
    EXPECT_EQ(boolean.error_if()->code(), error_code::wrong_type_or_access);
    ASSERT_NE(string.error_if(), nullptr);
    EXPECT_EQ(string.error_if()->code(), error_code::wrong_type_or_access);
    ASSERT_NE(number.error_if(), nullptr);
    EXPECT_EQ(number.error_if()->code(), error_code::wrong_type_or_access);
    ASSERT_NE(member.error_if(), nullptr);
    EXPECT_EQ(member.error_if()->code(), error_code::wrong_type_or_access);
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * Arrange: Two parsed arrays with matching positions and one shared accounting provider.
   * Act: Obtain the index for 20 from A and use it with B containing [90,80].
   * Expect: Same-document access reads 20; foreign access fails explicitly instead of reading 80.
   */
  TEST(J5_039_IndexIdentity, RejectsForeignIndexAtMatchingPosition)
  {
    /* Arrange */
    static_assert(std::is_default_constructible_v<value_index>);
    static_assert(std::is_copy_constructible_v<value_index>);
    static_assert(!std::is_constructible_v<value_index, std::size_t>);
    static_assert(!std::is_constructible_v<value_index, const void*, std::size_t>);
    document_provider provider;
    const parse_options options{};
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    auto first = parse_document({"[10,20]", 7}, options, accounting);
    auto second = parse_document({"[90,80]", 7}, options, accounting);
    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    const auto root = first.value_if()->values.root();
    ASSERT_TRUE(root.has_value());
    const auto element = root.value_if()->element_at(1);
    ASSERT_TRUE(element.has_value());
    const auto index = element.value_if()->index();
    ASSERT_TRUE(index.has_value());
    const auto attempts = provider.m_attempts;
    /* Act */
    const auto same = first.value_if()->values.view(*index.value_if());
    const auto foreign = second.value_if()->values.view(*index.value_if());
    const auto unset = second.value_if()->values.view(value_index{});
    /* Expect */
    ASSERT_TRUE(same.has_value());
    const auto number = same.value_if()->as_number();
    ASSERT_TRUE(number.has_value());
    const auto spelling = (*number.value_if())->spelling();
    EXPECT_EQ(std::string_view(spelling.data, spelling.size), "20");
    ASSERT_FALSE(foreign.has_value());
    EXPECT_EQ(foreign.error_if()->code(), error_code::wrong_type_or_access);
    EXPECT_EQ(foreign.error_if()->access_reason(), access_error_reason::foreign_index);
    ASSERT_FALSE(unset.has_value());
    EXPECT_EQ(unset.error_if()->access_reason(), access_error_reason::index_out_of_range);
    EXPECT_EQ(provider.m_attempts, attempts);
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * Arrange: Two parsed object documents sharing a provider and an index from A's root.
   * Act: Ask B to iterate and look up a member using A's index.
   * Expect: Both index-taking public operations reject ownership before container/name access.
   */
  TEST(J5_039_IndexIdentity, RejectsForeignContainerInIterationAndLookup)
  {
    /* Arrange */
    document_provider provider;
    const parse_options options{};
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    auto first = parse_document({"{}", 2}, options, accounting);
    auto second = parse_document({"{}", 2}, options, accounting);
    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    const auto root = first.value_if()->values.root();
    ASSERT_TRUE(root.has_value());
    const auto index = root.value_if()->index();
    ASSERT_TRUE(index.has_value());
    const decoded_string name{};
    /* Act */
    const auto iterated = second.value_if()->values.iterate(*index.value_if());
    const auto found = second.value_if()->values.lookup_member(*index.value_if(), name);
    /* Expect */
    ASSERT_FALSE(iterated.has_value());
    ASSERT_FALSE(found.has_value());
    EXPECT_EQ(iterated.error_if()->access_reason(), access_error_reason::foreign_index);
    EXPECT_EQ(found.error_if()->access_reason(), access_error_reason::foreign_index);
  }
  /* ----------------------------------------------------------------------------------- */
  /* ================================= End Test_J5_039 ================================== */
  /* =================================== Test_J5_040 ==================================== */
  /*
   * Arrange: A parsed array and a copied index obtained from its second element.
   * Act: Move the document and use the index with destination and source.
   * Expect: It reads 20 in the destination and reports foreign_index in the emptied source.
   */
  TEST(J5_040_IndexIdentity, DocumentMoveTransfersIssuedIndices)
  {
    /* Arrange */
    document_provider provider;
    const parse_options options{};
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    auto parsed = parse_document({"[10,20]", 7}, options, accounting);
    ASSERT_TRUE(parsed.has_value());
    auto& source = parsed.value_if()->values;
    const auto root = source.root();
    ASSERT_TRUE(root.has_value());
    const auto element = root.value_if()->element_at(1);
    ASSERT_TRUE(element.has_value());
    const auto issued = element.value_if()->index();
    ASSERT_TRUE(issued.has_value());
    const value_index copied = *issued.value_if();
    const auto attempts = provider.m_attempts;
    /* Act */
    document destination{std::move(source)};
    const auto accepted = destination.view(copied);
    const auto rejected = source.view(copied);
    /* Expect */
    ASSERT_TRUE(accepted.has_value());
    const auto number = accepted.value_if()->as_number();
    ASSERT_TRUE(number.has_value());
    const auto spelling = (*number.value_if())->spelling();
    EXPECT_EQ(std::string_view(spelling.data, spelling.size), "20");
    ASSERT_FALSE(rejected.has_value());
    EXPECT_EQ(rejected.error_if()->access_reason(), access_error_reason::foreign_index);
    EXPECT_EQ(provider.m_attempts, attempts);
  }
  /* ================================= End Test_J5_040 ================================== */

  /* =================================== Test_J5_018 ==================================== */
  /*
   * Arrange: A high surrogate followed by a BMP unit above the low-surrogate range.
   * Act: Size and export the retained units through checked UTF-8 APIs.
   * Expect: Encoding errors preserve caller output and original decoded units.
   */
  TEST(J5_018_Utf8Boundary, RejectsHighSurrogateFollowedByUpperBmpUnit)
  {
    /* Arrange */
    document_provider provider;
    const std::uint16_t units[] = {0xD800, 0xE000};
    const auto text = decoded_string::copy_units(provider.callbacks(), units, 2);
    ASSERT_TRUE(text.has_value());
    char output[] = {'#', '#', '#', '#'};
    /* Act */
    // The second unit exists but cannot complete the pair; it must not be silently emitted separately.
    const auto size = utf8_size(*text.value_if());
    const auto exported = export_utf8(*text.value_if(), output, sizeof(output));
    /* Expect */
    ASSERT_FALSE(size.has_value());
    ASSERT_FALSE(exported.has_value());
    EXPECT_EQ(size.error_if()->code(), error_code::encoding);
    EXPECT_EQ(exported.error_if()->code(), error_code::encoding);
    for (const char unit : output)
      EXPECT_EQ(unit, '#');
    EXPECT_EQ(*text.value_if()->unit_at(1).value_if(), 0xE000);
  }
  /* ================================= End Test_J5_018 ================================== */
} // namespace
