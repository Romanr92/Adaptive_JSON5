// SPDX-License-Identifier: Apache-2.0
#include "../support/document_provider.hpp"
#include "adaptive_json5/memory/accounted_storage.hpp"
#include "adaptive_json5/value/document.hpp"
#include "adaptive_json5/value/utf8_export.hpp"

#include <array>
#include <gtest/gtest.h>
#include <utility>

namespace
{
  using namespace adaptive_json5;
  /* =================================== Test_J5_008 ==================================== */
  /*
   * Exercise the compiled library exclusively through supported consumer headers.
   * Arrange: Caller storage and a document containing an owned named string.
   * Act: Establish the root, move the document, enumerate and export the value.
   * Expect: Exact content and balanced allocations without private include access.
   */
  TEST(J5_008_PublicLibrary, BuildsAndInspectsOwnedValuesThroughPublicHeaders)
  {
    /* Arrange */
    adaptive_json5_test::document_provider provider;
    accounted_storage accounting{provider.callbacks(), 4096};
    {
      document original{accounting.callbacks(), 4};
      const auto object = original.make_object();
      ASSERT_TRUE(object.has_value());
      auto name = decoded_string::copy_units(accounting.callbacks(), u"name", 4);
      auto text = decoded_string::copy_units(accounting.callbacks(), u"value", 5);
      ASSERT_TRUE(name.has_value());
      ASSERT_TRUE(text.has_value());
      const auto value = original.make_string(std::move(*text.value_if()));
      ASSERT_TRUE(value.has_value());
      ASSERT_TRUE(
        original.append_member(*object.value_if(), std::move(*name.value_if()), *value.value_if()).has_value());
      ASSERT_TRUE(original.set_root(*object.value_if()).has_value());
      /* Act */
      document moved{std::move(original)};
      const auto root = moved.root();
      ASSERT_TRUE(root.has_value());
      const auto member = root.value_if()->member_at(0);
      ASSERT_TRUE(member.has_value());
      const auto string = member.value_if()->as_string();
      ASSERT_TRUE(string.has_value());
      std::array<unsigned char, 5> output{};
      const auto exported = export_utf8(**string.value_if(), output.data(), output.size());
      /* Expect */
      ASSERT_TRUE(exported.has_value());
      EXPECT_EQ(*exported.value_if(), output.size());
      EXPECT_EQ(output, (std::array<unsigned char, 5>{'v', 'a', 'l', 'u', 'e'}));
    }
    EXPECT_EQ(accounting.current_bytes(), 0u);
  }
  /* ================================= End Test_J5_008 ================================== */
} // namespace
