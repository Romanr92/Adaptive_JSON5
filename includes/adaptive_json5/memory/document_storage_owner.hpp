#ifndef DOCUMENT_STORAGE_OWNER_HPP_
#define DOCUMENT_STORAGE_OWNER_HPP_

namespace adaptive_json5
{
  class accounted_storage;

  namespace detail
  {
    /**
     * @brief Own a document's stable storage control block.
     * @par Requirements
     * P003; P037; J5-040; J5-046; J5-051; J5-101.
     * @note Representation support, not a supported direct consumer API.
     * The block and its accounting must remain at their original addresses.
     * Destroy every allocation before destroying this owner.
     */
    class [[nodiscard]] document_storage_owner
    {
      private:
        void* m_block{};
        accounted_storage* m_accounting{};
        void (*m_release)(void*) noexcept {};

      public:
        /**
         * @brief Adopt a fully constructed storage control block.
         * @par Requirements
         * J5-040; J5-046.
         * @note block, accounting and release must be nonnull.
         * accounting belongs to block. release must destroy the block without
         * throwing or allocating; caller-buffer storage must not free its buffer.
         */
        document_storage_owner(void* block, accounted_storage* accounting, void (*release)(void*) noexcept) noexcept;

        /**
         * @brief Release the control block after its allocations are destroyed.
         * @par Requirements
         * J5-040; J5-051.
         */
        ~document_storage_owner() noexcept;

        /**
         * @brief Transfer ownership without relocating the control block.
         * @par Requirements
         * J5-040; J5-051.
         * @note Leaves other empty.
         */
        document_storage_owner(document_storage_owner&& other) noexcept;

        /**
         * @brief Prevent two owners from releasing the same control block.
         * @par Requirements
         * J5-040.
         */
        document_storage_owner(const document_storage_owner&) = delete;

        /**
         * @brief Prevent copying ownership.
         * @par Requirements
         * J5-040.
         */
        document_storage_owner& operator=(const document_storage_owner&) = delete;

        /**
         * @brief Prevent replacing storage while document allocations remain.
         * @par Requirements
         * J5-040; J5-051.
         */
        document_storage_owner& operator=(document_storage_owner&&) = delete;

        /**
         * @brief Borrow the stable accounting object.
         * @par Requirements
         * J5-040; J5-046.
         * @return Accounting pointer, or nullptr after ownership was moved away.
         */
        [[nodiscard]] accounted_storage* accounting() const noexcept;
    };

  } // namespace detail
} // namespace adaptive_json5

#endif /* DOCUMENT_STORAGE_OWNER_HPP_ */
