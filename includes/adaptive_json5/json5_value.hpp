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
   * not been moved from. It owns nothing and allocates nothing, except the as_double() and
   * as_float() overloads without a scratch argument, which may take scratch memory from the
   * document's own storage for numbers outside the scratch-free range.
   * Thread safety: several threads may navigate and read one document concurrently, with
   * one exception. as_double() and as_float() without a scratch argument use the
   * document's storage when a number needs scratch, so they must not run concurrently on
   * one document unless every number they read is scratch-free. For guaranteed concurrent
   * reads, pass each thread its own scratch provider to as_double(scratch) / as_float(scratch).
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
      // Scratch storage of the owning document, used by as_double() and as_float().
      storage_callbacks m_scratch;

      /**
       * @brief Wrap a value or an error together with the document's scratch storage.
       * @par Requirements
       * J5-040.
       * @param value The value, or the error of the step that produced it.
       * @param scratch Storage callbacks of the owning document, used by as_double() and as_float().
       * @note Only json5_document and json5_value itself create values.
       * @par ID SWU-VALUE-059
       */
      json5_value(result<value_view> value, const storage_callbacks& scratch) noexcept;

      friend class json5_document;

    public:
      /**
       * @brief Report whether this object holds a value rather than an error.
       * @par Requirements
       * J5-003; J5-039.
       * @return True for a value, false for an error.
       * @par ID SWU-VALUE-060
       */
      [[nodiscard]] bool ok() const noexcept;

      /**
       * @brief Borrow the error this object holds, if any.
       * @par Requirements
       * J5-003; J5-043.
       * @return The error, or nullptr when this object holds a value.
       * @par ID SWU-VALUE-061
       */
      [[nodiscard]] const error* error_if() const noexcept;

      /**
       * @brief Report the kind of the value.
       * @par Requirements
       * J5-028; J5-039.
       * @return The value's kind, or the held error.
       * @par ID SWU-VALUE-062
       */
      [[nodiscard]] result<value_kind> kind() const noexcept;

      /**
       * @brief Report whether the value is null.
       * @par Requirements
       * J5-028.
       * @return True only for a null value; false for every other kind and for an error.
       * @par ID SWU-VALUE-063
       */
      [[nodiscard]] bool is_null() const noexcept;

      /**
       * @brief Report whether the value is a boolean.
       * @par Requirements
       * J5-028.
       * @return True only for a boolean value; false otherwise and for an error.
       * @par ID SWU-VALUE-064
       */
      [[nodiscard]] bool is_boolean() const noexcept;

      /**
       * @brief Report whether the value is a string.
       * @par Requirements
       * J5-028.
       * @return True only for a string value; false otherwise and for an error.
       * @par ID SWU-VALUE-065
       */
      [[nodiscard]] bool is_string() const noexcept;

      /**
       * @brief Report whether the value is a number.
       * @par Requirements
       * J5-028.
       * @return True only for a number, including Infinity and NaN; false otherwise and for
       * an error.
       * @par ID SWU-VALUE-066
       */
      [[nodiscard]] bool is_number() const noexcept;

      /**
       * @brief Report whether the value is an array.
       * @par Requirements
       * J5-028.
       * @return True only for an array; false otherwise and for an error.
       * @par ID SWU-VALUE-067
       */
      [[nodiscard]] bool is_array() const noexcept;

      /**
       * @brief Report whether the value is an object.
       * @par Requirements
       * J5-028.
       * @return True only for an object; false otherwise and for an error.
       * @par ID SWU-VALUE-068
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
       * @par ID SWU-VALUE-069
       */
      [[nodiscard]] json5_value member(std::string_view name) const noexcept;

      /**
       * @brief Select the element of an array by position.
       * @par Requirements
       * J5-030; J5-039.
       * @param index Zero-based position.
       * @return The element's value, or an object holding the error: the held error;
       * wrong_type_or_access when this is not an array or index is out of range.
       * @par ID SWU-VALUE-070
       */
      [[nodiscard]] json5_value at(std::size_t index) const noexcept;

      /**
       * @brief Report the number of elements of an array or members of an object.
       * @par Requirements
       * J5-030; J5-031.
       * @return The count; the held error; or wrong_type_or_access for a scalar.
       * @note Members with the same name are each counted.
       * @par ID SWU-VALUE-071
       */
      [[nodiscard]] result<std::size_t> size() const noexcept;

      /**
       * @brief Read a boolean value.
       * @par Requirements
       * J5-028; J5-039.
       * @return The boolean; the held error; or wrong_type_or_access for another kind.
       * @par ID SWU-VALUE-072
       */
      [[nodiscard]] result<bool> as_bool() const noexcept;

      /**
       * @brief Read a number as a signed 64-bit integer, exactly.
       * @par Requirements
       * J5-034; J5-036; J5-039.
       * @return The integer; the held error; wrong_type_or_access for a non-number; or the
       * numeric conversion error when the number is not an integer or is out of range.
       * @note Integrality is mathematical: 1.0 and 1e2 are integers, 1.5 is not.
       * @par ID SWU-VALUE-073
       */
      [[nodiscard]] result<std::int64_t> as_int64() const noexcept;

      /**
       * @brief Read a number as an unsigned 64-bit integer, exactly.
       * @par Requirements
       * J5-034; J5-036; J5-039.
       * @return As as_int64(), for std::uint64_t; negative numbers are out of range.
       * @par ID SWU-VALUE-074
       */
      [[nodiscard]] result<std::uint64_t> as_uint64() const noexcept;

      /**
       * @brief Read a number as a double, rounded once, using the document's scratch storage.
       * @par Requirements
       * J5-034; J5-037; J5-039.
       * @return The double; the held error; wrong_type_or_access for a non-number; or the
       * numeric conversion error, for example for a non-finite value under the default policy.
       * @note Uses default floating conversion options. Scratch memory comes from the
       * document's own storage and is released before returning.
       * @par ID SWU-VALUE-075
       * @note Uses default floating conversion options. Scratch-free when the number has a
       * normal finite result and either fits Clinger's range (significand up to 2^53, power of
       * ten up to 22) or, with ADAPTIVE_JSON5_EISEL_LEMIRE ON, is a decimal of up to 19
       * significand digits (leading zeros included) with exponent within +-342, except rare
       * cases the approximation cannot decide. Other numbers take scratch from the document's
       * storage and release it before returning.
       * Not safe concurrently on one document for numbers that need scratch; see the class note.
       */
      [[nodiscard]] result<double> as_double() const noexcept;

      /**
       * @brief Read a number as a double, rounded once, using caller scratch storage.
       * @par Requirements
       * J5-034; J5-037; J5-039; J5-040.
       * @param scratch Caller-owned fallible temporary storage, used only when the number
       * needs scratch; released before returning.
       * @return As as_double(); allocation_failure when scratch is needed and the provider fails.
       * @note Never touches the document's storage, so threads that each pass their own
       * provider may read one document concurrently. An empty storage_callbacks{} reads
       * scratch-free numbers and reports allocation_failure for the rest.
       */
      [[nodiscard]] result<double> as_double(const storage_callbacks& scratch) const noexcept;

      /**
       * @brief Read a number as a float, rounded once, directly from the exact value.
       * @par Requirements
       * J5-034; J5-037; J5-039.
       * @return As as_double(), for float.
       * @note Rounds once to float, never through double. Scratch rules as as_double().
       */
      [[nodiscard]] result<float> as_float() const noexcept;

      /**
       * @brief Read a number as a float, rounded once, using caller scratch storage.
       * @par Requirements
       * J5-034; J5-037; J5-039; J5-040.
       * @param scratch Caller-owned fallible temporary storage; see as_double(scratch).
       * @return As as_double(scratch), for float.
       */
      [[nodiscard]] result<float> as_float(const storage_callbacks& scratch) const noexcept;

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
       * @par ID SWU-VALUE-076
       */
      [[nodiscard]] result<std::string_view> as_string(char* buffer, std::size_t capacity) const noexcept;
  };
} // namespace adaptive_json5

#endif /* JSON5_VALUE_HPP_ */
