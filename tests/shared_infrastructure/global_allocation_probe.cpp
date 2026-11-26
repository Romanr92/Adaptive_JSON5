// SPDX-License-Identifier: Apache-2.0
// Host-only replacements: link exactly once into each allocation-probe executable.
#include <cstddef>
#include <cstdlib>
#include <new>
#if defined(_WIN32)
  #include <malloc.h> // IWYU pragma: keep (_aligned_malloc, _aligned_free)
#endif

namespace adaptive_json5_test
{
  // Each executable records only allocations within its own observation window.
  void record_heap_allocation() noexcept;
} // namespace adaptive_json5_test

namespace
{
  /**
   * @brief Allocate host test memory using a matching C allocation family.
   * @par Requirements
   * Issue #132; J5-097.
   * @note Nothrow forms preserve allocation failure; ordinary forms retain the
   * existing probe's abort-on-host-exhaustion policy in both exception modes.
   */
  void* allocate_probe(std::size_t size, std::size_t alignment, bool nothrow) noexcept
  {
    adaptive_json5_test::record_heap_allocation();
    size = size == 0U ? 1U : size;
    void* memory = nullptr;
    if (alignment == 0U)
    {
      memory = std::malloc(size);
    }
    else
    {
#if defined(_WIN32)
      memory = _aligned_malloc(size, alignment);
#else
      if (posix_memalign(&memory, alignment, size) != 0)
      {
        memory = nullptr;
      }
#endif
    }
    if ((memory == nullptr) && !nothrow)
    {
      std::abort();
    }
    return memory;
  }

  /**
   * @brief Release an aligned host allocation through its matching allocator.
   * @par Requirements
   * Issue #132; J5-097.
   */
  void release_aligned(void* memory) noexcept
  {
#if defined(_WIN32)
    _aligned_free(memory);
#else
    std::free(memory);
#endif
  }
} // namespace

/** @brief Replace host allocation consistently. @par Requirements Issue #132; J5-097. */
void* operator new(std::size_t size)
{
  return allocate_probe(size, 0U, false);
}
/** @brief Replace host allocation consistently. @par Requirements Issue #132; J5-097. */
void* operator new(std::size_t size, const std::nothrow_t&) noexcept
{
  return allocate_probe(size, 0U, true);
}
/** @brief Replace matching host deallocation. @par Requirements Issue #132; J5-097. */
void operator delete(void* memory) noexcept
{
  std::free(memory);
}
/** @brief Replace matching host deallocation. @par Requirements Issue #132; J5-097. */
void operator delete(void* memory, std::size_t) noexcept
{
  std::free(memory);
}
/** @brief Replace matching host deallocation. @par Requirements Issue #132; J5-097. */
void operator delete(void* memory, const std::nothrow_t&) noexcept
{
  std::free(memory);
}
/** @brief Replace host allocation consistently. @par Requirements Issue #132; J5-097. */
void* operator new(std::size_t size, std::align_val_t alignment)
{
  return allocate_probe(size, static_cast<std::size_t>(alignment), false);
}
/** @brief Replace host allocation consistently. @par Requirements Issue #132; J5-097. */
void* operator new(std::size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept
{
  return allocate_probe(size, static_cast<std::size_t>(alignment), true);
}
/** @brief Replace matching host deallocation. @par Requirements Issue #132; J5-097. */
void operator delete(void* memory, std::align_val_t) noexcept
{
  release_aligned(memory);
}
/** @brief Replace matching host deallocation. @par Requirements Issue #132; J5-097. */
void operator delete(void* memory, std::size_t, std::align_val_t) noexcept
{
  release_aligned(memory);
}
/** @brief Replace matching host deallocation. @par Requirements Issue #132; J5-097. */
void operator delete(void* memory, std::align_val_t, const std::nothrow_t&) noexcept
{
  release_aligned(memory);
}
/** @brief Replace host allocation consistently. @par Requirements Issue #132; J5-097. */
void* operator new[](std::size_t size)
{
  return allocate_probe(size, 0U, false);
}
/** @brief Replace host allocation consistently. @par Requirements Issue #132; J5-097. */
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept
{
  return allocate_probe(size, 0U, true);
}
/** @brief Replace matching host deallocation. @par Requirements Issue #132; J5-097. */
void operator delete[](void* memory) noexcept
{
  std::free(memory);
}
/** @brief Replace matching host deallocation. @par Requirements Issue #132; J5-097. */
void operator delete[](void* memory, std::size_t) noexcept
{
  std::free(memory);
}
/** @brief Replace matching host deallocation. @par Requirements Issue #132; J5-097. */
void operator delete[](void* memory, const std::nothrow_t&) noexcept
{
  std::free(memory);
}
/** @brief Replace host allocation consistently. @par Requirements Issue #132; J5-097. */
void* operator new[](std::size_t size, std::align_val_t alignment)
{
  return allocate_probe(size, static_cast<std::size_t>(alignment), false);
}
/** @brief Replace host allocation consistently. @par Requirements Issue #132; J5-097. */
void* operator new[](std::size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept
{
  return allocate_probe(size, static_cast<std::size_t>(alignment), true);
}
/** @brief Replace matching host deallocation. @par Requirements Issue #132; J5-097. */
void operator delete[](void* memory, std::align_val_t) noexcept
{
  release_aligned(memory);
}
/** @brief Replace matching host deallocation. @par Requirements Issue #132; J5-097. */
void operator delete[](void* memory, std::size_t, std::align_val_t) noexcept
{
  release_aligned(memory);
}
/** @brief Replace matching host deallocation. @par Requirements Issue #132; J5-097. */
void operator delete[](void* memory, std::align_val_t, const std::nothrow_t&) noexcept
{
  release_aligned(memory);
}
