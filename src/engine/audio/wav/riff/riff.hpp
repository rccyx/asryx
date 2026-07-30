#ifndef ASRYX_ENGINE_AUDIO_WAV_RIFF_HPP
#define ASRYX_ENGINE_AUDIO_WAV_RIFF_HPP

#include "error.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace engine::audio {

struct ByteRange
{
  size_t total_size = 0;
  size_t offset = 0;
  size_t length = 0;
};

struct ChunkId
{
  size_t offset = 0;
  const char* id = nullptr;
};

bool has_range(const ByteRange& range);
bool chunk_is(const std::vector<std::uint8_t>& bytes, const ChunkId& chunk);
yx::Result<std::uint16_t> read_u16_le(const std::vector<std::uint8_t>& bytes, size_t offset);
yx::Result<std::uint32_t> read_u32_le(const std::vector<std::uint8_t>& bytes, size_t offset);
yx::Result<size_t> validate_riff_wave(const std::vector<std::uint8_t>& bytes);

} // namespace engine::audio

#endif // ASRYX_ENGINE_AUDIO_WAV_RIFF_HPP
