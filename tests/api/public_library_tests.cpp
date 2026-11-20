#include "../shared_infrastructure/byte_array.hpp"
#include "../shared_infrastructure/document_provider.hpp"
#include "adaptive_json5/input.hpp"
#include "adaptive_json5/memory/accounted_storage.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/parser.hpp"
#include "adaptive_json5/value/document.hpp"
#include "adaptive_json5/value/utf8_export.hpp"

#include <array>
#include <cstddef>
#include <gtest/gtest.h>
#include <string>
#include <type_traits>
#include <utility>

namespace
{
  using namespace adaptive_json5;
  /* =================================== Test_J5_008 ==================================== */
  // Construction and mutation are internal (ADR 0007): public code can neither build a
  // document nor change a parsed one. Each trait is true only if the call is accessible.
  template <typename T, typename = void>
  struct can_construct_empty : std::false_type
  {
  };
  template <typename T>
  struct can_construct_empty<T, std::void_t<decltype(T{std::declval<const storage_callbacks&>(), std::size_t{}})>>
      : std::true_type
  {
  };

#define ADAPTIVE_JSON5_TEST_MEMBER_TRAIT(trait, call)                                                                  \
  template <typename T, typename = void>                                                                               \
  struct trait : std::false_type                                                                                       \
  {                                                                                                                    \
  };                                                                                                                   \
  template <typename T>                                                                                                \
  struct trait<T, std::void_t<decltype(std::declval<T&>().call)>> : std::true_type                                     \
  {                                                                                                                    \
  }

  ADAPTIVE_JSON5_TEST_MEMBER_TRAIT(can_make_null, make_null());
  ADAPTIVE_JSON5_TEST_MEMBER_TRAIT(can_make_boolean, make_boolean(true));
  ADAPTIVE_JSON5_TEST_MEMBER_TRAIT(can_make_string, make_string(std::declval<decoded_string>()));
  ADAPTIVE_JSON5_TEST_MEMBER_TRAIT(can_make_number, make_number(std::declval<owned_number>()));
  ADAPTIVE_JSON5_TEST_MEMBER_TRAIT(can_make_array, make_array());
  ADAPTIVE_JSON5_TEST_MEMBER_TRAIT(can_make_object, make_object());
  ADAPTIVE_JSON5_TEST_MEMBER_TRAIT(can_append_element, append_element(value_index{}, value_index{}));
  ADAPTIVE_JSON5_TEST_MEMBER_TRAIT(can_append_member,
                                   append_member(value_index{}, std::declval<decoded_string&&>(), value_index{}));
  ADAPTIVE_JSON5_TEST_MEMBER_TRAIT(can_append_member_with_span,
                                   append_member(value_index{}, std::declval<decoded_string&&>(), value_index{},
                                                 source_span{}, std::size_t{}));
  ADAPTIVE_JSON5_TEST_MEMBER_TRAIT(can_set_source_span, set_source_span(value_index{}, source_span{}, std::size_t{}));
  ADAPTIVE_JSON5_TEST_MEMBER_TRAIT(can_set_member_name_source_span,
                                   set_member_name_source_span(value_index{}, source_span{}, std::size_t{}));
  ADAPTIVE_JSON5_TEST_MEMBER_TRAIT(can_set_root, set_root(value_index{}));
  ADAPTIVE_JSON5_TEST_MEMBER_TRAIT(can_read_root, root());
#undef ADAPTIVE_JSON5_TEST_MEMBER_TRAIT

  static_assert(!can_construct_empty<document>::value);
  static_assert(!can_make_null<document>::value);
  static_assert(!can_make_boolean<document>::value);
  static_assert(!can_make_string<document>::value);
  static_assert(!can_make_number<document>::value);
  static_assert(!can_make_array<document>::value);
  static_assert(!can_make_object<document>::value);
  static_assert(!can_append_element<document>::value);
  static_assert(!can_append_member<document>::value);
  static_assert(!can_append_member_with_span<document>::value);
  static_assert(!can_set_source_span<document>::value);
  static_assert(!can_set_member_name_source_span<document>::value);
  static_assert(!can_set_root<document>::value);
  // The detection itself works: the supported read API stays reachable.
  static_assert(can_read_root<const document>::value);
  static_assert(std::is_nothrow_move_constructible_v<document>);

  /*
   * Exercise the compiled library exclusively through supported consumer headers.
   * Arrange: Caller storage and JSON5 text holding an object with one named string.
   * Act: Parse it, move the document, enumerate and export the value.
   * Expect: Exact content and balanced allocations without private include access.
   */
  TEST(J5_008_PublicLibrary, ParsesAndInspectsOwnedValuesThroughPublicHeaders)
  {
    /* Arrange */
    adaptive_json5_test::document_provider provider;
    const parse_options options{};
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    const std::string text = "{name: 'value'}";
    {
      auto parsed = parse_document({text.data(), text.size()}, options, accounting);
      ASSERT_TRUE(parsed.has_value());
      /* Act */
      document moved{std::move(parsed.value_if()->values)};
      const auto root = moved.root();
      ASSERT_TRUE(root.has_value());
      const auto member = root.value_if()->member_at(0);
      ASSERT_TRUE(member.has_value());
      const auto string = member.value_if()->as_string();
      ASSERT_TRUE(string.has_value());
      std::array<char, 5> output{};
      const auto exported = export_utf8(**string.value_if(), output.data(), output.size());
      /* Expect */
      ASSERT_TRUE(exported.has_value());
      EXPECT_EQ(*exported.value_if(), output.size());
      EXPECT_EQ(output, adaptive_json5_test::make_chars('v', 'a', 'l', 'u', 'e'));
    }
    EXPECT_EQ(accounting.current_bytes(), 0u);
  }
  /* ================================= End Test_J5_008 ================================== */
} // namespace
