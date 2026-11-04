// Shared by the example programs: read a whole file into caller storage.
#ifndef READ_FILE_HPP_
#define READ_FILE_HPP_

#include <cstddef>
#include <cstdio>

/**
 * @brief Read a complete file into a caller buffer.
 * @param path File to read.
 * @param buffer Destination storage.
 * @param capacity Size of buffer in bytes.
 * @param size Receives the number of bytes read.
 * @return False if the file cannot be read or does not fit in buffer.
 */
inline bool read_file(const char* path, char* buffer, std::size_t capacity, std::size_t& size)
{
  std::FILE* const file = std::fopen(path, "rb");
  if (file == nullptr)
  {
    return false;
  }

  size = std::fread(buffer, 1, capacity, file);
  const bool complete = (std::ferror(file) == 0) && (size < capacity);
  std::fclose(file);

  return complete;
}

#endif /* READ_FILE_HPP_ */
