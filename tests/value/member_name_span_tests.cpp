#include "../support/document_provider.hpp"

#include <gtest/gtest.h>
#include <limits>

namespace
{
  using namespace adaptive_json5;
  using adaptive_json5_test::document_provider;
  /* =================================== Test_J5_032 ==================================== */
  /*
   * Arrange: Two source ranges and ordinary located/unlocated errors.
   * Act: Construct duplicate error metadata and copy it through an explicit result.
   * Expect: Both occurrences survive by value; ordinary errors carry no duplicate metadata.
   */
  TEST(J5_032_DuplicateError, RetainsBothSpansWithoutAllocation)
  {
    /* Arrange */
    constexpr auto duplicate = error::duplicate_member({1, 3}, {12, 8});
    static_assert(duplicate.code == error_code::duplicate_policy);
    static_assert(duplicate.duplicate_names.has_value());
    static_assert(duplicate.duplicate_names->first.byte_offset == 1);
    const auto located = error::at_offset(error_code::syntax, 0);
    const auto unlocated = error::without_location(error_code::allocation_failure);
    /* Act */
    const auto failed = result<int>::failure(duplicate);
    /* Expect */
    // Inline metadata must remain useful when no diagnostic storage provider exists.
    ASSERT_NE(failed.error_if(), nullptr);
    const auto& actual = *failed.error_if();
    EXPECT_TRUE(actual.has_byte_offset);
    EXPECT_EQ(actual.byte_offset, 12u);
    ASSERT_TRUE(actual.duplicate_names.has_value());
    EXPECT_EQ(actual.duplicate_names->first.byte_offset, 1u);
    EXPECT_EQ(actual.duplicate_names->first.byte_length, 3u);
    EXPECT_EQ(actual.duplicate_names->repeated.byte_offset, 12u);
    EXPECT_EQ(actual.duplicate_names->repeated.byte_length, 8u);
    EXPECT_FALSE(located.duplicate_names.has_value());
    EXPECT_FALSE(unlocated.duplicate_names.has_value());
  }
  /* ================================= End Test_J5_032 ================================== */

  /* =================================== Test_J5_041 ==================================== */
  /*
   * Arrange: A named member without provenance and a detached unnamed value.
   * Act: Read absent metadata, set a range, then try malformed ranges and invalid indices.
   * Expect: Missing metadata is successful absence; invalid access fails and old metadata survives.
   */
  TEST(J5_041_MemberNameSpan, DistinguishesAbsenceAndPreservesRangeOnFailure)
  {
    /* Arrange */
    document_provider provider;
    document values{provider.callbacks(), 8};
    const auto object = values.make_object();
    const auto child = values.make_null();
    const auto detached = values.make_null();
    ASSERT_TRUE(object.has_value());
    ASSERT_TRUE(child.has_value());
    ASSERT_TRUE(detached.has_value());
    decoded_string empty_name;
    ASSERT_TRUE(values.append_member(*object.value_if(), std::move(empty_name), *child.value_if()).has_value());
    const auto member = values.view(*child.value_if());
    ASSERT_TRUE(member.has_value());
    const auto absent = member.value_if()->member_name_source_span();
    ASSERT_TRUE(absent.has_value());
    EXPECT_FALSE(absent.value_if()->has_value());
    const auto unnamed = values.view(*detached.value_if());
    ASSERT_TRUE(unnamed.has_value());
    const auto attempts = provider.m_attempts;
    /* Act */
    const auto stored = values.set_member_name_source_span(*child.value_if(), {1, 2}, 10);
    /* Expect */
    ASSERT_TRUE(stored.has_value());
    // Offset subtraction must reject overflow-sized lengths without corrupting the prior range.
    for (const source_span invalid :
         {source_span{11, 0}, source_span{9, 2}, source_span{1, std::numeric_limits<std::size_t>::max()}})
    {
      EXPECT_FALSE(values.set_member_name_source_span(*child.value_if(), invalid, 10).has_value());
      const auto span = member.value_if()->member_name_source_span();
      ASSERT_TRUE(span.has_value());
      ASSERT_TRUE(span.value_if()->has_value());
      EXPECT_EQ((**span.value_if()).byte_offset, 1u);
      EXPECT_EQ((**span.value_if()).byte_length, 2u);
    }
    EXPECT_FALSE(unnamed.value_if()->member_name_source_span().has_value());
    EXPECT_FALSE(value_view{}.member_name_source_span().has_value());
    EXPECT_FALSE(values.set_member_name_source_span(*detached.value_if(), {1, 2}, 10).has_value());
    EXPECT_FALSE(values.set_member_name_source_span(value_index{99}, {1, 2}, 10).has_value());
    EXPECT_TRUE(values.set_member_name_source_span(*child.value_if(), {10, 0}, 10).has_value());
    EXPECT_EQ(provider.m_attempts, attempts);
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * Arrange: An owned name, a detached child and an object with no members.
   * Act: Reject invalid source ranges, retry atomically with a valid range, then attempt reattachment.
   * Expect: Failures preserve external ownership and existing relationships/span; success transfers once.
   */
  TEST(J5_041_MemberNameAttachment, ValidatesBeforeOwnershipTransfer)
  {
    /* Arrange */
    document_provider provider;
    document values{provider.callbacks(), 8};
    const auto object = values.make_object();
    const auto child = values.make_null();
    ASSERT_TRUE(object.has_value());
    ASSERT_TRUE(child.has_value());
    auto name = decoded_string::copy_units(provider.callbacks(), u"a", 1);
    ASSERT_TRUE(name.has_value());
    const auto parent = values.view(*object.value_if());
    ASSERT_TRUE(parent.has_value());
    const auto attempts = provider.m_attempts;
    /* Act */
    const auto invalid =
      values.append_member(*object.value_if(), std::move(*name.value_if()), *child.value_if(), {9, 2}, 10);
    /* Expect */
    // std::move alone must not empty a supplied owner when validation rejects attachment.
    ASSERT_FALSE(invalid.has_value());
    EXPECT_EQ(invalid.error_if()->code, error_code::wrong_type_or_access);
    EXPECT_EQ(name.value_if()->size(), 1u);
    auto count = parent.value_if()->container_size();
    ASSERT_TRUE(count.has_value());
    EXPECT_EQ(*count.value_if(), 0u);
    ASSERT_TRUE(
      values.append_member(*object.value_if(), std::move(*name.value_if()), *child.value_if(), {1, 3}, 10).has_value());
    decoded_string replacement;
    EXPECT_FALSE(
      values.append_member(*object.value_if(), std::move(replacement), *child.value_if(), {5, 2}, 10).has_value());
    const auto member = values.view(*child.value_if());
    ASSERT_TRUE(member.has_value());
    const auto span = member.value_if()->member_name_source_span();
    ASSERT_TRUE(span.has_value());
    ASSERT_TRUE(span.value_if()->has_value());
    EXPECT_EQ((**span.value_if()).byte_offset, 1u);
    EXPECT_EQ((**span.value_if()).byte_length, 3u);
    const auto final_count = parent.value_if()->container_size();
    ASSERT_TRUE(final_count.has_value());
    EXPECT_EQ(*final_count.value_if(), 1u);
    EXPECT_EQ(provider.m_attempts, attempts);
  }
  /* ================================= End Test_J5_041 ================================== */
} // namespace
