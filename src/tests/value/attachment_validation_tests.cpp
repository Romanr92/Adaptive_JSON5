// SPDX-License-Identifier: Apache-2.0
#include "../../../tests/support/document_provider.hpp"
#include "adaptive_json5/detail/value/document_links.hpp"

#include <gtest/gtest.h>
#include <limits>

namespace
{
  using namespace adaptive_json5;
  using namespace adaptive_json5::detail;
  using adaptive_json5_test::document_provider;

  /** @brief Select a single internal relationship inconsistency for attachment validation. */
  enum class attachment_fault
  {
    ancestor_outside_storage,
    ancestor_cycle,
    count_overflow,
    empty_first,
    empty_last,
    missing_first,
    missing_last,
    invalid_last,
    last_without_parent,
    last_foreign_parent,
    last_with_sibling,
    child_with_sibling,
    child_with_name
  };
  /** @brief Share atomicity assertions across malformed internal relationship descriptions. */
  class J5_039_AttachmentValidation : public testing::TestWithParam<attachment_fault>
  {
  };

  /* =================================== Test_J5_039 ==================================== */
  /*
   * Arrange: Three live internal records with one malformed relationship description.
   * Act: Try to attach the detached third record to the first.
   * Expect: An explicit error leaves both the candidate and existing chain unchanged.
   */
  TEST_P(J5_039_AttachmentValidation, RejectsDamagedLinksBeforeChangingChild)
  {
    /* Arrange */
    document_provider provider;
    value_storage values{provider.callbacks(), 8};
    value_record parent;
    parent.payload.emplace<array_data>();
    ASSERT_TRUE(values.append(std::move(parent)).has_value());
    value_record previous;
    previous.parent = value_index{0};
    ASSERT_TRUE(values.append(std::move(previous)).has_value());
    ASSERT_TRUE(values.append(value_record{}).has_value());
    auto parent_result = values.at(value_index{0});
    auto previous_result = values.at(value_index{1});
    auto child_result = values.at(value_index{2});
    ASSERT_TRUE(parent_result.has_value());
    ASSERT_TRUE(previous_result.has_value());
    ASSERT_TRUE(child_result.has_value());
    auto& parent_record = **parent_result.value_if();
    auto& previous_record = **previous_result.value_if();
    auto& child = **child_result.value_if();
    auto& links = std::get<array_data>(parent_record.payload).elements;
    links = {value_index{1}, value_index{1}, 1};
    // Inject metadata faults only through live internal records. No private-field hacks or fake allocations.
    switch (GetParam())
    {
    case attachment_fault::ancestor_outside_storage:
      parent_record.parent = value_index{99};
      break;
    case attachment_fault::ancestor_cycle:
      parent_record.parent = value_index{0};
      break;
    case attachment_fault::count_overflow:
      links.count = std::numeric_limits<std::size_t>::max();
      break;
    case attachment_fault::empty_first:
      links.count = 0;
      links.last = {};
      break;
    case attachment_fault::empty_last:
      links.count = 0;
      links.first = {};
      break;
    case attachment_fault::missing_first:
      links.first = {};
      break;
    case attachment_fault::missing_last:
      links.last = {};
      break;
    case attachment_fault::invalid_last:
      links.last = value_index{99};
      break;
    case attachment_fault::last_without_parent:
      previous_record.parent = {};
      break;
    case attachment_fault::last_foreign_parent:
      previous_record.parent = value_index{1};
      break;
    case attachment_fault::last_with_sibling:
      previous_record.next_sibling = value_index{1};
      break;
    case attachment_fault::child_with_sibling:
      child.next_sibling = value_index{1};
      break;
    case attachment_fault::child_with_name:
      child.member_name.emplace();
      break;
    }
    const auto old_links = links;
    const auto old_sibling = child.next_sibling;
    const bool old_name = child.member_name.has_value();
    /* Act */
    const auto attached = attach_child(values, {}, value_index{0}, value_index{2}, nullptr);
    /* Expect */
    ASSERT_FALSE(attached.has_value());
    EXPECT_EQ(attached.error_if()->code, GetParam() == attachment_fault::count_overflow
                                           ? error_code::resource_limit
                                           : error_code::wrong_type_or_access);
    EXPECT_FALSE(child.parent.is_set());
    EXPECT_EQ(child.next_sibling.is_set(), old_sibling.is_set());
    if (old_sibling.is_set())
    {
      EXPECT_EQ(child.next_sibling.position(), old_sibling.position());
    }
    EXPECT_EQ(child.member_name.has_value(), old_name);
    EXPECT_EQ(links.count, old_links.count);
    EXPECT_EQ(links.first.is_set(), old_links.first.is_set());
    EXPECT_EQ(links.last.is_set(), old_links.last.is_set());
    if (old_links.first.is_set())
    {
      EXPECT_EQ(links.first.position(), old_links.first.position());
    }
    if (old_links.last.is_set())
    {
      EXPECT_EQ(links.last.position(), old_links.last.position());
    }
  }
  INSTANTIATE_TEST_SUITE_P(
    InternalFaults, J5_039_AttachmentValidation,
    testing::Values(attachment_fault::ancestor_outside_storage, attachment_fault::ancestor_cycle,
                    attachment_fault::count_overflow, attachment_fault::empty_first, attachment_fault::empty_last,
                    attachment_fault::missing_first, attachment_fault::missing_last, attachment_fault::invalid_last,
                    attachment_fault::last_without_parent, attachment_fault::last_foreign_parent,
                    attachment_fault::last_with_sibling, attachment_fault::child_with_sibling,
                    attachment_fault::child_with_name),
    (
      [](const testing::TestParamInfo<attachment_fault>& info)
      {
        const char* names[] = {
          "AncestorOutsideStorage", "AncestorCycle",    "CountOverflow", "EmptyFirst",        "EmptyLast",
          "MissingFirst",           "MissingLast",      "InvalidLast",   "LastWithoutParent", "LastForeignParent",
          "LastWithSibling",        "ChildWithSibling", "ChildWithName"};
        return names[static_cast<std::size_t>(info.param)];
      }));
  /* ================================= End Test_J5_039 ================================== */
} // namespace
