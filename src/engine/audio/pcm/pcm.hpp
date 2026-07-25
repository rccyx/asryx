#ifndef ASRYX_ENGINE_AUDIO_PCM_HPP
#define ASRYX_ENGINE_AUDIO_PCM_HPP

#include "engine/audio/wav/wav.hpp"
#include "error.hpp"

#include <expected>
#include <vector>

namespace engine::audio {

std::expected<std::vector<float>, asryx::Error> decode_pcm16(const WavSampleData& data);

} // namespace engine::audio

#endif // ASRYX_ENGINE_AUDIO_PCM_HPP
