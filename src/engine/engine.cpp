#include "engine/engine.hpp"

#include "engine/recorder/recorder.hpp"
#include "engine/transcription/transcription.hpp"

#include <string>

namespace engine {

yx::Result<pid_t> start_recording(const std::string& wav_path, const std::string& err_path)
{
  return recorder::start(wav_path, err_path);
}

yx::Result<bool> stop_recording(pid_t pid)
{
  return recorder::stop(pid);
}

yx::Result<std::string> transcribe(const TranscriptionRequest& request)
{
  return transcription::run(request);
}

} // namespace engine
