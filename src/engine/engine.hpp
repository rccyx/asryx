#ifndef ASRYX_ENGINE_ENGINE_HPP
#define ASRYX_ENGINE_ENGINE_HPP

#include "error.hpp"

#include <string>
#include <sys/types.h>

namespace engine {

struct TranscriptionRequest
{
  std::string model_path;
  std::string vad_model_path;
  std::string wav_path;
  std::string language;
  std::string prompt;
  std::string cancel_marker_path;
};

yx::Result<pid_t> start_recording(const std::string& wav_path, const std::string& err_path);
yx::Result<bool> stop_recording(pid_t pid);
yx::Result<void> validate_prompt(const std::string& model_path, const std::string& prompt);
yx::Result<std::string> transcribe(const TranscriptionRequest& request);
yx::Result<bool> copy_to_clipboard(const std::string& text);
yx::Result<bool> send_notification(const std::string& message);

} // namespace engine

#endif // ASRYX_ENGINE_ENGINE_HPP
