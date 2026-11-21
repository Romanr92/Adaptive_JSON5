#ifndef VALUE_INDEX_HPP_
#define VALUE_INDEX_HPP_

#include <cstddef>
#include <limits>

namespace adaptive_json5
{
  namespace detail
  {
    class value_storage;
  } // namespace detail
  /**
   * @brief Identify a position in one document's value storage.
   * @par Requirements
   * P030; J5-028; J5-030; J5-031; J5-039.
   * @note Contains no ownership and is meaningful only within its originating document.
   * An unset index does not identify a null value.
   * is_set() checks only the sentinel, not document membership or bounds.
   * Document access must validate owner identity and bounds before reading storage.
   * No allocation or pointer arithmetic is performed.
   */
  // MISRA rule: RULE-15-0-1 cpp/misra/improperly-provided-special-member-functions-audit
  // MISRA justification: Audit result only: the class declares no special members of the listed kind, so the
  // compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because
  // implicit members that are never odr-used are not in its database.
  // codeql::misra_deviation_next_line(DEV-020.40)
  class [[nodiscard]] value_index
  {
    public:
      static constexpr std::size_t c_unset_position = std::numeric_limits<std::size_t>::max();

    private:
      std::size_t m_position = c_unset_position;
      const void* m_owner = nullptr;

      constexpr value_index(const void* owner, std::size_t position) noexcept : m_position(position), m_owner(owner) {}

      friend class detail::value_storage;

    public:
      /**
       * @brief Construct an unset index.
       * @par Requirements
       * P030; J5-039.
       */
      constexpr value_index() noexcept = default;

      /**
       * @brief Check whether this index contains a position rather than the sentinel.
       * @par Requirements
       * P030; J5-039.
       * @note A set index can still be outside a particular document's bounds.
       * @par ID SWU-VALUE-148
       */
      [[nodiscard]] constexpr bool is_set() const noexcept
      {
        return m_position != c_unset_position;
      }

      /**
       * @brief Read the stored position or unset sentinel.
       * @par Requirements
       * P030; J5-039.
       * @note Callers must validate the index before indexing document storage.
       * @par ID SWU-VALUE-149
       */
      [[nodiscard]] constexpr std::size_t position() const noexcept
      {
        return m_position;
      }
  };
} // namespace adaptive_json5

#endif /* VALUE_INDEX_HPP_ */
