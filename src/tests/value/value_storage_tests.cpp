// SPDX-License-Identifier: Apache-2.0
#include "../../../tests/support/document_provider.hpp"
#include "adaptive_json5/detail/value/value_record.hpp"
#include "adaptive_json5/value/document.hpp"
namespace
{
  using namespace adaptive_json5;
  using adaptive_json5_test::document_provider;
  /* =================================== Test_J5_046 ==================================== */
  /*
   * PreservesExternalRecordOnFailedAppend.
   * Arrange: Prepare an externally owned string record and fail the first growth.
   * Act: Attempt append and retry the same external record.
   * Expect: Failure retains payload ownership and retry transfers it once.
   */
  TEST(J5_046_Document, PreservesExternalRecordOnFailedAppend)
  {
    /* Arrange */
    document_provider provider;
    detail::value_storage storage{provider.callbacks(), 2};
    auto text = decoded_string::copy_units(provider.callbacks(), u"held", 4);
    ASSERT_TRUE(text.has_value());
    detail::value_record record;
    record.payload.emplace<decoded_string>(std::move(*text.value_if()));
    provider.m_fail_on = provider.m_attempts + 1;
    /* Act */
    const auto failed = storage.append(std::move(record));
    ASSERT_FALSE(failed.has_value());
    EXPECT_EQ(failed.error_if()->code, error_code::allocation_failure);
    EXPECT_EQ(std::get<decoded_string>(record.payload).size(), 4u);
    EXPECT_EQ(storage.size(), 0u);
    provider.m_fail_on = 0;
    const auto retry = storage.append(std::move(record));
    /* Expect */
    ASSERT_TRUE(retry.has_value());
    EXPECT_EQ(std::get<decoded_string>(record.payload).size(), 0u);
    const detail::value_storage& read_only = storage;
    const auto held = read_only.at(*retry.value_if());
    ASSERT_TRUE(held.has_value());
    EXPECT_EQ(std::get<decoded_string>((*held.value_if())->payload).size(), 4u);
    EXPECT_FALSE(storage.at(value_index{}).has_value());
    EXPECT_FALSE(read_only.at(value_index{1}).has_value());
  }
  /* ================================= End Test_J5_046 ================================== */
} // namespace
