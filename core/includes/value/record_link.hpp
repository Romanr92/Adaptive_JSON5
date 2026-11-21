#ifndef RECORD_LINK_HPP_
#define RECORD_LINK_HPP_

#include <cstddef>
#include <limits>

namespace adaptive_json5
{
  namespace detail
  {
    /**
     * @brief Untagged position of a record inside one value_storage.
     * @par Requirements
     * P030; J5-030; J5-031; J5-039; J5-040.
     * @note Private representation of parent, sibling and container links
     * (ADR 0007 §4, compact private links). Holds no owner tag and no ownership;
     * it is meaningful only inside the storage that holds it.
     * Convert to a public index only through value_storage::issue(); record access
     * still goes through value_storage::at(), which checks bounds before reading.
     * Damaged links report internal_error, not a caller access error.
     * Trivially copyable; performs no allocation.
     */
    // MISRA rule: RULE-4-1-2 cpp/misra/implicit-declaration-of-copy-constructor
    // MISRA justification: The copy constructor and copy assignment are explicitly declared in the class (=
    // default or = delete), so nothing relies on a deprecated implicit declaration.
    // codeql::misra_deviation_next_line(DEV-009.02)
    class [[nodiscard]] record_link
    {
      public:
        /** @brief Sentinel position of an unset link; equal to value_index::c_unset_position. */
        static constexpr std::size_t c_unset_position = std::numeric_limits<std::size_t>::max();

        /**
         * @brief Construct an unset link.
         * @par Requirements
         * P030; J5-039.
         * @par ID SWU-VALUE-172
         */
        constexpr record_link() noexcept = default;
        /**
         * @brief Construct a link to a record position.
         * @par Requirements
         * P030; J5-039.
         * @param position Record position in the owning storage; not validated here.
         * @par ID SWU-VALUE-173
         */
        constexpr explicit record_link(std::size_t position) noexcept : m_position(position) {}

        ~record_link() noexcept = default;

        record_link(const record_link&) noexcept = default;
        record_link& operator=(const record_link&) noexcept = default;
        // MISRA rule: RULE-18-4-1 cpp/misra/exception-unfriendly-function-must-be-noexcept
        // MISRA justification: The move operation is declared noexcept in both its declaration and its
        // definition. The tool does not see the specifier in the -fno-exceptions analysis build.
        // codeql::misra_deviation_next_line(DEV-022.06)
        record_link(record_link&&) noexcept = default;
        // MISRA rule: RULE-18-4-1 cpp/misra/exception-unfriendly-function-must-be-noexcept
        // MISRA justification: The move operation is declared noexcept in both its declaration and its
        // definition. The tool does not see the specifier in the -fno-exceptions analysis build.
        // codeql::misra_deviation_next_line(DEV-022.05)
        record_link& operator=(record_link&&) noexcept = default;

        /**
         * @brief Check whether this link holds a position rather than the sentinel.
         * @par Requirements
         * P030; J5-039.
         * @note Does not check bounds.
         * @par ID SWU-VALUE-174
         */
        [[nodiscard]] constexpr bool is_set() const noexcept
        {
          return m_position != c_unset_position;
        }

        /**
         * @brief Return the stored position, or c_unset_position.
         * @par Requirements
         * P030; J5-039.
         * @par ID SWU-VALUE-175
         */
        [[nodiscard]] constexpr std::size_t position() const noexcept
        {
          return m_position;
        }

      private:
        std::size_t m_position = c_unset_position;
    };
  } // namespace detail
} // namespace adaptive_json5

#endif /* RECORD_LINK_HPP_ */
