#include "engine/transcription/request/request.hpp"

#include <filesystem>

namespace engine::transcription::request {

yx::Result<void> validate(const TranscriptionRequest& request)
{
  if (!std::filesystem::exists(request.model_path)) {
    return yx::fail("model file does not exist: " + request.model_path);
  }

  if (!std::filesystem::exists(request.wav_path)) {
    return yx::fail("wav file does not exist: " + request.wav_path);
  }

  if (!std::filesystem::exists(request.vad_model_path)) {
    return yx::fail("VAD model file does not exist: " + request.vad_model_path);
  }

  return yx::ok();
}

} // namespace engine::transcription::request
