#ifndef ASRYX_ENGINE_RECORDER_HPP
#define ASRYX_ENGINE_RECORDER_HPP

#include "error.hpp"

#include <expected>
#include <string>
#include <sys/types.h>

namespace engine::recorder {

std::expected<pid_t, asryx::Error> start(const std::string& wav_path, const std::string& err_path);
std::expected<bool, asryx::Error> stop(pid_t pid);

} // namespace engine::recorder

#endif // ASRYX_ENGINE_RECORDER_HPP
