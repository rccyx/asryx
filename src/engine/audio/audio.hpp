#ifndef ASRYX_ENGINE_AUDIO_HPP
#define ASRYX_ENGINE_AUDIO_HPP

#include "error.hpp"

#include <expected>
#include <string>
#include <vector>

namespace engine::audio {

std::expected<std::vector<float>, asryx::Error> read_pcm16_wav(const std::string& path);

} // namespace engine::audio

#endif // ASRYX_ENGINE_AUDIO_HPP
