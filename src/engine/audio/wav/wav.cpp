#include "engine/audio/wav/wav.hpp"

#include "engine/audio/wav/chunk/chunk.hpp"
#include "engine/audio/wav/format/format.hpp"
#include "engine/audio/wav/riff/riff.hpp"

namespace engine::audio {

std::expected<WavSampleData, asryx::Error> parse_pcm16_wav(const std::vector<std::uint8_t>& bytes)
{
  const auto riff_end = validate_riff_wave(bytes);
  if (!riff_end) {
    return std::unexpected(riff_end.error());
  }

  const auto chunks = read_wav_chunks(bytes, *riff_end);
  if (!chunks) {
    return std::unexpected(chunks.error());
  }

  const auto valid_format = validate_wav_format(chunks->format);
  if (!valid_format) {
    return std::unexpected(valid_format.error());
  }

  return chunks->sample_data;
}

} // namespace engine::audio
