#ifndef STORAGE_HPP_
#define STORAGE_HPP_

#include <cstddef>

namespace adaptive_json5
{
  /**
   * @brief Request aligned raw storage from the caller.
   * @par Requirements
   * P003; J5-046; J5-101.
   * @return Aligned storage, or nullptr when the request cannot be fulfilled.
   * @note bytes and alignment describe the allocation request.
   * The callback must not throw or terminate on allocation failure.
   */
  using allocate_function = void* (*)(void* context, std::size_t bytes, std::size_t alignment) noexcept;

  /**
   * @brief Release storage through its original provider.
   * @par Requirements
   * P003; J5-040; J5-051; J5-101.
   * @note address, bytes and alignment must match an outstanding
   * successful allocation. Release must not throw or allocate.
   */
  using deallocate_function = void (*)(void* context, void* address, std::size_t bytes, std::size_t alignment) noexcept;

  /**
   * @brief Borrow a caller-owned storage provider.
   * @par Requirements
   * P003; J5-040; J5-046; J5-101.
   * @note Provider state must outlive every allocation using it.
   * context may be nullptr for a stateless provider.
   * Default construction supplies no usable provider.
   */
  struct storage_callbacks
  {
      void* context = nullptr;
      allocate_function allocate = nullptr;
      deallocate_function deallocate = nullptr;
  };
} // namespace adaptive_json5

#endif /* STORAGE_HPP_ */
