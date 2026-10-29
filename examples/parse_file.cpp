// Read a JSON5 file, parse it and report the outcome.
#include "adaptive_json5/adaptive_json5.hpp"
#include "read_file.hpp"

#include <cstddef>
#include <cstdio>

namespace
{
  // Relative to the repository root; run the program from there.
  constexpr const char* c_path = "examples/data/parse_file.json5";
  constexpr std::size_t c_max_file_bytes = 16384;

  char file_text[c_max_file_bytes];
  alignas(std::max_align_t) unsigned char memory[65536];
} // namespace

int main()
{
  std::size_t size{};

  if (!read_file(c_path, file_text, sizeof(file_text), size))
  {
    std::fprintf(stderr, "cannot read %s, or it is too large\n", c_path);
    return 2;
  }

  const adaptive_json5::input_view input{file_text, size};
  const auto parsed = adaptive_json5::json5_document::parse(input, memory, sizeof(memory));
  if (!parsed.has_value())
  {
    char message[128];
    const auto described = adaptive_json5::describe(*parsed.error_if(), input, message);
    std::printf("%s: %s\n", c_path, described.has_value() ? message : "invalid input");
    return 1;
  }

  const auto& document = *parsed.value_if();
  std::printf("%s: valid, %zu warning(s), %zu bytes used\n", c_path, document.warning_count(), document.peak_bytes());
  return 0;
}
