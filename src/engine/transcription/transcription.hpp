#ifndef ASRYX_ENGINE_TRANSCRIPTION_HPP
#define ASRYX_ENGINE_TRANSCRIPTION_HPP

#include "engine/engine.hpp"

#include <string>

namespace engine::transcription {

yx::Result<void> validate_prompt(const std::string& model_path, const std::string& prompt);
yx::Result<std::string> run(const TranscriptionRequest& request);

} // namespace engine::transcription

#endif // ASRYX_ENGINE_TRANSCRIPTION_HPP
