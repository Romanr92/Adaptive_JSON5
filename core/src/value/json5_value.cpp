#include "adaptive_json5/json5_value.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/floating_conversion.hpp"
#include "adaptive_json5/value/integer_conversion.hpp"
#include "adaptive_json5/value/utf8_export.hpp"
#include "adaptive_json5/value/value_kind.hpp"
#include "adaptive_json5/value/value_view.hpp"
#include "value/utf8_name_equal.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

namespace adaptive_json5
{
  namespace
  {
    /**
     * @brief Build the error every navigation step reports for the wrong kind or a missing value.
     * @par Requirements
     * J5-039.
     */
    [[nodiscard]] error wrong_access() noexcept
    {
      return error::without_location(error_code::wrong_type_or_access);
    }
  } // namespace

  /**
   * @brief Wrap a value or an error together with the document's scratch storage.
   * @par Requirements
   * J5-040.
   */
  json5_value::json5_value(result<value_view> value, const storage_callbacks& scratch) noexcept
      : m_value(std::move(value)), m_scratch(scratch)
  {
  }

  /**
   * @brief Report whether this object holds a value rather than an error.
   * @par Requirements
   * J5-003; J5-039.
   */
  bool json5_value::ok() const noexcept
  {
    return m_value.has_value();
  }

  /**
   * @brief Borrow the error this object holds, if any.
   * @par Requirements
   * J5-003; J5-043.
   */
  const error* json5_value::error_if() const noexcept
  {
    return m_value.error_if();
  }

  /**
   * @brief Report the kind of the value.
   * @par Requirements
   * J5-028; J5-039.
   */
  result<value_kind> json5_value::kind() const noexcept
  {
    if (!m_value.has_value())
    {
      return result<value_kind>::failure(*m_value.error_if());
    }

    return m_value.value_if()->kind();
  }

  /**
   * @brief Report whether the value is null.
   * @par Requirements
   * J5-028.
   */
  bool json5_value::is_null() const noexcept
  {
    const auto current = kind();
    return current.has_value() && (*current.value_if() == value_kind::null);
  }

  /**
   * @brief Report whether the value is a boolean.
   * @par Requirements
   * J5-028.
   */
  bool json5_value::is_boolean() const noexcept
  {
    const auto current = kind();
    return current.has_value() && (*current.value_if() == value_kind::boolean);
  }

  /**
   * @brief Report whether the value is a string.
   * @par Requirements
   * J5-028.
   */
  bool json5_value::is_string() const noexcept
  {
    const auto current = kind();
    return current.has_value() && (*current.value_if() == value_kind::string);
  }

  /**
   * @brief Report whether the value is a number.
   * @par Requirements
   * J5-028.
   */
  bool json5_value::is_number() const noexcept
  {
    const auto current = kind();
    return current.has_value() && (*current.value_if() == value_kind::number);
  }

  /**
   * @brief Report whether the value is an array.
   * @par Requirements
   * J5-028.
   */
  bool json5_value::is_array() const noexcept
  {
    const auto current = kind();
    return current.has_value() && (*current.value_if() == value_kind::array);
  }

  /**
   * @brief Report whether the value is an object.
   * @par Requirements
   * J5-028.
   */
  bool json5_value::is_object() const noexcept
  {
    const auto current = kind();
    return current.has_value() && (*current.value_if() == value_kind::object);
  }

  /**
   * @brief Select the member of an object by its UTF-8 name.
   * @par Requirements
   * J5-030; J5-031; J5-033; J5-039.
   */
  json5_value json5_value::member(std::string_view name) const noexcept
  {
    using view_result = result<value_view>;

    // A held error passes on unchanged, so chained calls report the first failure.
    if (!m_value.has_value())
    {
      return *this;
    }

    if (!is_object())
    {
      return json5_value{view_result::failure(wrong_access()), m_scratch};
    }

    const value_view& object = *m_value.value_if();
    const auto count = object.container_size();

    if (!count.has_value())
    {
      return json5_value{view_result::failure(*count.error_if()), m_scratch};
    }

    std::optional<value_view> found{};

    // Every member is checked, so a name that occurs twice is detected instead of resolved.
    for (std::size_t position{}; position < *count.value_if(); ++position)
    {
      const auto candidate = object.member_at(position);

      if (!candidate.has_value())
      {
        return json5_value{view_result::failure(*candidate.error_if()), m_scratch};
      }

      const auto candidate_name = candidate.value_if()->member_name();

      if (!candidate_name.has_value())
      {
        return json5_value{view_result::failure(*candidate_name.error_if()), m_scratch};
      }

      const auto equal = detail::utf8_name_equal(**candidate_name.value_if(), name);

      if (!equal.has_value())
      {
        return json5_value{view_result::failure(*equal.error_if()), m_scratch};
      }

      if (*equal.value_if())
      {
        // Preserved duplicates are never resolved silently (J5-031).
        if (found.has_value())
        {
          return json5_value{view_result::failure(wrong_access()), m_scratch};
        }

        found = *candidate.value_if();
      }
    }

    if (!found.has_value())
    {
      return json5_value{view_result::failure(wrong_access()), m_scratch};
    }

    return json5_value{view_result::success(*found), m_scratch};
  }

