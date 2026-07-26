#include "engine/transcription/request/request.hpp"

#include <filesystem>

namespace engine::transcription::request {

std::expected<void, asryx::Error> validate(const TranscriptionRequest& request)
{
  if (!std::filesystem::exists(request.model_path)) {
    return asryx::fail("model file does not exist: " + request.model_path);
  }

  if (!std::filesystem::exists(request.wav_path)) {
    return asryx::fail("wav file does not exist: " + request.wav_path);
  }

  if (!std::filesystem::exists(request.vad_model_path)) {
    return asryx::fail("VAD model file does not exist: " + request.vad_model_path);
  }

  return {};
}

} // namespace engine::transcription::request
