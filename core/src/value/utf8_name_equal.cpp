#include "value/utf8_name_equal.hpp"

#include "adaptive_json5/input.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/decoded_string.hpp"
#include "input/cursor.hpp"
#include "unicode/utf16.hpp"
#include "unicode/utf8.hpp"

#include <cstddef>

namespace adaptive_json5
{
  namespace detail
  {
    /**
     * @brief Compare a decoded member name with a UTF-8 name, without allocating.
     * @par Requirements
     * P030; J5-031; J5-033; J5-046.
     * @return True for the same characters, false otherwise, or the UTF-8 decoding error.
     */
    result<bool> utf8_name_equal(const decoded_string& name, std::string_view utf8) noexcept
    {
      using equal_result = result<bool>;

      cursor source{input_view{utf8.data(), utf8.size()}};
      std::size_t position{};

      while (true)
      {
        const auto decoded = decode_utf8(source);

        if (!decoded.has_value())
        {
          return equal_result::failure(*decoded.error_if());
        }

        // The end of the UTF-8 name: equal only if the decoded name ends here too.
        if (!decoded.value_if()->has_value())
        {
          return equal_result::success(position == name.size());
        }

        const auto encoded = encode_utf16((*decoded.value_if())->value);

        if (!encoded.has_value())
        {
          return equal_result::failure(*encoded.error_if());
        }

        // One character may be two UTF-16 units (a surrogate pair); compare each.
        for (std::size_t index{}; index < encoded.value_if()->size; ++index)
        {
          if (position >= name.size())
          {
            return equal_result::success(false);
          }

          const auto unit = name.unit_at(position);

          if (!unit.has_value())
          {
            return equal_result::failure(*unit.error_if());
          }

          if (*unit.value_if() != encoded.value_if()->units[index])
          {
            return equal_result::success(false);
          }

          ++position;
        }
      }
    }
  } // namespace detail
} // namespace adaptive_json5
