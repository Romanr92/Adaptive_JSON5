#include "adaptive_json5/adaptive_json5.hpp"

#include <gtest/gtest.h>

namespace
{
  /* =================================== Test_P043_ParserOnly ==================================== */
  /*
   * Arrange: A translation unit that includes only the parser umbrella header.
   * Act: Check the schema entry header guard.
   * Expect: The umbrella pulls in no schema header (J5-068, ADR 0006 decision 1).
   */
  TEST(P043_ParserOnly, UmbrellaHeaderIncludesNoSchemaHeader)
  {
#ifdef ADAPTIVE_JSON5_SCHEMA_HPP_
    const bool schema_included = true;
#else
    const bool schema_included = false;
#endif
    EXPECT_FALSE(schema_included);
  }
} // namespace
