#include "engine/audio/audio.hpp"

#include "engine/audio/file/file.hpp"
#include "engine/audio/pcm/pcm.hpp"
#include "engine/audio/wav/wav.hpp"

namespace engine::audio {

yx::Result<std::vector<float>> read_pcm16_wav(const std::string& path)
{
  const auto bytes = read_audio_file(path);
  if (!bytes) {
    return yx::fail(bytes.error());
  }

  const auto wav = parse_pcm16_wav(*bytes);
  if (!wav) {
    return yx::fail(wav.error());
  }

  return decode_pcm16(*wav);
}

} // namespace engine::audio
