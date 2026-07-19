#ifndef ASRYX_ENGINE_RECORDER_HPP
#define ASRYX_ENGINE_RECORDER_HPP

#include <string>
#include <sys/types.h>

namespace engine::recorder {

pid_t start(const std::string& wav_path, const std::string& err_path);
bool stop(pid_t pid);

} // namespace engine::recorder

#endif // ASRYX_ENGINE_RECORDER_HPP
