#ifndef ASRYX_ENGINE_AUDIO_WAV_HPP
#define ASRYX_ENGINE_AUDIO_WAV_HPP

#include "error.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <vector>

namespace engine::audio {

constexpr std::uint32_t WAV_SAMPLE_RATE = 16000;
constexpr std::uint16_t WAV_CHANNELS = 1;
constexpr std::uint16_t WAV_BITS_PER_SAMPLE = 16;
constexpr std::uint16_t WAV_BLOCK_ALIGN =
    WAV_CHANNELS * static_cast<std::uint16_t>(WAV_BITS_PER_SAMPLE / 8U);
constexpr std::uint32_t WAV_BYTE_RATE = WAV_SAMPLE_RATE * WAV_BLOCK_ALIGN;

struct WavSampleData
{
  const std::vector<std::uint8_t>* bytes = nullptr;
  size_t offset = 0;
  size_t size = 0;
};

std::expected<WavSampleData, asryx::Error> parse_pcm16_wav(const std::vector<std::uint8_t>& bytes);

} // namespace engine::audio

#endif // ASRYX_ENGINE_AUDIO_WAV_HPP
