#ifndef ASRYX_ENGINE_AUDIO_FILE_HPP
#define ASRYX_ENGINE_AUDIO_FILE_HPP

#include "error.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace engine::audio {

yx::Result<std::vector<std::uint8_t>> read_audio_file(const std::string& path);

} // namespace engine::audio

#endif // ASRYX_ENGINE_AUDIO_FILE_HPP
