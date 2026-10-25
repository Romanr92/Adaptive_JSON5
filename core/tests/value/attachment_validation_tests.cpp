#include "../../../tests/shared_infrastructure/document_provider.hpp"
#include "value/document_links.hpp"
#include "value/value_record.hpp"

#include <gtest/gtest.h>
#include <limits>
#include <utility>
#include <variant>

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

  /**
   * @brief Inject one internal relationship inconsistency into three live records.
   * @par Requirements
   * P030; J5-039.
   * @param fault Inconsistency to inject.
   * @param parent Destination array record at index 0.
   * @param previous Existing element record at index 1.
   * @param child Detached candidate record at index 2.
   * @param links Element links of parent.
   * @note Shared by both attach entry points; changes only live internal records.
   */
  void inject_fault(attachment_fault fault, value_record& parent, value_record& previous, value_record& child,
                    container_links& links)
  {
    switch (fault)
    {
    case attachment_fault::ancestor_outside_storage:
      parent.parent = value_index{99};
      break;
    case attachment_fault::ancestor_cycle:
      parent.parent = value_index{0};
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
      previous.parent = {};
      break;
    case attachment_fault::last_foreign_parent:
      previous.parent = value_index{1};
      break;
    case attachment_fault::last_with_sibling:
      previous.next_sibling = value_index{1};
      break;
    case attachment_fault::child_with_sibling:
      child.next_sibling = value_index{1};
      break;
    case attachment_fault::child_with_name:
      child.member_name.emplace();
      break;
    }
  }

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
    inject_fault(GetParam(), parent_record, previous_record, child, links);
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
  /* ----------------------------------------------------------------------------------- */
  /*
   * Verify the loop-free parser entry point keeps every O(1) relationship check.
   * Arrange: The same three live records and injected fault as the attach_child case.
   * Act: Attach the detached third record to the first through attach_to_detached_parent.
   * Expect: The same explicit error codes, with the candidate and existing chain unchanged.
   *         Ancestor faults surface as an attached parent, which the entry point rejects.
   */
  TEST_P(J5_039_AttachmentValidation, DetachedParentEntryRejectsDamagedLinksBeforeChangingChild)
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
    inject_fault(GetParam(), parent_record, previous_record, child, links);
    const auto old_links = links;
    const bool old_sibling = child.next_sibling.is_set();
    const bool old_name = child.member_name.has_value();
    /* Act */
    const auto attached = attach_to_detached_parent(values, {}, value_index{0}, value_index{2}, nullptr);
    /* Expect */
    ASSERT_FALSE(attached.has_value());
    EXPECT_EQ(attached.error_if()->code, GetParam() == attachment_fault::count_overflow
                                           ? error_code::resource_limit
                                           : error_code::wrong_type_or_access);
    EXPECT_FALSE(child.parent.is_set());
    EXPECT_EQ(child.next_sibling.is_set(), old_sibling);
    EXPECT_EQ(child.member_name.has_value(), old_name);
    EXPECT_EQ(links.count, old_links.count);
    EXPECT_EQ(links.first.is_set(), old_links.first.is_set());
    EXPECT_EQ(links.last.is_set(), old_links.last.is_set());
  }

  /* ----------------------------------------------------------------------------------- */
  /*
   * Verify the entry point proves acyclicity locally by requiring a detached parent.
   * Arrange: An array whose parent link is set (no cycle possible) and a detached scalar.
   * Act: Attach the scalar to that array through both entry points.
   * Expect: The loop-free entry rejects with nothing changed; attach_child walks and succeeds.
   */
  TEST(J5_039_DetachedParentAttach, RejectsAttachedParentThatAttachChildAccepts)
  {
    /* Arrange */
    document_provider provider;
    value_storage values{provider.callbacks(), 8};
    value_record outer;
    outer.payload.emplace<object_data>();
    ASSERT_TRUE(values.append(std::move(outer)).has_value());
    value_record array;
    array.payload.emplace<array_data>();
    ASSERT_TRUE(values.append(std::move(array)).has_value());
    ASSERT_TRUE(values.append(value_record{}).has_value());
    auto array_result = values.at(value_index{1});
    auto child_result = values.at(value_index{2});
    ASSERT_TRUE(array_result.has_value());
    ASSERT_TRUE(child_result.has_value());
    auto& array_record = **array_result.value_if();
    auto& child = **child_result.value_if();
    // Only the parent link matters to the detached-parent check; the outer object's links stay unused.
    array_record.parent = value_index{0};
    const auto& elements = std::get<array_data>(array_record.payload).elements;
    /* Act */
    const auto rejected = attach_to_detached_parent(values, {}, value_index{1}, value_index{2}, nullptr);
    const bool unchanged = !child.parent.is_set() && (elements.count == 0U) && !elements.first.is_set();
    const auto walked = attach_child(values, {}, value_index{1}, value_index{2}, nullptr);
    /* Expect */
    ASSERT_FALSE(rejected.has_value());
    EXPECT_EQ(rejected.error_if()->code, error_code::wrong_type_or_access);
    EXPECT_TRUE(unchanged);
    ASSERT_TRUE(walked.has_value());
    ASSERT_TRUE(child.parent.is_set());
    EXPECT_EQ(child.parent.position(), 1U);
    EXPECT_EQ(elements.count, 1U);
  }

  /* ----------------------------------------------------------------------------------- */
  /*
   * Verify successful parser-style attachment of an element and a named member with its span.
   * Arrange: A detached array, a detached object, two detached scalars and an owned name.
   * Act: Attach one scalar to the array and the other, named, to the object.
   * Expect: Links, counts, the transferred name and its span are recorded.
   */
  TEST(J5_039_DetachedParentAttach, LinksElementAndNamedMemberWithSpan)
  {
    /* Arrange */
    document_provider provider;
    value_storage values{provider.callbacks(), 8};
    value_record array;
    array.payload.emplace<array_data>();
    ASSERT_TRUE(values.append(std::move(array)).has_value());
    value_record object;
    object.payload.emplace<object_data>();
    ASSERT_TRUE(values.append(std::move(object)).has_value());
    ASSERT_TRUE(values.append(value_record{}).has_value());
    ASSERT_TRUE(values.append(value_record{}).has_value());
    auto name = decoded_string::copy_units(provider.callbacks(), u"k", 1);
    ASSERT_TRUE(name.has_value());
    const adaptive_json5::source_span span{1, 3}; // Name token at bytes 1..3 of a 5-byte input.
    /* Act */
    const auto element = attach_to_detached_parent(values, {}, value_index{0}, value_index{2}, nullptr);
    const auto member = attach_to_detached_parent(values, {}, value_index{1}, value_index{3}, name.value_if(), span, 5);
    /* Expect */
    ASSERT_TRUE(element.has_value());
    ASSERT_TRUE(member.has_value());
    const auto& elements = std::get<array_data>((*values.at(value_index{0}).value_if())->payload).elements;
    const auto& members = std::get<object_data>((*values.at(value_index{1}).value_if())->payload).members;
    EXPECT_EQ(elements.count, 1U);
    EXPECT_EQ(elements.first.position(), 2U);
    EXPECT_EQ(members.count, 1U);
    EXPECT_EQ(members.last.position(), 3U);
    const auto& named = **values.at(value_index{3}).value_if();
    ASSERT_TRUE(named.member_name.has_value());
    EXPECT_EQ(named.member_name->size(), 1U);
    ASSERT_TRUE(named.member_name_source_span.has_value());
    EXPECT_EQ(named.member_name_source_span->byte_offset, 1U);
    EXPECT_EQ(named.member_name_source_span->byte_length, 3U);
    // Success transfers the name, so the caller's owner is now empty.
    EXPECT_EQ(name.value_if()->size(), 0U);
  }
  /* ================================= End Test_J5_039 ================================== */
} // namespace
