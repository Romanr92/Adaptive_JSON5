// Report why a JSON5 file was rejected. Errors are values; nothing is thrown.
#include "adaptive_json5/adaptive_json5.hpp"
#include "read_file.hpp"

#include <cstddef>
#include <cstdio>

namespace
{
  // Relative to the repository root; run the program from there. This file is intentionally invalid.
  constexpr const char* c_path = "examples/data/handle_error.json5";

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

  const adaptive_json5::input_view input{file_text, size};

  const auto parsed = adaptive_json5::json5_document::parse(input, memory, sizeof(memory));
  if (parsed.has_value())
  {
    std::fprintf(stderr, "%s should be rejected\n", c_path);
    return 1;
  }

  const adaptive_json5::error& failure = *parsed.error_if();

  // describe() writes a readable message with its line and column; it never allocates.
  char message[128];
  const auto described = adaptive_json5::describe(failure, input, message);
  std::printf("rejected: %s\n", described.has_value() ? message : "invalid input");

  // The category is available for program logic.
  if (failure.code == adaptive_json5::error_code::syntax)
  {
    std::puts("fix the input; parsing it again gives the same error");
  }

  return 0;
}
