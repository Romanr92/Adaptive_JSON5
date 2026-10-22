#include "../../../tests/support/document_provider.hpp"
#include "adaptive_json5/detail/value/document_links.hpp"
#include "adaptive_json5/detail/value/member_lookup.hpp"
#include "adaptive_json5/detail/value/value_record.hpp"
#include "adaptive_json5/value/value_iterator.hpp"

#include <gtest/gtest.h>
#include <limits>

namespace
{
  using namespace adaptive_json5;
  using namespace adaptive_json5::detail;
  using adaptive_json5_test::document_provider;

  /** @brief Describe malformed metadata supplied directly to internal record validation. */
  enum class relationship_fault
  {
    count_too_large,
    empty_first,
    empty_last,
    missing_first,
    missing_last,
    invalid_first,
    no_parent,
    wrong_parent,
    unexpected_name,
    extra_sibling,
    missing_sibling
  };
  /** @brief Exercise the same traversal failure assertions for independent relationship faults. */
  class J5_039_RelationshipValidation : public testing::TestWithParam<relationship_fault>
  {
  };

  /* =================================== Test_J5_039 ==================================== */
  /*
   * Arrange: Internal records with one inconsistent count, endpoint or child link.
   * Act: Create an iterator and, if admitted, ask it for its first child.
   * Expect: Validation returns an access error before exposing a foreign or broken child.
   */
  TEST_P(J5_039_RelationshipValidation, RejectsMalformedIteratorChains)
  {
    /* Arrange */
    document_provider provider;
    value_storage values{provider.callbacks(), 8};
    value_record array;
    array.payload.emplace<array_data>();
    ASSERT_TRUE(values.append(std::move(array)).has_value());
    value_record child;
    child.parent = value_index{0};
    ASSERT_TRUE(values.append(std::move(child)).has_value());
    const auto parent_record = values.at(value_index{0});
    const auto child_record = values.at(value_index{1});
    ASSERT_TRUE(parent_record.has_value());
    ASSERT_TRUE(child_record.has_value());
    auto& links = std::get<array_data>((*parent_record.value_if())->payload).elements;
    links = {value_index{1}, value_index{1}, 1};
    auto& item = **child_record.value_if();
    // These are internal fault-injection cases, not trees obtainable from the public builder.
    // Mutating live record fields exercises defensive checks without invalid object lifetimes.
    switch (GetParam())
    {
    case relationship_fault::count_too_large:
      links.count = 3;
      break;
    case relationship_fault::empty_first:
      links.count = 0;
      links.last = {};
      break;
    case relationship_fault::empty_last:
      links.count = 0;
      links.first = {};
      break;
    case relationship_fault::missing_first:
      links.first = {};
      break;
    case relationship_fault::missing_last:
      links.last = {};
      break;
    case relationship_fault::invalid_first:
      links.first = value_index{99};
      break;
    case relationship_fault::no_parent:
      item.parent = {};
      break;
    case relationship_fault::wrong_parent:
      item.parent = value_index{1};
      break;
    case relationship_fault::unexpected_name:
      item.member_name.emplace();
      break;
    case relationship_fault::extra_sibling:
      item.next_sibling = value_index{1};
      break;
    case relationship_fault::missing_sibling:
      links.count = 2;
      break;
    }
    /* Act */
    auto iterator = value_iterator::create(values, value_index{0});
    /* Expect */
    const bool rejected_at_creation =
      GetParam() == relationship_fault::count_too_large || GetParam() == relationship_fault::empty_first ||
      GetParam() == relationship_fault::empty_last || GetParam() == relationship_fault::missing_first ||
      GetParam() == relationship_fault::missing_last;
    if (rejected_at_creation)
    {
      ASSERT_FALSE(iterator.has_value());
      EXPECT_EQ(iterator.error_if()->code, error_code::wrong_type_or_access);
    }
    else
    {
      ASSERT_TRUE(iterator.has_value());
      for (int retry = 0; retry < 2; ++retry)
      {
        const auto next = iterator.value_if()->next();
        ASSERT_FALSE(next.has_value());
        EXPECT_EQ(next.error_if()->code, error_code::wrong_type_or_access);
      }
    }
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * Arrange: The same internal faults in an object member chain.
   * Act: Look up an empty decoded member name.
   * Expect: Lookup propagates iterator validation instead of returning absent or unique.
   */
  TEST_P(J5_039_RelationshipValidation, LookupPropagatesMalformedObjectChains)
  {
    /* Arrange */
    document_provider provider;
    value_storage values{provider.callbacks(), 8};
    value_record object;
    object.payload.emplace<object_data>();
    ASSERT_TRUE(values.append(std::move(object)).has_value());
    value_record child;
    child.parent = value_index{0};
    child.member_name.emplace();
    ASSERT_TRUE(values.append(std::move(child)).has_value());
    const auto parent_record = values.at(value_index{0});
    const auto child_record = values.at(value_index{1});
    ASSERT_TRUE(parent_record.has_value());
    ASSERT_TRUE(child_record.has_value());
    auto& links = std::get<object_data>((*parent_record.value_if())->payload).members;
    links = {value_index{1}, value_index{1}, 1};
    auto& item = **child_record.value_if();
    // Lookup must preserve a broken-chain error; treating it as absence would conceal internal damage.
    switch (GetParam())
    {
    case relationship_fault::count_too_large:
      links.count = 3;
      break;
    case relationship_fault::empty_first:
      links.count = 0;
      links.last = {};
      break;
    case relationship_fault::empty_last:
      links.count = 0;
      links.first = {};
      break;
    case relationship_fault::missing_first:
      links.first = {};
      break;
    case relationship_fault::missing_last:
      links.last = {};
      break;
    case relationship_fault::invalid_first:
      links.first = value_index{99};
      break;
    case relationship_fault::no_parent:
      item.parent = {};
      break;
    case relationship_fault::wrong_parent:
      item.parent = value_index{1};
      break;
    case relationship_fault::unexpected_name:
      item.member_name.reset();
      break;
    case relationship_fault::extra_sibling:
      item.next_sibling = value_index{1};
      break;
    case relationship_fault::missing_sibling:
      links.count = 2;
      break;
    }
    const decoded_string name;
    /* Act */
    const auto found = lookup_member(values, value_index{0}, name);
    /* Expect */
    ASSERT_FALSE(found.has_value());
    EXPECT_EQ(found.error_if()->code, error_code::wrong_type_or_access);
  }
  INSTANTIATE_TEST_SUITE_P(InternalFaults, J5_039_RelationshipValidation,
                           testing::Values(relationship_fault::count_too_large, relationship_fault::empty_first,
                                           relationship_fault::empty_last, relationship_fault::missing_first,
                                           relationship_fault::missing_last, relationship_fault::invalid_first,
                                           relationship_fault::no_parent, relationship_fault::wrong_parent,
                                           relationship_fault::unexpected_name, relationship_fault::extra_sibling,
                                           relationship_fault::missing_sibling),
                           (
                             [](const testing::TestParamInfo<relationship_fault>& info)
                             {
                               const char* names[] = {"CountTooLarge",  "EmptyFirst",   "EmptyLast",     "MissingFirst",
                                                      "MissingLast",    "InvalidFirst", "NoParent",      "WrongParent",
                                                      "UnexpectedName", "ExtraSibling", "MissingSibling"};
                               return names[static_cast<std::size_t>(info.param)];
                             }));

  /* ----------------------------------------------------------------------------------- */
  /*
   * Arrange: An array record with a missing child or mismatched parent/name metadata.
   * Act: Request its first element through a checked value view.
   * Expect: A bad relationship never becomes a successful child view.
   */
  TEST(J5_039_ValueViewValidation, RejectsUnresolvableAndForeignChildren)
  {
    /* Arrange */
    for (int fault = 0; fault < 5; ++fault)
    {
      SCOPED_TRACE(fault);
      document_provider provider;
      value_storage values{provider.callbacks(), 8};
      value_record array;
      array.payload.emplace<array_data>(array_data{{value_index{1}, value_index{1}, 1}});
      ASSERT_TRUE(values.append(std::move(array)).has_value());
      value_record child;
      child.parent = value_index{0};
      if (fault == 1)
        child.parent = {};
      if (fault == 2)
        child.parent = value_index{1};
      if (fault == 3)
        child.member_name.emplace();
      ASSERT_TRUE(values.append(std::move(child)).has_value());
      auto parent = values.at(value_index{0});
      ASSERT_TRUE(parent.has_value());
      if (fault == 0)
        std::get<array_data>((*parent.value_if())->payload).elements.first = value_index{99};
      if (fault == 4)
        std::get<array_data>((*parent.value_if())->payload).elements.count = 3;
      const value_view view{values, value_index{0}};
      /* Act */
      // Access validation must precede constructing the returned borrowed view.
      const auto element = view.element_at(0);
      /* Expect */
      ASSERT_FALSE(element.has_value());
      EXPECT_EQ(element.error_if()->code, error_code::wrong_type_or_access);
    }
  }
  /* ================================= End Test_J5_039 ================================== */
} // namespace
