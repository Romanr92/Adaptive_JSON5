#include "../../../tests/shared_infrastructure/document_provider.hpp"
#include "adaptive_json5/value/document.hpp"
#include "adaptive_json5/value/value_iterator.hpp"
#include "adaptive_json5/value/value_view.hpp"
#include "value/record_link.hpp"
#include "value/value_record.hpp"

#include <cstddef>
#include <gtest/gtest.h>
#include <optional>
#include <utility>
#include <variant>

namespace
{
  using namespace adaptive_json5;
  using namespace adaptive_json5::detail;
  using adaptive_json5_test::document_provider;

  /* ==================================== Fixtures ===================================== */

  /**
   * @brief Own records for one array with a chosen number of null elements.
   * @par Requirements
   * J5-030; J5-048.
   * @note Record 0 is the array; records 1..count are its elements, linked in order. The
   * records are built directly so a unit can damage individual elements afterwards.
   */
  struct array_records
  {
      document_provider m_provider;
      value_storage m_values;

      /**
       * @brief Append the array record and its count element records.
       * @par Requirements
       * J5-030.
       * @param count Number of elements.
       */
      explicit array_records(std::size_t count) : m_values{m_provider.callbacks(), count + 1U}
      {
        if (!m_values.reserve_exact(count + 1U).has_value())
        {
          return;
        }
        value_record array;
        array.payload.emplace<array_data>();
        if (count != 0U)
        {
          std::get<array_data>(array.payload).elements = {record_link{1U}, record_link{count}, count};
        }
        static_cast<void>(m_values.append(std::move(array)));
        for (std::size_t position = 1U; position <= count; ++position)
        {
          value_record element;
          element.parent = record_link{0U};
          // Only the last element ends the sibling chain.
          if (position < count)
          {
            element.next_sibling = record_link{position + 1U};
          }
          static_cast<void>(m_values.append(std::move(element)));
        }
      }

      /**
       * @brief Remove the parent link of one record, so any later visit of it fails.
       * @par Requirements
       * J5-039.
       * @param position Record position (1-based element position).
       */
      void damage(std::size_t position)
      {
        const auto record = m_values.at(m_values.issue(position));
        ASSERT_TRUE(record.has_value());
        (*record.value_if())->parent = {};
      }
  };
  /* ================================== End Fixtures =================================== */
  /* =================================== Test_J5_048 =================================== */

  class J5_048_ChildIteration : public ::testing::TestWithParam<std::size_t>
  {
  };

  /* Verify children() reads every element record exactly once (linear work, issue #154).
   * Arrange: An array of n elements built from internal records.
   * Act: Walk it with value_view::children(); right after each element is returned,
   *      damage that element's record so any later read of it fails with internal_error.
   * Expect: The walk still returns all n elements in order and then ends, so no record was
   *         read twice: the walk does at most n record reads, never the n(n+1)/2 of an
   *         at(i) loop. A restarted lookup, element_at(0), does hit the damage.
   */
  TEST_P(J5_048_ChildIteration, ReadsEachElementRecordOnce)
  {
    /* Arrange */
    const std::size_t count = GetParam();
    array_records records{count};
    ASSERT_EQ(records.m_values.size(), count + 1U);
    const value_view array{records.m_values, records.m_values.issue(0U)};
    auto iterator = array.children();
    ASSERT_TRUE(iterator.has_value());
    std::size_t visited{};
    bool ended = false;
    /* Act */
    // count + 1 calls: one per element and the one that reports the end.
    for (std::size_t call = 0U; call <= count; ++call)
    {
      const auto step = iterator.value_if()->next();
      ASSERT_TRUE(step.has_value()) << "call " << call;
      if (!step.value_if()->has_value())
      {
        ended = true;
        break;
      }
      const auto index = (*step.value_if())->index();
      ASSERT_TRUE(index.has_value());
      EXPECT_EQ(index.value_if()->position(), visited + 1U);
      records.damage(visited + 1U);
      ++visited;
    }
    /* Expect */
    EXPECT_TRUE(ended);
    EXPECT_EQ(visited, count);
    if (count != 0U)
    {
      const auto restarted = array.element_at(0U);
      ASSERT_FALSE(restarted.has_value());
      EXPECT_EQ(restarted.error_if()->code(), error_code::internal_error);
    }
  }

  INSTANTIATE_TEST_SUITE_P(Sizes, J5_048_ChildIteration, ::testing::Values(0U, 1U, 1023U));

  /* ----------------------------------------------------------------------------------- */

  /* Verify value_view::children() rejects views it cannot iterate.
   * Arrange: A default-constructed view and a view of a null scalar record.
   * Act: Call children() on each.
   * Expect: wrong_type_or_access for both: invalid_argument for the default view and
   *         wrong_type for the scalar.
   */
  TEST(J5_048_ChildIteration, RejectsDefaultViewAndScalar)
  {
    /* Arrange */
    document_provider provider;
    value_storage values{provider.callbacks(), 1U};
    ASSERT_TRUE(values.reserve_exact(1U).has_value());
    ASSERT_TRUE(values.append(value_record{}).has_value());
    const value_view empty{};
    const value_view scalar{values, values.issue(0U)};
    /* Act */
    const auto from_empty = empty.children();
    const auto from_scalar = scalar.children();
    /* Expect */
    ASSERT_FALSE(from_empty.has_value());
    EXPECT_EQ(from_empty.error_if()->code(), error_code::wrong_type_or_access);
    EXPECT_EQ(from_empty.error_if()->access_reason(), access_error_reason::invalid_argument);
    ASSERT_FALSE(from_scalar.has_value());
    EXPECT_EQ(from_scalar.error_if()->code(), error_code::wrong_type_or_access);
    EXPECT_EQ(from_scalar.error_if()->access_reason(), access_error_reason::wrong_type);
  }
  /* ================================= End Test_J5_048 ================================= */
} // namespace
