#ifndef PARSER_STACK_HPP_
#define PARSER_STACK_HPP_

#include "adaptive_json5/detail/memory/work_budget.hpp"
#include "adaptive_json5/detail/parser/parser_frame.hpp"
#include "adaptive_json5/memory/owned_block.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"

#include <cstddef>
#include <type_traits>

namespace adaptive_json5
{
  namespace detail
  {
    /**
     * @brief Own a bounded stack of open-container frames.
     * @par Requirements
     * P033; J5-003; J5-046; J5-048; J5-049.
     * @note Provider state must outlive this stack.
     * Use accounted callbacks to include frame storage in owned-byte accounting.
     * The caller separately reserves and releases resource-budget depth.
     * Growth invalidates frame pointers.
     * Cleanup is iterative and requires no allocation or work charge.
     */
    class [[nodiscard]] parser_stack
    {
      private:
        static_assert(std::is_nothrow_move_constructible_v<parser_frame>);
        static_assert(std::is_nothrow_destructible_v<parser_frame>);

        storage_callbacks m_provider;
        owned_block m_storage{};
        std::size_t m_size{};
        std::size_t m_capacity{};
        std::size_t m_max_frames;

        /**
         * @brief Address raw storage for one frame.
         * @par Requirements
         * P033; J5-046.
         * @note Private callers supply an index below allocated capacity.
         * No frame lifetime is assumed at this address.
         */
        [[nodiscard]] static void* slot_address(const owned_block& storage, std::size_t index) noexcept;

        /**
         * @brief Access an already constructed frame.
         * @par Requirements
         * P033; J5-040.
         * @note Private callers supply an index below m_size.
         */
        [[nodiscard]] parser_frame* live_frame(std::size_t index) noexcept;

        /**
         * @brief Access an already constructed frame without mutation.
         * @par Requirements
         * P033; J5-040.
         * @note Private callers supply an index below m_size.
         */
        [[nodiscard]] const parser_frame* live_frame(std::size_t index) const noexcept;

        /**
         * @brief Ensure storage for the requested number of frames.
         * @par Requirements
         * P033; J5-046; J5-048; J5-049; J5-051.
         * @note Failure preserves existing frames.
         * Growth costs one work unit per relocated frame.
         * Old and new allocations coexist during relocation.
         */
        [[nodiscard]] status_result reserve(std::size_t required, work_budget& work) noexcept;

      public:
        /**
         * @brief Construct an empty stack with an explicit frame limit.
         * @par Requirements
         * P033; J5-046; J5-048.
         * @note Construction allocates nothing. Zero permits no frames.
         */
        parser_stack(const storage_callbacks& provider, std::size_t max_frames) noexcept;

        /**
         * @brief Destroy live frames before releasing their raw storage.
         * @par Requirements
         * P033; J5-040; J5-048.
         * @note Cleanup is iterative and does not charge work.
         */
        ~parser_stack() noexcept;

        // Keep the operation-local stack at stable address.
        parser_stack(const parser_stack&) = delete;
        parser_stack& operator=(const parser_stack&) = delete;
        parser_stack(parser_stack&&) = delete;
        parser_stack& operator=(parser_stack&&) = delete;

        /**
         * @brief Push an externally held frame after checking limits and storage.
         * @par Requirements
         * P033; J5-046; J5-048; J5-049; J5-051.
         * @note frame must not reside inside this stack.
         * Failure preserves frames and the incoming frame.
         * Each push attempt passing the frame limit costs one work unit.
         * Successful work charges are not refunded after later failure.
         */
        [[nodiscard]] status_result push(parser_frame&& frame, work_budget& work) noexcept;

        /**
         * @brief Inspect the innermost open container.
         * @par Requirements
         * P033; J5-013.
         * @return Pointer to the top frame, or nullptr when empty.
         * @note Growth, removal or destruction can invalidate the pointer.
         */
        [[nodiscard]] parser_frame* top() noexcept;

        /**
         * @brief Inspect the innermost container without mutation.
         * @par Requirements
         * P033; J5-013.
         * @return Pointer to the top frame, or nullptr when empty.
         */
        [[nodiscard]] const parser_frame* top() const noexcept;

        /**
         * @brief Remove the innermost frame if present.
         * @par Requirements
         * P033; J5-040; J5-048.
         * @return True when a frame was removed.
         * @note Performs no allocation or work charge.
         */
        bool pop() noexcept;

        /**
         * @brief Destroy all frames while retaining reusable raw storage.
         * @par Requirements
         * P033; J5-040; J5-048.
         * @note Also works after work-budget exhaustion.
         */
        void clear() noexcept;

        /**
         * @brief Return the number of live frames.
         * @par Requirements
         * P033; J5-048.
         */
        [[nodiscard]] std::size_t size() const noexcept;
    };
  } // namespace detail
} // namespace adaptive_json5

#endif /* PARSER_STACK_HPP_ */
