#ifndef ADAPTIVE_JSON5_HPP_
#define ADAPTIVE_JSON5_HPP_

/**
 * @file
 * @brief Include the complete embedded Adaptive JSON5 parser API with one header.
 * @par Requirements
 * J5-092; J5-101.
 * @note Start with json5_document (parse into a caller buffer, navigate through
 * json5_value, describe errors and warnings). Every other header here is the
 * advanced API: custom storage, limits, warning policy, exact numbers and
 * conversions, explicit-storage parsing with parse_document.
 * Does not include adaptive_json5/hosted/parse.hpp: that header belongs to the
 * optional AdaptiveJSON5::hosted library and is installed only when it is built.
 * Includes no private header and adds no declarations of its own.
 */

#include "adaptive_json5/diagnostic_format.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/input.hpp"
#include "adaptive_json5/json5_document.hpp"
#include "adaptive_json5/json5_value.hpp"
#include "adaptive_json5/memory/accounted_storage.hpp"
#include "adaptive_json5/memory/buffer_arena.hpp"
#include "adaptive_json5/memory/document_storage_owner.hpp"
#include "adaptive_json5/memory/owned_block.hpp"
#include "adaptive_json5/memory/resource_limits.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/parser.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/syntax_error_reason.hpp"
#include "adaptive_json5/text_position.hpp"
#include "adaptive_json5/value/decoded_name_equal.hpp"
#include "adaptive_json5/value/decoded_string.hpp"
#include "adaptive_json5/value/document.hpp"
#include "adaptive_json5/value/floating_conversion.hpp"
#include "adaptive_json5/value/floating_conversion_policy.hpp"
#include "adaptive_json5/value/integer_conversion.hpp"
#include "adaptive_json5/value/member_lookup.hpp"
#include "adaptive_json5/value/number_comparison.hpp"
#include "adaptive_json5/value/number_kind.hpp"
#include "adaptive_json5/value/numeric_conversion.hpp"
#include "adaptive_json5/value/numeric_conversion_reason.hpp"
#include "adaptive_json5/value/owned_number.hpp"
#include "adaptive_json5/value/retained_source.hpp"
#include "adaptive_json5/value/string_warning.hpp"
#include "adaptive_json5/value/utf8_export.hpp"
#include "adaptive_json5/value/value_index.hpp"
#include "adaptive_json5/value/value_iterator.hpp"
#include "adaptive_json5/value/value_kind.hpp"
#include "adaptive_json5/value/value_view.hpp"
#include "adaptive_json5/value/warning_list.hpp"

#endif /* ADAPTIVE_JSON5_HPP_ */
