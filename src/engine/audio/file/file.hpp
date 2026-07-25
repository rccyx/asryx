#ifndef ASRYX_ENGINE_AUDIO_FILE_HPP
#define ASRYX_ENGINE_AUDIO_FILE_HPP

#include "error.hpp"

#include <cstdint>
#include <expected>
#include <string>
#include <vector>

namespace engine::audio {

std::expected<std::vector<std::uint8_t>, asryx::Error> read_audio_file(const std::string& path);

} // namespace engine::audio

#endif // ASRYX_ENGINE_AUDIO_FILE_HPP
