#include "engine/audio/wav/format/format.hpp"

#include "engine/audio/wav/riff/riff.hpp"
#include "engine/audio/wav/wav.hpp"

#include <array>
#include <cstring>

namespace engine::audio {

namespace {

constexpr std::uint16_t WAV_PCM = 1;
constexpr std::uint16_t WAV_EXTENSIBLE = 65534;
constexpr std::uint16_t WAV_EXTENSIBLE_EXTRA_SIZE = 22;
constexpr size_t WAV_BASE_FORMAT_SIZE = 16;
constexpr size_t WAV_FORMAT_EX_SIZE = 18;
constexpr size_t WAV_EXTENSIBLE_FORMAT_SIZE = 40;

constexpr std::array<std::uint8_t, 16> PCM_SUBFORMAT_GUID = {
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71};

std::expected<std::uint16_t, asryx::Error> _read_wav_extensible_format(const WavFormatRead& read)
{
  const auto& bytes = *read.bytes;

  if (read.chunk_size < WAV_EXTENSIBLE_FORMAT_SIZE) {
    return asryx::fail("invalid wav fmt chunk: truncated WAVEFORMATEXTENSIBLE");
  }

  const auto extra_size = read_u16_le(bytes, read.offset + 16);
  const auto valid_bits = read_u16_le(bytes, read.offset + 18);
  if (!extra_size)
    return std::unexpected(extra_size.error());
  if (!valid_bits)
    return std::unexpected(valid_bits.error());

  if (*extra_size < WAV_EXTENSIBLE_EXTRA_SIZE ||
      WAV_FORMAT_EX_SIZE + static_cast<size_t>(*extra_size) > read.chunk_size)
  {
    return asryx::fail("invalid wav fmt chunk: invalid extensible extra size");
  }

  if (std::memcmp(bytes.data() + read.offset + 24, PCM_SUBFORMAT_GUID.data(),
                  PCM_SUBFORMAT_GUID.size()) != 0)
  {
    return asryx::fail("unsupported wav format: extensible subtype is not PCM");
  }

  return *valid_bits;
}

} // namespace

std::expected<WavFormat, asryx::Error> read_wav_format(const WavFormatRead& read)
{
  const auto& bytes = *read.bytes;

  if (read.chunk_size < WAV_BASE_FORMAT_SIZE ||
      !has_range({.total_size = bytes.size(), .offset = read.offset, .length = read.chunk_size}))
  {
    return asryx::fail("invalid wav fmt chunk");
  }

  const auto audio_format = read_u16_le(bytes, read.offset);
  const auto channels = read_u16_le(bytes, read.offset + 2);
  const auto sample_rate = read_u32_le(bytes, read.offset + 4);
  const auto byte_rate = read_u32_le(bytes, read.offset + 8);
  const auto block_align = read_u16_le(bytes, read.offset + 12);
  const auto bits_per_sample = read_u16_le(bytes, read.offset + 14);

  if (!audio_format)
    return std::unexpected(audio_format.error());
  if (!channels)
    return std::unexpected(channels.error());
  if (!sample_rate)
    return std::unexpected(sample_rate.error());
  if (!byte_rate)
    return std::unexpected(byte_rate.error());
  if (!block_align)
    return std::unexpected(block_align.error());
  if (!bits_per_sample)
    return std::unexpected(bits_per_sample.error());

  std::uint16_t valid_bits_per_sample = *bits_per_sample;
  if (*audio_format == WAV_EXTENSIBLE) {
    const auto valid_bits = _read_wav_extensible_format(read);
    if (!valid_bits) {
      return std::unexpected(valid_bits.error());
    }

    valid_bits_per_sample = *valid_bits;
  }
  else if (*audio_format != WAV_PCM) {
    return asryx::fail("unsupported wav format: expected PCM");
  }

  return WavFormat{.audio_format = *audio_format,
                   .channels = *channels,
                   .sample_rate = *sample_rate,
                   .byte_rate = *byte_rate,
                   .block_align = *block_align,
                   .bits_per_sample = *bits_per_sample,
                   .valid_bits_per_sample = valid_bits_per_sample};
}

std::expected<void, asryx::Error> validate_wav_format(const WavFormat& format)
{
  if (format.channels != WAV_CHANNELS || format.sample_rate != WAV_SAMPLE_RATE ||
      format.bits_per_sample != WAV_BITS_PER_SAMPLE ||
      format.valid_bits_per_sample != WAV_BITS_PER_SAMPLE)
  {
    return asryx::fail("unsupported wav format: expected 16 kHz mono s16 PCM");
  }

  if (format.block_align != WAV_BLOCK_ALIGN) {
    return asryx::fail("invalid wav format: block alignment does not match mono s16 PCM");
  }

  if (format.byte_rate != WAV_BYTE_RATE) {
    return asryx::fail(
        "invalid wav format: byte rate does not match sample rate and block alignment");
  }

  return {};
}

} // namespace engine::audio
