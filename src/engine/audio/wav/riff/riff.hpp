#ifndef ASRYX_ENGINE_AUDIO_WAV_RIFF_HPP
#define ASRYX_ENGINE_AUDIO_WAV_RIFF_HPP

#include "error.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
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
std::expected<std::uint16_t, asryx::Error> read_u16_le(const std::vector<std::uint8_t>& bytes,
                                                       size_t offset);
std::expected<std::uint32_t, asryx::Error> read_u32_le(const std::vector<std::uint8_t>& bytes,
                                                       size_t offset);
std::expected<size_t, asryx::Error> validate_riff_wave(const std::vector<std::uint8_t>& bytes);

} // namespace engine::audio

#endif // ASRYX_ENGINE_AUDIO_WAV_RIFF_HPP
