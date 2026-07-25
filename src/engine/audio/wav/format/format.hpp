#ifndef ASRYX_ENGINE_AUDIO_WAV_FORMAT_HPP
#define ASRYX_ENGINE_AUDIO_WAV_FORMAT_HPP

#include "error.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <vector>

namespace engine::audio {

struct WavFormat
{
  std::uint16_t audio_format = 0;
  std::uint16_t channels = 0;
  std::uint32_t sample_rate = 0;
  std::uint32_t byte_rate = 0;
  std::uint16_t block_align = 0;
  std::uint16_t bits_per_sample = 0;
  std::uint16_t valid_bits_per_sample = 0;
};

struct WavFormatRead
{
  const std::vector<std::uint8_t>* bytes = nullptr;
  size_t offset = 0;
  size_t chunk_size = 0;
};

std::expected<WavFormat, asryx::Error> read_wav_format(const WavFormatRead& read);
std::expected<void, asryx::Error> validate_wav_format(const WavFormat& format);

} // namespace engine::audio

#endif // ASRYX_ENGINE_AUDIO_WAV_FORMAT_HPP
