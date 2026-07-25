#include "engine/audio/wav/chunk/chunk.hpp"

#include "engine/audio/wav/riff/riff.hpp"

namespace engine::audio {

namespace {

struct WavChunk
{
  WavFormat format;
  WavSampleData sample_data;
  size_t next_offset = 0;
};

struct WavChunkRead
{
  const std::vector<std::uint8_t>* bytes = nullptr;
  size_t offset = 0;
  size_t riff_end = 0;
};

std::expected<WavChunk, asryx::Error> _read_wav_chunk(const WavChunkRead& read)
{
  const auto& bytes = *read.bytes;
  const auto declared_size = read_u32_le(bytes, read.offset + 4);
  if (!declared_size) {
    return std::unexpected(declared_size.error());
  }

  const auto chunk_size = static_cast<size_t>(*declared_size);
  const size_t chunk_data_offset = read.offset + 8;
  if (!has_range({.total_size = read.riff_end, .offset = chunk_data_offset, .length = chunk_size}))
  {
    return asryx::fail("invalid wav file: chunk exceeds RIFF boundary");
  }

  const size_t padding = chunk_size % 2U;
  const size_t chunk_end = chunk_data_offset + chunk_size;
  if (!has_range({.total_size = read.riff_end, .offset = chunk_end, .length = padding})) {
    return asryx::fail("invalid wav file: missing chunk padding byte");
  }

  WavChunk chunk{
      .format = {},
      .sample_data = {},
      .next_offset = chunk_end + padding,
  };

  if (chunk_is(bytes, {.offset = read.offset, .id = "fmt "})) {
    const auto format =
        read_wav_format({.bytes = &bytes, .offset = chunk_data_offset, .chunk_size = chunk_size});
    if (!format) {
      return std::unexpected(format.error());
    }

    chunk.format = *format;
  }
  else if (chunk_is(bytes, {.offset = read.offset, .id = "data"})) {
    if (chunk_size == 0) {
      return asryx::fail("invalid wav file: empty data chunk");
    }

    chunk.sample_data = {.bytes = &bytes, .offset = chunk_data_offset, .size = chunk_size};
  }

  return chunk;
}

} // namespace

std::expected<WavChunks, asryx::Error> read_wav_chunks(const std::vector<std::uint8_t>& bytes,
                                                       size_t riff_end)
{
  bool found_fmt = false;
  bool found_sample_data = false;

  WavChunks chunks{
      .format = {},
      .sample_data = {.bytes = &bytes},
  };
  size_t offset = 12;

  while (offset < riff_end) {
    if (!has_range({.total_size = riff_end, .offset = offset, .length = 8})) {
      return asryx::fail("invalid wav file: truncated chunk header");
    }

    const auto chunk = _read_wav_chunk({.bytes = &bytes, .offset = offset, .riff_end = riff_end});
    if (!chunk) {
      return std::unexpected(chunk.error());
    }

    if (chunk_is(bytes, {.offset = offset, .id = "fmt "})) {
      if (found_fmt) {
        return asryx::fail("invalid wav file: duplicate fmt chunk");
      }

      chunks.format = chunk->format;
      found_fmt = true;
    }
    else if (chunk_is(bytes, {.offset = offset, .id = "data"})) {
      if (found_sample_data) {
        return asryx::fail("invalid wav file: duplicate data chunk");
      }

      chunks.sample_data = chunk->sample_data;
      found_sample_data = true;
    }

    offset = chunk->next_offset;
  }

  if (!found_fmt || !found_sample_data) {
    return asryx::fail("invalid wav file: missing fmt or data chunk");
  }

  if (chunks.sample_data.size % chunks.format.block_align != 0) {
    return asryx::fail("invalid wav file: data chunk is not block aligned");
  }

  return chunks;
}

} // namespace engine::audio
