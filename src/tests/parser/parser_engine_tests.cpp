// SPDX-License-Identifier: Apache-2.0
#include "adaptive_json5/detail/parser/parser_engine.hpp"

#include <array>
#include <cstddef>
#include <cstdlib>
#include <gtest/gtest.h>
#include <ostream>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

namespace
{
  using namespace adaptive_json5;
  using namespace adaptive_json5::detail;

  /* ===================================== Fixtures ==================================== */
  /**
   * @brief Track host allocations and inject one selected request failure.
   * @par Requirements
   * P033; J5-040; J5-046; J5-048; J5-051.
   * @note Only the unit provider uses malloc; parser storage remains caller supplied.
   */
  class engine_provider
  {
    private:
      struct allocation_record
      {
          void* address{};
          std::size_t bytes{};
          std::size_t alignment{};
      };
      std::array<allocation_record, 256> m_records{};

      /**
       * @brief Return aligned host storage or the selected injected failure.
       * @par Requirements
       * P033; J5-046.
       */
      static void* allocate(void* context, std::size_t bytes, std::size_t alignment) noexcept
      {
        auto& self = *static_cast<engine_provider*>(context);
        ++self.m_attempts;
        if ((self.m_attempts == self.m_fail_on) || (alignment > alignof(std::max_align_t)))
        {
          return nullptr;
        }
        for (auto& record : self.m_records)
        {
          if (record.address == nullptr)
          {
            record = {std::malloc(bytes), bytes, alignment};
            if (record.address != nullptr)
            {
              ++self.m_allocations;
              self.m_live_bytes += bytes;
            }
            return record.address;
          }
        }
        return nullptr;
      }

      /**
       * @brief Validate the original allocation description before release.
       * @par Requirements
       * P033; J5-040; J5-051.
       */
      static void release(void* context, void* address, std::size_t bytes, std::size_t alignment) noexcept
      {
        auto& self = *static_cast<engine_provider*>(context);
        for (auto& record : self.m_records)
        {
          if ((record.address == address) && (address != nullptr))
          {
            EXPECT_EQ(record.bytes, bytes);
            EXPECT_EQ(record.alignment, alignment);
            self.m_live_bytes -= record.bytes;
            ++self.m_releases;
            std::free(address);
            record = {};
            return;
          }
        }
        ADD_FAILURE() << "Unknown allocation or duplicate release";
      }

    public:
      std::size_t m_attempts{};
      std::size_t m_fail_on{};
      std::size_t m_allocations{};
      std::size_t m_releases{};
      std::size_t m_live_bytes{};

      /**
       * @brief Check ownership balance and reclaim any reported test leak.
       * @par Requirements
       * P033; J5-040; J5-051.
       */
      ~engine_provider()
      {
        EXPECT_EQ(m_live_bytes, 0u);
        EXPECT_EQ(m_allocations, m_releases);
        for (const auto& record : m_records)
        {
          std::free(record.address);
        }
      }

      /**
       * @brief Borrow test storage through the real public callbacks.
       * @par Requirements
       * P033; J5-046.
       */
      storage_callbacks callbacks() noexcept
      {
        return {this, &allocate, &release};
      }
  };

  /**
   * @brief Compare exact decoded units, including embedded zero and surrogates.
   * @par Requirements
   * P033; J5-029; J5-040.
   */
  void expect_units(const decoded_string& actual, const std::u16string& expected)
  {
    ASSERT_EQ(actual.size(), expected.size());
    for (std::size_t index{}; index < expected.size(); ++index)
    {
      const auto unit = actual.unit_at(index);
      ASSERT_TRUE(unit.has_value());
      EXPECT_EQ(*unit.value_if(), expected[index]);
    }
  }
  /* =================================== End Fixtures ================================== */

