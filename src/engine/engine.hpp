#ifndef ASRYX_ENGINE_ENGINE_HPP
#define ASRYX_ENGINE_ENGINE_HPP

#include <string>
#include <sys/types.h>

namespace engine {

pid_t start_recording(const std::string& wav_path, const std::string& err_path);
bool stop_recording(pid_t pid);
std::string transcribe(const std::string& model_path, const std::string& wav_path,
                       const std::string& language);
bool copy_to_clipboard(const std::string& text);
bool send_notification(const std::string& message);

} // namespace engine

#endif // ASRYX_ENGINE_ENGINE_HPP