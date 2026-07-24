#include "engine/audio/audio.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace engine::audio {

namespace {

constexpr std::uint16_t WAV_PCM = 1;
constexpr std::uint16_t WAV_EXTENSIBLE = 65534;
constexpr std::uint32_t WAV_SAMPLE_RATE = 16000;
constexpr float PCM16_SCALE = 32768.0F;

struct WavFormat
{
  std::uint16_t audio_format = 0;
  std::uint16_t channels = 0;
  std::uint32_t sample_rate = 0;
  std::uint16_t bits_per_sample = 0;
};

struct WavDataChunk
{
  size_t offset = 0;
  size_t size = 0;
};

struct WavChunks
{
  WavFormat format;
  WavDataChunk sample_data;
};

struct Pcm16Data
{
  const std::vector<std::uint8_t>* bytes = nullptr;
  size_t offset = 0;
  size_t size = 0;
};

bool _chunk_is(const std::vector<std::uint8_t>& bytes, size_t offset, const char* id)
{
  return offset + 4 <= bytes.size() && std::memcmp(bytes.data() + offset, id, 4) == 0;
}

std::expected<std::uint16_t, asryx::Error> _read_u16_le(const std::vector<std::uint8_t>& bytes,
                                                        size_t offset)
{
  if (offset + 2 > bytes.size()) {
    return asryx::fail("invalid wav header");
  }

  return static_cast<std::uint16_t>(bytes[offset]) |
         static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[offset + 1]) << 8U);
}

std::expected<std::uint32_t, asryx::Error> _read_u32_le(const std::vector<std::uint8_t>& bytes,
                                                        size_t offset)
{
  if (offset + 4 > bytes.size()) {
    return asryx::fail("invalid wav header");
  }

  return static_cast<std::uint32_t>(bytes[offset]) |
         (static_cast<std::uint32_t>(bytes[offset + 1]) << 8U) |
         (static_cast<std::uint32_t>(bytes[offset + 2]) << 16U) |
         (static_cast<std::uint32_t>(bytes[offset + 3]) << 24U);
}

std::expected<std::vector<std::uint8_t>, asryx::Error> _read_file(const std::string& path)
{
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) {
    return asryx::fail("failed to open wav file: " + path);
  }

  std::vector<std::uint8_t> bytes;
  std::transform(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>{},
                 std::back_inserter(bytes),
                 [](char byte) { return static_cast<std::uint8_t>(byte); });
  return bytes;
}

std::expected<void, asryx::Error> _validate_wav_container(const std::vector<std::uint8_t>& bytes)
{
  if (bytes.size() < 44 || !_chunk_is(bytes, 0, "RIFF") || !_chunk_is(bytes, 8, "WAVE")) {
    return asryx::fail("unsupported wav file: expected RIFF/WAVE");
  }

  return {};
}

std::expected<void, asryx::Error> _validate_wav_format(const WavFormat& format)
{
  const bool supported_format =
      format.audio_format == WAV_PCM || format.audio_format == WAV_EXTENSIBLE;
  if (!supported_format) {
    return asryx::fail("unsupported wav format: expected PCM");
  }

  if (format.channels != 1 || format.sample_rate != WAV_SAMPLE_RATE || format.bits_per_sample != 16)
  {
    return asryx::fail("unsupported wav format: expected 16 kHz mono s16 PCM");
  }

  return {};
}

std::expected<WavFormat, asryx::Error> _read_wav_format(const std::vector<std::uint8_t>& bytes,
                                                        size_t offset)
{
  const auto audio_format = _read_u16_le(bytes, offset);
  const auto channels = _read_u16_le(bytes, offset + 2);
  const auto sample_rate = _read_u32_le(bytes, offset + 4);
  const auto bits_per_sample = _read_u16_le(bytes, offset + 14);
  if (!audio_format)
    return std::unexpected(audio_format.error());
  if (!channels)
    return std::unexpected(channels.error());
  if (!sample_rate)
    return std::unexpected(sample_rate.error());
  if (!bits_per_sample)
    return std::unexpected(bits_per_sample.error());

  return WavFormat{.audio_format = *audio_format,
                   .channels = *channels,
                   .sample_rate = *sample_rate,
                   .bits_per_sample = *bits_per_sample};
}

WavDataChunk _read_wav_data_chunk(const std::vector<std::uint8_t>& bytes, size_t offset,
                                  std::uint32_t declared_size)
{
  const size_t remaining_bytes = bytes.size() - offset;
  const size_t size =
      declared_size == 0 || declared_size > remaining_bytes ? remaining_bytes : declared_size;

  return {.offset = offset, .size = size};
}

std::expected<WavChunks, asryx::Error> _read_wav_chunks(const std::vector<std::uint8_t>& bytes)
{
  bool found_fmt = false;
  bool found_sample_data = false;

  WavChunks chunks;
  size_t offset = 12;

  while (offset + 8 <= bytes.size()) {
    const auto declared_size = _read_u32_le(bytes, offset + 4);
    if (!declared_size) {
      return std::unexpected(declared_size.error());
    }

    const size_t chunk_data_offset = offset + 8;
    const size_t remaining_bytes = bytes.size() - chunk_data_offset;

    if (_chunk_is(bytes, offset, "fmt ")) {
      if (*declared_size > remaining_bytes || *declared_size < 16) {
        return asryx::fail("invalid wav fmt chunk");
      }

      const auto format = _read_wav_format(bytes, chunk_data_offset);
      if (!format) {
        return std::unexpected(format.error());
      }

      chunks.format = *format;
      found_fmt = true;
    }
    else if (_chunk_is(bytes, offset, "data")) {
      chunks.sample_data = _read_wav_data_chunk(bytes, chunk_data_offset, *declared_size);
      found_sample_data = true;
      break;
    }

    if (*declared_size > remaining_bytes) {
      break;
    }

    offset = chunk_data_offset + *declared_size + (*declared_size % 2U);
  }

  if (!found_fmt || !found_sample_data) {
    return asryx::fail("invalid wav file: missing fmt or data chunk");
  }

  return chunks;
}

std::vector<float> _decode_pcm16(const Pcm16Data& data)
{
  const size_t aligned_size = data.size - (data.size % 2U);

  std::vector<float> samples;
  samples.reserve(aligned_size / 2U);

  for (size_t i = data.offset; i < data.offset + aligned_size; i += 2) {
    const auto high = static_cast<std::uint16_t>((*data.bytes)[i + 1]);
    const auto low = static_cast<std::uint16_t>((*data.bytes)[i]);
    const auto raw = static_cast<std::uint16_t>(low | static_cast<std::uint16_t>(high << 8U));
    const auto sample = static_cast<std::int16_t>(raw);
    samples.push_back(static_cast<float>(sample) / PCM16_SCALE);
  }

  return samples;
}

} // namespace

std::expected<std::vector<float>, asryx::Error> read_pcm16_wav(const std::string& path)
{
  const auto bytes = _read_file(path);
  if (!bytes) {
    return std::unexpected(bytes.error());
  }

  const auto valid_container = _validate_wav_container(*bytes);
  if (!valid_container) {
    return std::unexpected(valid_container.error());
  }

  const auto chunks = _read_wav_chunks(*bytes);
  if (!chunks) {
    return std::unexpected(chunks.error());
  }

  const auto valid_format = _validate_wav_format(chunks->format);
  if (!valid_format) {
    return std::unexpected(valid_format.error());
  }

  return _decode_pcm16(
      {.bytes = &*bytes, .offset = chunks->sample_data.offset, .size = chunks->sample_data.size});
}

} // namespace engine::audio