  /**
   * @brief Select the element of an array by position.
   * @par Requirements
   * J5-030; J5-039.
   */
  json5_value json5_value::at(std::size_t index) const noexcept
  {
    using view_result = result<value_view>;

    if (!m_value.has_value())
    {
      return *this;
    }

    // element_at() rejects non-arrays and out-of-range positions with wrong_type_or_access.
    return json5_value{m_value.value_if()->element_at(index), m_scratch};
  }

  /**
   * @brief Report the number of elements of an array or members of an object.
   * @par Requirements
   * J5-030; J5-031.
   */
  result<std::size_t> json5_value::size() const noexcept
  {
    if (!m_value.has_value())
    {
      return result<std::size_t>::failure(*m_value.error_if());
    }

    // container_size() rejects scalars instead of treating them as empty.
    return m_value.value_if()->container_size();
  }

  /**
   * @brief Read a boolean value.
   * @par Requirements
   * J5-028; J5-039.
   */
  result<bool> json5_value::as_bool() const noexcept
  {
    if (!m_value.has_value())
    {
      return result<bool>::failure(*m_value.error_if());
    }

    return m_value.value_if()->as_boolean();
  }

  /**
   * @brief Read a number as a signed 64-bit integer, exactly.
   * @par Requirements
   * J5-034; J5-036; J5-039.
   */
  result<std::int64_t> json5_value::as_int64() const noexcept
  {
    if (!m_value.has_value())
    {
      return result<std::int64_t>::failure(*m_value.error_if());
    }

    return to_integer<std::int64_t>(*m_value.value_if());
  }

  /**
   * @brief Read a number as an unsigned 64-bit integer, exactly.
   * @par Requirements
   * J5-034; J5-036; J5-039.
   */
  result<std::uint64_t> json5_value::as_uint64() const noexcept
  {
    if (!m_value.has_value())
    {
      return result<std::uint64_t>::failure(*m_value.error_if());
    }

    return to_integer<std::uint64_t>(*m_value.value_if());
  }

  /**
   * @brief Read a number as a double, rounded once.
   * @par Requirements
   * J5-034; J5-037; J5-039.
   */
  result<double> json5_value::as_double() const noexcept
  {
    if (!m_value.has_value())
    {
      return result<double>::failure(*m_value.error_if());
    }

    const auto number = m_value.value_if()->as_number();

    if (!number.has_value())
    {
      return result<double>::failure(*number.error_if());
    }

    // Exact rounding may need big-integer scratch; it comes from the document's own storage.
    return to_double(**number.value_if(), m_scratch);
  }

  /**
   * @brief Copy a string value as UTF-8 into a caller buffer.
   * @par Requirements
   * J5-029; J5-039; J5-041.
   */
  result<std::string_view> json5_value::as_string(char* buffer, std::size_t capacity) const noexcept
  {
    using string_result = result<std::string_view>;

    if (!m_value.has_value())
    {
      return string_result::failure(*m_value.error_if());
    }

    const auto text = m_value.value_if()->as_string();

    if (!text.has_value())
    {
      return string_result::failure(*text.error_if());
    }

    // export_utf8 writes bytes as unsigned char; char and unsigned char may alias each other.
    const auto written = export_utf8(**text.value_if(), reinterpret_cast<unsigned char*>(buffer), capacity);

    if (!written.has_value())
    {
      return string_result::failure(*written.error_if());
    }

    return string_result::success(std::string_view{buffer, *written.value_if()});
  }
} // namespace adaptive_json5
