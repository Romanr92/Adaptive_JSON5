// SPDX-License-Identifier: Apache-2.0
#include "adaptive_json5/diagnostic_format.hpp"
#include "adaptive_json5/json5_document.hpp"
#include "adaptive_json5/memory/buffer_arena.hpp"
#include "adaptive_json5/value/floating_conversion.hpp"
#include "adaptive_json5/value/numeric_conversion.hpp"
#include "adaptive_json5/value/utf8_export.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <utility>

namespace
{
  using namespace adaptive_json5;

  /**
   * @brief Visit every parsed node with a bounded, nonrecursive work list.
   * @par Requirements
   * Issue #132; J5-097.
   * @note Structured conversion and export failures are expected outcomes.
   */
  void inspect(const document& values)
  {
    std::array<value_index, 1024> pending{};
    std::size_t count = 0U;
    const auto root = values.root();
    if (!root.has_value())
    {
      std::abort();
    }
    const auto index = root.value_if()->index();
    if (!index.has_value())
    {
      std::abort();
    }
    pending[count++] = *index.value_if();
    alignas(std::max_align_t) std::array<std::byte, 65536> scratch{};
    buffer_arena arena(scratch.data(), scratch.size());
    std::array<std::byte, 16384> utf8{};
    std::size_t visited = 0U;
    while (count != 0U)
    {
      if (++visited > pending.size())
      {
        std::abort();
      }
      const auto current = pending[--count];
      const auto value = values.view(current);
      if (!value.has_value())
      {
        std::abort();
      }
      const auto& view = *value.value_if();
      (void)view.source_span();
      const auto name = view.member_name();
      if (name.has_value())
      {
        (void)export_utf8(**name.value_if(), utf8.data(), utf8.size());
      }
      const auto text = view.as_string();
      if (text.has_value())
      {
        (void)utf8_size(**text.value_if());
        (void)export_utf8(**text.value_if(), utf8.data(), utf8.size());
      }
      const auto number = view.as_number();
      if (number.has_value())
      {
        (void)to_signed_integer(**number.value_if());
        (void)to_unsigned_integer(**number.value_if());
        (void)is_integral_number(**number.value_if());
        (void)to_double(**number.value_if(), arena.callbacks());
        (void)to_float(**number.value_if(), arena.callbacks());
      }
      const auto children = values.iterate(current);
      if (children.has_value())
      {
        auto iterator = std::move(*children.value_if());
        for (;;)
        {
          const auto next = iterator.next();
          if (!next.has_value())
          {
            std::abort();
          }
          if (!next.value_if()->has_value())
          {
            break;
          }
          const auto child = next.value_if()->value().index();
          if (!child.has_value() || count == pending.size())
          {
            std::abort();
          }
          pending[count++] = *child.value_if();
        }
      }
    }
  }
} // namespace

/**
 * @brief Fuzz complete parsing and public access in both syntax and duplicate modes.
 * @par Requirements
 * Issue #132; J5-097; J5-098.
 * @note libFuzzer owns input. Each parse owns a separate bounded caller buffer.
 */
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
  using namespace adaptive_json5;
  if (size > 16384U)
  {
    return 0;
  }
  const input_view input{reinterpret_cast<const char*>(data), size};
  for (const auto mode : {syntax_mode::json5, syntax_mode::strict_json})
  {
    for (const auto duplicates : {duplicate_name_policy::preserve_all, duplicate_name_policy::reject_duplicates})
    {
      alignas(std::max_align_t) std::array<std::byte, 524288> storage{};
      parse_options options{};
      options.syntax = mode;
      options.duplicate_names = duplicates;
      options.source_text = (size % 2U == 0U) ? source_retention_policy::retain_copy : source_retention_policy::discard;
      options.limits.max_depth = 64U;
      const auto parsed = json5_document::parse(input, storage.data(), storage.size(), options);
      if (parsed.has_value())
      {
        inspect(parsed.value_if()->values());
        const auto root = parsed.value_if()->root();
        (void)root.member("x").at(0U).as_int64();
        (void)root.as_uint64();
        (void)root.as_double();
        char text[256]{};
        (void)root.as_string(text, sizeof(text));
      }
      else
      {
        char message[256]{};
        (void)format_error(*parsed.error_if(), input, message, sizeof(message));
      }
    }
  }
  return 0;
}
