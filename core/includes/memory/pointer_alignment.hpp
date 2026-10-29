#ifndef POINTER_ALIGNMENT_HPP_
#define POINTER_ALIGNMENT_HPP_

#include <cstddef>

namespace adaptive_json5
{
  namespace detail
  {
    /**
     * @brief Align a position inside a buffer, like std::align, without <memory>.
     * @par Requirements
     * J5-042; J5-046; J5-101.
     * @param alignment Power-of-two alignment.
     * @param bytes Bytes that must fit after the aligned position.
     * @param position In: current position. Out on success: the aligned position.
     * @param space In: bytes from position to the end of the buffer. Out on success:
     * reduced by the padding, but not by bytes.
     * @return The aligned position, or nullptr when bytes do not fit; then position and
     * space are unchanged.
     * @note Same contract as std::align; <memory> is not on the J5-042 allowlist.
     */
    [[nodiscard]] void* align_within(std::size_t alignment, std::size_t bytes, void*& position,
                                     std::size_t& space) noexcept;
  } // namespace detail
} // namespace adaptive_json5

#endif /* POINTER_ALIGNMENT_HPP_ */