  /* =================================== Test_J5_004 =================================== */
  /*
   * Reject invalid buffer/configuration descriptions before touching input or storage.
   * Arrange: Null nonempty input, or an accounted ceiling inconsistent with options.
   * Act: Run each engine with an otherwise valid configuration.
   * Expect: Explicit access/configuration failure with no provider requests.
   */
  TEST(J5_004_ContainerInput, RejectsInvalidBufferAndAccountingBeforeAllocation)
  {
    /* Arrange */
    for (const bool invalid_buffer : {true, false})
    {
      engine_provider provider;
      parse_options options;
      accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes - (invalid_buffer ? 0 : 1)};
      const input_view input = invalid_buffer ? input_view{nullptr, 1} : input_view{"[]", 2};
      parser_engine engine{input, options, accounting};
      /* Act */
      const auto parsed = engine.run();
      /* Expect */
      ASSERT_FALSE(parsed.has_value());
      EXPECT_EQ(parsed.error_if()->code, error_code::wrong_type_or_access);
      EXPECT_EQ(provider.m_attempts, 0u);
    }
  }
  /* ================================= End Test_J5_004 ================================= */

  /* =================================== Test_J5_013 =================================== */
  /**
   * @brief Specify empty and mutually nested container shapes independently.
   * @par Requirements
   * P033; J5-013; J5-027.
   */
  struct container_case
  {
      const char* input; // Input bytes, containing one root container.
      value_kind root;   // Expected root type.
      std::size_t nodes; // Expected total record count.
      std::size_t count; // Expected direct root children.
  };
  /**
   * @brief Print the source rather than opaque parameter object bytes.
   * @par Requirements
   * P033; J5-013.
   */
  void PrintTo(const container_case& value, std::ostream* output)
  {
    *output << value.input;
  }
  class J5_013_ContainerEngine : public testing::TestWithParam<std::tuple<container_case, syntax_mode>>
  {
  };
  /*
   * Parse empty and mixed nested containers in both modes.
   * Arrange: Independent container shape and selected syntax mode.
   * Act: Run a fresh engine through accounted caller storage.
   * Expect: Exact root kind, record count, child count, span and consumed prefix.
   */
  TEST_P(J5_013_ContainerEngine, ConstructsEmptyAndNestedContainers)
  {
    /* Arrange */
    const auto fixture = std::get<0>(GetParam());
    const std::string input = fixture.input;
    engine_provider provider;
    parse_options options{std::get<1>(GetParam())};
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    parser_engine engine{{input.data(), input.size()}, options, accounting};
    /* Act */
    const auto parsed = engine.run();
    /* Expect */
    ASSERT_TRUE(parsed.has_value());
    const auto& tree = *parsed.value_if();
    EXPECT_EQ(tree.consumed_bytes, input.size());
    EXPECT_EQ(tree.values.size(), fixture.nodes);
    const auto root = tree.values.root();
    ASSERT_TRUE(root.has_value());
    const auto kind = root.value_if()->kind();
    ASSERT_TRUE(kind.has_value());
    EXPECT_EQ(*kind.value_if(), fixture.root);
    const auto count = root.value_if()->container_size();
    ASSERT_TRUE(count.has_value());
    EXPECT_EQ(*count.value_if(), fixture.count);
    const auto span = root.value_if()->source_span();
    ASSERT_TRUE(span.has_value());
    ASSERT_TRUE(span.value_if()->has_value());
    EXPECT_EQ((**span.value_if()).byte_offset, 0u);
    EXPECT_EQ((**span.value_if()).byte_length, input.size());
  }
  INSTANTIATE_TEST_SUITE_P(P033, J5_013_ContainerEngine,
                           testing::Combine(testing::Values(container_case{"[]", value_kind::array, 1, 0},
                                                            container_case{"{}", value_kind::object, 1, 0},
                                                            container_case{"[{}]", value_kind::array, 2, 1},
                                                            container_case{"{\"a\":[]}", value_kind::object, 2, 1},
                                                            container_case{"[{\"a\":[{},[]]},{}]", value_kind::array, 6,
                                                                           2}),
                                            testing::Values(syntax_mode::json5, syntax_mode::strict_json)));

  /* ----------------------------------------------------------------------------------- */
  /**
   * @brief Specify malformed structure and its first offending byte.
   * @par Requirements
   * P033; J5-013; J5-043.
   */
  struct invalid_case
  {
      const char* input;  // Malformed input bytes.
      std::size_t offset; // Expected original byte location, including EOF.
  };
  /**
   * @brief Identify malformed source and expected location in parameter diagnostics.
   * @par Requirements
   * P033; J5-013.
   */
  void PrintTo(const invalid_case& value, std::ostream* output)
  {
    *output << value.input << " at byte " << value.offset;
  }
  class J5_013_ContainerFailure : public testing::TestWithParam<std::tuple<invalid_case, syntax_mode>>
  {
  };
  /*
   * Report missing and mismatched structure without publishing a partial tree.
   * Arrange: Malformed container or structural root and a syntax mode.
   * Act: Parse within an engine lifetime, then destroy that failed engine.
   * Expect: Located syntax error, no success payload and no outstanding bytes.
   */
  TEST_P(J5_013_ContainerFailure, ReportsLocatedSyntaxAndReleasesCandidate)
  {
    /* Arrange */
    const auto fixture = std::get<0>(GetParam());
    const std::string input = fixture.input;
    engine_provider provider;
    parse_options options{std::get<1>(GetParam())};
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    /* Act */
    {
      parser_engine engine{{input.data(), input.size()}, options, accounting};
      const auto parsed = engine.run();
      /* Expect */
      ASSERT_FALSE(parsed.has_value());
      EXPECT_EQ(parsed.value_if(), nullptr);
      ASSERT_NE(parsed.error_if(), nullptr);
      EXPECT_EQ(parsed.error_if()->code, error_code::syntax);
      EXPECT_TRUE(parsed.error_if()->has_byte_offset);
      EXPECT_EQ(parsed.error_if()->byte_offset, fixture.offset);
    }
    EXPECT_EQ(accounting.current_bytes(), 0u);
  }
  INSTANTIATE_TEST_SUITE_P(
    P033, J5_013_ContainerFailure,
    testing::Combine(testing::Values(invalid_case{"", 0}, invalid_case{" ", 1}, invalid_case{":", 0},
                                     invalid_case{",", 0}, invalid_case{"]", 0}, invalid_case{"}", 0},
                                     invalid_case{"[", 1}, invalid_case{"{", 1}, invalid_case{"[}", 1},
                                     invalid_case{"{]", 1}, invalid_case{"[1", 2}, invalid_case{"[1 2]", 3},
                                     invalid_case{"[1,,2]", 3}, invalid_case{"[,1]", 1}, invalid_case{"{\"a\"}", 4},
                                     invalid_case{"{\"a\":}", 5}, invalid_case{"{\"a\":1 \"b\":2}", 7},
                                     invalid_case{"[{\"a\":[1,2]}", 12}),
                     testing::Values(syntax_mode::json5, syntax_mode::strict_json)));

  /* ----------------------------------------------------------------------------------- */
  /*
   * Preserve internal prefix semantics until the P037 complete-input entry exists.
   * Arrange: A container root with leading trivia and an invalid unparsed suffix.
   * Act: Run the internal engine.
   * Expect: Success ends exactly after the root, with original root span.
   */
  TEST(J5_013_ContainerPrefix, StopsAfterRootWithoutValidatingSuffix)
  {
    /* Arrange */
    const std::string input = " /*a*/ [{}] @";
    engine_provider provider;
    parse_options options;
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    parser_engine engine{{input.data(), input.size()}, options, accounting};
    /* Act */
    const auto parsed = engine.run();
    /* Expect */
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed.value_if()->consumed_bytes, 11u);
    const auto root = parsed.value_if()->values.root();
    ASSERT_TRUE(root.has_value());
    const auto span = root.value_if()->source_span();
    ASSERT_TRUE(span.has_value());
    ASSERT_TRUE(span.value_if()->has_value());
    EXPECT_EQ((**span.value_if()).byte_offset, 7u);
    EXPECT_EQ((**span.value_if()).byte_length, 4u);
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * Reject every unfinished prefix of one nested root without retaining storage.
   * Arrange: A root whose only complete document prefix is its full length.
   * Act: Parse every shorter explicit input length using a fresh engine.
   * Expect: Located syntax failure and zero outstanding allocations after destruction.
   */
  TEST(J5_013_ContainerTruncation, RejectsEveryUnfinishedNestedPrefix)
  {
    /* Arrange */
    const std::string input = "[{\"a\":[true,{}]}]";
    engine_provider provider;
    parse_options options;
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    /* Act */
    for (std::size_t length{}; length < input.size(); ++length)
    {
      SCOPED_TRACE(length);
      {
        parser_engine engine{{input.data(), length}, options, accounting};
        const auto parsed = engine.run();
        /* Expect */
        ASSERT_FALSE(parsed.has_value());
        EXPECT_EQ(parsed.error_if()->code, error_code::syntax);
        EXPECT_TRUE(parsed.error_if()->has_byte_offset);
        EXPECT_LE(parsed.error_if()->byte_offset, length);
      }
      EXPECT_EQ(accounting.current_bytes(), 0u);
    }
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * Exercise mode-dependent names, comments and trailing closure through the engine.
   * Arrange: JSON5-only nested source, and the same source in strict mode.
   * Act: Parse each selected mode independently.
   * Expect: JSON5 success and located strict syntax rejection.
   */
  TEST(J5_013_ContainerModes, AppliesModeToNestedGrammar)
  {
    /* Arrange */
    const std::string input = "[{a:/*x*/[1,],},]";
    for (const auto mode : {syntax_mode::json5, syntax_mode::strict_json})
    {
      engine_provider provider;
      parse_options options{mode};
      accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
      parser_engine engine{{input.data(), input.size()}, options, accounting};
      /* Act */
      const auto parsed = engine.run();
      /* Expect */
      if (mode == syntax_mode::json5)
      {
        ASSERT_TRUE(parsed.has_value());
        EXPECT_EQ(parsed.value_if()->values.size(), 4u);
      }
      else
      {
        ASSERT_FALSE(parsed.has_value());
        EXPECT_EQ(parsed.error_if()->code, error_code::syntax);
        EXPECT_EQ(parsed.error_if()->byte_offset, 2u);
      }
    }
  }
  /* ================================= End Test_J5_013 ================================= */

  /* =================================== Test_J5_009 =================================== */
  /*
   * Preserve encoding failures inside nested lexical content.
   * Arrange: An array containing a string with a malformed UTF-8 leading byte.
   * Act: Run the engine over the explicit byte range.
   * Expect: Encoding error at the original byte and complete failed-engine cleanup.
   */
  TEST(J5_009_ContainerEncoding, ReportsOriginalNestedByteOffset)
  {
    /* Arrange */
    const std::string input = std::string("[\"") + static_cast<char>(0xC0) + "\"]";
    engine_provider provider;
    parse_options options;
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    /* Act */
    {
      parser_engine engine{{input.data(), input.size()}, options, accounting};
      const auto parsed = engine.run();
      /* Expect */
      ASSERT_FALSE(parsed.has_value());
      EXPECT_EQ(parsed.error_if()->code, error_code::encoding);
      EXPECT_TRUE(parsed.error_if()->has_byte_offset);
      EXPECT_EQ(parsed.error_if()->byte_offset, 2u);
    }
    EXPECT_EQ(accounting.current_bytes(), 0u);
  }
  /* ================================= End Test_J5_009 ================================= */

  /* =================================== Test_J5_017 =================================== */
  /*
   * Share a bounded first-warning descriptor across names and scalar strings.
   * Arrange: Raw U+2028 in a name and U+2029 in a nested value, with diagnostic limits 1/0.
   * Act: Parse each diagnostic allowance.
   * Expect: Two counted warnings but one retained descriptor, or explicit resource failure.
   */
  TEST(J5_017_ContainerWarnings, SharesFirstDescriptorAcrossNamesAndValues)
  {
    /* Arrange */
    const std::string input = "{'\xE2\x80\xA8':['\xE2\x80\xA9']}";
    for (const std::size_t diagnostics : {1u, 0u})
    {
      engine_provider provider;
      parse_options options;
      options.limits.max_diagnostics = diagnostics;
      accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
      parser_engine engine{{input.data(), input.size()}, options, accounting};
      /* Act */
      const auto parsed = engine.run();
      /* Expect */
      if (diagnostics == 0)
      {
        ASSERT_FALSE(parsed.has_value());
        EXPECT_EQ(parsed.error_if()->code, error_code::resource_limit);
      }
      else
      {
        ASSERT_TRUE(parsed.has_value());
        EXPECT_EQ(parsed.value_if()->warning_count, 2u);
        ASSERT_TRUE(parsed.value_if()->first_warning.has_value());
        EXPECT_EQ(parsed.value_if()->first_warning->code, string_warning_code::raw_line_separator);
        EXPECT_EQ(parsed.value_if()->first_warning->byte_offset, 2u);
        EXPECT_EQ(parsed.value_if()->first_warning->byte_length, 3u);
      }
    }
  }
  /* ================================= End Test_J5_017 ================================= */

  /* =================================== Test_J5_028 =================================== */
  static_assert(std::is_nothrow_move_constructible_v<parsed_value_tree>);
  static_assert(!std::is_copy_constructible_v<parsed_value_tree>);
  static_assert(noexcept(std::declval<parser_engine&>().run()));
  /*
   * Inspect heterogeneous nested values rather than acceptance alone.
   * Arrange: An array with every scalar family and an object containing an array.
   * Act: Parse and traverse public checked views.
   * Expect: Original order, exact string units, numeric spelling and nested content.
   */
  TEST(J5_028_ContainerValues, RetainsMixedContentAndChildSpans)
  {
    /* Arrange */
    const std::string input = "[null,true,\"x\\u0000\",16,{\"a\":[false,{}]}]";
    engine_provider provider;
    parse_options options;
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    parser_engine engine{{input.data(), input.size()}, options, accounting};
    /* Act */
    const auto parsed = engine.run();
    /* Expect */
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed.value_if()->values.size(), 9u);
    const auto root = parsed.value_if()->values.root();
    ASSERT_TRUE(root.has_value());
    const std::array<value_kind, 5> kinds{value_kind::null, value_kind::boolean, value_kind::string, value_kind::number,
                                          value_kind::object};
    for (std::size_t index{}; index < kinds.size(); ++index)
    {
      const auto child = root.value_if()->element_at(index);
      ASSERT_TRUE(child.has_value());
      const auto kind = child.value_if()->kind();
      ASSERT_TRUE(kind.has_value());
      EXPECT_EQ(*kind.value_if(), kinds[index]);
    }
    const auto boolean = root.value_if()->element_at(1);
    ASSERT_TRUE(boolean.has_value());
    const auto truth = boolean.value_if()->as_boolean();
    ASSERT_TRUE(truth.has_value());
    EXPECT_TRUE(*truth.value_if());
    const auto text = root.value_if()->element_at(2);
    ASSERT_TRUE(text.has_value());
    const auto string = text.value_if()->as_string();
    ASSERT_TRUE(string.has_value());
    expect_units(**string.value_if(), std::u16string{u'x', u'\0'});
    const auto number = root.value_if()->element_at(3);
    ASSERT_TRUE(number.has_value());
    const auto numeric = number.value_if()->as_number();
    ASSERT_TRUE(numeric.has_value());
    const auto spelling = (*numeric.value_if())->spelling();
    EXPECT_EQ(std::string(spelling.data, spelling.size), "16");
    const auto object = root.value_if()->element_at(4);
    ASSERT_TRUE(object.has_value());
    const auto member = object.value_if()->member_at(0);
    ASSERT_TRUE(member.has_value());
    const auto name = member.value_if()->member_name();
    ASSERT_TRUE(name.has_value());
    expect_units(**name.value_if(), u"a");
    const auto nested = member.value_if()->element_at(1);
    ASSERT_TRUE(nested.has_value());
    const auto nested_span = nested.value_if()->source_span();
    ASSERT_TRUE(nested_span.has_value());
    ASSERT_TRUE(nested_span.value_if()->has_value());
    EXPECT_EQ((**nested_span.value_if()).byte_offset, input.find("{}"));
    EXPECT_EQ((**nested_span.value_if()).byte_length, 2u);
  }
  /* ================================= End Test_J5_028 ================================= */

  /* =================================== Test_J5_040 =================================== */
  /*
   * Keep nested content independent of source and engine lifetimes.
   * Arrange: Source containing a decoded escaped name and lone surrogate value.
   * Act: Return the tree from a local engine, overwrite input, then move the tree.
   * Expect: Reacquired views retain exact units and storage releases after owners die.
   */
  TEST(J5_040_ContainerOwnership, SurvivesInputOverwriteEngineDestructionAndMove)
  {
    /* Arrange */
    engine_provider provider;
    parse_options options;
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    std::string input = "[{\\u0061:'\\uD800'}]";
    /* Act */
    {
      auto parsed = [&]()
      {
        parser_engine engine{{input.data(), input.size()}, options, accounting};
        return engine.run();
      }();
      ASSERT_TRUE(parsed.has_value());
      input.assign(input.size(), '?');
      parsed_value_tree moved{std::move(*parsed.value_if())};
      /* Expect */
      const auto root = moved.values.root();
      ASSERT_TRUE(root.has_value());
      const auto object = root.value_if()->element_at(0);
      ASSERT_TRUE(object.has_value());
      const auto member = object.value_if()->member_at(0);
      ASSERT_TRUE(member.has_value());
      const auto name = member.value_if()->member_name();
      ASSERT_TRUE(name.has_value());
      expect_units(**name.value_if(), u"a");
      const auto string = member.value_if()->as_string();
      ASSERT_TRUE(string.has_value());
      expect_units(**string.value_if(), std::u16string{static_cast<char16_t>(0xD800)});
      EXPECT_GT(accounting.current_bytes(), 0u);
    }
    EXPECT_EQ(accounting.current_bytes(), 0u);
  }
  /* ================================= End Test_J5_040 ================================= */

  /* =================================== Test_J5_048 =================================== */
  /**
   * @brief Pair one independently chosen limit with an admitted and rejected value.
   * @par Requirements
   * P033; J5-047; J5-048.
   */
  struct limit_case
  {
      const char* name;                    // Diagnostic label.
      const char* input;                   // Input exercising the selected limit.
      std::size_t resource_limits::*field; // Input option member to configure.
      std::size_t admitted;                // Inclusive passing limit.
      std::size_t rejected;                // Failing limit.
  };
  /**
   * @brief Print the independent limit fixture label.
   * @par Requirements
   * P033; J5-047.
   */
  void PrintTo(const limit_case& value, std::ostream* output)
  {
    *output << value.name;
  }
  class J5_048_ContainerLimits : public testing::TestWithParam<limit_case>
  {
  };
  /*
   * Enforce shared resource limits across mixed containers and owned names.
   * Arrange: One boundary fixture with independently specified pass/fail limits.
   * Act: Run fresh engines at the boundary and below it.
   * Expect: Success at the inclusive limit and recoverable resource failure below.
   */
  TEST_P(J5_048_ContainerLimits, EnforcesInclusiveSharedLimits)
  {
    /* Arrange */
    const auto fixture = GetParam();
    SCOPED_TRACE(fixture.name);
    const std::string input = fixture.input;
    for (const bool accepted : {true, false})
    {
      engine_provider provider;
      parse_options options;
      options.limits.*(fixture.field) = accepted ? fixture.admitted : fixture.rejected;
      accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
      /* Act */
      {
        parser_engine engine{{input.data(), input.size()}, options, accounting};
        const auto parsed = engine.run();
        /* Expect */
        ASSERT_EQ(parsed.has_value(), accepted);
        if (!accepted)
        {
          ASSERT_NE(parsed.error_if(), nullptr);
          EXPECT_EQ(parsed.error_if()->code, error_code::resource_limit);
        }
      }
      EXPECT_EQ(accounting.current_bytes(), 0u);
    }
  }
  INSTANTIATE_TEST_SUITE_P(
    P033, J5_048_ContainerLimits,
    testing::Values(limit_case{"depth", "[{\"a\":[]}]", &resource_limits::max_depth, 3, 2},
                    limit_case{"nodes", "[{},[]]", &resource_limits::max_nodes, 3, 2},
                    limit_case{"members", "{\"a\":{\"b\":0},\"c\":[]}", &resource_limits::max_members, 3, 2},
                    limit_case{"name_units", "{\"ab\":[]}", &resource_limits::max_string_units, 2, 1},
                    limit_case{"string_units", "[\"ab\"]", &resource_limits::max_string_units, 2, 1},
                    limit_case{"aggregate", "{\"a\":[\"bc\",{\"d\":\"e\"}]}", &resource_limits::max_total_string_units,
                               5, 4},
                    limit_case{"number_bytes", "[123]", &resource_limits::max_number_bytes, 3, 2},
                    limit_case{"exponent", "[1e01]", &resource_limits::max_exponent_digits, 2, 1},
                    limit_case{"input_bytes", "[{}]", &resource_limits::max_input_bytes, 4, 3},
                    limit_case{"zero_nodes", "[]", &resource_limits::max_nodes, 1, 0},
                    limit_case{"zero_depth", "[]", &resource_limits::max_depth, 1, 0}));

  /* ----------------------------------------------------------------------------------- */
  /*
   * Check depth before descent and clean deeply unfinished mixed nesting.
   * Arrange: Alternating arrays/objects at depth 64 and one level beyond it.
   * Act: Parse exact depth, excessive depth, and an unterminated deep prefix.
   * Expect: Success, resource failure and syntax failure with balanced storage.
   */
  TEST(J5_048_ContainerDepth, HandlesBoundaryAndDeepFailureWithoutRecursiveCleanup)
  {
    /* Arrange */
    for (const std::size_t depth : {64u, 65u})
    {
      std::string input;
      std::string suffix;
      for (std::size_t level{}; level < depth; ++level)
      {
        input += level % 2 == 0 ? "[" : "{\"a\":";
        suffix.insert(suffix.begin(), level % 2 == 0 ? ']' : '}');
      }
      input += "null";
      for (const bool complete : {true, false})
      {
        engine_provider provider;
        parse_options options;
        options.limits.max_work_units = 1000000;
        options.limits.max_owned_bytes = 262144;
        accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
        const std::string source = complete ? input + suffix : input;
        /* Act */
        {
          parser_engine engine{{source.data(), source.size()}, options, accounting};
          const auto parsed = engine.run();
          /* Expect */
          if ((depth == 64) && complete)
          {
            ASSERT_TRUE(parsed.has_value());
            EXPECT_EQ(parsed.value_if()->values.size(), 65u);
          }
          else
          {
            ASSERT_FALSE(parsed.has_value());
            EXPECT_EQ(parsed.error_if()->code, depth == 65 ? error_code::resource_limit : error_code::syntax);
          }
        }
        EXPECT_EQ(accounting.current_bytes(), 0u);
      }
    }
  }

  /* ----------------------------------------------------------------------------------- */
  /*
   * Inject failure at every allocation request of a successful mixed-tree parse.
   * Arrange: A baseline tree requiring frame/record growth, names and scalar storage.
   * Act: Fail each request separately and then run a fresh engine on the same adapter.
   * Expect: Explicit allocation failure, complete cleanup and successful fresh reuse.
   */
  TEST(J5_048_ContainerAllocation, ReleasesEveryFailedAllocationAndAllowsFreshEngine)
  {
    /* Arrange */
    const std::string input = "[{alpha:[\"text\",123,{beta:[]}],gamma:{delta:'x'}},[]]";
    std::size_t attempts{};
    {
      engine_provider provider;
      parse_options options;
      accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
      parser_engine engine{{input.data(), input.size()}, options, accounting};
      const auto parsed = engine.run();
      ASSERT_TRUE(parsed.has_value());
      attempts = provider.m_attempts;
    }
    ASSERT_GT(attempts, 0u);
    for (std::size_t fail_on = 1; fail_on <= attempts; ++fail_on)
    {
      SCOPED_TRACE(fail_on);
      engine_provider provider;
      provider.m_fail_on = fail_on;
      parse_options options;
      accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
      /* Act */
      {
        parser_engine engine{{input.data(), input.size()}, options, accounting};
        const auto parsed = engine.run();
        /* Expect */
        ASSERT_FALSE(parsed.has_value());
        EXPECT_EQ(parsed.error_if()->code, error_code::allocation_failure);
      }
      EXPECT_EQ(accounting.current_bytes(), 0u);
      provider.m_fail_on = 0;
      {
        parser_engine fresh{{input.data(), input.size()}, options, accounting};
        const auto parsed = fresh.run();
        ASSERT_TRUE(parsed.has_value());
      }
      EXPECT_EQ(accounting.current_bytes(), 0u);
    }
  }

  /* ----------------------------------------------------------------------------------- */
  /*
   * Exhaust work at every charge boundary of a representative nested parse.
   * Arrange: Measure the first successful finite allowance, retaining all smaller failures.
   * Act: Run fresh engines with increasing work limits.
   * Expect: Only explicit resource failures before success, and cleanup at each boundary.
   */
  TEST(J5_048_ContainerWork, ExhaustsRecoverablyAcrossAllChargedPhases)
  {
    /* Arrange */
    const std::string input = "[{a:[1]}]";
    bool succeeded = false;
    /* Act */
    for (std::size_t allowance{}; allowance < 1024; ++allowance)
    {
      engine_provider provider;
      parse_options options;
      options.limits.max_work_units = allowance;
      accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
      {
        parser_engine engine{{input.data(), input.size()}, options, accounting};
        const auto parsed = engine.run();
        /* Expect */
        if (parsed.has_value())
        {
          EXPECT_EQ(parsed.value_if()->consumed_bytes, input.size());
          succeeded = true;
        }
        else
        {
          EXPECT_EQ(parsed.error_if()->code, error_code::resource_limit);
        }
      }
      EXPECT_EQ(accounting.current_bytes(), 0u);
      if (succeeded)
      {
        break;
      }
    }
    EXPECT_TRUE(succeeded);
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * Admit empty decoded content even when no string units may be retained.
   * Arrange: Empty quoted name and string alongside containers with zero unit limits.
   * Act: Parse the nested tree.
   * Expect: Exact empty owned content without treating zero as an unlimited sentinel.
   */
  TEST(J5_048_ContainerZeroUnits, AcceptsEmptyNamesAndValues)
  {
    /* Arrange */
    const std::string input = "{\"\":[\"\",{}]}";
    engine_provider provider;
    parse_options options;
    options.limits.max_string_units = 0;
    options.limits.max_total_string_units = 0;
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    parser_engine engine{{input.data(), input.size()}, options, accounting};
    /* Act */
    const auto parsed = engine.run();
    /* Expect */
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed.value_if()->values.size(), 4u);
    const auto root = parsed.value_if()->values.root();
    ASSERT_TRUE(root.has_value());
    const auto member = root.value_if()->member_at(0);
    ASSERT_TRUE(member.has_value());
    const auto name = member.value_if()->member_name();
    ASSERT_TRUE(name.has_value());
    expect_units(**name.value_if(), u"");
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * Keep scalar depth and temporary identifier storage separate from retained strings.
   * Arrange: Each nonstring scalar root with zero depth/member/decoded-unit allowance.
   * Act: Parse using a fresh engine for each root.
   * Expect: Success with one record, exact consumed prefix and no string warnings.
   */
  TEST(J5_048_ContainerScalarRoot, NeedsNoDepthMemberOrRetainedStringUnits)
  {
    /* Arrange */
    for (const std::string input : {"null", "true", "false", "123", "Infinity"})
    {
      SCOPED_TRACE(input);
      engine_provider provider;
      parse_options options;
      options.limits.max_depth = 0;
      options.limits.max_members = 0;
      options.limits.max_string_units = 0;
      options.limits.max_total_string_units = 0;
      accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
      parser_engine engine{{input.data(), input.size()}, options, accounting};
      /* Act */
      const auto parsed = engine.run();
      /* Expect */
      ASSERT_TRUE(parsed.has_value());
      EXPECT_EQ(parsed.value_if()->values.size(), 1u);
      EXPECT_EQ(parsed.value_if()->consumed_bytes, input.size());
      EXPECT_EQ(parsed.value_if()->warning_count, 0u);
      EXPECT_FALSE(parsed.value_if()->first_warning.has_value());
    }
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * Include frame and record capacity overlap in the shared byte ceiling.
   * Arrange: Baseline peak for mixed nesting, then limits immediately below/at/above it.
   * Act: Parse with each byte ceiling using fresh provider state.
   * Expect: Resource rejection below peak and success at or above, with complete cleanup.
   */
  TEST(J5_048_ContainerBytes, EnforcesOutstandingPeakIncludingGrowthOverlap)
  {
    /* Arrange */
    const std::string input = "[{a:[\"x\",123]}]";
    std::size_t peak{};
    {
      engine_provider provider;
      parse_options options;
      accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
      parser_engine engine{{input.data(), input.size()}, options, accounting};
      const auto parsed = engine.run();
      ASSERT_TRUE(parsed.has_value());
      peak = accounting.peak_bytes();
    }
    ASSERT_GT(peak, 0u);
    for (const std::size_t ceiling : {peak - 1, peak, peak + 1})
    {
      engine_provider provider;
      parse_options options;
      options.limits.max_owned_bytes = ceiling;
      accounted_storage accounting{provider.callbacks(), ceiling};
      /* Act */
      {
        parser_engine engine{{input.data(), input.size()}, options, accounting};
        const auto parsed = engine.run();
        /* Expect */
        ASSERT_EQ(parsed.has_value(), ceiling >= peak);
        if (!parsed.has_value())
        {
          EXPECT_EQ(parsed.error_if()->code, error_code::resource_limit);
        }
        EXPECT_LE(accounting.peak_bytes(), ceiling);
      }
      EXPECT_EQ(accounting.current_bytes(), 0u);
    }
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * Preserve live pending names when frame storage grows or a later push fails.
   * Arrange: Three frames with owned names and a three-frame ceiling.
   * Act: Grow the stack, attempt one excessive push, then pop every frame.
   * Expect: Names/order survive relocation, failed input remains owned, cleanup balances.
   */
  TEST(J5_048_ParserStack, PreservesOwnedNamesAcrossGrowthAndLimitFailure)
  {
    /* Arrange */
    engine_provider provider;
    accounted_storage accounting{provider.callbacks(), 32768};
    work_budget work{100};
    parser_stack frames{accounting.callbacks(), 3};
    /* Act */
    for (std::size_t index{}; index < 3; ++index)
    {
      const char16_t unit = static_cast<char16_t>(u'a' + index);
      auto name = decoded_string::copy_units(accounting.callbacks(), &unit, 1);
      ASSERT_TRUE(name.has_value());
      parser_frame frame{};
      frame.opening_offset = index;
      frame.pending_name.emplace(std::move(*name.value_if()));
      const auto pushed = frames.push(std::move(frame), work);
      ASSERT_TRUE(pushed.has_value());
    }
    parser_frame excess{};
    const auto rejected = frames.push(std::move(excess), work);
    /* Expect */
    ASSERT_FALSE(rejected.has_value());
    EXPECT_EQ(rejected.error_if()->code, error_code::resource_limit);
    EXPECT_EQ(frames.size(), 3u);
    for (std::size_t remaining = 3; remaining != 0; --remaining)
    {
      ASSERT_NE(frames.top(), nullptr);
      EXPECT_EQ(frames.top()->opening_offset, remaining - 1);
      ASSERT_TRUE(frames.top()->pending_name.has_value());
      expect_units(*frames.top()->pending_name, std::u16string{static_cast<char16_t>(u'a' + remaining - 1)});
      EXPECT_TRUE(frames.pop());
    }
    EXPECT_EQ(frames.top(), nullptr);
    EXPECT_FALSE(frames.pop());
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * Preserve existing frames and external input when growth allocation fails.
   * Arrange: One live frame and a second external frame with a pending name.
   * Act: Inject the next allocation failure during a capacity-growing push.
   * Expect: Existing stack and input ownership unchanged, with allocation_failure.
   */
  TEST(J5_048_ParserStack, PreservesFramesAndInputOnGrowthAllocationFailure)
  {
    /* Arrange */
    engine_provider provider;
    accounted_storage accounting{provider.callbacks(), 32768};
    work_budget work{100};
    parser_stack frames{accounting.callbacks(), 4};
    parser_frame first{};
    first.opening_offset = 7;
    ASSERT_TRUE(frames.push(std::move(first), work).has_value());
    const char16_t unit = u'x';
    auto name = decoded_string::copy_units(accounting.callbacks(), &unit, 1);
    ASSERT_TRUE(name.has_value());
    parser_frame second{};
    second.pending_name.emplace(std::move(*name.value_if()));
    provider.m_fail_on = provider.m_attempts + 1;
    /* Act */
    const auto pushed = frames.push(std::move(second), work);
    /* Expect */
    ASSERT_FALSE(pushed.has_value());
    EXPECT_EQ(pushed.error_if()->code, error_code::allocation_failure);
    EXPECT_EQ(frames.size(), 1u);
    ASSERT_NE(frames.top(), nullptr);
    EXPECT_EQ(frames.top()->opening_offset, 7u);
    ASSERT_TRUE(second.pending_name.has_value());
    expect_units(*second.pending_name, u"x");
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * Roll back the provisional frame if container record allocation fails.
   * Arrange: Empty document/stack with the provider failing its second allocation.
   * Act: Open an array, admitting frame storage before record storage.
   * Expect: No document record or live frame remains, while capacity may be retained.
   */
  TEST(J5_048_ContainerOpen, RollsBackFrameAfterRecordAllocationFailure)
  {
    /* Arrange */
    engine_provider provider;
    provider.m_fail_on = 2;
    accounted_storage accounting{provider.callbacks(), 32768};
    work_budget work{100};
    document destination{accounting.callbacks(), 10};
    parser_stack frames{accounting.callbacks(), 4};
    /* Act */
    const auto opened = open_container(destination, frames, {punctuation_kind::left_bracket, 0, 1}, work);
    /* Expect */
    ASSERT_FALSE(opened.has_value());
    EXPECT_EQ(opened.error_if()->code, error_code::allocation_failure);
    EXPECT_EQ(provider.m_attempts, 2u);
    EXPECT_EQ(destination.size(), 0u);
    EXPECT_EQ(frames.size(), 0u);
    EXPECT_EQ(frames.top(), nullptr);
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * Reject a mismatched close atomically before accepting the correct delimiter.
   * Arrange: An empty array frame in a private document.
   * Act: Try right brace, then right bracket at its original offset.
   * Expect: Mismatch retains frame and absent span; success pops and stores full span.
   */
  TEST(J5_048_ContainerClose, PreservesFrameAndSpanOnMismatch)
  {
    /* Arrange */
    engine_provider provider;
    accounted_storage accounting{provider.callbacks(), 32768};
    work_budget work{100};
    document destination{accounting.callbacks(), 10};
    parser_stack frames{accounting.callbacks(), 4};
    const auto opened = open_container(destination, frames, {punctuation_kind::left_bracket, 0, 1}, work);
    ASSERT_TRUE(opened.has_value());
    /* Act */
    const auto rejected =
      close_container(destination, frames, {punctuation_kind::right_brace, 2, 1}, syntax_mode::json5, 3, work);
    /* Expect */
    ASSERT_FALSE(rejected.has_value());
    EXPECT_EQ(rejected.error_if()->code, error_code::syntax);
    EXPECT_EQ(frames.size(), 1u);
    const auto view = destination.view(*opened.value_if());
    ASSERT_TRUE(view.has_value());
    const auto absent = view.value_if()->source_span();
    ASSERT_TRUE(absent.has_value());
    EXPECT_FALSE(absent.value_if()->has_value());
    const auto closed =
      close_container(destination, frames, {punctuation_kind::right_bracket, 2, 1}, syntax_mode::json5, 3, work);
    ASSERT_TRUE(closed.has_value());
    EXPECT_EQ(closed.value_if()->position(), opened.value_if()->position());
    EXPECT_EQ(frames.size(), 0u);
    const auto span = view.value_if()->source_span();
    ASSERT_TRUE(span.has_value());
    ASSERT_TRUE(span.value_if()->has_value());
    EXPECT_EQ((**span.value_if()).byte_offset, 0u);
    EXPECT_EQ((**span.value_if()).byte_length, 3u);
  }
  /* ================================= End Test_J5_048 ================================= */
} // namespace
