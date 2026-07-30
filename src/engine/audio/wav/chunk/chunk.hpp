#ifndef ASRYX_ENGINE_AUDIO_WAV_CHUNK_HPP
#define ASRYX_ENGINE_AUDIO_WAV_CHUNK_HPP

#include "engine/audio/wav/format/format.hpp"
#include "engine/audio/wav/wav.hpp"
#include "error.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace engine::audio {

struct WavChunks
{
  WavFormat format;
  WavSampleData sample_data;
};

yx::Result<WavChunks> read_wav_chunks(const std::vector<std::uint8_t>& bytes, size_t riff_end);

} // namespace engine::audio

#endif // ASRYX_ENGINE_AUDIO_WAV_CHUNK_HPP
