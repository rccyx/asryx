#include "engine/audio/wav/wav.hpp"

#include "engine/audio/wav/chunk/chunk.hpp"
#include "engine/audio/wav/format/format.hpp"
#include "engine/audio/wav/riff/riff.hpp"

namespace engine::audio {

yx::Result<WavSampleData> parse_pcm16_wav(const std::vector<std::uint8_t>& bytes)
{
  const auto riff_end = validate_riff_wave(bytes);
  if (!riff_end) {
    return yx::fail(riff_end.error());
  }

  const auto chunks = read_wav_chunks(bytes, *riff_end);
  if (!chunks) {
    return yx::fail(chunks.error());
  }

  const auto valid_format = validate_wav_format(chunks->format);
  if (!valid_format) {
    return yx::fail(valid_format.error());
  }

  return yx::ok(chunks->sample_data);
}

} // namespace engine::audio
