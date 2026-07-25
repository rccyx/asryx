#include "engine/audio/file/file.hpp"

#include <fstream>
#include <limits>

namespace engine::audio {

std::expected<std::vector<std::uint8_t>, asryx::Error> read_audio_file(const std::string& path)
{
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    return asryx::fail("failed to open wav file: " + path);
  }

  const std::streamoff end = file.tellg();
  if (end < 0) {
    return asryx::fail("failed to determine wav file size: " + path);
  }

  const auto unsigned_size = static_cast<std::uintmax_t>(end);
  const auto max_stream_size =
      static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max());

  if (unsigned_size > max_stream_size) {
    return asryx::fail("wav file is too large to read: " + path);
  }

  const auto size = static_cast<size_t>(unsigned_size);
  if (size == 0) {
    return asryx::fail("wav file is empty: " + path);
  }

  file.seekg(0, std::ios::beg);
  if (!file) {
    return asryx::fail("failed to seek wav file: " + path);
  }

  std::vector<char> raw_bytes(size);
  const auto stream_size = static_cast<std::streamsize>(size);
  file.read(raw_bytes.data(), stream_size);

  if (file.gcount() != stream_size || file.bad()) {
    return asryx::fail("failed to read complete wav file: " + path);
  }

  std::vector<std::uint8_t> bytes;
  bytes.reserve(size);

  for (const char byte : raw_bytes) {
    bytes.push_back(static_cast<std::uint8_t>(byte));
  }

  return bytes;
}

} // namespace engine::audio
