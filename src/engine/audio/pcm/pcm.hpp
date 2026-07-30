#ifndef ASRYX_ENGINE_AUDIO_PCM_HPP
#define ASRYX_ENGINE_AUDIO_PCM_HPP

#include "engine/audio/wav/wav.hpp"
#include "error.hpp"

#include <vector>

namespace engine::audio {

yx::Result<std::vector<float>> decode_pcm16(const WavSampleData& data);

} // namespace engine::audio

#endif // ASRYX_ENGINE_AUDIO_PCM_HPP
