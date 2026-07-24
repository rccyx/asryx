#include "engine/engine.hpp"

#include "engine/recorder/recorder.hpp"
#include "engine/transcription/transcription.hpp"

#include <string>

namespace engine {

std::expected<pid_t, asryx::Error> start_recording(const std::string& wav_path,
                                                   const std::string& err_path)
{
  return recorder::start(wav_path, err_path);
}

std::expected<bool, asryx::Error> stop_recording(pid_t pid)
{
  return recorder::stop(pid);
}

std::expected<std::string, asryx::Error> transcribe(const TranscriptionRequest& request)
{
  return transcription::run(request);
}

} // namespace engine
