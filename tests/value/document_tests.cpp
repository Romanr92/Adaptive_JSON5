#include "../shared_infrastructure/document_provider.hpp"
#include "adaptive_json5/value/decoded_name_equal.hpp"
#include "adaptive_json5/value/document.hpp"

#include <array>
#include <cstddef>
#include <cstdlib>
#include <gtest/gtest.h>
#include <limits>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace
{
  using namespace adaptive_json5;
  /* ===================================== Fixtures ===================================== */
  using adaptive_json5_test::document_provider;
  /* =================================== End Fixtures =================================== */
  /* =================================== Test_J5_028 =================================== */
  static_assert(std::is_nothrow_move_constructible_v<document>);
  static_assert(std::is_nothrow_destructible_v<document>);
  static_assert(!std::is_copy_constructible_v<document>);
  static_assert(!std::is_move_assignable_v<document>);
  static_assert(noexcept(std::declval<const document&>().iterate(value_index{})));
  static_assert(noexcept(std::declval<const document&>().lookup_member(value_index{},
                                                                       std::declval<const decoded_string&>())));
  /*
   * Distinct owned value alternatives remain observable.
   * Arrange: Create an empty document.
   * Act: Create and inspect one value.
   * Expect: The exact kind and content are retained.
   */
  TEST(J5_028_Document, ConstructsNull)
  {
    /* Arrange */
    document_provider provider;
    document doc{provider.callbacks(), 8};
    /* Act */
    const auto index = doc.make_null();
    ASSERT_TRUE(index.has_value());
    const auto view = doc.view(*index.value_if());
    /* Expect */
    ASSERT_TRUE(view.has_value());
    EXPECT_EQ(*view.value_if()->kind().value_if(), value_kind::null);
    EXPECT_EQ(doc.size(), 1u);
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * Distinct owned value alternatives remain observable.
   * Arrange: Create an empty document.
   * Act: Create and inspect one value.
   * Expect: The exact kind and content are retained.
   */
  TEST(J5_028_Document, ConstructsBoolean)
  {
    /* Arrange */
    document_provider provider;
    document doc{provider.callbacks(), 8};
    /* Act */
    const auto index = doc.make_boolean(true);
    ASSERT_TRUE(index.has_value());
    const auto view = doc.view(*index.value_if());
    /* Expect */
    ASSERT_TRUE(view.has_value());
    EXPECT_EQ(*view.value_if()->kind().value_if(), value_kind::boolean);
    EXPECT_EQ(doc.size(), 1u);
    EXPECT_TRUE(*view.value_if()->as_boolean().value_if());
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * Distinct owned value alternatives remain observable.
   * Arrange: Create an empty document.
   * Act: Create and inspect one value.
   * Expect: The exact kind and content are retained.
   */
  TEST(J5_028_Document, ConstructsArray)
  {
    /* Arrange */
    document_provider provider;
    document doc{provider.callbacks(), 8};
    /* Act */
    const auto index = doc.make_array();
    ASSERT_TRUE(index.has_value());
    const auto view = doc.view(*index.value_if());
    /* Expect */
    ASSERT_TRUE(view.has_value());
    EXPECT_EQ(*view.value_if()->kind().value_if(), value_kind::array);
    EXPECT_EQ(doc.size(), 1u);
    EXPECT_EQ(*view.value_if()->container_size().value_if(), 0u);
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * Distinct owned value alternatives remain observable.
   * Arrange: Create an empty document.
   * Act: Create and inspect one value.
   * Expect: The exact kind and content are retained.
   */
  TEST(J5_028_Document, ConstructsObject)
  {
    /* Arrange */
    document_provider provider;
    document doc{provider.callbacks(), 8};
    /* Act */
    const auto index = doc.make_object();
    ASSERT_TRUE(index.has_value());
    const auto view = doc.view(*index.value_if());
    /* Expect */
    ASSERT_TRUE(view.has_value());
    EXPECT_EQ(*view.value_if()->kind().value_if(), value_kind::object);
    EXPECT_EQ(doc.size(), 1u);
    EXPECT_EQ(*view.value_if()->container_size().value_if(), 0u);
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * Owned scalar payloads survive record growth and input mutation.
   * Arrange: Copy exact UTF-16 and numeric spelling.
   * Act: Transfer both payloads and force storage growth.
   * Expect: Views reacquire records and exact original payloads survive.
   */
  TEST(J5_028_Document, RetainsOwnedStringAndNumber)
  {
    /* Arrange */
    document_provider provider;
    document doc{provider.callbacks(), 32};
    char16_t units[] = {u'a', u'\0', static_cast<char16_t>(0xD800)};
    auto text = decoded_string::copy_units(provider.callbacks(), units, 3);
    std::string spelling = "9007199254740993";
    auto number =
      owned_number::copy_spelling({spelling.data(), spelling.size()}, syntax_mode::json5, provider.callbacks(), 32);
    ASSERT_TRUE(text.has_value());
    ASSERT_TRUE(number.has_value());
    /* Act */
    const auto string_index = doc.make_string(std::move(*text.value_if()));
    const auto number_index = doc.make_number(std::move(*number.value_if()));
    ASSERT_TRUE(string_index.has_value());
    ASSERT_TRUE(number_index.has_value());
    const auto string_view = doc.view(*string_index.value_if());
    ASSERT_TRUE(string_view.has_value());
    for (std::size_t i{}; i < 20; ++i)
    {
      ASSERT_TRUE(doc.make_null().has_value());
    }
    units[0] = u'z';
    spelling.assign(spelling.size(), '0');
    const auto string = string_view.value_if()->as_string();
    const auto number_view = doc.view(*number_index.value_if());
    ASSERT_TRUE(number_view.has_value());
    const auto stored_number = number_view.value_if()->as_number();
    /* Expect */
    ASSERT_TRUE(string.has_value());
    EXPECT_EQ((*string.value_if())->size(), 3u);
    EXPECT_EQ(*(*string.value_if())->unit_at(0).value_if(), u'a');
    EXPECT_EQ(*(*string.value_if())->unit_at(1).value_if(), u'\0');
    EXPECT_EQ(*(*string.value_if())->unit_at(2).value_if(), static_cast<char16_t>(0xD800));
    ASSERT_TRUE(stored_number.has_value());
    const auto bytes = (*stored_number.value_if())->spelling();
    EXPECT_EQ(std::string(bytes.data, bytes.size), "9007199254740993");
    EXPECT_EQ(*string_view.value_if()->kind().value_if(), value_kind::string);
    EXPECT_EQ(*number_view.value_if()->kind().value_if(), value_kind::number);
  }
  /* ----------------------------------------------------------------------------------- */
  /* ================================= End Test_J5_028 ================================= */
  /* =================================== Test_J5_030 =================================== */
  /*
   * Repeated array values and nested containers retain direct-child order.
   * Arrange: Create an array with repeated booleans and a nested array.
   * Act: Attach children and enumerate them once.
   * Expect: Indexing and iteration agree without allocating or visiting descendants.
   */
  TEST(J5_030_Document, PreservesOrderAndSinglePassIteration)
  {
    /* Arrange */
    document_provider provider;
    document doc{provider.callbacks(), 16};
    const auto array = doc.make_array();
    const auto first = doc.make_boolean(true);
    const auto second = doc.make_boolean(true);
    const auto nested = doc.make_array();
    const auto leaf = doc.make_null();
    ASSERT_TRUE(array.has_value());
    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    ASSERT_TRUE(nested.has_value());
    ASSERT_TRUE(leaf.has_value());
    ASSERT_TRUE(doc.append_element(*nested.value_if(), *leaf.value_if()).has_value());
    /* Act */
    ASSERT_TRUE(doc.append_element(*array.value_if(), *first.value_if()).has_value());
    ASSERT_TRUE(doc.append_element(*array.value_if(), *second.value_if()).has_value());
    ASSERT_TRUE(doc.append_element(*array.value_if(), *nested.value_if()).has_value());
    auto iterator = doc.iterate(*array.value_if());
    ASSERT_TRUE(iterator.has_value());
    const auto attempts = provider.m_attempts;
    const std::array<value_kind, 3> expected{value_kind::boolean, value_kind::boolean, value_kind::array};
    /* Expect */
    const auto view = doc.view(*array.value_if());
    ASSERT_TRUE(view.has_value());
    EXPECT_EQ(*view.value_if()->container_size().value_if(), 3u);
    for (std::size_t i{}; i < expected.size(); ++i)
    {
      const auto next = iterator.value_if()->next();
      ASSERT_TRUE(next.has_value());
      ASSERT_TRUE(next.value_if()->has_value());
      EXPECT_EQ(*next.value_if()->value().kind().value_if(), expected[i]);
      const auto indexed = view.value_if()->element_at(i);
      ASSERT_TRUE(indexed.has_value());
      EXPECT_EQ(*indexed.value_if()->kind().value_if(), expected[i]);
    }
    for (int i{}; i < 2; ++i)
    {
      const auto end = iterator.value_if()->next();
      ASSERT_TRUE(end.has_value());
      EXPECT_FALSE(end.value_if()->has_value());
    }
    EXPECT_EQ(provider.m_attempts, attempts);
  }
  /* ----------------------------------------------------------------------------------- */
  /* ================================= End Test_J5_030 ================================= */
  /* =================================== Test_J5_031 =================================== */
  /*
   * Lookup never silently chooses a duplicate member.
   * Arrange: Create an object with x, y, x and an empty name.
   * Act: Look up all outcome classes and enumerate every member.
   * Expect: Ambiguous and absent expose no value; all four members remain ordered.
   */
  TEST(J5_031_Document, PreservesDuplicatesAndReportsLookupOutcomes)
  {
    /* Arrange */
    document_provider provider;
    document doc{provider.callbacks(), 16};
    const auto object = doc.make_object();
    ASSERT_TRUE(object.has_value());
    const std::array<const char16_t*, 4> names{u"x", u"y", u"x", u""};
    for (std::size_t i{}; i < names.size(); ++i)
    {
      auto name = decoded_string::copy_units(provider.callbacks(), names[i], i == 3 ? 0 : 1);
      const auto child = doc.make_boolean(i == 1);
      ASSERT_TRUE(name.has_value());
      ASSERT_TRUE(child.has_value());
      ASSERT_TRUE(doc.append_member(*object.value_if(), std::move(*name.value_if()), *child.value_if()).has_value());
    }
    auto x = decoded_string::copy_units(provider.callbacks(), u"x", 1);
    auto y = decoded_string::copy_units(provider.callbacks(), u"y", 1);
    auto z = decoded_string::copy_units(provider.callbacks(), u"z", 1);
    ASSERT_TRUE(x.has_value());
    ASSERT_TRUE(y.has_value());
    ASSERT_TRUE(z.has_value());
    decoded_string empty;
    /* Act */
    const auto attempts = provider.m_attempts;
    const auto duplicate = doc.lookup_member(*object.value_if(), *x.value_if());
    const auto unique = doc.lookup_member(*object.value_if(), *y.value_if());
    const auto absent = doc.lookup_member(*object.value_if(), *z.value_if());
    const auto empty_match = doc.lookup_member(*object.value_if(), empty);
    auto iterator = doc.iterate(*object.value_if());
    /* Expect */
    ASSERT_TRUE(duplicate.has_value());
    ASSERT_TRUE(unique.has_value());
    ASSERT_TRUE(absent.has_value());
    EXPECT_EQ(duplicate.value_if()->state, member_lookup_state::ambiguous);
    EXPECT_FALSE(duplicate.value_if()->value.has_value());
    EXPECT_EQ(absent.value_if()->state, member_lookup_state::absent);
    EXPECT_FALSE(absent.value_if()->value.has_value());
    EXPECT_EQ(unique.value_if()->state, member_lookup_state::unique);
    ASSERT_TRUE(unique.value_if()->value.has_value());
    EXPECT_TRUE(*unique.value_if()->value->as_boolean().value_if());
    ASSERT_TRUE(empty_match.has_value());
    EXPECT_EQ(empty_match.value_if()->state, member_lookup_state::unique);
    ASSERT_TRUE(iterator.has_value());
    for (std::size_t i{}; i < names.size(); ++i)
    {
      const auto child = iterator.value_if()->next();
      ASSERT_TRUE(child.has_value());
      ASSERT_TRUE(child.value_if()->has_value());
      const auto name = child.value_if()->value().member_name();
      ASSERT_TRUE(name.has_value());
      EXPECT_EQ((*name.value_if())->size(), i == 3 ? 0u : 1u);
      if (i != 3)
      {
        EXPECT_EQ(*(*name.value_if())->unit_at(0).value_if(), names[i][0]);
      }
      const auto view = doc.view(*object.value_if());
      ASSERT_TRUE(view.has_value());
      EXPECT_TRUE(view.value_if()->member_at(i).has_value());
    }
    EXPECT_EQ(provider.m_attempts, attempts);
  }
  /* ----------------------------------------------------------------------------------- */
  /* ================================= End Test_J5_031 ================================= */
  /* =================================== Test_J5_033 =================================== */
  /**
   * @brief Exact decoded-name fixtures without normalization or case folding.
   * @par Requirements
   * P030; J5-033.
   */
  struct name_case
  {
      std::u16string left;  // Input decoded UTF-16 units.
      std::u16string right; // Input comparison UTF-16 units.
      bool equal;           // Expected exact equality.
  };
  class J5_033_DecodedName : public testing::TestWithParam<name_case>
  {
  };
  /*
   * Compare exact decoded content.
   * Arrange: Copy parameter names through caller storage.
   * Act: Compare in both directions.
   * Expect: Literal equality expectations including NULs and surrogates.
   */
  TEST_P(J5_033_DecodedName, ComparesExactUnits)
  {
    /* Arrange */
    document_provider provider;
    const auto& input = GetParam();
    auto left = decoded_string::copy_units(provider.callbacks(), input.left.data(), input.left.size());
    auto right = decoded_string::copy_units(provider.callbacks(), input.right.data(), input.right.size());
    ASSERT_TRUE(left.has_value());
    ASSERT_TRUE(right.has_value());
    /* Act */
    const auto attempts = provider.m_attempts;
    const auto forward = decoded_name_equal(*left.value_if(), *right.value_if());
    const auto reverse = decoded_name_equal(*right.value_if(), *left.value_if());
    /* Expect */
    ASSERT_TRUE(forward.has_value());
    ASSERT_TRUE(reverse.has_value());
    EXPECT_EQ(*forward.value_if(), input.equal);
    EXPECT_EQ(*reverse.value_if(), input.equal);
    EXPECT_EQ(provider.m_attempts, attempts);
  }
  INSTANTIATE_TEST_SUITE_P(
    P030, J5_033_DecodedName,
    testing::Values(
      name_case{u"", u"", true}, name_case{u"x", u"x", true}, name_case{u"x", u"X", false},
      name_case{u"x", u"xx", false}, name_case{u"abc", u"abd", false}, name_case{u"\u00e9", u"e\u0301", false},
      name_case{std::u16string{u'a', u'\0', u'b'}, std::u16string{u'a', u'\0', u'b'}, true},
      name_case{std::u16string{u'a', u'\0', u'b'}, std::u16string{u'a', u'\0', u'c'}, false},
      name_case{std::u16string{static_cast<char16_t>(0xD800)}, std::u16string{static_cast<char16_t>(0xD800)}, true},
      name_case{std::u16string{static_cast<char16_t>(0xD800)}, std::u16string{static_cast<char16_t>(0xDC00)}, false}));
  /* ================================= End Test_J5_033 ================================= */
  /* =================================== Test_J5_039 =================================== */
  /*
   * RejectsInvalidViewsAndWrongTypes.
   * Arrange: Create empty storage, an invalid view and all value kinds.
   * Act: Inspect invalid indices, scalar access and container mismatches.
   * Expect: Every ordinary invalid access is explicit and nonthrowing.
   */
  TEST(J5_039_Document, RejectsInvalidViewsAndWrongTypes)
  {
    /* Arrange */
    document_provider provider;
    document doc{provider.callbacks(), 16};
    value_view invalid;
    decoded_string name;
    const auto null = doc.make_null();
    const auto boolean = doc.make_boolean(false);
    const auto array = doc.make_array();
    const auto object = doc.make_object();
    ASSERT_TRUE(null.has_value());
    ASSERT_TRUE(boolean.has_value());
    ASSERT_TRUE(array.has_value());
    ASSERT_TRUE(object.has_value());
    /* Act */
    const auto missing = doc.view(value_index{});
    const auto outside = doc.view(value_index{doc.size()});
    const auto scalar = doc.view(*null.value_if());
    ASSERT_TRUE(scalar.has_value());
    const auto array_view = doc.view(*array.value_if());
    ASSERT_TRUE(array_view.has_value());
    const auto object_view = doc.view(*object.value_if());
    ASSERT_TRUE(object_view.has_value());
    /* Expect */
    EXPECT_FALSE(missing.has_value());
    EXPECT_FALSE(outside.has_value());
    EXPECT_EQ(missing.error_if()->code, error_code::wrong_type_or_access);
    EXPECT_FALSE(doc.root().has_value());
    EXPECT_FALSE(invalid.kind().has_value());
    EXPECT_FALSE(invalid.as_boolean().has_value());
    EXPECT_FALSE(invalid.as_string().has_value());
    EXPECT_FALSE(invalid.as_number().has_value());
    EXPECT_FALSE(invalid.container_size().has_value());
    EXPECT_FALSE(invalid.member_name().has_value());
    EXPECT_FALSE(invalid.element_at(0).has_value());
    EXPECT_FALSE(invalid.member_at(0).has_value());
    EXPECT_FALSE(scalar.value_if()->as_boolean().has_value());
    EXPECT_FALSE(scalar.value_if()->as_string().has_value());
    EXPECT_FALSE(scalar.value_if()->as_number().has_value());
    EXPECT_FALSE(scalar.value_if()->container_size().has_value());
    EXPECT_FALSE(scalar.value_if()->member_name().has_value());
    EXPECT_FALSE(doc.iterate(*null.value_if()).has_value());
    EXPECT_FALSE(doc.iterate(value_index{}).has_value());
    EXPECT_FALSE(doc.lookup_member(*null.value_if(), name).has_value());
    EXPECT_FALSE(doc.lookup_member(*array.value_if(), name).has_value());
    EXPECT_FALSE(doc.lookup_member(value_index{}, name).has_value());
    EXPECT_FALSE(array_view.value_if()->member_at(0).has_value());
    EXPECT_FALSE(object_view.value_if()->element_at(0).has_value());
    EXPECT_FALSE(array_view.value_if()->element_at(0).has_value());
    EXPECT_FALSE(object_view.value_if()->member_at(0).has_value());
    EXPECT_FALSE(array_view.value_if()->element_at(std::numeric_limits<std::size_t>::max()).has_value());
    value_iterator iterator;
    EXPECT_FALSE(iterator.next().has_value());
    auto empty = doc.iterate(*array.value_if());
    ASSERT_TRUE(empty.has_value());
    const auto end = empty.value_if()->next();
    ASSERT_TRUE(end.has_value());
    EXPECT_FALSE(end.value_if()->has_value());
    const auto absent = doc.lookup_member(*object.value_if(), name);
    ASSERT_TRUE(absent.has_value());
    EXPECT_EQ(absent.value_if()->state, member_lookup_state::absent);
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * RejectsRelationshipChangesAtomically.
   * Arrange: Create detached containers, a scalar and an external name.
   * Act: Attempt wrong types, self attachment, cycles, reattachment and root reuse.
   * Expect: Failed attachment retains the name and root; detached child remains reusable.
   */
  TEST(J5_039_Document, RejectsRelationshipChangesAtomically)
  {
    /* Arrange */
    document_provider provider;
    document doc{provider.callbacks(), 16};
    const auto outer = doc.make_array();
    const auto inner = doc.make_array();
    const auto object = doc.make_object();
    const auto scalar = doc.make_null();
    ASSERT_TRUE(outer.has_value());
    ASSERT_TRUE(inner.has_value());
    ASSERT_TRUE(object.has_value());
    ASSERT_TRUE(scalar.has_value());
    auto name = decoded_string::copy_units(provider.callbacks(), u"kept", 4);
    ASSERT_TRUE(name.has_value());
    const auto attempts = provider.m_attempts;
    /* Act */
    EXPECT_FALSE(doc.append_element(*scalar.value_if(), *inner.value_if()).has_value());
    EXPECT_FALSE(doc.append_element(*object.value_if(), *scalar.value_if()).has_value());
    EXPECT_FALSE(doc.append_member(*outer.value_if(), std::move(*name.value_if()), *scalar.value_if()).has_value());
    EXPECT_FALSE(doc.append_element(*outer.value_if(), *outer.value_if()).has_value());
    EXPECT_FALSE(doc.append_element(value_index{}, *inner.value_if()).has_value());
    EXPECT_FALSE(doc.append_element(*outer.value_if(), value_index{}).has_value());
    ASSERT_TRUE(doc.append_element(*outer.value_if(), *inner.value_if()).has_value());
    EXPECT_FALSE(doc.append_element(*inner.value_if(), *outer.value_if()).has_value());
    EXPECT_FALSE(doc.append_element(*outer.value_if(), *inner.value_if()).has_value());
    EXPECT_FALSE(doc.set_root(*inner.value_if()).has_value());
    EXPECT_FALSE(doc.set_root(value_index{}).has_value());
    ASSERT_TRUE(doc.set_root(*outer.value_if()).has_value());
    EXPECT_FALSE(doc.set_root(*object.value_if()).has_value());
    EXPECT_FALSE(doc.append_element(*inner.value_if(), *outer.value_if()).has_value());
    /* Expect */
    EXPECT_EQ(name.value_if()->size(), 4u);
    EXPECT_EQ(*name.value_if()->unit_at(0).value_if(), u'k');
    ASSERT_TRUE(doc.append_member(*object.value_if(), std::move(*name.value_if()), *scalar.value_if()).has_value());
    EXPECT_EQ(name.value_if()->size(), 0u);
    const auto root = doc.root();
    ASSERT_TRUE(root.has_value());
    EXPECT_EQ(*root.value_if()->container_size().value_if(), 1u);
    EXPECT_EQ(provider.m_attempts, attempts);
  }
  /* ----------------------------------------------------------------------------------- */
  /* ================================= End Test_J5_039 ================================= */
  /* =================================== Test_J5_040 =================================== */
  /*
   * MovesOwnedTreeAndReusesSource.
   * Arrange: Build a rooted object owning a named string.
   * Act: Move the owner and create a new root in the source.
   * Expect: New views observe retained content; both owners remain independent.
   */
  TEST(J5_040_Document, MovesOwnedTreeAndReusesSource)
  {
    /* Arrange */
    document_provider provider;
    document source{provider.callbacks(), 16};
    const auto object = source.make_object();
    ASSERT_TRUE(object.has_value());
    auto text = decoded_string::copy_units(provider.callbacks(), u"payload", 7);
    auto name = decoded_string::copy_units(provider.callbacks(), u"key", 3);
    ASSERT_TRUE(text.has_value());
    ASSERT_TRUE(name.has_value());
    const auto child = source.make_string(std::move(*text.value_if()));
    ASSERT_TRUE(child.has_value());
    ASSERT_TRUE(source.append_member(*object.value_if(), std::move(*name.value_if()), *child.value_if()).has_value());
    ASSERT_TRUE(source.set_root(*object.value_if()).has_value());
    const auto attempts = provider.m_attempts;
    /* Act */
    document moved{std::move(source)};
    EXPECT_EQ(provider.m_attempts, attempts);
    EXPECT_EQ(source.size(), 0u);
    EXPECT_FALSE(source.root().has_value());
    const auto replacement = source.make_boolean(false);
    ASSERT_TRUE(replacement.has_value());
    ASSERT_TRUE(source.set_root(*replacement.value_if()).has_value());
    const auto root = moved.root();
    ASSERT_TRUE(root.has_value());
    const auto member = root.value_if()->member_at(0);
    ASSERT_TRUE(member.has_value());
    const auto stored = member.value_if()->as_string();
    /* Expect */
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ((*stored.value_if())->size(), 7u);
    EXPECT_EQ(*(*stored.value_if())->unit_at(6).value_if(), u'd');
    EXPECT_EQ(moved.size(), 2u);
    EXPECT_EQ(source.size(), 1u);
    EXPECT_FALSE(*source.root().value_if()->as_boolean().value_if());
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * DestroysDeepTreeWithoutFollowingChildren.
   * Arrange: Create many detached arrays.
   * Act: Attach each new parent above the previous subtree and destroy the owner.
   * Expect: Every record allocation is released exactly once.
   */
  TEST(J5_040_Document, DestroysDeepTreeWithoutFollowingChildren)
  {
    /* Arrange */
    document_provider provider;
    constexpr std::size_t depth = 2048;
    /* Act */
    {
      document doc{provider.callbacks(), depth};
      auto first = doc.make_array();
      ASSERT_TRUE(first.has_value());
      value_index subtree = *first.value_if();
      for (std::size_t i = 1; i < depth; ++i)
      {
        const auto parent = doc.make_array();
        ASSERT_TRUE(parent.has_value());
        ASSERT_TRUE(doc.append_element(*parent.value_if(), subtree).has_value());
        subtree = *parent.value_if();
      }
      ASSERT_TRUE(doc.set_root(subtree).has_value());
      EXPECT_EQ(doc.size(), depth);
    }
    /* Expect */
    EXPECT_EQ(provider.m_live_bytes, 0u);
    EXPECT_EQ(provider.m_allocations, provider.m_releases);
  }
  /* ----------------------------------------------------------------------------------- */
  /* ================================= End Test_J5_040 ================================= */
  /* =================================== Test_J5_046 =================================== */
  /*
   * EnforcesRecordLimitsBeforeAllocation.
   * Arrange: Configure zero and exact record limits.
   * Act: Create at the boundary and attempt one value beyond it.
   * Expect: Explicit resource-limit failures preserve size and avoid allocation.
   */
  TEST(J5_046_Document, EnforcesRecordLimitsBeforeAllocation)
  {
    /* Arrange */
    document_provider provider;
    document zero{provider.callbacks(), 0};
    document doc{provider.callbacks(), 2};
    /* Act */
    const auto denied = zero.make_null();
    EXPECT_EQ(provider.m_attempts, 0u);
    ASSERT_TRUE(doc.make_null().has_value());
    ASSERT_TRUE(doc.make_boolean(true).has_value());
    const auto attempts = provider.m_attempts;
    const auto excess = doc.make_array();
    /* Expect */
    ASSERT_FALSE(denied.has_value());
    EXPECT_EQ(denied.error_if()->code, error_code::resource_limit);
    ASSERT_FALSE(excess.has_value());
    EXPECT_EQ(excess.error_if()->code, error_code::resource_limit);
    EXPECT_EQ(doc.size(), 2u);
    EXPECT_EQ(provider.m_attempts, attempts);
    document missing{{}, 1};
    const auto no_provider = missing.make_null();
    ASSERT_FALSE(no_provider.has_value());
    EXPECT_EQ(no_provider.error_if()->code, error_code::allocation_failure);
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * RetriesEveryRecordGrowthFailure.
   * Arrange: Choose each allocation in a seven-request growth sequence.
   * Act: Inject one growth failure, inspect old records, then retry.
   * Expect: Each targeted failure occurs and reuse reaches the exact limit.
   */
  TEST(J5_046_Document, RetriesEveryRecordGrowthFailure)
  {
    /* Arrange */
    constexpr std::size_t records = 33;
    /* Act */
    for (std::size_t fail = 1; fail <= 7; ++fail)
    {
      document_provider provider;
      provider.m_fail_on = fail;
      document doc{provider.callbacks(), records};
      bool observed = false;
      for (std::size_t i{}; i < records; ++i)
      {
        const auto added = doc.make_boolean(i % 2 == 0);
        if (!added.has_value())
        {
          observed = true;
          EXPECT_EQ(added.error_if()->code, error_code::allocation_failure);
          EXPECT_EQ(doc.size(), i);
          for (std::size_t j{}; j < i; ++j)
          {
            const auto old = doc.view(value_index{j});
            ASSERT_TRUE(old.has_value());
            EXPECT_EQ(*old.value_if()->as_boolean().value_if(), j % 2 == 0);
          }
          provider.m_fail_on = 0;
          const auto retry = doc.make_boolean(i % 2 == 0);
          ASSERT_TRUE(retry.has_value());
          EXPECT_EQ(retry.value_if()->position(), i);
        }
      }
      /* Expect */
      EXPECT_TRUE(observed);
      EXPECT_EQ(doc.size(), records);
    }
  }
  /* ----------------------------------------------------------------------------------- */
  /* ----------------------------------------------------------------------------------- */
  /* ================================= End Test_J5_046 ================================= */
  /* ==================================== Test_J5_033 ==================================== */
  /*
   * Exact names drive lookup outcomes as well as standalone equality.
   * Arrange: Store one member using the left name from each fixture.
   * Act: Look up its right name without changing the document.
   * Expect: A unique match only for exact decoded equality.
   */
  TEST_P(J5_033_DecodedName, LookupUsesExactUnits)
  {
    /* Arrange */
    document_provider provider;
    document doc{provider.callbacks(), 2};
    const auto& input = GetParam();
    auto stored = decoded_string::copy_units(provider.callbacks(), input.left.data(), input.left.size());
    auto query = decoded_string::copy_units(provider.callbacks(), input.right.data(), input.right.size());
    ASSERT_TRUE(stored.has_value());
    ASSERT_TRUE(query.has_value());
    const auto object = doc.make_object();
    const auto child = doc.make_null();
    ASSERT_TRUE(object.has_value());
    ASSERT_TRUE(child.has_value());
    ASSERT_TRUE(doc.append_member(*object.value_if(), std::move(*stored.value_if()), *child.value_if()).has_value());
    /* Act */
    const auto attempts = provider.m_attempts;
    const auto lookup = doc.lookup_member(*object.value_if(), *query.value_if());
    /* Expect */
    ASSERT_TRUE(lookup.has_value());
    EXPECT_EQ(lookup.value_if()->state, input.equal ? member_lookup_state::unique : member_lookup_state::absent);
    EXPECT_EQ(lookup.value_if()->value.has_value(), input.equal);
    if (input.equal)
    {
      EXPECT_EQ(*lookup.value_if()->value->kind().value_if(), value_kind::null);
    }
    EXPECT_EQ(provider.m_attempts, attempts);
  }
  /* ================================== End Test_J5_033 ================================== */
  /* ==================================== Test_J5_040 ==================================== */
  class J5_040_ValueMove : public testing::TestWithParam<value_kind>
  {
  };
  /*
   * Every value alternative survives a document ownership transfer.
   * Arrange: Create one selected root alternative.
   * Act: Move its owning document twice.
   * Expect: Exact kind and content survive, with empty reusable sources.
   */
  TEST_P(J5_040_ValueMove, PreservesEachRootAlternative)
  {
    /* Arrange */
    document_provider provider;
    document source{provider.callbacks(), 1};
    auto text = decoded_string::copy_units(provider.callbacks(), u"", 0);
    auto number = owned_number::copy_spelling({"-0", 2}, syntax_mode::json5, provider.callbacks(), 2);
    ASSERT_TRUE(text.has_value());
    ASSERT_TRUE(number.has_value());
    const auto make_root = [&]() noexcept -> result<value_index>
    {
      switch (GetParam())
      {
      case value_kind::null:
        return source.make_null();
      case value_kind::boolean:
        return source.make_boolean(true);
      case value_kind::string:
        return source.make_string(std::move(*text.value_if()));
      case value_kind::number:
        return source.make_number(std::move(*number.value_if()));
      case value_kind::array:
        return source.make_array();
      case value_kind::object:
        return source.make_object();
      }
      return result<value_index>::failure(error::without_location(error_code::wrong_type_or_access));
    };
    const auto index = make_root();
    ASSERT_TRUE(index.has_value());
    ASSERT_TRUE(source.set_root(*index.value_if()).has_value());
    const auto attempts = provider.m_attempts;
    /* Act */
    document first{std::move(source)};
    document second{std::move(first)};
    const auto root = second.root();
    /* Expect */
    EXPECT_EQ(source.size(), 0u);
    EXPECT_EQ(first.size(), 0u);
    EXPECT_FALSE(source.root().has_value());
    EXPECT_FALSE(first.root().has_value());
    ASSERT_TRUE(root.has_value());
    EXPECT_EQ(*root.value_if()->kind().value_if(), GetParam());
    EXPECT_EQ(provider.m_attempts, attempts);
    if (GetParam() == value_kind::boolean)
    {
      EXPECT_TRUE(*root.value_if()->as_boolean().value_if());
    }
    if (GetParam() == value_kind::string)
    {
      const auto content = root.value_if()->as_string();
      ASSERT_TRUE(content.has_value());
      EXPECT_EQ((*content.value_if())->size(), 0u);
    }
    if (GetParam() == value_kind::number)
    {
      const auto content = root.value_if()->as_number();
      ASSERT_TRUE(content.has_value());
      const auto bytes = (*content.value_if())->spelling();
      EXPECT_EQ(std::string(bytes.data, bytes.size), "-0");
    }
    if (GetParam() == value_kind::array || GetParam() == value_kind::object)
    {
      EXPECT_EQ(*root.value_if()->container_size().value_if(), 0u);
    }
  }
  INSTANTIATE_TEST_SUITE_P(P030, J5_040_ValueMove,
                           testing::Values(value_kind::null, value_kind::boolean, value_kind::string,
                                           value_kind::number, value_kind::array, value_kind::object));
  /* ================================== End Test_J5_040 ================================== */
} // namespace
