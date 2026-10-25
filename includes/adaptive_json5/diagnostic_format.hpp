#ifndef ADAPTIVE_JSON5_DIAGNOSTIC_FORMAT_HPP_
#define ADAPTIVE_JSON5_DIAGNOSTIC_FORMAT_HPP_

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/input.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/text_position.hpp"
#include "adaptive_json5/value/string_warning.hpp"

#include <cstddef>
#include <optional>

namespace adaptive_json5
{
  /**
   * @brief Format an error as one human-readable line, locating it in the original input.
   * @par Requirements
   * J5-043; J5-044; J5-045; issue #96.
   * @param failure Error returned by a library operation.
   * @param input Original input the error's offsets refer to.
   * @param buffer Caller storage for the NUL-terminated message, or nullptr when capacity is 0.
   * @param capacity Size of buffer in bytes, including room for the terminating NUL.
   * @return Full message length excluding the NUL, even when the buffer was too small;
   *         wrong_type_or_access for a null buffer with nonzero capacity or an offset beyond input.
   * @note Output is "line L, column C: message"; located duplicates append
   * " (first at line L, column C)"; unlocated errors print only the message.
   * Truncated output is always NUL-terminated when capacity is nonzero.
   * Text is English, stable and contains no input bytes; callers use error codes, not text.
   * Performs no allocation and no locale-dependent operation. Scans input up to each offset.
   */
  [[nodiscard]] result<std::size_t> format_error(const error& failure, input_view input, char* buffer,
                                                 std::size_t capacity) noexcept;

  /**
   * @brief Format an error using positions the caller has already located.
   * @par Requirements
   * J5-043; J5-044; J5-045; issue #96.
   * @param failure Error returned by a library operation.
   * @param primary Position of failure.byte_offset; ignored when the error has no location.
   * @param first_duplicate Position of the first duplicate name; omitted text when absent.
   * @param buffer Caller storage for the NUL-terminated message, or nullptr when capacity is 0.
   * @param capacity Size of buffer in bytes, including room for the terminating NUL.
   * @return Full message length excluding the NUL; wrong_type_or_access for a null buffer with nonzero capacity.
   * @note Produces exactly the text of format_error for the same positions, without scanning input.
   * Performs no allocation and no locale-dependent operation.
   */
  [[nodiscard]] result<std::size_t> format_error_at(const error& failure, text_position primary,
                                                    std::optional<text_position> first_duplicate, char* buffer,
                                                    std::size_t capacity) noexcept;

  /**
   * @brief Format a string warning as one human-readable line, locating it in the original input.
   * @par Requirements
   * J5-017; J5-044; issue #96.
   * @param warning Warning recorded for a parsed string.
   * @param input Original input the warning's offset refers to.
   * @param buffer Caller storage for the NUL-terminated message, or nullptr when capacity is 0.
   * @param capacity Size of buffer in bytes, including room for the terminating NUL.
   * @return Full message length excluding the NUL; wrong_type_or_access for a null buffer with
   *         nonzero capacity or an offset beyond input.
   * @note Output is "line L, column C: warning: message". Same truncation, text and allocation rules as format_error.
   */
  [[nodiscard]] result<std::size_t> format_warning(const string_warning& warning, input_view input, char* buffer,
                                                   std::size_t capacity) noexcept;

  /**
   * @brief Format a string warning using a position the caller has already located.
   * @par Requirements
   * J5-017; J5-044; issue #96.
   * @param warning Warning recorded for a parsed string.
   * @param position Position of warning.byte_offset.
   * @param buffer Caller storage for the NUL-terminated message, or nullptr when capacity is 0.
   * @param capacity Size of buffer in bytes, including room for the terminating NUL.
   * @return Full message length excluding the NUL; wrong_type_or_access for a null buffer with nonzero capacity.
   * @note Produces exactly the text of format_warning for the same position, without scanning input.
   */
  [[nodiscard]] result<std::size_t> format_warning_at(const string_warning& warning, text_position position,
                                                      char* buffer, std::size_t capacity) noexcept;
} // namespace adaptive_json5

#endif /* ADAPTIVE_JSON5_DIAGNOSTIC_FORMAT_HPP_ */
