#include "engine/audio/wav/riff/riff.hpp"

#include <cstring>

namespace engine::audio {

bool has_range(const ByteRange& range)
{
  return range.offset <= range.total_size && range.length <= range.total_size - range.offset;
}

bool chunk_is(const std::vector<std::uint8_t>& bytes, const ChunkId& chunk)
{
  return has_range({.total_size = bytes.size(), .offset = chunk.offset, .length = 4}) &&
         std::memcmp(bytes.data() + chunk.offset, chunk.id, 4) == 0;
}

yx::Result<std::uint16_t> read_u16_le(const std::vector<std::uint8_t>& bytes, size_t offset)
{
  if (!has_range({.total_size = bytes.size(), .offset = offset, .length = 2})) {
    return yx::fail("invalid wav header: truncated 16-bit field");
  }

  return yx::ok(static_cast<std::uint16_t>(
      static_cast<std::uint16_t>(bytes[offset]) |
      static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[offset + 1]) << 8U)));
}

yx::Result<std::uint32_t> read_u32_le(const std::vector<std::uint8_t>& bytes, size_t offset)
{
  if (!has_range({.total_size = bytes.size(), .offset = offset, .length = 4})) {
    return yx::fail("invalid wav header: truncated 32-bit field");
  }

  return yx::ok(static_cast<std::uint32_t>(bytes[offset]) |
                (static_cast<std::uint32_t>(bytes[offset + 1]) << 8U) |
                (static_cast<std::uint32_t>(bytes[offset + 2]) << 16U) |
                (static_cast<std::uint32_t>(bytes[offset + 3]) << 24U));
}

yx::Result<size_t> validate_riff_wave(const std::vector<std::uint8_t>& bytes)
{
  if (bytes.size() < 12 || !chunk_is(bytes, {.offset = 0, .id = "RIFF"}) ||
      !chunk_is(bytes, {.offset = 8, .id = "WAVE"}))
  {
    return yx::fail("unsupported wav file: expected RIFF/WAVE");
  }

  const auto declared_size = read_u32_le(bytes, 4);
  if (!declared_size) {
    return yx::fail(declared_size.error());
  }

  const std::uint64_t declared_file_size = static_cast<std::uint64_t>(*declared_size) + 8U;
  if (declared_file_size != bytes.size()) {
    return yx::fail("invalid wav file: RIFF size does not match file size");
  }

  return yx::ok(bytes.size());
}

} // namespace engine::audio
