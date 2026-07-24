#ifndef ASRYX_ENGINE_ENGINE_HPP
#define ASRYX_ENGINE_ENGINE_HPP

#include "error.hpp"

#include <expected>
#include <string>
#include <sys/types.h>

namespace engine {

struct TranscriptionRequest
{
  std::string model_path;
  std::string vad_model_path;
  std::string wav_path;
  std::string language;
  std::string cancel_marker_path;
};

std::expected<pid_t, asryx::Error> start_recording(const std::string& wav_path,
                                                   const std::string& err_path);
std::expected<bool, asryx::Error> stop_recording(pid_t pid);
std::expected<std::string, asryx::Error> transcribe(const TranscriptionRequest& request);
std::expected<bool, asryx::Error> copy_to_clipboard(const std::string& text);
std::expected<bool, asryx::Error> send_notification(const std::string& message);

} // namespace engine

#endif // ASRYX_ENGINE_ENGINE_HPP
