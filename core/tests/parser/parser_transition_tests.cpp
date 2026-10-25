#include "../../../tests/shared_infrastructure/document_provider.hpp"
#include "parser/parser_attachment.hpp"
#include "parser/parser_container_open.hpp"
#include "parser/parser_engine.hpp"
#include "parser/parser_work.hpp"
#include "parser/primitive_parser.hpp"

#include <gtest/gtest.h>

namespace
{
  using namespace adaptive_json5;
  using namespace adaptive_json5::detail;
  using adaptive_json5_test::document_provider;

  /* =================================== Test_J5_013 ==================================== */
  /*
   * Arrange: Supply every grammar state, both modes and representative next bytes.
   * Act: Ask the dispatcher for the next operation.
   * Expect: Separators, values and closers obey their state-specific grammar.
   */
  TEST(J5_013_ParserDispatch, SelectsActionsForEveryStateAndMode)
  {
    /* Arrange */
    const parser_state states[] = {parser_state::array_first_or_end,
                                   parser_state::array_value_after_comma,
                                   parser_state::array_comma_or_end,
                                   parser_state::object_first_name_or_end,
                                   parser_state::object_name_after_comma,
                                   parser_state::object_colon,
                                   parser_state::object_value,
                                   parser_state::object_comma_or_end};
    const syntax_mode modes[] = {syntax_mode::json5, syntax_mode::strict_json};
    const char bytes[] = {',', ':', ']', '}', '0', '"'};
    const std::optional<parser_action> no = std::nullopt;
    const std::optional<parser_action> value = parser_action::read_value;
    const std::optional<parser_action> name = parser_action::read_name;
    const std::optional<parser_action> comma = parser_action::consume_comma;
    const std::optional<parser_action> colon = parser_action::consume_colon;
    const std::optional<parser_action> close = parser_action::close_container;
    // Expected grammar data is independent of the dispatcher's decision tree.
    // Columns follow bytes above; rows follow states above. Only trailing-comma closes differ by mode.
    const std::optional<parser_action> expected[2][8][6] = {
      {
        {no, no, close, no, value, value},
        {no, no, close, no, value, value},
        {comma, no, close, no, no, no},
        {name, name, name, close, name, name},
        {name, name, name, close, name, name},
        {no, colon, no, no, no, no},
        {no, no, no, no, value, value},
        {comma, no, no, close, no, no},
      },
      {
        {no, no, close, no, value, value},
        {no, no, no, no, value, value},
        {comma, no, close, no, no, no},
        {name, name, name, close, name, name},
        {name, name, name, no, name, name},
        {no, colon, no, no, no, no},
        {no, no, no, no, value, value},
        {comma, no, no, close, no, no},
      },
    };
    /* Act */
    for (std::size_t state_index = 0; state_index < 8; ++state_index)
    {
      for (std::size_t mode_index = 0; mode_index < 2; ++mode_index)
      {
        for (std::size_t byte_index = 0; byte_index < 6; ++byte_index)
        {
          SCOPED_TRACE(static_cast<int>(states[state_index]));
          SCOPED_TRACE(bytes[byte_index]);
          SCOPED_TRACE(static_cast<int>(modes[mode_index]));
          parser_frame frame;
          frame.state = states[state_index];
          const auto selected = next_container_action(frame, bytes[byte_index], 17, modes[mode_index]);
          const auto& action = expected[mode_index][state_index][byte_index];
          /* Expect */
          ASSERT_EQ(selected.has_value(), action.has_value());
          if (action.has_value())
          {
            EXPECT_EQ(*selected.value_if(), *action);
          }
          else
          {
            EXPECT_EQ(selected.error_if()->code, error_code::syntax);
            EXPECT_TRUE(selected.error_if()->has_byte_offset);
            EXPECT_EQ(selected.error_if()->byte_offset, 17u);
          }
        }
      }
    }
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * Arrange: A frame with an out-of-domain state and a frame at end of input.
   * Act: Dispatch each frame without changing it.
   * Expect: Bad internal state is an access error; missing input is located syntax.
   */
  TEST(J5_013_ParserDispatch, DistinguishesInvalidStateFromMissingInput)
  {
    /* Arrange */
    parser_frame frame;
    frame.state = static_cast<parser_state>(99);
    /* Act */
    // A fixed-underlying enum can represent an unknown value; reject it explicitly.
    const auto invalid = next_container_action(frame, 'x', 9, syntax_mode::json5);
    const auto missing = next_container_action(frame, std::nullopt, 9, syntax_mode::json5);
    /* Expect */
    ASSERT_FALSE(invalid.has_value());
    EXPECT_EQ(invalid.error_if()->code, error_code::wrong_type_or_access);
    EXPECT_FALSE(invalid.error_if()->has_byte_offset);
    ASSERT_FALSE(missing.has_value());
    EXPECT_EQ(missing.error_if()->code, error_code::syntax);
    EXPECT_EQ(missing.error_if()->byte_offset, 9u);
  }
  /* ================================= End Test_J5_013 ================================== */

  /* =================================== Test_J5_004 ==================================== */
  /*
   * Arrange: Invalid source/input pairs with no destination records.
   * Act: Call primitive recognition with a
   * null buffer and an offset beyond input.
   * Expect: Access errors precede recognition work and document mutation.

   */
  TEST(J5_004_PrimitiveBoundary, RejectsInconsistentSourceMetadata)
  {
    /* Arrange */
    document_provider provider;
    document values{provider.callbacks(), 8};
    parse_options options;
    work_budget work{100};
    cursor missing{{nullptr, 1}};
    cursor beyond{{"12", 2}};
    beyond.advance(2);
    /* Act */
    // Validate before calculating token ranges against an inconsistent original-buffer description.
    const auto null_input = consume_primitive(values, missing, {nullptr, 1}, options, provider.callbacks(), work);
    const auto bad_offset = consume_primitive(values, beyond, {"1", 1}, options, provider.callbacks(), work);
    /* Expect */
    ASSERT_FALSE(null_input.has_value());
    ASSERT_FALSE(bad_offset.has_value());
    EXPECT_EQ(null_input.error_if()->code, error_code::wrong_type_or_access);
    EXPECT_EQ(bad_offset.error_if()->code, error_code::wrong_type_or_access);
    EXPECT_EQ(work.consumed(), 0u);
    EXPECT_EQ(values.size(), 0u);
    EXPECT_EQ(provider.m_attempts, 0u);
  }
  /* ================================= End Test_J5_004 ================================== */

  /* =================================== Test_J5_026 ==================================== */
  /*
   * Arrange: A default empty input view and otherwise valid parser configuration.
   * Act: Parse through both
   * internal root entry points.
   * Expect: Absence is located syntax, not malformed-buffer access, with no
   * allocations.
   */
  TEST(J5_026_EmptyRoot, NullEmptyViewReportsMissingValue)
  {
    /* Arrange */
    document_provider provider;
    parse_options options;
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    parser_engine engine{{}, options, accounting};
    /* Act */
    // nullptr is permitted for zero bytes, so root recognition must diagnose the missing value.
    const auto tree = engine.run();
    const auto primitive = parse_primitive_document({}, options, accounting);
    /* Expect */
    ASSERT_FALSE(tree.has_value());
    ASSERT_FALSE(primitive.has_value());
    EXPECT_EQ(tree.error_if()->code, error_code::syntax);
    EXPECT_EQ(primitive.error_if()->code, error_code::syntax);
    EXPECT_TRUE(tree.error_if()->has_byte_offset);
    EXPECT_TRUE(primitive.error_if()->has_byte_offset);
    EXPECT_EQ(tree.error_if()->byte_offset, 0u);
    EXPECT_EQ(primitive.error_if()->byte_offset, 0u);
    EXPECT_EQ(provider.m_attempts, 0u);
  }
  /* ================================= End Test_J5_026 ================================== */

  /* =================================== Test_J5_024 ==================================== */
  /*
   * Arrange: Borrowed numeric metadata without an input buffer and an unknown literal tag.
   * Act: Ask the
   * primitive constructor to append each malformed descriptor.
   * Expect: Access errors leave the document empty and
   * never invoke allocation.
   */
  TEST(J5_024_PrimitiveMetadata, RejectsMissingNumericInputAndUnknownLiteralTag)
  {
    /* Arrange */
    document_provider provider;
    document values{provider.callbacks(), 8};
    parse_options options;
    scalar_token number{number_token{number_kind::decimal, 0, 1}};
    scalar_token literal{literal_token{static_cast<literal_kind>(99), 0, 1}};
    /* Act */
    // A malformed internal descriptor must not fabricate a value or perform pointer arithmetic on nullptr.
    const auto no_input = append_primitive_token(values, std::move(number), {}, options, provider.callbacks());
    const auto unknown = append_primitive_token(values, std::move(literal), {"x", 1}, options, provider.callbacks());
    /* Expect */
    ASSERT_FALSE(no_input.has_value());
    ASSERT_FALSE(unknown.has_value());
    EXPECT_EQ(no_input.error_if()->code, error_code::wrong_type_or_access);
    EXPECT_EQ(unknown.error_if()->code, error_code::wrong_type_or_access);
    EXPECT_EQ(values.size(), 0u);
    EXPECT_EQ(provider.m_attempts, 0u);
  }
  /* ================================= End Test_J5_024 ================================== */

  /* =================================== Test_J5_048 ==================================== */
  /*
   * Arrange: An empty stack and a frame carrying a known source offset.
   * Act: Read through a const reference before push, after push and after pop.
   * Expect: Read-only inspection tracks the same frame lifetime as mutable inspection.
   */
  TEST(J5_048_ParserStack, ConstInspectionTracksFrameLifetime)
  {
    /* Arrange */
    document_provider provider;
    parser_stack frames{provider.callbacks(), 4};
    const parser_stack& view = frames;
    work_budget work{100};
    parser_frame frame;
    frame.opening_offset = 7;
    /* Act */
    // Const callers must observe absence rather than a pointer into unconstructed storage.
    EXPECT_EQ(view.top(), nullptr);
    ASSERT_TRUE(frames.push(std::move(frame), work).has_value());
    /* Expect */
    ASSERT_NE(view.top(), nullptr);
    EXPECT_EQ(view.top()->opening_offset, 7u);
    EXPECT_EQ(view.top(), frames.top());
    ASSERT_TRUE(frames.pop());
    EXPECT_EQ(view.top(), nullptr);
    EXPECT_FALSE(frames.pop());
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * Arrange: Container tokens with malformed metadata or exhausted work.
   * Act: Attempt to open each token into an empty document.
   * Expect: Rejection precedes document/frame mutation and allocation.
   */
  TEST(J5_048_ContainerOpen, RejectsInvalidTokensAndWorkBeforeMutation)
  {
    /* Arrange */
    document_provider provider;
    document values{provider.callbacks(), 8};
    parser_stack frames{provider.callbacks(), 4};
    work_budget work{0};
    /* Act */
    const auto length = open_container(values, frames, {punctuation_kind::left_bracket, 0, 2}, work);
    const auto kind = open_container(values, frames, {punctuation_kind::colon, 0, 1}, work);
    const auto exhausted = open_container(values, frames, {punctuation_kind::left_bracket, 0, 1}, work);
    /* Expect */
    // Caller-supplied token descriptors must be validated before reserving a provisional frame.
    ASSERT_FALSE(length.has_value());
    ASSERT_FALSE(kind.has_value());
    ASSERT_FALSE(exhausted.has_value());
    EXPECT_EQ(length.error_if()->code, error_code::wrong_type_or_access);
    EXPECT_EQ(kind.error_if()->code, error_code::wrong_type_or_access);
    EXPECT_EQ(exhausted.error_if()->code, error_code::resource_limit);
    EXPECT_EQ(values.size(), 0u);
    EXPECT_EQ(frames.size(), 0u);
    EXPECT_EQ(provider.m_attempts, 0u);
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * Arrange: Array/object frames in phases that cannot accept a completed child.
   * Act: Attach a detached null through each invalid frame.
   * Expect: The name and grammar phase remain intact and no work is charged.
   */
  TEST(J5_048_ParserAttachment, RejectsInconsistentPhasesBeforeTransfer)
  {
    /* Arrange */
    document_provider provider;
    document values{provider.callbacks(), 8};
    const auto parent = values.make_array();
    const auto child = values.make_null();
    ASSERT_TRUE(parent.has_value());
    ASSERT_TRUE(child.has_value());
    /* Act */
    for (const auto state : {parser_state::array_first_or_end, parser_state::array_value_after_comma,
                             parser_state::object_value, parser_state::object_colon})
    {
      parser_frame frame;
      frame.container = *parent.value_if();
      frame.state = state;
      if (state != parser_state::object_value)
        frame.pending_name.emplace();
      work_budget work{100};
      const auto attached = attach_parsed_value(values, frame, *child.value_if(), work, 0);
      /* Expect */
      // A pending object name must never be silently discarded by array attachment.
      ASSERT_FALSE(attached.has_value());
      EXPECT_EQ(attached.error_if()->code, error_code::wrong_type_or_access);
      EXPECT_EQ(frame.state, state);
      EXPECT_EQ(frame.pending_name.has_value(), state != parser_state::object_value);
      EXPECT_EQ(work.consumed(), 0u);
    }
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * Arrange: A valid parent frame and an invalid child index.
   * Act: Attempt attachment after admitting its traversal work.
   * Expect: The document error propagates without publishing a grammar transition.
   */
  TEST(J5_048_ParserAttachment, PreservesPhaseWhenDocumentRejectsChild)
  {
    /* Arrange */
    document_provider provider;
    document values{provider.callbacks(), 8};
    const auto parent = values.make_array();
    ASSERT_TRUE(parent.has_value());
    parser_frame frame;
    frame.container = *parent.value_if();
    work_budget work{100};
    /* Act */
    // Work admission can succeed while relationship validation still fails.
    const auto attached = attach_parsed_value(values, frame, value_index{99}, work, 0);
    /* Expect */
    ASSERT_FALSE(attached.has_value());
    EXPECT_EQ(attached.error_if()->code, error_code::wrong_type_or_access);
    EXPECT_EQ(frame.state, parser_state::array_first_or_end);
    EXPECT_EQ(work.consumed(), c_attachment_work);
  }
  /* ================================= End Test_J5_048 ================================== */
} // namespace
