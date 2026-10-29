#ifndef JSON5_VALUE_HPP_
#define JSON5_VALUE_HPP_

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/value_kind.hpp"
#include "adaptive_json5/value/value_view.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace adaptive_json5
{
  class json5_document;

  /**
   * @brief Navigate and read one value of a json5_document in a few calls.
   * @par Requirements
   * P030; J5-028; J5-030; J5-031; J5-033; J5-036; J5-039; J5-040.
   * @note Holds either a value or the error of the step that produced it, so calls can be
   * chained: root().member("server").member("port").as_uint64(). A failed step passes its
   * error on unchanged, and the as_ functions return it.
   * A value borrows its document: it is valid only while that document is alive and has
   * not been moved from. It owns nothing and allocates nothing, except as_double(), which
   * uses scratch memory from the document's own storage.
   * Copyable; copy and move assignment are unavailable because result is not assignable.
   * Every function is noexcept and never throws.
   * Error codes: wrong_type_or_access for a missing member, a member name that occurs more
   * than once, an index out of range or a value of another kind; encoding for a name that
   * is not valid UTF-8; the conversion errors of the numeric and UTF-8 export functions.
   */
  class [[nodiscard]] json5_value
  {
    private:
      // The value, or the error of the step that produced this object.
      result<value_view> m_value;
      // Scratch storage of the owning document, used by as_double().
      storage_callbacks m_scratch;

      /**
       * @brief Wrap a value or an error together with the document's scratch storage.
       * @par Requirements
       * J5-040.
       * @param value The value, or the error of the step that produced it.
       * @param scratch Storage callbacks of the owning document, used by as_double().
       * @note Only json5_document and json5_value itself create values.
       */
      json5_value(result<value_view> value, const storage_callbacks& scratch) noexcept;

      friend class json5_document;

    public:
      /**
       * @brief Report whether this object holds a value rather than an error.
       * @par Requirements
       * J5-003; J5-039.
       * @return True for a value, false for an error.
       */
      [[nodiscard]] bool ok() const noexcept;

      /**
       * @brief Borrow the error this object holds, if any.
       * @par Requirements
       * J5-003; J5-043.
       * @return The error, or nullptr when this object holds a value.
       */
      [[nodiscard]] const error* error_if() const noexcept;

      /**
       * @brief Report the kind of the value.
       * @par Requirements
       * J5-028; J5-039.
       * @return The value's kind, or the held error.
       */
      [[nodiscard]] result<value_kind> kind() const noexcept;

      /**
       * @brief Report whether the value is null.
       * @par Requirements
       * J5-028.
       * @return True only for a null value; false for every other kind and for an error.
       */
      [[nodiscard]] bool is_null() const noexcept;

      /**
       * @brief Report whether the value is a boolean.
       * @par Requirements
       * J5-028.
       * @return True only for a boolean value; false otherwise and for an error.
       */
      [[nodiscard]] bool is_boolean() const noexcept;

      /**
       * @brief Report whether the value is a string.
       * @par Requirements
       * J5-028.
       * @return True only for a string value; false otherwise and for an error.
       */
      [[nodiscard]] bool is_string() const noexcept;

      /**
       * @brief Report whether the value is a number.
       * @par Requirements
       * J5-028.
       * @return True only for a number, including Infinity and NaN; false otherwise and for
       * an error.
       */
      [[nodiscard]] bool is_number() const noexcept;

      /**
       * @brief Report whether the value is an array.
       * @par Requirements
       * J5-028.
       * @return True only for an array; false otherwise and for an error.
       */
      [[nodiscard]] bool is_array() const noexcept;

      /**
       * @brief Report whether the value is an object.
       * @par Requirements
       * J5-028.
       * @return True only for an object; false otherwise and for an error.
       */
      [[nodiscard]] bool is_object() const noexcept;

      /**
       * @brief Select the member of an object by its UTF-8 name.
       * @par Requirements
       * J5-030; J5-031; J5-033; J5-039.
       * @param name Member name in UTF-8, compared exactly with the decoded name.
       * @return The member's value, or an object holding the error: the held error;
       * wrong_type_or_access when this is not an object, when no member has the name, or when
       * more than one member has it; encoding when name is not valid UTF-8.
       * @note Exact comparison without normalization or case folding. Preserved duplicate
       * names are never resolved silently (J5-031). Allocates nothing.
       */
      [[nodiscard]] json5_value member(std::string_view name) const noexcept;

      /**
       * @brief Select the element of an array by position.
       * @par Requirements
       * J5-030; J5-039.
       * @param index Zero-based position.
       * @return The element's value, or an object holding the error: the held error;
       * wrong_type_or_access when this is not an array or index is out of range.
       */
      [[nodiscard]] json5_value at(std::size_t index) const noexcept;

      /**
       * @brief Report the number of elements of an array or members of an object.
       * @par Requirements
       * J5-030; J5-031.
       * @return The count; the held error; or wrong_type_or_access for a scalar.
       * @note Members with the same name are each counted.
       */
      [[nodiscard]] result<std::size_t> size() const noexcept;

      /**
       * @brief Read a boolean value.
       * @par Requirements
       * J5-028; J5-039.
       * @return The boolean; the held error; or wrong_type_or_access for another kind.
       */
      [[nodiscard]] result<bool> as_bool() const noexcept;

      /**
       * @brief Read a number as a signed 64-bit integer, exactly.
       * @par Requirements
       * J5-034; J5-036; J5-039.
       * @return The integer; the held error; wrong_type_or_access for a non-number; or the
       * numeric conversion error when the number is not an integer or is out of range.
       * @note Integrality is mathematical: 1.0 and 1e2 are integers, 1.5 is not.
       */
      [[nodiscard]] result<std::int64_t> as_int64() const noexcept;

      /**
       * @brief Read a number as an unsigned 64-bit integer, exactly.
       * @par Requirements
       * J5-034; J5-036; J5-039.
       * @return As as_int64(), for std::uint64_t; negative numbers are out of range.
       */
      [[nodiscard]] result<std::uint64_t> as_uint64() const noexcept;

      /**
       * @brief Read a number as a double, rounded once.
       * @par Requirements
       * J5-034; J5-037; J5-039.
       * @return The double; the held error; wrong_type_or_access for a non-number; or the
       * numeric conversion error, for example for a non-finite value under the default policy.
       * @note Uses default floating conversion options. Scratch memory comes from the
       * document's own storage and is released before returning.
       */
      [[nodiscard]] result<double> as_double() const noexcept;

      /**
       * @brief Copy a string value as UTF-8 into a caller buffer.
       * @par Requirements
       * J5-029; J5-039; J5-041.
       * @param buffer Destination for the UTF-8 bytes; no terminating NUL is written.
       * @param capacity Bytes available in buffer.
       * @return A view of the written bytes inside buffer; the held error;
       * wrong_type_or_access for another kind; resource_limit when capacity is too small;
       * encoding for an unpaired surrogate.
       * @note Allocates nothing. The view is valid while buffer is unchanged. Use utf8_size()
       * from adaptive_json5/value/utf8_export.hpp when the needed capacity is unknown.
       */
      [[nodiscard]] result<std::string_view> as_string(char* buffer, std::size_t capacity) const noexcept;
  };
} // namespace adaptive_json5

#endif /* JSON5_VALUE_HPP_ */
