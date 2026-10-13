#ifndef ADAPTIVE_JSON5_ERROR_HPP_
#define ADAPTIVE_JSON5_ERROR_HPP_

namespace adaptive_json5
{
  enum class error_code
  {
    syntax = 1,
    encoding = 2,
    duplicate_policy = 3,
    wrong_type_or_access = 4,
    numeric_conversion = 5,
    resource_limit = 6,
    allocation_failure = 7
  };
} // namespace adaptive_json5

#endif /* ADAPTIVE_JSON5_ERROR_HPP_ */
