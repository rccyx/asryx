#include "engine/audio/pcm/pcm.hpp"

#include <cstdint>

namespace engine::audio {

namespace {

constexpr float PCM16_SCALE = 32768.0F;

struct ByteRange
{
  size_t total_size = 0;
  size_t offset = 0;
  size_t length = 0;
};

bool _has_range(const ByteRange& range)
{
  return range.offset <= range.total_size && range.length <= range.total_size - range.offset;
}

} // namespace

std::expected<std::vector<float>, asryx::Error> decode_pcm16(const WavSampleData& data)
{
  if (data.bytes == nullptr ||
      !_has_range({.total_size = data.bytes->size(), .offset = data.offset, .length = data.size}))
  {
    return asryx::fail("invalid wav data range");
  }

  if (data.size == 0 || data.size % WAV_BLOCK_ALIGN != 0) {
    return asryx::fail("invalid wav data: expected complete mono s16 samples");
  }

  const size_t sample_count = data.size / WAV_BLOCK_ALIGN;
  std::vector<float> samples(sample_count);

  for (size_t sample_index = 0; sample_index < sample_count; ++sample_index) {
    const size_t byte_index = data.offset + (sample_index * WAV_BLOCK_ALIGN);
    const auto low = static_cast<std::uint16_t>((*data.bytes)[byte_index]);
    const auto high = static_cast<std::uint16_t>((*data.bytes)[byte_index + 1]);
    const auto raw = static_cast<std::uint16_t>(low | static_cast<std::uint16_t>(high << 8U));

    const std::int32_t signed_sample =
        raw >= 0x8000U ? static_cast<std::int32_t>(raw) - 0x10000 : static_cast<std::int32_t>(raw);

    samples[sample_index] = static_cast<float>(signed_sample) / PCM16_SCALE;
  }

  return samples;
}

} // namespace engine::audio
