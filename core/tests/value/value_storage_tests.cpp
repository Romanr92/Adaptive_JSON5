#include "../../../tests/shared_infrastructure/document_provider.hpp"
#include "adaptive_json5/value/document.hpp"
#include "value/value_record.hpp"
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
  /* ----------------------------------------------------------------------------------- */
  /*
   * ReserveExactAllocatesOnceAndNeverGrows.
   * Arrange: Storage with a limit of four records.
   * Act: Reserve two exactly, append three records, then reserve again.
   * Expect: One allocation; the third append and the second reservation fail with
   *         wrong_type_or_access and make no provider request (ADR 0003, decision 7).
   */
  TEST(J5_046_Document, ReserveExactAllocatesOnceAndNeverGrows)
  {
    /* Arrange */
    document_provider provider;
    detail::value_storage storage{provider.callbacks(), 4};
    /* Act */
    const auto reserved = storage.reserve_exact(2);
    const std::size_t attempts_after_reserve = provider.m_attempts;
    const auto first = storage.append(detail::value_record{});
    const auto second = storage.append(detail::value_record{});
    const auto third = storage.append(detail::value_record{});
    const auto again = storage.reserve_exact(2);
    /* Expect */
    ASSERT_TRUE(reserved.has_value());
    EXPECT_EQ(attempts_after_reserve, 1U);
    EXPECT_TRUE(first.has_value());
    EXPECT_TRUE(second.has_value());
    ASSERT_FALSE(third.has_value());
    EXPECT_EQ(third.error_if()->code, error_code::wrong_type_or_access);
    ASSERT_FALSE(again.has_value());
    EXPECT_EQ(again.error_if()->code, error_code::wrong_type_or_access);
    EXPECT_EQ(provider.m_attempts, attempts_after_reserve);
    EXPECT_EQ(storage.size(), 2U);
  }

  /* ----------------------------------------------------------------------------------- */
  /*
   * ReserveExactChecksLimitZeroAndFailure.
   * Arrange: Three storages with a limit of two records.
   * Act: Reserve three; reserve zero and append; fail the allocation of two, then retry.
   * Expect: resource_limit without a request; zero allocates nothing and then refuses to grow;
   *         a failed reservation leaves the storage reservable.
   */
  TEST(J5_046_Document, ReserveExactChecksLimitZeroAndFailure)
  {
    /* Arrange */
    document_provider provider;
    detail::value_storage over{provider.callbacks(), 2};
    detail::value_storage empty{provider.callbacks(), 2};
    detail::value_storage failing{provider.callbacks(), 2};
    /* Act */
    const auto above = over.reserve_exact(3);
    const auto zero = empty.reserve_exact(0);
    const std::size_t attempts_after_zero = provider.m_attempts;
    const auto refused = empty.append(detail::value_record{});
    provider.m_fail_on = provider.m_attempts + 1;
    const auto failed = failing.reserve_exact(2);
    provider.m_fail_on = 0;
    const auto retried = failing.reserve_exact(2);
    /* Expect */
    ASSERT_FALSE(above.has_value());
    EXPECT_EQ(above.error_if()->code, error_code::resource_limit);
    EXPECT_TRUE(zero.has_value());
    EXPECT_EQ(attempts_after_zero, 0U);
    ASSERT_FALSE(refused.has_value());
    EXPECT_EQ(refused.error_if()->code, error_code::wrong_type_or_access);
    ASSERT_FALSE(failed.has_value());
    EXPECT_EQ(failed.error_if()->code, error_code::allocation_failure);
    EXPECT_TRUE(retried.has_value());
  }

  /* ----------------------------------------------------------------------------------- */
  /*
   * MovedFromExactStorageGrowsAgain.
   * Arrange: Storage reserved exactly for one record, holding one.
   * Act: Move it into new storage, then append twice to the emptied source.
   * Expect: The destination keeps the record and stays fixed; the source grows normally.
   */
  TEST(J5_046_Document, MovedFromExactStorageGrowsAgain)
  {
    /* Arrange */
    document_provider provider;
    detail::value_storage source{provider.callbacks(), 4};
    ASSERT_TRUE(source.reserve_exact(1).has_value());
    ASSERT_TRUE(source.append(detail::value_record{}).has_value());
    /* Act */
    detail::value_storage destination{std::move(source)};
    const auto beyond = destination.append(detail::value_record{});
    const auto regrown_first = source.append(detail::value_record{});
    const auto regrown_second = source.append(detail::value_record{});
    /* Expect */
    EXPECT_EQ(destination.size(), 1U);
    ASSERT_FALSE(beyond.has_value());
    EXPECT_EQ(beyond.error_if()->code, error_code::wrong_type_or_access);
    EXPECT_TRUE(regrown_first.has_value());
    EXPECT_TRUE(regrown_second.has_value());
    EXPECT_EQ(source.size(), 2U);
  }
  /* ================================= End Test_J5_046 ================================== */
} // namespace
