#ifndef OPTIONS_HPP_
#define OPTIONS_HPP_

namespace adaptive_json5
{
  /**
   * @brief Select the input language.
   * @par Requirements
   * J5-005.
   * @note Selection does not permit additional nonstandard syntax.
   */
  enum class syntax_mode
  {
    json5,
    strict_json
  };

  /**
   * @brief Configure the reader's syntax selection.
   * @par Requirements
   * J5-005.
   * @note JSON5 is the default. Strict JSON requires explicit selection.
   * Storage, resource limits and other policies are specified separately.
   */
  struct parse_options
  {
      syntax_mode syntax = syntax_mode::json5;
  };
} // namespace adaptive_json5
#endif /* OPTIONS_HPP_ */
