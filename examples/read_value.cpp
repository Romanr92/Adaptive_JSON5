// Read typed values from a JSON5 file, by member name and array index.
#include "adaptive_json5/adaptive_json5.hpp"
#include "read_file.hpp"

#include <cinttypes>
#include <cstddef>
#include <cstdio>

namespace
{
  // Relative to the repository root; run the program from there.
  constexpr const char* c_path = "examples/data/read_value.json5";

  char file_text[4096];
  alignas(std::max_align_t) std::byte memory[16384];
} // namespace

// The C++ standard requires main to return int ([basic.start.main]), so int stays here.
int main()
{
  std::size_t size{};
  if (!read_file(c_path, file_text, sizeof(file_text), size))
  {
    std::fprintf(stderr, "cannot read %s, or it is too large\n", c_path);
    return 2;
  }

  const auto parsed = adaptive_json5::json5_document::parse({file_text, size}, memory, sizeof(memory));
  if (!parsed.has_value())
  {
    std::fprintf(stderr, "%s should parse\n", c_path);
    return 1;
  }

  const auto root = parsed.value_if()->root();

  // A string is exported as UTF-8 into caller storage.
  char name[32];
  const auto text = root.member("name").as_string(name, sizeof(name));
  const auto port = root.member("port").as_int64();
  const auto ratio = root.member("ratio").as_double();
  const auto enabled = root.member("enabled").as_bool();
  // member() and at() can be chained; a failure in the chain is reported by the final read.
  const auto second_range = root.member("ranges").at(1).as_int64();
  const auto absent = root.member("absent").as_int64();

  if (!text.has_value() || !port.has_value() || !ratio.has_value() || !enabled.has_value() || !second_range.has_value())
  {
    std::fputs("a value has an unexpected type\n", stderr);
    return 1;
  }

  // The C++ standard (through C's printf) requires the %.*s precision argument to be exactly int,
  // so int stays here.
  std::printf("name: %.*s\n", static_cast<int>(text.value_if()->size()), text.value_if()->data());
  std::printf("port: %" PRId64 "\n", *port.value_if());
  std::printf("ratio: %g\n", *ratio.value_if());
  std::printf("enabled: %s\n", *enabled.value_if() ? "yes" : "no");
  std::printf("ranges[1]: %" PRId64 "\n", *second_range.value_if());
  std::printf("absent member reported: %s\n", absent.has_value() ? "no" : "yes");

  return 0;
}
