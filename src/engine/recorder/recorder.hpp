#ifndef ASRYX_ENGINE_RECORDER_HPP
#define ASRYX_ENGINE_RECORDER_HPP

#include "error.hpp"

#include <string>
#include <sys/types.h>

namespace engine::recorder {

yx::Result<pid_t> start(const std::string& wav_path, const std::string& err_path);
yx::Result<bool> stop(pid_t pid);

} // namespace engine::recorder

#endif // ASRYX_ENGINE_RECORDER_HPP
