#include "../../../tests/shared_infrastructure/document_provider.hpp"
#include "../../../tests/shared_infrastructure/utf16_units.hpp"
#include "adaptive_json5/access_error_reason.hpp"
#include "adaptive_json5/memory/accounted_storage.hpp"
#include "adaptive_json5/value/document.hpp"
#include "value/value_record.hpp"

#include <array>
#include <type_traits>
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
    auto text = decoded_string::copy_units(provider.callbacks(), adaptive_json5_test::utf16(u"held").data(), 4);
    ASSERT_TRUE(text.has_value());
    detail::value_record record;
    record.payload.emplace<decoded_string>(std::move(*text.value_if()));
    provider.m_fail_on = provider.m_attempts + 1;
    /* Act */
    const auto failed = storage.append(std::move(record));
    ASSERT_FALSE(failed.has_value());
    EXPECT_EQ(failed.error_if()->code(), error_code::allocation_failure);
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
    EXPECT_FALSE(read_only.at(storage.issue(1)).has_value());
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * ReserveExactAllocatesTokenAndRecordsAndNeverGrows.
   * Arrange: Storage with a limit of four records.
   * Act: Reserve two exactly, append three records, then reserve again.
   * Expect: Two allocations (identity token and records); the third append and the second reservation fail with
   *         wrong_type_or_access and make no provider request (ADR 0003, decision 7).
   */
  TEST(J5_046_Document, ReserveExactAllocatesTokenAndRecordsAndNeverGrows)
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
    EXPECT_EQ(attempts_after_reserve, 2U);
    EXPECT_TRUE(first.has_value());
    EXPECT_TRUE(second.has_value());
    ASSERT_FALSE(third.has_value());
    EXPECT_EQ(third.error_if()->code(), error_code::wrong_type_or_access);
    ASSERT_FALSE(again.has_value());
    EXPECT_EQ(again.error_if()->code(), error_code::internal_error);
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
    EXPECT_EQ(above.error_if()->code(), error_code::resource_limit);
    EXPECT_TRUE(zero.has_value());
    EXPECT_EQ(attempts_after_zero, 0U);
    ASSERT_FALSE(refused.has_value());
    EXPECT_EQ(refused.error_if()->code(), error_code::wrong_type_or_access);
    ASSERT_FALSE(failed.has_value());
    EXPECT_EQ(failed.error_if()->code(), error_code::allocation_failure);
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
    EXPECT_EQ(beyond.error_if()->code(), error_code::wrong_type_or_access);
    EXPECT_TRUE(regrown_first.has_value());
    EXPECT_TRUE(regrown_second.has_value());
    EXPECT_EQ(source.size(), 2U);
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * Arrange: Exact and growing storage with failure injected at token or record allocation.
   * Act: Attempt the first allocation, inspect rollback and retry without failure injection.
   * Expect: Failure publishes no token or records, releases all acquired blocks and permits retry.
   */
  TEST(J5_046_StorageIdentity, FirstAllocationFailureRollsBackAndRetries)
  {
    /* Arrange */
    for (const bool exact : {false, true})
    {
      for (const std::size_t failure : {1U, 2U})
      {
        SCOPED_TRACE(exact);
        SCOPED_TRACE(failure);
        document_provider provider;
        {
          detail::value_storage storage{provider.callbacks(), 2};
          provider.m_fail_on = failure;
          /* Act */
          if (exact)
          {
            const auto failed = storage.reserve_exact(2);
            ASSERT_FALSE(failed.has_value());
            EXPECT_EQ(failed.error_if()->code(), error_code::allocation_failure);
          }
          else
          {
            const auto failed = storage.append(detail::value_record{});
            ASSERT_FALSE(failed.has_value());
            EXPECT_EQ(failed.error_if()->code(), error_code::allocation_failure);
          }
          /* Expect */
          EXPECT_EQ(storage.identity(), nullptr);
          EXPECT_EQ(storage.size(), 0U);
          EXPECT_EQ(provider.m_attempts, failure);
          EXPECT_EQ(provider.m_live_bytes, 0U);
          EXPECT_EQ(provider.m_allocations, provider.m_releases);
          provider.m_fail_on = 0;
          if (exact)
          {
            ASSERT_TRUE(storage.reserve_exact(2).has_value());
          }
          const auto retry = storage.append(detail::value_record{});
          ASSERT_TRUE(retry.has_value());
          EXPECT_NE(storage.identity(), nullptr);
          EXPECT_TRUE(storage.at(*retry.value_if()).has_value());
        }
        EXPECT_EQ(provider.m_live_bytes, 0U);
        EXPECT_EQ(provider.m_allocations, provider.m_releases);
      }
    }
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * Arrange: One live record and a failure on the next record-block growth request.
   * Act: Append, inspect the original record and token, then retry.
   * Expect: Failure retains the old token, record and allocation ownership; retry reuses the token.
   */
  TEST(J5_046_StorageIdentity, FailedGrowthRetainsIdentityAndRecords)
  {
    /* Arrange */
    document_provider provider;
    detail::value_storage storage{provider.callbacks(), 2};
    const auto first = storage.append(detail::value_record{});
    ASSERT_TRUE(first.has_value());
    const void* identity = storage.identity();
    const auto live_bytes = provider.m_live_bytes;
    const auto allocations = provider.m_allocations;
    provider.m_fail_on = provider.m_attempts + 1U;
    /* Act */
    const auto failed = storage.append(detail::value_record{});
    /* Expect */
    ASSERT_FALSE(failed.has_value());
    EXPECT_EQ(failed.error_if()->code(), error_code::allocation_failure);
    EXPECT_EQ(storage.identity(), identity);
    EXPECT_EQ(storage.size(), 1U);
    EXPECT_TRUE(storage.at(*first.value_if()).has_value());
    EXPECT_EQ(provider.m_live_bytes, live_bytes);
    EXPECT_EQ(provider.m_allocations, allocations);
    provider.m_fail_on = 0;
    ASSERT_TRUE(storage.append(detail::value_record{}).has_value());
    EXPECT_EQ(storage.identity(), identity);
    EXPECT_TRUE(storage.at(*first.value_if()).has_value());
    EXPECT_EQ(provider.m_allocations, allocations + 1U);
  }
  /* ================================= End Test_J5_046 ================================== */
  /* =================================== Test_J5_039 ==================================== */
  /* ----------------------------------------------------------------------------------- */
  /*
   * Arrange: Two live storages with overlapping record positions.
   * Act: Access unset, foreign and same-owner out-of-range indices through both overloads.
   * Expect: Unset and bounds failures report index_out_of_range; foreign ownership wins over bounds.
   */
  TEST(J5_039_StorageIdentity, ValidatesUnsetOwnerThenBounds)
  {
    /* Arrange */
    document_provider provider;
    detail::value_storage first{provider.callbacks(), 2};
    detail::value_storage second{provider.callbacks(), 2};
    ASSERT_TRUE(first.append(detail::value_record{}).has_value());
    ASSERT_TRUE(second.append(detail::value_record{}).has_value());
    /* Act */
    const auto unset = second.at(value_index{});
    const auto foreign = second.at(first.issue(0));
    const auto foreign_outside = second.at(first.issue(99));
    const auto outside = second.at(second.issue(99));
    const auto& read_only = second;
    /* Expect */
    ASSERT_FALSE(unset.has_value());
    ASSERT_FALSE(foreign.has_value());
    ASSERT_FALSE(foreign_outside.has_value());
    ASSERT_FALSE(outside.has_value());
    EXPECT_EQ(unset.error_if()->access_reason(), access_error_reason::index_out_of_range);
    EXPECT_EQ(foreign.error_if()->access_reason(), access_error_reason::foreign_index);
    EXPECT_EQ(foreign_outside.error_if()->access_reason(), access_error_reason::foreign_index);
    EXPECT_EQ(outside.error_if()->access_reason(), access_error_reason::index_out_of_range);
    EXPECT_EQ(read_only.at(value_index{}).error_if()->access_reason(), access_error_reason::index_out_of_range);
    EXPECT_EQ(read_only.at(first.issue(0)).error_if()->access_reason(), access_error_reason::foreign_index);
    EXPECT_EQ(read_only.at(first.issue(99)).error_if()->access_reason(), access_error_reason::foreign_index);
    EXPECT_EQ(read_only.at(second.issue(99)).error_if()->access_reason(), access_error_reason::index_out_of_range);
  }

  /* ----------------------------------------------------------------------------------- */
  /*
   * Arrange: A growing storage with one issued index and a retained identity address.
   * Act: Move it, reject its old index at the source, reuse the source and grow the destination.
   * Expect: Identity follows records; reuse receives a different token and growth preserves indices.
   */
  TEST(J5_039_StorageIdentity, MoveReuseAndGrowthPreserveOwnership)
  {
    /* Arrange */
    document_provider provider;
    detail::value_storage source{provider.callbacks(), 16};
    const auto first = source.append(detail::value_record{});
    ASSERT_TRUE(first.has_value());
    const value_index old = *first.value_if();
    const void* identity = source.identity();
    /* Act */
    detail::value_storage destination{std::move(source)};
    const auto moved_from = source.at(old);
    EXPECT_EQ(source.identity(), nullptr);
    const auto reused = source.append(detail::value_record{});
    /* Expect */
    EXPECT_EQ(destination.identity(), identity);
    ASSERT_TRUE(destination.at(old).has_value());
    ASSERT_FALSE(moved_from.has_value());
    EXPECT_EQ(moved_from.error_if()->access_reason(), access_error_reason::foreign_index);
    ASSERT_TRUE(reused.has_value());
    EXPECT_NE(source.identity(), identity);
    EXPECT_TRUE(source.at(*reused.value_if()).has_value());
    EXPECT_EQ(source.at(old).error_if()->access_reason(), access_error_reason::foreign_index);
    EXPECT_EQ(destination.at(*reused.value_if()).error_if()->access_reason(), access_error_reason::foreign_index);
    std::array<value_index, 16> indices{};
    indices[0] = old;
    for (std::size_t position = 1; position < indices.size(); ++position)
    {
      const auto appended = destination.append(detail::value_record{});
      ASSERT_TRUE(appended.has_value());
      indices[position] = *appended.value_if();
      EXPECT_EQ(destination.identity(), identity);
      for (std::size_t previous = 0; previous <= position; ++previous)
      {
        EXPECT_TRUE(destination.at(indices[previous]).has_value());
        EXPECT_EQ(indices[previous].position(), previous);
      }
    }
  }
  /* ================================= End Test_J5_039 ================================== */
  /* =================================== Test_J5_047 ==================================== */
  /*
   * Arrange: Exact and growing storage with budgets immediately below and at token-plus-record size.
   * Act: Allocate the first record through accounted storage.
   * Expect: The one-byte token consumes a block and budget; rejection rolls back and exact fit succeeds.
   */
  TEST(J5_047_StorageIdentity, TokenCountsAgainstOwnedByteBudget)
  {
    /* Arrange */
    for (const bool exact : {false, true})
    {
      for (const std::size_t limit : {std::size_t{0}, sizeof(detail::value_record), sizeof(detail::value_record) + 1U})
      {
        SCOPED_TRACE(exact);
        SCOPED_TRACE(limit);
        document_provider provider;
        accounted_storage accounting{provider.callbacks(), limit};
        {
          detail::value_storage storage{accounting.callbacks(), 1};
          /* Act */
          const auto reserved = exact ? storage.reserve_exact(1) : status_result::success();
          const auto added = (!reserved.has_value()) ? result<value_index>::failure(*reserved.error_if())
                                                     : storage.append(detail::value_record{});
          /* Expect */
          if (limit == sizeof(detail::value_record) + 1U)
          {
            ASSERT_TRUE(added.has_value());
            EXPECT_EQ(accounting.current_bytes(), limit);
            EXPECT_EQ(provider.m_allocations, 2U);
            EXPECT_NE(storage.identity(), nullptr);
          }
          else
          {
            ASSERT_FALSE(added.has_value());
            EXPECT_EQ(added.error_if()->code(), error_code::resource_limit);
            EXPECT_EQ(added.error_if()->limit_reason(), resource_limit_reason::owned_bytes);
            EXPECT_EQ(accounting.current_bytes(), 0U);
            EXPECT_EQ(storage.identity(), nullptr);
            EXPECT_EQ(provider.m_attempts, limit == 0U ? 0U : 1U);
          }
        }
        EXPECT_EQ(accounting.current_bytes(), 0U);
        EXPECT_EQ(provider.m_live_bytes, 0U);
        EXPECT_FALSE(accounting.faulted());
      }
    }
  }
  /* ================================= End Test_J5_047 ================================== */
} // namespace
