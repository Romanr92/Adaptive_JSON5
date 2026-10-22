#include "../../../tests/support/document_provider.hpp"
#include "adaptive_json5/detail/parser/parser_container_close.hpp"

#include <gtest/gtest.h>

namespace
{
  using namespace adaptive_json5;
  using namespace adaptive_json5::detail;
  using adaptive_json5_test::document_provider;

  /** @brief Identify independent preconditions checked before container publication. */
  enum class close_fault
  {
    empty_stack,
    wrong_length,
    wrong_kind,
    outside_input,
    reversed_span,
    awaiting_colon,
    awaiting_value,
    strict_array_comma,
    strict_object_comma,
    pending_name,
    exhausted_work,
    invalid_container
  };
  /** @brief Share transactional assertions across container-close preconditions. */
  class J5_048_ContainerCloseBoundary : public testing::TestWithParam<close_fault>
  {
  };

  /* =================================== Test_J5_048 ==================================== */
  /*
   * Arrange: A valid close operation with exactly one violated precondition.
   * Act: Close the container through the internal component interface.
   * Expect: The frame is retained and the document gains no completed span.
   */
  TEST_P(J5_048_ContainerCloseBoundary, RetainsFrameOnRejectedCompletion)
  {
    /* Arrange */
    document_provider provider;
    document values{provider.callbacks(), 8};
    const auto container = values.make_array();
    ASSERT_TRUE(container.has_value());
    parser_stack frames{provider.callbacks(), 2};
    parser_frame frame;
    frame.container = *container.value_if();
    punctuation_token closing{punctuation_kind::right_bracket, 1, 1};
    std::size_t input_size = 2;
    syntax_mode mode = syntax_mode::json5;
    work_budget setup{100};
    const auto fault = GetParam();
    // Isolate each condition so an earlier guard cannot mask the intended failure.
    switch (fault)
    {
    case close_fault::wrong_length:
      closing.byte_length = 2;
      break;
    case close_fault::wrong_kind:
      closing.kind = punctuation_kind::colon;
      break;
    case close_fault::outside_input:
      input_size = 1;
      break;
    case close_fault::reversed_span:
      frame.opening_offset = 1;
      break;
    case close_fault::awaiting_colon:
      frame.state = parser_state::object_colon;
      break;
    case close_fault::awaiting_value:
      frame.state = parser_state::object_value;
      break;
    case close_fault::strict_array_comma:
      frame.state = parser_state::array_value_after_comma;
      mode = syntax_mode::strict_json;
      break;
    case close_fault::strict_object_comma:
      frame.state = parser_state::object_name_after_comma;
      closing.kind = punctuation_kind::right_brace;
      mode = syntax_mode::strict_json;
      break;
    case close_fault::pending_name:
      frame.pending_name.emplace();
      break;
    case close_fault::invalid_container:
      frame.container = value_index{99};
      break;
    default:
      break;
    }
    if (fault != close_fault::empty_stack)
    {
      ASSERT_TRUE(frames.push(std::move(frame), setup).has_value());
    }
    const auto original_size = frames.size();
    work_budget work{fault == close_fault::exhausted_work ? 0u : 100u};
    /* Act */
    const auto closed = close_container(values, frames, closing, mode, input_size, work);
    /* Expect */
    ASSERT_FALSE(closed.has_value());
    const bool syntax = fault == close_fault::awaiting_colon || fault == close_fault::awaiting_value ||
                        fault == close_fault::strict_array_comma || fault == close_fault::strict_object_comma;
    EXPECT_EQ(closed.error_if()->code, syntax                                 ? error_code::syntax
                                       : fault == close_fault::exhausted_work ? error_code::resource_limit
                                                                              : error_code::wrong_type_or_access);
    EXPECT_EQ(frames.size(), original_size);
    const auto view = values.view(*container.value_if());
    ASSERT_TRUE(view.has_value());
    const auto span = view.value_if()->source_span();
    ASSERT_TRUE(span.has_value());
    EXPECT_FALSE(span.value_if()->has_value());
  }
  INSTANTIATE_TEST_SUITE_P(
    Preconditions, J5_048_ContainerCloseBoundary,
    testing::Values(close_fault::empty_stack, close_fault::wrong_length, close_fault::wrong_kind,
                    close_fault::outside_input, close_fault::reversed_span, close_fault::awaiting_colon,
                    close_fault::awaiting_value, close_fault::strict_array_comma, close_fault::strict_object_comma,
                    close_fault::pending_name, close_fault::exhausted_work, close_fault::invalid_container),
    (
      [](const testing::TestParamInfo<close_fault>& info)
      {
        const char* names[] = {"EmptyStack",        "WrongLength",   "WrongKind",     "OutsideInput",
                               "ReversedSpan",      "AwaitingColon", "AwaitingValue", "StrictArrayComma",
                               "StrictObjectComma", "PendingName",   "ExhaustedWork", "InvalidContainer"};
        return names[static_cast<std::size_t>(info.param)];
      }));
  /* ================================= End Test_J5_048 ================================== */
} // namespace
