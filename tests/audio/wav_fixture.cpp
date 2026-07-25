#include "tests/audio/wav_fixture.hpp"

#include "engine/audio/audio.hpp"
#include "error.hpp"
#include "platform/fs.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace audio_test {

namespace {

constexpr std::uint32_t WAV_SAMPLE_RATE = 16000;
constexpr std::uint16_t WAV_BITS_PER_SAMPLE = 16;
constexpr std::uint16_t WAV_BLOCK_ALIGN = 2;

using Fourcc = std::array<char, 4>;

struct WavSpec
{
  std::uint32_t sample_rate = WAV_SAMPLE_RATE;
};

struct WavChunk
{
  Fourcc id;
  std::vector<std::uint8_t> data;
};

struct U32Write
{
  size_t offset = 0;
  std::uint32_t value = 0;
};

std::filesystem::path _wav_path()
{
  return std::filesystem::absolute("./.asryx-test-audio-sample.wav");
}

void _append_fourcc(std::vector<std::uint8_t>& bytes, const Fourcc& id)
{
  std::transform(id.begin(), id.end(), std::back_inserter(bytes),
                 [](char byte) { return static_cast<std::uint8_t>(byte); });
}

void _append_u16(std::vector<std::uint8_t>& bytes, std::uint16_t value)
{
  const auto wide_value = static_cast<std::uint32_t>(value);
  bytes.push_back(static_cast<std::uint8_t>(wide_value & 0xFFU));
  bytes.push_back(static_cast<std::uint8_t>((wide_value >> 8U) & 0xFFU));
}

void _append_u32(std::vector<std::uint8_t>& bytes, std::uint32_t value)
{
  bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
  bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
  bytes.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xFFU));
  bytes.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xFFU));
}

void _append_sample(std::vector<std::uint8_t>& bytes, std::int16_t sample)
{
  _append_u16(bytes, static_cast<std::uint16_t>(sample));
}

void _append_chunk(std::vector<std::uint8_t>& bytes, const WavChunk& chunk)
{
  _append_fourcc(bytes, chunk.id);
  _append_u32(bytes, static_cast<std::uint32_t>(chunk.data.size()));
  bytes.insert(bytes.end(), chunk.data.begin(), chunk.data.end());

  if (chunk.data.size() % 2U != 0) {
    bytes.push_back(0);
  }
}

void _write_bytes(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes)
{
  std::ofstream file(path, std::ios::binary);

  for (const std::uint8_t byte : bytes) {
    file.put(static_cast<char>(byte));
  }
}

std::vector<std::uint8_t> _sample_data(std::span<const std::int16_t> samples)
{
  std::vector<std::uint8_t> bytes;

  for (const std::int16_t sample : samples) {
    _append_sample(bytes, sample);
  }

  return bytes;
}

std::vector<std::uint8_t> _pcm_format_chunk(const WavSpec& spec)
{
  std::vector<std::uint8_t> bytes;
  _append_u16(bytes, 1);
  _append_u16(bytes, 1);
  _append_u32(bytes, spec.sample_rate);
  _append_u32(bytes, spec.sample_rate * static_cast<std::uint32_t>(WAV_BLOCK_ALIGN));
  _append_u16(bytes, WAV_BLOCK_ALIGN);
  _append_u16(bytes, WAV_BITS_PER_SAMPLE);
  return bytes;
}

std::vector<std::uint8_t> _extensible_format_chunk()
{
  std::vector<std::uint8_t> bytes = _pcm_format_chunk({});
  const std::array<std::uint8_t, 16> pcm_guid = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00,
                                                 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71};

  bytes[0] = 0xFE;
  bytes[1] = 0xFF;
  _append_u16(bytes, 22);
  _append_u16(bytes, WAV_BITS_PER_SAMPLE);
  _append_u32(bytes, 0);
  bytes.insert(bytes.end(), pcm_guid.begin(), pcm_guid.end());
  return bytes;
}

WavChunk _format_chunk(const WavSpec& spec)
{
  return {
      .id = {'f', 'm', 't', ' '},
      .data = _pcm_format_chunk(spec)
  };
}

WavChunk _extensible_format_wav_chunk()
{
  return {
      .id = {'f', 'm', 't', ' '},
      .data = _extensible_format_chunk()
  };
}

WavChunk _junk_chunk()
{
  return {
      .id = {'J', 'U', 'N', 'K'},
      .data = {0x01}
  };
}

WavChunk _data_chunk(std::span<const std::int16_t> samples)
{
  return {
      .id = {'d', 'a', 't', 'a'},
      .data = _sample_data(samples)
  };
}

std::vector<std::uint8_t> _riff_wave(std::span<const WavChunk> chunks)
{
  std::vector<std::uint8_t> bytes;
  _append_fourcc(bytes, {'R', 'I', 'F', 'F'});
  _append_u32(bytes, 0);
  _append_fourcc(bytes, {'W', 'A', 'V', 'E'});

  for (const auto& chunk : chunks) {
    _append_chunk(bytes, chunk);
  }

  write_riff_size(bytes, static_cast<std::uint32_t>(bytes.size() - 8U));
  return bytes;
}

} // namespace

std::vector<std::int16_t> sample_triplet()
{
  return {-32768, 0, 32767};
}

std::vector<std::uint8_t> pcm16_wav(std::span<const std::int16_t> samples)
{
  return pcm16_wav_at(WAV_SAMPLE_RATE, samples);
}

std::vector<std::uint8_t> pcm16_wav_at(std::uint32_t sample_rate,
                                       std::span<const std::int16_t> samples)
{
  const std::array<WavChunk, 2> chunks = {
      _format_chunk({.sample_rate = sample_rate}),
      _data_chunk(samples),
  };

  return _riff_wave(chunks);
}

std::vector<std::uint8_t> wav_with_padded_chunk(std::span<const std::int16_t> samples)
{
  const std::array<WavChunk, 3> chunks = {
      _format_chunk({}),
      _junk_chunk(),
      _data_chunk(samples),
  };

  return _riff_wave(chunks);
}

std::vector<std::uint8_t> extensible_pcm16_wav(std::span<const std::int16_t> samples)
{
  const std::array<WavChunk, 2> chunks = {
      _extensible_format_wav_chunk(),
      _data_chunk(samples),
  };

  return _riff_wave(chunks);
}

void write_riff_size(std::vector<std::uint8_t>& bytes, std::uint32_t value)
{
  const U32Write write = {.offset = 4, .value = value};
  bytes[write.offset] = static_cast<std::uint8_t>(write.value & 0xFFU);
  bytes[write.offset + 1] = static_cast<std::uint8_t>((write.value >> 8U) & 0xFFU);
  bytes[write.offset + 2] = static_cast<std::uint8_t>((write.value >> 16U) & 0xFFU);
  bytes[write.offset + 3] = static_cast<std::uint8_t>((write.value >> 24U) & 0xFFU);
}

std::expected<std::vector<float>, asryx::Error> read_samples(const std::vector<std::uint8_t>& bytes)
{
  const auto path = _wav_path();
  _write_bytes(path, bytes);
  const auto samples = engine::audio::read_pcm16_wav(path.string());
  asryx::ignore_failure(platform::safe_delete_file(path));
  return samples;
}

} // namespace audio_test
