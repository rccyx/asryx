#ifndef ASRYX_ENGINE_AUDIO_HPP
#define ASRYX_ENGINE_AUDIO_HPP

#include "error.hpp"

#include <string>
#include <vector>

namespace engine::audio {

yx::Result<std::vector<float>> read_pcm16_wav(const std::string& path);

} // namespace engine::audio

#endif // ASRYX_ENGINE_AUDIO_HPP
