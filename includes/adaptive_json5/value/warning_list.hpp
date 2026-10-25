#ifndef ADAPTIVE_JSON5_WARNING_LIST_HPP_
#define ADAPTIVE_JSON5_WARNING_LIST_HPP_

#include "adaptive_json5/memory/owned_block.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/string_warning.hpp"

#include <cstddef>

namespace adaptive_json5
{
  /**
   * @brief Own every retained warning of a parsed document in source order.
   * @par Requirements
   * P039; J5-017; J5-041; J5-044; J5-046.
   * @note Growth is split into a fallible reserve and an append that cannot run out of storage,
   * so callers can secure capacity before committing state and record warnings afterwards.
   * Storage comes from the borrowed provider, which must outlive the list.
   * Moving transfers the entries and leaves the source empty; copying is unavailable.
   * Entries own no input bytes; offsets and lengths refer to the original input.
   */
  class [[nodiscard]] warning_list
  {
    private:
      storage_callbacks m_provider{};
      owned_block m_storage{};
      std::size_t m_size{};
      std::size_t m_capacity{};

    public:
      /**
       * @brief Construct an empty list without a storage provider.
       * @par Requirements
       * P039; J5-046.
       * @note Performs no allocation; only reserve(0) can succeed.
       */
      warning_list() noexcept = default;

      /**
       * @brief Construct an empty list that allocates through a borrowed provider.
       * @par Requirements
       * P039; J5-046.
       * @param provider Fallible storage provider that must outlive the list.
       * @note Performs no allocation.
       */
      explicit warning_list(const storage_callbacks& provider) noexcept;

      /**
       * @brief Transfer all entries and storage, leaving the source empty.
       * @par Requirements
       * P039; J5-040.
       * @param other List whose entries are transferred.
       * @note Performs no allocation; the source keeps its provider and reports size zero.
       */
      warning_list(warning_list&& other) noexcept;

      /**
       * @brief Release entry storage through its original provider.
       * @par Requirements
       * P039; J5-040.
       */
      ~warning_list() noexcept = default;

      warning_list(const warning_list&) = delete;
      warning_list& operator=(const warning_list&) = delete;
      warning_list& operator=(warning_list&&) = delete;

      /**
       * @brief Ensure capacity for additional entries without changing the entries.
       * @par Requirements
       * P039; J5-003; J5-046.
       * @param additional Number of further entries the caller will append.
       * @return Success, or an explicit resource/storage failure that leaves the list unchanged.
       * @note Grows to at least double the old capacity; the old allocation is retained during copying.
       */
      [[nodiscard]] status_result reserve(std::size_t additional) noexcept;

      /**
       * @brief Append one entry into previously reserved capacity.
       * @par Requirements
       * P039; J5-003; J5-017.
       * @param warning Entry to store, in source order.
       * @return Success, or wrong_type_or_access when no reserved capacity remains.
       * @note Never allocates, so it is safe inside a warning observer.
       */
      [[nodiscard]] status_result append_reserved(const string_warning& warning) noexcept;

      /**
       * @brief Report the number of retained entries.
       * @par Requirements
       * P039; J5-017.
       */
      [[nodiscard]] std::size_t size() const noexcept;

      /**
       * @brief Read one entry by position.
       * @par Requirements
       * P039; J5-003; J5-017; J5-044.
       * @param index Zero-based position in source order.
       * @return A copy of the entry, or wrong_type_or_access when index is out of range.
       */
      [[nodiscard]] result<string_warning> at(std::size_t index) const noexcept;
  };
} // namespace adaptive_json5

#endif /* ADAPTIVE_JSON5_WARNING_LIST_HPP_ */
