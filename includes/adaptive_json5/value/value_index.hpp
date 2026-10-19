#ifndef VALUE_INDEX_HPP_
#define VALUE_INDEX_HPP_

#include <cstddef>
#include <limits>

namespace adaptive_json5
{
  /**
   * @brief Identify a position in one document's value storage.
   * @par Requirements
   * P030; J5-028; J5-030; J5-031; J5-039.
   * @note Contains no ownership and is meaningful only within its originating document.
   * An unset index does not identify a null value.
   * is_set() checks only the sentinel, not document membership or bounds.
   * Document access must validate bounds before reading storage.
   * No allocation or pointer arithmetic is performed.
   */
  class value_index
  {
    public:
      static constexpr std::size_t c_unset_position = std::numeric_limits<std::size_t>::max();

    private:
      std::size_t m_position = c_unset_position;

    public:
      /**
       * @brief Construct an unset index.
       * @par Requirements
       * P030; J5-039.
       */
      constexpr value_index() noexcept = default;

      /**
       * @brief Store an explicit document-relative position.
       * @par Requirements
       * P030; J5-039.
       * @param position Value-storage position, or c_unset_position for unset.
       * @note Construction does not validate document bounds or membership.
       */
      constexpr explicit value_index(std::size_t position) noexcept : m_position(position) {}

      /**
       * @brief Check whether this index contains a position rather than the sentinel.
       * @par Requirements
       * P030; J5-039.
       * @note A set index can still be outside a particular document's bounds.
       */
      constexpr bool is_set() const noexcept
      {
        return m_position != c_unset_position;
      }

      /**
       * @brief Read the stored position or unset sentinel.
       * @par Requirements
       * P030; J5-039.
       * @note Callers must validate the index before indexing document storage.
       */
      constexpr std::size_t position() const noexcept
      {
        return m_position;
      }
  };
} // namespace adaptive_json5

#endif /* VALUE_INDEX_HPP_ */
