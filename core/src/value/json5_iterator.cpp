#include "adaptive_json5/json5_iterator.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/json5_value.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/value_view.hpp"

#include <optional>

namespace adaptive_json5
{
  /**
   * @brief Wrap traversal state or an error together with the document's scratch storage.
   * @par Requirements
   * J5-030; J5-040.
   * @note Only json5_value::children() creates iterators.
   * @par ID SWU-VALUE-180
   */
  json5_iterator::json5_iterator(result<value_iterator> state, const storage_callbacks& scratch) noexcept
      : m_state(state), m_scratch(scratch)
  {
  }

  /**
   * @brief End the walk; releases nothing, because the iterator owns no storage.
   * @par Requirements
   * J5-030; J5-040.
   * @note Defined here, not inline, so the destructor is always emitted in the library
   * and static analysis can check the class (MISRA C++:2023 Rule 15.0.1).
   */
  json5_iterator::~json5_iterator() noexcept = default;

  /**
   * @brief Return the next child, or an empty optional at the end.
   * @par Requirements
   * J5-030; J5-031; J5-039; J5-048; SSR-02.
   * @note Delegates to value_iterator::next(), which does constant work per call, so a full
   * walk is linear in the child count (issue #154).
   * @par ID SWU-VALUE-181
   */
  result<std::optional<json5_value>> json5_iterator::next() noexcept
  {
    using next_result = result<std::optional<json5_value>>;

    // A held error (from the value or from creation, e.g. wrong_type) is reported on every call.
    auto* state = m_state.value_if();
    if (state == nullptr)
    {
      return next_result::failure(*m_state.error_if());
    }

    // A failed step (damaged link) leaves the traversal position unchanged.
    const auto step = state->next();
    if (!step.has_value())
    {
      return next_result::failure(*step.error_if());
    }

    // An empty optional means the walk is complete; later calls stay empty.
    const auto& child = *step.value_if();
    if (!child.has_value())
    {
      return next_result::success(std::nullopt);
    }

    // Wrap the child view with the same scratch storage, through json5_value's private constructor.
    return next_result::success(json5_value{result<value_view>::success(*child), m_scratch});
  }
} // namespace adaptive_json5
